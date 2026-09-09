#include "StepGrid.h"

namespace click
{

namespace
{
    const juce::Colour offColour     { 0xff23262c };
    const juce::Colour regularColour { 0xff4b8ede };
    const juce::Colour accentColour  { 0xffe8a33d };
    const juce::Colour disabledColour{ 0xff17191d };
    const juce::Colour outlineColour { 0xff34383f };
}

StepGrid::StepGrid (ClickAudioProcessor& processorToUse)
    : processor (processorToUse)
{
    startTimerHz (30);
}

void StepGrid::timerCallback()
{
    const auto step = processor.isRunning() ? processor.getDisplayStep() : -1;

    if (step != highlightedStep)
    {
        highlightedStep = step;
        repaint();
    }
}

juce::Rectangle<float> StepGrid::boundsForStep (int step) const
{
    const auto area = getLocalBounds().toFloat();
    const auto cellWidth = area.getWidth() / (float) columns;
    const auto cellHeight = area.getHeight() / (float) rows;

    const auto column = step % columns;
    const auto row = step / columns;

    return juce::Rectangle<float> (area.getX() + (float) column * cellWidth,
                                   area.getY() + (float) row * cellHeight,
                                   cellWidth,
                                   cellHeight).reduced (2.0f);
}

int StepGrid::stepAt (juce::Point<int> position) const
{
    const auto area = getLocalBounds();

    if (! area.contains (position) || area.isEmpty())
        return -1;

    const auto column = juce::jlimit (0, columns - 1, position.x * columns / juce::jmax (1, area.getWidth()));
    const auto row = juce::jlimit (0, rows - 1, position.y * rows / juce::jmax (1, area.getHeight()));

    return row * columns + column;
}

StepState StepGrid::getState (int step) const
{
    if (! juce::isPositiveAndBelow (step, maxSteps))
        return StepState::off;

    const auto value = processor.apvts.getRawParameterValue (params::stepParam (step))->load();
    return (StepState) juce::jlimit (0, 2, (int) std::round (value));
}

void StepGrid::setState (int step, StepState state)
{
    if (! juce::isPositiveAndBelow (step, maxSteps))
        return;

    if (auto* parameter = processor.apvts.getParameter (params::stepParam (step)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (int) state));
        parameter->endChangeGesture();
    }
}

void StepGrid::paint (juce::Graphics& g)
{
    const auto activeSteps = juce::jlimit (1, maxSteps,
        (int) std::round (processor.apvts.getRawParameterValue (params::stepCount)->load()));

    for (int step = 0; step < maxSteps; ++step)
    {
        const auto cell = boundsForStep (step);
        const auto isActive = step < activeSteps;

        auto colour = disabledColour;

        if (isActive)
        {
            switch (getState (step))
            {
                case StepState::accent:  colour = accentColour;  break;
                case StepState::regular: colour = regularColour; break;
                case StepState::off:     colour = offColour;     break;
            }
        }

        g.setColour (colour);
        g.fillRoundedRectangle (cell, 3.0f);

        g.setColour (outlineColour);
        g.drawRoundedRectangle (cell, 3.0f, 1.0f);

        // Beat markers make it easy to count in fours.
        if (isActive && step % 4 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (cell.getX() + 1.0f, cell.getY() + 1.0f,
                        cell.getX() + 1.0f, cell.getBottom() - 1.0f, 2.0f);
        }

        if (step == highlightedStep)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (cell.reduced (1.0f), 3.0f, 2.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (isActive ? 0.55f : 0.18f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String (step + 1), cell, juce::Justification::centred, false);
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& event)
{
    const auto step = stepAt (event.getPosition());

    if (step < 0)
        return;

    // Left click cycles forwards, right click clears.
    const auto current = getState (step);
    auto next = StepState::off;

    if (! event.mods.isPopupMenu())
    {
        switch (current)
        {
            case StepState::off:     next = StepState::regular; break;
            case StepState::regular: next = StepState::accent;  break;
            case StepState::accent:  next = StepState::off;     break;
        }
    }

    setState (step, next);

    paintingStep = step;
    paintingState = next;

    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& event)
{
    const auto step = stepAt (event.getPosition());

    if (step < 0 || step == paintingStep || paintingStep < 0)
        return;

    // Dragging paints the same state across neighbouring steps.
    setState (step, paintingState);
    paintingStep = step;

    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    paintingStep = -1;
}

} // namespace click
