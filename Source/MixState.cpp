#include "MixState.h"

namespace whitman
{

static bool bandEqIsDefault (const BandEqParams& p, const BandEqDef& def)
{
    for (int b = 0; b < BandEqParams::maxBands; ++b)
    {
        const auto i = (size_t) b;
        const float f0 = b < (int) def.bands.size() ? def.bands[i].defaultFreq() : 0.5f;
        if (p.gain[i].load() != 0.0f || p.freq[i].load() != f0 || p.q[i].load() != 0.5f || p.alt[i].load()) return false;
    }
    return p.hpfPos.load() == 0.0f && p.lpfPos.load() == 0.0f && p.hpfMask.load() == 0 && p.lpfMask.load() == 0;
}

static bool sslIsDefault (const MixEngine::Strip& s)
{
    return s.sslRatio.load() == 1.0f && s.sslThresh.load() == 10.0f && s.sslRelease.load() == 0.3f && ! s.sslFast.load()
        && s.gateRange.load() == 0.0f && s.gateThresh.load() == -30.0f && s.gateRelease.load() == 0.3f && ! s.gateFast.load() && ! s.gateMode.load();
}

static void resetSsl (MixEngine::Strip& s)
{
    s.sslRatio = 1.0f; s.sslThresh = 10.0f; s.sslRelease = 0.3f; s.sslFast = false;
    s.gateRange = 0.0f; s.gateThresh = -30.0f; s.gateRelease = 0.3f; s.gateFast = false; s.gateMode = false;
}

std::unique_ptr<juce::XmlElement> MixState::toXml (const MixEngine& e, const juce::StringArray& names)
{
    auto x = std::make_unique<juce::XmlElement> ("WHITMAN_MIX");
    x->setAttribute ("version", 1);
    x->setAttribute ("master", (double) e.masterDb.load());
    x->setAttribute ("desk", e.desk.load());
    x->setAttribute ("echoReturn", (double) e.echoReturnDb.load());
    x->setAttribute ("spread", (double) e.spread.load());
    x->setAttribute ("mainPresence", (double) e.mainPresenceDb.load());
    x->setAttribute ("mainPresenceFreq", e.mainPresenceFreq.load());
    for (int i = 0; i < maxStrips; ++i)
    {
        auto& s = e.strip (i);
        const auto name = names[i];
        const bool isDefault = (name.isEmpty() || name == defaultName (i))
                               && s.gainDb.load() == 0.0f && s.pan.load() == 0.0f && ! s.mute.load() && ! s.solo.load()
                               && s.inputDb.load() == 0.0f && s.bassDb.load() == 0.0f && s.trebleDb.load() == 0.0f
                               && ! s.classic.load() && s.echoDb.load() <= echoMinDb
                               && s.presenceDb.load() == 0.0f && s.presenceFreq.load() == 4 && s.compMode.load() == 0
                               && s.compDb.load() == 0.0f && s.recovery.load() == 2
                               && sslIsDefault (s)
                               && [&] { for (int k = 0; k < numBandEqDesks; ++k) if (! bandEqIsDefault (s.eqs[(size_t) k], *bandEqOf (bandEqDesk (k)))) return false; return true; }();
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
        c->setAttribute ("presence", (double) s.presenceDb.load());
        c->setAttribute ("presenceFreq", s.presenceFreq.load());
        c->setAttribute ("comp", s.compMode.load());
        c->setAttribute ("compress", (double) s.compDb.load());
        c->setAttribute ("recovery", s.recovery.load());
        if (! sslIsDefault (s))
        {
            auto* d = c->createNewChildElement ("SSL_DYNAMICS");
            d->setAttribute ("ratio", (double) s.sslRatio.load());
            d->setAttribute ("threshold", (double) s.sslThresh.load());
            d->setAttribute ("release", (double) s.sslRelease.load());
            d->setAttribute ("fast", s.sslFast.load());
            d->setAttribute ("gateRange", (double) s.gateRange.load());
            d->setAttribute ("gateThreshold", (double) s.gateThresh.load());
            d->setAttribute ("gateRelease", (double) s.gateRelease.load());
            d->setAttribute ("gateFast", s.gateFast.load());
            d->setAttribute ("gate", s.gateMode.load());
        }
        for (int k = 0; k < numBandEqDesks; ++k)
        {
            const auto& p = s.eqs[(size_t) k];
            const auto* def = bandEqOf (bandEqDesk (k));
            if (bandEqIsDefault (p, *def)) continue;
            auto* q = c->createNewChildElement ("BAND_EQ");
            q->setAttribute ("desk", (int) bandEqDesk (k));
            for (int b = 0; b < BandEqParams::maxBands; ++b)
            {
                const auto i = (size_t) b;
                q->setAttribute ("gain" + juce::String (b), (double) p.gain[i].load());
                q->setAttribute ("freq" + juce::String (b), (double) p.freq[i].load());
                q->setAttribute ("q" + juce::String (b), (double) p.q[i].load());
                q->setAttribute ("alt" + juce::String (b), p.alt[i].load());
            }
            q->setAttribute ("hpf", (double) p.hpfPos.load());
            q->setAttribute ("lpf", (double) p.lpfPos.load());
            q->setAttribute ("hpfMask", p.hpfMask.load());
            q->setAttribute ("lpfMask", p.lpfMask.load());
        }
    }
    return x;
}

void MixState::reset (MixEngine& e, juce::StringArray& names)
{
    names.clear();
    for (int i = 0; i < maxStrips; ++i)
    {
        names.add ({});     // not named here: the strip shows Hawthorne's name for the track, or its number
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
        s.presenceDb = 0.0f;
        s.presenceFreq = 4;
        s.compMode = 0;
        s.compDb = 0.0f;
        s.recovery = 2;
        resetSsl (s);
        for (int k = 0; k < numBandEqDesks; ++k) s.eqs[(size_t) k].reset (bandEqOf (bandEqDesk (k)));
    }
    e.masterDb = 0.0f;
    e.echoReturnDb = -6.0f;
    e.spread = 1.0f;
    e.mainPresenceDb = 0.0f;
    e.mainPresenceFreq = 4;
}

bool MixState::fromXml (const juce::XmlElement& x, MixEngine& e, juce::StringArray& names)
{
    if (! x.hasTagName ("WHITMAN_MIX")) return false;
    reset (e, names);
    e.masterDb = (float) juce::jlimit ((double) faderMinDb, (double) faderMaxDb, x.getDoubleAttribute ("master", 0.0));
    e.desk = (int) deskFromInt (x.getIntAttribute ("desk", (int) Desk::whitman));
    e.echoReturnDb = (float) juce::jlimit ((double) echoMinDb, 10.0, x.getDoubleAttribute ("echoReturn", -6.0));
    e.spread = (float) juce::jlimit (0.0, 2.0, x.getDoubleAttribute ("spread", 1.0));
    e.mainPresenceDb = (float) juce::jlimit (-10.0, 10.0, x.getDoubleAttribute ("mainPresence", 0.0));
    e.mainPresenceFreq = juce::jlimit (0, numPresenceFreqs - 1, x.getIntAttribute ("mainPresenceFreq", 4));
    for (auto* c : x.getChildWithTagNameIterator ("STRIP"))
    {
        const int i = c->getIntAttribute ("index", -1);
        if (! juce::isPositiveAndBelow (i, maxStrips)) continue;
        auto& s = e.strip (i);
        const auto name = c->getStringAttribute ("name").trim();
        names.set (i, name == defaultName (i) ? juce::String() : name.substring (0, 24));
        s.gainDb = (float) juce::jlimit ((double) faderMinDb, (double) faderMaxDb, c->getDoubleAttribute ("gain", 0.0));
        s.pan    = (float) juce::jlimit (-1.0, 1.0, c->getDoubleAttribute ("pan", 0.0));
        s.mute   = c->getBoolAttribute ("mute", false);
        s.solo   = c->getBoolAttribute ("solo", false);
        s.inputDb  = (float) juce::jlimit (-10.0, 20.0, c->getDoubleAttribute ("input", 0.0));
        s.bassDb   = (float) juce::jlimit (-10.0, 10.0, c->getDoubleAttribute ("bass", 0.0));
        s.trebleDb = (float) juce::jlimit (-10.0, 10.0, c->getDoubleAttribute ("treble", 0.0));
        s.classic  = c->getBoolAttribute ("classic", false);
        s.echoDb   = (float) juce::jlimit ((double) echoMinDb, 10.0, c->getDoubleAttribute ("echo", echoMinDb));
        s.presenceDb   = (float) juce::jlimit (-10.0, 10.0, c->getDoubleAttribute ("presence", 0.0));
        s.presenceFreq = juce::jlimit (0, numPresenceFreqs - 1, c->getIntAttribute ("presenceFreq", 4));
        s.compMode     = juce::jlimit (0, 2, c->getIntAttribute ("comp", 0));
        s.compDb       = (float) juce::jlimit (0.0, 24.0, c->getDoubleAttribute ("compress", 0.0));
        s.recovery     = juce::jlimit (0, numRecoveries - 1, c->getIntAttribute ("recovery", 2));
        if (auto* d = c->getChildByName ("SSL_DYNAMICS"))
        {
            s.sslRatio    = (float) juce::jlimit (1.0, 20.0, d->getDoubleAttribute ("ratio", 1.0));
            s.sslThresh   = (float) juce::jlimit (-20.0, 10.0, d->getDoubleAttribute ("threshold", 10.0));
            s.sslRelease  = (float) juce::jlimit (0.1, 4.0, d->getDoubleAttribute ("release", 0.3));
            s.sslFast     = d->getBoolAttribute ("fast", false);
            s.gateRange   = (float) juce::jlimit (0.0, 40.0, d->getDoubleAttribute ("gateRange", 0.0));
            s.gateThresh  = (float) juce::jlimit (-30.0, 10.0, d->getDoubleAttribute ("gateThreshold", -30.0));
            s.gateRelease = (float) juce::jlimit (0.1, 4.0, d->getDoubleAttribute ("gateRelease", 0.3));
            s.gateFast    = d->getBoolAttribute ("gateFast", false);
            s.gateMode    = d->getBoolAttribute ("gate", false);
        }
        for (auto* q : c->getChildWithTagNameIterator ("BAND_EQ"))
        {
            const int k = bandEqSlot (deskFromInt (q->getIntAttribute ("desk", -1)));
            if (k < 0) continue;
            auto& p = s.eqs[(size_t) k];
            const auto* def = bandEqOf (bandEqDesk (k));
            for (int b = 0; b < BandEqParams::maxBands; ++b)
            {
                const auto i = (size_t) b;
                const bool has = b < (int) def->bands.size();
                const float range = has ? juce::jmax (def->bands[i].maxDb, def->bands[i].altMaxDb) : 18.0f;
                const float fMax = has && ! def->bands[i].swept() ? (float) def->bands[i].numSteps() - 1.0f : 1.0f;
                p.gain[i] = (float) juce::jlimit ((double) -range, (double) range, q->getDoubleAttribute ("gain" + juce::String (b), 0.0));
                p.freq[i] = (float) juce::jlimit (0.0, (double) fMax, q->getDoubleAttribute ("freq" + juce::String (b), has ? def->bands[i].defaultFreq() : 0.5));
                p.q[i]    = (float) juce::jlimit (0.0, 1.0, q->getDoubleAttribute ("q" + juce::String (b), 0.5));
                p.alt[i]  = q->getBoolAttribute ("alt" + juce::String (b), false);
            }
            p.hpfPos  = (float) juce::jmax (0.0, q->getDoubleAttribute ("hpf", 0.0));
            p.lpfPos  = (float) juce::jmax (0.0, q->getDoubleAttribute ("lpf", 0.0));
            p.hpfMask = q->getIntAttribute ("hpfMask", 0) & 7;
            p.lpfMask = q->getIntAttribute ("lpfMask", 0) & 7;
        }
    }
    return true;
}

} // namespace whitman
