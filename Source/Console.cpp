#include "Console.h"
#include "DeskPanels.h"

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
          false, false, true, 76, 236 },
        // REDD.51: EMI grey-green painted steel, cream engraving, black bakelite knobs, Painton quadrant faders, VUs
        { C (0xff252825), C (0xff737a73), C (0xff5f665f), C (0xff383d39),
          C (0xfff2eee0), C (0xffd2cfc0), C (0xfff0dfb0), C (0xff1b1d1b),
          true, true, true, 94, 236, 0, "ABBEY ROAD  -  STUDIO TWO" },
        // REDD.37: the same EMI family, an older, browner grey-green
        { C (0xff262622), C (0xff74776a), C (0xff606355), C (0xff3a3c34),
          C (0xfff2ecda), C (0xffd4cfba), C (0xfff2d9a0), C (0xff1c1c19),
          true, true, true, 94, 236, 0, "ABBEY ROAD  -  STUDIO TWO" },
        // TG12345 Mk I: pale grey modules, black legends, black knobs, straight faders, VUs
        { C (0xff1f2224), C (0xffaeb3b6), C (0xff989da1), C (0xff5c6266),
          C (0xff15181a), C (0xff3a4044), C (0xffd8492e), C (0xff15181a),
          true, false, true, 104, 236, 0, "ABBEY ROAD  -  STUDIO TWO" },
        // TG12345 Mk IV: the same
        { C (0xff1f2224), C (0xffaeb3b6), C (0xff989da1), C (0xff5c6266),
          C (0xff15181a), C (0xff3a4044), C (0xffd8492e), C (0xff15181a),
          true, false, true, 104, 236, 0, "ABBEY ROAD  -  STUDIO TWO" },
        // Trident A-Range: aubergine modules, cream legends, aluminium knobs, slide-fader EQ
        { C (0xff1d1519), C (0xff5a3448), C (0xff47283a), C (0xff2b1822),
          C (0xfff1e7d6), C (0xffcdbfae), C (0xffeec06a), C (0xff1a1214),
          true, false, true, 104, 236, 1, "TRIDENT STUDIOS  -  LONDON" },
        // Neve 8028: blue-grey modules, white legends, grey knobs with coloured caps
        { C (0xff1b1f23), C (0xff5d6c7b), C (0xff4c5a67), C (0xff323b44),
          C (0xfff3f4f2), C (0xffcdd3d8), C (0xffe9c46a), C (0xff15191c),
          true, false, true, 104, 236, 2, "SOUND CITY  -  VAN NUYS" },
        // SSL 4000 E: dark grey, white legends, colour-capped knobs
        { C (0xff151617), C (0xff43464b), C (0xff35383c), C (0xff23252a),
          C (0xfff2f2f2), C (0xffbfc2c6), C (0xfff0c24a), C (0xff111213),
          true, false, true, 126, 236, 2, "OXFORD  -  ENGLAND", 1 },
        // Neotek Elite: light grey, dark legends, grey knobs with coloured caps
        { C (0xff1e2022), C (0xffb2b4b3), C (0xff9fa2a1), C (0xff5d6062),
          C (0xff16181a), C (0xff3a3e41), C (0xff2f6fae), C (0xff16181a),
          true, false, true, 104, 236, 2, "ELECTRICAL AUDIO  -  CHICAGO", 2 },
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
    if (style != 0)
    {
        // a meter-bridge bargraph: lit segments up a black window, with a scale beside it
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff070707));
        g.fillRoundedRectangle (r, 2.0f);
        auto col = r.reduced (3.0f).removeFromRight (r.getWidth() * 0.42f);
        auto scale = r.reduced (3.0f).withRight (col.getX() - 2.0f);
        const int segs = 30;
        const float sh = col.getHeight() / (float) segs;
        g.setFont (Pequod::heading (7.5f, false));
        for (float db : { 0.0f, -10.0f, -20.0f })
        {
            const float y = col.getBottom() - col.getHeight() * dbToPos (db + nominalDbfs + 3.0f);   // marked in VU
            g.setColour (juce::Colour (0xff9a9a9a));
            g.drawText (db == 0.0f ? juce::String ("0") : juce::String ((int) db), scale.withY (y - 5.0f).withHeight (10.0f), juce::Justification::centredRight, false);
        }
        for (int i = 0; i < segs; ++i)
        {
            const float pos = ((float) i + 0.5f) / (float) segs;
            const auto seg = juce::Rectangle<float> (col.getX(), col.getBottom() - (float) (i + 1) * sh, col.getWidth(), sh).reduced (0.0f, sh * 0.18f);
            // the segment's level in dBFS, for its colour
            float dbAt = -60.0f;
            for (float d = -60.0f; d <= 6.0f; d += 0.25f) if (dbToPos (d) <= pos) dbAt = d;
            const bool lit = level[0] > -60.0f && pos <= dbToPos (level[0]);
            const bool peak = hold[0] > -60.0f && std::abs (pos - dbToPos (hold[0])) < 0.5f / (float) segs;
            juce::Colour on;
            if (style == 1) on = dbAt > -18.0f + 6.0f ? juce::Colour (0xffff5a2a) : juce::Colour (0xffff8a2e);          // plasma glows orange
            else            on = dbAt > -3.0f ? juce::Colour (0xffff3b2e) : dbAt > -18.0f + 3.0f ? juce::Colour (0xffffc23a) : juce::Colour (0xff46d65a);
            g.setColour (lit || peak ? on : on.withAlpha (0.10f));
            g.fillRect (seg);
        }
        return;
    }

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
    // PPM: peaks straight in, then falls 24 dB in 2.8 s
    const float rel = peak > 0.0f ? juce::Decibels::gainToDecibels (peak) - nominalDbfs : -40.0f;
    const float nextPpm = juce::jlimit (-40.0f, 20.0f, juce::jmax (rel, ppmDb - 8.6f / 30.0f));
    if (ppm && std::abs (nextPpm - ppmDb) > 0.05f) repaint();
    ppmDb = nextPpm;

    // a VU reads the average: a sine's is ~3 dB under its peak. 0 VU = -18 dBFS.
    const float vu = peak > 0.0f ? juce::Decibels::gainToDecibels (peak * 0.707f) - nominalDbfs : -40.0f;
    const float target = juce::jlimit (-22.0f, 4.0f, vu);
    const float next = needle + (target - needle) * 0.32f;     // ~300 ms to settle, like a real VU
    if (std::abs (next - needle) > 0.02f) { needle = next; if (! ppm) repaint(); }
}

// PPM scale: marks 1..7, 4 = reference level, 4 dB a mark (6 dB between 1 and 2), as in Hawthorne
static float ppmMarkPos (float mark) { return 0.06f + (mark - 1.0f) / 6.0f * 0.88f; }

void VUMeter::paintPpm (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff111211));
    g.fillRoundedRectangle (r, 4.0f);
    auto face = r.reduced (4.0f);
    g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (face, 2.5f);

    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.06f);
    const float radius = face.getHeight() * 0.64f;
    const float span = 0.62f;
    auto angleOf = [span] (float pos) { return -span + 2.0f * span * pos; };
    auto onArc = [&] (float a, float rad) { return pivot.translated (rad * std::sin (a), -rad * std::cos (a)); };
    const bool small = getWidth() < 130;

    const juce::Colour ink (0xfff2f2ee);
    g.setColour (ink);
    g.setFont (Pequod::heading (small ? 8.5f : 10.0f));
    for (int m = 1; m <= 7; ++m)
    {
        const float a = angleOf (ppmMarkPos ((float) m));
        g.drawLine (juce::Line<float> (onArc (a, radius), onArc (a, radius + 5.0f)), 1.4f);
        if (! small || m % 2 == 0 || m == 7)
            g.drawText (juce::String (m), juce::Rectangle<float> (14.0f, 10.0f).withCentre (onArc (a, radius + 12.0f)), juce::Justification::centred);
        if (m < 7)
        {
            const float h = angleOf (ppmMarkPos ((float) m + 0.5f));
            g.drawLine (juce::Line<float> (onArc (h, radius), onArc (h, radius + 2.5f)), 0.9f);
        }
    }
    g.setFont (Pequod::heading (small ? 10.0f : 12.0f));
    g.drawText ("PPM", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (14.0f), juce::Justification::centred);
    if (caption.isNotEmpty())
    {
        g.setFont (Pequod::heading (8.5f, false));
        g.drawText (caption, face.withTrimmedTop (face.getHeight() * 0.52f + 14.0f).withHeight (11.0f), juce::Justification::centred);
    }

    // needle: white on the black face
    const float mark = ppmDb >= -8.0f ? 4.0f + ppmDb / 4.0f : 2.0f + (ppmDb + 8.0f) / 6.0f;
    const float pos = juce::jlimit (0.0f, 1.06f, mark < 1.0f ? ppmMarkPos (1.0f) * juce::jmax (0.0f, mark) : ppmMarkPos (mark));
    const float a = angleOf (pos);
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (face.toNearestInt());
        g.setColour (juce::Colour (0xfffafafa));
        g.drawLine (juce::Line<float> (onArc (a, radius * 0.25f), onArc (a, radius + 8.0f)), 1.6f);
    }
    g.setColour (juce::Colour (0xff111211));
    g.fillRect (face.withTop (face.getBottom() - 10.0f));
}

void VUMeter::paint (juce::Graphics& g)
{
    if (ppm) { paintPpm (g); return; }
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
        const bool small = getWidth() < 130;    // a channel's meter: fewer numbers, or they run together
        if (small ? (vu == -10.0f || vu == 0.0f || vu == 3.0f)
                  : (vu == -20.0f || vu == -10.0f || vu == -5.0f || vu == -3.0f || vu == 0.0f || vu == 3.0f))
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
    setScrollWheelEnabled (false);       // the wheel scrolls the console, it never moves a fader
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
        g.setColour (unity ? (skin != nullptr ? skin->accent : Pequod::Colours::mustard)
                           : (skin != nullptr ? skin->engravingDim : Pequod::Colours::textDim.withAlpha (0.6f)));
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
    // A Painton quadrant fader: the lever swings on an arc under the panel, but from above you
    // see a straight slot in a chrome plate, with the lever's black knob riding along it.
    // Up is louder.
    const auto& s = *skin;
    auto r = getLocalBounds().toFloat();
    const float knobH = 22.0f, inset = knobH * 0.5f + 6.0f;
    const float top = r.getY() + inset, bottom = r.getBottom() - inset;
    const float len = bottom - top;
    const float x = r.getRight() - 16.0f;
    auto yOf = [&] (double db) { return (float) (bottom - len * valueToProportionOfLength (db)); };

    // chrome plate, the slot down its middle
    auto plate = juce::Rectangle<float> (18.0f, len + 26.0f).withCentre ({ x, (top + bottom) * 0.5f });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd4d7d2), plate.getX(), plate.getY(),
                                             juce::Colour (0xff8e918c), plate.getRight(), plate.getY(), false));
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (juce::Colour (0xff5d605b));
    g.drawRoundedRectangle (plate, 3.0f, 0.8f);
    for (float y : { plate.getY() + 5.0f, plate.getBottom() - 5.0f })
        Pequod::drawScrew (g, { x, y }, 2.4f);
    g.setColour (juce::Colour (0xff0d0e0d));
    g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, len + 4.0f).withCentre ({ x, (top + bottom) * 0.5f }), 2.0f);

    // engraved scale, left of the plate
    g.setFont (Pequod::heading (9.5f, false));
    for (double db : { 10.0, 5.0, 0.0, -5.0, -10.0, -20.0, -30.0, -40.0, -60.0 })
    {
        const float y = yOf (db);
        const bool unity = db == 0.0;
        g.setColour (unity ? s.accent : s.engravingDim);
        g.drawLine (plate.getX() - 9.0f, y, plate.getX() - 3.0f, y, unity ? 1.6f : 1.0f);
        g.drawText (db <= faderMinDb ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")) : (db > 0.0 ? "+" : "") + juce::String ((int) std::abs (db)),
                    juce::Rectangle<float> (plate.getX() - 36.0f, y - 6.0f, 25.0f, 12.0f), juce::Justification::centredRight, false);
    }

    // the lever's knob: black, rounded, with a white line across it
    const float y = yOf (getValue());
    auto knob = juce::Rectangle<float> (26.0f, knobH).withCentre ({ x, y });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (knob.translated (1.5f, 2.5f), 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3d3d), x, knob.getY(), juce::Colour (0xff0a0a0a), x, knob.getBottom(), false));
    g.fillRoundedRectangle (knob, 6.0f);
    g.setColour (getValue() <= faderMinDb + 0.05 ? juce::Colour (0xff6a6a6a) : juce::Colour (0xfff2eee0));
    g.fillRect (knob.getX() + 4.0f, y - 1.0f, knob.getWidth() - 8.0f, 2.0f);
}

//==============================================================================
DeskKnob::DeskKnob() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
{
    setWantsKeyboardFocus (false);
    setScrollWheelEnabled (false);       // the wheel never turns a knob
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

    if (skin->knobStyle == 1)
    {
        // brushed aluminium, a black line for a pointer
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (1.0f, 2.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9eaea), c.x - knobR, c.y - knobR, juce::Colour (0xff8b8d8f), c.x + knobR, c.y + knobR, true));
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));
        g.setColour (juce::Colour (0xff6b6d6f));
        g.drawEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c), 0.8f);
        g.setColour (juce::Colour (0xffc9cbcc));
        g.fillEllipse (juce::Rectangle<float> (knobR * 1.2f, knobR * 1.2f).withCentre (c));
        g.setColour (juce::Colour (0xff111111));
        g.drawLine (juce::Line<float> (c.translated (std::sin (angle) * knobR * 0.2f, -std::cos (angle) * knobR * 0.2f),
                                       c.translated (std::sin (angle) * knobR * 0.95f, -std::cos (angle) * knobR * 0.95f)), 2.0f);
        return;
    }
    if (skin->knobStyle == 2)
    {
        // a grey skirted knob with a coloured cap
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (1.0f, 2.0f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b5e62), c.x - knobR, c.y - knobR, juce::Colour (0xff1d1f21), c.x + knobR, c.y + knobR, true));
        g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));
        const auto capColour = cap.isTransparent() ? juce::Colour (0xff7d8186) : cap;
        g.setGradientFill (juce::ColourGradient (capColour.brighter (0.35f), c.x - knobR * 0.6f, c.y - knobR * 0.6f, capColour.darker (0.3f), c.x + knobR * 0.6f, c.y + knobR * 0.6f, true));
        g.fillEllipse (juce::Rectangle<float> (knobR * 1.25f, knobR * 1.25f).withCentre (c));
        g.setColour (juce::Colour (0xfff4f4f2));
        g.drawLine (juce::Line<float> (c.translated (std::sin (angle) * knobR * 0.3f, -std::cos (angle) * knobR * 0.3f),
                                       c.translated (std::sin (angle) * knobR * 0.98f, -std::cos (angle) * knobR * 0.98f)), 2.0f);
        return;
    }

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

static juce::String presenceLabel (int i)
{
    if (presenceIsShelf (i)) return "10k SHELF";
    const float hz = presenceHz (i);
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, std::fmod (hz, 1000.0f) != 0.0f ? 1 : 0) + "k" : juce::String ((int) hz);
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
    nameLabel.onTextChange = [this]
    {
        // a name typed here wins; clear it (or type Hawthorne's) to follow Hawthorne again
        auto t = nameLabel.getText().trim().substring (0, 24);
        if (t == juce::String (index + 1) || t == hawthorneName) t = {};
        names.set (index, t);
        showName();
        changed();
    };
    addAndMakeVisible (nameLabel);

    // GAIN: the channel amp's input, switched in 2 dB steps
    input.setRange (-10.0, 20.0, 0.5);
    input.setDoubleClickReturnValue (true, 0.0);
    input.setTooltip ("GAIN into the valve channel amp (double-click for 0). Push it to drive the valves; bring the fader down to match.");
    input.onValueChange = [this] { engine.strip (index).inputDb = (float) input.getValue(); repaint(); changed(); };

    treble.setRange (-10.0, 10.0, 0.5);
    treble.setDoubleClickReturnValue (true, 0.0);
    treble.setTooltip ("TREBLE (double-click for flat). Pop: lift is a bell at 5 kHz, cut a shelf at 10 kHz. Classic: a 10 kHz shelf.");
    treble.onValueChange = [this] { engine.strip (index).trebleDb = (float) treble.getValue(); repaint(); changed(); };

    bass.setRange (-10.0, 10.0, 0.5);
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

    presence.setRange (-10.0, 10.0, 0.5);
    presence.setDoubleClickReturnValue (true, 0.0);
    presence.setTooltip ("PRESENCE: a bell at the frequency on the small switch (double-click for flat)");
    presence.onValueChange = [this] { engine.strip (index).presenceDb = (float) presence.getValue(); repaint(); changed(); };

    presenceFreq.setRange (0.0, numPresenceFreqs - 1, 1.0);
    presenceFreq.setDoubleClickReturnValue (true, 4.0);
    presenceFreq.setTooltip ("Presence frequency: a bell at 500, 800, 1.2k, 1.8k, 2.8k, 4.2k or 6.8k, or a 10 kHz shelf");
    presenceFreq.onValueChange = [this] { engine.strip (index).presenceFreq = (int) presenceFreq.getValue(); repaint(); changed(); };

    comp.setRange (0.0, 24.0, 1.0);
    comp.setDoubleClickReturnValue (true, 0.0);
    comp.setTooltip ("COMPRESS: how hard the channel compressor / limiter is driven (with some make-up gain). The figure is the gain reduction now.");
    comp.onValueChange = [this] { engine.strip (index).compDb = (float) comp.getValue(); repaint(); changed(); };

    recovery.setRange (0.0, numRecoveries - 1, 1.0);
    recovery.setDoubleClickReturnValue (true, 2.0);
    recovery.setTooltip ("RECOVERY: how fast it lets go - 0.1, 0.25, 0.5, 1, 2 or 5 seconds");
    recovery.onValueChange = [this] { engine.strip (index).recovery = (int) recovery.getValue(); repaint(); changed(); };

    for (auto* k : { &input, &treble, &presence, &presenceFreq, &bass, &comp, &recovery, &echo, &pan }) addChildComponent (k);
    pan.setVisible (true);

    compMode.setWantsKeyboardFocus (false);
    compMode.setTooltip ("The channel's compressor: OFF, COMP (2:1) or LIMIT (8:1)");
    compMode.onClick = [this]
    {
        auto& s = engine.strip (index);
        s.compMode = (s.compMode.load() + 1) % 3;
        showCompMode();
        repaint();
        changed();
    };
    addChildComponent (compMode);

    trebleMode.setWantsKeyboardFocus (false);
    trebleMode.setTooltip ("Tone module: POP (treble lift a 4.7 kHz bell, bass reaching higher) or CLASSIC (100 Hz and 10 kHz shelves). Studio Two's desks ran Pop modules.");
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
    addChildComponent (chanVu);

    bandPanel = std::make_unique<BandEqPanel> (engine, index);
    bandPanel->onChanged = [this] { changed(); };
    addChildComponent (*bandPanel);
    dynPanel = std::make_unique<SslDynamicsPanel> (engine, index);
    dynPanel->onChanged = [this] { changed(); };
    addChildComponent (*dynPanel);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (Pequod::heading (12.0f, false));
    addAndMakeVisible (dbLabel);

    setDesk (Desk::whitman);
    refreshFromEngine();
}

ChannelStrip::~ChannelStrip() = default;

void ChannelStrip::setDesk (Desk d)
{
    desk = d;
    skin = &DeskSkin::of (d);
    const auto& info = deskInfo (d);
    input.setVisible (info.hasGain);
    treble.setVisible (info.eq == EqStyle::redd || info.eq == EqStyle::tgMk1);
    bass.setVisible (info.eq != EqStyle::none);
    trebleMode.setVisible (info.eq == EqStyle::redd);
    presence.setVisible (info.eq == EqStyle::tgMk4);
    presenceFreq.setVisible (info.eq == EqStyle::tgMk4);
    // every desk shows a meter per channel at the top: its own kind where it had one, else an analogue VU
    chanVu.setVisible (skin->meterStyle == 0);
    meter.setVisible (skin->meterStyle != 0);
    meter.style = skin->meterStyle;
    if (bandPanel != nullptr) bandPanel->setDesk (d);
    if (dynPanel != nullptr) { dynPanel->setVisible (info.sslDynamics); dynPanel->setSkin (skin); }
    echo.setTooltip (juce::String (info.sendName) + " send, after the fader (double-click for off)");
    comp.setVisible (info.hasComp);
    recovery.setVisible (info.hasComp);
    compMode.setVisible (info.hasComp);
    echo.setVisible (info.hasEcho);
    input.setTooltip (info.channelAsym > 0.2f
        ? "GAIN into the valve channel amp (double-click for 0). Push it to drive the valves; bring the fader down to match."
        : "GAIN into the channel amp (double-click for 0). Push it and the transistors get edgy; bring the fader down to match.");
    const bool tg = info.eq == EqStyle::tgMk1 || info.eq == EqStyle::tgMk4;
    const bool redd = info.eq == EqStyle::redd;
    // the REDDs' tone controls and pan pots were switches: EQ in 2 dB steps, pan 5 positions either side of centre
    treble.setRange (-10.0, 10.0, redd ? 2.0 : 0.5);
    bass.setRange (-10.0, 10.0, redd ? 2.0 : 0.5);
    pan.setRange (-1.0, 1.0, redd ? 0.2 : 0.01);
    bass.setTooltip (tg ? "BASS: a 50 Hz shelf (double-click for flat)"
                        : "BASS, switched in 2 dB steps (double-click for flat). Classic: a 100 Hz shelf. Pop: a broader shelf that reaches up to about 500 Hz.");
    treble.setTooltip (tg ? "TREBLE (double-click for flat): the lift is a bell at 5 kHz, the cut a shelf at 10 kHz"
                          : "TREBLE, switched in 2 dB steps (double-click for flat). Pop: lift is a broad bell at 4.7 kHz, cut a shelf from about 2 kHz. Classic: a 10 kHz shelf.");
    pan.setTooltip (redd ? "Pan: a switch, 5 positions either side of centre (double-click to centre)" : "Pan (double-click to centre)");
    for (auto* k : { &input, &treble, &presence, &presenceFreq, &bass, &comp, &recovery, &echo, &pan }) k->setSkin (skin);
    fader.setSkin (skin);

    nameLabel.setColour (juce::Label::backgroundColourId, skin->well);
    nameLabel.setColour (juce::Label::textColourId, skin->bakeliteKnobs ? skin->accent : Pequod::Colours::text);
    nameLabel.setColour (juce::Label::textWhenEditingColourId, Pequod::Colours::text);
    dbLabel.setColour (juce::Label::textColourId, skin->engravingDim);
    meter.lowColour = skin->bakeliteKnobs ? juce::Colour (0xffcbbf98) : Pequod::Colours::mustardLo.brighter (0.25f);
    meter.midColour = skin->bakeliteKnobs ? skin->accent : Pequod::Colours::mustard;
    const bool lightPanel = skin->panelTop.getPerceivedBrightness() > 0.6f;
    for (auto* b : { &mute, &solo, &trebleMode, &compMode })
    {
        b->setColour (juce::TextButton::buttonColourId, skin->bakeliteKnobs ? juce::Colour (0xff2b2d2b) : Pequod::Colours::panelHi);
        b->setColour (juce::TextButton::textColourOffId, skin->bakeliteKnobs ? (lightPanel ? juce::Colour (0xffeceae2) : skin->engraving) : Pequod::Colours::text);
    }
    nameLabel.setColour (juce::Label::textColourId, skin->bakeliteKnobs ? (lightPanel ? juce::Colour (0xffeceae2) : skin->accent) : Pequod::Colours::text);
    if (lightPanel) meter.lowColour = juce::Colour (0xff4b5a63), meter.midColour = skin->accent;
    resized();
    repaint();
}

void ChannelStrip::setHawthorneTrack (const juce::String& name, bool armed)
{
    if (name == hawthorneName && armed == hawthorneArmed) return;
    hawthorneName = name;
    hawthorneArmed = armed;
    showName();
    repaint();
}

void ChannelStrip::showName()
{
    const auto own = names[index];
    const auto shown = own.isNotEmpty() ? own : (hawthorneName.isNotEmpty() ? hawthorneName : juce::String (index + 1));
    nameLabel.setText (shown, juce::dontSendNotification);
    nameLabel.setTooltip (own.isNotEmpty() ? juce::String ("Named here. Double-click to rename; clear it to use the name from Hawthorne.")
                        : hawthorneName.isNotEmpty() ? juce::String ("Name from Hawthorne. Double-click to rename it here.")
                                                     : juce::String ("Double-click to name this track (or name it in Hawthorne)"));
}

void ChannelStrip::refreshFromEngine()
{
    auto& s = engine.strip (index);
    showName();
    pan.setValue (s.pan.load(), juce::dontSendNotification);
    input.setValue (s.inputDb.load(), juce::dontSendNotification);
    treble.setValue (s.trebleDb.load(), juce::dontSendNotification);
    bass.setValue (s.bassDb.load(), juce::dontSendNotification);
    echo.setValue (s.echoDb.load(), juce::dontSendNotification);
    trebleMode.setButtonText (s.classic.load() ? "CLAS" : "POP");
    presence.setValue (s.presenceDb.load(), juce::dontSendNotification);
    presenceFreq.setValue (s.presenceFreq.load(), juce::dontSendNotification);
    comp.setValue (s.compDb.load(), juce::dontSendNotification);
    recovery.setValue (s.recovery.load(), juce::dontSendNotification);
    showCompMode();
    if (bandPanel != nullptr) bandPanel->refresh();
    if (dynPanel != nullptr) dynPanel->refresh();
    mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    solo.setToggleState (s.solo.load(), juce::dontSendNotification);
    fader.setValue (s.gainDb.load(), juce::dontSendNotification);
    dbLabel.setText (fader.getTextFromValue (fader.getValue()), juce::dontSendNotification);
    repaint();
}

void ChannelStrip::updateMeter()
{
    const float pk = MixEngine::takePeak (engine.strip (index).peak);
    meter.push (pk);
    chanVu.push (pk);
    auto& s = engine.strip (index);
    const bool d = s.mute.load() || (engine.anySolo() && ! s.solo.load());
    if (d != dimmed) { dimmed = d; repaint(); }
    if (mute.getToggleState() != s.mute.load()) mute.setToggleState (s.mute.load(), juce::dontSendNotification);
    if (solo.getToggleState() != s.solo.load()) solo.setToggleState (s.solo.load(), juce::dontSendNotification);
    if (dynPanel != nullptr && dynPanel->isVisible()) dynPanel->updateMeters();
    if (comp.isVisible())
    {
        const float gr = s.gainReduction.load();
        if (std::abs (gr - shownGr) > 0.4f || (gr < 0.05f) != (shownGr < 0.05f))
        {
            shownGr = gr;
            repaint (comp.getBounds().translated (0, -14).withHeight (14).withX (0).withWidth (getWidth()));
        }
    }
}

void ChannelStrip::showCompMode()
{
    const int m = engine.strip (index).compMode.load();
    compMode.setButtonText (m == 2 ? "LIMIT" : m == 1 ? "COMP" : "OFF");
    compMode.setToggleState (m != 0, juce::dontSendNotification);
    compMode.setColour (juce::TextButton::buttonOnColourId, m == 2 ? juce::Colour (0xffd8492e) : juce::Colour (0xffd9a521));
    compMode.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff111111));
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
    if (hawthorneArmed)
    {
        // armed to record in Hawthorne: a red lamp by the number
        const auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getRight() - 11.0f, r.getY() + 19.0f });
        g.setColour (Pequod::Colours::danger.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (2.5f));
        g.setColour (juce::Colour (0xffe8432f));
        g.fillEllipse (lamp);
    }

    const float p = (float) pan.getValue();
    const auto panTxt = std::abs (p) < 0.005f ? juce::String ("C") : (p < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (p) * 100.0f));
    if (skin->bakeliteKnobs)
    {
        drawKnobCaption (g, input, "GAIN", signedDb (input.getValue()));
        drawKnobCaption (g, treble, "TREBLE", signedDb (treble.getValue()));
        {
            drawKnobCaption (g, presence, "PRES " + presenceLabel ((int) presenceFreq.getValue()), signedDb (presence.getValue()));
        }
        drawKnobCaption (g, bass, "BASS", signedDb (bass.getValue()));
        if (comp.isVisible())
        {
            const auto& st = engine.strip (index);
            const auto gr = st.compMode.load() != 0 && shownGr >= 0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (shownGr, 1)
                                                                       : juce::String (juce::roundToInt (comp.getValue()));
            const float rs = recoverySeconds ((int) recovery.getValue());
            drawKnobCaption (g, comp, "COMP " + (rs < 1.0f ? juce::String (rs, rs < 0.2f ? 1 : 2).trimCharactersAtStart ("0") : juce::String ((int) rs)) + "s", gr);
        }
        drawKnobCaption (g, echo, deskInfo (desk).sendName, sendText (echo.getValue()));
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
    // Lay the controls out at a scale, see how much room the fader gets, then make the fader
    // 3/5 of what it would have had at scale 1 and give the rest to the controls.
    auto full = getLocalBounds().reduced (6, 0);
    const int bottom = 20 + 12 + (skin->bakeliteKnobs ? 14 : 0);      // the dB readout, its gap, the panel's screw
    auto layoutAt = [&] (float s) -> int                                // returns the y where the fader starts
    {
        auto r = full;
        auto px = [s] (int v) { return juce::roundToInt ((float) v * s); };
        r.removeFromTop (28);
        if (chanVu.isVisible())   { chanVu.setBounds (r.removeFromTop (52)); r.removeFromTop (4); }
        else if (meter.isVisible()) { meter.setBounds (r.removeFromTop (72).withSizeKeepingCentre (34, 72)); r.removeFromTop (4); }
        nameLabel.setBounds (r.removeFromTop (22));

        if (skin->bakeliteKnobs)
        {
            const bool many = comp.isVisible();          // the TG has more rows: smaller knobs
            const int k = juce::jmin (r.getWidth() - 4, px (many ? 34 : 40)), gap = px (many ? 14 : 16);
            auto knobRow = [&] (DeskKnob& knob)
            {
                if (! knob.isVisible()) return juce::Rectangle<int>();
                r.removeFromTop (gap);
                auto row = r.removeFromTop (k);
                knob.setBounds (row.withSizeKeepingCentre (k, k));
                return row;
            };
            auto withSide = [&] (DeskKnob& knob, juce::Component* button, DeskKnob* small)
            {
                auto row = knobRow (knob);
                if (row.isEmpty()) return;
                knob.setBounds (row.removeFromLeft (k + 4).withSizeKeepingCentre (k, k));
                if (button != nullptr && small != nullptr)
                {
                    button->setBounds (row.removeFromTop (16).reduced (1, 0));
                    small->setBounds (row.withSizeKeepingCentre (px (24), px (24)));
                }
                else if (button != nullptr) button->setBounds (row.withSizeKeepingCentre (row.getWidth(), 18));
                else if (small != nullptr)  small->setBounds (row.withSizeKeepingCentre (px (28), px (28)));
            };
            if (bandPanel != nullptr && bandPanel->isVisible())
            {
                const int kk = juce::jmin (r.getWidth() - 4, px (30)), gg = px (13);
                auto smallRow = [&] (DeskKnob& knob)
                {
                    r.removeFromTop (gg);
                    knob.setBounds (r.removeFromTop (kk).withSizeKeepingCentre (kk, kk));
                };
                smallRow (input);
                r.removeFromTop (4);
                bandPanel->setScale (s);
                bandPanel->setBounds (r.removeFromTop (bandPanel->preferredHeight()));
                if (dynPanel->isVisible()) { dynPanel->setScale (s); dynPanel->setBounds (r.removeFromTop (dynPanel->preferredHeight())); }
                smallRow (echo);
                smallRow (pan);
                r.removeFromTop (8);
            }
            else
            {
                knobRow (input);
                withSide (treble, trebleMode.isVisible() ? &trebleMode : nullptr, nullptr);
                withSide (presence, nullptr, &presenceFreq);
                knobRow (bass);
                withSide (comp, &compMode, &recovery);
                knobRow (echo);
                knobRow (pan);
                r.removeFromTop (8);
            }
        }
        else
        {
            r.removeFromTop (px (6));
            pan.setBounds (r.removeFromTop (px (44)).withSizeKeepingCentre (px (44), px (44)));
            r.removeFromTop (px (16));
        }

        auto ms = r.removeFromTop (22);
        mute.setBounds (ms.removeFromLeft (ms.getWidth() / 2 - 1));
        ms.removeFromLeft (2);
        solo.setBounds (ms);
        r.removeFromTop (8);
        return r.getY();
    };

    const int faderTop1 = layoutAt (1.0f);
    const int faderLen1 = full.getBottom() - bottom - faderTop1;
    const int controls1 = faderTop1 - full.getY();
    const int target = juce::roundToInt ((float) faderLen1 * 0.6f);        // 3/5 the length
    const float s = juce::jlimit (1.0f, 1.45f, (float) (controls1 + (faderLen1 - target)) / (float) controls1);
    const int faderTop = layoutAt (s);

    auto r = full.withTop (faderTop);
    r.removeFromBottom (skin->bakeliteKnobs ? 14 : 0);
    dbLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (12);
    // what's left over below the controls goes above the fader, so the faders line up along the bottom
    const int len = juce::jmax (target, juce::jmin (r.getHeight(), target));
    fader.setBounds (r.withTop (r.getBottom() - len));
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

    mainPresence.setRange (-10.0, 10.0, 0.5);
    mainPresence.setDoubleClickReturnValue (true, 0.0);
    mainPresence.setTooltip ("PRESENCE on the main outputs: a bell at the frequency on the small switch, or a 10 kHz shelf (double-click for flat)");
    mainPresence.onValueChange = [this] { engine.mainPresenceDb = (float) mainPresence.getValue(); repaint(); if (onChanged) onChanged(); };
    addChildComponent (mainPresence);
    mainPresenceFreq.setRange (0.0, numPresenceFreqs - 1, 1.0);
    mainPresenceFreq.setDoubleClickReturnValue (true, 4.0);
    mainPresenceFreq.setTooltip ("Presence frequency on the mains");
    mainPresenceFreq.onValueChange = [this] { engine.mainPresenceFreq = (int) mainPresenceFreq.getValue(); repaint(); if (onChanged) onChanged(); };
    addChildComponent (mainPresenceFreq);

    spread.setRange (0.0, 2.0, 0.05);
    spread.setDoubleClickReturnValue (true, 1.0);
    spread.setTooltip ("SPREAD: the width of the stereo mix - 0 mono, the middle as panned, right of it wider (double-click for as panned)");
    spread.onValueChange = [this] { engine.spread = (float) spread.getValue(); repaint(); if (onChanged) onChanged(); };
    addChildComponent (spread);

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
    meter.setVisible (false);         // every desk reads its mix on a pair of VUs
    vuL.setVisible (true);
    vuR.setVisible (true);
    echoReturn.setVisible (info.hasEcho);
    echoReturn.setSkin (skin);
    spread.setVisible (info.hasSpread);
    spread.setSkin (skin);
    mainPresence.setVisible (info.hasMainPresence);
    mainPresenceFreq.setVisible (info.hasMainPresence);
    mainPresence.setSkin (skin);
    mainPresenceFreq.setSkin (skin);
    spread.setTooltip (desk == Desk::redd51 ? "SPREADER: the width of the stereo mix - 0 mono, the middle as panned, right of it wider (double-click for as panned)"
                                            : "SPREAD: the width of the stereo mix - 0 mono, the middle as panned, right of it wider (double-click for as panned)");
    fader.setSkin (skin);
    dbLabel.setColour (juce::Label::textColourId, skin->bakeliteKnobs ? skin->accent : Pequod::Colours::text);
    resized();
    repaint();
}

void MasterStrip::refreshFromEngine()
{
    fader.setValue (engine.masterDb.load(), juce::dontSendNotification);
    echoReturn.setValue (engine.echoReturnDb.load(), juce::dontSendNotification);
    spread.setValue (engine.spread.load(), juce::dontSendNotification);
    mainPresence.setValue (engine.mainPresenceDb.load(), juce::dontSendNotification);
    mainPresenceFreq.setValue (engine.mainPresenceFreq.load(), juce::dontSendNotification);
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
    if (! corrArea.isEmpty())
    {
        const float t = engine.correlation.load();
        const float target = t > 1.5f ? 1.0f : juce::jlimit (-1.0f, 1.0f, t);   // at rest the needle sits at +1
        shownCorr += (target - shownCorr) * 0.2f;
        repaint (corrArea);
    }
}

void MasterStrip::mouseDown (const juce::MouseEvent& e)
{
    if (lampArea.contains (e.getPosition())) { engine.masterClipped = false; clipped = false; repaint (lampArea); }
    else if (plateArea.contains (e.getPosition()) && onPickDesk) onPickDesk();
}

void MasterStrip::mouseMove (const juce::MouseEvent& e)
{
    const bool over = plateArea.contains (e.getPosition());
    if (over != plateHover)
    {
        plateHover = over;
        setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        setTooltip (over ? "Change desk: pick the console the tracks are mixed on" : "");
        repaint (plateArea.expanded (2));
    }
}

void MasterStrip::mouseExit (const juce::MouseEvent&)
{
    if (plateHover) { plateHover = false; setMouseCursor (juce::MouseCursor::NormalCursor); setTooltip (""); repaint (plateArea.expanded (2)); }
}

void MasterStrip::paintPlateCue (juce::Graphics& g, juce::Rectangle<float> plate)
{
    // a small chevron at the right of the plate says it opens a choice; hovering lights the plate's edge
    const float cx = plate.getRight() - 12.0f, cy = plate.getCentreY();
    juce::Path chev;
    chev.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (plateHover ? skin->accent : skin->accent.withAlpha (0.55f));
    g.strokePath (chev, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (plateHover)
    {
        g.setColour (skin->accent.withAlpha (0.8f));
        g.drawRoundedRectangle (plate.expanded (1.0f), 3.0f, 1.2f);
    }
}

void MasterStrip::paintCorrelator (juce::Graphics& g)
{
    // a small moving-coil meter: -1 (out of phase) on the left, 0, +1 (mono) on the right
    auto r = corrArea.toFloat();
    g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (r, 3.0f);
    auto face = r.reduced (4.0f);
    g.setColour (juce::Colour (0xfff1e6c8));
    g.fillRoundedRectangle (face, 2.0f);
    const float arcR = face.getHeight() * 0.95f;
    const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.12f);
    const float span = juce::jmin (1.0f, std::asin (juce::jmin (0.95f, face.getWidth() * 0.42f / arcR)));
    const float radius = arcR;
    auto at = [&] (float v, float rad) { const float a = v * span; return pivot + juce::Point<float> (std::sin (a), -std::cos (a)) * rad; };
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0.0f, -span, span, true);
    g.setColour (juce::Colour (0xff2a2a2a));
    g.strokePath (arc, juce::PathStrokeType (1.0f));
    juce::Path red;
    red.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0.0f, -span, 0.0f, true);
    g.setColour (juce::Colour (0xffc0392b));
    g.strokePath (red, juce::PathStrokeType (2.5f));
    g.setColour (juce::Colour (0xff2a2a2a));
    g.setFont (Pequod::heading (8.5f, false));
    for (float v : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
    {
        g.drawLine (juce::Line<float> (at (v, arcR * 0.92f), at (v, arcR * 1.06f)), 1.0f);
        if (v == -1.0f || v == 0.0f || v == 1.0f)
        {
            const auto p = at (v, arcR * 0.74f);
            g.drawText (v == 0.0f ? juce::String ("0") : (v < 0 ? juce::String ("-1") : juce::String ("+1")),
                        juce::Rectangle<float> (p.x - 10.0f, p.y - 5.0f, 20.0f, 10.0f), juce::Justification::centred);
        }
    }
    g.setFont (Pequod::heading (7.0f, false));
    g.drawText ("PHASE", face.withTrimmedTop (face.getHeight() * 0.68f), juce::Justification::centred);
    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    g.setColour (juce::Colour (0xff111111));
    g.drawLine (juce::Line<float> (at (shownCorr, radius * 0.25f), at (shownCorr, radius * 1.08f)), 1.3f);
    g.restoreState();
}

void MasterStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    drawPanel (g, r, *skin, true);
    const auto& info = deskInfo (desk);

    if (skin->bakeliteKnobs)
    {
        // an engraved maker's plate
        auto plate = plateArea.toFloat();
        g.setColour (plateHover ? juce::Colour (0xff252825) : juce::Colour (0xff1b1d1b));
        g.fillRoundedRectangle (plate, 2.0f);
        paintPlateCue (g, plate);
        g.setColour (skin->accent);
        g.setFont (Pequod::heading (17.0f));
        g.drawText (isEmi (desk) ? juce::String ("EMI  ") + info.badge : juce::String (info.name).toUpperCase(), plate.withTrimmedBottom (12.0f), juce::Justification::centred);
        g.setFont (Pequod::heading (8.5f, false));
        g.setColour (skin->engravingDim);
        g.setColour (juce::Colour (0xffcfcab8));
        g.drawText (juce::String (skin->plateLine).replace ("  -  ", juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))), plate.withTrimmedTop (21.0f), juce::Justification::centred);

        g.setFont (Pequod::heading (10.0f));
        g.setColour (skin->engraving);
        auto knobLabel = [&] (const DeskKnob& k, const juce::String& title, const juce::String& value)
        {
            if (! k.isVisible()) return;
            auto b = k.getBounds().toFloat();
            g.setFont (Pequod::heading (10.0f));
            g.setColour (skin->engraving);
            g.drawText (title, juce::Rectangle<float> (b.getX() - 76.0f, b.getY(), 72.0f, b.getHeight()), juce::Justification::centredRight);
            g.setColour (skin->panelTop.getPerceivedBrightness() > 0.6f ? skin->engraving : skin->accent);
            g.setFont (Pequod::heading (10.0f, false));
            g.drawText (value, juce::Rectangle<float> (b.getRight() + 4.0f, b.getY(), 50.0f, b.getHeight()), juce::Justification::centredLeft);
        };
        knobLabel (echoReturn, juce::String (info.sendName) + " RETURN", sendText (echoReturn.getValue()));
        {
            const float sp = (float) spread.getValue();
            knobLabel (spread, desk == Desk::redd51 ? "SPREADER" : "SPREAD", sp < 0.025f ? juce::String ("MONO") : juce::String (juce::roundToInt (sp * 100.0f)) + "%");
        }
        if (mainPresence.isVisible())
        {
            auto b = mainPresence.getBounds().toFloat();
            g.setFont (Pequod::heading (10.0f));
            g.setColour (skin->engraving);
            g.drawText ("PRESENCE", juce::Rectangle<float> (b.getX() - 76.0f, b.getY(), 72.0f, b.getHeight() * 0.5f), juce::Justification::centredRight);
            g.setFont (Pequod::heading (10.0f, false));
            g.drawText (signedDb (mainPresence.getValue()) + "  " + presenceLabel ((int) mainPresenceFreq.getValue()),
                        juce::Rectangle<float> (b.getX() - 76.0f, b.getCentreY(), 72.0f, b.getHeight() * 0.5f), juce::Justification::centredRight);
        }
        g.setFont (Pequod::heading (11.0f));
        g.setColour (skin->engraving);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("MAIN  \xe2\x86\x92  2-TRACK")),
                    juce::Rectangle<float> (r.getX(), (float) fader.getY() - 20.0f, r.getWidth(), 14.0f), juce::Justification::centred);
    }
    else
    {
        auto plate = plateArea.toFloat();
        g.setColour (plateHover ? skin->well.brighter (0.15f) : skin->well);
        g.fillRoundedRectangle (plate, 3.0f);
        paintPlateCue (g, plate);
        g.setFont (Pequod::heading (15.0f));
        g.setColour (skin->accent);
        g.drawText (juce::String (info.badge) + "  MASTER", plate.withHeight (22.0f).translated (0.0f, 2.0f), juce::Justification::centred);
        g.setFont (Pequod::body (10.5f));
        g.setColour (skin->engravingDim);
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x86\x92 HAWTHORNE 2-TRACK"), plate.withTrimmedTop (21.0f), juce::Justification::centred);
    }

    if (! corrArea.isEmpty()) paintCorrelator (g);

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
    plateArea = skin->bakeliteKnobs ? getLocalBounds().reduced (16, 0).withTop (18).withHeight (34)
                                    : getLocalBounds().reduced (16, 0).withTop (8).withHeight (38);
    r.removeFromTop (skin->bakeliteKnobs ? 62 : 54);
    auto meters = r.removeFromTop (110);
    vuL.setBounds (meters.removeFromLeft (meters.getWidth() / 2 - 3));
    meters.removeFromLeft (6);
    vuR.setBounds (meters);
    r.removeFromTop (10);
    if (! skin->bakeliteKnobs)
    {
        lampArea = r.removeFromTop (20).withSizeKeepingCentre (54, 20);
        r.removeFromTop (8);
    }
    else lampArea = {};      // the modelled desks' main amps round off before they clip: no lamp
    // the REDD desks had a stereo correlator between their VUs (MusicTech, Studio Icons: EMI REDD.51)
    if (desk == Desk::redd51 || desk == Desk::redd37)
    {
        corrArea = r.removeFromTop (62).withSizeKeepingCentre (juce::jmin (r.getWidth(), 170), 62);
        r.removeFromTop (8);
    }
    else corrArea = {};
    if (echoReturn.isVisible())
        echoReturn.setBounds (r.removeFromTop (44).withSizeKeepingCentre (44, 44));
    if (spread.isVisible())
    {
        r.removeFromTop (4);
        spread.setBounds (r.removeFromTop (44).withSizeKeepingCentre (44, 44));
    }
    if (mainPresence.isVisible())
    {
        r.removeFromTop (4);
        auto row = r.removeFromTop (44).withSizeKeepingCentre (44 + 6 + 28, 44);
        mainPresence.setBounds (row.removeFromLeft (44));
        mainPresenceFreq.setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
    }
    r.removeFromTop (30);
    if (skin->bakeliteKnobs) r.removeFromBottom (14);
    dbLabel.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (8);
    // the same 3/5 length as the channels' faders, along the bottom
    const int len = juce::roundToInt ((float) getHeight() * 0.36f);
    fader.setBounds (r.withTop (r.getBottom() - juce::jmin (r.getHeight(), len)).withSizeKeepingCentre (96, juce::jmin (r.getHeight(), len)));
}

} // namespace whitman
