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

private:
    int numChannels;
    std::array<float, 2> level { -100.0f, -100.0f }, hold { -100.0f, -100.0f };
    std::array<int, 2> holdFrames {};
};

//==============================================================================
/** A needle VU meter: cream face, -20 to +3, 0 VU = -18 dBFS (as in Hawthorne). */
class VUMeter : public juce::Component
{
public:
    explicit VUMeter (juce::String label) : caption (std::move (label)) {}
    void push (float peak);          // ~30 times a second
    void paint (juce::Graphics&) override;

private:
    juce::String caption;
    float needle = -20.0f;           // VU
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

private:
    const DeskSkin* skin = nullptr;
};

//==============================================================================
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (MixEngine& engine, int index, juce::StringArray& names);

    void setDesk (Desk d);
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
    juce::TextButton trebleMode { "POP" };
    juce::TextButton mute { "M" }, solo { "S" };
    Fader fader;
    Meter meter { 1 };
    juce::Label dbLabel;
    bool dimmed = false;

    void changed() { if (onChanged) onChanged(); }
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
    DeskKnob echoReturn;
    juce::Label dbLabel;
    bool clipped = false;
    juce::Rectangle<int> lampArea;
};

} // namespace whitman
