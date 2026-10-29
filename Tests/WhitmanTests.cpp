// Headless tests for WHITMAN's engine. Prints PASS / FAIL per check; exit code 1 if anything failed.

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/MixEngine.h"
#include "../Source/MixState.h"
#include "../Source/RouteStatus.h"

#if JUCE_WINDOWS
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include "../Source/HawthorneRoute/RouteShared.h"
#endif

using namespace whitman;

static int failures = 0, checks = 0;

static void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) ++failures;
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
}

static bool approx (float a, float b, float tol = 1.0e-4f) { return std::abs (a - b) <= tol; }

//==============================================================================
// The channel names exactly as Hawthorne Route's driver gives them (HawthorneRouteDriver.cpp)
static void routeNames (int tracks, juce::StringArray& ins, juce::StringArray& outs)
{
    ins.clear(); outs.clear();
    for (int t = 1; t <= tracks; ++t) ins.add ("Hawthorne track " + juce::String (t));
    ins.add ("Hawthorne master L"); ins.add ("Hawthorne master R"); ins.add ("Hawthorne LTC");
    for (int t = 1; t <= tracks; ++t) outs.add ("To Hawthorne track " + juce::String (t));
    outs.add ("To Hawthorne master L"); outs.add ("To Hawthorne master R");
}

static juce::BigInteger all (int n) { juce::BigInteger b; b.setRange (0, n, true); return b; }

static void testRouting()
{
    for (int tracks : { 8, 16, 24, 32, 96 })
    {
        juce::StringArray ins, outs;
        routeNames (tracks, ins, outs);
        auto r = Routing::make (ins, all (ins.size()), outs, all (outs.size()));
        bool tracksRight = (int) r.stripOfInput.size() == tracks + 3;
        for (int t = 0; t < tracks && tracksRight; ++t) tracksRight = r.stripOfInput[(size_t) t] == t;
        const bool extrasIgnored = r.stripOfInput[(size_t) tracks] == -1 && r.stripOfInput[(size_t) tracks + 1] == -1
                                   && r.stripOfInput[(size_t) tracks + 2] == -1;
        check (r.isHawthorneRoute && r.numStrips == tracks && tracksRight && extrasIgnored,
               "Route, " + juce::String (tracks) + " tracks: track n -> strip n, master and LTC not mixed");
        check (r.outLeft == tracks && r.outRight == tracks + 1,
               "Route, " + juce::String (tracks) + " tracks: mix -> To Hawthorne master L / R");
    }

    {
        // the reel grew and only the old channels are on yet (JUCE reopens with the old ones)
        juce::StringArray ins, outs;
        routeNames (32, ins, outs);
        auto r = Routing::make (ins, all (24 + 3), outs, all (24 + 2));
        // (the old 27 input bits now land on tracks 1-27; the master outputs are past the old 26)
        check (r.numStrips == 27 && r.outLeft == -1 && r.outRight == -1,
               "Route, channels not all on yet: strips follow the names, no output is guessed wrongly");
    }

    {
        // some inputs switched off: indices are into the ACTIVE channels
        juce::StringArray ins, outs;
        routeNames (8, ins, outs);
        juce::BigInteger inMask; inMask.setBit (2); inMask.setBit (5);
        auto r = Routing::make (ins, inMask, outs, all (outs.size()));
        check (r.stripOfInput.size() == 2 && r.stripOfInput[0] == 2 && r.stripOfInput[1] == 5 && r.numStrips == 6,
               "Route, some inputs off: active input k maps to its own track");
    }

    {
        juce::StringArray ins { "Input 1", "Input 2", "Input 3", "Input 4" }, outs { "Out 1", "Out 2", "Out 3", "Out 4" };
        auto r = Routing::make (ins, all (4), outs, all (4));
        check (! r.isHawthorneRoute && r.numStrips == 4 && r.stripOfInput[3] == 3 && r.outLeft == 0 && r.outRight == 1,
               "Other device: inputs are strips 1-4, mix to outputs 1-2");
    }

    check (Routing::trackNumberOf ("Hawthorne track 12") == 12 && Routing::trackNumberOf ("To Hawthorne track 12") == 0
           && Routing::trackNumberOf ("Hawthorne master L") == 0 && Routing::trackNumberOf ("Hawthorne track 97") == 0,
           "Channel names parsed: inputs only, tracks 1-96");
}

//==============================================================================
struct Rig
{
    int tracks;
    MixEngine engine;
    juce::AudioBuffer<float> in, out;
    std::vector<const float*> inPtrs;
    std::vector<float*> outPtrs;

    explicit Rig (int t, int block = 256) : tracks (t), in (t + 3, block), out (t + 2, block)
    {
        juce::StringArray ins, outs;
        routeNames (t, ins, outs);
        engine.setRouting (Routing::make (ins, all (ins.size()), outs, all (outs.size())));
        engine.prepare (48000.0, block);
        in.clear();
        for (int c = 0; c < in.getNumChannels(); ++c)  inPtrs.push_back (in.getReadPointer (c));
        for (int c = 0; c < out.getNumChannels(); ++c) outPtrs.push_back (out.getWritePointer (c));
    }

    void run (int blocks = 3)
    {
        for (int b = 0; b < blocks; ++b)
            engine.process (inPtrs.data(), (int) inPtrs.size(), outPtrs.data(), (int) outPtrs.size(), in.getNumSamples());
    }

    float L (int k = -1) const { return out.getSample (tracks, k < 0 ? out.getNumSamples() - 1 : k); }
    float R (int k = -1) const { return out.getSample (tracks + 1, k < 0 ? out.getNumSamples() - 1 : k); }

    void dc (int track, float v) { juce::FloatVectorOperations::fill (in.getWritePointer (track), v, in.getNumSamples()); }
};

static void testMix()
{
    const float c = std::sqrt (0.5f);

    { Rig r (24); r.dc (0, 0.5f); r.run();
      check (approx (r.L(), 0.5f * c) && approx (r.R(), 0.5f * c), "Unity, centre: -3 dB each side"); }

    { Rig r (24); r.dc (3, 0.5f); r.engine.strip (3).pan = -1.0f; r.run();
      check (approx (r.L(), 0.5f) && approx (r.R(), 0.0f), "Hard left: all of it left"); }

    { Rig r (24); r.dc (3, 0.5f); r.engine.strip (3).pan = 1.0f; r.run();
      check (approx (r.L(), 0.0f) && approx (r.R(), 0.5f), "Hard right: all of it right"); }

    { Rig r (24); r.dc (5, 0.5f); r.engine.strip (5).pan = 0.37f; r.run();
      check (approx (r.L() * r.L() + r.R() * r.R(), 0.25f), "Pan keeps constant power"); }

    { Rig r (24); r.dc (1, 0.5f); r.engine.strip (1).gainDb = -6.0f; r.engine.strip (1).pan = -1.0f; r.run();
      check (approx (r.L(), 0.5f * juce::Decibels::decibelsToGain (-6.0f)), "Fader -6 dB"); }

    { Rig r (24); r.dc (1, 0.5f); r.engine.strip (1).gainDb = faderMinDb; r.run();
      check (r.L() == 0.0f && r.R() == 0.0f, "Fader at the bottom is off"); }

    { Rig r (24); r.dc (0, 0.25f); r.dc (1, 0.25f); r.engine.strip (0).pan = -1.0f; r.engine.strip (1).pan = -1.0f; r.run();
      check (approx (r.L(), 0.5f), "Tracks sum"); }

    { Rig r (24); r.dc (0, 0.5f); r.dc (1, 0.5f); r.engine.strip (0).pan = -1.0f; r.engine.strip (1).pan = 1.0f;
      r.engine.strip (0).mute = true; r.run();
      check (approx (r.L(), 0.0f) && approx (r.R(), 0.5f), "Mute"); }

    { Rig r (24); for (int t : { 0, 1, 2 }) { r.dc (t, 0.2f); r.engine.strip (t).pan = -1.0f; }
      r.engine.strip (1).solo = true; r.engine.strip (2).solo = true; r.run();
      check (approx (r.L(), 0.4f), "Solo: only soloed tracks (two of three) heard"); }

    { Rig r (24); r.dc (0, 0.5f); r.engine.strip (0).pan = -1.0f; r.engine.strip (0).solo = true; r.engine.strip (0).mute = true; r.run();
      check (approx (r.L(), 0.0f), "Muted wins over solo"); }

    { Rig r (24); r.dc (0, 0.5f); r.engine.strip (0).pan = -1.0f; r.engine.masterDb = -6.0f; r.run();
      check (approx (r.L(), 0.5f * juce::Decibels::decibelsToGain (-6.0f)), "Master fader"); }

    { Rig r (24);
      // master returns and LTC from Hawthorne must never reach the mix (that would feed back)
      r.dc (24, 0.9f); r.dc (25, 0.9f); r.dc (26, 0.9f); r.run();
      check (r.L() == 0.0f && r.R() == 0.0f, "Hawthorne's master and LTC aren't mixed back in"); }

    { Rig r (24); r.dc (0, 0.5f); r.run();
      bool tracksSilent = true;
      for (int t = 0; t < 24; ++t) tracksSilent = tracksSilent && r.out.getMagnitude (t, 0, r.out.getNumSamples()) == 0.0f;
      check (tracksSilent, "Nothing is sent to Hawthorne's track inputs"); }

    { Rig r (24, 512); r.dc (0, 0.5f); r.engine.strip (0).pan = -1.0f; r.run (2);
      r.engine.strip (0).gainDb = faderMinDb; r.run (1);
      float maxStep = 0.0f;
      for (int k = 1; k < 512; ++k) maxStep = juce::jmax (maxStep, std::abs (r.L (k) - r.L (k - 1)));
      check (maxStep < 0.002f && approx (r.L(), 0.0f, 1.0e-3f), "Fader moves ramp over a block (no clicks)"); }

    { Rig r (24); r.dc (0, 1.0f); r.dc (1, 1.0f); r.engine.strip (0).pan = -1.0f; r.engine.strip (1).pan = -1.0f; r.run();
      check (r.engine.masterClipped.load(), "Over 0 dBFS lights CLIP"); }

    { Rig r (24); r.dc (2, 0.5f); r.engine.strip (2).gainDb = -6.0f; r.run();
      const float p = MixEngine::takePeak (r.engine.strip (2).peak);
      check (approx (p, 0.5f * juce::Decibels::decibelsToGain (-6.0f)) && r.engine.strip (2).peak.load() == 0.0f,
             "Strip meter: post-fader peak, cleared when read"); }

    { // 96 tracks, real-ish signals, nothing unusual
      Rig r (96, 256);
      juce::Random rnd (7);
      for (int t = 0; t < 96; ++t) for (int k = 0; k < 256; ++k) r.in.setSample (t, k, rnd.nextFloat() * 0.02f - 0.01f);
      r.run (4);
      bool finite = true;
      for (int k = 0; k < 256; ++k) finite = finite && std::isfinite (r.L (k)) && std::isfinite (r.R (k));
      check (finite && r.out.getMagnitude (96, 0, 256) > 0.0f, "96 tracks mix cleanly"); }
}

//==============================================================================
static void testMixFiles()
{
    MixEngine a, b;
    juce::StringArray na, nb;
    MixState::reset (a, na);
    a.strip (4).gainDb = -7.5f; a.strip (4).pan = 0.3f; a.strip (4).mute = true; na.set (4, "Kick");
    a.strip (40).solo = true;
    a.masterDb = -2.0f;
    auto x = MixState::toXml (a, na);
    const auto text = x->toString();
    auto parsed = juce::parseXML (text);
    MixState::reset (b, nb);
    const bool ok = parsed != nullptr && MixState::fromXml (*parsed, b, nb);
    check (ok && approx (b.strip (4).gainDb.load(), -7.5f) && approx (b.strip (4).pan.load(), 0.3f) && b.strip (4).mute.load()
               && b.strip (40).solo.load() && nb[4] == "Kick" && nb[5] == "6" && approx (b.masterDb.load(), -2.0f),
           "Mix file round trip");
    check (x->getNumChildElements() == 2, "Mix file keeps only strips that changed");

    juce::XmlElement wrong ("SOMETHING_ELSE");
    check (! MixState::fromXml (wrong, b, nb) && b.strip (4).mute.load(), "Not a mix file: left alone");
}

//==============================================================================
static void testRouteStatus()
{
   #if JUCE_WINDOWS
    using namespace hawthorne::route;
    {
        RouteStatus s;
        const auto i = s.poll();
        check (! i.hawthorneRunning, "Route status: Hawthorne not running when there's no shared memory");
    }
    // stand in for Hawthorne: create the mapping as RouteHost::open does
    auto mapping = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) sizeof (Shared), mappingName);
    auto* shared = mapping != nullptr ? (Shared*) MapViewOfFile (mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (Shared)) : nullptr;
    check (shared != nullptr, "Route status: test mapping made");
    if (shared == nullptr) return;
    shared->magic = magic; shared->version = version;
    shared->sampleRate = 48000; shared->numTracks = 32;
    shared->heartbeatMs = (std::int64_t) GetTickCount64();
    RouteStatus s;
    auto i = s.poll();
    check (i.hawthorneRunning && i.tracks == 32 && i.sampleRate == 48000, "Route status: reads Hawthorne's rate and track count");
    shared->heartbeatMs = (std::int64_t) GetTickCount64() - 5000;
    i = s.poll();
    check (! i.hawthorneRunning, "Route status: a stale heartbeat means Hawthorne stopped");
    UnmapViewOfFile (shared);
    CloseHandle (mapping);
   #endif
}

int main()
{
    testRouting();
    testMix();
    testMixFiles();
    testRouteStatus();
    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
