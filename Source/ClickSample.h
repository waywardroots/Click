#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace click
{

/** A click sample that has been fully processed by the sample editor and is
    ready for the audio thread to play back verbatim.

    Instances are reference counted so that a voice can hold on to the exact
    buffer it started playing even after the user edits the slot and a new
    render replaces it. See ClickProcessor::renderPool for the lifetime rules.
*/
struct RenderedClick : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RenderedClick>;

    juce::AudioBuffer<float> buffer;

    /** Sample rate the buffer is stored at. Playback compensates for any
        mismatch with the device rate, so no offline resampling is needed. */
    double sourceRate = 48000.0;

    /** Consecutive message-thread ticks this render has been unreferenced for.
        Used to defer deletion long enough that the audio thread can never be
        mid-way through taking a reference when the object goes away. */
    int idleTicks = 0;
};

/** Minimal RBJ biquad used for the slot's offline high-pass / low-pass stages.

    The filters only ever run on the message thread while rendering a click, so
    a direct-form-I implementation with double state is more than fast enough.
*/
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }

    void setLowPass (double frequency, double sampleRate, double q = 0.7071)
    {
        const auto w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto cosW0 = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * q);

        setCoefficients ((1.0 - cosW0) * 0.5,
                          1.0 - cosW0,
                         (1.0 - cosW0) * 0.5,
                          1.0 + alpha,
                         -2.0 * cosW0,
                          1.0 - alpha);
    }

    void setHighPass (double frequency, double sampleRate, double q = 0.7071)
    {
        const auto w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const auto cosW0 = std::cos (w0);
        const auto alpha = std::sin (w0) / (2.0 * q);

        setCoefficients ((1.0 + cosW0) * 0.5,
                         -(1.0 + cosW0),
                          (1.0 + cosW0) * 0.5,
                           1.0 + alpha,
                          -2.0 * cosW0,
                           1.0 - alpha);
    }

    float process (float input) noexcept
    {
        const auto x0 = (double) input;
        const auto out = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        x2 = x1;
        x1 = x0;
        y2 = y1;
        y1 = out;

        return (float) out;
    }

private:
    void setCoefficients (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0;
        b1 = nb1 / na0;
        b2 = nb2 / na0;
        a1 = na1 / na0;
        a2 = na2 / na0;
        reset();
    }
};

} // namespace click
