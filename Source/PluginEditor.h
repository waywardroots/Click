#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "StepGrid.h"
#include "WaveformDisplay.h"
#include "EnvelopeDisplay.h"

namespace click
{

/** A slider with a caption above it. */
class LabelledSlider : public juce::Component
{
public:
    LabelledSlider (const juce::String& text, juce::Slider::SliderStyle style)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (label);

        slider.setSliderStyle (style);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        addAndMakeVisible (slider);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (16));
        slider.setBounds (area);
    }

    juce::Label label;
    juce::Slider slider;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelledSlider)
};

class ClickAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::ChangeListener,
                                  private juce::Timer
{
public:
    explicit ClickAudioProcessorEditor (ClickAudioProcessor& processorToUse);
    ~ClickAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    void setActiveSlot (int slot);
    void updateSampleInfo();
    void showLoadDialog();
    void showSaveDialog();

    ClickAudioProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel { juce::LookAndFeel_V4::getMidnightColourScheme() };

    // --- global controls --------------------------------------------------
    juce::ToggleButton hostSyncButton { "Host Sync" };
    juce::ToggleButton muteButton { "Mute" };
    LabelledSlider tempoSlider { "TEMPO", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider stepsSlider { "STEPS", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider swingSlider { "SWING", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider outputSlider { "OUTPUT", juce::Slider::RotaryHorizontalVerticalDrag };
    juce::Label divisionLabel;
    juce::ComboBox divisionBox;

    std::unique_ptr<ButtonAttachment> hostSyncAttachment, muteAttachment;
    std::unique_ptr<SliderAttachment> tempoAttachment, stepsAttachment, swingAttachment, outputAttachment;
    std::unique_ptr<ComboAttachment> divisionAttachment;

    StepGrid stepGrid;

    // --- slot editor ------------------------------------------------------
    juce::TextButton accentTab { "ACCENT" };
    juce::TextButton regularTab { "REGULAR" };
    juce::Label sourceLabel;
    juce::ComboBox sourceBox;
    juce::TextButton loadButton { "Load WAV..." };
    juce::TextButton saveButton { "Save WAV..." };
    juce::TextButton auditionButton { "Audition" };

    WaveformDisplay waveform;
    juce::Label sampleInfoLabel;

    LabelledSlider levelSlider { "LEVEL", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider tuneSlider { "TUNE", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider highPassSlider { "HI PASS", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider lowPassSlider { "LO PASS", juce::Slider::RotaryHorizontalVerticalDrag };
    juce::ToggleButton reverseButton { "Reverse" };
    juce::ToggleButton normaliseButton { "Normalise" };

    // --- ADSR strip along the bottom --------------------------------------
    EnvelopeDisplay envelopeDisplay;
    LabelledSlider attackSlider { "ATTACK", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider decaySlider { "DECAY", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider sustainSlider { "SUSTAIN", juce::Slider::RotaryHorizontalVerticalDrag };
    LabelledSlider releaseSlider { "RELEASE", juce::Slider::RotaryHorizontalVerticalDrag };

    std::unique_ptr<ComboAttachment> sourceAttachment;
    std::unique_ptr<SliderAttachment> levelAttachment, tuneAttachment, highPassAttachment,
                                      lowPassAttachment, attackAttachment, decayAttachment,
                                      sustainAttachment, releaseAttachment;
    std::unique_ptr<ButtonAttachment> reverseAttachment, normaliseAttachment;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Label statusLabel;

    /** Backdrops filled in paint() and set in resized(). */
    juce::Rectangle<int> panelBounds;
    juce::Rectangle<int> adsrBounds;

    int activeSlot = accentSlot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickAudioProcessorEditor)
};

} // namespace click
