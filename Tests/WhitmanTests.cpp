// Headless tests for WHITMAN's engine. Prints PASS / FAIL per check; exit code 1 if anything failed.

#include <juce_core/juce_core.h>
#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/MixEngine.h"
#include "../Source/ReddEq.h"
#include "../Source/MixState.h"
#include "../Source/RouteStatus.h"
#include "../Source/CircuitSim.h"

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

    // the tone controls: switched bridged-T networks (ReddEq.h)
    check (ReddEq::snap (3.0f) == 4.0f && ReddEq::snap (-0.9f) == 0.0f && ReddEq::snap (15.0f) == 10.0f, "REDD tone controls are switches: 2 dB steps, +/-10");
    auto db = [] (const ReddEq::Analog& h, double hz) { return (float) ReddEq::responseDb (h, hz); };
    { const auto h = ReddEq::bass (10.0f, true);
      check (db (h, 20) > 8.5f && std::abs (db (h, 100) - 5.0f) < 0.3f && db (h, 1000) < 1.0f,
             "Classic BASS +10: shelf, half at 100 Hz (" + juce::String (db (h, 20), 1) + " / " + juce::String (db (h, 100), 1) + " / " + juce::String (db (h, 1000), 1) + " dB at 20 / 100 / 1k)"); }
    { const auto h = ReddEq::bass (-10.0f, true);
      check (db (h, 20) < -8.5f && std::abs (db (h, 100) + 5.0f) < 0.3f && db (h, 1000) > -1.0f, "Classic BASS -10: shelf down, half at 100 Hz"); }
    { const auto h = ReddEq::bass (10.0f, false);
      check (std::abs (db (h, 500) - 5.0f) < 0.4f && db (h, 100) > 8.0f && db (h, 3000) < 1.0f,
             "Pop BASS +10 reaches higher: half at 500 Hz (" + juce::String (db (h, 500), 1) + " dB)"); }
    { const auto h = ReddEq::treble (10.0f, false);
      float best = -100.0f; double at = 0;
      for (double f = 1000; f < 20000; f *= 1.01) if (db (h, f) > best) { best = db (h, f); at = f; }
      check (std::abs (at - 4700.0) < 150.0 && std::abs (best - 10.0f) < 0.05f && db (h, 1300.0 * 4700.0 / 3986.0) > 4.0f && db (h, 20000) < 6.0f && db (h, 300) < 1.0f,
             "Pop TREBLE +10: a broad bell at 4.7 kHz (peak " + juce::String (best, 1) + " dB at " + juce::String (at, 0) + " Hz)"); }
    { const auto h = ReddEq::treble (-10.0f, false);
      check (std::abs (db (h, 2300) + 5.0f) < 0.3f && db (h, 15000) < -8.5f, "Pop TREBLE -10: a shelf down, half at 2.3 kHz"); }
    { const auto h = ReddEq::treble (10.0f, true);
      check (std::abs (db (h, 10000) - 5.0f) < 0.3f && db (h, 1000) < 1.0f, "Classic TREBLE +10: a shelf up, half at 10 kHz"); }
    { const auto h = ReddEq::treble (4.0f, false);
      check (std::abs (db (h, 4700) - 4.0f) < 0.05f, "Pop TREBLE +4: the step's boost at the peak"); }
    { // and the engine runs them as they should
      const float pk = sineGainDb (R, 4700.0f, [] (MixEngine& e) { e.strip (0).trebleDb = 10.0f; });
      const float cl = sineGainDb (R, 50.0f, [] (MixEngine& e) { e.strip (0).bassDb = 10.0f; e.strip (0).classic = true; });
      check (std::abs (pk - 10.0f) < 0.8f && std::abs (cl - (float) ReddEq::responseDb (ReddEq::bass (10.0f, true), 50.0)) < 0.8f,
             "Engine: Pop +10 at 4.7 kHz " + juce::String (pk, 1) + " dB, Classic bass +10 at 50 Hz " + juce::String (cl, 1) + " dB"); }

    { // pan pots: switches, 5 positions a side; a tone through both sides of the stereo correlator
      auto ratio = [] (float pan)
      {
          Rig r (2, 480);
          r.engine.desk = (int) Desk::redd51;
          r.engine.strip (0).pan = pan;
          double ph = 0.0, l = 0.0, rr = 0.0;
          for (int b = 0; b < 20; ++b)
          {
              for (int k = 0; k < 480; ++k) { r.in.setSample (0, k, 0.05f * (float) std::sin (ph)); ph += 0.13; }
              r.run (1);
              if (b > 10) for (int k = 0; k < 480; ++k) { l += r.L (k) * r.L (k); rr += r.R (k) * r.R (k); }
          }
          return std::pair<double, float> (rr / l, r.engine.correlation.load());
      };
      const auto a = ratio (0.33f), b = ratio (0.4f), c = ratio (0.0f);
      check (std::abs (a.first - b.first) < 1.0e-3 && a.first > 1.5, "REDD pan: 0.33 lands on the 0.4 step");
      check (c.second > 0.99f, "Stereo correlator: a centred track reads +1 (" + juce::String (c.second, 3) + ")"); }

    { const float g = sineGainDb (R, 1000.0f, [] (MixEngine& e) { e.strip (0).inputDb = 10.0f; e.strip (0).gainDb = -10.0f; });
      check (std::abs (g) < 0.8f, "GAIN +10 with the fader 10 down: about the same level (" + juce::String (g, 2) + " dB)"); }

    { const float clean2 = harmonicDb (0.0f, 2), hot2 = harmonicDb (18.0f, 2), hot3 = harmonicDb (18.0f, 3);
      check (clean2 < -45.0f, "At 0 VU the channel amp is clean (2nd harmonic " + juce::String (clean2, 1) + " dB)");
      check (hot3 > -60.0f && hot3 > harmonicDb (0.0f, 3) + 15.0f, "GAIN +18 drives the valves (3rd harmonic " + juce::String (hot3, 1) + " dB)");
      check (hot3 > hot2, "REDD.47: its stages' 2nd harmonics cancel, so the 3rd leads (2nd " + juce::String (hot2, 1) + ", 3rd " + juce::String (hot3, 1) + ")"); }

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
      const float rail = dbuToPeak (ampSpecOf (Desk::redd51, true).clipDbu);
      check (most <= rail * 1.001f && most > rail * 0.85f, "REDD.51 main amps top out at their clip level, +24 dBu (peak " + juce::String (most, 3) + " of " + juce::String (rail, 3) + ")"); }

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
      check (wet > -45.0f && dry < -100.0f && clean < -120.0f,
             "ECHO: a chamber tail with the send up (" + juce::String (wet, 1) + " dB), none without it (" + juce::String (dry, 1) + ") or on the clean desk"); }

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

    { const float top37 = sineGainDb (Desk::redd37, 20000.0f, none), lo37 = sineGainDb (Desk::redd37, 50.0f, none);
      check (top37 > -1.5f && lo37 > -1.5f, "V72s: within about 1 dB from 50 Hz to 20 kHz, as measured (" + juce::String (lo37, 1) + " / " + juce::String (top37, 1) + " dB)"); }
    { const float h2 = harmonicDb (4.0f, 2, nominalDbfs, Desk::redd37), h3 = harmonicDb (4.0f, 3, nominalDbfs, Desk::redd37);
      check (h2 > h3, "V72s clip lopsided: 2nd harmonic leads (" + juce::String (h2, 1) + " vs 3rd " + juce::String (h3, 1) + " dB)"); }

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
             "REDD.51's Spreader works; the REDD.37 has none");
      const float rca = rOverL (1.0f, Desk::rca76d), r17 = rOverL (0.0f, Desk::redd17);
      check (std::abs (rca - 1.0f) < 0.01f, "RCA 76-D is mono: a track panned hard left comes out both sides (R/L " + juce::String (rca, 2) + ")");
      check (std::abs (r17 - 1.0f) < 0.05f, "REDD.17's Spreader works (R/L at 0: " + juce::String (r17, 2) + ")"); }

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
// Distortion against the published figures, and the four band-EQ desks
static float thdPct (Desk d, float dbu, float hz, std::function<void (MixEngine&)> setup = {})
{
    Rig r (8, 480);
    r.engine.desk = (int) d;
    r.engine.strip (0).pan = -1.0f;            // all of it left, at unity
    if (setup) setup (r.engine);
    const double sr = 48000.0;
    const float amp = dbuToPeak (dbu);
    std::vector<float> out;
    double ph = 0.0;
    const int blocks = 200;
    for (int b = 0; b < blocks; ++b)
    {
        for (int k = 0; k < 480; ++k) { r.in.setSample (0, k, amp * (float) std::sin (ph)); ph += juce::MathConstants<double>::twoPi * hz / sr; }
        r.run (1);
        if (b >= 100) for (int k = 0; k < 480; ++k) out.push_back (r.L (k));
    }
    auto goertzel = [&] (double f)
    {
        const double w = juce::MathConstants<double>::twoPi * f / sr, c = 2.0 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (auto v : out) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - c * s1 * s2));
    };
    const double fund = goertzel (hz);
    double harm = 0.0;
    for (int h = 2; h <= 7; ++h) if (hz * h < sr * 0.45) { const double a = goertzel (hz * h); harm += a * a; }
    return (float) (100.0 * std::sqrt (harm) / fund);
}

static float eqGainDb (Desk d, float hz, std::function<void (BandEqParams&)> set)
{
    BandEqParams p;
    p.reset (bandEqOf (d));
    set (p);
    return BandEq::responseDb (*bandEqOf (d), p, 48000.0, hz);
}

static void testAmpsAndBandDesks()
{
    // --- Neve 1073 / 1084 spec: THD no more than 0.07 % from 50 Hz to 10 kHz at +20 dBu out
    for (float hz : { 50.0f, 1000.0f, 10000.0f })
    {
        const float t = thdPct (Desk::neve8028, 20.0f, hz);
        check (t <= 0.07f && t > 0.005f, "Neve: THD at +20 dBu, " + juce::String (hz, 0) + " Hz: " + juce::String (t, 3) + " % (spec: no more than 0.07 %)");
    }
    { const float lo = thdPct (Desk::neve8028, 20.0f, 50.0f), hi = thdPct (Desk::neve8028, 20.0f, 1000.0f);
      check (lo > hi * 1.3f, "Neve: the transformers distort more in the low end (" + juce::String (lo, 3) + " % at 50 Hz, " + juce::String (hi, 3) + " % at 1 kHz)"); }
    { const float t = thdPct (Desk::neve8028, 25.5f, 1000.0f);
      check (t < 1.0f, "Neve: still clean at +25.5 dBu (spec: over +26 dBu out) - " + juce::String (t, 2) + " %"); }
    { bool flat = true; juce::String worst;
      for (float hz : { 20.0f, 40.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
      { const float g = sineGainDb (Desk::neve8028, hz, [] (MixEngine&) {}); if (std::abs (g) > 0.5f) { flat = false; worst << juce::String (hz) << " Hz " << juce::String (g, 2) << "  "; } }
      check (flat, "Neve: flat within 0.5 dB, 20 Hz to 20 kHz (spec) " + worst); }

    // --- REDD.47 (REDD.51): Chandler's faithful copy measures 0.15 % THD at +20 dBu out, more than +23 dBu available
    { const float t = thdPct (Desk::redd51, 20.0f, 1000.0f), hot = thdPct (Desk::redd51, 23.0f, 1000.0f);
      check (t > 0.08f && t < 0.3f && hot < 3.0f, "REDD.51: THD " + juce::String (t, 3) + " % at +20 dBu (REDD.47 measured 0.15 %), " + juce::String (hot, 2) + " % at +23 dBu"); }
    { bool flat = true; juce::String worst;
      for (float hz : { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f })
      { const float g = sineGainDb (Desk::redd51, hz, [] (MixEngine&) {}); if (std::abs (g) > 0.5f) { flat = false; worst << juce::String (hz) << " Hz " << juce::String (g, 2) << "  "; } }
      check (flat, "REDD.51: flat within 0.5 dB, 20 Hz to 20 kHz (REDD.47: -3 dB at 7 Hz and 40 kHz) " + worst); }

    // --- the ordering of how much each desk distorts at the same level
    { const float ssl = thdPct (Desk::ssl4000e, 20.0f, 1000.0f), neotek = thdPct (Desk::neotek, 20.0f, 1000.0f),
                  neve = thdPct (Desk::neve8028, 20.0f, 1000.0f), trident = thdPct (Desk::tridentA, 20.0f, 1000.0f),
                  r51 = thdPct (Desk::redd51, 14.0f, 1000.0f), r37 = thdPct (Desk::redd37, 14.0f, 1000.0f);
      check (ssl < 0.02f && neotek < 0.02f, "SSL and Neotek nearly distortion-free at +20 dBu (" + juce::String (ssl, 4) + " %, " + juce::String (neotek, 4) + " %)");
      check (ssl < neve && neotek < neve && neve < trident, "At +20 dBu: SSL / Neotek < Neve < Trident (" + juce::String (neve, 3) + " %, " + juce::String (trident, 3) + " %)");
      check (r51 < r37, "At +14 dBu: REDD.51 cleaner than the REDD.37 (" + juce::String (r51, 2) + " % vs " + juce::String (r37, 2) + " %)"); }
    { const float t = thdPct (Desk::ssl4000e, 28.0f, 1000.0f);
      check (t > 1.0f, "SSL clips hard above +26.5 dBu (" + juce::String (t, 1) + " % at +28)"); }

    // --- band EQ: each band at its frequency
    for (auto d : { Desk::tridentA, Desk::neve8028, Desk::ssl4000e, Desk::neotek })
    {
        const auto& def = *bandEqOf (d);
        bool ok = true; juce::String bad;
        for (int b = 0; b < (int) def.bands.size(); ++b)
        {
            const auto& band = def.bands[(size_t) b];
            const int stepsToTry = band.swept() ? 3 : band.numSteps();
            for (int k = 0; k < stepsToTry; ++k)
            {
                const float pos = band.swept() ? 0.15f + 0.35f * (float) k : (float) k;
                const float hz = band.hzAt (pos);
                if (hz > 18000.0f) continue;
                const bool shelf = band.shape != Shape::bell;
                // shelves: well past the corner; bells: at the centre
                const bool lowShelf = band.shape == Shape::lowShelf || band.shape == Shape::lowShelf6;
                const float at = shelf ? (lowShelf ? hz / 6.0f : juce::jmin (20000.0f, hz * 2.2f)) : hz;
                const bool nearTop = shelf && ! lowShelf && hz * 2.2f > 20000.0f;   // a shelf this high hasn't finished rising by 20 kHz
                if (at < 15.0f || at > 21000.0f) continue;
                const float g = eqGainDb (d, at, [&] (BandEqParams& p) { p.gain[(size_t) b] = band.maxDb; p.freq[(size_t) b] = pos; p.q[(size_t) b] = 1.0f; });
                const float want = band.rangeAt (1.0f);
                const bool gentle = band.shape == Shape::lowShelf6 || band.shape == Shape::highShelf6;
                if (! (g > want - (gentle ? 4.5f : nearTop ? 3.0f : 2.0f) && g < want + 1.0f)) { ok = false; bad << band.label << " " << juce::String (hz, 0) << ": " << juce::String (g, 1) << "  "; }
            }
        }
        check (ok, juce::String (deskInfo (d).name) + ": every band reaches its full boost at its frequency " + bad);
    }

    // Trident: the shelves are gentle 6 dB/octave; its mids interact
    { const float a = eqGainDb (Desk::tridentA, 400.0f, [] (BandEqParams& p) { p.gain[3] = 15.0f; p.freq[3] = 3; });   // bass shelf at 150 Hz
      const float b = eqGainDb (Desk::neve8028, 400.0f, [] (BandEqParams& p) { p.gain[2] = 16.0f; p.freq[2] = 2; });   // Neve low shelf at 110 Hz
      check (a > b, "Trident's 6 dB/octave bass shelf reaches further up than the Neve's (" + juce::String (a, 1) + " vs " + juce::String (b, 1) + " dB at 400 Hz)"); }

    // Neve Hi-Q narrows the mid
    { const float wide = eqGainDb (Desk::neve8028, 3200.0f, [] (BandEqParams& p) { p.gain[1] = 18.0f; p.freq[1] = 2; });
      const float narrow = eqGainDb (Desk::neve8028, 3200.0f, [] (BandEqParams& p) { p.gain[1] = 18.0f; p.freq[1] = 2; p.alt[1] = true; });
      check (narrow < wide - 3.0f, "Neve HI-Q narrows the mid (1.6 kHz boost, an octave up: " + juce::String (wide, 1) + " -> " + juce::String (narrow, 1) + " dB)"); }

    // SSL: the mids' range widens with Q (+/-15 at the widest, +/-18 at the narrowest)
    { const float lowQ = eqGainDb (Desk::ssl4000e, 1600.0f, [] (BandEqParams& p) { p.gain[1] = 18.0f; p.freq[1] = std::log (1600.0f / 600.0f) / std::log (7000.0f / 600.0f); p.q[1] = 0.0f; });
      const float highQ = eqGainDb (Desk::ssl4000e, 1600.0f, [] (BandEqParams& p) { p.gain[1] = 18.0f; p.freq[1] = std::log (1600.0f / 600.0f) / std::log (7000.0f / 600.0f); p.q[1] = 1.0f; });
      check (std::abs (lowQ - 15.0f) < 1.0f && std::abs (highQ - 18.0f) < 1.0f, "SSL HMF: +15 dB at Q 0.1, +18 at Q 3.5 (" + juce::String (lowQ, 1) + ", " + juce::String (highQ, 1) + ")"); }

    // filters
    { const float at = eqGainDb (Desk::neve8028, 80.0f, [] (BandEqParams& p) { p.hpfPos = 2.0f; });     // 80 Hz
      const float oct = eqGainDb (Desk::neve8028, 40.0f, [] (BandEqParams& p) { p.hpfPos = 2.0f; });
      check (at < -2.5f && at > -3.5f && oct - at < -14.0f, "Neve HPF 80 Hz, 18 dB/octave (" + juce::String (at, 1) + " dB at 80, " + juce::String (oct, 1) + " at 40)"); }
    { const float one = eqGainDb (Desk::tridentA, 50.0f, [] (BandEqParams& p) { p.hpfMask = 2; });
      const float two = eqGainDb (Desk::tridentA, 50.0f, [] (BandEqParams& p) { p.hpfMask = 2 | 4; });
      check (two < one - 6.0f, "Trident's high-pass filters stack (50 Hz: " + juce::String (one, 1) + " with one, " + juce::String (two, 1) + " with two)"); }
    { const float off = eqGainDb (Desk::ssl4000e, 20.0f, [] (BandEqParams&) {});
      const float on = eqGainDb (Desk::ssl4000e, 20.0f, [] (BandEqParams& p) { p.hpfPos = 0.5f; });
      check (std::abs (off) < 0.1f && on < -20.0f, "SSL HPF: off at 0, cuts when swept in (" + juce::String (on, 1) + " dB at 20 Hz)"); }

    // SSL dynamics
    { auto slopeAt = [] (float ratio)
      {
          auto at = [ratio] (float dbu) { return sineGainDb (Desk::ssl4000e, 1000.0f, [ratio] (MixEngine& e) { e.strip (0).sslRatio = ratio; e.strip (0).sslThresh = -10.0f; }, dbu - 22.0f) + dbu; };
          return (at (12.0f) - at (4.0f)) / 8.0f;
      };
      const float r4 = slopeAt (4.0f), inf = slopeAt (20.0f);
      check (std::abs (r4 - 0.25f) < 0.08f && inf < 0.06f, "SSL compressor: 4:1 is 4:1 (" + juce::String (r4, 2) + "), the end is a limiter (" + juce::String (inf, 2) + ")"); }
    { const float quiet = sineGainDb (Desk::ssl4000e, 1000.0f, [] (MixEngine& e) { e.strip (0).gateRange = 30.0f; e.strip (0).gateThresh = -10.0f; e.strip (0).gateMode = true; }, -40.0f);
      const float loud = sineGainDb (Desk::ssl4000e, 1000.0f, [] (MixEngine& e) { e.strip (0).gateRange = 30.0f; e.strip (0).gateThresh = -10.0f; e.strip (0).gateMode = true; }, -12.0f);
      check (quiet < -28.0f && std::abs (loud) < 0.5f, "SSL gate: under the threshold it shuts by its range (" + juce::String (quiet, 1) + " dB), over it it's open (" + juce::String (loud, 1) + ")"); }
    { const float ex = sineGainDb (Desk::ssl4000e, 1000.0f, [] (MixEngine& e) { e.strip (0).gateRange = 40.0f; e.strip (0).gateThresh = 0.0f; }, -40.0f + 22.0f - 22.0f - 10.0f + 10.0f);
      check (ex < -5.0f && ex > -40.0f, "SSL expander: below the threshold it turns down gradually, not all the way (" + juce::String (ex, 1) + " dB)"); }

    // mix files keep each desk's EQ apart, and the SSL dynamics
    { MixEngine a, b; juce::StringArray na, nb;
      MixState::reset (a, na);
      a.strip (1).eqFor (Desk::neve8028)->gain[1] = 6.0f;
      a.strip (1).eqFor (Desk::neve8028)->freq[1] = 4.0f;
      a.strip (1).eqFor (Desk::tridentA)->hpfMask = 5;
      a.strip (1).eqFor (Desk::ssl4000e)->q[2] = 0.8f;
      a.strip (1).sslRatio = 8.0f; a.strip (1).gateRange = 20.0f; a.strip (1).gateMode = true;
      auto x = juce::parseXML (MixState::toXml (a, na)->toString());
      const bool ok = x != nullptr && MixState::fromXml (*x, b, nb);
      check (ok && approx (b.strip (1).eqFor (Desk::neve8028)->gain[1].load(), 6.0f) && approx (b.strip (1).eqFor (Desk::neve8028)->freq[1].load(), 4.0f)
               && b.strip (1).eqFor (Desk::tridentA)->hpfMask.load() == 5 && approx (b.strip (1).eqFor (Desk::ssl4000e)->q[2].load(), 0.8f)
               && b.strip (1).eqFor (Desk::neotek)->gain[0].load() == 0.0f
               && approx (b.strip (1).sslRatio.load(), 8.0f) && approx (b.strip (1).gateRange.load(), 20.0f) && b.strip (1).gateMode.load(),
             "Mix file keeps each desk's EQ separately, and the SSL dynamics"); }
}

//==============================================================================
// The circuit simulator, against circuits worked out by hand
static float runnerGainDb (CircuitRunner& r, double hz, double sr = 48000.0)
{
    double ph = 0.0, sin2 = 0.0, sout2 = 0.0;
    const int total = (int) (sr * 0.5);
    for (int i = 0; i < total; ++i)
    {
        const float x = 0.1f * (float) std::sin (ph);
        ph += juce::MathConstants<double>::twoPi * hz / sr;
        const float y = r.processSample (x);
        if (i > total / 2) { sin2 += (double) x * x; sout2 += (double) y * y; }
    }
    return (float) (10.0 * std::log10 (sout2 / sin2));
}

static float exactDb (const Circuit& c, double hz) { return (float) (20.0 * std::log10 (std::abs (c.responseAt (hz)))); }

static void testCircuitSim()
{
    const double pi = juce::MathConstants<double>::pi;
    { // RC low-pass, 1 kHz corner
        Circuit c (3); c.setInput (1); c.setOutput (2);
        c.resistor (1, 2, 1000.0); c.capacitor (2, 0, 1.0 / (2.0 * pi * 1000.0 * 1000.0));
        CircuitRunner r; r.prepare (c, 48000.0);
        const float exact = exactDb (c, 1000.0), sim = runnerGainDb (r, 1000.0);
        check (std::abs (exact + 3.0103f) < 0.01f && std::abs (sim - exact) < 0.05f,
               "Circuit: RC low-pass is -3.01 dB at its corner (exact " + juce::String (exact, 3) + ", simulated " + juce::String (sim, 3) + ")");
    }
    { // series RLC band-pass: peak at 1/(2 pi sqrt(LC)), Q = sqrt(L/C)/R
        Circuit c (4); c.setInput (1); c.setOutput (3);
        const double L = 0.1, C = 1.0e-7, R = 200.0;     // f0 = 1591.5 Hz, Q = 5
        c.inductor (1, 2, L); c.capacitor (2, 3, C); c.resistor (3, 0, R);
        const double f0 = 1.0 / (2.0 * pi * std::sqrt (L * C));
        const float peak = exactDb (c, f0), edge = exactDb (c, f0 * (std::sqrt (1.0 + 1.0 / 100.0) + 0.1));
        CircuitRunner r; r.prepare (c, 48000.0);
        const float sim = runnerGainDb (r, f0);
        check (std::abs (peak) < 0.01f && std::abs (edge + 3.01f) < 0.05f && std::abs (sim - peak) < 0.1f,
               "Circuit: RLC band-pass peaks at 0 dB at its resonance, -3 dB at the Q 5 band edge (" + juce::String (edge, 2) + "), simulated " + juce::String (sim, 2));
    }
    { // inverting op-amp, gain -10
        Circuit c (4); c.setInput (1); c.setOutput (3);
        c.resistor (1, 2, 1000.0); c.resistor (2, 3, 10000.0); c.opamp (0, 2, 3);
        CircuitRunner r; r.prepare (c, 48000.0);
        const auto h = c.responseAt (1000.0);
        const float y = r.processSample (0.05f);
        check (std::abs (h.real() + 10.0) < 1.0e-6 && std::abs (y + 0.5f) < 1.0e-5f, "Circuit: inverting op-amp, gain -10 (exact " + juce::String (h.real(), 3) + ", simulated " + juce::String (y / 0.05f, 3) + ")");
    }
    { // Sallen-Key low-pass, unity gain: f0 = 1/(2 pi R sqrt(C1 C2)), Q = sqrt(C1/C2)/2
        Circuit c (5); c.setInput (1); c.setOutput (4);
        const double R = 10000.0, C1 = 2.0e-8, C2 = 1.0e-8;
        c.resistor (1, 2, R); c.resistor (2, 3, R); c.capacitor (2, 4, C1); c.capacitor (3, 0, C2);
        c.vcvs (3, 0, 4, 1.0);
        const double f0 = 1.0 / (2.0 * pi * R * std::sqrt (C1 * C2)), Q = std::sqrt (C1 / C2) / 2.0;
        const float atF0 = exactDb (c, f0);
        CircuitRunner r; r.prepare (c, 48000.0);
        const float sim = runnerGainDb (r, f0);
        check (std::abs (atF0 - (float) (20.0 * std::log10 (Q))) < 0.01f && std::abs (sim - atF0) < 0.1f,
               "Circuit: Sallen-Key low-pass, " + juce::String (20.0 * std::log10 (Q), 2) + " dB at f0 (exact " + juce::String (atF0, 2) + ", simulated " + juce::String (sim, 2) + ")");
    }
    { // a passive bass / treble tone network (several reactive parts): simulation matches the exact response below 8 kHz
        Circuit c (7); c.setInput (1); c.setOutput (5);
        c.resistor (1, 2, 22000.0); c.capacitor (2, 3, 4.7e-9); c.resistor (3, 5, 100000.0); c.capacitor (1, 4, 22e-9);
        c.resistor (4, 5, 47000.0); c.capacitor (5, 6, 100e-9); c.resistor (6, 0, 4700.0); c.resistor (5, 0, 100000.0);
        c.inductor (2, 0, 0.5, 30.0);
        CircuitRunner r; r.prepare (c, 48000.0);
        float worst = 0.0f;
        for (double hz : { 30.0, 100.0, 300.0, 1000.0, 3000.0, 8000.0 })
        { r.reset(); worst = juce::jmax (worst, std::abs (runnerGainDb (r, hz) - exactDb (c, hz))); }
        check (worst < 0.15f, "Circuit: a passive tone network matches its exact response, 30 Hz to 8 kHz (worst " + juce::String (worst, 3) + " dB)");
    }
    { // moving a value keeps the state: no jump
        Circuit c (3); c.setInput (1); c.setOutput (2);
        const int rr = c.resistor (1, 2, 1000.0); c.capacitor (2, 0, 1.0e-6);
        CircuitRunner r; r.prepare (c, 48000.0);
        float last = 0.0f;
        for (int i = 0; i < 2000; ++i) last = r.processSample (1.0f);
        c.setValue (rr, 1100.0); r.prepare (c, 48000.0);
        const float next = r.processSample (1.0f);
        check (std::abs (next - last) < 0.01f, "Circuit: changing a value mid-signal doesn't jump (" + juce::String (last, 4) + " -> " + juce::String (next, 4) + ")");
    }
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
    testAmpsAndBandDesks();
    testCircuitSim();
    testMixFiles();
    testRouteStatus();
    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
