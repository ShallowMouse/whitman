#include "Console.h"

namespace whitman
{

//==============================================================================
const DeskSkin& DeskSkin::of (Desk d)
{
    using C = juce::Colour;
    static const DeskSkin skins[numDesks] = {
        // WHITMAN: PEQUOD charcoal, ink blue
        { Pequod::Colours::bg, Pequod::Colours::panelHi, Pequod::Colours::panel, Pequod::Colours::line,
          Pequod::Colours::text, Pequod::Colours::textDim, Pequod::Colours::mustard, Pequod::Colours::well,
          false, false, false, 76, 132 },
        // REDD.51: EMI grey-green painted steel, cream engraving, black bakelite knobs, Painton quadrant faders, VUs
        { C (0xff252825), C (0xff737a73), C (0xff5f665f), C (0xff383d39),
          C (0xfff2eee0), C (0xffd2cfc0), C (0xfff0dfb0), C (0xff1b1d1b),
          true, true, true, 94, 236 },
    };
    return skins[juce::jlimit (0, numDesks - 1, (int) d)];
}

static void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const DeskSkin& s, bool lit)
{
    g.setGradientFill (juce::ColourGradient (s.panelTop, r.getX(), r.getY(), s.panelBottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    if (s.bakeliteKnobs)
    {
        // painted steel: a faint hammertone speckle, and screws top and bottom
        juce::Random rnd ((juce::int64) (r.getX() * 31 + r.getWidth()));
        for (int i = 0; i < (int) (r.getWidth() * r.getHeight() / 90.0f); ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.05f + rnd.nextFloat() * 0.05f));
            g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.5f, 1.5f);
        }
        for (float y : { r.getY() + 8.0f, r.getBottom() - 8.0f })
            Pequod::drawScrew (g, { r.getCentreX(), y }, 3.0f);
    }
    else
    {
        g.setColour (lit ? s.accent : s.edge);
        g.fillRect (r.withHeight (3.0f));
    }
    g.setColour (s.edge);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
}

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
            auto fill = col.withTop (top);
            const float y6 = col.getBottom() - col.getHeight() * dbToPos (-6.0f);
            const float y0 = col.getBottom() - col.getHeight() * dbToPos (0.0f);
            g.setColour (lowColour);
            g.fillRect (fill.withTop (juce::jmax (fill.getY(), y6)));
            if (fill.getY() < y6) { g.setColour (midColour); g.fillRect (fill.withTop (juce::jmax (fill.getY(), y0)).withBottom (y6)); }
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
void VUMeter::push (float peak)
{
    // a VU reads the average: a sine's is ~3 dB under its peak. 0 VU = -18 dBFS.
    const float vu = peak > 0.0f ? juce::Decibels::gainToDecibels (peak * 0.707f) - nominalDbfs : -40.0f;
    const float target = juce::jlimit (-22.0f, 4.0f, vu);
    const float next = needle + (target - needle) * 0.32f;     // ~300 ms to settle, like a real VU
    if (std::abs (next - needle) > 0.02f) { needle = next; repaint(); }
}

void VUMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // bezel and cream face
    g.setColour (juce::Colour (0xff111211));
    g.fillRoundedRectangle (r, 4.0f);
    auto face = r.reduced (4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6ecd0), face.getCentreX(), face.getY(),
                                             juce::Colour (0xffe2d4ad), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 2.5f);

    // the scale: an arc, -20 at the left, +3 at the right
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.06f);
    const float radius = face.getHeight() * 0.64f;
    const float span = 0.62f;    // radians either side of vertical
    auto angleOf = [&] (float vu)
    {
        // VU scales are linear in voltage, not dB
        const float v  = std::pow (10.0f, juce::jlimit (-22.0f, 4.0f, vu) / 20.0f);
        const float v0 = std::pow (10.0f, -20.0f / 20.0f), v1 = std::pow (10.0f, 3.0f / 20.0f);
        return -span + 2.0f * span * (v - v0) / (v1 - v0);
    };
    auto onArc = [&] (float a, float rad) { return pivot.translated (rad * std::sin (a), -rad * std::cos (a)); };

    juce::Path arc, red;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleOf (-20.0f), angleOf (0.0f), true);
    red.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleOf (0.0f), angleOf (3.0f), true);
    g.setColour (juce::Colour (0xff1b1a16));
    g.strokePath (arc, juce::PathStrokeType (1.4f));
    g.setColour (juce::Colour (0xffb8261b));
    g.strokePath (red, juce::PathStrokeType (3.5f));

    g.setFont (Pequod::heading (9.0f));
    for (float vu : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angleOf (vu);
        g.setColour (vu > 0.0f ? juce::Colour (0xffb8261b) : juce::Colour (0xff1b1a16));
        g.drawLine (juce::Line<float> (onArc (a, radius), onArc (a, radius + 5.0f)), 1.2f);
        if (vu == -20.0f || vu == -10.0f || vu == -5.0f || vu == -3.0f || vu == 0.0f || vu == 3.0f)
        {
            const auto p = onArc (a, radius + 12.0f);
            g.drawText (juce::String ((int) std::abs (vu)), juce::Rectangle<float> (18.0f, 10.0f).withCentre (p), juce::Justification::centred);
        }
    }
    g.setColour (juce::Colour (0xff1b1a16));
    g.setFont (Pequod::heading (12.0f));
    g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (14.0f), juce::Justification::centred);
    g.setFont (Pequod::heading (8.5f, false));
    g.drawText (caption, face.withTrimmedTop (face.getHeight() * 0.52f + 14.0f).withHeight (11.0f), juce::Justification::centred);

    // needle
    const float a = angleOf (needle);
    g.setColour (juce::Colour (0xff141414));
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (face.toNearestInt());
        g.drawLine (juce::Line<float> (onArc (a, radius * 0.25f), onArc (a, radius + 8.0f)), 1.6f);
    }
    // the needle's pivot cover
    g.setColour (juce::Colour (0xff111211));
    g.fillRect (face.withTop (face.getBottom() - 10.0f));
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
    if (skin != nullptr && skin->quadrantFaders) paintQuadrant (g);
    else                                         paintStraight (g);
}

void Fader::paintStraight (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float capH = 34.0f, inset = capH * 0.5f;
    const float top = r.getY() + inset, bottom = r.getBottom() - inset;
    auto yOf = [&] (double db) { return (float) (bottom - (bottom - top) * valueToProportionOfLength (db)); };

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

    auto slot = juce::Rectangle<float> (5.0f, bottom - top + 8.0f).withCentre ({ r.getCentreX() + 8.0f, (top + bottom) * 0.5f });
    g.setColour (Pequod::Colours::rail);
    g.fillRoundedRectangle (slot, 2.0f);

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

void Fader::paintQuadrant (juce::Graphics& g)
{
    // A Painton quadrant fader: a lever on a pivot behind the panel, its knob travelling
    // along a curved slot in a chrome plate. Up is louder.
    const auto& s = *skin;
    auto r = getLocalBounds().toFloat();
    const float knobH = 26.0f, inset = knobH * 0.5f + 4.0f;
    const float top = r.getY() + inset, bottom = r.getBottom() - inset;
    const float len = bottom - top;
    const float radius = len * 3.2f;
    const float half = len * 0.5f + 6.0f;
    const float xEnds = r.getRight() - 12.0f;            // the slot's ends; its middle bows to the left
    const juce::Point<float> pivot (xEnds + std::sqrt (radius * radius - half * half), (top + bottom) * 0.5f);
    auto pointAt = [&] (float y)
    {
        const float dy = y - pivot.y;
        return juce::Point<float> (pivot.x - std::sqrt (juce::jmax (0.0f, radius * radius - dy * dy)), y);
    };
    auto yOf = [&] (double db) { return (float) (bottom - len * valueToProportionOfLength (db)); };

    // chrome plate with the curved slot
    juce::Path slot;
    for (int i = 0; i <= 40; ++i)
    {
        const auto p = pointAt (top - 6.0f + (len + 12.0f) * (float) i / 40.0f);
        if (i == 0) slot.startNewSubPath (p); else slot.lineTo (p);
    }
    g.setColour (juce::Colour (0xffb9bcb7));
    g.strokePath (slot, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff8e918c));
    g.strokePath (slot, juce::PathStrokeType (15.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff0d0e0d));
    g.strokePath (slot, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // engraved scale, left of the slot
    g.setFont (Pequod::heading (9.5f, false));
    for (double db : { 10.0, 5.0, 0.0, -5.0, -10.0, -20.0, -30.0, -40.0, -60.0 })
    {
        const float y = yOf (db);
        const auto p = pointAt (y);
        const bool unity = db == 0.0;
        g.setColour (unity ? s.accent : s.engravingDim);
        g.drawLine (p.x - 16.0f, y, p.x - 10.0f, y, unity ? 1.6f : 1.0f);
        g.drawText (db <= faderMinDb ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : (db > 0.0 ? "+" : "") + juce::String ((int) std::abs (db)),
                    juce::Rectangle<float> (p.x - 40.0f, y - 6.0f, 23.0f, 12.0f), juce::Justification::centredRight, false);
    }

    // the lever's knob, tilted along the radius
    const float y = yOf (getValue());
    const auto p = pointAt (y);
    const float tilt = std::asin (juce::jlimit (-1.0f, 1.0f, (y - pivot.y) / radius));
    const auto t = juce::AffineTransform::rotation (tilt).translated (p);
    juce::Path stem;
    stem.addRectangle (-2.0f, -3.0f, 18.0f, 6.0f);
    g.setColour (juce::Colour (0xffcfd2cd));
    g.fillPath (stem, t);
    juce::Path knob;
    knob.addRoundedRectangle (-26.0f, -knobH * 0.5f, 30.0f, knobH, 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), p.x, p.y - knobH * 0.5f,
                                             juce::Colour (0xff0b0b0b), p.x, p.y + knobH * 0.5f, false));
    g.fillPath (knob, t);
    juce::Path line;
    line.addRectangle (-24.0f, -1.0f, 26.0f, 2.0f);
    g.setColour (getValue() <= faderMinDb + 0.05 ? juce::Colour (0xff6a6a6a) : juce::Colour (0xfff2eee0));
    g.fillPath (line, t);
}

//==============================================================================
DeskKnob::DeskKnob() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
{
    setWantsKeyboardFocus (false);
    setMouseDragSensitivity (180);
}

void DeskKnob::paint (juce::Graphics& g)
{
    if (skin == nullptr || ! skin->bakeliteKnobs)
    {
        juce::Slider::paint (g);
        return;
    }

    const auto rp = getRotaryParameters();
    auto r = getLocalBounds().toFloat();
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const float knobR = size * 0.36f;
    const float pos = (float) valueToProportionOfLength (getValue());
    const float angle = rp.startAngleRadians + pos * (rp.endAngleRadians - rp.startAngleRadians);

    // a dot for each switch position (or the ends and the middle for a smooth control)
    const double interval = getInterval();
    const int steps = interval > 0.0 ? (int) std::round ((getMaximum() - getMinimum()) / interval) : 0;
    g.setColour (skin->engraving);
    auto dot = [&] (float p, float rad)
    {
        const float a = rp.startAngleRadians + p * (rp.endAngleRadians - rp.startAngleRadians);
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c.translated (std::sin (a) * size * 0.47f, -std::cos (a) * size * 0.47f)));
    };
    if (steps > 0 && steps <= 30)
        for (int i = 0; i <= steps; ++i) dot ((float) i / (float) steps, i == steps / 2 ? 1.6f : 1.0f);
    else
        for (float p : { 0.0f, 0.5f, 1.0f }) dot (p, 1.4f);

    // black bakelite, with a fluted edge
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (1.0f, 2.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3c3c), c.x - knobR, c.y - knobR,
                                             juce::Colour (0xff070707), c.x + knobR, c.y + knobR, true));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));
    g.setColour (juce::Colour (0xff000000));
    for (int i = 0; i < 18; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 18.0f + angle;
        g.drawLine (juce::Line<float> (c.translated (std::sin (a) * knobR * 0.86f, -std::cos (a) * knobR * 0.86f),
                                       c.translated (std::sin (a) * knobR, -std::cos (a) * knobR)), 1.0f);
    }
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillEllipse (juce::Rectangle<float> (knobR * 1.3f, knobR * 1.3f).withCentre (c));
    // the white pointer
    g.setColour (juce::Colour (0xfff4f1e6));
    g.drawLine (juce::Line<float> (c.translated (std::sin (angle) * knobR * 0.25f, -std::cos (angle) * knobR * 0.25f),
                                   c.translated (std::sin (angle) * knobR * 0.95f, -std::cos (angle) * knobR * 0.95f)), 2.2f);
}

//==============================================================================
static juce::String signedDb (double v)
{
    const int i = juce::roundToInt (v);
    return i == 0 ? juce::String ("0") : (i > 0 ? "+" : "") + juce::String (i);
}

static juce::String sendText (double v)
{
    return v <= echoMinDb + 0.05 ? juce::String ("OFF") : signedDb (v);
}

ChannelStrip::ChannelStrip (MixEngine& e, int i, juce::StringArray& n) : engine (e), index (i), names (n)
{
    nameLabel.setEditable (false, true, false);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (Pequod::heading (13.0f));
    nameLabel.setTooltip ("Double-click to name this track");
    nameLabel.onTextChange = [this]
    {
        auto t = nameLabel.getText().trim().substring (0, 24);
        if (t.isEmpty()) t = juce::String (index + 1);
        nameLabel.setText (t, juce::dontSendNotification);
        names.set (index, t);
        changed();
    };
    addAndMakeVisible (nameLabel);

    // GAIN: the channel amp's input, switched in 2 dB steps
    input.setRange (-10.0, 20.0, 2.0);
    input.setDoubleClickReturnValue (true, 0.0);
    input.setTooltip ("GAIN into the valve channel amp (double-click for 0). Push it to drive the valves; bring the fader down to match.");
    input.onValueChange = [this] { engine.strip (index).inputDb = (float) input.getValue(); repaint(); changed(); };

    treble.setRange (-10.0, 10.0, 2.0);
    treble.setDoubleClickReturnValue (true, 0.0);
    treble.setTooltip ("TREBLE (double-click for flat). Pop: lift is a bell at 5 kHz, cut a shelf at 10 kHz. Classic: a 10 kHz shelf.");
    treble.onValueChange = [this] { engine.strip (index).trebleDb = (float) treble.getValue(); repaint(); changed(); };

    bass.setRange (-10.0, 10.0, 2.0);
    bass.setDoubleClickReturnValue (true, 0.0);
    bass.setTooltip ("BASS: a 100 Hz shelf (double-click for flat)");
    bass.onValueChange = [this] { engine.strip (index).bassDb = (float) bass.getValue(); repaint(); changed(); };

    echo.setRange (echoMinDb, 6.0, 0.5);
    echo.setSkewFactorFromMidPoint (-14.0);
    echo.setDoubleClickReturnValue (true, echoMinDb);
    echo.setTooltip ("ECHO send to the chamber, after the fader (double-click for off)");
    echo.onValueChange = [this] { engine.strip (index).echoDb = (float) echo.getValue(); repaint(); changed(); };

    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setTooltip ("Pan (double-click to centre)");
    pan.onValueChange = [this] { engine.strip (index).pan = (float) pan.getValue(); repaint(); changed(); };

    for (auto* k : { &input, &treble, &bass, &echo, &pan }) addChildComponent (k);
    pan.setVisible (true);

    trebleMode.setWantsKeyboardFocus (false);
    trebleMode.setTooltip ("Treble module: POP (5 kHz bell) or CLASSIC (10 kHz shelf)");
    trebleMode.onClick = [this]
    {
        auto& s = engine.strip (index);
        s.classic = ! s.classic.load();
        trebleMode.setButtonText (s.classic.load() ? "CLAS" : "POP");
        changed();
    };
    addChildComponent (trebleMode);

    mute.setClickingTogglesState (true);
    mute.setColour (juce::TextButton::buttonOnColourId, Pequod::Colours::danger);
    mute.setTooltip ("Mute");
    mute.onClick = [this] { engine.strip (index).mute = mute.getToggleState(); changed(); };
    solo.setClickingTogglesState (true);
    solo.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0c060));
    solo.setTooltip ("Solo (more than one can be soloed)");
    solo.onClick = [this] { engine.strip (index).solo = solo.getToggleState(); changed(); };
    for (auto* b : { &mute, &solo }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }

    fader.setTooltip ("Level (double-click for 0 dB)");
    fader.onValueChange = [this]
    {
        engine.strip (index).gainDb = (float) fader.getValue();
        dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
        changed();
    };
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (Pequod::heading (12.0f, false));
    addAndMakeVisible (dbLabel);

    setDesk (Desk::whitman);
    refreshFromEngine();
}

void ChannelStrip::setDesk (Desk d)
{
    desk = d;
    skin = &DeskSkin::of (d);
    const auto& info = deskInfo (d);
    input.setVisible (info.hasGain);
    treble.setVisible (info.hasEq);
    bass.setVisible (info.hasEq);
    trebleMode.setVisible (info.hasEq);
    echo.setVisible (info.hasEcho);
    for (auto* k : { &input, &treble, &bass, &echo, &pan }) k->setSkin (skin);
    fader.setSkin (skin);

    nameLabel.setColour (juce::Label::backgroundColourId, skin->well);
    nameLabel.setColour (juce::Label::textColourId, skin->bakeliteKnobs ? skin->accent : Pequod::Colours::text);
    nameLabel.setColour (juce::Label::textWhenEditingColourId, Pequod::Colours::text);
    dbLabel.setColour (juce::Label::textColourId, skin->engravingDim);
    meter.lowColour = skin->bakeliteKnobs ? juce::Colour (0xffcbbf98) : Pequod::Colours::mustardLo.brighter (0.25f);
    meter.midColour = skin->bakeliteKnobs ? skin->accent : Pequod::Colours::mustard;
    for (auto* b : { &mute, &solo, &trebleMode })
    {
        b->setColour (juce::TextButton::buttonColourId, skin->bakeliteKnobs ? juce::Colour (0xff2b2d2b) : Pequod::Colours::panelHi);
        b->setColour (juce::TextButton::textColourOffId, skin->bakeliteKnobs ? skin->engraving : Pequod::Colours::text);
    }
    resized();
    repaint();
}

void ChannelStrip::refreshFromEngine()
{
    auto& s = engine.strip (index);
    nameLabel.setText (names[index].isEmpty() ? juce::String (index + 1) : names[index], juce::dontSendNotification);
    pan.setValue (s.pan.load(), juce::dontSendNotification);
    input.setValue (s.inputDb.load(), juce::dontSendNotification);
    treble.setValue (s.trebleDb.load(), juce::dontSendNotification);
    bass.setValue (s.bassDb.load(), juce::dontSendNotification);
    echo.setValue (s.echoDb.load(), juce::dontSendNotification);
    trebleMode.setButtonText (s.classic.load() ? "CLAS" : "POP");
    mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    solo.setToggleState (s.solo.load(), juce::dontSendNotification);
    fader.setValue (s.gainDb.load(), juce::dontSendNotification);
    dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
    repaint();
}

void ChannelStrip::updateMeter()
{
    meter.push (MixEngine::takePeak (engine.strip (index).peak));
    auto& s = engine.strip (index);
    const bool d = s.mute.load() || (engine.anySolo() && ! s.solo.load());
    if (d != dimmed) { dimmed = d; repaint(); }
    if (mute.getToggleState() != s.mute.load()) mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    if (solo.getToggleState() != s.solo.load()) solo.setToggleState (s.solo.load(), juce::dontSendNotification);
}

void ChannelStrip::drawKnobCaption (juce::Graphics& g, const DeskKnob& k, const juce::String& title, const juce::String& value)
{
    if (! k.isVisible()) return;
    auto b = k.getBounds().toFloat();
    g.setFont (Pequod::heading (10.0f));
    g.setColour (skin->engraving);
    g.drawText (title, juce::Rectangle<float> ((float) getWidth(), 11.0f).withPosition (0.0f, b.getY() - 12.0f).reduced (6.0f, 0.0f),
                skin->bakeliteKnobs ? juce::Justification::centredLeft : juce::Justification::centred, false);
    if (value.isNotEmpty())
    {
        g.setFont (Pequod::heading (10.0f, false));
        g.setColour (skin->bakeliteKnobs ? skin->accent : skin->engravingDim);
        g.drawText (value, juce::Rectangle<float> ((float) getWidth(), 11.0f).withPosition (0.0f, b.getY() - 12.0f).reduced (6.0f, 0.0f),
                    skin->bakeliteKnobs ? juce::Justification::centredRight : juce::Justification::centred, false);
    }
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    drawPanel (g, r, *skin, ! dimmed);

    g.setFont (Pequod::heading (11.0f));
    g.setColour (skin->engravingDim);
    g.drawText ((skin->bakeliteKnobs ? "CH " : "TRK ") + juce::String (index + 1), r.withTop (12.0f).withHeight (14.0f), juce::Justification::centred);

    const float p = (float) pan.getValue();
    const auto panTxt = std::abs (p) < 0.005f ? juce::String ("C") : (p < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (p) * 100.0f));
    if (skin->bakeliteKnobs)
    {
        drawKnobCaption (g, input, "GAIN", signedDb (input.getValue()));
        drawKnobCaption (g, treble, "TREBLE", signedDb (treble.getValue()));
        drawKnobCaption (g, bass, "BASS", signedDb (bass.getValue()));
        drawKnobCaption (g, echo, "ECHO", sendText (echo.getValue()));
        drawKnobCaption (g, pan, "PAN", panTxt);
    }
    else
    {
        g.setFont (Pequod::heading (10.5f, false));
        g.setColour (skin->engravingDim);
        g.drawText (panTxt, pan.getBounds().translated (0, pan.getHeight() - 4).withHeight (14), juce::Justification::centred);
    }

    if (dimmed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (r, 3.0f);
    }
}

void ChannelStrip::resized()
{
    if (skin == nullptr) return;
    auto r = getLocalBounds().reduced (6, 0);
    r.removeFromTop (28);
    nameLabel.setBounds (r.removeFromTop (22));

    if (skin->bakeliteKnobs)
    {
        const int k = 40;
        auto knobRow = [&] (DeskKnob& knob)
        {
            if (! knob.isVisible()) return juce::Rectangle<int>();
            r.removeFromTop (16);
            auto row = r.removeFromTop (k);
            knob.setBounds (row.withSizeKeepingCentre (k, k));
            return row;
        };
        knobRow (input);
        auto t = knobRow (treble);
        if (! t.isEmpty())
        {
            treble.setBounds (t.removeFromLeft (k + 4).withSizeKeepingCentre (k, k));
            trebleMode.setBounds (t.withSizeKeepingCentre (t.getWidth(), 18));
        }
        knobRow (bass);
        knobRow (echo);
        knobRow (pan);
        r.removeFromTop (8);
    }
    else
    {
        r.removeFromTop (6);
        pan.setBounds (r.removeFromTop (44).withSizeKeepingCentre (44, 44));
        r.removeFromTop (16);
    }

    auto ms = r.removeFromTop (22);
    mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2 - 1));
    ms.removeFromLeft (2);
    solo.setBounds (ms);
    r.removeFromTop (8);
    if (skin->bakeliteKnobs) r.removeFromBottom (14);    // the panel's screw
    dbLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (12);
    meter.setBounds (r.removeFromRight (skin->bakeliteKnobs ? 7 : 10).reduced (0, 17));
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
    addChildComponent (vuL);
    addChildComponent (vuR);

    echoReturn.setRange (echoMinDb, 6.0, 0.5);
    echoReturn.setSkewFactorFromMidPoint (-14.0);
    echoReturn.setDoubleClickReturnValue (true, -6.0);
    echoReturn.setTooltip ("ECHO RETURN: how much of the chamber comes back into the mix");
    echoReturn.onValueChange = [this] { engine.echoReturnDb = (float) echoReturn.getValue(); repaint(); if (onChanged) onChanged(); };
    addChildComponent (echoReturn);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (Pequod::heading (13.0f));
    addAndMakeVisible (dbLabel);
    setDesk (Desk::whitman);
    refreshFromEngine();
}

void MasterStrip::setDesk (Desk d)
{
    desk = d;
    skin = &DeskSkin::of (d);
    const auto& info = deskInfo (d);
    meter.setVisible (! skin->vuMeters);
    vuL.setVisible (skin->vuMeters);
    vuR.setVisible (skin->vuMeters);
    echoReturn.setVisible (info.hasEcho);
    echoReturn.setSkin (skin);
    fader.setSkin (skin);
    dbLabel.setColour (juce::Label::textColourId, skin->bakeliteKnobs ? skin->accent : Pequod::Colours::text);
    resized();
    repaint();
}

void MasterStrip::refreshFromEngine()
{
    fader.setValue (engine.masterDb.load(), juce::dontSendNotification);
    echoReturn.setValue (engine.echoReturnDb.load(), juce::dontSendNotification);
    dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
    repaint();
}

void MasterStrip::updateMeter()
{
    const float l = MixEngine::takePeak (engine.masterPeakL), r = MixEngine::takePeak (engine.masterPeakR);
    meter.push (l, r);
    vuL.push (l);
    vuR.push (r);
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
    drawPanel (g, r, *skin, true);
    const auto& info = deskInfo (desk);

    if (skin->bakeliteKnobs)
    {
        // an engraved maker's plate
        auto plate = r.reduced (14.0f, 0.0f).withTop (18.0f).withHeight (34.0f);
        g.setColour (juce::Colour (0xff1b1d1b));
        g.fillRoundedRectangle (plate, 2.0f);
        g.setColour (skin->accent);
        g.setFont (Pequod::heading (17.0f));
        g.drawText (juce::String ("EMI  ") + info.badge, plate.withTrimmedBottom (12.0f), juce::Justification::centred);
        g.setFont (Pequod::heading (8.5f, false));
        g.setColour (skin->engravingDim);
        g.drawText ("ABBEY ROAD  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  STUDIO TWO", plate.withTrimmedTop (21.0f), juce::Justification::centred);

        g.setFont (Pequod::heading (10.0f));
        g.setColour (skin->engraving);
        if (echoReturn.isVisible())
        {
            auto b = echoReturn.getBounds().toFloat();
            g.drawText ("ECHO RETURN", juce::Rectangle<float> (b.getX() - 70.0f, b.getY(), 66.0f, b.getHeight()), juce::Justification::centredRight);
            g.setColour (skin->accent);
            g.setFont (Pequod::heading (10.0f, false));
            g.drawText (sendText (echoReturn.getValue()), juce::Rectangle<float> (b.getRight() + 4.0f, b.getY(), 50.0f, b.getHeight()), juce::Justification::centredLeft);
        }
        g.setFont (Pequod::heading (11.0f));
        g.setColour (skin->engraving);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("MAIN  \xe2\x86\x92  2-TRACK")),
                    juce::Rectangle<float> (r.getX(), (float) fader.getY() - 20.0f, r.getWidth(), 14.0f), juce::Justification::centred);
    }
    else
    {
        g.setFont (Pequod::heading (15.0f));
        g.setColour (skin->accent);
        g.drawText ("MASTER", r.withTop (8.0f).withHeight (18.0f), juce::Justification::centred);
        g.setFont (Pequod::body (10.5f));
        g.setColour (skin->engravingDim);
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x92 HAWTHORNE 2-TRACK"), r.withTop (28.0f).withHeight (14.0f), juce::Justification::centred);
    }

    if (! lampArea.isEmpty())
    {
        auto lamp = lampArea.toFloat();
        g.setColour (clipped ? Pequod::Colours::danger : Pequod::Colours::rail);
        g.fillRoundedRectangle (lamp, 3.0f);
        g.setFont (Pequod::heading (11.0f));
        g.setColour (clipped ? Pequod::Colours::text : Pequod::Colours::textDim);
        g.drawText ("CLIP", lamp, juce::Justification::centred);
    }
}

void MasterStrip::resized()
{
    if (skin == nullptr) return;
    auto r = getLocalBounds().reduced (10, 0);
    if (skin->bakeliteKnobs)
    {
        r.removeFromTop (62);
        auto meters = r.removeFromTop (110);
        vuL.setBounds (meters.removeFromLeft (meters.getWidth() / 2 - 3));
        meters.removeFromLeft (6);
        vuR.setBounds (meters);
        r.removeFromTop (10);
        if (echoReturn.isVisible())
            echoReturn.setBounds (r.removeFromTop (44).withSizeKeepingCentre (44, 44));
        lampArea = {};      // the main valves round off before full scale: nothing to clip
        r.removeFromTop (30);
        r.removeFromBottom (14);
        dbLabel.setBounds (r.removeFromBottom (22));
        r.removeFromBottom (8);
        fader.setBounds (r.withSizeKeepingCentre (96, r.getHeight()));
    }
    else
    {
        r.removeFromTop (50);
        lampArea = r.removeFromTop (20).withSizeKeepingCentre (54, 20);
        r.removeFromTop (16);
        dbLabel.setBounds (r.removeFromBottom (22));
        r.removeFromBottom (4);
        meter.setBounds (r.removeFromRight (26).reduced (0, 17));
        r.removeFromRight (4);
        fader.setBounds (r);
    }
}

} // namespace whitman
