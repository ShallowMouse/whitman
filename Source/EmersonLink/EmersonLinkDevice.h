#pragma once

#include <JuceHeader.h>
#include "EmersonLink.h"
#include "LinkClientCore.h"

namespace whitman
{

//==============================================================================
/** WHITMAN's end of the EMERSON Link: an audio device ("EMERSON Link" > "EMERSON") that
    runs inside WHITMAN, with nothing to install. EMERSON (the FX rack) is the only program
    on Hawthorne Route; it sends WHITMAN each track after its inserts, and WHITMAN sends
    back the mix (and the echo send, for EMERSON's FX return A). Everything runs off
    Hawthorne's clock, sample-locked.

    Its channels are named like Hawthorne Route's ("Hawthorne track 7", "To Hawthorne
    master L"), so the console routes them the same way, plus EMERSON's returns and sends.
    When EMERSON isn't running the device keeps going with silence. Windows only. */
class EmersonLinkDevice : public juce::AudioIODevice,
                          private juce::Thread
{
public:
    static constexpr const char* typeName   = "EMERSON Link";
    static constexpr const char* deviceName = "EMERSON";

    EmersonLinkDevice();
    ~EmersonLinkDevice() override;

    juce::StringArray getOutputChannelNames() override;
    juce::StringArray getInputChannelNames() override;
    juce::Array<double> getAvailableSampleRates() override;
    juce::Array<int> getAvailableBufferSizes() override;
    int getDefaultBufferSize() override;
    juce::String open (const juce::BigInteger& inputChannels, const juce::BigInteger& outputChannels,
                       double sampleRate, int bufferSizeSamples) override;
    void close() override;
    bool isOpen() override                              { return opened; }
    void start (juce::AudioIODeviceCallback*) override;
    void stop() override;
    bool isPlaying() override                           { return callback != nullptr; }
    juce::String getLastError() override                { return {}; }
    int getCurrentBufferSizeSamples() override          { return blockSize; }
    double getCurrentSampleRate() override              { return sampleRate; }
    int getCurrentBitDepth() override                   { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return activeOut; }
    juce::BigInteger getActiveInputChannels() const override  { return activeIn; }
    int getOutputLatencyInSamples() override            { return blockSize; }
    int getInputLatencyInSamples() override             { return 0; }

    /** EMERSON is running and writing. */
    bool isEmersonRunning() const;
    /** EMERSON's track count, rate or block size changed since the device opened: close it
        and open it again (the console's strips follow). Message thread. */
    bool needsReopen() const;

    /** The names of an N-track link's channels (also used by the tests). */
    static juce::StringArray inputNames (int tracks);
    static juce::StringArray outputNames();

private:
    void* mapping = nullptr;
    void* tick = nullptr;
    emerson::link::Shared* shared = nullptr;
    emerson::link::ClientCore core;

    bool opened = false;
    int tracks = 24, blockSize = 512;
    double sampleRate = 48000.0;
    juce::BigInteger activeIn, activeOut;
    juce::CriticalSection callbackLock;
    juce::AudioIODeviceCallback* callback = nullptr;

    juce::AudioBuffer<float> linkIn, linkOut;        // all the link's channels
    std::vector<const float*> devIn;                 // the ACTIVE ones, as the callback wants them
    std::vector<float*> devOut;
    std::vector<int> inChannelOf, outChannelOf;      // active index -> link channel

    void connect();
    void disconnect();
    void run() override;
    void deliver (const float* const* in, float* const* out, int n);
};

//==============================================================================
class EmersonLinkDeviceType : public juce::AudioIODeviceType
{
public:
    EmersonLinkDeviceType() : AudioIODeviceType (EmersonLinkDevice::typeName) {}
    void scanForDevices() override {}
    juce::StringArray getDeviceNames (bool) const override  { return { EmersonLinkDevice::deviceName }; }
    int getDefaultDeviceIndex (bool) const override         { return 0; }
    int getIndexOfDevice (juce::AudioIODevice* d, bool) const override { return d != nullptr && d->getName() == EmersonLinkDevice::deviceName ? 0 : -1; }
    bool hasSeparateInputsAndOutputs() const override       { return false; }
    juce::AudioIODevice* createDevice (const juce::String& out, const juce::String& in) override
    {
        if (out == EmersonLinkDevice::deviceName || in == EmersonLinkDevice::deviceName || (out.isEmpty() && in.isEmpty()))
            return new EmersonLinkDevice();
        return nullptr;
    }
};

} // namespace whitman
