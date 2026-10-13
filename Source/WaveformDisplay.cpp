#include "WaveformDisplay.h"

namespace click
{

namespace
{
    const juce::Colour backgroundColour { 0xff1a1d22 };
    const juce::Colour sourceColour     { 0xff444a54 };
    const juce::Colour editedColour     { 0xff58c8a8 };
    const juce::Colour handleColour     { 0xffe6a23c };
    const juce::Colour maskColour       { 0xcc111317 };

    constexpr int handleGrabRadius = 6;
}

WaveformDisplay::WaveformDisplay (ClickAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    processor.addChangeListener (this);
}

WaveformDisplay::~WaveformDisplay()
{
    processor.removeChangeListener (this);
}

void WaveformDisplay::setSlot (int slot)
{
    if (slot == slotIndex)
        return;

    slotIndex = slot;
    repaint();
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

float WaveformDisplay::getNormalised (const char* suffix) const
{
    return processor.apvts.getRawParameterValue (params::slotParam (slotIndex, suffix))->load();
}

float WaveformDisplay::xForNormalised (float value) const
{
    return (float) getWidth() * juce::jlimit (0.0f, 1.0f, value);
}

void WaveformDisplay::drawBuffer (juce::Graphics& g,
                                  const juce::AudioBuffer<float>& buffer,
                                  juce::Rectangle<float> area,
                                  juce::Colour colour)
{
    const auto numFrames = buffer.getNumSamples();

    if (numFrames <= 0 || area.getWidth() < 1.0f)
        return;

    g.setColour (colour);

    const auto centreY = area.getCentreY();
    const auto halfHeight = area.getHeight() * 0.5f;
    const auto pixels = juce::jmax (1, (int) area.getWidth());

    for (int pixel = 0; pixel < pixels; ++pixel)
    {
        const auto first = (int) ((juce::int64) pixel * numFrames / pixels);
        const auto last = juce::jmax (first + 1, (int) ((juce::int64) (pixel + 1) * numFrames / pixels));

        auto minimum = 0.0f;
        auto maximum = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* samples = buffer.getReadPointer (channel);

            for (int i = first; i < juce::jmin (last, numFrames); ++i)
            {
                minimum = juce::jmin (minimum, samples[i]);
                maximum = juce::jmax (maximum, samples[i]);
            }
        }

        const auto x = area.getX() + (float) pixel;
        const auto top = centreY - maximum * halfHeight;
        const auto bottom = centreY - minimum * halfHeight;

        g.drawLine (x, top, x, juce::jmax (bottom, top + 1.0f), 1.0f);
    }
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();

    g.setColour (backgroundColour);
    g.fillRoundedRectangle (area, 4.0f);

    const auto plot = area.reduced (2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    const auto& slot = processor.getSlot (slotIndex);

    if (slot.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText ("No sample loaded", getLocalBounds(), juce::Justification::centred, false);
        return;
    }

    drawBuffer (g, slot.getSource(), plot, sourceColour);

    auto startNorm = juce::jlimit (0.0f, 1.0f, getNormalised (params::startTrim));
    auto endNorm = juce::jlimit (0.0f, 1.0f, getNormalised (params::endTrim));

    if (endNorm < startNorm)
        std::swap (startNorm, endNorm);

    const auto startX = xForNormalised (startNorm);
    const auto endX = xForNormalised (endNorm);

    // Dim everything outside the trim region.
    g.setColour (maskColour);
    g.fillRect (area.withRight (startX));
    g.fillRect (area.withLeft (endX));

    if (auto clip = processor.getRenderedClip (slotIndex))
    {
        auto region = plot.withLeft (juce::jmax (plot.getX(), startX))
                          .withRight (juce::jmin (plot.getRight(), endX));

        if (region.getWidth() >= 1.0f)
            drawBuffer (g, clip->buffer, region, editedColour);
    }

    // Trim handles.
    for (const auto& handle : { std::make_pair (Handle::start, startX), std::make_pair (Handle::end, endX) })
    {
        const auto emphasised = (dragging == handle.first) || (dragging == Handle::none && hovered == handle.first);

        g.setColour (handleColour.withAlpha (emphasised ? 1.0f : 0.75f));
        g.drawLine (handle.second, area.getY(), handle.second, area.getBottom(), emphasised ? 3.0f : 2.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);
}

WaveformDisplay::Handle WaveformDisplay::handleAt (int x) const
{
    const auto startX = (int) xForNormalised (getNormalised (params::startTrim));
    const auto endX = (int) xForNormalised (getNormalised (params::endTrim));

    const auto startDistance = std::abs (x - startX);
    const auto endDistance = std::abs (x - endX);

    if (startDistance <= handleGrabRadius && startDistance <= endDistance)
        return Handle::start;

    if (endDistance <= handleGrabRadius)
        return Handle::end;

    return Handle::none;
}

void WaveformDisplay::mouseMove (const juce::MouseEvent& event)
{
    const auto target = handleAt (event.x);

    if (target != hovered)
    {
        hovered = target;
        setMouseCursor (target == Handle::none ? juce::MouseCursor::NormalCursor
                                               : juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
{
    dragging = handleAt (event.x);

    // Clicking away from a handle moves the nearer one to the click position.
    if (dragging == Handle::none)
    {
        const auto centre = (getNormalised (params::startTrim) + getNormalised (params::endTrim)) * 0.5f;
        dragging = ((float) event.x / (float) juce::jmax (1, getWidth())) < centre ? Handle::start : Handle::end;
    }

    const auto* suffix = (dragging == Handle::start) ? params::startTrim : params::endTrim;

    if (auto* parameter = processor.apvts.getParameter (params::slotParam (slotIndex, suffix)))
        parameter->beginChangeGesture();

    dragTo (event.x);
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (dragging != Handle::none)
        dragTo (event.x);
}

void WaveformDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging == Handle::none)
        return;

    const auto* suffix = (dragging == Handle::start) ? params::startTrim : params::endTrim;

    if (auto* parameter = processor.apvts.getParameter (params::slotParam (slotIndex, suffix)))
        parameter->endChangeGesture();

    dragging = Handle::none;
    repaint();
}

void WaveformDisplay::dragTo (int x)
{
    const auto value = juce::jlimit (0.0f, 1.0f, (float) x / (float) juce::jmax (1, getWidth()));
    const auto* suffix = (dragging == Handle::start) ? params::startTrim : params::endTrim;

    // Keep a sliver between the handles so the render can never be empty.
    const auto other = getNormalised (dragging == Handle::start ? params::endTrim : params::startTrim);
    const auto limited = (dragging == Handle::start) ? juce::jmin (value, other - 0.005f)
                                                     : juce::jmax (value, other + 0.005f);

    if (auto* parameter = processor.apvts.getParameter (params::slotParam (slotIndex, suffix)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (0.0f, 1.0f, limited)));

    repaint();
}

} // namespace click
