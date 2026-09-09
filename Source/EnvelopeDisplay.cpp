#include "EnvelopeDisplay.h"

namespace click
{

namespace
{
    const juce::Colour backgroundColour { 0xff1a1d22 };
    const juce::Colour curveColour      { 0xffe8a33d };
    const juce::Colour gridColour       { 0xff2c3138 };
}

EnvelopeDisplay::EnvelopeDisplay (ClickAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    processor.addChangeListener (this);
}

EnvelopeDisplay::~EnvelopeDisplay()
{
    processor.removeChangeListener (this);
}

void EnvelopeDisplay::setSlot (int slot)
{
    if (slot == slotIndex)
        return;

    slotIndex = slot;
    repaint();
}

void EnvelopeDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

double EnvelopeDisplay::getClipLengthSeconds() const
{
    if (auto clip = processor.getRenderedClip (slotIndex))
        if (clip->sourceRate > 0.0)
            return (double) clip->buffer.getNumSamples() / clip->sourceRate;

    return 0.0;
}

void EnvelopeDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    g.setColour (backgroundColour);
    g.fillRoundedRectangle (area, 4.0f);

    const auto plot = area.reduced (6.0f);

    if (plot.getWidth() < 2.0f || plot.getHeight() < 2.0f)
        return;

    // quarter-height guides
    g.setColour (gridColour);

    for (int i = 1; i < 4; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
    }

    const auto length = getClipLengthSeconds();

    if (length <= 0.0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("No sample loaded", getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    const auto settings = processor.readSettings (slotIndex);

    juce::Path curve;
    juce::Path filled;

    const auto steps = juce::jmax (2, (int) plot.getWidth());

    for (int i = 0; i <= steps; ++i)
    {
        const auto proportion = (double) i / (double) steps;
        const auto value = envelopeValueAt (proportion * length, length, settings);

        const auto x = plot.getX() + (float) proportion * plot.getWidth();
        const auto y = plot.getBottom() - (float) value * plot.getHeight();

        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            filled.startNewSubPath (x, plot.getBottom());
            filled.lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            filled.lineTo (x, y);
        }
    }

    filled.lineTo (plot.getRight(), plot.getBottom());
    filled.closeSubPath();

    g.setColour (curveColour.withAlpha (0.18f));
    g.fillPath (filled);

    g.setColour (curveColour);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // total length of the trimmed clip, which is what the envelope spans
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (juce::String (length * 1000.0, 1) + " ms",
                plot.reduced (2.0f), juce::Justification::topRight, false);

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
}

} // namespace click
