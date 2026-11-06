#pragma once

#include "Console.h"

namespace whitman
{

//==============================================================================
/** An EQ band's boost / cut on a short slide fader (the Trident A-Range). */
class EqSlider : public juce::Slider
{
public:
    EqSlider();
    void setSkin (const DeskSkin* s) { skin = s; repaint(); }
    void paint (juce::Graphics&) override;

private:
    const DeskSkin* skin = nullptr;
};

//==============================================================================
/** A channel's band EQ and filters, for the desks that have one (BandEq.h). */
class BandEqPanel : public juce::Component
{
public:
    BandEqPanel (MixEngine& engine, int strip);

    void setDesk (Desk d);
    void refresh();
    void setScale (float s, int width) { scale = s; availW = width; }
    int preferredHeight() const;
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onChanged;

private:
    MixEngine& engine;
    const int index;
    Desk desk = Desk::whitman;
    const BandEqDef* def = nullptr;
    const DeskSkin* skin = nullptr;

    std::array<DeskKnob, 4> gain, freq, q;
    std::array<EqSlider, 4> slider;
    std::array<juce::TextButton, 4> alt;
    DeskKnob hpf, lpf;
    std::array<juce::TextButton, 3> hpfButton, lpfButton;
    float scale = 1.0f;
    int availW = 100;
    // knobs grow with the scale but always fit the strip's width; the space between rows grows more freely
    int rowKnob() const   { return juce::jmin (juce::roundToInt (30.0f * scale), availW * 34 / 100); }
    int smallKnob() const { return juce::jmin (juce::roundToInt (22.0f * scale), availW * 24 / 100); }
    int rowGap() const    { return juce::jmax (2, juce::roundToInt (2.0f + (scale - 1.0f) * 12.0f)); }
    // the Trident's slide-fader EQ
    int sliderH() const   { return juce::jlimit (80, 150, juce::roundToInt (96.0f * scale)); }
    int switchSz() const  { return juce::jlimit (20, availW / 4 - 2, juce::roundToInt (22.0f * scale)); }
    int buttonH() const   { return juce::jlimit (16, 20, juce::roundToInt (17.0f * scale)); }

    BandEqParams* params() const;
    void changed() { repaint(); if (onChanged) onChanged(); }
    juce::String freqText (int band) const;
    static juce::String hzText (float hz);
    juce::String filterText (const FilterDef& f, float pos) const;
};

//==============================================================================
/** The SSL's channel dynamics: compressor / limiter and expander / gate. */
class SslDynamicsPanel : public juce::Component
{
public:
    SslDynamicsPanel (MixEngine& engine, int strip);

    void setSkin (const DeskSkin* s);
    void refresh();
    void updateMeters();             // the gain-reduction figures (UI timer)
    void setScale (float s, int width) { scale = s; availW = width; }
    int preferredHeight() const { return 2 * (12 + rowKnob() + rowGap()); }
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onChanged;

private:
    MixEngine& engine;
    const int index;
    const DeskSkin* skin = nullptr;
    DeskKnob ratio, threshold, release, range, gateThreshold, gateRelease;
    juce::TextButton fast { "F.ATK" }, gateFast { "F.ATK" }, gateMode { "EXP" };
    float shownGr = 0.0f, shownGate = 0.0f;
    float scale = 1.0f;
    int availW = 100;
    int rowKnob() const   { return juce::jmin (juce::roundToInt (30.0f * scale), availW * 30 / 100); }
    int smallKnob() const { return juce::jmin (juce::roundToInt (22.0f * scale), availW * 22 / 100); }
    int rowGap() const    { return juce::jmax (2, juce::roundToInt (2.0f + (scale - 1.0f) * 12.0f)); }
    void changed() { repaint(); if (onChanged) onChanged(); }
};

/** A row's caption: the title on the left, its value on the right. */
void drawRowCaption (juce::Graphics&, const DeskSkin&, juce::Rectangle<float> row, const juce::String& title, const juce::String& value);

/** An engraved rule across a panel between its sections: a dark groove with a light edge under it. */
void drawSectionRule (juce::Graphics&, const DeskSkin&, float x0, float x1, float y);

} // namespace whitman
