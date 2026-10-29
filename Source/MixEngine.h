#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>

namespace whitman
{

constexpr int maxStrips = 96;                 // Hawthorne Route's most tracks
constexpr float faderMinDb = -60.0f;          // at or below this the fader is off (-inf)
constexpr float faderMaxDb = 10.0f;

inline float dbToGain (float db) noexcept     { return db <= faderMinDb ? 0.0f : juce::Decibels::decibelsToGain (db); }

/** Constant-power pan (-3 dB in the middle): pan -1 = hard left, +1 = hard right. */
inline void panGains (float pan, float& l, float& r) noexcept
{
    const float a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    l = std::cos (a);
    r = std::sin (a);
}

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
    reads them and ramps gains over each block so moves don't click. */
class MixEngine
{
public:
    struct Strip
    {
        std::atomic<float> gainDb { 0.0f };
        std::atomic<float> pan    { 0.0f };
        std::atomic<bool>  mute   { false };
        std::atomic<bool>  solo   { false };
        std::atomic<float> peak   { 0.0f };    // post-fader, since the UI last took it

        // audio thread
        float curL = 0.0f, curR = 0.0f;
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

    std::atomic<float> masterDb { 0.0f };
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<bool>  masterClipped { false };    // over 0 dBFS since the UI last cleared it

    bool anySolo() const noexcept;

    /** Peak since last asked (and reset to 0). UI thread. */
    static float takePeak (std::atomic<float>& p) noexcept { return p.exchange (0.0f, std::memory_order_relaxed); }

private:
    std::array<Strip, maxStrips> strips;
    Routing routing;
    juce::AudioBuffer<float> mix;
    float masterCur = 1.0f;

    static void raisePeak (std::atomic<float>& p, float v) noexcept
    {
        auto old = p.load (std::memory_order_relaxed);
        while (v > old && ! p.compare_exchange_weak (old, v, std::memory_order_relaxed)) {}
    }
};

} // namespace whitman
