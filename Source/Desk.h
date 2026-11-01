#pragma once

#include <juce_core/juce_core.h>

namespace whitman
{

/** The desks WHITMAN can be. Saved by number in mix files: only ever add to the end. */
enum class Desk { whitman = 0, redd51 = 1, redd37 = 2, tg12345 = 3, tg12345mk4 = 4,    // tg12345 = the Mk I
                  tridentA = 5, neve8028 = 6, ssl4000e = 7, neotek = 8 };
constexpr int numDesks = 9;

/** The channel EQ's layout.
    redd:  bass 100 Hz shelf; treble Pop (lift a bell at 5 kHz, cut a 10 kHz shelf) or Classic (10 kHz shelf)
    tgMk1: bass 50 Hz shelf; treble (lift a bell at 5 kHz, cut a 10 kHz shelf)
    tgMk4: bass 50 Hz shelf; presence at 8 positions (7 bells, then a 10 kHz shelf) - no treble */
enum class EqStyle { none, redd, tgMk1, tgMk4, bands };     // bands: BandEq.h (Trident, Neve, SSL, Neotek)

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
    bool hasSpread;                        // stereo width on the main outputs (REDD.51's Spreader, TG's SPREAD)
    bool hasMainPresence;                  // presence on the main outputs (TG: the main cassettes had it)
    bool channelVus;                       // a VU meter on every channel (TG)
    // the amplifiers: 0 = none (clean)
    float channelCeiling;                  // where the channel amp tops out (full scale = 1)
    float channelAsym;                     // lopsidedness: even harmonics (valves); 0 = symmetrical (transistors)
    float mainCeiling, mainAsym;           // the main (line) amplifiers on the mix
    float lowCutHz, highCutHz;             // the amps' and transformers' bandwidth
    bool sslDynamics = false;              // the SSL channel compressor and expander / gate
    const char* sendName = "ECHO";         // the send to the chamber
};

/*  Sources, and the calls made where they disagree:
    - REDD.37 / .51: MusicTech "Studio Icons: EMI REDD.51"; Wikipedia "EMI REDD"; Puremix "The REDD console";
      the Waves / Abbey Road REDD plug-in pages. EQ: bass 100 Hz shelf, Pop (5 kHz bell up, 10 kHz shelf down) or
      Classic (10 kHz shelf), +/-10 dB, continuous. REDD.51: Spreader, VU meters, Painton quadrant faders.
      No source gives the REDD.37 a spreader, so it has none here.
    - TG12345: MusicTech "Vintage: EMI TG12345"; Abbey Road "#GearThatMadeUs"; the Waves TG12345 guide; Tape Op's
      review; Chandler's TG12345 MkIV EQ (Bonedo's test). Bass 50 Hz shelf (Chandler's MkIV EQ module uses 90/150 Hz,
      but it blends input and group EQs; the console sources say 50 Hz). Mk I channel treble: Waves (measured with
      Abbey Road) has the lift a 5 kHz bell and the cut a 10 kHz shelf; MusicTech says a 10 kHz peak - Waves used.
      Presence (Mk II on): 500, 800, 1.2k, 1.8k, 2.8k, 4.2k, 6.8k (Chandler: 6.5k) bells, then a 10 kHz shelf.
      Limiter: MusicTech 8:1, Waves 7:1 - 8:1 used. Recovery: six positions, 0.1 to 5 s (the in-between values are
      spread evenly here). Faders: no source found; straight faders drawn.
    - GAIN (all EMI desks) isn't a control on the real desks: it stands in for how hard the amps were driven.  */
inline const DeskInfo& deskInfo (Desk d)
{
    static const DeskInfo desks[numDesks] = {
        { "WHITMAN (clean)", "WHITMAN",
          "PEQUOD  -  digital",
          "A plain, transparent mixer: a fader, pan, mute and solo per track and a master fader. Nothing added.",
          "Clean: exactly what comes off the tape, mixed.",
          false, false, EqStyle::none, false, false, false, false,
          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },

        { "EMI REDD.51", "REDD.51",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road Studio Two, 1964 to 1968",
          "EMI built its own desks. The valve REDD.51 replaced the REDD.37 in Studio Two in 1964 and fed the Studer J37 "
          "4-tracks through Revolver and Sgt. Pepper until the solid-state TG12345 took over in November 1968. Eight mic "
          "channels with REDD.47 valve amplifiers (more headroom and less distortion than the REDD.37's V72s), bass and "
          "treble EQ (the treble a 'Pop' or 'Classic' module), pan pots, a Spreader for the stereo width, echo sends to "
          "the chambers, Painton quadrant faders and VU meters.",
          "All valve: thick and warm with a soft, rounded top. Push the GAIN and the channel amps thicken and grit up "
          "sweetly; the main amps glue the mix. Pop treble lifts around 5 kHz; Classic opens the top at 10 kHz.",
          true, true, EqStyle::redd, false, true, false, false,
          0.90f, 0.35f, 0.98f, 0.15f, 8.0f, 20000.0f },

        { "EMI REDD.37", "REDD.37",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road Studio Two, 1958 to 1964",
          "The REDD.51's older brother and the first of EMI's own valve desks with eight mic channels - nicknamed 'the "
          "Beatles' console', it's the desk of their early records, from Love Me Do. Its channel amplifiers were Siemens "
          "V72s, with less headroom than the REDD.51's later amps. The same bass and Pop / Classic treble EQ, echo sends "
          "and Painton quadrant faders.",
          "Valve, and rawer than the REDD.51: the V72s run out of headroom sooner, so the GAIN gets gritty and "
          "compressed quicker, with a darker, more rounded top. The early-60s Abbey Road sound.",
          true, true, EqStyle::redd, false, false, false, false,
          0.70f, 0.45f, 0.95f, 0.20f, 12.0f, 12000.0f },

        { "EMI TG12345 Mk I", "TG12345",
          "EMI  -  England  -  Abbey Road Studio Two, from November 1968",
          "EMI's first solid-state desk, built for 8-track: 24 mic inputs in two-channel 'cassettes', a compressor / "
          "limiter on every channel and VU meters on every input and output. The Mk I went into Studio Two in November "
          "1968 and made Abbey Road - the only Beatles album recorded on a TG - and solo records like All Things Must Pass "
          "and McCartney. Its channels have bass at 50 Hz and treble; the presence band was on the groups and mains.",
          "Solid-state: smoother and brighter than the REDD valves, tighter in the low end. Pushed, the transistors get "
          "edgier rather than fatter. The channel compressors squash hard - COMP is 2:1, LIMIT 8:1 - for that pumping, "
          "glued late-60s sound on drums, piano and guitars.",
          true, true, EqStyle::tgMk1, true, true, true, true,
          0.95f, 0.06f, 0.99f, 0.0f, 10.0f, 22000.0f },

        { "EMI TG12345 Mk IV", "TG12345 MK IV",
          "EMI  -  England  -  Abbey Road, 1971 to 1983",
          "From the Mk II on, the TGs swapped each channel's treble for the presence band the Mk I had on its groups and "
          "mains: seven bell frequencies from 500 Hz up, and a 10 kHz shelf. The Mk III and Mk IV added utility options, "
          "but the EQ and compressors stayed the same. A Mk IV made The Dark Side of the Moon.",
          "The TG's sound with the presence band on every channel: carve space for each instrument in the mids, or open "
          "the top with the 10 kHz shelf. The same hard-working channel compressors and SPREAD.",
          true, true, EqStyle::tgMk4, true, true, true, true,
          0.95f, 0.06f, 0.99f, 0.0f, 10.0f, 22000.0f },

        { "Trident A-Range", "A-RANGE",
          "Trident Audio Developments  -  London  -  Trident Studios, early 1970s",
          "Designed at Trident Studios by Malcolm Toft and Barry Porter, with engineers and producers like Roy Thomas Baker, "
          "Ken Scott and Tony Visconti; only 13 were built. Records made on it include Bowie's Ziggy Stardust, Lou Reed's "
          "Transformer and Queen's Sheer Heart Attack. Its four-band inductor EQ has a slide fader for each band's boost "
          "and cut and a four-position switch for its frequency, with high- and low-pass filters that stack.",
          "Warm, musical and a little aggressive: broad inductor bells in the mids, gentle 6 dB/octave shelves, and bands "
          "that interact when they're close. Great on kick, bass, vocals and electric guitars.",
          true, true, EqStyle::bands, false, false, false, false,
          0.93f, 0.15f, 0.97f, 0.08f, 12.0f, 0.0f, false, "REVERB" },

        { "Neve 8028", "8028",
          "Neve Electronics  -  England  -  1970s; Sound City, Los Angeles, from 1973",
          "Rupert Neve's class A desks, transformer-coupled with Marinair transformers. Sound City's 8028 made records "
          "from Fleetwood Mac's 1975 album to Nirvana's Nevermind, and starred in Dave Grohl's Sound City film. The "
          "channels here have the 1084 EQ: a high shelf at 10, 12 or 16 kHz, a mid bell from 360 Hz to 7.2 kHz with "
          "Hi-Q, a low shelf at 35, 60, 110 or 220 Hz, and 18 dB/octave high- and low-pass filters.",
          "Big, warm and solid: a thick low end and a smooth top, with the transformers adding weight and a gentle "
          "even-harmonic glow when you push the GAIN. The classic 70s rock console.",
          true, true, EqStyle::bands, false, false, false, false,
          0.90f, 0.25f, 0.96f, 0.12f, 14.0f, 0.0f, false, "REVERB" },

        { "SSL 4000 E", "4000 E",
          "Solid State Logic  -  Oxford, England  -  from 1979",
          "The desk of the 80s. Every channel has a four-band EQ (the 'black knob' 242), high- and low-pass filters, and "
          "its own dynamics: a compressor / limiter and an expander / gate. At the Townhouse, Hugh Padgham and Phil "
          "Collins found the big gated drum sound on an SSL (its talkback mic compressor, with a gate).",
          "Clean, fast and punchy. The EQ is precise and can be aggressive; the channel compressor and gate tighten "
          "drums and vocals into that bright, upfront, controlled 80s sound.",
          true, true, EqStyle::bands, false, false, false, false,
          0.98f, 0.02f, 0.99f, 0.0f, 5.0f, 0.0f, true, "REVERB" },

        { "Neotek Elite", "ELITE",
          "Neotek  -  Chicago, USA  -  Electrical Audio's custom Elite, 1998",
          "Neotek made quiet, transparent American consoles. Steve Albini's Electrical Audio in Chicago has a custom "
          "Neotek Elite in Studio A and a Series II in Studio B. The Elite's four-band state-variable parametric EQ: the "
          "outer bands peak or shelf, the mids are wide or narrow, and a sweepable high-pass.",
          "Clean and honest, with very low distortion and noise. It lets the room, the mics and the playing do the work.",
          true, true, EqStyle::bands, false, false, false, false,
          0.99f, 0.01f, 0.995f, 0.0f, 5.0f, 0.0f, false, "REVERB" },
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
inline bool presenceIsShelf (int i) { return i >= numPresenceFreqs - 1; }
inline bool isTg (Desk d)           { return d == Desk::tg12345 || d == Desk::tg12345mk4; }
inline bool isEmi (Desk d)          { return d == Desk::redd37 || d == Desk::redd51 || isTg (d); }
constexpr int numRecoveries = 6;
inline float recoverySeconds (int i)
{
    static const float s[numRecoveries] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f };
    return s[juce::jlimit (0, numRecoveries - 1, i)];
}
/** Compressor modes. */
enum class CompMode { off = 0, comp = 1, limit = 2 };

} // namespace whitman
