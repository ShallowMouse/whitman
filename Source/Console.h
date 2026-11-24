#pragma once

#include <JuceHeader.h>
#include "MixEngine.h"
#include "WhitmanLookAndFeel.h"

namespace whitman
{

//==============================================================================
/** How a desk looks: colours, knobs, faders, meters. */
/** Every desk's channel strips are this wide. */
constexpr int channelWidth = 104;

struct DeskSkin
{
    juce::Colour frame;           // behind the strips
    juce::Colour panelTop, panelBottom, edge;
    juce::Colour engraving, engravingDim, accent;
    juce::Colour well;            // name plate / meter background
    bool bakeliteKnobs;           // black knobs with a white pointer (else WHITMAN's own)
    bool quadrantFaders;          // Painton quadrant lever (else a straight fader)
    bool vuMeters;                // the master's meters are needle VUs (else bar meters)
    int stripWidth, masterWidth;
    int knobStyle = 0;            // 0 black bakelite, 1 aluminium, 2 grey with a coloured cap
    const char* plateLine = "";   // under the desk's name on the master plate
    int meterStyle = 0;           // each channel's meter: 0 a needle VU, 1 a plasma bargraph (SSL), 2 an LED bargraph (Neotek)

    // how the desk was built - so it looks like the hardware
    bool oneShell = false;        // one steel desk, its controls in rows (the REDDs, the RCA): no module seams between channels
    bool channelMeters = true;    // a meter on every channel (the REDDs, the RCA and the 610 had none: the mix's VUs only)
    bool meterBridge = false;     // the channels' meters in a hood across the top (the 1970s desks)
    bool backlitMeters = false;   // VUs lit from behind (the 1970s on)
    bool rotaryFaders = false;    // level on big rotary attenuators, not faders (the RCA broadcast board, the UA 610)
    bool woodCheeks = false;      // wooden end cheeks either side of the desk
    bool armrest = false;         // a padded armrest along the front
    int modulePairs = 1;          // channels per module (the TG's microphone cassettes held two)
    juce::Colour hood;            // the meter bridge's housing (transparent: the frame's)
    juce::Colour faderCap { 0xff8b9095 };   // a straight fader's cap
    juce::Colour mainFaderCap;    // the master's (transparent: as the channels')
    juce::Colour muteLamp { 0xffff5a3a }, soloLamp { 0xff7ee06a };
    /** Knob caps by section (knob style 1 and 3: aluminium anodised / chicken-head colours): input, EQ, dynamics, sends, pan. */
    juce::Colour knobGain, knobEq, knobDyn, knobSend, knobPan;

    static const DeskSkin& of (Desk d);
};

//==============================================================================
/** Peak meter with a slow fall and a held peak line (mono or stereo). */
class Meter : public juce::Component
{
public:
    explicit Meter (int channels = 1) : numChannels (channels) {}

    /** Feed the peak gain(s) since the last frame; call ~30 times a second. */
    void push (float p0, float p1 = 0.0f);
    void paint (juce::Graphics&) override;

    static float dbToPos (float db) noexcept;   // 0 (bottom) .. 1 (top), -60 .. +6 dB

    juce::Colour lowColour = Pequod::Colours::mustardLo.brighter (0.25f), midColour = Pequod::Colours::mustard;
    /** 0 a smooth bar; 1 a gas-plasma bargraph (orange segments, SSL); 2 an LED bargraph (green / yellow / red) */
    int style = 0;

private:
    int numChannels;
    std::array<float, 2> level { -100.0f, -100.0f }, hold { -100.0f, -100.0f };
    std::array<int, 2> holdFrames {};
};

//==============================================================================
/** A needle meter, either a VU (cream face, -20 to +3, 0 VU = -18 dBFS, as in Hawthorne) or an
    EMI-style PPM (black face, white marks 1-7, 4 = the same reference, 4 dB a mark; fast rise,
    falls 24 dB in 2.8 s). Which one is a setting for every meter (SETTINGS > METERS). */
class VUMeter : public juce::Component
{
public:
    explicit VUMeter (juce::String label) : caption (std::move (label)) {}
    void push (float peak);          // ~30 times a second
    void paint (juce::Graphics&) override;
    bool backlit = false;            // lit from behind (the 1970s desks), else a plain cream face

    static void setPpm (bool on) noexcept { ppm = on; }
    static bool isPpm() noexcept          { return ppm; }

private:
    juce::String caption;
    float needle = -20.0f;           // VU
    void paintGlass (juce::Graphics&, juce::Rectangle<float> face);
    float ppmDb = -40.0f;            // PPM, dB relative to the reference
    inline static bool ppm = false;
    void paintPpm (juce::Graphics&);
};

//==============================================================================
/** A console fader in dB: -inf (bottom) to +10, unity marked, double-click for unity.
    Drawn straight, or as a Painton quadrant lever on the EMI desks. */
class Fader : public juce::Slider
{
public:
    Fader();
    void setSkin (const DeskSkin* s) { skin = s; repaint(); }
    juce::String getTextFromValue (double v) override;
    double getValueFromText (const juce::String& t) override;
    void paint (juce::Graphics&) override;

    bool isMain = false;             // the master's

private:
    const DeskSkin* skin = nullptr;
    void paintStraight (juce::Graphics&);
    void paintQuadrant (juce::Graphics&);
    void paintRotary (juce::Graphics&);
};

//==============================================================================
/** A push-button with a lamp in its cap, lit when it's on (mute, solo, the switches). */
class LampButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
    void setSkin (const DeskSkin* s) { skin = s; repaint(); }
    juce::Colour lamp { 0xffff5a3a };
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    const DeskSkin* skin = nullptr;
};

//==============================================================================
/** A rotary control: WHITMAN's own, or a black bakelite knob (stepped like a switch
    on the EMI desks, with a dot for each step). */
class DeskKnob : public juce::Slider
{
public:
    DeskKnob();
    void setSkin (const DeskSkin* s) { skin = s; repaint(); }
    void paint (juce::Graphics&) override;
    juce::Colour cap;             // the cap's colour (style 2), transparent = the desk's grey

private:
    const DeskSkin* skin = nullptr;
};

//==============================================================================
class BandEqPanel;
class SslDynamicsPanel;

class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (MixEngine& engine, int index, juce::StringArray& names);
    ~ChannelStrip() override;

    void setDesk (Desk d);
    /** What Hawthorne says about this track (its name; armed to record). */
    void setHawthorneTrack (const juce::String& name, bool armed);
    void setTrackColour (juce::Colour c) { if (c != trackColour) { trackColour = c; repaint(); } }   // (MELVILLE: the reel's)
    std::function<void (int, const juce::String&)> onRenameTrack;     // MELVILLE: the name goes on the reel
    void refreshFromEngine();         // after a mix is opened / reset
    void updateMeter();               // UI timer
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onChanged;  // something the mix file keeps changed

private:
    juce::Colour trackColour { juce::Colours::transparentBlack };
    MixEngine& engine;
    const int index;
    juce::StringArray& names;
    Desk desk = Desk::whitman;
    const DeskSkin* skin = nullptr;

    juce::Label nameLabel;
    DeskKnob input, treble, bass, echo, pan;
    DeskKnob presence, presenceFreq, comp, recovery;     // TG12345
    VUMeter chanVu { {} };                               // TG: a VU on every channel
    std::unique_ptr<BandEqPanel> bandPanel;              // Trident, Neve, SSL, Neotek
    std::unique_ptr<SslDynamicsPanel> dynPanel;          // SSL
    juce::TextButton trebleMode { "POP" }, compMode { "OFF" };
    float shownGr = 0.0f;
    LampButton mute { "M" }, solo { "S" }, ins { "INS" };
    Fader fader;
    Meter meter { 1 };
    juce::Label dbLabel;
    bool dimmed = false;
    juce::String hawthorneName;
    bool hawthorneArmed = false;

    void changed() { if (onChanged) onChanged(); }
    void showName();
    void showCompMode();
    void drawKnobCaption (juce::Graphics&, const DeskKnob&, const juce::String& title, const juce::String& value);
    int hoodBottom = 0;               // where the meter bridge ends (0: none)
};

//==============================================================================
class MasterStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MasterStrip (MixEngine& engine);

    void setDesk (Desk d);
    void refreshFromEngine();
    void updateMeter();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // the clip lamp clears it; the desk's plate picks the desk
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void()> onChanged;
    std::function<void()> onPickDesk;                    // the desk's name plate was clicked

private:
    MixEngine& engine;
    Desk desk = Desk::whitman;
    const DeskSkin* skin = nullptr;
    Fader fader;
    Meter meter { 2 };
    VUMeter vuL { "LEFT" }, vuR { "RIGHT" };
    DeskKnob echoReturn, spread, mainPresence, mainPresenceFreq;
    juce::Label dbLabel;
    bool clipped = false;
    juce::Rectangle<int> lampArea, corrArea;     // corrArea: the REDD desks' stereo correlator
    juce::Rectangle<int> plateArea;               // the desk's name plate: click to change desk
    juce::Rectangle<int> tapeArea;                // 'MIX' on tape under the master fader
    bool plateHover = false;
    void paintPlateCue (juce::Graphics&, juce::Rectangle<float> plate);
    float shownCorr = 1.0f;
    void paintCorrelator (juce::Graphics&);
    juce::Rectangle<int> hoodArea;                // the meters' housing
};

} // namespace whitman
