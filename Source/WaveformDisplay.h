#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace click
{

/** Waveform view for a slot.

    The raw sample is drawn dimly across the full width; the edited result is
    drawn on top, inside the trim region, so the effect of the filters, envelope
    and gain is visible. The trim points are draggable handles.
*/
class WaveformDisplay : public juce::Component,
                        private juce::ChangeListener
{
public:
    explicit WaveformDisplay (ClickAudioProcessor& processorToUse);
    ~WaveformDisplay() override;

    void setSlot (int slot);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

private:
    enum class Handle { none, start, end };

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    float getNormalised (const char* suffix) const;
    Handle handleAt (int x) const;
    float xForNormalised (float value) const;
    void dragTo (int x);

    static void drawBuffer (juce::Graphics& g,
                            const juce::AudioBuffer<float>& buffer,
                            juce::Rectangle<float> area,
                            juce::Colour colour);

    ClickAudioProcessor& processor;
    int slotIndex = 0;

    Handle dragging = Handle::none;
    Handle hovered = Handle::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

} // namespace click
