#include "Console.h"

namespace whitman
{

//==============================================================================
float Meter::dbToPos (float db) noexcept
{
    // more room near the top, where mixing happens: -60..-20 gets the bottom fifth
    db = juce::jlimit (-60.0f, 6.0f, db);
    if (db < -20.0f) return (db + 60.0f) / 40.0f * 0.2f;
    return 0.2f + (db + 20.0f) / 26.0f * 0.8f;
}

void Meter::push (float p0, float p1)
{
    const float in[2] = { p0, p1 };
    bool changed = false;
    for (int c = 0; c < numChannels; ++c)
    {
        const float db = in[c] > 0.0f ? juce::Decibels::gainToDecibels (in[c]) : -100.0f;
        const float fallen = level[(size_t) c] - 1.2f;                 // ~36 dB/s at 30 fps
        const float nl = juce::jmax (db, fallen);
        if (std::abs (nl - level[(size_t) c]) > 0.05f) changed = true;
        level[(size_t) c] = nl;
        if (db >= hold[(size_t) c]) { hold[(size_t) c] = db; holdFrames[(size_t) c] = 45; changed = true; }
        else if (--holdFrames[(size_t) c] <= 0 && hold[(size_t) c] > -100.0f) { hold[(size_t) c] = juce::jmax (-100.0f, hold[(size_t) c] - 1.5f); changed = true; }
    }
    if (changed) repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Pequod::Colours::rail);
    g.fillRoundedRectangle (r, 2.0f);
    r.reduce (2.0f, 2.0f);
    const float gap = 2.0f;
    const float w = (r.getWidth() - gap * (float) (numChannels - 1)) / (float) numChannels;

    for (int c = 0; c < numChannels; ++c)
    {
        auto col = r.withX (r.getX() + (float) c * (w + gap)).withWidth (w);
        const float top = col.getBottom() - col.getHeight() * dbToPos (level[(size_t) c]);
        if (level[(size_t) c] > -60.0f)
        {
            // green-ish ink below -6, accent to 0, red over
            auto fill = col.withTop (top);
            const float y6 = col.getBottom() - col.getHeight() * dbToPos (-6.0f);
            const float y0 = col.getBottom() - col.getHeight() * dbToPos (0.0f);
            g.setColour (Pequod::Colours::mustardLo.brighter (0.25f));
            g.fillRect (fill.withTop (juce::jmax (fill.getY(), y6)));
            if (fill.getY() < y6) { g.setColour (Pequod::Colours::mustard); g.fillRect (fill.withTop (juce::jmax (fill.getY(), y0)).withBottom (y6)); }
            if (fill.getY() < y0) { g.setColour (Pequod::Colours::danger); g.fillRect (fill.withBottom (y0)); }
        }
        if (hold[(size_t) c] > -60.0f)
        {
            const float hy = col.getBottom() - col.getHeight() * dbToPos (hold[(size_t) c]);
            g.setColour (hold[(size_t) c] > 0.0f ? Pequod::Colours::danger : Pequod::Colours::text);
            g.fillRect (col.getX(), hy - 1.0f, col.getWidth(), 1.5f);
        }
    }
}

//==============================================================================
Fader::Fader() : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
{
    setRange (faderMinDb, faderMaxDb, 0.1);
    setSkewFactorFromMidPoint (-14.0);
    setDoubleClickReturnValue (true, 0.0);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (400);
    setWantsKeyboardFocus (false);
    setPopupDisplayEnabled (false, false, nullptr);
}

juce::String Fader::getTextFromValue (double v)
{
    if (v <= faderMinDb + 0.05) return "-inf";
    return (v > 0.05 ? "+" : "") + juce::String (v, 1);
}

double Fader::getValueFromText (const juce::String& t)
{
    if (t.containsIgnoreCase ("inf")) return faderMinDb;
    return t.retainCharacters ("-+.0123456789").getDoubleValue();
}

void Fader::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float capH = 34.0f, inset = capH * 0.5f;
    const float top = r.getY() + inset, bottom = r.getBottom() - inset;
    auto yOf = [&] (double db) { return (float) (bottom - (bottom - top) * valueToProportionOfLength (db)); };

    // scale
    g.setFont (Pequod::body (9.5f));
    for (double db : { 10.0, 5.0, 0.0, -5.0, -10.0, -20.0, -30.0, -40.0, -60.0 })
    {
        const float y = yOf (db);
        const bool unity = db == 0.0;
        g.setColour (unity ? Pequod::Colours::mustard : Pequod::Colours::textDim.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getX() + (unity ? 10.0f : 6.0f));
        g.drawText (db <= faderMinDb ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : (db > 0.0 ? "+" : "") + juce::String ((int) std::abs (db)),
                    juce::Rectangle<float> (r.getX() + 10.0f, y - 6.0f, 20.0f, 12.0f), juce::Justification::centredLeft, false);
    }

    // slot
    auto slot = juce::Rectangle<float> (5.0f, bottom - top + 8.0f).withCentre ({ r.getCentreX() + 8.0f, (top + bottom) * 0.5f });
    g.setColour (Pequod::Colours::rail);
    g.fillRoundedRectangle (slot, 2.0f);

    // cap
    const float y = yOf (getValue());
    auto cap = juce::Rectangle<float> (28.0f, capH).withCentre ({ slot.getCentreX(), y });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d52), cap.getX(), cap.getY(),
                                             juce::Colour (0xff222428), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (Pequod::Colours::rail);
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    for (float dy : { -9.0f, -5.0f, 5.0f, 9.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawHorizontalLine ((int) (y + dy), cap.getX() + 4.0f, cap.getRight() - 4.0f);
    }
    g.setColour (getValue() <= faderMinDb + 0.05 ? Pequod::Colours::textDim : Pequod::Colours::mustard);
    g.fillRect (cap.getX() + 3.0f, y - 1.0f, cap.getWidth() - 6.0f, 2.0f);
}

//==============================================================================
ChannelStrip::ChannelStrip (MixEngine& e, int i, juce::StringArray& n) : engine (e), index (i), names (n)
{
    nameLabel.setEditable (false, true, false);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (Pequod::heading (13.0f));
    nameLabel.setColour (juce::Label::backgroundColourId, Pequod::Colours::well);
    nameLabel.setColour (juce::Label::textWhenEditingColourId, Pequod::Colours::text);
    nameLabel.setTooltip ("Double-click to name this track");
    nameLabel.onTextChange = [this]
    {
        auto t = nameLabel.getText().trim().substring (0, 24);
        if (t.isEmpty()) t = juce::String (index + 1);
        nameLabel.setText (t, juce::dontSendNotification);
        names.set (index, t);
        if (onChanged) onChanged();
    };
    addAndMakeVisible (nameLabel);

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setWantsKeyboardFocus (false);
    pan.setTooltip ("Pan (double-click to centre)");
    pan.onValueChange = [this]
    {
        engine.strip (index).pan = (float) pan.getValue();
        repaint();
        if (onChanged) onChanged();
    };
    addAndMakeVisible (pan);

    mute.setClickingTogglesState (true);
    mute.setColour (juce::TextButton::buttonOnColourId, Pequod::Colours::danger);
    mute.setTooltip ("Mute");
    mute.onClick = [this] { engine.strip (index).mute = mute.getToggleState(); if (onChanged) onChanged(); };
    solo.setClickingTogglesState (true);
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0c060));
    solo.setTooltip ("Solo (more than one can be soloed)");
    solo.onClick = [this] { engine.strip (index).solo = solo.getToggleState(); if (onChanged) onChanged(); };
    for (auto* b : { &mute, &solo }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }

    fader.setTooltip ("Level (double-click for 0 dB)");
    fader.onValueChange = [this]
    {
        engine.strip (index).gainDb = (float) fader.getValue();
        dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
        if (onChanged) onChanged();
    };
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (Pequod::heading (12.0f, false));
    dbLabel.setColour (juce::Label::textColourId, Pequod::Colours::textDim);
    addAndMakeVisible (dbLabel);

    refreshFromEngine();
}

void ChannelStrip::refreshFromEngine()
{
    auto& s = engine.strip (index);
    nameLabel.setText (names[index].isEmpty() ? juce::String (index + 1) : names[index], juce::dontSendNotification);
    pan.setValue (s.pan.load(), juce::dontSendNotification);
    mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    solo.setToggleState (s.solo.load(), juce::dontSendNotification);
    fader.setValue (s.gainDb.load(), juce::dontSendNotification);
    dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
    repaint();
}

void ChannelStrip::updateMeter()
{
    meter.push (MixEngine::takePeak (engine.strip (index).peak));
    // dim the strip when it can't be heard (muted, or someone else is soloed)
    auto& s = engine.strip (index);
    const bool d = s.mute.load() || (engine.anySolo() && ! s.solo.load());
    if (d != dimmed) { dimmed = d; repaint(); }
    // keep the buttons in step if something else changed them
    if (mute.getToggleState() != s.mute.load()) mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    if (solo.getToggleState() != s.solo.load()) solo.setToggleState (s.solo.load(), juce::dontSendNotification);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    g.setGradientFill (juce::ColourGradient (Pequod::Colours::panelHi, r.getX(), r.getY(), Pequod::Colours::panel, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (dimmed ? Pequod::Colours::line : Pequod::Colours::mustard);
    g.fillRect (r.withHeight (3.0f));
    g.setColour (Pequod::Colours::line);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    // track number
    g.setFont (Pequod::heading (11.0f));
    g.setColour (Pequod::Colours::textDim);
    g.drawText ("TRK " + juce::String (index + 1), r.withTop (6.0f).withHeight (14.0f), juce::Justification::centred);

    // pan readout under the knob
    const float p = (float) pan.getValue();
    const auto txt = std::abs (p) < 0.005f ? juce::String ("C") : (p < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (p) * 100.0f));
    g.setFont (Pequod::heading (10.5f, false));
    g.drawText (txt, pan.getBounds().translated (0, pan.getHeight() - 4).withHeight (14), juce::Justification::centred);

    if (dimmed)
    {
        g.setColour (Pequod::Colours::bg.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 3.0f);
    }
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 0);
    r.removeFromTop (22);
    nameLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    pan.setBounds (r.removeFromTop (44).withSizeKeepingCentre (44, 44));
    r.removeFromTop (16);
    auto ms = r.removeFromTop (22);
    mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2 - 1));
    ms.removeFromLeft (2);
    solo.setBounds (ms);
    r.removeFromTop (8);
    dbLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    meter.setBounds (r.removeFromRight (10).reduced (0, 17));
    r.removeFromRight (2);
    fader.setBounds (r);
}

//==============================================================================
MasterStrip::MasterStrip (MixEngine& e) : engine (e)
{
    fader.setTooltip ("Master level (double-click for 0 dB)");
    fader.onValueChange = [this]
    {
        engine.masterDb = (float) fader.getValue();
        dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
        if (onChanged) onChanged();
    };
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);
    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (Pequod::heading (13.0f));
    addAndMakeVisible (dbLabel);
    refreshFromEngine();
}

void MasterStrip::refreshFromEngine()
{
    fader.setValue (engine.masterDb.load(), juce::dontSendNotification);
    dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
}

void MasterStrip::updateMeter()
{
    meter.push (MixEngine::takePeak (engine.masterPeakL), MixEngine::takePeak (engine.masterPeakR));
    const bool c = engine.masterClipped.load();
    if (c != clipped) { clipped = c; repaint (lampArea); }
}

void MasterStrip::mouseDown (const juce::MouseEvent& e)
{
    if (lampArea.contains (e.getPosition())) { engine.masterClipped = false; clipped = false; repaint (lampArea); }
}

void MasterStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    g.setGradientFill (juce::ColourGradient (Pequod::Colours::panelHi, r.getX(), r.getY(), Pequod::Colours::panel, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (Pequod::Colours::mustard);
    g.fillRect (r.withHeight (3.0f));
    g.setColour (Pequod::Colours::line);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    g.setFont (Pequod::heading (15.0f));
    g.setColour (Pequod::Colours::mustard);
    g.drawText ("MASTER", r.withTop (8.0f).withHeight (18.0f), juce::Justification::centred);
    g.setFont (Pequod::body (10.5f));
    g.setColour (Pequod::Colours::textDim);
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x92 HAWTHORNE 2-TRACK"), r.withTop (28.0f).withHeight (14.0f), juce::Justification::centred);

    // clip lamp
    auto lamp = lampArea.toFloat();
    g.setColour (clipped ? Pequod::Colours::danger : Pequod::Colours::rail);
    g.fillRoundedRectangle (lamp, 3.0f);
    g.setFont (Pequod::heading (11.0f));
    g.setColour (clipped ? Pequod::Colours::text : Pequod::Colours::textDim);
    g.drawText ("CLIP", lamp, juce::Justification::centred);
}

void MasterStrip::resized()
{
    auto r = getLocalBounds().reduced (8, 0);
    r.removeFromTop (50);
    lampArea = r.removeFromTop (20).withSizeKeepingCentre (54, 20);
    r.removeFromTop (16);
    dbLabel.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (4);
    meter.setBounds (r.removeFromRight (26).reduced (0, 17));
    r.removeFromRight (4);
    fader.setBounds (r);
}

} // namespace whitman
