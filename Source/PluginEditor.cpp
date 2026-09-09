#include "PluginEditor.h"

namespace click
{

namespace
{
    const juce::Colour panelColour  { 0xff1e2127 };
    const juce::Colour windowColour { 0xff14161a };
    const juce::Colour accentColour { 0xffe8a33d };
    const juce::Colour textColour   { 0xffd8dce3 };
}

ClickAudioProcessorEditor::ClickAudioProcessorEditor (ClickAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      stepGrid (processorToUse),
      waveform (processorToUse),
      envelopeDisplay (processorToUse)
{
    setLookAndFeel (&lookAndFeel);

    auto& state = processor.apvts;

    // --- global -----------------------------------------------------------
    addAndMakeVisible (hostSyncButton);
    hostSyncAttachment = std::make_unique<ButtonAttachment> (state, params::hostSync, hostSyncButton);

    muteButton.setColour (juce::ToggleButton::textColourId, accentColour);
    addAndMakeVisible (muteButton);
    muteAttachment = std::make_unique<ButtonAttachment> (state, params::mute, muteButton);

    for (auto* control : { &tempoSlider, &stepsSlider, &swingSlider, &outputSlider })
        addAndMakeVisible (control);

    tempoAttachment  = std::make_unique<SliderAttachment> (state, params::tempo, tempoSlider.slider);
    stepsAttachment  = std::make_unique<SliderAttachment> (state, params::stepCount, stepsSlider.slider);
    swingAttachment  = std::make_unique<SliderAttachment> (state, params::swing, swingSlider.slider);
    outputAttachment = std::make_unique<SliderAttachment> (state, params::outputGain, outputSlider.slider);

    divisionLabel.setText ("DIVISION", juce::dontSendNotification);
    divisionLabel.setJustificationType (juce::Justification::centred);
    divisionLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (divisionLabel);

    divisionBox.addItemList (params::divisionNames(), 1);
    addAndMakeVisible (divisionBox);
    divisionAttachment = std::make_unique<ComboAttachment> (state, params::division, divisionBox);

    patternLabel.setText ("PATTERN", juce::dontSendNotification);
    patternLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (patternLabel);

    randomiseButton.onClick = [this] { processor.randomisePattern(); };
    addAndMakeVisible (randomiseButton);

    addAndMakeVisible (stepGrid);

    // --- slot selection ---------------------------------------------------
    for (auto* tab : { &accentTab, &regularTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1001);
        tab->setColour (juce::TextButton::buttonOnColourId, accentColour);
        tab->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (tab);
    }

    accentTab.onClick  = [this] { setActiveSlot (accentSlot); };
    regularTab.onClick = [this] { setActiveSlot (regularSlot); };

    sourceLabel.setText ("SOUND", juce::dontSendNotification);
    sourceLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (sourceLabel);
    addAndMakeVisible (sourceBox);

    loadButton.onClick     = [this] { showLoadDialog(); };
    saveButton.onClick     = [this] { showSaveDialog(); };
    auditionButton.onClick = [this] { processor.auditionSlot (activeSlot); };

    for (auto* button : { &loadButton, &saveButton, &auditionButton })
        addAndMakeVisible (button);

    addAndMakeVisible (waveform);

    sampleInfoLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    sampleInfoLabel.setColour (juce::Label::textColourId, textColour.withAlpha (0.7f));
    addAndMakeVisible (sampleInfoLabel);

    for (auto* control : { &levelSlider, &tuneSlider, &highPassSlider, &lowPassSlider })
        addAndMakeVisible (control);

    addAndMakeVisible (reverseButton);
    addAndMakeVisible (normaliseButton);

    addAndMakeVisible (envelopeDisplay);

    for (auto* control : { &attackSlider, &decaySlider, &sustainSlider, &releaseSlider })
        addAndMakeVisible (control);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, textColour.withAlpha (0.7f));
    addAndMakeVisible (statusLabel);

    accentTab.setToggleState (true, juce::dontSendNotification);
    setActiveSlot (accentSlot);

    processor.addChangeListener (this);
    startTimerHz (15);

    setResizable (true, true);
    setResizeLimits (860, 720, 1600, 1130);
    setSize (920, 782);
}

ClickAudioProcessorEditor::~ClickAudioProcessorEditor()
{
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void ClickAudioProcessorEditor::setActiveSlot (int slot)
{
    activeSlot = slot;

    // Attachments are rebuilt so that every control edits the selected slot.
    sourceAttachment.reset();
    levelAttachment.reset();
    tuneAttachment.reset();
    attackAttachment.reset();
    decayAttachment.reset();
    sustainAttachment.reset();
    releaseAttachment.reset();
    highPassAttachment.reset();
    lowPassAttachment.reset();
    reverseAttachment.reset();
    normaliseAttachment.reset();

    auto& state = processor.apvts;

    sourceBox.clear (juce::dontSendNotification);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter (params::slotParam (slot, params::source))))
        sourceBox.addItemList (choice->choices, 1);

    sourceAttachment   = std::make_unique<ComboAttachment> (state, params::slotParam (slot, params::source), sourceBox);
    levelAttachment    = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::gain), levelSlider.slider);
    tuneAttachment     = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::tune), tuneSlider.slider);
    attackAttachment   = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::attack), attackSlider.slider);
    decayAttachment    = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::decay), decaySlider.slider);
    sustainAttachment  = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::sustain), sustainSlider.slider);
    releaseAttachment  = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::release), releaseSlider.slider);
    highPassAttachment = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::highPass), highPassSlider.slider);
    lowPassAttachment  = std::make_unique<SliderAttachment> (state, params::slotParam (slot, params::lowPass), lowPassSlider.slider);
    reverseAttachment  = std::make_unique<ButtonAttachment> (state, params::slotParam (slot, params::reverse), reverseButton);
    normaliseAttachment = std::make_unique<ButtonAttachment> (state, params::slotParam (slot, params::normalise), normaliseButton);

    waveform.setSlot (slot);
    envelopeDisplay.setSlot (slot);
    updateSampleInfo();
}

void ClickAudioProcessorEditor::updateSampleInfo()
{
    const auto& slot = processor.getSlot (activeSlot);

    if (slot.isEmpty())
    {
        sampleInfoLabel.setText ("No sample loaded", juce::dontSendNotification);
        return;
    }

    const auto frames = slot.getSource().getNumSamples();
    const auto rate = slot.getSourceRate();
    const auto lengthMs = rate > 0.0 ? (double) frames / rate * 1000.0 : 0.0;

    juce::String text;
    text << slot.getDisplayName()
         << "   |   " << juce::String (lengthMs, 1) << " ms"
         << "   |   " << juce::String (rate / 1000.0, 1) << " kHz"
         << "   |   " << (slot.getSource().getNumChannels() > 1 ? "stereo" : "mono");

    sampleInfoLabel.setText (text, juce::dontSendNotification);
}

void ClickAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateSampleInfo();
    repaint();
}

void ClickAudioProcessorEditor::timerCallback()
{
    const auto synced = processor.apvts.getRawParameterValue (params::hostSync)->load() > 0.5f;
    tempoSlider.slider.setEnabled (! synced);

    juce::String status;

    if (processor.isRunning())
    {
        const auto step = processor.getDisplayStep();
        status << "RUNNING";

        if (step >= 0)
            status << "  -  step " << (step + 1);
    }
    else
    {
        status << "STOPPED";
    }

    status << (synced ? "  -  host tempo" : "  -  internal tempo");

    if (processor.apvts.getRawParameterValue (params::mute)->load() > 0.5f)
        status << "  -  MUTED";

    if (status != statusLabel.getText())
        statusLabel.setText (status, juce::dontSendNotification);
}

void ClickAudioProcessorEditor::showLoadDialog()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Load a click sample",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        processor.getFormatManager().getWildcardForAllFormats());

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (! file.existsAsFile())
            return;

        if (! processor.loadUserSample (activeSlot, file))
            juce::NativeMessageBox::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle ("Could not load sample")
                    .withMessage ("\"" + file.getFileName() + "\" could not be decoded."),
                nullptr);

        updateSampleInfo();
    });
}

void ClickAudioProcessorEditor::showSaveDialog()
{
    const auto suggested = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                               .getChildFile (processor.getSlot (activeSlot).getDisplayName() + " (edited).wav");

    chooser = std::make_unique<juce::FileChooser> ("Save the edited click", suggested, "*.wav");

    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("wav"))
            file = file.withFileExtension ("wav");

        if (! processor.exportSlot (activeSlot, file))
            juce::NativeMessageBox::showAsync (
                juce::MessageBoxOptions()
                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                    .withTitle ("Could not save sample")
                    .withMessage ("Writing \"" + file.getFileName() + "\" failed."),
                nullptr);
    });
}

void ClickAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (windowColour);

    auto header = getLocalBounds().reduced (12).removeFromTop (40);

    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("CLICK", header, juce::Justification::centredLeft, false);

    g.setColour (accentColour);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("STEP SEQUENCED CLICK GENERATOR",
                header.withTrimmedLeft (86), juce::Justification::centredLeft, false);

    // panel behind the slot editor
    g.setColour (panelColour);
    g.fillRoundedRectangle (panelBounds.toFloat(), 6.0f);

    // panel behind the ADSR strip
    g.setColour (panelColour);
    g.fillRoundedRectangle (adsrBounds.toFloat(), 6.0f);

    g.setColour (accentColour);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("AMPLITUDE ENVELOPE",
                adsrBounds.reduced (12, 8), juce::Justification::topLeft, false);
}

void ClickAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto header = area.removeFromTop (40);
    statusLabel.setBounds (header.removeFromRight (260));

    area.removeFromTop (8);

    // --- global control strip --------------------------------------------
    auto controls = area.removeFromTop (78);
    const auto controlWidth = controls.getWidth() / 6;

    auto syncArea = controls.removeFromLeft (controlWidth);
    auto toggleColumn = syncArea.withSizeKeepingCentre (controlWidth - 16, 62);
    hostSyncButton.setBounds (toggleColumn.removeFromTop (28));
    toggleColumn.removeFromTop (6);
    muteButton.setBounds (toggleColumn.removeFromTop (28));

    tempoSlider.setBounds (controls.removeFromLeft (controlWidth).reduced (4, 0));

    auto divisionArea = controls.removeFromLeft (controlWidth).reduced (6, 0);
    divisionLabel.setBounds (divisionArea.removeFromTop (16));
    divisionBox.setBounds (divisionArea.withSizeKeepingCentre (divisionArea.getWidth(), 26));

    stepsSlider.setBounds (controls.removeFromLeft (controlWidth).reduced (4, 0));
    swingSlider.setBounds (controls.removeFromLeft (controlWidth).reduced (4, 0));
    outputSlider.setBounds (controls.reduced (4, 0));

    area.removeFromTop (10);

    auto patternRow = area.removeFromTop (24);
    patternLabel.setBounds (patternRow.removeFromLeft (90));
    randomiseButton.setBounds (patternRow.removeFromRight (110).reduced (0, 1));
    area.removeFromTop (6);

    stepGrid.setBounds (area.removeFromTop (96));
    area.removeFromTop (12);

    // --- ADSR strip along the bottom --------------------------------------
    adsrBounds = area.removeFromBottom (130);
    area.removeFromBottom (12);

    {
        auto adsr = adsrBounds.reduced (10);
        adsr.removeFromTop (14);   // room for the section caption

        envelopeDisplay.setBounds (adsr.removeFromLeft (juce::jmax (140, adsr.getWidth() / 3)));
        adsr.removeFromLeft (14);

        const auto stageWidth = adsr.getWidth() / 4;

        attackSlider.setBounds (adsr.removeFromLeft (stageWidth).reduced (2, 0));
        decaySlider.setBounds (adsr.removeFromLeft (stageWidth).reduced (2, 0));
        sustainSlider.setBounds (adsr.removeFromLeft (stageWidth).reduced (2, 0));
        releaseSlider.setBounds (adsr.reduced (2, 0));
    }

    // --- slot editor ------------------------------------------------------
    panelBounds = area;
    auto panel = area.reduced (10);

    auto sidebar = panel.removeFromLeft (170);
    auto tabs = sidebar.removeFromTop (28);
    accentTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (1));
    regularTab.setBounds (tabs.reduced (1));

    sidebar.removeFromTop (10);
    sourceLabel.setBounds (sidebar.removeFromTop (16));
    sourceBox.setBounds (sidebar.removeFromTop (26));
    sidebar.removeFromTop (10);
    loadButton.setBounds (sidebar.removeFromTop (26));
    sidebar.removeFromTop (6);
    saveButton.setBounds (sidebar.removeFromTop (26));
    sidebar.removeFromTop (6);
    auditionButton.setBounds (sidebar.removeFromTop (26));

    panel.removeFromLeft (12);

    waveform.setBounds (panel.removeFromTop (juce::jmax (90, panel.getHeight() - 130)));
    sampleInfoLabel.setBounds (panel.removeFromTop (18));
    panel.removeFromTop (4);

    auto knobs = panel.removeFromTop (juce::jmax (70, panel.getHeight() - 28));
    const auto knobWidth = knobs.getWidth() / 4;

    for (auto* control : { &levelSlider, &tuneSlider, &highPassSlider, &lowPassSlider })
        control->setBounds (knobs.removeFromLeft (knobWidth).reduced (2, 0));

    auto toggles = panel;
    reverseButton.setBounds (toggles.removeFromLeft (110));
    normaliseButton.setBounds (toggles.removeFromLeft (110));
}

} // namespace click
