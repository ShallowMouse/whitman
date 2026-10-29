#pragma once

#include <juce_core/juce_core.h>
#include "MixEngine.h"

namespace whitman
{

/** The mix as a file: every strip's name, fader, pan, mute and solo, and the master.
    WHITMAN keeps the current mix itself between runs; SAVE / OPEN write and read .whitman files. */
struct MixState
{
    static constexpr const char* fileExtension = ".whitman";

    static std::unique_ptr<juce::XmlElement> toXml (const MixEngine& e, const juce::StringArray& names);
    /** Returns false (and changes nothing) if the XML isn't a WHITMAN mix. */
    static bool fromXml (const juce::XmlElement& x, MixEngine& e, juce::StringArray& names);

    /** Everything back to unity, centre, nothing muted or soloed. */
    static void reset (MixEngine& e, juce::StringArray& names);

    static juce::String defaultName (int strip) { return juce::String (strip + 1); }
};

} // namespace whitman
