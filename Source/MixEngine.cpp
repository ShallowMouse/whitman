#include "MixEngine.h"

namespace whitman
{

int Routing::trackNumberOf (const juce::String& name)
{
    // the driver's inputs: "Hawthorne track 7" (not "To Hawthorne track 7", which is an output)
    const auto t = name.trim();
    if (! t.startsWithIgnoreCase ("Hawthorne track ")) return 0;
    const auto n = t.fromLastOccurrenceOf (" ", false, false).getIntValue();
    return n >= 1 && n <= maxStrips ? n : 0;
}

Routing Routing::make (const juce::StringArray& inputNames, const juce::BigInteger& activeInputs,
                       const juce::StringArray& outputNames, const juce::BigInteger& activeOutputs)
{
    Routing r;

    // Hawthorne Route?
    for (auto& n : inputNames)
        if (trackNumberOf (n) > 0) { r.isHawthorneRoute = true; break; }

    int activeIndex = 0;
    for (int ch = 0; ch < inputNames.size(); ++ch)
    {
        if (! activeInputs[ch]) continue;
        int strip = -1;
        if (r.isHawthorneRoute)
        {
            if (const int t = trackNumberOf (inputNames[ch]); t > 0) strip = t - 1;   // master and LTC: not mixed
        }
        else if (activeIndex < maxStrips)
        {
            strip = activeIndex;
        }
        r.stripOfInput.push_back (strip);
        if (strip >= 0) r.numStrips = juce::jmax (r.numStrips, strip + 1);
        ++activeIndex;
    }

    activeIndex = 0;
    int firstTwo[2] = { -1, -1 }, found = 0;
    for (int ch = 0; ch < outputNames.size(); ++ch)
    {
        if (! activeOutputs[ch]) continue;
        const auto n = outputNames[ch].trim();
        if (n.equalsIgnoreCase ("To Hawthorne master L")) r.outLeft = activeIndex;
        if (n.equalsIgnoreCase ("To Hawthorne master R")) r.outRight = activeIndex;
        if (found < 2) firstTwo[found++] = activeIndex;
        ++activeIndex;
    }

    if (! r.isHawthorneRoute)
    {
        r.outLeft = firstTwo[0];
        r.outRight = firstTwo[1];
    }
    return r;
}

//==============================================================================
void MixEngine::Strip::resetState() noexcept
{
    curL = curR = curEcho = 0.0f;
    curIn = juce::Decibels::decibelsToGain (inputDb.load());
    bass.reset();
    treble.reset();
    setBass = setTreble = 1000.0f;   // force new coefficients
    hp = hpIn = lp = 0.0f;
}

MixEngine::MixEngine() = default;

void MixEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    const int n = juce::jmax (1, maxBlockSize);
    mix.setSize (2, n, false, false, true);
    scratch.setSize (4, n, false, false, true);    // 0: strip, 1: echo bus, 2-3: chamber L/R

    chamber.setSampleRate (sampleRate);
    juce::Reverb::Parameters p;
    p.roomSize = 0.80f;      // a tiled room, not a hall
    p.damping = 0.55f;
    p.wetLevel = 0.33f;
    p.dryLevel = 0.0f;
    p.width = 0.75f;
    p.freezeMode = 0.0f;
    chamber.setParameters (p);
    preDelayLen = juce::jmax (1, (int) (sampleRate * 0.022));      // the walk from speaker to mic
    preDelay.assign ((size_t) preDelayLen, 0.0f);
    preDelayPos = 0;
    echoHpCoef = onePoleCoef (180.0, sampleRate);                   // chambers were fed without the bottom
    echoLpCoef = onePoleCoef (7000.0, sampleRate);

    lastDesk = -1;            // re-apply the desk on the first block
    masterCur = dbToGain (masterDb.load());
}

void MixEngine::deskChanged (Desk d)
{
    const auto& info = deskInfo (d);
    channelAmp.set (info.channelCeiling > 0.0f ? info.channelCeiling : 1.0f, info.channelAsym);
    mainAmp.set (info.mainCeiling > 0.0f ? info.mainCeiling : 1.0f, info.mainAsym);
    hpCoef = info.lowCutHz > 0.0f ? onePoleCoef (info.lowCutHz, sampleRate) : 1.0f;    // 1 = passes everything
    lpCoef = info.highCutHz > 0.0f && info.highCutHz < sampleRate * 0.45 ? onePoleCoef (info.highCutHz, sampleRate) : 0.0f;
    for (auto& s : strips) s.resetState();
    mainHp = {}; mainHpIn = {};
    chamber.reset();
    std::fill (preDelay.begin(), preDelay.end(), 0.0f);
    echoHp = echoHpIn = echoLp = 0.0f;
    echoRetCur = 0.0f;
}

void MixEngine::updateEq (Strip& s, float bassDb, float trebleDb, bool classic) noexcept
{
    if (bassDb != s.setBass)
    {
        s.setBass = bassDb;
        s.bass.setCoefficients (juce::IIRCoefficients::makeLowShelf (sampleRate, 100.0, 0.6, juce::Decibels::decibelsToGain (bassDb)));
    }
    if (trebleDb != s.setTreble || classic != s.setClassic)
    {
        s.setTreble = trebleDb;
        s.setClassic = classic;
        const double g = juce::Decibels::decibelsToGain (trebleDb);
        const double hf = juce::jmin (10000.0, sampleRate * 0.45);
        // Pop module: the lift is a bell around 5 kHz, the cut a shelf at 10 kHz. Classic: a 10 kHz shelf both ways.
        s.treble.setCoefficients (! classic && trebleDb > 0.0f
                                      ? juce::IIRCoefficients::makePeakFilter (sampleRate, 5000.0, 0.8, g)
                                      : juce::IIRCoefficients::makeHighShelf (sampleRate, hf, 0.6, g));
    }
}

bool MixEngine::anySolo() const noexcept
{
    for (auto& s : strips)
        if (s.solo.load (std::memory_order_relaxed)) return true;
    return false;
}

void MixEngine::process (const float* const* in, int numIn, float* const* out, int numOut, int n) noexcept
{
    for (int ch = 0; ch < numOut; ++ch)
        if (out[ch] != nullptr) juce::FloatVectorOperations::clear (out[ch], n);

    if (n > mix.getNumSamples())   // shouldn't happen; stay safe rather than write past the buffer
        return;

    const Desk d = getDesk();
    if ((int) d != lastDesk) { lastDesk = (int) d; deskChanged (d); }
    const auto& info = deskInfo (d);
    const bool valves = info.channelCeiling > 0.0f;

    float* mixL = mix.getWritePointer (0);
    float* mixR = mix.getWritePointer (1);
    float* tmp  = scratch.getWritePointer (0);
    float* echo = scratch.getWritePointer (1);
    juce::FloatVectorOperations::clear (mixL, n);
    juce::FloatVectorOperations::clear (mixR, n);
    juce::FloatVectorOperations::clear (echo, n);
    bool echoUsed = false;

    const bool soloing = anySolo();
    const float inv = 1.0f / (float) n;

    for (int i = 0; i < numIn && i < (int) routing.stripOfInput.size(); ++i)
    {
        const int si = routing.stripOfInput[(size_t) i];
        if (si < 0 || in[i] == nullptr) continue;
        auto& s = strips[(size_t) si];

        const bool audible = ! s.mute.load (std::memory_order_relaxed)
                             && (! soloing || s.solo.load (std::memory_order_relaxed));
        const float g = audible ? dbToGain (s.gainDb.load (std::memory_order_relaxed)) : 0.0f;
        float pl, pr;
        panGains (s.pan.load (std::memory_order_relaxed), pl, pr);
        const float tl = g * pl, tr = g * pr;
        const float echoDb = s.echoDb.load (std::memory_order_relaxed);
        const float te = info.hasEcho && echoDb > echoMinDb ? g * juce::Decibels::decibelsToGain (echoDb) : 0.0f;

        // --- the channel: amp, filters, EQ (modelled desks)
        const float* x = in[i];
        if (valves)
        {
            const float ti = juce::Decibels::decibelsToGain (s.inputDb.load (std::memory_order_relaxed));
            float gi = s.curIn;
            const float di = (ti - gi) * inv;
            float hp = s.hp, hpIn = s.hpIn, lp = s.lp;
            for (int k = 0; k < n; ++k)
            {
                gi += di;
                float v = channelAmp (x[k] * gi);
                // the transformers: no DC, a gentle low end below the audio band
                hp = hpCoef * (hp + v - hpIn); hpIn = v; v = hp;
                // and the valves' bandwidth
                lp = lp * lpCoef + v * (1.0f - lpCoef); v = lp;
                tmp[k] = v;
            }
            s.curIn = ti; s.hp = hp; s.hpIn = hpIn; s.lp = lp;

            if (info.hasEq)
            {
                const float b = s.bassDb.load (std::memory_order_relaxed), t = s.trebleDb.load (std::memory_order_relaxed);
                const bool cl = s.classic.load (std::memory_order_relaxed);
                updateEq (s, b, t, cl);
                if (b != 0.0f) s.bass.processSamples (tmp, n);
                if (t != 0.0f) s.treble.processSamples (tmp, n);
            }
            x = tmp;
        }

        // --- fader, pan, echo send
        float gl = s.curL, gr = s.curR, ge = s.curEcho;
        const float dl = (tl - gl) * inv, dr = (tr - gr) * inv, de = (te - ge) * inv;
        if (! (gl == 0.0f && gr == 0.0f && tl == 0.0f && tr == 0.0f))
        {
            for (int k = 0; k < n; ++k)
            {
                gl += dl; gr += dr;
                const float v = x[k];
                mixL[k] += v * gl;
                mixR[k] += v * gr;
            }
        }
        if (! (ge == 0.0f && te == 0.0f))
        {
            for (int k = 0; k < n; ++k) { ge += de; echo[k] += x[k] * ge; }
            echoUsed = true;
        }
        s.curL = tl; s.curR = tr; s.curEcho = te;

        // meter: the strip's level after its fader (and mute / solo), before pan
        float peak = 0.0f;
        const float gainNow = audible ? dbToGain (s.gainDb.load (std::memory_order_relaxed)) : 0.0f;
        if (gainNow > 0.0f)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (x, n);
            peak = juce::jmax (range.getEnd(), -range.getStart()) * gainNow;
        }
        raisePeak (s.peak, peak);
    }

    // --- echo chamber: the sends, thinned and darkened, a short walk across the room, then the room
    const float retDb = echoReturnDb.load (std::memory_order_relaxed);
    const float rt = info.hasEcho && retDb > echoMinDb ? juce::Decibels::decibelsToGain (retDb) : 0.0f;
    if (info.hasEcho && (echoUsed || echoRetCur > 0.0f || rt > 0.0f))
    {
        float* cl = scratch.getWritePointer (2);
        float* cr = scratch.getWritePointer (3);
        for (int k = 0; k < n; ++k)
        {
            float v = echo[k];
            echoHp = echoHpCoef * (echoHp + v - echoHpIn); echoHpIn = v; v = echoHp;
            echoLp = echoLp * echoLpCoef + v * (1.0f - echoLpCoef); v = echoLp;
            const float delayed = preDelay[(size_t) preDelayPos];
            preDelay[(size_t) preDelayPos] = v;
            if (++preDelayPos >= preDelayLen) preDelayPos = 0;
            cl[k] = cr[k] = delayed;
        }
        chamber.processStereo (cl, cr, n);
        float gret = echoRetCur;
        const float dret = (rt - gret) * inv;
        for (int k = 0; k < n; ++k)
        {
            gret += dret;
            mixL[k] += cl[k] * gret;
            mixR[k] += cr[k] * gret;
        }
        echoRetCur = rt;
    }

    // --- master fader, then the main amplifiers
    const float mt = dbToGain (masterDb.load (std::memory_order_relaxed));
    float m = masterCur;
    const float dm = (mt - m) * inv;
    float pL = 0.0f, pR = 0.0f;
    const bool mainValves = info.mainCeiling > 0.0f;
    for (int k = 0; k < n; ++k)
    {
        m += dm;
        float l = mixL[k] * m, r = mixR[k] * m;
        if (mainValves)
        {
            // input transformers, then the valves (last, so nothing can overshoot their ceiling)
            mainHp[0] = hpCoef * (mainHp[0] + l - mainHpIn[0]); mainHpIn[0] = l; l = mainHp[0];
            mainHp[1] = hpCoef * (mainHp[1] + r - mainHpIn[1]); mainHpIn[1] = r; r = mainHp[1];
            l = mainAmp (l);
            r = mainAmp (r);
        }
        mixL[k] = l;
        mixR[k] = r;
        pL = juce::jmax (pL, std::abs (l));
        pR = juce::jmax (pR, std::abs (r));
    }
    masterCur = mt;
    raisePeak (masterPeakL, pL);
    raisePeak (masterPeakR, pR);
    if (pL > 1.0f || pR > 1.0f) masterClipped.store (true, std::memory_order_relaxed);

    if (juce::isPositiveAndBelow (routing.outLeft, numOut) && out[routing.outLeft] != nullptr)
        juce::FloatVectorOperations::copy (out[routing.outLeft], mixL, n);
    if (juce::isPositiveAndBelow (routing.outRight, numOut) && out[routing.outRight] != nullptr)
        juce::FloatVectorOperations::copy (out[routing.outRight], mixR, n);
}

} // namespace whitman
