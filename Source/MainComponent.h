#pragma once

#include <JuceHeader.h>
#include "MixEngine.h"
#include "MixState.h"
#include "Console.h"
#include "RouteStatus.h"
#include "Updater.h"

namespace whitman
{

//==============================================================================
/** WHITMAN: a basic mixer for Hawthorne. Its audio device is the Hawthorne Route ASIO
    driver, so every tape track arrives on its own strip, and the stereo mix goes back
    out of the driver's "To Hawthorne master L / R" into Hawthorne's 2-track master. */
class MainComponent : public juce::Component,
                      private juce::AudioIODeviceCallback,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ApplicationProperties props;
    bool propsReady = false;
    juce::AudioDeviceManager deviceManager;
    MixEngine engine;
    juce::StringArray names;
    RouteStatus routeStatus;
    RouteStatus::Info routeInfo;
    Updater updater;

    std::atomic<int> shownStrips { 0 };              // set when the device starts
    std::atomic<bool> deviceIsRoute { false };
    std::atomic<bool> routingChanged { false };
    juce::String deviceName;
    double deviceRate = 0.0;

    // top bar
    juce::TextButton audioButton { "AUDIO" }, openButton { "OPEN" }, saveButton { "SAVE" },
                     resetButton { "RESET" }, updateButton { "UPDATE" };
    juce::Rectangle<int> statusArea;

    // console
    juce::Viewport viewport;
    juce::Component stripHolder;
    juce::OwnedArray<ChannelStrip> strips;
    MasterStrip master { engine };
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::File currentFile;
    bool dirty = false;
    int saveCountdown = 0;
    int tick = 0;

    // audio
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int n, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;   // device changed
    void timerCallback() override;

    void openAudio();
    void enableAllChannels();          // every input and output of the device on
    void rebuildStrips();
    void refreshAll();
    void mixChanged();
    void saveSettings();

    void showAudioSettings();
    void openMix();
    void saveMix();
    void resetMix();

    juce::String statusLine (juce::Colour& colour) const;
    juce::String hintLine() const;

    juce::PropertiesFile& settings();
};

} // namespace whitman
