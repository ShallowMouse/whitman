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

The top bar tells you what WHITMAN sees: `HAWTHORNE ROUTE · 24 TRACKS · 48.0 kHz` when it's all connected, or what's missing.

## The console

Each strip, top to bottom:

* **Name**: double-click to name the track.
* **Pan**: constant power (−3 dB in the middle). Double-click to centre.
* **M / S**: mute and solo. Several tracks can be soloed; mute wins over solo. A strip you can't hear is dimmed.
* **Fader**: −∞ to +10 dB. Double-click for 0 dB. The meter beside it is the track after its fader.

**MASTER** is the level of the mix sent to Hawthorne, with a stereo meter. **CLIP** lights if the mix goes over 0 dBFS; click it to clear.

The mix is kept between runs automatically. **SAVE** / **OPEN** keep mixes as `.whitman` files; **RESET** puts every fader to 0 dB and every pan in the middle.

## Other audio devices

WHITMAN works with any audio device: its inputs become strips 1, 2, 3… and the mix goes to outputs 1-2. Choose which channels in **AUDIO**.

## Building

JUCE 8, CMake. Every push to `main` builds on GitHub, runs the engine tests (`Tests/WhitmanTests.cpp`) and publishes the installer to `ShallowMouse/Whitman-Releases`; see `Installer/README.md`.

```
cmake -S . -B build -DWHITMAN_BUILD_TESTS=ON
cmake --build build --config Release
```

`Source/HawthorneRoute/RouteShared.h` is a copy of Hawthorne's (`Source/Route/RouteShared.h`), used only to show whether Hawthorne is running. Keep it in step if Hawthorne Route's shared memory changes version.
