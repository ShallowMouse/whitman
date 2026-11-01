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
    presence.reset();
    setBass = setTreble = setPresence = 1000.0f;   // force new coefficients
    setPresFreq = -1;
    env = 0.0f;
    gainReduction = 0.0f;
    gateEnv = gateLevel = 0.0f;
    gateReduction = 0.0f;
    flux = 0.0f;
    bandEq.reset();
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
    deskNow = d;
    const auto ch = ampSpecOf (d, false), mn = ampSpecOf (d, true);
    channelAmp.set (ch, sampleRate);
    mainAmp.set (mn, sampleRate);
    hpCoef = ch.lowCutHz > 0.0f ? onePoleCoef (ch.lowCutHz, sampleRate) : 1.0f;    // 1 = passes everything
    lpCoef = ch.highCutHz > 0.0f && ch.highCutHz < sampleRate * 0.45 ? onePoleCoef (ch.highCutHz, sampleRate) : 0.0f;
    mainHpCoef = mn.lowCutHz > 0.0f ? onePoleCoef (mn.lowCutHz, sampleRate) : 1.0f;
    for (auto& s : strips) { s.resetState(); s.bandEq.prepare (sampleRate); }
    mainHp = {}; mainHpIn = {}; mainFlux = {};
    chamber.reset();
    std::fill (preDelay.begin(), preDelay.end(), 0.0f);
    echoHp = echoHpIn = echoLp = 0.0f;
    echoRetCur = 0.0f;
    spreadCur = info.hasSpread ? spread.load() : 1.0f;
    mainPresence[0].reset(); mainPresence[1].reset();
    setMainPres = 1000.0f; setMainPresFreq = -1;
}

void MixEngine::updateTgEq (Strip& s, float bassDb, float trebleDb, float presDb, int presFreq) noexcept
{
    if (bassDb != s.setBass)
    {
        s.setBass = bassDb;
        s.bass.setCoefficients (juce::IIRCoefficients::makeLowShelf (sampleRate, 50.0, 0.6, juce::Decibels::decibelsToGain (bassDb)));
    }
    if (trebleDb != s.setTreble)
    {
        s.setTreble = trebleDb;
        s.treble.setCoefficients (trebleCoefs (trebleDb, false));    // Mk I: lift a bell at 5 kHz, cut a 10 kHz shelf
    }
    if (presDb != s.setPresence || presFreq != s.setPresFreq)
    {
        s.setPresence = presDb;
        s.setPresFreq = presFreq;
        s.presence.setCoefficients (presenceCoefs (presDb, presFreq));
    }
}

juce::IIRCoefficients MixEngine::trebleCoefs (float db, bool classic) const noexcept
{
    const double g = juce::Decibels::decibelsToGain (db);
    const double hf = juce::jmin (10000.0, sampleRate * 0.45);
    return ! classic && db > 0.0f ? juce::IIRCoefficients::makePeakFilter (sampleRate, 5000.0, 0.8, g)
                                  : juce::IIRCoefficients::makeHighShelf (sampleRate, hf, 0.6, g);
}

juce::IIRCoefficients MixEngine::presenceCoefs (float db, int freq) const noexcept
{
    const double g = juce::Decibels::decibelsToGain (db);
    const double hz = juce::jmin ((double) presenceHz (freq), sampleRate * 0.42);
    // positions 1-7 are bells; the last is a 10 kHz shelf
    return presenceIsShelf (freq) ? juce::IIRCoefficients::makeHighShelf (sampleRate, hz, 0.6, g)
                                  : juce::IIRCoefficients::makePeakFilter (sampleRate, hz, 1.0, g);
}

void MixEngine::compress (Strip& s, float* x, int n) noexcept
{
    // TG channel compressor: feed-forward, a fast fixed attack, switched recovery.
    // COMP 2:1, LIMIT 8:1. COMPRESS sets how hard it's driven (the threshold comes down
    // from -6 dBFS), with half the squeeze made up so the level stays roughly where it was.
    const auto mode = (CompMode) s.compMode.load (std::memory_order_relaxed);
    if (mode == CompMode::off) { s.env = 0.0f; s.gainReduction.store (0.0f, std::memory_order_relaxed); return; }
    const float ratio = mode == CompMode::limit ? 8.0f : 2.0f;
    const float amount = juce::jlimit (0.0f, 24.0f, s.compDb.load (std::memory_order_relaxed));
    const float threshold = -6.0f - amount;
    const float slope = 1.0f - 1.0f / ratio;
    const float makeUp = amount * slope * 0.5f;
    const float att = std::exp (-1.0f / (0.0015f * (float) sampleRate));
    const float rel = std::exp (-1.0f / (recoverySeconds (s.recovery.load (std::memory_order_relaxed)) * 0.4f * (float) sampleRate));
    float env = s.env, most = 0.0f;
    for (int k = 0; k < n; ++k)
    {
        const float a = std::abs (x[k]);
        const float over = a > 1.0e-6f ? juce::jmax (0.0f, juce::Decibels::gainToDecibels (a) - threshold) : 0.0f;
        const float want = over * slope;                                 // dB of reduction wanted now
        env = want > env ? want + att * (env - want) : want + rel * (env - want);
        x[k] *= juce::Decibels::decibelsToGain (makeUp - env);
        most = juce::jmax (most, env);
    }
    s.env = env;
    s.gainReduction.store (most, std::memory_order_relaxed);
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
        // Pop module: the lift is a bell around 5 kHz, the cut a shelf at 10 kHz. Classic: a 10 kHz shelf both ways.
        s.treble.setCoefficients (trebleCoefs (trebleDb, classic));
    }
}

void MixEngine::sslDynamics (Strip& s, float* x, int n) noexcept
{
    // SSL 4000 E channel dynamics: the expander / gate, then the compressor / limiter.
    // Levels in dBu (0 VU = +4 dBu = -18 dBFS peak). Attack 1 ms with FAST, else program-dependent (~10 ms here).
    const float sr = (float) sampleRate;
    auto coef = [sr] (float seconds) { return std::exp (-1.0f / (juce::jmax (1.0e-4f, seconds) * sr)); };
    auto dbuOf = [] (float a) { return a > 1.0e-7f ? juce::Decibels::gainToDecibels (a) + 22.0f : -200.0f; };

    // --- expander / gate
    const float range = juce::jlimit (0.0f, 40.0f, s.gateRange.load (std::memory_order_relaxed));
    float mostGate = 0.0f;
    if (range > 0.05f)
    {
        const float thr = s.gateThresh.load (std::memory_order_relaxed);
        const bool gate = s.gateMode.load (std::memory_order_relaxed);
        const float att = coef (s.gateFast.load (std::memory_order_relaxed) ? 0.001f : 0.004f);
        const float rel = coef (s.gateRelease.load (std::memory_order_relaxed));
        const float lvlRel = coef (0.005f);
        float lvl = s.gateLevel, g = s.gateEnv;
        for (int k = 0; k < n; ++k)
        {
            const float a = std::abs (x[k]);
            lvl = a > lvl ? a : a + lvlRel * (lvl - a);              // peak follower
            const float under = thr - dbuOf (lvl);
            // gate: all the range below the threshold; expander: 1:2 below it, up to the range
            const float want = under <= 0.0f ? 0.0f : (gate ? range : juce::jmin (range, under));
            g = want < g ? want + att * (g - want) : want + rel * (g - want);   // opening is the attack
            x[k] *= juce::Decibels::decibelsToGain (-g);
            mostGate = juce::jmax (mostGate, g);
        }
        s.gateLevel = lvl; s.gateEnv = g;
    }
    else { s.gateEnv = 0.0f; }
    s.gateReduction.store (mostGate, std::memory_order_relaxed);

    // --- compressor / limiter
    const float ratioKnob = juce::jlimit (1.0f, 20.0f, s.sslRatio.load (std::memory_order_relaxed));
    float most = 0.0f;
    if (ratioKnob > 1.01f)
    {
        const float slope = ratioKnob >= 19.99f ? 1.0f : 1.0f - 1.0f / ratioKnob;   // infinity at the end
        const float thr = s.sslThresh.load (std::memory_order_relaxed);
        // automatic make-up (SSL's comes from ratio and threshold): half the reduction a 0 VU tone above the threshold would get
        const float makeUp = juce::jmax (0.0f, 4.0f - thr) * slope * 0.5f;
        const float att = coef (s.sslFast.load (std::memory_order_relaxed) ? 0.001f : 0.010f);
        const float rel = coef (s.sslRelease.load (std::memory_order_relaxed) * 0.4f);
        float env = s.env;
        for (int k = 0; k < n; ++k)
        {
            const float over = juce::jmax (0.0f, dbuOf (std::abs (x[k])) - 3.0f - thr);     // -3: peak to rms of a sine
            const float want = over * slope;
            env = want > env ? want + att * (env - want) : want + rel * (env - want);
            x[k] *= juce::Decibels::decibelsToGain (makeUp - env);
            most = juce::jmax (most, env);
        }
        s.env = env;
    }
    else s.env = 0.0f;
    s.gainReduction.store (most, std::memory_order_relaxed);
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
    const bool valves = channelAmp.on;     // a modelled desk (the clean one has no amps)

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
            float hp = s.hp, hpIn = s.hpIn, lp = s.lp, flux = s.flux;
            for (int k = 0; k < n; ++k)
            {
                gi += di;
                float v = channelAmp.process (x[k] * gi, flux);
                // the transformers: no DC, a gentle low end below the audio band
                hp = hpCoef * (hp + v - hpIn); hpIn = v; v = hp;
                // and the valves' bandwidth
                lp = lp * lpCoef + v * (1.0f - lpCoef); v = lp;
                tmp[k] = v;
            }
            s.curIn = ti; s.hp = hp; s.hpIn = hpIn; s.lp = lp; s.flux = flux;

            const float b = s.bassDb.load (std::memory_order_relaxed), t = s.trebleDb.load (std::memory_order_relaxed);
            if (info.eq == EqStyle::redd)
            {
                const bool cl = s.classic.load (std::memory_order_relaxed);
                updateEq (s, b, t, cl);
                if (b != 0.0f) s.bass.processSamples (tmp, n);
                if (t != 0.0f) s.treble.processSamples (tmp, n);
            }
            else if (info.eq == EqStyle::tgMk1 || info.eq == EqStyle::tgMk4)
            {
                // the Mk I has treble on its channels, the Mk II on presence instead
                const bool mk4 = info.eq == EqStyle::tgMk4;
                const float pr = mk4 ? s.presenceDb.load (std::memory_order_relaxed) : 0.0f;
                const float tr = mk4 ? 0.0f : t;
                updateTgEq (s, b, tr, pr, s.presenceFreq.load (std::memory_order_relaxed));
                if (b != 0.0f)  s.bass.processSamples (tmp, n);
                if (pr != 0.0f) s.presence.processSamples (tmp, n);
                if (tr != 0.0f) s.treble.processSamples (tmp, n);
            }
            else if (info.eq == EqStyle::bands)
            {
                if (const auto* def = bandEqOf (d))
                    s.bandEq.process (*def, *s.eqFor (d), tmp, n);
            }
            if (info.hasComp)
                compress (s, tmp, n);
            if (info.sslDynamics)
                sslDynamics (s, tmp, n);
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

    // --- presence on the main outputs (the TG's main cassettes)
    if (info.hasMainPresence)
    {
        const float pd = mainPresenceDb.load (std::memory_order_relaxed);
        const int pf = mainPresenceFreq.load (std::memory_order_relaxed);
        if (pd != setMainPres || pf != setMainPresFreq)
        {
            setMainPres = pd; setMainPresFreq = pf;
            const auto c = presenceCoefs (pd, pf);
            mainPresence[0].setCoefficients (c);
            mainPresence[1].setCoefficients (c);
        }
        if (pd != 0.0f)
        {
            mainPresence[0].processSamples (mixL, n);
            mainPresence[1].processSamples (mixR, n);
        }
    }

    // --- SPREAD (REDD.51's Spreader, TG): the mix's width, by turning its side (L - R) up or down
    if (info.hasSpread)
    {
        const float st = juce::jlimit (0.0f, 2.0f, spread.load (std::memory_order_relaxed));
        float sp = spreadCur;
        const float ds = (st - sp) * inv;
        if (! (sp == 1.0f && st == 1.0f))
            for (int k = 0; k < n; ++k)
            {
                sp += ds;
                const float mid = 0.5f * (mixL[k] + mixR[k]), side = 0.5f * (mixL[k] - mixR[k]) * sp;
                mixL[k] = mid + side;
                mixR[k] = mid - side;
            }
        spreadCur = st;
    }

    // --- master fader, then the main amplifiers
    const float mt = dbToGain (masterDb.load (std::memory_order_relaxed));
    float m = masterCur;
    const float dm = (mt - m) * inv;
    float pL = 0.0f, pR = 0.0f;
    const bool mainValves = mainAmp.on;
    for (int k = 0; k < n; ++k)
    {
        m += dm;
        float l = mixL[k] * m, r = mixR[k] * m;
        if (mainValves)
        {
            // input transformers, then the amps (last, so nothing overshoots their clip level)
            mainHp[0] = mainHpCoef * (mainHp[0] + l - mainHpIn[0]); mainHpIn[0] = l; l = mainHp[0];
            mainHp[1] = mainHpCoef * (mainHp[1] + r - mainHpIn[1]); mainHpIn[1] = r; r = mainHp[1];
            l = mainAmp.process (l, mainFlux[0]);
            r = mainAmp.process (r, mainFlux[1]);
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
