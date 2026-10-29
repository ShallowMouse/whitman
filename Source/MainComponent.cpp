#include "MainComponent.h"
#include "DeskPicker.h"

namespace whitman
{

static constexpr const char* routeDeviceName = "Hawthorne Route";

juce::PropertiesFile& MainComponent::settings()
{
    if (! propsReady)
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "WHITMAN";
        o.filenameSuffix = ".settings";
        o.folderName = "PEQUOD/WHITMAN";
        o.osxLibrarySubFolder = "Application Support";
        props.setStorageParameters (o);
        propsReady = true;
    }
    return *props.getUserSettings();
}

//==============================================================================
MainComponent::MainComponent()
{
    MixState::reset (engine, names);
    if (auto mix = settings().getXmlValue ("mix"))
        MixState::fromXml (*mix, engine, names);
    currentFile = juce::File (settings().getValue ("file"));

    for (auto* b : { &deskButton, &audioButton, &openButton, &saveButton, &resetButton, &updateButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    audioButton.setTooltip ("Audio device: pick Hawthorne Route (ASIO)");
    openButton.setTooltip ("Open a saved mix");
    saveButton.setTooltip ("Save this mix to a file");
    resetButton.setTooltip ("Every fader to 0 dB, pans centred, EQ flat, no echo, nothing muted or soloed");
    deskButton.setTooltip ("Pick the desk: the console the tracks are mixed on");
    deskButton.onClick = [this] { showDeskPicker(); };
    audioButton.onClick = [this] { showAudioSettings(); };
    openButton.onClick  = [this] { openMix(); };
    saveButton.onClick  = [this] { saveMix(); };
    resetButton.onClick = [this] { resetMix(); };
    updateButton.setVisible (false);
    updateButton.setColour (juce::TextButton::buttonColourId, Pequod::Colours::mustardLo);
    updateButton.onClick = [this]
    {
        if (updater.isDownloading()) return;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::InfoIcon, "WHITMAN " + updater.getLatestVersion(),
            updater.getReleaseNotes() + "\n\nInstall it now? WHITMAN closes, updates and starts again.", "INSTALL", "LATER", this,
            juce::ModalCallbackFunction::create ([this] (int r) { if (r == 1) { saveSettings(); updater.install(); } }));
    };
    updater.onChange = [this]
    {
        updateButton.setVisible (updater.isUpdateAvailable());
        updateButton.setButtonText (updater.isDownloading() ? "DOWNLOADING " + juce::String (juce::roundToInt (updater.getProgress() * 100)) + "%"
                                                            : "UPDATE " + updater.getLatestVersion());
        resized();
    };
    updater.onStatus = [] (const juce::String&) {};
    updater.checkSoon();

    master.onChanged = [this] { mixChanged(); };
    addAndMakeVisible (master);

    viewport.setViewedComponent (&stripHolder, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    setSize (1360, 820);
    openAudio();
    applyDesk();
    refreshAll();
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveSettings();
    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

//==============================================================================
// Audio

void MainComponent::openAudio()
{
    auto saved = settings().getXmlValue ("audio");
    deviceManager.initialise (maxStrips + 3, maxStrips + 2, saved.get(), true);

    if (saved == nullptr)
    {
        // first run: Hawthorne Route if it's installed
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            type->scanForDevices();
            if (type->getDeviceNames (true).contains (routeDeviceName))
            {
                deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);
                auto setup = deviceManager.getAudioDeviceSetup();
                setup.inputDeviceName = setup.outputDeviceName = routeDeviceName;
                deviceManager.setAudioDeviceSetup (setup, true);
                break;
            }
        }
    }

    enableAllChannels();
    deviceManager.addChangeListener (this);
    deviceManager.addAudioCallback (this);
}

void MainComponent::enableAllChannels()
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    if (dev == nullptr || ! dev->getName().containsIgnoreCase (routeDeviceName)) return;   // other devices: the user's choice in AUDIO

    const int ins = dev->getInputChannelNames().size(), outs = dev->getOutputChannelNames().size();
    if (dev->getActiveInputChannels().countNumberOfSetBits() >= ins
        && dev->getActiveOutputChannels().countNumberOfSetBits() >= outs)
        return;

    // Hawthorne Route shows as many tracks as the reel has; when that grows, switch the new ones on too
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.useDefaultInputChannels = setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.inputChannels.setRange (0, ins, true);
    setup.outputChannels.clear();
    setup.outputChannels.setRange (0, outs, true);
    deviceManager.setAudioDeviceSetup (setup, true);
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* dev)
{
    auto r = Routing::make (dev->getInputChannelNames(), dev->getActiveInputChannels(),
                            dev->getOutputChannelNames(), dev->getActiveOutputChannels());
    shownStrips = r.numStrips;
    deviceIsRoute = r.isHawthorneRoute;
    engine.setRouting (std::move (r));
    engine.prepare (dev->getCurrentSampleRate(), dev->getCurrentBufferSizeSamples());
    routingChanged = true;
}

void MainComponent::audioDeviceStopped() {}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                       int n, const juce::AudioIODeviceCallbackContext&)
{
    engine.process (in, numIn, out, numOut, n);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    enableAllChannels();
    routingChanged = true;
    if (auto state = deviceManager.createStateXml())
        settings().setValue ("audio", state.get());
}

//==============================================================================
// Console

void MainComponent::rebuildStrips()
{
    auto* dev = deviceManager.getCurrentAudioDevice();
    deviceName = dev != nullptr ? dev->getName() : juce::String();
    deviceRate = dev != nullptr ? dev->getCurrentSampleRate() : 0.0;

    int n = dev != nullptr ? shownStrips.load() : 0;
    // test hook for screenshots on machines without a sound card
    if (dev == nullptr)
        n = juce::jlimit (0, maxStrips, juce::SystemStats::getEnvironmentVariable ("WHITMAN_PREVIEW_STRIPS", "0").getIntValue());
    while (strips.size() < n)
    {
        auto* s = strips.add (new ChannelStrip (engine, strips.size(), names));
        s->setDesk (engine.getDesk());
        s->onChanged = [this] { mixChanged(); };
        stripHolder.addChildComponent (s);
    }
    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setVisible (i < n);

    const int w = DeskSkin::of (engine.getDesk()).stripWidth;
    stripHolder.setSize (juce::jmax (1, n * w), juce::jmax (1, viewport.getMaximumVisibleHeight()));
    for (int i = 0; i < n; ++i)
        strips[i]->setBounds (i * w, 0, w, stripHolder.getHeight());
    repaint();
}

void MainComponent::refreshAll()
{
    for (auto* s : strips) s->refreshFromEngine();
    master.refreshFromEngine();
}

void MainComponent::mixChanged()
{
    dirty = true;
    saveCountdown = 30;      // keep it a second after the last move
}

void MainComponent::saveSettings()
{
    if (auto x = MixState::toXml (engine, names))
        settings().setValue ("mix", x.get());
    settings().setValue ("file", currentFile.getFullPathName());
    if (auto state = deviceManager.createStateXml())
        settings().setValue ("audio", state.get());
    settings().saveIfNeeded();
    dirty = false;
}

void MainComponent::timerCallback()
{
    if (routingChanged.exchange (false))
        rebuildStrips();

    for (auto* s : strips)
        if (s->isVisible()) s->updateMeter();
    master.updateMeter();

    if (saveCountdown > 0 && --saveCountdown == 0 && dirty)
        saveSettings();

    if (++tick % 15 == 0)
    {
        enableAllChannels();
        const auto before = routeInfo;
        routeInfo = routeStatus.poll();
        if (before.hawthorneRunning != routeInfo.hawthorneRunning || before.tracks != routeInfo.tracks)
            repaint (statusArea);
    }
}

//==============================================================================
// Mix files

void MainComponent::applyDesk()
{
    const auto d = engine.getDesk();
    deskButton.setButtonText (juce::String ("DESK: ") + deskInfo (d).badge);
    for (auto* s : strips) s->setDesk (d);
    master.setDesk (d);
    resized();
    repaint();
}

void MainComponent::showDeskPicker()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new DeskPicker (engine.getDesk(), [this] (Desk d)
    {
        if (d == engine.getDesk()) return;
        engine.desk = (int) d;      // the engine switches at the start of its next block
        applyDesk();
        mixChanged();
    }));
    o.dialogTitle = "WHITMAN desk";
    o.dialogBackgroundColour = Pequod::Colours::bg;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.componentToCentreAround = this;
    o.launchAsync();
}

void MainComponent::showAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent (deviceManager, 0, maxStrips + 3, 2, maxStrips + 2, false, false, false, false);
    sel->setSize (560, 420);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (sel);
    o.dialogTitle = "WHITMAN audio";
    o.dialogBackgroundColour = Pequod::Colours::panel;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.componentToCentreAround = this;
    o.launchAsync();
}

void MainComponent::openMix()
{
    chooser = std::make_unique<juce::FileChooser> ("Open a WHITMAN mix",
                                                   currentFile.existsAsFile() ? currentFile : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   juce::String ("*") + MixState::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        auto x = juce::XmlDocument::parse (f);
        if (x == nullptr || ! MixState::fromXml (*x, engine, names))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "WHITMAN", f.getFileName() + " isn't a WHITMAN mix.");
            return;
        }
        currentFile = f;
        applyDesk();
        refreshAll();
        saveSettings();
        repaint();
    });
}

void MainComponent::saveMix()
{
    auto start = currentFile.existsAsFile() ? currentFile
                                            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::String ("Mix") + MixState::fileExtension);
    chooser = std::make_unique<juce::FileChooser> ("Save this mix", start, juce::String ("*") + MixState::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension (MixState::fileExtension);
        if (auto x = MixState::toXml (engine, names); x == nullptr || ! x->writeTo (f))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "WHITMAN", "Couldn't save " + f.getFullPathName());
            return;
        }
        currentFile = f;
        saveSettings();
        repaint();
    });
}

void MainComponent::resetMix()
{
    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Reset the mix?",
        "Every fader back to 0 dB, pans centred, EQ flat, no echo, nothing muted or soloed. Track names are cleared too. The desk stays.", "RESET", "CANCEL", this,
        juce::ModalCallbackFunction::create ([this] (int r)
        {
            if (r != 1) return;
            MixState::reset (engine, names);
            currentFile = juce::File();
            refreshAll();
            saveSettings();
            repaint();
        }));
}

//==============================================================================
// Drawing

juce::String MainComponent::statusLine (juce::Colour& c) const
{
    const auto dot = juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    if (deviceName.isEmpty())
    {
        c = Pequod::Colours::danger;
        return "NO AUDIO DEVICE" + dot + "click AUDIO and pick Hawthorne Route (ASIO)";
    }
    const auto rate = juce::String (deviceRate / 1000.0, 1) + " kHz";
    if (deviceIsRoute.load())
    {
       #if JUCE_WINDOWS
        if (! routeInfo.hawthorneRunning)
        {
            c = Pequod::Colours::danger;
            return "HAWTHORNE ROUTE" + dot + "Hawthorne isn't running: start it and the tracks come in";
        }
       #endif
        c = Pequod::Colours::mustard;
        return "HAWTHORNE ROUTE" + dot + juce::String (shownStrips.load()) + " TRACKS" + dot + rate
               + dot + juce::String (juce::CharPointer_UTF8 ("MIX \xe2\x86\x92 HAWTHORNE MASTER"));
    }
    c = Pequod::Colours::textDim;
    return deviceName.toUpperCase() + dot + rate + dot + "not Hawthorne Route: inputs are strips, the mix goes to outputs 1-2";
}

juce::String MainComponent::hintLine() const
{
    if (deviceIsRoute.load())
        return "In Hawthorne's ROUTING: master L / R inputs from ROUTE master L / R, and each track's MIX off (or you'll hear it twice)";
    return "Hawthorne Route is the ASIO driver Hawthorne installs (Hawthorne > SETTINGS)";
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Pequod::Colours::bg);
    g.setColour (DeskSkin::of (engine.getDesk()).frame);
    g.fillRect (getLocalBounds().withTrimmedTop (66));

    // top bar
    auto bar = getLocalBounds().removeFromTop (64);
    g.setColour (Pequod::Colours::rail);
    g.fillRect (bar);
    g.setColour (Pequod::Colours::mustard);
    g.fillRect (bar.removeFromBottom (2));

    auto logo = bar.reduced (18, 0).removeFromLeft (190);
    g.setFont (Pequod::heading (30.0f));
    g.setColour (Pequod::Colours::mustard);
    g.drawText ("WHITMAN", logo.withTrimmedBottom (14), juce::Justification::bottomLeft);
    g.setFont (Pequod::heading (11.0f, false));
    g.setColour (Pequod::Colours::textDim);
    g.drawText ("PEQUOD  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  MIXER  " + juce::String (JUCE_APPLICATION_VERSION_STRING),
                logo.withTrimmedTop (42), juce::Justification::topLeft);

    juce::Colour c;
    const auto status = statusLine (c);
    g.setFont (Pequod::heading (13.0f));
    g.setColour (c);
    g.drawText (status, statusArea.withHeight (statusArea.getHeight() / 2).translated (0, 4), juce::Justification::centredLeft, true);
    g.setFont (Pequod::body (12.0f));
    g.setColour (Pequod::Colours::textDim);
    g.drawText (hintLine() + (currentFile.existsAsFile() ? "     [" + currentFile.getFileNameWithoutExtension() + "]" : juce::String()),
                statusArea.withTrimmedTop (statusArea.getHeight() / 2).translated (0, -2), juce::Justification::centredLeft, true);

    if (shownStrips.load() == 0 || deviceName.isEmpty())
    {
        auto area = viewport.getBounds();
        g.setFont (Pequod::heading (16.0f));
        g.setColour (Pequod::Colours::textDim);
        g.drawText (deviceName.isEmpty() ? "No audio device" : "No input channels",
                    area, juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (64).reduced (14, 14);
    bar.removeFromLeft (200);
    for (auto* b : { &audioButton, &saveButton, &openButton, &resetButton })
    {
        b->setBounds (bar.removeFromRight (76));
        bar.removeFromRight (6);
    }
    deskButton.setBounds (bar.removeFromRight (140));
    bar.removeFromRight (6);
    if (updateButton.isVisible())
    {
        updateButton.setBounds (bar.removeFromRight (150));
        bar.removeFromRight (6);
    }
    statusArea = bar.withTrimmedRight (10).expanded (0, 8);

    r.reduce (8, 8);
    master.setBounds (r.removeFromRight (DeskSkin::of (engine.getDesk()).masterWidth));
    r.removeFromRight (8);
    viewport.setBounds (r);
    rebuildStrips();
}

} // namespace whitman
