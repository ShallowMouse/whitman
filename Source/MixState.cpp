#include "MixState.h"

namespace whitman
{

std::unique_ptr<juce::XmlElement> MixState::toXml (const MixEngine& e, const juce::StringArray& names)
{
    auto x = std::make_unique<juce::XmlElement> ("WHITMAN_MIX");
    x->setAttribute ("version", 1);
    x->setAttribute ("master", (double) e.masterDb.load());
    for (int i = 0; i < maxStrips; ++i)
    {
        auto& s = e.strip (i);
        const auto name = names[i];
        const bool isDefault = (name.isEmpty() || name == defaultName (i))
                               && s.gainDb.load() == 0.0f && s.pan.load() == 0.0f && ! s.mute.load() && ! s.solo.load();
        if (isDefault) continue;   // keep files small: only strips that differ from a fresh one
        auto* c = x->createNewChildElement ("STRIP");
        c->setAttribute ("index", i);
        c->setAttribute ("name", name);
        c->setAttribute ("gain", (double) s.gainDb.load());
        c->setAttribute ("pan", (double) s.pan.load());
        c->setAttribute ("mute", s.mute.load());
        c->setAttribute ("solo", s.solo.load());
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
    }
    e.masterDb = 0.0f;
}

bool MixState::fromXml (const juce::XmlElement& x, MixEngine& e, juce::StringArray& names)
{
    if (! x.hasTagName ("WHITMAN_MIX")) return false;
    reset (e, names);
    e.masterDb = (float) juce::jlimit ((double) faderMinDb, (double) faderMaxDb, x.getDoubleAttribute ("master", 0.0));
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
    }
    return true;
}

} // namespace whitman
