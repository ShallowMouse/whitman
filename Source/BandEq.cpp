#include "BandEq.h"

namespace whitman
{

/*  Sources for the definitions below (see also Desk.h):
    - Trident A-Range: Universal Audio's Trident A-Range EQ manual; Mix "Trident-MTA A Range"; Barry Rudolph's
      Trident A-Range review. Four bands, each a 4-position frequency switch, +/-15 dB on slide faders; the shelves
      6 dB/octave, the mids inductor bells about 1 to 1.4 octaves wide. High-pass 25 / 50 / 100 Hz and low-pass
      9 / 12 / 15 kHz, which stack (UA: high-pass 18 dB/octave, low-pass 12 dB/octave - used; Mix says second order).
    - Neve 1084: the AMS Neve / Economik module specs. High shelf 10 / 12 / 16 kHz +/-16 dB; mid bell 360 Hz to
      7.2 kHz +/-18 dB with Hi-Q; low shelf 35 / 60 / 110 / 220 Hz +/-16 dB; high-pass 50 / 80 / 160 / 300 Hz and
      low-pass 18 / 10 / 8 / 6 kHz, both 18 dB/octave. (The Q values aren't published: 0.8 normal, 2 Hi-Q here.)
    - SSL 4000 E (black knob): Waves SSL E-Channel docs (Joseph Nilo's write-up). HF 1.5-16 kHz shelf +/-16.5 dB or
      bell +/-18; HMF 600 Hz-7 kHz and LMF 200 Hz-2.5 kHz, Q 0.1-3.5, +/-15 dB at the widest to +/-18 at the
      narrowest; LF 30-450 Hz shelf +/-16.5 or bell +/-18; high-pass 16-350 Hz 18 dB/octave, low-pass 3-22 kHz
      12 dB/octave.
    - Neotek Elite: its operator's guide. State-variable parametric: low band to 20 Hz-400 Hz and high band up to
      20 kHz, each a peak or (pulled) a Bessel shelf; low-mid down to 50 Hz, high-mid up to 8 kHz, each wide or
      narrow ('a bit wider than third octave'); high-pass 20-375 Hz, two-pole. The guide doesn't give the bands'
      other ends or the gain range: 1.5-20 kHz, 400 Hz-8 kHz, 50 Hz-2 kHz and +/-15 dB are guesses.   */

const BandEqDef* bandEqOf (Desk d)
{
    using A = BandDef::Alt;
    static const BandEqDef trident = []
    {
        BandEqDef e;
        e.sliders = true;
        BandDef hi  { "TREBLE", Shape::highShelf6, { 8000, 10000, 12000, 15000 } };  hi.maxDb = 15; hi.defaultStep = 1;
        BandDef hm  { "HI MID", Shape::bell, { 3000, 5000, 7000, 9000 } };           hm.maxDb = 15; hm.q = 1.1f; hm.defaultStep = 1;
        BandDef lm  { "LO MID", Shape::bell, { 250, 500, 1000, 2000 } };             lm.maxDb = 15; lm.q = 1.1f; lm.defaultStep = 1;
        BandDef lo  { "BASS",   Shape::lowShelf6, { 50, 80, 100, 150 } };            lo.maxDb = 15; lo.defaultStep = 1;
        e.bands = { hi, hm, lm, lo };
        e.hpf = { FilterDef::Kind::stacked, { 25, 50, 100 }, 0, 0, 3, "HPF" };
        e.lpf = { FilterDef::Kind::stacked, { 9000, 12000, 15000 }, 0, 0, 2, "LPF" };
        return e;
    }();
    static const BandEqDef neve = []
    {
        BandEqDef e;
        BandDef hi  { "HIGH", Shape::highShelf, { 10000, 12000, 16000 } };                       hi.maxDb = 16; hi.q = 0.6f; hi.defaultStep = 1;
        BandDef mid { "MID",  Shape::bell, { 360, 700, 1600, 3200, 4800, 7200 } };               mid.maxDb = 18; mid.q = 0.8f; mid.defaultStep = 2;
        mid.alt = A::q; mid.altLabel = "HI-Q"; mid.altQ = 2.0f;
        BandDef lo  { "LOW",  Shape::lowShelf, { 35, 60, 110, 220 } };                           lo.maxDb = 16; lo.q = 0.6f; lo.defaultStep = 1;
        e.bands = { hi, mid, lo };
        e.hpf = { FilterDef::Kind::switched, { 50, 80, 160, 300 }, 0, 0, 3, "HPF" };
        e.lpf = { FilterDef::Kind::switched, { 18000, 10000, 8000, 6000 }, 0, 0, 3, "LPF" };
        return e;
    }();
    static const BandEqDef ssl = []
    {
        BandEqDef e;
        BandDef hf  { "HF",  Shape::highShelf, {}, 1500, 16000 };  hf.maxDb = 16.5f; hf.q = 0.7f;
        hf.alt = A::shape; hf.altLabel = "BELL"; hf.altShape = Shape::bell; hf.altQ = 0.7f; hf.altMaxDb = 18.0f;
        BandDef hmf { "HMF", Shape::bell, {}, 600, 7000 };         hmf.maxDb = 18; hmf.qKnob = true; hmf.qLo = 0.1f; hmf.qHi = 3.5f; hmf.maxDbAtLowQ = 15;
        BandDef lmf { "LMF", Shape::bell, {}, 200, 2500 };         lmf.maxDb = 18; lmf.qKnob = true; lmf.qLo = 0.1f; lmf.qHi = 3.5f; lmf.maxDbAtLowQ = 15;
        BandDef lf  { "LF",  Shape::lowShelf, {}, 30, 450 };       lf.maxDb = 16.5f; lf.q = 0.7f;
        lf.alt = A::shape; lf.altLabel = "BELL"; lf.altShape = Shape::bell; lf.altQ = 0.7f; lf.altMaxDb = 18.0f;
        e.bands = { hf, hmf, lmf, lf };
        e.hpf = { FilterDef::Kind::swept, {}, 16, 350, 3, "HPF" };
        e.lpf = { FilterDef::Kind::swept, {}, 3000, 22000, 2, "LPF" };
        return e;
    }();
    static const BandEqDef neotek = []
    {
        BandEqDef e;
        BandDef hi  { "HIGH",   Shape::bell, {}, 1500, 20000 };  hi.maxDb = 15; hi.q = 0.7f;
        hi.alt = A::shape; hi.altLabel = "SHELF"; hi.altShape = Shape::highShelf; hi.altQ = 0.55f;
        BandDef hm  { "HI MID", Shape::bell, {}, 400, 8000 };    hm.maxDb = 15; hm.q = 0.7f;
        hm.alt = A::q; hm.altLabel = "NARROW"; hm.altQ = 3.5f;
        BandDef lm  { "LO MID", Shape::bell, {}, 50, 2000 };     lm.maxDb = 15; lm.q = 0.7f;
        lm.alt = A::q; lm.altLabel = "NARROW"; lm.altQ = 3.5f;
        BandDef lo  { "LOW",    Shape::bell, {}, 20, 400 };      lo.maxDb = 15; lo.q = 0.7f;
        lo.alt = A::shape; lo.altLabel = "SHELF"; lo.altShape = Shape::lowShelf; lo.altQ = 0.55f;
        e.bands = { hi, hm, lm, lo };
        e.hpf = { FilterDef::Kind::swept, {}, 20, 375, 2, "HPF" };
        return e;
    }();

    switch (d)
    {
        case Desk::tridentA:  return &trident;
        case Desk::neve8028:  return &neve;
        case Desk::ssl4000e:  return &ssl;
        case Desk::neotek:    return &neotek;
        default:              return nullptr;
    }
}

float BandDef::hzAt (float pos) const noexcept
{
    if (swept())
        return sweepLo * std::pow (sweepHi / sweepLo, juce::jlimit (0.0f, 1.0f, pos));
    return steps[(size_t) juce::jlimit (0, numSteps() - 1, juce::roundToInt (pos))];
}

void BandEqParams::reset (const BandEqDef* def) noexcept
{
    for (int i = 0; i < maxBands; ++i)
    {
        gain[(size_t) i] = 0.0f;
        q[(size_t) i] = 0.5f;
        alt[(size_t) i] = false;
        freq[(size_t) i] = def != nullptr && i < (int) def->bands.size() ? def->bands[(size_t) i].defaultFreq() : 0.5f;
    }
    hpfPos = 0.0f; lpfPos = 0.0f; hpfMask = 0; lpfMask = 0;
}

//==============================================================================
void BandEq::reset() noexcept
{
    for (auto& f : band) f.reset();
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    setGain.fill (1000.0f); setFreq.fill (-1.0f); setQ.fill (-1.0f); live.fill (false);
    setHpf = setLpf = -1.0f; setHpMask = setLpMask = -1;
    hpCount = lpCount = 0;
}

static juce::IIRCoefficients firstOrder (double b0, double b1, double a1)
{
    return juce::IIRCoefficients (b0, b1, 0.0, 1.0, a1, 0.0);
}

juce::IIRCoefficients BandEq::bandCoefs (const BandDef& b, float gainDb, float freq, float qPos, bool alt, double sr)
{
    const double hz = juce::jlimit (10.0, sr * 0.45, (double) b.hzAt (freq));
    const float range = b.rangeAt (qPos, alt);
    const double g = juce::Decibels::decibelsToGain (juce::jlimit (-range, range, gainDb));
    Shape shape = b.shape;
    double q = b.qAt (qPos);
    if (alt && b.alt == BandDef::Alt::shape) { shape = b.altShape; q = b.altQ; }
    if (alt && b.alt == BandDef::Alt::q)     q = b.altQ;

    switch (shape)
    {
        case Shape::lowShelf:   return juce::IIRCoefficients::makeLowShelf (sr, hz, q, g);
        case Shape::highShelf:  return juce::IIRCoefficients::makeHighShelf (sr, hz, q, g);
        case Shape::bell:       return juce::IIRCoefficients::makePeakFilter (sr, hz, q, g);
        case Shape::lowShelf6:
        {
            // H(s) = (s + G wc) / (s + wc), bilinear
            const double K = std::tan (juce::MathConstants<double>::pi * hz / sr);
            return firstOrder ((1.0 + g * K) / (1.0 + K), (g * K - 1.0) / (1.0 + K), (K - 1.0) / (1.0 + K));
        }
        case Shape::highShelf6:
        {
            // H(s) = (G s + wc) / (s + wc)
            const double K = std::tan (juce::MathConstants<double>::pi * hz / sr);
            return firstOrder ((g + K) / (1.0 + K), (K - g) / (1.0 + K), (K - 1.0) / (1.0 + K));
        }
    }
    return juce::IIRCoefficients::makePeakFilter (sr, hz, q, 1.0);
}

std::vector<juce::IIRCoefficients> BandEq::filterSections (bool high, float hz, int order, double sr)
{
    std::vector<juce::IIRCoefficients> c;
    const double f = juce::jlimit (10.0, sr * 0.45, (double) hz);
    // Butterworth, -3 dB at the frequency: 2nd order Q 0.707; 3rd order = a Q 1.0 pair and a real pole
    const double q = order >= 3 ? 1.0 : 0.7071;
    c.push_back (high ? juce::IIRCoefficients::makeHighPass (sr, f, q) : juce::IIRCoefficients::makeLowPass (sr, f, q));
    if (order >= 3)
    {
        const double K = std::tan (juce::MathConstants<double>::pi * f / sr);
        c.push_back (high ? firstOrder (1.0 / (1.0 + K), -1.0 / (1.0 + K), (K - 1.0) / (1.0 + K))
                          : firstOrder (K / (1.0 + K), K / (1.0 + K), (K - 1.0) / (1.0 + K)));
    }
    return c;
}

std::vector<juce::IIRCoefficients> BandEq::filterChain (const FilterDef& f, bool high, float pos, int mask, double sr)
{
    std::vector<juce::IIRCoefficients> all;
    auto add = [&] (float hz) { for (auto& c : filterSections (high, hz, f.order, sr)) all.push_back (c); };
    switch (f.kind)
    {
        case FilterDef::Kind::none: break;
        case FilterDef::Kind::switched:
        {
            const int k = juce::roundToInt (pos);
            if (k >= 1 && k <= (int) f.steps.size()) add (f.steps[(size_t) k - 1]);
            break;
        }
        case FilterDef::Kind::swept:
            if (pos > 0.001f) add (f.sweepLo * std::pow (f.sweepHi / f.sweepLo, juce::jlimit (0.0f, 1.0f, pos)));
            break;
        case FilterDef::Kind::stacked:
            for (int i = 0; i < (int) f.steps.size(); ++i)
                if ((mask >> i) & 1) add (f.steps[(size_t) i]);
            break;
    }
    return all;
}

void BandEq::process (const BandEqDef& def, const BandEqParams& p, float* x, int n) noexcept
{
    // filters first, then the bands
    const float hpf = p.hpfPos.load (std::memory_order_relaxed), lpf = p.lpfPos.load (std::memory_order_relaxed);
    const int hm = p.hpfMask.load (std::memory_order_relaxed), lm = p.lpfMask.load (std::memory_order_relaxed);
    if (hpf != setHpf || hm != setHpMask)
    {
        setHpf = hpf; setHpMask = hm;
        const auto c = filterChain (def.hpf, true, hpf, hm, sr);
        hpCount = juce::jmin ((int) hp.size(), (int) c.size());
        for (int i = 0; i < hpCount; ++i) { hp[(size_t) i].setCoefficients (c[(size_t) i]); }
    }
    if (lpf != setLpf || lm != setLpMask)
    {
        setLpf = lpf; setLpMask = lm;
        const auto c = filterChain (def.lpf, false, lpf, lm, sr);
        lpCount = juce::jmin ((int) lp.size(), (int) c.size());
        for (int i = 0; i < lpCount; ++i) { lp[(size_t) i].setCoefficients (c[(size_t) i]); }
    }
    for (int i = 0; i < hpCount; ++i) hp[(size_t) i].processSamples (x, n);
    for (int i = 0; i < lpCount; ++i) lp[(size_t) i].processSamples (x, n);

    for (int b = 0; b < (int) def.bands.size() && b < BandEqParams::maxBands; ++b)
    {
        const auto i = (size_t) b;
        const float g = p.gain[i].load (std::memory_order_relaxed), f = p.freq[i].load (std::memory_order_relaxed);
        const float q = p.q[i].load (std::memory_order_relaxed);
        const bool a = p.alt[i].load (std::memory_order_relaxed);
        if (g == 0.0f) { if (live[i]) { band[i].reset(); live[i] = false; setGain[i] = 1000.0f; } continue; }
        if (g != setGain[i] || f != setFreq[i] || q != setQ[i] || a != setAlt[i])
        {
            setGain[i] = g; setFreq[i] = f; setQ[i] = q; setAlt[i] = a;
            band[i].setCoefficients (bandCoefs (def.bands[i], g, f, q, a, sr));
        }
        band[i].processSamples (x, n);
        live[i] = true;
    }
}

float BandEq::responseDb (const BandEqDef& def, const BandEqParams& p, double sampleRate, float hz)
{
    // run a sine through a fresh BandEq and compare levels
    BandEq eq;
    eq.prepare (sampleRate);
    const int n = 512, blocks = (int) (sampleRate * 0.6 / n);
    std::vector<float> buf ((size_t) n);
    double ph = 0.0, sumIn = 0.0, sumOut = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int k = 0; k < n; ++k) { buf[(size_t) k] = 0.1f * (float) std::sin (ph); ph += juce::MathConstants<double>::twoPi * hz / sampleRate; }
        const double before = [&] { double s = 0; for (auto v : buf) s += (double) v * v; return s; }();
        eq.process (def, p, buf.data(), n);
        if (b >= blocks / 2)
        {
            sumIn += before;
            for (auto v : buf) sumOut += (double) v * v;
        }
    }
    return (float) (10.0 * std::log10 ((sumOut + 1.0e-30) / sumIn));
}

} // namespace whitman
