#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <vector>
#include "Desk.h"

namespace whitman
{

//==============================================================================
/** The channel EQ of the desks that have a band EQ (Trident, Neve, SSL, Neotek): up to four
    bands, each switched or swept, and high- / low-pass filters. Desk.h's EMI desks keep their
    own EQ (bass / treble / presence) in MixEngine. */
enum class Shape { lowShelf, highShelf, bell, lowShelf6, highShelf6 };   // ...6: gentle first-order shelves

struct BandDef
{
    const char* label;
    Shape shape;
    std::vector<float> steps;     // switched frequencies; empty = swept between sweepLo and sweepHi
    float sweepLo = 0, sweepHi = 0;
    float maxDb = 15.0f;
    float q = 0.7f;               // bell Q (or shelf Q)
    // the band's switch, if it has one
    enum class Alt { none, shape, q } alt = Alt::none;
    const char* altLabel = nullptr;   // "BELL", "SHELF", "HI-Q", "NARROW"
    Shape altShape = Shape::bell;
    float altQ = 0.7f;
    float altMaxDb = 0.0f;        // the range with the switch in, if different (0 = the same)
    // a Q knob (SSL mids): gain range widens with Q (maxDbAtLowQ at qLo .. maxDb at qHi)
    bool qKnob = false;
    float qLo = 0.0f, qHi = 0.0f, maxDbAtLowQ = 0.0f;
    int defaultStep = 0;

    bool swept() const noexcept           { return steps.empty(); }
    int  numSteps() const noexcept        { return (int) steps.size(); }
    float hzAt (float pos) const noexcept;        // pos: a step index (switched) or 0..1 (swept)
    float qAt (float qPos) const noexcept         { return qKnob ? qLo * std::pow (qHi / qLo, juce::jlimit (0.0f, 1.0f, qPos)) : q; }
    float rangeAt (float qPos, bool altOn = false) const noexcept
    {
        if (altOn && alt != Alt::none && altMaxDb > 0.0f) return altMaxDb;
        return qKnob ? maxDbAtLowQ + (maxDb - maxDbAtLowQ) * juce::jlimit (0.0f, 1.0f, qPos) : maxDb;
    }
    float defaultFreq() const noexcept            { return swept() ? 0.5f : (float) defaultStep; }
};

struct FilterDef
{
    enum class Kind { none, switched, swept, stacked } kind = Kind::none;
    std::vector<float> steps;     // switched: the positions after OFF; stacked: each a button of its own
    float sweepLo = 0, sweepHi = 0;
    int order = 2;                // 2 = 12 dB/octave, 3 = 18 dB/octave
    const char* label = "";
};

struct BandEqDef
{
    std::vector<BandDef> bands;   // top (treble) first
    FilterDef hpf, lpf;
    bool sliders = false;         // the gains on slide faders (Trident) rather than knobs
};

/** nullptr for the desks without a band EQ. */
const BandEqDef* bandEqOf (Desk d);

/** Each band-EQ desk keeps its own settings, so switching desks doesn't lose them. */
constexpr int numBandEqDesks = 6;
inline int bandEqSlot (Desk d)
{
    switch (d)
    {
        case Desk::tridentA: return 0;
        case Desk::neve8028: return 1;
        case Desk::ssl4000e: return 2;
        case Desk::neotek:   return 3;
        case Desk::redd17:   return 4;
        case Desk::ua610:    return 5;
        default:             return -1;
    }
}
inline Desk bandEqDesk (int slot)
{
    static const Desk d[numBandEqDesks] = { Desk::tridentA, Desk::neve8028, Desk::ssl4000e, Desk::neotek, Desk::redd17, Desk::ua610 };
    return d[juce::jlimit (0, numBandEqDesks - 1, slot)];
}

//==============================================================================
/** One strip's settings for the band EQ (UI writes, audio reads). */
struct BandEqParams
{
    static constexpr int maxBands = 4;
    std::array<std::atomic<float>, maxBands> gain {}, freq {}, q {};
    std::array<std::atomic<bool>,  maxBands> alt {};
    std::atomic<float> hpfPos { 0.0f }, lpfPos { 0.0f };   // switched: 0 = off, k = step k; swept: 0 = off, else (0,1]
    std::atomic<int>   hpfMask { 0 }, lpfMask { 0 };       // stacked: a bit per button

    void reset (const BandEqDef* def) noexcept;
};

/** One strip's band-EQ filters (audio thread). */
class BandEq
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept;
    void process (const BandEqDef& def, const BandEqParams& p, float* x, int n) noexcept;

    /** The magnitude response at hz, for tests (dB). */
    static float responseDb (const BandEqDef& def, const BandEqParams& p, double sampleRate, float hz);

private:
    double sr = 48000.0;
    std::array<juce::IIRFilter, BandEqParams::maxBands> band;
    std::array<float, BandEqParams::maxBands> setGain {}, setFreq {}, setQ {};
    std::array<bool,  BandEqParams::maxBands> setAlt {}, live {};
    std::array<juce::IIRFilter, 6> hp, lp;        // up to 3 filters, each up to two sections
    int hpCount = 0, lpCount = 0;
    float setHpf = -1.0f, setLpf = -1.0f; int setHpMask = -1, setLpMask = -1;

    static std::vector<juce::IIRCoefficients> filterSections (bool high, float hz, int order, double sr);
    static std::vector<juce::IIRCoefficients> filterChain (const FilterDef& f, bool high, float pos, int mask, double sr);
    static juce::IIRCoefficients bandCoefs (const BandDef& b, float gainDb, float freq, float q, bool alt, double sr);

    friend struct BandEqTestAccess;
};

} // namespace whitman
