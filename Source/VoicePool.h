#pragma once

#include "ClickSample.h"
#include <array>

namespace click
{

/** A small fixed pool of sample playback voices.

    Everything here runs on the audio thread and never allocates: triggering a
    voice only copies a reference-counted pointer, and the buffers themselves
    are rendered ahead of time on the message thread.
*/
class VoicePool
{
public:
    static constexpr int numVoices = 16;

    void prepare (double newDeviceRate)
    {
        deviceRate = newDeviceRate > 0.0 ? newDeviceRate : 44100.0;
        reset();
    }

    void reset()
    {
        for (auto& voice : voices)
        {
            voice.active = false;
            voice.clip = nullptr;
        }
    }

    /** Starts `clip` on a free voice, stealing the oldest one if necessary. */
    void trigger (RenderedClick::Ptr clip, float gain, double pitchRatio)
    {
        if (clip == nullptr || clip->buffer.getNumSamples() <= 1)
            return;

        auto* target = &voices[0];

        for (auto& voice : voices)
        {
            if (! voice.active)
            {
                target = &voice;
                break;
            }

            if (voice.age < target->age)
                target = &voice;
        }

        target->clip = clip;
        target->position = 0.0;
        target->increment = juce::jlimit (0.01, 32.0, (clip->sourceRate / deviceRate) * pitchRatio);
        target->gain = gain;
        target->age = ++counter;
        target->active = true;
    }

    /** Mixes all active voices into `output` over the given range. */
    void render (juce::AudioBuffer<float>& output, int startSample, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const auto numOutputChannels = output.getNumChannels();

        for (auto& voice : voices)
        {
            if (! voice.active || voice.clip == nullptr)
                continue;

            const auto& clip = voice.clip->buffer;
            const auto clipFrames = clip.getNumSamples();
            const auto clipChannels = clip.getNumChannels();
            const auto lastReadable = (double) (clipFrames - 2);

            auto position = voice.position;

            for (int i = 0; i < numSamples; ++i)
            {
                if (position > lastReadable)
                {
                    voice.active = false;
                    break;
                }

                const auto index = (int) position;
                const auto frac = (float) (position - (double) index);

                for (int channel = 0; channel < numOutputChannels; ++channel)
                {
                    const auto sourceChannel = juce::jmin (channel, clipChannels - 1);
                    const auto* source = clip.getReadPointer (sourceChannel);
                    const auto value = source[index] + frac * (source[index + 1] - source[index]);

                    output.addSample (channel, startSample + i, value * voice.gain);
                }

                position += voice.increment;
            }

            voice.position = position;

            if (! voice.active)
                voice.clip = nullptr;
        }
    }

private:
    struct Voice
    {
        RenderedClick::Ptr clip;
        double position = 0.0;
        double increment = 1.0;
        float gain = 1.0f;
        juce::uint32 age = 0;
        bool active = false;
    };

    std::array<Voice, (size_t) numVoices> voices;
    double deviceRate = 44100.0;
    juce::uint32 counter = 0;
};

} // namespace click
