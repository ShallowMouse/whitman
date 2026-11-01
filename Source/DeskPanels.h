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
    int preferredHeight() const { return 2 * 44; }
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
    void changed() { repaint(); if (onChanged) onChanged(); }
};

/** A row's caption: the title on the left, its value on the right. */
void drawRowCaption (juce::Graphics&, const DeskSkin&, juce::Rectangle<float> row, const juce::String& title, const juce::String& value);

} // namespace whitman
