#pragma once

#include <JuceHeader.h>
#include "MixEngine.h"
#include "WhitmanLookAndFeel.h"

namespace whitman
{

//==============================================================================
/** How a desk looks: colours, knobs, faders, meters. */
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

    static void setPpm (bool on) noexcept { ppm = on; }
    static bool isPpm() noexcept          { return ppm; }

private:
    juce::String caption;
    float needle = -20.0f;           // VU
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

private:
    const DeskSkin* skin = nullptr;
    void paintStraight (juce::Graphics&);
    void paintQuadrant (juce::Graphics&);
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
    void refreshFromEngine();         // after a mix is opened / reset
    void updateMeter();               // UI timer
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onChanged;  // something the mix file keeps changed

private:
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
    juce::TextButton mute { "M" }, solo { "S" };
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
};

//==============================================================================
class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip (MixEngine& engine);

    void setDesk (Desk d);
    void refreshFromEngine();
    void updateMeter();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // click the clip lamp to clear it

    std::function<void()> onChanged;

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
    float shownCorr = 1.0f;
    void paintCorrelator (juce::Graphics&);
};

} // namespace whitman
