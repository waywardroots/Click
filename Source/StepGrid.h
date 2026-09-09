#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace click
{

/** The pattern editor: a clickable grid of steps, each cycling through
    off -> regular -> accent, with a playhead that follows the sequencer. */
class StepGrid : public juce::Component,
                 private juce::Timer
{
public:
    explicit StepGrid (ClickAudioProcessor& processorToUse);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static constexpr int columns = 16;
    static constexpr int rows = maxSteps / columns;

    void timerCallback() override;

    juce::Rectangle<float> boundsForStep (int step) const;
    int stepAt (juce::Point<int> position) const;
    StepState getState (int step) const;
    void setState (int step, StepState state);

    ClickAudioProcessor& processor;

    int highlightedStep = -1;
    int paintingStep = -1;
    StepState paintingState = StepState::off;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};

} // namespace click
