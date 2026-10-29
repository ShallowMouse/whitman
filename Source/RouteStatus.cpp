#include "RouteStatus.h"

#if JUCE_WINDOWS
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include "HawthorneRoute/RouteShared.h"
#endif

namespace whitman
{

RouteStatus::~RouteStatus() { close(); }

void RouteStatus::close()
{
   #if JUCE_WINDOWS
    if (view != nullptr)    UnmapViewOfFile (view);
    if (mapping != nullptr) CloseHandle ((HANDLE) mapping);
   #endif
    view = nullptr;
    mapping = nullptr;
}

RouteStatus::Info RouteStatus::poll()
{
    Info info;
   #if JUCE_WINDOWS
    using namespace hawthorne::route;
    if (view == nullptr)
    {
        // Hawthorne creates the mapping; until it (or a program using the driver) has, there's nothing to read
        mapping = OpenFileMappingW (FILE_MAP_READ, FALSE, mappingName);
        if (mapping == nullptr) return info;
        // read-only; only the header is ever looked at
        view = MapViewOfFile ((HANDLE) mapping, FILE_MAP_READ, 0, 0, 0);
        if (view == nullptr) { close(); return info; }
    }
    const auto* s = static_cast<const Shared*> (view);
    if (s->magic != magic || s->version != version) return info;
    info.sampleRate = s->sampleRate.load();
    const auto beat = s->heartbeatMs.load();
    info.hawthorneRunning = info.sampleRate > 0 && (std::int64_t) GetTickCount64() - beat < 500;
    info.tracks = info.hawthorneRunning ? tracksOf (s->numTracks.load()) : 0;
   #endif
    return info;
}

} // namespace whitman
