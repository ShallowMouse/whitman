#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>
#include "Desk.h"

namespace whitman
{

constexpr int maxStrips = 96;                 // Hawthorne Route's most tracks
constexpr float faderMinDb = -60.0f;          // at or below this the fader is off (-inf)
constexpr float faderMaxDb = 10.0f;
constexpr float echoMinDb = -60.0f;           // echo send / return off
constexpr float nominalDbfs = -18.0f;         // 0 VU, as in Hawthorne

inline float dbToGain (float db) noexcept     { return db <= faderMinDb ? 0.0f : juce::Decibels::decibelsToGain (db); }

/** Constant-power pan (-3 dB in the middle): pan -1 = hard left, +1 = hard right. */
inline void panGains (float pan, float& l, float& r) noexcept
{
    const float a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    l = std::cos (a);
    r = std::sin (a);
}

/** tanh, close enough for a valve curve and much cheaper. */
inline float fastTanh (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** A valve gain stage: unity slope for small signals, rounding off towards `ceiling`.
    Lopsided like a single-ended stage: one half of the wave rounds off sooner than the
    other (by `asym`), so as it's driven the grit is mostly even harmonics. The output
    never swings past `ceiling`. */
struct Valve
{
    float pos = 1.0f, neg = 1.0f;
    void set (float ceiling, float asym) noexcept
    {
        pos = ceiling * (1.0f - asym);
        neg = ceiling;
    }
    float operator() (float x) const noexcept
    {
        return x >= 0.0f ? pos * fastTanh (x / pos) : neg * fastTanh (x / neg);
    }
};

//==============================================================================
/** Which of the audio device's channels go where. With Hawthorne Route the driver
    names its channels ("Hawthorne track 7", "To Hawthorne master L"), so each strip
    is fed by its own tape track and the mix goes to Hawthorne's 2-track master. On
    any other device the inputs feed strips 1, 2, 3... and the mix goes to outputs 1-2. */
struct Routing
{
    std::vector<int> stripOfInput;   // per ACTIVE input channel (in the order the callback gets them): strip, or -1
    int outLeft = -1, outRight = -1; // ACTIVE output channel indices for the mix, or -1
    int numStrips = 0;               // strips the console shows
    bool isHawthorneRoute = false;

    static Routing make (const juce::StringArray& inputNames, const juce::BigInteger& activeInputs,
                         const juce::StringArray& outputNames, const juce::BigInteger& activeOutputs);

    /** "Hawthorne track 12" -> 12, otherwise 0. */
    static int trackNumberOf (const juce::String& channelName);
};

//==============================================================================
/** The mixer's audio side. Parameters are atomics the UI writes; the audio thread
    reads them and ramps gains over each block so moves don't click.

    Per strip:  GAIN -> channel amp -> BASS / TREBLE -> fader -> pan -> mix bus
                                                          \-> ECHO send -> chamber
    Master:     mix bus + chamber return -> master fader -> main amps -> out
    The clean WHITMAN desk has only the fader, pan and master; the modelled desks add
    their amplifiers, EQ and echo (see Desk.h). */
class MixEngine
{
public:
    struct Strip
    {
        std::atomic<float> gainDb   { 0.0f };       // fader
        std::atomic<float> pan      { 0.0f };
        std::atomic<bool>  mute     { false };
        std::atomic<bool>  solo     { false };
        std::atomic<float> inputDb  { 0.0f };       // GAIN into the channel amp
        std::atomic<float> bassDb   { 0.0f };       // 100 Hz shelf
        std::atomic<float> trebleDb { 0.0f };       // Pop: 5 kHz bell up / 10 kHz shelf down; Classic: 10 kHz shelf
        std::atomic<bool>  classic  { false };      // treble module
        std::atomic<float> echoDb   { echoMinDb };  // post-fader echo send
        std::atomic<float> peak     { 0.0f };       // post-fader, since the UI last took it

        // audio thread
        float curL = 0.0f, curR = 0.0f, curIn = 1.0f, curEcho = 0.0f;
        juce::IIRFilter bass, treble;
        float setBass = 1000.0f, setTreble = 1000.0f; bool setClassic = false;
        float hp = 0.0f, hpIn = 0.0f, lp = 0.0f;
        void resetState() noexcept;
    };

    MixEngine();

    void prepare (double sampleRate, int maxBlockSize);

    /** Call before the device starts (audioDeviceAboutToStart), not while it runs. */
    void setRouting (Routing r)       { routing = std::move (r); }
    const Routing& getRouting() const { return routing; }

    /** The audio callback: the device's active inputs in, its active outputs out. */
    void process (const float* const* in, int numIn, float* const* out, int numOut, int n) noexcept;

    Strip& strip (int i)              { return strips[(size_t) i]; }
    const Strip& strip (int i) const  { return strips[(size_t) i]; }

    std::atomic<int>   desk { (int) Desk::whitman };
    std::atomic<float> masterDb { 0.0f };
    std::atomic<float> echoReturnDb { -6.0f };
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<bool>  masterClipped { false };    // over 0 dBFS since the UI last cleared it

    bool anySolo() const noexcept;
    Desk getDesk() const noexcept     { return deskFromInt (desk.load (std::memory_order_relaxed)); }

    /** Peak since last asked (and reset to 0). UI thread. */
    static float takePeak (std::atomic<float>& p) noexcept { return p.exchange (0.0f, std::memory_order_relaxed); }

private:
    std::array<Strip, maxStrips> strips;
    Routing routing;
    juce::AudioBuffer<float> mix, scratch;
    float masterCur = 1.0f, echoRetCur = 0.0f;
    double sampleRate = 48000.0;
    int lastDesk = -1;

    // the desk's valves and filters, set when the desk changes
    Valve channelAmp, mainAmp;
    float hpCoef = 0.0f, lpCoef = 1.0f;
    std::array<float, 2> mainHp {}, mainHpIn {};

    // echo chamber
    juce::Reverb chamber;
    float echoHp = 0.0f, echoHpIn = 0.0f, echoLp = 0.0f, echoHpCoef = 0.0f, echoLpCoef = 1.0f;
    std::vector<float> preDelay;
    int preDelayPos = 0, preDelayLen = 1;

    void deskChanged (Desk d);
    void updateEq (Strip& s, float bassDb, float trebleDb, bool classic) noexcept;
    static float onePoleCoef (double hz, double sr) noexcept { return (float) std::exp (-juce::MathConstants<double>::twoPi * hz / sr); }

    static void raisePeak (std::atomic<float>& p, float v) noexcept
    {
        auto old = p.load (std::memory_order_relaxed);
        while (v > old && ! p.compare_exchange_weak (old, v, std::memory_order_relaxed)) {}
    }
};

} // namespace whitman
