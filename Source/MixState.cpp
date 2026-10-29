#include "MixState.h"

namespace whitman
{

std::unique_ptr<juce::XmlElement> MixState::toXml (const MixEngine& e, const juce::StringArray& names)
{
    auto x = std::make_unique<juce::XmlElement> ("WHITMAN_MIX");
    x->setAttribute ("version", 1);
    x->setAttribute ("master", (double) e.masterDb.load());
    x->setAttribute ("desk", e.desk.load());
    x->setAttribute ("echoReturn", (double) e.echoReturnDb.load());
    for (int i = 0; i < maxStrips; ++i)
    {
        auto& s = e.strip (i);
        const auto name = names[i];
        const bool isDefault = (name.isEmpty() || name == defaultName (i))
                               && s.gainDb.load() == 0.0f && s.pan.load() == 0.0f && ! s.mute.load() && ! s.solo.load()
                               && s.inputDb.load() == 0.0f && s.bassDb.load() == 0.0f && s.trebleDb.load() == 0.0f
                               && ! s.classic.load() && s.echoDb.load() <= echoMinDb;
        if (isDefault) continue;   // keep files small: only strips that differ from a fresh one
        auto* c = x->createNewChildElement ("STRIP");
        c->setAttribute ("index", i);
        c->setAttribute ("name", name);
        c->setAttribute ("gain", (double) s.gainDb.load());
        c->setAttribute ("pan", (double) s.pan.load());
        c->setAttribute ("mute", s.mute.load());
        c->setAttribute ("solo", s.solo.load());
        c->setAttribute ("input", (double) s.inputDb.load());
        c->setAttribute ("bass", (double) s.bassDb.load());
        c->setAttribute ("treble", (double) s.trebleDb.load());
        c->setAttribute ("classic", s.classic.load());
        c->setAttribute ("echo", (double) s.echoDb.load());
    }
    return x;
}

void MixState::reset (MixEngine& e, juce::StringArray& names)
{
    names.clear();
    for (int i = 0; i < maxStrips; ++i)
    {
        names.add (defaultName (i));
        auto& s = e.strip (i);
        s.gainDb = 0.0f;
        s.pan = 0.0f;
        s.mute = false;
        s.solo = false;
        s.inputDb = 0.0f;
        s.bassDb = 0.0f;
        s.trebleDb = 0.0f;
        s.classic = false;
        s.echoDb = echoMinDb;
    }
    e.masterDb = 0.0f;
    e.echoReturnDb = -6.0f;
}

bool MixState::fromXml (const juce::XmlElement& x, MixEngine& e, juce::StringArray& names)
{
    if (! x.hasTagName ("WHITMAN_MIX")) return false;
    reset (e, names);
    e.masterDb = (float) juce::jlimit ((double) faderMinDb, (double) faderMaxDb, x.getDoubleAttribute ("master", 0.0));
    e.desk = (int) deskFromInt (x.getIntAttribute ("desk", (int) Desk::whitman));
    e.echoReturnDb = (float) juce::jlimit ((double) echoMinDb, 10.0, x.getDoubleAttribute ("echoReturn", -6.0));
    for (auto* c : x.getChildWithTagNameIterator ("STRIP"))
    {
        const int i = c->getIntAttribute ("index", -1);
        if (! juce::isPositiveAndBelow (i, maxStrips)) continue;
        auto& s = e.strip (i);
        const auto name = c->getStringAttribute ("name").trim();
        names.set (i, name.isEmpty() ? defaultName (i) : name.substring (0, 24));
        s.gainDb = (float) juce::jlimit ((double) faderMinDb, (double) faderMaxDb, c->getDoubleAttribute ("gain", 0.0));
        s.pan    = (float) juce::jlimit (-1.0, 1.0, c->getDoubleAttribute ("pan", 0.0));
        s.mute   = c->getBoolAttribute ("mute", false);
        s.solo   = c->getBoolAttribute ("solo", false);
        s.inputDb  = (float) juce::jlimit (-10.0, 20.0, c->getDoubleAttribute ("input", 0.0));
        s.bassDb   = (float) juce::jlimit (-10.0, 10.0, c->getDoubleAttribute ("bass", 0.0));
        s.trebleDb = (float) juce::jlimit (-10.0, 10.0, c->getDoubleAttribute ("treble", 0.0));
        s.classic  = c->getBoolAttribute ("classic", false);
        s.echoDb   = (float) juce::jlimit ((double) echoMinDb, 10.0, c->getDoubleAttribute ("echo", echoMinDb));
    }
    return true;
}

} // namespace whitman
