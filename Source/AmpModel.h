#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include "Desk.h"

namespace whitman
{

/** Levels: as in Hawthorne, 0 VU = +4 dBu = a sine peaking at -18 dBFS, so 0 dBFS = +22 dBu.
    A desk that clips at +26 dBu has 4 dB of headroom over digital full scale (floats carry it
    to Hawthorne; WHITMAN's CLIP lamp shows it). */
inline float dbuToPeak (float dbu) noexcept { return std::pow (10.0f, (dbu - 22.0f) / 20.0f); }

/** How one of a desk's amplifiers distorts, from the outside:
    - clipDbu: where it clips (its maximum output)
    - knee: the fraction of that it stays linear up to; above it, it rounds off into the clip
      (low for valves, which round off gradually; near 1 for op-amp and transistor stages)
    - h2Pct at refDbu: second harmonic from a lopsided (single-ended / class A) stage
    - lfH3Pct at refDbu and 50 Hz: third harmonic from the transformers' iron, which saturates
      with flux - so it rises as the frequency falls (lfHz sets where)
    - asym: how much sooner the positive half clips
    - lowCutHz / highCutHz: its bandwidth (0 = beyond the audio band)
    - h3Pct at refDbu: third harmonic at any frequency (the stage's own curvature, where its
      second harmonic cancels)
    clipDbu 0 = no amp at all (the clean desk). */
struct AmpSpec
{
    float clipDbu = 0.0f, knee = 0.9f;
    float refDbu = 20.0f, h2Pct = 0.0f, lfH3Pct = 0.0f, lfHz = 30.0f;
    float asym = 0.0f;
    float lowCutHz = 0.0f, highCutHz = 0.0f;
    float h3Pct = 0.0f;        // third harmonic at refDbu across the band (a balanced or push-pull stage, feedback)
};

/*  What each figure rests on. Only the Neve has published distortion figures; the rest are estimates
    from how the desks are described (valve or solid-state, transformers or not, headroom relative to
    each other) and are marked as such.
    - Neve 1073/1084 spec: response +/-0.5 dB 20 Hz-20 kHz (-3 dB at 40 kHz); THD no more than 0.07 % from
      50 Hz to 10 kHz at +20 dBu out; maximum output above +26 dBu. Modelled: clip +26.5; at +20 dBu the channel
      gives 2nd 0.025 % and transformer 3rd 0.04 % at 50 Hz, the mix amps a third of that, so a channel through
      the whole desk measures about 0.065 % at 50 Hz and 0.035 % at 1 kHz; flat to 20 Hz.
    - SSL: the 611G line amp (the nearest published SSL figures): clipping above +26.5 dBu, 20 Hz-20 kHz
      +0/-0.3 dB. Op-amps, no transformers in the channel: a hard knee, almost no distortion before it.
    - Neotek: 'very low distortion' (Electrical Audio), transformerless: as the SSL, estimated.
    - Trident A-Range: discrete class A with transformers: estimated between the Neve and the SSL.
    - EMI REDD.47 (REDD.51's channel, inter and line amps): EF86 pentode into a paralleled E88CC with
      negative feedback; its two stages' second harmonics cancel, leaving mostly the third (Phaedrus Audio,
      from EMI's description); 34 / 40 / 46 dB gain; -3 dB at 7 Hz and 40 kHz, 0.15 % THD at +20 dBu out
      and more than +23 dBu available (Sound On Sound's measurements of Chandler's faithful REDD.47).
      Modelled per stage: third 0.07 % and second 0.02 % at +20 dBu, rounding off from +22 into a +24.5 dBu
      clip; a channel through the desk (its amp, then the mix amps) measures about 0.15 %.
    - Siemens V72 / V72S (the REDD.37's 31 amps): two EF804 pentodes, 34 dB (V72S 40 dB), within 1 dB from
      50 Hz to 20 kHz (tube-workshop.de's measurements), lopsided (asymmetric) clipping when pushed,
      'laid back' beside the REDD.47 (SOS). No published THD or maximum output: those are estimates, lower
      headroom than the REDD.47 and second-harmonic led.
    - EMI TG12345: solid-state, transistor: estimated.  */
inline AmpSpec ampSpecOf (Desk d, bool main)
{
    AmpSpec a;
    switch (d)
    {
        case Desk::whitman: break;
        case Desk::redd37:
            a = main ? AmpSpec { 20.0f, 0.55f, 4.0f, 0.20f, 0.0f, 30.0f, 0.40f, 18.0f, 0.0f }
                     : AmpSpec { 16.0f, 0.50f, 4.0f, 0.40f, 0.0f, 30.0f, 0.40f, 18.0f, 0.0f };
            break;
        case Desk::redd51:
            a = AmpSpec { 24.5f, 0.80f, 20.0f, 0.02f, 0.0f, 30.0f, 0.05f, 4.0f, 40000.0f, 0.07f };   // each stage; chained, about 0.15 %
            break;
        case Desk::tg12345:
        case Desk::tg12345mk4:
            a = main ? AmpSpec { 24.0f, 0.85f, 20.0f, 0.05f, 0.03f, 30.0f, 0.02f, 8.0f, 0.0f }
                     : AmpSpec { 22.0f, 0.80f, 20.0f, 0.10f, 0.05f, 30.0f, 0.03f, 8.0f, 0.0f };
            break;
        case Desk::tridentA:
            a = AmpSpec { 24.0f, 0.82f, 20.0f, 0.08f, 0.06f, 30.0f, 0.05f, 6.0f, 0.0f };
            break;
        case Desk::neve8028:
            // the channel module carries most of it; the mix amps add a little, so a channel through the
            // whole desk stays inside the module's 0.07 %
            a = main ? AmpSpec { 26.5f, 0.88f, 20.0f, 0.010f, 0.015f, 30.0f, 0.02f, 3.0f, 0.0f }
                     : AmpSpec { 26.5f, 0.88f, 20.0f, 0.025f, 0.040f, 30.0f, 0.03f, 3.0f, 0.0f };
            break;
        case Desk::ssl4000e:
            a = AmpSpec { 26.5f, 0.96f, 20.0f, 0.003f, 0.0f, 30.0f, 0.0f, 3.0f, 0.0f };
            break;
        case Desk::neotek:
            a = AmpSpec { 26.0f, 0.96f, 20.0f, 0.002f, 0.0f, 30.0f, 0.0f, 3.0f, 0.0f };
            break;
    }
    return a;
}

/** One amplifier, per the spec above. State (the transformer's flux and the DC blocker) lives
    with the caller, so one Amp can serve every channel. */
struct Amp
{
    bool on = false;
    float posClip = 1.0f, negClip = 1.0f, kneePos = 1.0f, kneeNeg = 1.0f;
    float h2c = 0.0f, h3c = 0.0f, h3k = 0.0f, fluxCoef = 0.0f;

    void set (const AmpSpec& s, double sampleRate) noexcept
    {
        on = s.clipDbu > 0.0f;
        if (! on) return;
        const float clip = dbuToPeak (s.clipDbu);
        negClip = clip;
        posClip = clip * (1.0f - s.asym);
        kneePos = posClip * s.knee;
        kneeNeg = negClip * s.knee;
        const float ref = dbuToPeak (s.refDbu);
        // y += h2c * x^2  ->  2nd harmonic / fundamental = h2c * A / 2
        h2c = 2.0f * (s.h2Pct / 100.0f) / ref;
        // y -= h3c * x^3  ->  3rd / fundamental = h3c * A^2 / 4
        h3c = 4.0f * (s.h3Pct / 100.0f) / (ref * ref);
        // flux = a one-pole low-pass of the signal at lfHz (the iron's integral); y -= h3k * flux^3
        // ->  3rd / fundamental = h3k * (A g)^3 / 4 / A, with g the low-pass's gain at 50 Hz
        fluxCoef = std::exp (-juce::MathConstants<float>::twoPi * s.lfHz / (float) sampleRate);
        const float g50 = s.lfHz / std::sqrt (50.0f * 50.0f + s.lfHz * s.lfHz);
        h3k = s.lfH3Pct > 0.0f ? 4.0f * (s.lfH3Pct / 100.0f) / (ref * ref * g50 * g50 * g50) : 0.0f;
    }

    float process (float x, float& flux) const noexcept
    {
        if (! on) return x;
        flux = flux * fluxCoef + x * (1.0f - fluxCoef);
        float y = x + h2c * x * x - h3c * x * x * x - h3k * flux * flux * flux;
        // into the clip: linear to the knee, then rounding off towards the rail
        if (y > kneePos)       y = kneePos + (posClip - kneePos) * std::tanh ((y - kneePos) / (posClip - kneePos));
        else if (y < -kneeNeg) y = -kneeNeg - (negClip - kneeNeg) * std::tanh ((-y - kneeNeg) / (negClip - kneeNeg));
        return y;
    }
};

} // namespace whitman
