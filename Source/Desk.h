#pragma once

#include <juce_core/juce_core.h>

namespace whitman
{

/** The desks WHITMAN can be. Saved by number in mix files: only ever add to the end. */
enum class Desk { whitman = 0, redd51 = 1 };
constexpr int numDesks = 2;

struct DeskInfo
{
    const char* name;         // for the picker and the top bar
    const char* badge;        // engraved on the master section
    const char* maker;        // maker, country, era
    const char* history;
    const char* sound;

    bool hasGain, hasEq, hasEcho;          // what the channel strips have
    bool stepped;                          // switched (stepped) controls
    // valve stages: 0 = none (clean)
    float channelCeiling;                  // where the channel amp tops out (full scale = 1)
    float channelAsym;                     // lopsidedness: even harmonics
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
          false, false, false, false,
          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },

        { "EMI REDD.51", "REDD.51",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road Studio Two, 1964 to 1968",
          "EMI built its own desks. The valve REDD.51 replaced the REDD.37 in Studio Two in 1964 and fed the Studer J37 "
          "4-tracks through Revolver and Sgt. Pepper until the solid-state TG12345 took over in November 1968. Eight mic "
          "channels with REDD.47 valve amplifiers, bass and treble EQ (the treble a 'Pop' or 'Classic' module), pan pots, "
          "echo sends to the chambers, and Painton quadrant faders.",
          "All valve: thick and warm with a soft, rounded top. Push the GAIN and the channel amps thicken and grit up "
          "sweetly; the main amps glue the mix. Pop treble lifts around 5 kHz; Classic opens the top at 10 kHz.",
          true, true, true, true,
          0.90f, 0.35f, 0.98f, 0.15f, 8.0f, 20000.0f },
    };
    return desks[juce::jlimit (0, numDesks - 1, (int) d)];
}

inline Desk deskFromInt (int i) { return (Desk) juce::jlimit (0, numDesks - 1, i); }

} // namespace whitman
