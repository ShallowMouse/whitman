#pragma once

#include <JuceHeader.h>
#include "MixEngine.h"
#include "WhitmanLookAndFeel.h"

namespace whitman
{

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

private:
    int numChannels;
    std::array<float, 2> level { -100.0f, -100.0f }, hold { -100.0f, -100.0f };
    std::array<int, 2> holdFrames {};
};

//==============================================================================
/** A console fader in dB: -inf (bottom) to +10, unity marked, double-click for unity. */
class Fader : public juce::Slider
{
public:
    Fader();
    juce::String getTextFromValue (double v) override;
    double getValueFromText (const juce::String& t) override;
    void paint (juce::Graphics&) override;
};

//==============================================================================
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (MixEngine& engine, int index, juce::StringArray& names);

    void refreshFromEngine();         // after a mix is opened / reset
    void updateMeter();               // UI timer
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onChanged;  // something the mix file keeps changed

    static constexpr int width = 76;

private:
    MixEngine& engine;
    const int index;
    juce::StringArray& names;

    juce::Label nameLabel;
    juce::Slider pan;
    juce::TextButton mute { "M" }, solo { "S" };
    Fader fader;
    Meter meter { 1 };
    juce::Label dbLabel;
    bool dimmed = false;
};

//==============================================================================
class MasterStrip : public juce::Component
{
public:
    explicit MasterStrip (MixEngine& engine);

    void refreshFromEngine();
    void updateMeter();
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // click the clip lamp to clear it

    std::function<void()> onChanged;
    static constexpr int width = 132;

private:
    MixEngine& engine;
    Fader fader;
    Meter meter { 2 };
    juce::Label dbLabel;
    bool clipped = false;
    juce::Rectangle<int> lampArea;
};

} // namespace whitman
