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
MixEngine::MixEngine() = default;

void MixEngine::prepare (double, int maxBlockSize)
{
    mix.setSize (2, juce::jmax (1, maxBlockSize), false, false, true);
    for (auto& s : strips) s.curL = s.curR = 0.0f;
    masterCur = dbToGain (masterDb.load());
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

    float* mixL = mix.getWritePointer (0);
    float* mixR = mix.getWritePointer (1);
    juce::FloatVectorOperations::clear (mixL, n);
    juce::FloatVectorOperations::clear (mixR, n);

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

        const float* x = in[i];
        float gl = s.curL, gr = s.curR;
        const float dl = (tl - gl) * inv, dr = (tr - gr) * inv;
        float peak = 0.0f;
        const bool silent = gl == 0.0f && gr == 0.0f && tl == 0.0f && tr == 0.0f;

        if (! silent)
        {
            for (int k = 0; k < n; ++k)
            {
                gl += dl; gr += dr;
                const float v = x[k];
                mixL[k] += v * gl;
                mixR[k] += v * gr;
            }
        }

        // meter: the strip's level after its fader (and mute / solo), before pan
        const float gainNow = audible ? dbToGain (s.gainDb.load (std::memory_order_relaxed)) : 0.0f;
        if (gainNow > 0.0f)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (x, n);
            peak = juce::jmax (range.getEnd(), -range.getStart()) * gainNow;
        }
        raisePeak (s.peak, peak);

        s.curL = tl; s.curR = tr;
    }

    // master fader
    const float mt = dbToGain (masterDb.load (std::memory_order_relaxed));
    float m = masterCur;
    const float dm = (mt - m) * inv;
    float pL = 0.0f, pR = 0.0f;
    for (int k = 0; k < n; ++k)
    {
        m += dm;
        mixL[k] *= m;
        mixR[k] *= m;
        pL = juce::jmax (pL, std::abs (mixL[k]));
        pR = juce::jmax (pR, std::abs (mixR[k]));
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
