#pragma once

#include "Console.h"

namespace whitman
{

//==============================================================================
/** Pick the desk: a card for each, with a little of its face, its story and its sound
    (the same idea as Hawthorne's machine picker). */
class DeskPicker : public juce::Component
{
public:
    DeskPicker (Desk current, std::function<void (Desk)> onPick) : chosen (current), pick (std::move (onPick))
    {
        for (int i = 0; i < numDesks; ++i)
            addAndMakeVisible (cards.add (new Card (*this, deskFromInt (i))));
        setSize (760, 40 + numDesks * 236);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Pequod::Colours::bg);
        g.setColour (Pequod::Colours::mustard);
        g.setFont (Pequod::heading (15.0f));
        g.drawText ("DESK  -  the console the tracks are mixed on", 14, 8, getWidth() - 28, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        int y = 40;
        // oldest first, the clean desk last
        for (auto d : { Desk::redd51, Desk::whitman })
            for (auto* c : cards)
                if (c->desk == d) { c->setBounds (10, y, getWidth() - 20, 224); y += 236; }
    }

private:
    struct Card : juce::Component
    {
        Card (DeskPicker& o, Desk d) : owner (o), desk (d) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            const bool isChosen = owner.chosen == desk;
            const auto& info = deskInfo (desk);
            const auto& skin = DeskSkin::of (desk);
            auto r = getLocalBounds().toFloat();
            g.setColour (Pequod::Colours::panel);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (isChosen ? Pequod::Colours::mustard : (isMouseOver() ? Pequod::Colours::line.brighter (0.4f) : Pequod::Colours::line));
            g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, isChosen ? 2.5f : 1.0f);

            auto a = getLocalBounds().reduced (12);
            drawPreview (g, a.removeFromLeft (190).toFloat(), skin);
            a.removeFromLeft (14);
            auto title = a.removeFromTop (22);
            g.setColour (Pequod::Colours::text);
            g.setFont (Pequod::body (16.0f, true));
            g.drawText (info.name, title, juce::Justification::centredLeft);
            if (isChosen)
            {
                g.setColour (Pequod::Colours::mustard);
                g.setFont (Pequod::heading (11.0f));
                g.drawText ("IN USE", title, juce::Justification::centredRight);
            }
            g.setColour (Pequod::Colours::textDim);
            g.setFont (Pequod::heading (11.0f));
            g.drawFittedText (info.maker, a.removeFromTop (16), juce::Justification::centredLeft, 1, 0.8f);
            a.removeFromTop (6);
            g.setColour (Pequod::Colours::text.withAlpha (0.9f));
            g.setFont (Pequod::body (13.0f));
            g.drawFittedText (info.history, a.removeFromTop (100), juce::Justification::topLeft, 6, 1.0f);
            g.setColour (Pequod::Colours::mustard.brighter (0.2f));
            g.setFont (Pequod::body (13.0f, true));
            g.drawFittedText (juce::String ("Sound: ") + info.sound, a, juce::Justification::topLeft, 4, 1.0f);
        }

        /** A few strips of the desk's face. */
        static void drawPreview (juce::Graphics& g, juce::Rectangle<float> r, const DeskSkin& s)
        {
            g.setColour (s.frame);
            g.fillRoundedRectangle (r, 4.0f);
            r.reduce (6.0f, 6.0f);
            const int n = 5;
            const float w = r.getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                auto strip = r.withX (r.getX() + w * (float) i).withWidth (w).reduced (1.5f, 0.0f);
                g.setGradientFill (juce::ColourGradient (s.panelTop, strip.getX(), strip.getY(), s.panelBottom, strip.getX(), strip.getBottom(), false));
                g.fillRect (strip);
                g.setColour (s.well);
                g.fillRect (strip.withHeight (9.0f).translated (0.0f, 8.0f).reduced (4.0f, 0.0f));
                const int knobs = s.bakeliteKnobs ? 5 : 1;
                for (int k = 0; k < knobs; ++k)
                {
                    const auto c = juce::Point<float> (strip.getCentreX(), strip.getY() + 30.0f + (float) k * 21.0f);
                    g.setColour (s.bakeliteKnobs ? juce::Colour (0xff0d0d0d) : juce::Colour (0xff2a2c30));
                    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (c));
                    g.setColour (s.bakeliteKnobs ? juce::Colour (0xfff2eee0) : s.accent);
                    const float a = -0.8f + (float) ((i * 3 + k * 5) % 7) * 0.25f;
                    g.drawLine (c.x, c.y, c.x + std::sin (a) * 6.0f, c.y - std::cos (a) * 6.0f, 1.5f);
                }
                // fader
                const float top = strip.getY() + (s.bakeliteKnobs ? 135.0f : 60.0f), bottom = strip.getBottom() - 6.0f;
                g.setColour (s.quadrantFaders ? juce::Colour (0xff8e918c) : Pequod::Colours::rail);
                g.fillRoundedRectangle (juce::Rectangle<float> (s.quadrantFaders ? 8.0f : 3.0f, bottom - top).withCentre ({ strip.getCentreX(), (top + bottom) * 0.5f }), 2.0f);
                const float y = top + (bottom - top) * (0.2f + 0.12f * (float) ((i * 7) % 5));
                g.setColour (s.quadrantFaders ? juce::Colour (0xff0d0d0d) : juce::Colour (0xff4a4d52));
                g.fillRoundedRectangle (juce::Rectangle<float> (strip.getWidth() * 0.6f, 9.0f).withCentre ({ strip.getCentreX(), y }), 2.0f);
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            owner.chosen = desk;
            if (owner.pick) owner.pick (desk);
            for (auto* c : owner.cards) c->repaint();
        }

        DeskPicker& owner;
        const Desk desk;
    };

    Desk chosen;
    std::function<void (Desk)> pick;
    juce::OwnedArray<Card> cards;
};

} // namespace whitman
