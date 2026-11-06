#include "EmersonLinkDevice.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace whitman
{

namespace link = emerson::link;

static std::int64_t nowMs() { return (std::int64_t) juce::Time::getMillisecondCounterHiRes(); }

//==============================================================================
EmersonLinkDevice::EmersonLinkDevice()
    : AudioIODevice (deviceName, typeName), Thread ("EMERSON Link")
{
    connect();
    if (shared != nullptr && shared->sampleRate.load() > 0)
    {
        tracks = juce::jlimit (1, link::maxTracks, (int) shared->numTracks.load());
        sampleRate = shared->sampleRate.load();
        blockSize = juce::jlimit (16, link::maxBlock, (int) shared->blockSize.load());
    }
}

EmersonLinkDevice::~EmersonLinkDevice()
{
    close();
    disconnect();
}

void EmersonLinkDevice::connect()
{
#if JUCE_WINDOWS
    if (shared != nullptr) return;
    auto* m = OpenFileMappingW (FILE_MAP_ALL_ACCESS, FALSE, link::mappingName);
    if (m == nullptr) return;
    auto* v = MapViewOfFile (m, FILE_MAP_ALL_ACCESS, 0, 0, sizeof (link::Shared));
    if (v == nullptr) { CloseHandle (m); return; }
    auto* s = static_cast<link::Shared*> (v);
    if (s->magic != link::magic || s->version != link::version)
    {
        UnmapViewOfFile (v);
        CloseHandle (m);
        return;
    }
    mapping = m;
    shared = s;
    tick = OpenEventW (SYNCHRONIZE, FALSE, link::tickName);
    core.attach (shared);
#endif
}

void EmersonLinkDevice::disconnect()
{
#if JUCE_WINDOWS
    if (shared != nullptr) UnmapViewOfFile (shared);
    if (mapping != nullptr) CloseHandle ((HANDLE) mapping);
    if (tick != nullptr) CloseHandle ((HANDLE) tick);
#endif
    shared = nullptr;
    mapping = tick = nullptr;
    core.attach (nullptr);
}

juce::StringArray EmersonLinkDevice::inputNames (int n)
{
    juce::StringArray s;
    for (int t = 1; t <= n; ++t) s.add ("Hawthorne track " + juce::String (t));
    s.addArray ({ "EMERSON return A L", "EMERSON return A R", "EMERSON return B L", "EMERSON return B R" });
    return s;
}

juce::StringArray EmersonLinkDevice::outputNames()
{
    return { "To Hawthorne master L", "To Hawthorne master R",
             "To EMERSON send A L", "To EMERSON send A R", "To EMERSON send B L", "To EMERSON send B R" };
}

juce::StringArray EmersonLinkDevice::getInputChannelNames()  { return inputNames (tracks); }
juce::StringArray EmersonLinkDevice::getOutputChannelNames() { return outputNames(); }

juce::Array<double> EmersonLinkDevice::getAvailableSampleRates() { return { sampleRate }; }   // EMERSON's
juce::Array<int> EmersonLinkDevice::getAvailableBufferSizes()    { return { blockSize }; }
int EmersonLinkDevice::getDefaultBufferSize()                    { return blockSize; }

juce::String EmersonLinkDevice::open (const juce::BigInteger& ins, const juce::BigInteger& outs, double, int)
{
    close();
    connect();
    if (shared != nullptr && shared->sampleRate.load() > 0)
    {
        tracks = juce::jlimit (1, link::maxTracks, (int) shared->numTracks.load());
        sampleRate = shared->sampleRate.load();
        blockSize = juce::jlimit (16, link::maxBlock, (int) shared->blockSize.load());
    }

    const int numIn = tracks + 2 * link::numReturns, numOut = link::fromWhitmanChannels;
    activeIn = ins;  activeIn.setRange (numIn, 1024, false);
    activeOut = outs; activeOut.setRange (numOut, 1024, false);

    inChannelOf.clear();
    for (int c = 0; c < numIn; ++c)
        if (activeIn[c]) inChannelOf.push_back (c < tracks ? c : link::toReturnA + (c - tracks));
    outChannelOf.clear();
    for (int c = 0; c < numOut; ++c)
        if (activeOut[c]) outChannelOf.push_back (c);

    linkIn.setSize (link::toWhitmanChannels, link::maxBlock);
    linkOut.setSize (link::fromWhitmanChannels, link::maxBlock);
    devIn.assign (inChannelOf.size(), nullptr);
    devOut.assign (outChannelOf.size(), nullptr);
    opened = true;
    return {};
}

void EmersonLinkDevice::close()
{
    stop();
    opened = false;
}

void EmersonLinkDevice::start (juce::AudioIODeviceCallback* cb)
{
    if (! opened || cb == nullptr) return;
    stop();
    cb->audioDeviceAboutToStart (this);
    {
        const juce::ScopedLock sl (callbackLock);
        callback = cb;
    }
    if (shared != nullptr)
    {
        shared->clientId = shared->clientId.load() + 1;
        std::strncpy (shared->clientName, "WHITMAN", sizeof (shared->clientName) - 1);
        shared->clientRunning = 1;
    }
    core.resync();
    startThread (juce::Thread::Priority::highest);
}

void EmersonLinkDevice::stop()
{
    juce::AudioIODeviceCallback* old = nullptr;
    {
        const juce::ScopedLock sl (callbackLock);
        old = callback;
        callback = nullptr;
    }
    stopThread (2000);
    if (shared != nullptr) shared->clientRunning = 0;
    if (old != nullptr) old->audioDeviceStopped();
}

bool EmersonLinkDevice::isEmersonRunning() const
{
    return shared != nullptr && shared->sampleRate.load() > 0 && nowMs() - shared->heartbeatMs.load() < 1000;
}

bool EmersonLinkDevice::needsReopen() const
{
    if (! opened || shared == nullptr || shared->sampleRate.load() <= 0) return false;
    return shared->numTracks.load() != tracks
        || shared->sampleRate.load() != juce::roundToInt (sampleRate)
        || juce::jlimit (16, link::maxBlock, (int) shared->blockSize.load()) != blockSize;
}

void EmersonLinkDevice::deliver (const float* const* in, float* const* out, int n)
{
    for (size_t i = 0; i < devIn.size(); ++i)  devIn[i] = in[inChannelOf[i]];
    for (size_t i = 0; i < devOut.size(); ++i) devOut[i] = out[outChannelOf[i]];
    for (int c = 0; c < link::fromWhitmanChannels; ++c) juce::FloatVectorOperations::clear (out[c], n);

    const juce::ScopedLock sl (callbackLock);
    if (callback != nullptr)
        callback->audioDeviceIOCallbackWithContext (devIn.data(), (int) devIn.size(), devOut.data(), (int) devOut.size(), n, {});
}

void EmersonLinkDevice::run()
{
    auto* const* inP = linkIn.getArrayOfWritePointers();
    auto* const* outP = linkOut.getArrayOfWritePointers();
    std::int64_t lastSilence = 0;

    while (! threadShouldExit())
    {
#if JUCE_WINDOWS
        if (shared == nullptr)
        {
            connect();   // EMERSON started after WHITMAN
            if (shared == nullptr) { wait (200); continue; }
        }
        if (tick != nullptr) WaitForSingleObject ((HANDLE) tick, 20);
        else wait (5);
#else
        wait (20);
#endif
        if (threadShouldExit()) break;

        if (isEmersonRunning())
        {
            core.pump (inP, outP, blockSize, [this] (const float* const* in, float* const* out, int n) { deliver (in, out, n); });
            lastSilence = nowMs();
        }
        else if (nowMs() - lastSilence >= 20)
        {
            // EMERSON isn't there: keep the console going (meters fall, nothing plays)
            linkIn.clear();
            deliver (inP, outP, juce::jmin (blockSize, (int) (sampleRate * 0.02)));
            lastSilence = nowMs();
            core.resync();
        }
    }
}

} // namespace whitman
