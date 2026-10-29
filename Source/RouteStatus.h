#pragma once

#include <juce_core/juce_core.h>

namespace whitman
{

/** A read-only look at Hawthorne Route's shared memory (Source/HawthorneRoute/RouteShared.h,
    a copy of Hawthorne's), just to tell the user whether Hawthorne is running and how
    many tracks its reel has. The audio itself goes through the ASIO driver. Windows only. */
class RouteStatus
{
public:
    RouteStatus() = default;
    ~RouteStatus();
    RouteStatus (const RouteStatus&) = delete;

    struct Info
    {
        bool hawthorneRunning = false;   // Hawthorne has Route open and its audio is running
        int  tracks = 0;                 // the reel's track count (0 if not running)
        int  sampleRate = 0;
    };

    /** Message thread; cheap enough to call from a timer. */
    Info poll();

private:
    void* mapping = nullptr;
    const void* view = nullptr;
    void close();
};

} // namespace whitman
