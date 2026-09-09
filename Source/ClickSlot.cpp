#include "ClickSlot.h"

namespace click
{

namespace
{
    /** Shortest render we will ever produce, so a fully closed trim range can
        never yield an empty (or negative length) buffer. */
    constexpr int minRenderFrames = 16;

    void applyFilters (juce::AudioBuffer<float>& buffer, double rate, float highPassHz, float lowPassHz)
    {
        const auto wantsHighPass = highPassHz > 20.5f;
        const auto wantsLowPass = lowPassHz < 19900.0f;

        if (! wantsHighPass && ! wantsLowPass)
            return;

        const auto nyquist = rate * 0.5;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            Biquad highPass, lowPass;

            if (wantsHighPass)
                highPass.setHighPass (juce::jlimit (10.0, nyquist * 0.9, (double) highPassHz), rate);

            if (wantsLowPass)
                lowPass.setLowPass (juce::jlimit (10.0, nyquist * 0.9, (double) lowPassHz), rate);

            auto* samples = buffer.getWritePointer (channel);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                auto value = samples[i];

                if (wantsHighPass)
                    value = highPass.process (value);

                if (wantsLowPass)
                    value = lowPass.process (value);

                samples[i] = value;
            }
        }
    }

    void applyEnvelope (juce::AudioBuffer<float>& buffer, double rate, const EditSettings& settings)
    {
        const auto numFrames = buffer.getNumSamples();

        if (numFrames <= 0)
            return;

        const auto lengthSeconds = (double) numFrames / rate;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer (channel);

            for (int i = 0; i < numFrames; ++i)
                samples[i] = (float) (samples[i] * envelopeValueAt ((double) i / rate, lengthSeconds, settings));
        }
    }
} // namespace

double envelopeValueAt (double timeSeconds, double lengthSeconds, const EditSettings& settings)
{
    if (lengthSeconds <= 0.0)
        return 0.0;

    const auto time = juce::jlimit (0.0, lengthSeconds, timeSeconds);

    // Stage lengths are shares of the clip, so a full knob always spans the
    // whole sample. Release is reserved at the end, then attack and decay take
    // what is left, so no combination can overrun the sample.
    const auto release = juce::jlimit (0.0, 1.0, (double) settings.releasePortion) * lengthSeconds;
    const auto beforeRelease = lengthSeconds - release;
    const auto attack = juce::jmin (juce::jlimit (0.0, 1.0, (double) settings.attackPortion) * lengthSeconds,
                                    beforeRelease);
    const auto decay = juce::jmin (juce::jlimit (0.0, 1.0, (double) settings.decayPortion) * lengthSeconds,
                                   beforeRelease - attack);
    const auto sustain = juce::jlimit (0.0, 1.0, (double) settings.sustainLevel);

    double envelope;

    if (attack > 0.0 && time < attack)
    {
        envelope = time / attack;
    }
    else if (decay > 0.0 && time < attack + decay)
    {
        // Exponential fall towards the sustain level, which sounds far more
        // natural on a percussive click than a straight line.
        envelope = sustain + (1.0 - sustain) * std::exp (-3.0 * (time - attack) / decay);
    }
    else if (time < beforeRelease || release <= 0.0)
    {
        envelope = sustain;
    }
    else
    {
        envelope = sustain * std::exp (-3.0 * (time - beforeRelease) / release);
    }

    // Guarantee a click-free start and end whatever the stage times are.
    constexpr double fadeIn = 0.0005;
    constexpr double fadeOut = 0.002;

    if (time < fadeIn)
        envelope *= time / fadeIn;

    const auto remaining = lengthSeconds - time;

    if (remaining < fadeOut)
        envelope *= remaining / fadeOut;

    return juce::jlimit (0.0, 1.0, envelope);
}

bool ClickSlot::loadStock (int stockIndex)
{
    juce::AudioBuffer<float> loaded;
    const auto rate = library::loadStockClick (stockIndex, loaded);

    if (rate <= 0.0)
        return false;

    source = std::move (loaded);
    sourceRate = rate;
    displayName = library::getStockName (stockIndex);

    // userFile is deliberately kept, so switching back to "User Sample"
    // restores the last file the user picked.

    return true;
}

bool ClickSlot::loadUserFile (const juce::File& file, juce::AudioFormatManager& formatManager)
{
    juce::AudioBuffer<float> loaded;
    const auto rate = library::loadFile (file, loaded, formatManager);

    if (rate <= 0.0)
        return false;

    source = std::move (loaded);
    sourceRate = rate;
    displayName = file.getFileNameWithoutExtension();
    userFile = file;

    return true;
}

RenderedClick::Ptr ClickSlot::render (const EditSettings& settings) const
{
    RenderedClick::Ptr result = new RenderedClick();
    result->sourceRate = sourceRate;

    const auto sourceFrames = source.getNumSamples();
    const auto numChannels = juce::jmax (1, source.getNumChannels());

    if (sourceFrames <= 0)
    {
        result->buffer.setSize (1, minRenderFrames, false, true, false);
        result->buffer.clear();
        return result;
    }

    auto startNorm = juce::jlimit (0.0f, 1.0f, settings.start);
    auto endNorm = juce::jlimit (0.0f, 1.0f, settings.end);

    if (endNorm < startNorm)
        std::swap (startNorm, endNorm);

    auto startFrame = juce::jlimit (0, sourceFrames - 1, (int) std::round (startNorm * (float) sourceFrames));
    auto endFrame = juce::jlimit (0, sourceFrames, (int) std::round (endNorm * (float) sourceFrames));

    if (endFrame - startFrame < minRenderFrames)
    {
        endFrame = juce::jmin (sourceFrames, startFrame + minRenderFrames);
        startFrame = juce::jmax (0, endFrame - minRenderFrames);
    }

    const auto numFrames = juce::jmax (minRenderFrames, endFrame - startFrame);

    result->buffer.setSize (numChannels, numFrames, false, true, false);
    result->buffer.clear();

    const auto framesToCopy = juce::jmin (numFrames, sourceFrames - startFrame);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto sourceChannel = juce::jmin (channel, source.getNumChannels() - 1);
        result->buffer.copyFrom (channel, 0, source, sourceChannel, startFrame, framesToCopy);
    }

    if (settings.reverse)
        result->buffer.reverse (0, numFrames);

    applyFilters (result->buffer, sourceRate, settings.highPassHz, settings.lowPassHz);
    applyEnvelope (result->buffer, sourceRate, settings);

    if (settings.normalise)
    {
        const auto peak = result->buffer.getMagnitude (0, numFrames);

        if (peak > 1.0e-6f)
            result->buffer.applyGain (juce::Decibels::decibelsToGain (-0.5f) / peak);
    }

    result->buffer.applyGain (juce::Decibels::decibelsToGain (settings.gainDb));

    return result;
}

} // namespace click
