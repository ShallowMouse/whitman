#pragma once

#include <juce_core/juce_core.h>

namespace whitman
{

/** The desks WHITMAN can be. Saved by number in mix files: only ever add to the end. */
enum class Desk { whitman = 0, redd51 = 1, redd37 = 2, tg12345 = 3 };
constexpr int numDesks = 4;

/** The channel EQ's layout. */
enum class EqStyle { none, redd, tg };

struct DeskInfo
{
    const char* name;         // for the picker and the top bar
    const char* badge;        // engraved on the master section
    const char* maker;        // maker, country, era
    const char* history;
    const char* sound;

    bool hasGain, hasEcho;                 // what the channel strips have
    EqStyle eq;                            // REDD: bass 100 Hz + treble Pop / Classic. TG: bass 50 Hz, presence, treble 10 kHz
    bool hasComp;                          // a compressor / limiter on every channel (TG)
    bool hasSpread;                        // stereo width on the main outputs (TG)
    // the amplifiers: 0 = none (clean)
    float channelCeiling;                  // where the channel amp tops out (full scale = 1)
    float channelAsym;                     // lopsidedness: even harmonics (valves); 0 = symmetrical (transistors)
    float mainCeiling, mainAsym;           // the main (line) amplifiers on the mix
    float lowCutHz, highCutHz;             // the amps' and transformers' bandwidth
};

inline const DeskInfo& deskInfo (Desk d)
{
    static const DeskInfo desks[numDesks] = {
        { "WHITMAN (clean)", "WHITMAN",
          "PEQUOD  -  digital",
          "A plain, transparent mixer: a fader, pan, mute and solo per track and a master fader. Nothing added.",
          "Clean: exactly what comes off the tape, mixed.",
          false, false, EqStyle::none, false, false,
          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },

        { "EMI REDD.51", "REDD.51",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road Studio Two, 1964 to 1968",
          "EMI built its own desks. The valve REDD.51 replaced the REDD.37 in Studio Two in 1964 and fed the Studer J37 "
          "4-tracks through Revolver and Sgt. Pepper until the solid-state TG12345 took over in November 1968. Eight mic "
          "channels with REDD.47 valve amplifiers, bass and treble EQ (the treble a 'Pop' or 'Classic' module), pan pots, "
          "echo sends to the chambers, and Painton quadrant faders.",
          "All valve: thick and warm with a soft, rounded top. Push the GAIN and the channel amps thicken and grit up "
          "sweetly; the main amps glue the mix. Pop treble lifts around 5 kHz; Classic opens the top at 10 kHz.",
          true, true, EqStyle::redd, false, false,
          0.90f, 0.35f, 0.98f, 0.15f, 8.0f, 20000.0f },

        { "EMI REDD.37", "REDD.37",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road Studio Two, 1958 to 1964",
          "The REDD.51's older brother and the first of EMI's own valve desks with eight mic channels - nicknamed 'the "
          "Beatles' console', it's the desk of their early records, from Love Me Do. Its channel amplifiers were Siemens "
          "V72s, with less headroom than the REDD.51's later amps. The same bass and Pop / Classic treble EQ, echo sends "
          "and Painton quadrant faders.",
          "Valve, and rawer than the REDD.51: the V72s run out of headroom sooner, so the GAIN gets gritty and "
          "compressed quicker, with a darker, more rounded top. The early-60s Abbey Road sound.",
          true, true, EqStyle::redd, false, false,
          0.70f, 0.45f, 0.95f, 0.20f, 12.0f, 12000.0f },

        { "EMI TG12345", "TG12345",
          "EMI  -  England  -  Abbey Road Studio Two from November 1968",
          "EMI's first solid-state desk, built for 8-track: 24 mic inputs, VU meters everywhere, and a compressor / "
          "limiter on every channel. It replaced the REDD.51 in Studio Two and made Abbey Road (the only Beatles album "
          "recorded on one), All Things Must Pass and, as the Mk IV, The Dark Side of the Moon. Bass at 50 Hz, a presence "
          "band with eight frequencies (first on the groups and mains, later added to every channel), treble at 10 kHz, "
          "and SPREAD to widen or narrow the stereo mix.",
          "Solid-state: smoother and brighter than the REDD valves, tighter in the low end. Pushed, the transistors get "
          "edgier rather than fatter. The channel compressors squash hard - COMP is 2:1, LIMIT 8:1 - for that pumping, "
          "glued late-60s sound on drums, piano and guitars.",
          true, true, EqStyle::tg, true, true,
          0.95f, 0.06f, 0.99f, 0.0f, 10.0f, 22000.0f },
    };
    return desks[juce::jlimit (0, numDesks - 1, (int) d)];
}

inline Desk deskFromInt (int i) { return (Desk) juce::jlimit (0, numDesks - 1, i); }

// TG presence frequencies and compressor recovery times (switch positions)
constexpr int numPresenceFreqs = 8;
inline float presenceHz (int i)
{
    static const float f[numPresenceFreqs] = { 500.0f, 800.0f, 1200.0f, 1800.0f, 2800.0f, 4200.0f, 6800.0f, 10000.0f };
    return f[juce::jlimit (0, numPresenceFreqs - 1, i)];
}
constexpr int numRecoveries = 6;
inline float recoverySeconds (int i)
{
    static const float s[numRecoveries] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f };
    return s[juce::jlimit (0, numRecoveries - 1, i)];
}
/** Compressor modes. */
enum class CompMode { off = 0, comp = 1, limit = 2 };

} // namespace whitman
