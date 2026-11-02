#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <complex>
#include <cmath>

namespace whitman
{

/**
    The REDD.37 / REDD.51 channel tone controls, as their circuit: passive, constant-impedance
    (200 ohm) bridged-T networks, switched in 2 dB steps to +/-10 dB, with 10 dB of insertion loss
    made up by the next amplifier.

    A constant-resistance bridged-T with bridging arm Z1 (and its dual, R0^2 / Z1, as the shunt arm)
    passes  H = R0 / (R0 + Z1). Each control's switch sets the resistor R1 in Z1 for its step
    (R1 = R0 (G - 1) for G = the step's gain); the reactive parts stay fixed, so - as in the real
    thing - the curve's turnover moves a little with the step.

        bass  boost: Z1 = R1 || L     (+ the 10 dB made up)      bass  cut: Z1 = R1 || C
        treble boost: Z1 = R1 || C    (+ the 10 dB made up)      treble cut: Z1 = R1 || L
        Pop treble boost: Z1 = R1 || (L + C in series)  - a broad bell

    Sources and the values chosen (see README, "REDD accuracy"):
    - Steps, range, loss, topology: Kehew & Ryan, Recording the Beatles, p. 89 (+/-10 dB in 2 dB steps;
      Classic 100 Hz and 10 kHz shelves; Pop treble boost a peak); EMI's own description, quoted by
      Phaedrus Audio ("constant-impedance", "10 dB insertion loss", Pop "top boost broadly tuned to
      4,700 c/s", "the bass control also affects higher bass frequencies", "the top control works at
      a lower frequency"); Abbey Road Studios (REDD.37: 100 Hz low shelf, 10 kHz high cut shelf, Pop
      boost a bell); Ian Bruce (groupdiy, from the surviving REDD.51's measured curves): Pop peaks at
      about 4.7 kHz.
    - Component values: Pop from K. Hicks's MTSU thesis, which designed these networks from the
      published curves (bass boost L = 77.4 mH -> half its boost at 500 Hz; treble boost
      L 3.36 mH / C 0.474 uF, i.e. sqrt(L/C) = 84 ohm, half its boost 1.3 kHz below the peak; treble
      cut half at 2.3 kHz). The bell is retuned from his 4 kHz to EMI's 4.7 kHz keeping its width.
      Classic: half the full boost / cut at 100 Hz and 10 kHz. Pop bass cut: as its boost, 500 Hz.
*/
struct ReddEq
{
    static constexpr double r0 = 200.0;

    /** An s-domain biquad: (b2 s^2 + b1 s + b0) / (a2 s^2 + a1 s + a0). */
    struct Analog { double b2 = 0, b1 = 0, b0 = 1, a2 = 0, a1 = 0, a0 = 1; };

    /** The step nearest a setting: the controls are switches. */
    static float snap (float db) noexcept { return 2.0f * std::round (juce::jlimit (-10.0f, 10.0f, db) / 2.0f); }

    /** Size a first-order network's reactance so that, at full (10 dB) boost or cut, half of it falls at hz. */
    static double fullR1() { return r0 * (std::pow (10.0, 0.5) - 1.0); }
    static double lFor (double hz) { return fullR1() / (juce::MathConstants<double>::twoPi * hz * std::pow (10.0, 0.25)); }   // R1 || L
    static double cFor (double hz) { return std::pow (10.0, 0.25) / (fullR1() * juce::MathConstants<double>::twoPi * hz); }   // R1 || C

    static Analog bass (float db, bool classic)
    {
        Analog h;
        db = snap (db);
        if (db == 0.0f) return h;
        const double g = std::pow (10.0, std::abs (db) / 20.0), r1 = r0 * (g - 1.0);
        if (db > 0.0f)
        {
            // Z1 = R1 || sL; H = G R0 (R1 + sL) / (R0 R1 + sL (R0 + R1))
            const double l = classic ? lFor (100.0) : 77.4e-3;
            h = { 0, g * r0 * l, g * r0 * r1, 0, l * (r0 + r1), r0 * r1 };
        }
        else
        {
            // Z1 = R1 || 1/sC; H = R0 (1 + s R1 C) / (R0 + R1 + s R0 R1 C)
            const double c = cFor (classic ? 100.0 : 500.0);
            h = { 0, r0 * r1 * c, r0, 0, r0 * r1 * c, r0 + r1 };
        }
        return h;
    }

    static Analog treble (float db, bool classic)
    {
        Analog h;
        db = snap (db);
        if (db == 0.0f) return h;
        const double g = std::pow (10.0, std::abs (db) / 20.0), r1 = r0 * (g - 1.0);
        if (db > 0.0f && classic)
        {
            // Z1 = R1 || 1/sC, made up by G
            const double c = cFor (10000.0);
            h = { 0, g * r0 * r1 * c, g * r0, 0, r0 * r1 * c, r0 + r1 };
        }
        else if (db > 0.0f)
        {
            // Z1 = R1 || (sL + 1/sC):  H = (s^2 LC + s R1 C + 1) / (s^2 LC + s C R0 R1 / (R0 + R1) + 1)
            const double w0 = juce::MathConstants<double>::twoPi * 4700.0, z = std::sqrt (3.3638e-3 / 0.4741e-6);
            const double l = z / w0, c = 1.0 / (z * w0);
            h = { l * c, r1 * c, 1.0, l * c, c * r0 * r1 / (r0 + r1), 1.0 };
        }
        else
        {
            // Z1 = R1 || sL: H = R0 (R1 + sL) / (R0 R1 + sL (R0 + R1))
            const double l = classic ? lFor (10000.0) : lFor (2300.0);
            h = { 0, r0 * l, r0 * r1, 0, l * (r0 + r1), r0 * r1 };
        }
        return h;
    }

    /** The exact response, in dB. */
    static double responseDb (const Analog& h, double hz)
    {
        const std::complex<double> s (0.0, juce::MathConstants<double>::twoPi * hz);
        const auto num = h.b2 * s * s + h.b1 * s + h.b0, den = h.a2 * s * s + h.a1 * s + h.a0;
        return 20.0 * std::log10 (std::abs (num / den));
    }

    /** To a digital biquad by the bilinear transform, prewarped at hz (where the curve does its work). */
    static juce::IIRCoefficients digital (const Analog& h, double sampleRate, double hz)
    {
        hz = juce::jmin (hz, sampleRate * 0.4);
        const double w = juce::MathConstants<double>::twoPi * hz;
        const double k = w / std::tan (w / (2.0 * sampleRate)), k2 = k * k;
        return juce::IIRCoefficients (h.b2 * k2 + h.b1 * k + h.b0, 2.0 * (h.b0 - h.b2 * k2), h.b2 * k2 - h.b1 * k + h.b0,
                                      h.a2 * k2 + h.a1 * k + h.a0, 2.0 * (h.a0 - h.a2 * k2), h.a2 * k2 - h.a1 * k + h.a0);
    }

    static double bassWarpHz (bool classic)   { return classic ? 100.0 : 400.0; }
    static double trebleWarpHz (bool classic) { return classic ? 10000.0 : 4700.0; }
};

} // namespace whitman
