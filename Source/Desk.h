#pragma once

#include <juce_core/juce_core.h>

namespace whitman
{

/** The desks WHITMAN can be. Saved by number in mix files: only ever add to the end. */
enum class Desk { whitman = 0, redd51 = 1, redd37 = 2, tg12345 = 3, tg12345mk4 = 4,    // tg12345 = the Mk I
                  tridentA = 5, neve8028 = 6, ssl4000e = 7, neotek = 8,
                  rca76d = 9, redd17 = 10, ua610 = 11 };
constexpr int numDesks = 12;

/** The channel EQ's layout.
    redd:  bass 100 Hz shelf; treble Pop (lift a bell at 5 kHz, cut a 10 kHz shelf) or Classic (10 kHz shelf)
    tgMk1: bass 50 Hz shelf; treble (lift a bell at 5 kHz, cut a 10 kHz shelf)
    tgMk4: bass 50 Hz shelf; presence at 8 positions (7 bells, then a 10 kHz shelf) - no treble */
enum class EqStyle { none, redd, tgMk1, tgMk4, bands };     // bands: BandEq.h (REDD.17, UA 610, Trident, Neve, SSL, Neotek)

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
    bool mono = false;                     // one output: no pan pots, the mix comes out the same both sides
    bool steppedPan = false;               // switched pan pots (5 positions a side), as on the REDDs
};

/*  Sources, and the calls made where they disagree:
    - REDD.37 / .51: Kehew & Ryan, Recording the Beatles (EQ +/-10 dB in 2 dB steps, p. 89); EMI's description
      of the desk via Phaedrus Audio (constant-impedance passive EQ with 10 dB loss, Pop top boost 'broadly tuned
      to 4,700 c/s', pan pots switched 5 positions a side with 3 dB loss, Spreaders on the stereo channels 1, 2,
      7, 8); Sound On Sound (REDD.47, the .51 'essentially identical' to the .37, M/S Spreaders); MusicTech
      "Studio Icons: EMI REDD.51" (VU meters and a stereo correlator); Abbey Road "REDD.37 #GearThatMadeUs";
      Puremix; Wikipedia "EMI REDD"; K. Hicks, MTSU thesis (EQ network values). ReddEq.h has the details.
      Still a simplification: the Spreader here works on the whole mix (on the desks it sat on the stereo input
      channels), and only the REDD.51 has it.
    - TG12345: MusicTech "Vintage: EMI TG12345"; Abbey Road "#GearThatMadeUs"; the Waves TG12345 guide; Tape Op's
      review; Chandler's TG12345 MkIV EQ (Bonedo's test). Bass 50 Hz shelf (Chandler's MkIV EQ module uses 90/150 Hz,
      but it blends input and group EQs; the console sources say 50 Hz). Mk I channel treble: Waves (measured with
      Abbey Road) has the lift a 5 kHz bell and the cut a 10 kHz shelf; MusicTech says a 10 kHz peak - Waves used.
      Presence (Mk II on): 500, 800, 1.2k, 1.8k, 2.8k, 4.2k, 6.8k (Chandler: 6.5k) bells, then a 10 kHz shelf.
      Limiter: MusicTech 8:1, Waves 7:1 - 8:1 used. Recovery: six positions, 0.1 to 5 s (the in-between values are
      spread evenly here). Faders: no source found; straight faders drawn.
    - GAIN (all EMI desks) isn't a control on the real desks: it stands in for how hard the amps were driven.
    - RCA 76-D (Sun): Sound On Sound and scottymoore.net (a used RCA 76-D broadcast console from about 1954 - before
      it a five-input Presto - with two Ampex 350s, the second for the slapback); Vintage Guitar (Matt Ross-Spang:
      four mic inputs); thebdr.net (RCA's 76 series announced in 1940). A mono valve broadcast board,
      transformer-coupled, with rotary mixing pots and no EQ. Sun's echo was tape slapback, not a send, so it has
      none here. The amps follow RCA's instruction books for the 76 series (AmpModel.h).
    - EMI REDD.17: Abbey Road "#GearThatMadeUs" (1958, Peter Burkowitz and Lenn Page, four built, EMI's first
      console in desk form); Recording magazine (eight inputs, mono / stereo out, Siemens V72 amps, a simple
      two-band EQ); Vintage King (two buses; Spreaders on channels 1-2 and 7-8). The EQ's frequencies aren't
      published: a 10 kHz and a 100 Hz shelf (the REDD.37's Classic module) are assumed.
    - Universal Audio 610: Bill Putnam's modular valve channel amplifier, 1958, built into one-off consoles at
      United and Western in Hollywood (UA; Paul McManus's history). The 610-A module's EQ: a 10 kHz high shelf
      and a 100 Hz low shelf (UA's 610 manual), stepped to 9 dB (the 2-610's steps). The line amps' +30 dBm is
      McManus's; the rest of the amp figures are estimated (AmpModel.h).  */
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
          "EMI built its own desks. Designed in 1959, the REDD.51 went into Studio Three in 1963 and Studio Two in "
          "January 1964, where it fed the Studer J37 4-tracks through Revolver and Sgt. Pepper until the solid-state "
          "TG12345 took over in November 1968. The same facilities as the REDD.37, but every amplifier - channel, mix and "
          "line - is EMI's own REDD.47 (EF86 into E88CC), cheaper than the Siemens V72S and with more headroom and less "
          "distortion. Eight mic channels, passive tone controls switched in 2 dB steps (Pop or Classic modules - Studio "
          "Two ran Pop), switched pan pots, echo sends to the chambers, Painton quadrant faders, VU meters and a phase "
          "(stereo correlator) meter.",
          "Valve, but punchy and clean until pushed: the REDD.47's stages cancel each other's second harmonic, so what "
          "it adds when driven is mostly third - a firmer, harder edge. Pop treble is a broad lift centred on 4.7 kHz; "
          "Pop bass reaches up into the low mids.",
          true, true, EqStyle::redd, false, true, false, false,
          0.90f, 0.35f, 0.98f, 0.15f, 8.0f, 20000.0f },

        { "EMI REDD.37", "REDD.37",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road Studio Two, 1958 to 1964",
          "Designed in 1958 and first used on 8 September 1960; three were built, for Studios One and Two. Eight mic "
          "channels into four outputs, run on 31 Siemens V72 valve amplifiers (two EF804 pentodes each). Nicknamed 'the "
          "Beatles' console': Love Me Do, Please Please Me and most of With The Beatles. Passive tone controls in 2 dB "
          "steps (Pop or Classic modules), switched pan pots, echo sends, Painton quadrant faders, VU meters and a phase "
          "meter.",
          "Valve, warm and laid back beside the REDD.51: the V72s run out of headroom sooner and clip lopsided, so the "
          "GAIN thickens with second harmonic and squashes sooner. Full range - the V72 is flat to 20 kHz.",
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

        { "RCA 76-D", "76-D",
          "RCA  -  USA  -  Sun Studio, Memphis, from about 1954",
          "A valve radio broadcast console that Sam Phillips bought used for Sun Studio, replacing a five-input Presto. "
          "Mono, transformer-coupled, a handful of mic inputs on big rotary pots and no EQ. With it and two Ampex 350s - "
          "the second for the slapback echo - he recorded Elvis, Johnny Cash, Carl Perkins and Jerry Lee Lewis.",
          "Mono and valve: clean at its normal +18 dBm, thickening with second harmonic as you push it towards its "
          "+26 dBm ceiling, a broadcast bandwidth of about 30 Hz to 15 kHz and the iron of its transformers in the low "
          "end. Everything comes out the middle.",
          true, false, EqStyle::none, false, false, false, false,
          0.62f, 0.50f, 0.92f, 0.25f, 30.0f, 15000.0f, false, "ECHO", true, false },

        { "EMI REDD.17", "REDD.17",
          "EMI (Record Engineering Development Dept.)  -  England  -  Abbey Road, 1958",
          "EMI's first console in desk form, designed by Peter Burkowitz and Lenn Page for stereo; four were built, for "
          "Abbey Road's mixing rooms and for recording away. Eight mic inputs into two buses, Siemens V72 valve "
          "amplifiers, a simple two-band EQ on every channel, echo sends, and Spreaders for the stereo mic pairs. It "
          "was relied on through the late 50s, before 4-track brought the REDD.37.",
          "Valve and V72: warm, thick and lopsided when driven, with second harmonic and an early squash. Two gentle "
          "shelves for tone.",
          true, true, EqStyle::bands, false, true, false, false,
          0.68f, 0.48f, 0.94f, 0.22f, 14.0f, 12000.0f, false, "ECHO", false, true },

        { "Universal Audio 610", "610",
          "Universal Audio (Bill Putnam)  -  Hollywood, USA  -  United Recording, from 1958",
          "Bill Putnam built his own consoles: one-offs for United and Western in Hollywood, each made from his modular "
          "610 valve channel amplifiers - the start of the modular console. Sinatra, Nat King Cole, Ray Charles and later "
          "the Beach Boys were recorded through them. The 610's EQ is two shelves: 10 kHz and 100 Hz.",
          "Valve and transformers, but smooth: a rich, round low end and a gentle sheen on top, with a warm "
          "even-harmonic thickness when you push the GAIN. The sound of the great late-50s vocal records.",
          true, true, EqStyle::bands, false, false, false, false,
          0.84f, 0.30f, 0.96f, 0.12f, 10.0f, 0.0f },
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
inline bool isEmi (Desk d)          { return d == Desk::redd17 || d == Desk::redd37 || d == Desk::redd51 || isTg (d); }
constexpr int numRecoveries = 6;
inline float recoverySeconds (int i)
{
    static const float s[numRecoveries] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f };
    return s[juce::jlimit (0, numRecoveries - 1, i)];
}
/** Compressor modes. */
enum class CompMode { off = 0, comp = 1, limit = 2 };

} // namespace whitman
