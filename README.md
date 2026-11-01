# WHITMAN

A mixer for Hawthorne, from PEQUOD. Each tape track comes into its own channel strip, you mix it, and the stereo mix goes back into Hawthorne's 2-track master.

WHITMAN talks to Hawthorne through **Hawthorne Route**, the ASIO driver Hawthorne installs. No network, no extra cables, and the two run sample-locked.

## Setting up

1. **Hawthorne Route** must be installed: Hawthorne → SETTINGS (UPDATE DRIVER / install). Start Hawthorne.
2. **WHITMAN → SETTINGS**: device type **ASIO**, device **Hawthorne Route**. On first run WHITMAN picks it by itself if it's installed.
   WHITMAN turns on every channel the driver offers, and shows one strip per track on the reel. When the reel's track count changes, the strips follow.
3. **In Hawthorne's ROUTING:**
   * set the master **L / R inputs** to **ROUTE master L / R**. That's WHITMAN's mix coming back.
   * switch **MIX** off on every track. Hawthorne's master is its own track mix *plus* the returns, so with MIX left on you'd hear every track twice.

The top bar tells you what WHITMAN sees: `HAWTHORNE ROUTE · REEL: … · 24 TRACKS · 48.0 kHz` when it's all connected, or what's missing.

With Hawthorne 1.24 or later, WHITMAN also gets each track's **name** from Hawthorne, lights a red lamp on the strips of **armed** tracks, shows the **reel's name**, and warns you if any track still has MIX on in Hawthorne. A name typed on a WHITMAN strip wins; clear it to follow Hawthorne again.

## Desks

**DESK** picks the console the tracks are mixed on. Like Hawthorne's machines, each desk has its own face and its own sound, and the choice is saved with the mix.

* **EMI REDD.37** (Studio Two, 1958 to 1964: the early Beatles records): valve, with Siemens V72 channel amps.
* **EMI REDD.51** (Studio Two, 1964 to 1968: Revolver, Sgt. Pepper; fed the Studer J37s): valve, with REDD.47 channel amps (more headroom, less distortion than the V72s).
  Both REDDs, per channel:
  * **GAIN** into the valve channel amp. Around 0 it's clean; push it and the valves thicken and grit up. Bring the fader down to match. *(Not a control on the real desks: it stands in for how hard the signal hit the amps.)*
  * **TREBLE** ±10 dB with the **POP** module (lift: a bell around 5 kHz; cut: a shelf at 10 kHz) or the **CLAS**sic module (a 10 kHz shelf both ways), and **BASS** ±10 dB, a 100 Hz shelf.
  * **ECHO**: a send, after the fader, to an echo chamber; **ECHO RETURN** on the master brings it back.
  * **PAN**, and a Painton quadrant fader (the lever swings on an arc under the panel; from above it's a straight slot).
  The REDD.51 also has the **SPREADER** on the master, for the stereo width. Both mix through valve main amps, which round off before full scale, and are read on VU meters.
* **EMI TG12345 Mk I** (Studio Two from November 1968: *Abbey Road*, *All Things Must Pass*) and **Mk IV** (1971 to 1983: *The Dark Side of the Moon*): EMI's solid-state desks, with a VU meter on every channel. Per channel:
  * **GAIN** into a transistor channel amp: it gets edgier, not fatter, when pushed. *(Again, not a control on the real desk.)*
  * EQ: **BASS** ±10 dB, a 50 Hz shelf, plus
    * on the **Mk I**, **TREBLE** ±10 dB (lift: a bell at 5 kHz; cut: a 10 kHz shelf). Its presence band was only on the groups and mains;
    * on the **Mk IV**, **PRES**ence ±10 dB instead of treble: a bell at 500, 800, 1.2k, 1.8k, 2.8k, 4.2k or 6.8k, or a 10 kHz shelf (the small switch).
  * **COMP**: the channel compressor. The button picks **OFF**, **COMP** (2:1) or **LIMIT** (8:1); the knob sets how hard it's driven (with some make-up gain), the small switch the recovery (0.1 to 5 s), and the figure above shows the gain reduction.
  * **ECHO** and **PAN**, and straight faders.
  On the master: **PRESENCE** on the mains, and **SPREAD** for the stereo width (all the way left is mono, the middle as panned, further right wider).
* **Trident A-Range** (Trident Studios, London, early 70s: *Ziggy Stardust*, *Transformer*, *Sheer Heart Attack*): four-band inductor EQ, each band's boost / cut on a slide fader (±15 dB) and its frequency on a four-position switch: **TREB**le shelf 8 / 10 / 12 / 15 kHz and **BASS** shelf 50 / 80 / 100 / 150 Hz (both gentle 6 dB/octave), **HI** mid 3 / 5 / 7 / 9 kHz and **LO** mid 250 / 500 / 1k / 2k (broad bells). High-pass 25 / 50 / 100 Hz and low-pass 9 / 12 / 15 kHz buttons, which stack.
* **Neve 8028** (Sound City from 1973: Fleetwood Mac, *Nevermind*), with the 1084 channel EQ: **HIGH** shelf 10 / 12 / 16 kHz ±16 dB, **MID** bell 360 Hz to 7.2 kHz ±18 dB with **HI-Q**, **LOW** shelf 35 / 60 / 110 / 220 Hz ±16 dB, high-pass 50 / 80 / 160 / 300 Hz and low-pass 18 / 10 / 8 / 6 kHz (18 dB/octave). Class A, Marinair transformers.
* **SSL 4000 E** (from 1979): the black-knob EQ, all swept: **HF** 1.5–16 kHz shelf or **BELL**, **HMF** 600 Hz–7 kHz and **LMF** 200 Hz–2.5 kHz with Q 0.1–3.5 (±15 dB at the widest to ±18 at the narrowest), **LF** 30–450 Hz shelf or **BELL**; high-pass 16–350 Hz and low-pass 3–22 kHz. Channel dynamics: **COMP** (ratio 1:1 to ∞:1, threshold +10 to −20 dBu, release 0.1–4 s, **F.ATK** 1 ms, automatic make-up) and **EXP / GATE** (range 0–40 dB, threshold −30 to +10 dBu, release, fast attack).
* **Neotek Elite** (Steve Albini's Electrical Audio, Chicago): four-band state-variable parametric; **HIGH** and **LOW** bell or **SHELF**, the mids wide or **NARR**ow; a sweepable high-pass from 20 to 375 Hz.
* **WHITMAN (clean)**: the plain console below, with nothing added.

Each band-EQ desk keeps its own EQ settings, so you can switch desks and back without losing them.

### How accurate the sound is

The EQ curves follow each desk's published frequencies, ranges and filter slopes; the tests check every band of every desk against them. The amplifiers are modelled from the outside: where each one clips, how it rounds off into the clip (gradually for valves, at the last moment for op-amps), its even harmonics, and its transformers' low-end saturation, which rises as the frequency falls. Levels follow Hawthorne: 0 VU = +4 dBu = −18 dBFS, so 0 dBFS = +22 dBu and a desk that clips at +26 dBu has 4 dB over digital full scale (WHITMAN's CLIP lamp shows it; Hawthorne gets the floats).

Only the Neve has published distortion figures (no more than 0.07 % THD from 50 Hz to 10 kHz at +20 dBu, flat 20 Hz–20 kHz, over +26 dBu out), and it's calibrated to them: a channel through the whole desk measures 0.065 % at 50 Hz and 0.035 % at 1 kHz. The others' figures are estimates from how they're documented (valve or solid-state, transformers or not, which has more headroom than which); `Source/AmpModel.h` says what each rests on. Matching the real units more closely than that needs measurements of the hardware.

The EMI desks are modelled from published descriptions (Abbey Road, MusicTech's Studio Icons, the Waves/Abbey Road plug-in guides, Chandler's licensed TG and REDD hardware), not measured from the real ones. Where sources disagree, the choice is noted in `Source/Desk.h`. Not modelled: the real desks' channel counts (8 on the REDDs, 24 on the TGs; WHITMAN gives you one strip per tape track), their groups and multiple echo sends (one chamber here, a general chamber-like reverb rather than Studio Two's), and noise and hum.

## The console

Each strip, top to bottom:

* **Name**: Hawthorne's name for the track, or double-click to name it here.
* **Pan**: constant power (−3 dB in the middle). Double-click to centre.
* **M / S**: mute and solo. Several tracks can be soloed; mute wins over solo. A strip you can't hear is dimmed.
* **Fader**: −∞ to +10 dB. Double-click for 0 dB. The meter beside it is the track after its fader.

**MASTER** is the level of the mix sent to Hawthorne, with a stereo meter. **CLIP** lights if the mix goes over 0 dBFS; click it to clear. (The REDD.51's main amps can't clip, so it has no CLIP lamp.)

The mix is kept between runs automatically. **SAVE** / **OPEN** keep mixes as `.whitman` files; **RESET** puts every fader to 0 dB, every pan in the middle and the EQ flat, and turns the echo off. It keeps the desk.

## Settings

**SETTINGS** has the audio device and the **METERS**: the EMI desks' needle meters as **VU** (average, 300 ms) or **PPM** (the EMI style, as in Hawthorne: peaks, marks 1–7 with 4 at 0 VU's level, falling 24 dB in 2.8 s).

The mouse wheel scrolls the console; it never moves a fader or turns a knob.

## Other audio devices

WHITMAN works with any audio device: its inputs become strips 1, 2, 3… and the mix goes to outputs 1-2. Choose which channels in **SETTINGS**.

## Building

JUCE 8, CMake. Every push to `main` builds on GitHub, runs the engine tests (`Tests/WhitmanTests.cpp`) and publishes the installer to `ShallowMouse/Whitman-Releases`; see `Installer/README.md`.

```
cmake -S . -B build -DWHITMAN_BUILD_TESTS=ON
cmake --build build --config Release
```

`Source/HawthorneRoute/RouteShared.h` is a copy of Hawthorne's (`Source/Route/RouteShared.h`), used only to show whether Hawthorne is running. Keep it in step if Hawthorne Route's shared memory changes version.
