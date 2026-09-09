#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace click
{

/** Draws the slot's ADSR envelope across the length of the trimmed sample.

    The curve is produced by the same envelopeValueAt() the renderer uses, so
    what is drawn is exactly what is applied to the audio.
*/
class EnvelopeDisplay : public juce::Component,
                        private juce::ChangeListener
{
public:
    explicit EnvelopeDisplay (ClickAudioProcessor& processorToUse);
    ~EnvelopeDisplay() override;

    void setSlot (int slot);

    void paint (juce::Graphics& g) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    /** Length of the trimmed clip in seconds, which is the envelope's x axis. */
    double getClipLengthSeconds() const;

    ClickAudioProcessor& processor;
    int slotIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeDisplay)
};

} // namespace click
