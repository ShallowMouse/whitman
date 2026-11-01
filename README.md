# WHITMAN

A mixer for Hawthorne, from PEQUOD. Each tape track comes into its own channel strip, you mix it, and the stereo mix goes back into Hawthorne's 2-track master.

WHITMAN talks to Hawthorne through **Hawthorne Route**, the ASIO driver Hawthorne installs. No network, no extra cables, and the two run sample-locked.

## Setting up

1. **Hawthorne Route** must be installed: Hawthorne → SETTINGS (UPDATE DRIVER / install). Start Hawthorne.
2. **WHITMAN → AUDIO**: device type **ASIO**, device **Hawthorne Route**. On first run WHITMAN picks it by itself if it's installed.
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
* **WHITMAN (clean)**: the plain console below, with nothing added.

The EMI desks are modelled from published descriptions (Abbey Road, MusicTech's Studio Icons, the Waves/Abbey Road plug-in guides, Chandler's licensed TG and REDD hardware), not measured from the real ones. Where sources disagree, the choice is noted in `Source/Desk.h`. Not modelled: the real desks' channel counts (8 on the REDDs, 24 on the TGs; WHITMAN gives you one strip per tape track), their groups and multiple echo sends (one chamber here, a general chamber-like reverb rather than Studio Two's), and noise and hum.

## The console

Each strip, top to bottom:

* **Name**: Hawthorne's name for the track, or double-click to name it here.
* **Pan**: constant power (−3 dB in the middle). Double-click to centre.
* **M / S**: mute and solo. Several tracks can be soloed; mute wins over solo. A strip you can't hear is dimmed.
* **Fader**: −∞ to +10 dB. Double-click for 0 dB. The meter beside it is the track after its fader.

**MASTER** is the level of the mix sent to Hawthorne, with a stereo meter. **CLIP** lights if the mix goes over 0 dBFS; click it to clear. (The REDD.51's main amps can't clip, so it has no CLIP lamp.)

The mix is kept between runs automatically. **SAVE** / **OPEN** keep mixes as `.whitman` files; **RESET** puts every fader to 0 dB, every pan in the middle and the EQ flat, and turns the echo off. It keeps the desk.

## Other audio devices

WHITMAN works with any audio device: its inputs become strips 1, 2, 3… and the mix goes to outputs 1-2. Choose which channels in **AUDIO**.

## Building

JUCE 8, CMake. Every push to `main` builds on GitHub, runs the engine tests (`Tests/WhitmanTests.cpp`) and publishes the installer to `ShallowMouse/Whitman-Releases`; see `Installer/README.md`.

```
cmake -S . -B build -DWHITMAN_BUILD_TESTS=ON
cmake --build build --config Release
```

`Source/HawthorneRoute/RouteShared.h` is a copy of Hawthorne's (`Source/Route/RouteShared.h`), used only to show whether Hawthorne is running. Keep it in step if Hawthorne Route's shared memory changes version.
