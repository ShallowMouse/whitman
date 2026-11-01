#include "DeskPanels.h"

namespace whitman
{

static constexpr int capH = 12, rowKnob = 30, smallKnob = 22;

void drawRowCaption (juce::Graphics& g, const DeskSkin& skin, juce::Rectangle<float> row, const juce::String& title, const juce::String& value)
{
    const bool light = skin.panelTop.getPerceivedBrightness() > 0.6f;
    g.setFont (Pequod::heading (9.5f));
    g.setColour (skin.engraving);
    g.drawText (title, row, juce::Justification::centredLeft, false);
    if (value.isNotEmpty())
    {
        g.setFont (Pequod::heading (9.5f, false));
        g.setColour (light ? skin.engraving : skin.accent);
        g.drawText (value, row, juce::Justification::centredRight, false);
    }
}

static juce::String gainText (double v)
{
    const double r = std::round (v * 2.0) / 2.0;
    if (std::abs (r) < 0.25) return "0";
    return (r > 0 ? "+" : "") + juce::String (r, std::fmod (std::abs (r), 1.0) > 0.25 ? 1 : 0);
}

static void styleButton (juce::TextButton& b, const DeskSkin& s)
{
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff222324));
    b.setColour (juce::TextButton::buttonOnColourId, s.accent);
    b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8d6ce));
    b.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111111));
}

//==============================================================================
EqSlider::EqSlider() : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
{
    setScrollWheelEnabled (false);
    setWantsKeyboardFocus (false);
    setDoubleClickReturnValue (true, 0.0);
    setMouseDragSensitivity (250);
}

void EqSlider::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float capHt = 12.0f, top = r.getY() + capHt * 0.5f, bottom = r.getBottom() - capHt * 0.5f;
    const float x = r.getCentreX();
    // the slot, with a centre detent mark
    g.setColour (juce::Colour (0xff0e0e0e));
    g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, bottom - top + 4.0f).withCentre ({ x, (top + bottom) * 0.5f }), 1.5f);
    g.setColour (skin != nullptr ? skin->engravingDim : juce::Colours::grey);
    const float mid = (top + bottom) * 0.5f;
    g.drawLine (r.getX() + 1.0f, mid, x - 4.0f, mid, 1.0f);
    g.drawLine (x + 4.0f, mid, r.getRight() - 1.0f, mid, 1.0f);
    // the cap
    const float y = bottom - (bottom - top) * (float) valueToProportionOfLength (getValue());
    auto cap = juce::Rectangle<float> (juce::jmin (r.getWidth() - 2.0f, 14.0f), capHt).withCentre ({ x, y });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e4d8), cap.getX(), cap.getY(), juce::Colour (0xffa9a498), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRect (cap.getX() + 2.0f, y - 0.75f, cap.getWidth() - 4.0f, 1.5f);
}

//==============================================================================
BandEqPanel::BandEqPanel (MixEngine& e, int i) : engine (e), index (i)
{
    for (int b = 0; b < 4; ++b)
    {
        auto bi = (size_t) b;
        gain[bi].onValueChange = [this, b] { if (auto* p = params()) p->gain[(size_t) b] = (float) gain[(size_t) b].getValue(); slider[(size_t) b].setValue (gain[(size_t) b].getValue(), juce::dontSendNotification); changed(); };
        slider[bi].onValueChange = [this, b] { if (auto* p = params()) p->gain[(size_t) b] = (float) slider[(size_t) b].getValue(); gain[(size_t) b].setValue (slider[(size_t) b].getValue(), juce::dontSendNotification); changed(); };
        freq[bi].onValueChange = [this, b] { if (auto* p = params()) p->freq[(size_t) b] = (float) freq[(size_t) b].getValue(); changed(); };
        q[bi].onValueChange = [this, b] { if (auto* p = params()) p->q[(size_t) b] = (float) q[(size_t) b].getValue(); changed(); };
        q[bi].setRange (0.0, 1.0, 0.001);
        q[bi].setDoubleClickReturnValue (true, 0.5);
        alt[bi].setClickingTogglesState (true);
        alt[bi].setWantsKeyboardFocus (false);
        alt[bi].onClick = [this, b] { if (auto* p = params()) p->alt[(size_t) b] = alt[(size_t) b].getToggleState(); changed(); };
        for (juce::Component* c : { (juce::Component*) &gain[bi], (juce::Component*) &freq[bi], (juce::Component*) &q[bi], (juce::Component*) &slider[bi], (juce::Component*) &alt[bi] })
            addChildComponent (c);
    }
    hpf.onValueChange = [this] { if (auto* p = params()) p->hpfPos = (float) hpf.getValue(); changed(); };
    lpf.onValueChange = [this] { if (auto* p = params()) p->lpfPos = (float) lpf.getValue(); changed(); };
    hpf.setTooltip ("High-pass filter (all the way left is off)");
    lpf.setTooltip ("Low-pass filter (all the way left is off)");
    addChildComponent (hpf);
    addChildComponent (lpf);
    for (int k = 0; k < 3; ++k)
    {
        for (auto* arr : { &hpfButton, &lpfButton })
        {
            auto& b = (*arr)[(size_t) k];
            b.setClickingTogglesState (true);
            b.setWantsKeyboardFocus (false);
            addChildComponent (b);
        }
        hpfButton[(size_t) k].onClick = [this, k] { if (auto* p = params()) p->hpfMask = (p->hpfMask.load() & ~(1 << k)) | (hpfButton[(size_t) k].getToggleState() ? 1 << k : 0); changed(); };
        lpfButton[(size_t) k].onClick = [this, k] { if (auto* p = params()) p->lpfMask = (p->lpfMask.load() & ~(1 << k)) | (lpfButton[(size_t) k].getToggleState() ? 1 << k : 0); changed(); };
    }
    setInterceptsMouseClicks (false, true);
}

BandEqParams* BandEqPanel::params() const
{
    return def != nullptr ? engine.strip (index).eqFor (desk) : nullptr;
}

void BandEqPanel::setDesk (Desk d)
{
    desk = d;
    def = bandEqOf (d);
    skin = &DeskSkin::of (d);
    setVisible (def != nullptr);
    if (def == nullptr) return;

    const int nb = (int) def->bands.size();
    // SSL colours its EQ knobs by band; the others use the desk's own
    static const juce::Colour sslCaps[4] = { juce::Colour (0xffc8352b), juce::Colour (0xff3f8f4a), juce::Colour (0xff3566a8), juce::Colour (0xff2a2a2a) };
    for (int b = 0; b < 4; ++b)
    {
        const auto bi = (size_t) b;
        const bool has = b < nb;
        const auto* band = has ? &def->bands[bi] : nullptr;
        gain[bi].setVisible (has && ! def->sliders);
        slider[bi].setVisible (has && def->sliders);
        freq[bi].setVisible (has);
        q[bi].setVisible (has && band->qKnob);
        alt[bi].setVisible (has && band->alt != BandDef::Alt::none);
        if (! has) continue;
        const float range = juce::jmax (band->maxDb, band->altMaxDb);
        for (juce::Slider* s : { (juce::Slider*) &gain[bi], (juce::Slider*) &slider[bi] })
        {
            s->setRange (-range, range, 0.1);
            s->setDoubleClickReturnValue (true, 0.0);
            s->setTooltip (juce::String (band->label) + ": boost / cut (double-click for flat)");
        }
        if (band->swept()) freq[bi].setRange (0.0, 1.0, 0.001);
        else               freq[bi].setRange (0.0, band->numSteps() - 1, 1.0);
        freq[bi].setDoubleClickReturnValue (true, band->defaultFreq());
        freq[bi].setTooltip (juce::String (band->label) + (band->swept() ? ": frequency" : ": frequency switch"));
        q[bi].setTooltip (juce::String (band->label) + ": Q (bandwidth) - narrower to the right, with more range");
        alt[bi].setButtonText (juce::String (band->altLabel).replace ("NARROW", "NARR"));
        alt[bi].setTooltip (band->alt == BandDef::Alt::q ? juce::String (band->label) + ": narrow the bandwidth"
                                                         : juce::String (band->label) + ": switch it to a " + juce::String (band->altLabel).toLowerCase());
        const auto cap = d == Desk::ssl4000e ? sslCaps[b] : juce::Colour();
        for (auto* k : { &gain[bi], &freq[bi], &q[bi] }) { k->setSkin (skin); k->cap = cap; }
        slider[bi].setSkin (skin);
        styleButton (alt[bi], *skin);
    }

    auto setupFilter = [&] (DeskKnob& k, std::array<juce::TextButton, 3>& buttons, const FilterDef& f)
    {
        k.setVisible (f.kind == FilterDef::Kind::switched || f.kind == FilterDef::Kind::swept);
        k.setSkin (skin);
        if (f.kind == FilterDef::Kind::switched) k.setRange (0.0, (double) f.steps.size(), 1.0);
        else                                     k.setRange (0.0, 1.0, 0.001);
        k.setDoubleClickReturnValue (true, 0.0);
        for (int i = 0; i < 3; ++i)
        {
            auto& b = buttons[(size_t) i];
            b.setVisible (f.kind == FilterDef::Kind::stacked && i < (int) f.steps.size());
            if (b.isVisible()) b.setButtonText (hzText (f.steps[(size_t) i]));
            styleButton (b, *skin);
        }
    };
    setupFilter (hpf, hpfButton, def->hpf);
    setupFilter (lpf, lpfButton, def->lpf);
    refresh();
    resized();
}

void BandEqPanel::refresh()
{
    auto* p = params();
    if (p == nullptr) return;
    for (int b = 0; b < 4; ++b)
    {
        const auto bi = (size_t) b;
        gain[bi].setValue (p->gain[bi].load(), juce::dontSendNotification);
        slider[bi].setValue (p->gain[bi].load(), juce::dontSendNotification);
        freq[bi].setValue (p->freq[bi].load(), juce::dontSendNotification);
        q[bi].setValue (p->q[bi].load(), juce::dontSendNotification);
        alt[bi].setToggleState (p->alt[bi].load(), juce::dontSendNotification);
    }
    hpf.setValue (p->hpfPos.load(), juce::dontSendNotification);
    lpf.setValue (p->lpfPos.load(), juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
    {
        hpfButton[(size_t) i].setToggleState ((p->hpfMask.load() >> i) & 1, juce::dontSendNotification);
        lpfButton[(size_t) i].setToggleState ((p->lpfMask.load() >> i) & 1, juce::dontSendNotification);
    }
    repaint();
}

juce::String BandEqPanel::hzText (float hz)
{
    if (hz >= 1000.0f) return juce::String (hz / 1000.0f, std::fmod (hz, 1000.0f) > 1.0f ? 1 : 0) + "k";
    return juce::String (juce::roundToInt (hz));
}

juce::String BandEqPanel::freqText (int b) const
{
    const auto& band = def->bands[(size_t) b];
    const float hz = band.hzAt ((float) freq[(size_t) b].getValue());
    // swept: two significant figures, as you'd read off the scale
    if (band.swept())
    {
        const float r = hz >= 1000.0f ? std::round (hz / 100.0f) * 100.0f : (hz >= 100.0f ? std::round (hz / 10.0f) * 10.0f : std::round (hz));
        return hzText (r);
    }
    return hzText (hz);
}

juce::String BandEqPanel::filterText (const FilterDef& f, float pos) const
{
    if (f.kind == FilterDef::Kind::switched)
    {
        const int k = juce::roundToInt (pos);
        return k <= 0 ? juce::String ("OFF") : hzText (f.steps[(size_t) k - 1]);
    }
    if (pos <= 0.001f) return "OFF";
    const float hz = f.sweepLo * std::pow (f.sweepHi / f.sweepLo, pos);
    return hzText (hz >= 1000.0f ? std::round (hz / 100.0f) * 100.0f : std::round (hz));
}

int BandEqPanel::preferredHeight() const
{
    if (def == nullptr) return 0;
    const int nb = (int) def->bands.size();
    if (def->sliders)
        return capH + 84 + 22 + 11 + 2 * 18 + 4;            // captions, sliders, frequency switches and their readings, filter buttons
    return capH + rowKnob + 2 + nb * (capH + rowKnob + 2);   // filters, then a row per band
}

void BandEqPanel::resized()
{
    if (def == nullptr) return;
    auto r = getLocalBounds();
    const int nb = (int) def->bands.size();
    if (def->sliders)
    {
        r.removeFromTop (capH);
        auto sl = r.removeFromTop (84);
        auto sw = r.removeFromTop (22);
        r.removeFromTop (11);                // the switches' readings
        const int w = r.getWidth() / nb;
        for (int b = 0; b < nb; ++b)
        {
            slider[(size_t) b].setBounds (sl.removeFromLeft (w).reduced (2, 0));
            freq[(size_t) b].setBounds (sw.removeFromLeft (w).withSizeKeepingCentre (20, 20));
        }
        r.removeFromTop (2);
        for (auto* arr : { &hpfButton, &lpfButton })
        {
            auto row = r.removeFromTop (16);
            row.removeFromLeft (24);         // room for "HPF" / "LPF"
            const int bw = row.getWidth() / 3;
            for (auto& b : *arr) b.setBounds (row.removeFromLeft (bw).reduced (1, 0));
            r.removeFromTop (2);
        }
        return;
    }

    // filters row
    r.removeFromTop (capH);
    {
        auto row = r.removeFromTop (rowKnob);
        auto half = row.removeFromLeft (row.getWidth() / 2);
        hpf.setBounds (half.withSizeKeepingCentre (smallKnob + 2, smallKnob + 2));
        lpf.setBounds (row.withSizeKeepingCentre (smallKnob + 2, smallKnob + 2));
        if (! lpf.isVisible()) hpf.setBounds (half.getUnion (row).withSizeKeepingCentre (smallKnob + 2, smallKnob + 2));
    }
    r.removeFromTop (2);
    for (int b = 0; b < nb; ++b)
    {
        const auto bi = (size_t) b;
        r.removeFromTop (capH);
        auto row = r.removeFromTop (rowKnob);
        gain[bi].setBounds (row.removeFromLeft (rowKnob + 2).withSizeKeepingCentre (rowKnob, rowKnob));
        row.removeFromLeft (2);
        freq[bi].setBounds (row.removeFromLeft (smallKnob + 2).withSizeKeepingCentre (smallKnob, smallKnob));
        row.removeFromLeft (2);
        if (q[bi].isVisible())   q[bi].setBounds (row.removeFromLeft (smallKnob + 2).withSizeKeepingCentre (smallKnob, smallKnob));
        if (alt[bi].isVisible()) alt[bi].setBounds (row.withSizeKeepingCentre (row.getWidth(), 15));
        r.removeFromTop (2);
    }
}

void BandEqPanel::paint (juce::Graphics& g)
{
    if (def == nullptr || skin == nullptr) return;
    auto* p = params();
    if (p == nullptr) return;
    const int nb = (int) def->bands.size();
    const float w = (float) getWidth();

    if (def->sliders)
    {
        // band names over the sliders, their frequencies under the switches
        const float cw = w / (float) nb;
        g.setFont (Pequod::heading (8.5f));
        for (int b = 0; b < nb; ++b)
        {
            g.setColour (skin->engraving);
            static const char* shortName[4] = { "TREB", "HI", "LO", "BASS" };
            g.drawText (shortName[b], juce::Rectangle<float> (cw * (float) b, 0.0f, cw, (float) capH), juce::Justification::centred, false);
            g.setColour (skin->accent);
            g.drawText (freqText (b), juce::Rectangle<float> (cw * (float) b, (float) capH + 84.0f + 22.0f, cw, 10.0f),
                        juce::Justification::centred, false);
        }
        g.setColour (skin->engraving);
        g.setFont (Pequod::heading (8.5f));
        g.drawText ("HP", juce::Rectangle<float> (0.0f, (float) hpfButton[0].getY(), 22.0f, 16.0f), juce::Justification::centredLeft, false);
        g.drawText ("LP", juce::Rectangle<float> (0.0f, (float) lpfButton[0].getY(), 22.0f, 16.0f), juce::Justification::centredLeft, false);
        return;
    }

    // filters
    {
        auto row = juce::Rectangle<float> (0.0f, 0.0f, w, (float) capH);
        juce::String t = juce::String (def->hpf.label) + " " + filterText (def->hpf, p->hpfPos.load());
        if (def->lpf.kind != FilterDef::Kind::none)
        {
            auto left = row.removeFromLeft (w * 0.5f);
            drawRowCaption (g, *skin, left, "HP " + filterText (def->hpf, p->hpfPos.load()), {});
            drawRowCaption (g, *skin, row, "LP " + filterText (def->lpf, p->lpfPos.load()), {});
        }
        else drawRowCaption (g, *skin, row, t, {});
    }
    float y = (float) capH + (float) rowKnob + 2.0f;
    for (int b = 0; b < nb; ++b)
    {
        const auto& band = def->bands[(size_t) b];
        juce::String title = juce::String (band.label) + " " + freqText (b);
        if (band.qKnob) title << " Q" << juce::String (band.qAt ((float) q[(size_t) b].getValue()), 1);
        drawRowCaption (g, *skin, juce::Rectangle<float> (0.0f, y, w, (float) capH), title, gainText (p->gain[(size_t) b].load()));
        y += (float) capH + (float) rowKnob + 2.0f;
    }
}

//==============================================================================
SslDynamicsPanel::SslDynamicsPanel (MixEngine& e, int i) : engine (e), index (i)
{
    ratio.setRange (1.0, 20.0, 0.1);
    ratio.setSkewFactorFromMidPoint (4.0);
    ratio.setDoubleClickReturnValue (true, 1.0);
    ratio.setTooltip ("RATIO: 1:1 (off) to infinity:1 (a limiter) at the end");
    threshold.setRange (-20.0, 10.0, 0.5);
    threshold.setDoubleClickReturnValue (true, 10.0);
    threshold.setTooltip ("Compressor THRESHOLD, +10 to -20 dBu");
    release.setRange (0.1, 4.0, 0.01);
    release.setSkewFactorFromMidPoint (0.5);
    release.setDoubleClickReturnValue (true, 0.3);
    release.setTooltip ("Compressor RELEASE, 0.1 to 4 s");
    range.setRange (0.0, 40.0, 0.5);
    range.setDoubleClickReturnValue (true, 0.0);
    range.setTooltip ("Expander / gate RANGE: how far it turns down, 0 (off) to 40 dB");
    gateThreshold.setRange (-30.0, 10.0, 0.5);
    gateThreshold.setDoubleClickReturnValue (true, -30.0);
    gateThreshold.setTooltip ("Expander / gate THRESHOLD, -30 to +10 dBu");
    gateRelease.setRange (0.1, 4.0, 0.01);
    gateRelease.setSkewFactorFromMidPoint (0.5);
    gateRelease.setDoubleClickReturnValue (true, 0.3);
    gateRelease.setTooltip ("Expander / gate RELEASE, 0.1 to 4 s");
    fast.setTooltip ("Compressor fast attack (1 ms); off, the attack follows the music");
    gateFast.setTooltip ("Expander / gate fast attack (1 ms)");
    gateMode.setTooltip ("EXP: an expander (1:2 below the threshold, down to the range). GATE: shuts by the whole range.");

    auto& s = engine.strip (index);
    ratio.onValueChange         = [this, &s] { s.sslRatio = (float) ratio.getValue(); changed(); };
    threshold.onValueChange     = [this, &s] { s.sslThresh = (float) threshold.getValue(); changed(); };
    release.onValueChange       = [this, &s] { s.sslRelease = (float) release.getValue(); changed(); };
    range.onValueChange         = [this, &s] { s.gateRange = (float) range.getValue(); changed(); };
    gateThreshold.onValueChange = [this, &s] { s.gateThresh = (float) gateThreshold.getValue(); changed(); };
    gateRelease.onValueChange   = [this, &s] { s.gateRelease = (float) gateRelease.getValue(); changed(); };
    for (auto* b : { &fast, &gateFast, &gateMode }) { b->setClickingTogglesState (true); b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
    fast.onClick     = [this, &s] { s.sslFast = fast.getToggleState(); changed(); };
    gateFast.onClick = [this, &s] { s.gateFast = gateFast.getToggleState(); changed(); };
    gateMode.onClick = [this, &s] { s.gateMode = gateMode.getToggleState(); gateMode.setButtonText (s.gateMode.load() ? "GATE" : "EXP"); changed(); };
    for (auto* k : { &ratio, &threshold, &release, &range, &gateThreshold, &gateRelease }) addAndMakeVisible (k);
    setInterceptsMouseClicks (false, true);
}

void SslDynamicsPanel::setSkin (const DeskSkin* s)
{
    skin = s;
    // SSL dynamics knobs: grey caps for the compressor, green-grey for the gate
    for (auto* k : { &ratio, &threshold, &release }) { k->setSkin (s); k->cap = juce::Colour (0xff8a8d90); }
    for (auto* k : { &range, &gateThreshold, &gateRelease }) { k->setSkin (s); k->cap = juce::Colour (0xff5f7f63); }
    for (auto* b : { &fast, &gateFast, &gateMode }) styleButton (*b, *s);
    repaint();
}

void SslDynamicsPanel::refresh()
{
    auto& s = engine.strip (index);
    ratio.setValue (s.sslRatio.load(), juce::dontSendNotification);
    threshold.setValue (s.sslThresh.load(), juce::dontSendNotification);
    release.setValue (s.sslRelease.load(), juce::dontSendNotification);
    range.setValue (s.gateRange.load(), juce::dontSendNotification);
    gateThreshold.setValue (s.gateThresh.load(), juce::dontSendNotification);
    gateRelease.setValue (s.gateRelease.load(), juce::dontSendNotification);
    fast.setToggleState (s.sslFast.load(), juce::dontSendNotification);
    gateFast.setToggleState (s.gateFast.load(), juce::dontSendNotification);
    gateMode.setToggleState (s.gateMode.load(), juce::dontSendNotification);
    gateMode.setButtonText (s.gateMode.load() ? "GATE" : "EXP");
    repaint();
}

void SslDynamicsPanel::updateMeters()
{
    auto& s = engine.strip (index);
    const float gr = s.gainReduction.load(), gt = s.gateReduction.load();
    if (std::abs (gr - shownGr) > 0.4f || std::abs (gt - shownGate) > 0.4f || (gr < 0.05f) != (shownGr < 0.05f) || (gt < 0.05f) != (shownGate < 0.05f))
    {
        shownGr = gr; shownGate = gt;
        repaint();
    }
}

void SslDynamicsPanel::resized()
{
    auto r = getLocalBounds();
    auto layoutRow = [&] (DeskKnob& big, DeskKnob& a, DeskKnob& b, juce::TextButton& btn1, juce::TextButton* btn2)
    {
        r.removeFromTop (capH);
        auto row = r.removeFromTop (rowKnob);
        big.setBounds (row.removeFromLeft (rowKnob + 2).withSizeKeepingCentre (rowKnob, rowKnob));
        a.setBounds (row.removeFromLeft (smallKnob + 2).withSizeKeepingCentre (smallKnob, smallKnob));
        b.setBounds (row.removeFromLeft (smallKnob + 2).withSizeKeepingCentre (smallKnob, smallKnob));
        auto side = row.reduced (1, 0);
        if (btn2 != nullptr)
        {
            btn1.setBounds (side.removeFromTop (side.getHeight() / 2).reduced (0, 1));
            btn2->setBounds (side.reduced (0, 1));
        }
        else btn1.setBounds (side.withSizeKeepingCentre (side.getWidth(), 15));
        r.removeFromTop (2);
    };
    layoutRow (ratio, threshold, release, fast, nullptr);
    layoutRow (range, gateThreshold, gateRelease, gateMode, &gateFast);
}

void SslDynamicsPanel::paint (juce::Graphics& g)
{
    if (skin == nullptr) return;
    const float w = (float) getWidth();
    const float r = (float) ratio.getValue();
    const auto ratioText = r <= 1.01f ? juce::String ("OFF") : r >= 19.99f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e:1")) : juce::String (r, r < 10.0f ? 1 : 0) + ":1";
    const auto grText = shownGr >= 0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (shownGr, 1) : juce::String();
    drawRowCaption (g, *skin, juce::Rectangle<float> (0.0f, 0.0f, w, (float) capH), "COMP " + ratioText + " " + juce::String (threshold.getValue(), 0), grText);
    const auto gateText = range.getValue() < 0.05 ? juce::String ("OFF") : juce::String (juce::roundToInt (range.getValue())) + "dB " + juce::String (gateThreshold.getValue(), 0);
    const auto gtText = shownGate >= 0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (shownGate, 1) : juce::String();
    drawRowCaption (g, *skin, juce::Rectangle<float> (0.0f, (float) (capH + rowKnob + 2), w, (float) capH), (gateMode.getToggleState() ? "GATE " : "EXP ") + gateText, gtText);
}

} // namespace whitman
