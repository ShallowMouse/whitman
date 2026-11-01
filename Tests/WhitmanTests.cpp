// Headless tests for WHITMAN's engine. Prints PASS / FAIL per check; exit code 1 if anything failed.

#include <juce_core/juce_core.h>
#include <functional>
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
// The REDD.51 model
static float sineGainDb (Desk d, float freq, std::function<void (MixEngine&)> setup, float levelDbfs = nominalDbfs)
{
    Rig r (8, 512);
    r.engine.desk = (int) d;
    r.engine.strip (0).pan = -1.0f;
    if (setup) setup (r.engine);
    const double sr = 48000.0;
    const float amp = juce::Decibels::decibelsToGain (levelDbfs);
    double phase = 0.0, sumIn = 0.0, sumOut = 0.0;
    const int blocks = 120;   // ~1.3 s
    for (int b = 0; b < blocks; ++b)
    {
        for (int k = 0; k < 512; ++k)
        {
            r.in.setSample (0, k, amp * (float) std::sin (phase));
            phase += juce::MathConstants<double>::twoPi * freq / sr;
        }
        r.run (1);
        if (b >= blocks / 2)
            for (int k = 0; k < 512; ++k)
            {
                sumIn  += (double) r.in.getSample (0, k) * r.in.getSample (0, k);
                sumOut += (double) r.L (k) * r.L (k);
            }
    }
    return (float) (10.0 * std::log10 (sumOut / sumIn));
}

/** Level of harmonic h relative to the fundamental (dB), via Goertzel on the left output. */
static float harmonicDb (float inputDb, int h, float levelDbfs = nominalDbfs, Desk desk = Desk::redd51)
{
    Rig r (8, 480);
    r.engine.desk = (int) desk;
    r.engine.strip (0).pan = -1.0f;
    r.engine.strip (0).inputDb = inputDb;
    r.engine.strip (0).gainDb = -inputDb;      // fader back down, so the main amps stay clean
    const double sr = 48000.0, f = 1000.0;
    const float amp = juce::Decibels::decibelsToGain (levelDbfs);
    std::vector<float> out;
    double phase = 0.0;
    for (int b = 0; b < 200; ++b)
    {
        for (int k = 0; k < 480; ++k) { r.in.setSample (0, k, amp * (float) std::sin (phase)); phase += juce::MathConstants<double>::twoPi * f / sr; }
        r.run (1);
        if (b >= 100) for (int k = 0; k < 480; ++k) out.push_back (r.L (k));
    }
    auto goertzel = [&] (double freq)
    {
        const double w = juce::MathConstants<double>::twoPi * freq / sr, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (auto v : out) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2);
    };
    return (float) (20.0 * std::log10 (goertzel (f * h) / goertzel (f) + 1.0e-12));
}

static void testRedd()
{
    const auto R = Desk::redd51;
    auto none = [] (MixEngine&) {};

    { const float g = sineGainDb (R, 1000.0f, none);
      check (std::abs (g) < 0.5f, "REDD.51 flat at 1 kHz at 0 VU: " + juce::String (g, 2) + " dB"); }

    { const float lo = sineGainDb (R, 30.0f, [] (MixEngine& e) { e.strip (0).bassDb = 10.0f; });
      const float mid = sineGainDb (R, 1000.0f, [] (MixEngine& e) { e.strip (0).bassDb = 10.0f; });
      check (lo > 8.5f && lo < 10.8f && std::abs (mid) < 1.0f,
             "BASS +10: " + juce::String (lo, 1) + " dB at 30 Hz, " + juce::String (mid, 1) + " at 1 kHz"); }

    { const float lo = sineGainDb (R, 30.0f, [] (MixEngine& e) { e.strip (0).bassDb = -10.0f; });
      check (lo < -8.5f && lo > -10.8f, "BASS -10: " + juce::String (lo, 1) + " dB at 30 Hz"); }

    { const float pk = sineGainDb (R, 5000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; });
      const float mid = sineGainDb (R, 1000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; });
      const float top = sineGainDb (R, 15000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; });
      check (pk > 8.5f && pk < 10.5f && mid < 3.0f && top < pk - 3.0f,
             "Pop TREBLE +10: a bell at 5 kHz (" + juce::String (pk, 1) + " dB; 1 kHz " + juce::String (mid, 1)
             + ", 15 kHz " + juce::String (top, 1) + ")"); }

    { const float top = sineGainDb (R, 15000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = -10.0f; });
      const float mid = sineGainDb (R, 1000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = -10.0f; });
      check (top < -6.5f && mid > -1.0f, "Pop TREBLE -10: a shelf down at 10 kHz (" + juce::String (top, 1) + " dB at 15 kHz)"); }

    { const float top = sineGainDb (R, 15000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; e.strip (0).classic = true; });
      const float five = sineGainDb (R, 5000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; e.strip (0).classic = true; });
      check (top > 6.5f && five < top, "Classic TREBLE +10: a shelf up at 10 kHz (" + juce::String (top, 1) + " dB at 15 kHz, "
             + juce::String (five, 1) + " at 5 kHz)"); }

    { const float g = sineGainDb (R, 1000.0f, [] (MixEngine& e) { e.strip (0).inputDb = 10.0f; e.strip (0).gainDb = -10.0f; });
      check (std::abs (g) < 0.8f, "GAIN +10 with the fader 10 down: about the same level (" + juce::String (g, 2) + " dB)"); }

    { const float clean2 = harmonicDb (0.0f, 2), hot2 = harmonicDb (18.0f, 2), hot3 = harmonicDb (18.0f, 3);
      check (clean2 < -45.0f, "At 0 VU the channel amp is clean (2nd harmonic " + juce::String (clean2, 1) + " dB)");
      check (hot2 > -40.0f && hot2 > clean2 + 15.0f, "GAIN +18 drives the valves (2nd harmonic " + juce::String (hot2, 1) + " dB)");
      check (hot2 > hot3 - 6.0f, "Valve grit has a strong 2nd harmonic, close to the 3rd (2nd " + juce::String (hot2, 1) + ", 3rd " + juce::String (hot3, 1) + ")"); }

    { // main amps: the mix can't go over full scale, however hard it's pushed
      Rig r (8, 512);
      r.engine.desk = (int) R;
      for (int t = 0; t < 8; ++t) { r.dc (t, 0.0f); r.engine.strip (t).gainDb = 10.0f; r.engine.strip (t).pan = -1.0f; }
      double ph = 0.0;
      float most = 0.0f;
      for (int b = 0; b < 40; ++b)
      {
          for (int k = 0; k < 512; ++k) { const float v = 0.9f * (float) std::sin (ph); ph += 0.05; for (int t = 0; t < 8; ++t) r.in.setSample (t, k, v); }
          r.run (1);
          most = juce::jmax (most, r.out.getMagnitude (8, 0, 512));
      }
      check (most <= 1.0f && most > 0.8f, "Main amps top out at full scale (peak " + juce::String (most, 3) + ")"); }

    { // echo: a burst, then a tail from the chamber; none without the send, none on the clean desk
      auto tailDb = [] (Desk d, float sendDb)
      {
          Rig r (8, 480);
          r.engine.desk = (int) d;
          r.engine.strip (0).pan = -1.0f;
          r.engine.strip (0).echoDb = sendDb;
          r.engine.echoReturnDb = 0.0f;
          double ph = 0.0;
          for (int b = 0; b < 20; ++b)   // 0.2 s of tone
          {
              for (int k = 0; k < 480; ++k) { r.in.setSample (0, k, 0.25f * (float) std::sin (ph)); ph += 0.13; }
              r.run (1);
          }
          r.in.clear();
          r.run (30);                    // 0.3 s later
          double e = 0.0;
          for (int b = 0; b < 10; ++b) { r.run (1); for (int k = 0; k < 480; ++k) e += (double) r.L (k) * r.L (k) + r.R (k) * r.R (k); }
          return (float) (10.0 * std::log10 (e / 4800.0 + 1.0e-20));
      };
      const float wet = tailDb (R, 0.0f), dry = tailDb (R, echoMinDb), clean = tailDb (Desk::whitman, 0.0f);
      check (wet > -45.0f && dry < -120.0f && clean < -120.0f,
             "ECHO: a chamber tail with the send up (" + juce::String (wet, 1) + " dB), none without it or on the clean desk"); }

    { // switching desks while running: nothing odd
      Rig r (24); r.dc (0, 0.3f); r.engine.strip (0).bassDb = 6.0f; r.engine.strip (0).echoDb = -10.0f;
      bool finite = true;
      for (int i = 0; i < 6; ++i)
      {
          r.engine.desk = i % 2;
          r.run (2);
          for (int k = 0; k < r.out.getNumSamples(); ++k) finite = finite && std::isfinite (r.L (k)) && std::isfinite (r.R (k));
      }
      check (finite, "Switching desks while playing stays clean"); }

    { MixEngine a, b; juce::StringArray na, nb;
      MixState::reset (a, na);
      a.desk = (int) R; a.strip (2).inputDb = 8.0f; a.strip (2).bassDb = -4.0f; a.strip (2).trebleDb = 6.0f;
      a.strip (2).classic = true; a.strip (2).echoDb = -12.0f; a.echoReturnDb = -3.0f;
      auto x = juce::parseXML (MixState::toXml (a, na)->toString());
      const bool ok = x != nullptr && MixState::fromXml (*x, b, nb);
      check (ok && b.getDesk() == R && approx (b.strip (2).inputDb.load(), 8.0f) && approx (b.strip (2).bassDb.load(), -4.0f)
               && approx (b.strip (2).trebleDb.load(), 6.0f) && b.strip (2).classic.load() && approx (b.strip (2).echoDb.load(), -12.0f)
               && approx (b.echoReturnDb.load(), -3.0f),
             "Mix file keeps the desk, GAIN, EQ and ECHO"); }
}

//==============================================================================
// The REDD.37 and the TG12345
static void testRedd37AndTg()
{
    auto none = [] (MixEngine&) {};
    { const float g = sineGainDb (Desk::redd37, 1000.0f, none);
      check (std::abs (g) < 0.6f, "REDD.37 flat at 1 kHz at 0 VU: " + juce::String (g, 2) + " dB"); }

    { const float r37 = harmonicDb (12.0f, 2, nominalDbfs, Desk::redd37), r51 = harmonicDb (12.0f, 2, nominalDbfs, Desk::redd51);
      check (r37 > r51 + 3.0f, "REDD.37's V72s break up sooner than the REDD.51 (2nd harmonic at GAIN +12: "
             + juce::String (r37, 1) + " vs " + juce::String (r51, 1) + " dB)"); }

    { const float top37 = sineGainDb (Desk::redd37, 14000.0f, none), top51 = sineGainDb (Desk::redd51, 14000.0f, none);
      check (top37 < top51 - 1.0f, "REDD.37 has the darker top (" + juce::String (top37, 1) + " vs " + juce::String (top51, 1) + " dB at 14 kHz)"); }

    const auto T = Desk::tg12345;
    { const float g = sineGainDb (T, 1000.0f, none);
      check (std::abs (g) < 0.5f, "TG12345 flat at 1 kHz at 0 VU: " + juce::String (g, 2) + " dB"); }

    { const float lo = sineGainDb (T, 20.0f, [] (MixEngine& e) { e.strip (0).bassDb = 10.0f; });
      const float at100 = sineGainDb (T, 200.0f, [] (MixEngine& e) { e.strip (0).bassDb = 10.0f; });
      check (lo > 6.5f && at100 < 3.0f, "TG BASS +10: a 50 Hz shelf (" + juce::String (lo, 1) + " dB at 20 Hz, " + juce::String (at100, 1) + " at 200 Hz)"); }

    const auto T4 = Desk::tg12345mk4;
    { bool ok = true; juce::String worst;
      for (int i = 0; i < numPresenceFreqs - 1; ++i)
      {
          const float f = presenceHz (i);
          const float on = sineGainDb (T4, f, [i] (MixEngine& e) { e.strip (0).presenceDb = 10.0f; e.strip (0).presenceFreq = i; });
          const float off = sineGainDb (T4, f * (f < 3000.0f ? 4.0f : 0.25f), [i] (MixEngine& e) { e.strip (0).presenceDb = 10.0f; e.strip (0).presenceFreq = i; });
          if (! (on > 8.0f && off < on - 4.0f)) { ok = false; worst << juce::String (f) << " Hz: " << juce::String (on, 1) << " / " << juce::String (off, 1) << "  "; }
      }
      check (ok, "TG Mk IV PRESENCE +10: a bell at each of its 7 bell frequencies " + worst); }

    { const float top = sineGainDb (T4, 16000.0f, [] (MixEngine& e) { e.strip (0).presenceDb = 10.0f; e.strip (0).presenceFreq = 7; });
      const float mid = sineGainDb (T4, 2000.0f, [] (MixEngine& e) { e.strip (0).presenceDb = 10.0f; e.strip (0).presenceFreq = 7; });
      check (top > 8.0f && mid < 1.5f, "TG Mk IV PRESENCE's last position is a 10 kHz shelf (" + juce::String (top, 1) + " dB at 16 kHz, "
             + juce::String (mid, 1) + " at 2 kHz)"); }

    { const float mk4 = sineGainDb (T4, 5000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; });
      const float mk1 = sineGainDb (T, 5000.0f, [] (MixEngine& e) { e.strip (0).presenceDb = 10.0f; e.strip (0).presenceFreq = 5; });
      check (std::abs (mk4) < 0.5f && std::abs (mk1) < 0.5f, "TG: no treble on the Mk IV's channels, no presence on the Mk I's"); }

    { const float pk = sineGainDb (T, 5000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; });
      const float top = sineGainDb (T, 15000.0f, [] (MixEngine& e) { e.strip (0).trebleDb = -10.0f; });
      check (pk > 8.5f && top < -8.0f, "TG Mk I TREBLE: lift a bell at 5 kHz (" + juce::String (pk, 1) + " dB), cut a 10 kHz shelf ("
             + juce::String (top, 1) + " dB at 15 kHz)"); }

    for (auto d : { T, T4 })
    { const float on = sineGainDb (d, 2800.0f, [] (MixEngine& e) { e.mainPresenceDb = 10.0f; e.mainPresenceFreq = 4; });
      check (on > 8.0f, juce::String (deskInfo (d).name) + ": PRESENCE on the mains (" + juce::String (on, 1) + " dB at 2.8 kHz)"); }

    // compressor: steady tone well over the threshold. Above it the output rises 1 dB per 2 (COMP) or 8 (LIMIT).
    auto slope = [T] (int mode)
    {
        auto at = [&] (float levelDbfs)
        {
            return sineGainDb (T, 1000.0f, [mode] (MixEngine& e) { e.strip (0).compMode = mode; e.strip (0).compDb = 12.0f; e.strip (0).recovery = 0; },
                               levelDbfs) + levelDbfs;
        };
        const float a = at (-12.0f), b = at (-4.0f);
        return (b - a) / 8.0f;
    };
    { const float c = slope (1), l = slope (2);
      check (c > 0.4f && c < 0.65f, "TG COMP is about 2:1 (output slope " + juce::String (c, 2) + ")");
      check (l > 0.06f && l < 0.22f, "TG LIMIT is about 8:1 (output slope " + juce::String (l, 2) + ")"); }

    { const float off = sineGainDb (T, 1000.0f, [] (MixEngine& e) { e.strip (0).compMode = 1; e.strip (0).compDb = 0.0f; });
      check (std::abs (off) < 0.6f, "TG COMP at 0 leaves a 0 VU signal alone (" + juce::String (off, 2) + " dB)"); }

    { // recovery: after a loud burst, how much gain reduction is left 0.3 s later - more on the slower settings
      auto leftAfter = [T] (int rec)
      {
          Rig r (8, 480);
          r.engine.desk = (int) T;
          r.engine.strip (0).compMode = 2; r.engine.strip (0).compDb = 12.0f; r.engine.strip (0).recovery = rec;
          r.dc (0, 0.7f); r.run (20);
          r.dc (0, 0.001f); r.run (30);
          return r.engine.strip (0).gainReduction.load();
      };
      const float fast = leftAfter (0), slow = leftAfter (5);
      check (fast < 1.0f && slow > 5.0f, "TG RECOVERY: 0.1 s has let go after 0.3 s (" + juce::String (fast, 1) + " dB), 5 s hasn't ("
             + juce::String (slow, 1) + " dB)"); }

    { // spread on a tone panned hard left: 0 = mono (R follows L), 1 = as panned (R silent), 2 = wider (R opposite L)
      auto rOverL = [] (float sp, Desk d = Desk::tg12345)
      {
          Rig r (8, 480); r.engine.desk = (int) d; r.engine.spread = sp;
          r.engine.strip (0).pan = -1.0f;
          double ph = 0.0, lr = 0.0, ll = 0.0;
          for (int b = 0; b < 20; ++b)
          {
              for (int k = 0; k < 480; ++k) { r.in.setSample (0, k, 0.1f * (float) std::sin (ph)); ph += 0.13; }
              r.run (1);
              if (b >= 10) for (int k = 0; k < 480; ++k) { lr += (double) r.L (k) * r.R (k); ll += (double) r.L (k) * r.L (k); }
          }
          return (float) (lr / ll);
      };
      const float mono = rOverL (0.0f), normal = rOverL (1.0f), wide = rOverL (2.0f);
      check (std::abs (mono - 1.0f) < 0.02f && std::abs (normal) < 0.01f && wide < -0.3f,
             "TG SPREAD: 0 is mono, 1 is as panned, 2 is wider (R/L " + juce::String (mono, 2) + " / " + juce::String (normal, 2)
             + " / " + juce::String (wide, 2) + ")");
      check (std::abs (rOverL (0.0f, Desk::redd51) - 1.0f) < 0.05f && std::abs (rOverL (0.0f, Desk::redd37)) < 0.02f,
             "REDD.51's Spreader works; the REDD.37 has none"); }

    { MixEngine a, b; juce::StringArray na, nb;
      MixState::reset (a, na);
      a.desk = (int) T4; a.spread = 0.6f; a.mainPresenceDb = -3.0f; a.mainPresenceFreq = 7;
      a.strip (3).presenceDb = 4.0f; a.strip (3).presenceFreq = 6; a.strip (3).compMode = 2; a.strip (3).compDb = 9.0f; a.strip (3).recovery = 4;
      auto x = juce::parseXML (MixState::toXml (a, na)->toString());
      const bool ok = x != nullptr && MixState::fromXml (*x, b, nb);
      check (ok && b.getDesk() == T4 && approx (b.spread.load(), 0.6f) && approx (b.mainPresenceDb.load(), -3.0f) && b.mainPresenceFreq.load() == 7 && approx (b.strip (3).presenceDb.load(), 4.0f)
               && b.strip (3).presenceFreq.load() == 6 && b.strip (3).compMode.load() == 2 && approx (b.strip (3).compDb.load(), 9.0f)
               && b.strip (3).recovery.load() == 4,
             "Mix file keeps the TG's presence, compressor and spread"); }
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
               && b.strip (40).solo.load() && nb[4] == "Kick" && nb[5].isEmpty() && approx (b.masterDb.load(), -2.0f),
           "Mix file round trip");
    check (x->getNumChildElements() == 2, "Mix file keeps only strips that changed");

    {
        // older mix files stored an unnamed strip as its number: it now follows Hawthorne instead
        auto old = juce::parseXML ("<WHITMAN_MIX version=\"1\"><STRIP index=\"2\" name=\"3\" gain=\"-4\"/></WHITMAN_MIX>");
        MixEngine c; juce::StringArray nc;
        check (old != nullptr && MixState::fromXml (*old, c, nc) && nc[2].isEmpty() && approx (c.strip (2).gainDb.load(), -4.0f),
               "Old mix files: a strip named by its number counts as unnamed");
    }

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
    // Hawthorne's reel and track info (as Hawthorne's RouteHost::publishInfo writes it)
    {
        auto im = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD) sizeof (hawthorne::route::Info), infoMappingName);
        auto* inf = im != nullptr ? (hawthorne::route::Info*) MapViewOfFile (im, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (hawthorne::route::Info)) : nullptr;
        check (inf != nullptr, "Route info: test mapping made");
        if (inf != nullptr)
        {
            shared->heartbeatMs = (std::int64_t) GetTickCount64();
            RouteStatus rs;
            auto before = rs.poll();
            check (before.hawthorneRunning && ! before.hasInfo, "Route info: none until Hawthorne writes it (an older Hawthorne)");

            inf->magic = infoMagic; inf->version = infoVersion;
            inf->seq = 0;
            inf->seq.fetch_add (1);
            inf->numTracks = 32;
            juce::String ("Penny Lane take 7").copyToUTF8 (inf->reelName, sizeof (inf->reelName) - 1);
            juce::String ("Kick").copyToUTF8 (inf->trackNames[0], infoNameBytes - 1);
            juce::String (juce::CharPointer_UTF8 ("Vox caf\xc3\xa9")).copyToUTF8 (inf->trackNames[4], infoNameBytes - 1);
            inf->armed[4] = 1;
            for (int t = 0; t < 4; ++t) inf->toMix[t] = 1;
            inf->seq.fetch_add (1);
            shared->heartbeatMs = (std::int64_t) GetTickCount64();
            auto i2 = rs.poll();
            check (i2.hasInfo && i2.reelName == "Penny Lane take 7" && i2.trackNames.size() == 32 && i2.trackNames[0] == "Kick"
                       && i2.trackNames[4] == juce::String (juce::CharPointer_UTF8 ("Vox caf\xc3\xa9")) && i2.trackNames[1].isEmpty()
                       && i2.armed[4] && ! i2.armed[0] && i2.toMix[3] && ! i2.toMix[4],
                   "Route info: reel name, track names (UTF-8), armed and mix feeds read back");

            inf->seq.fetch_add (1);   // Hawthorne mid-write: keep what we had rather than read half a change
            juce::String ("Snare").copyToUTF8 (inf->trackNames[0], infoNameBytes - 1);
            shared->heartbeatMs = (std::int64_t) GetTickCount64();
            auto i3 = rs.poll();
            check (! i3.hasInfo, "Route info: never read while Hawthorne is writing it");
            inf->seq.fetch_add (1);
            UnmapViewOfFile (inf);
        }
        if (im != nullptr) CloseHandle (im);
    }
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
    testRedd();
    testRedd37AndTg();
    testMixFiles();
    testRouteStatus();
    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
