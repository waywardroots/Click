#pragma once

#include "ClickSample.h"
#include "ClickLibrary.h"

namespace click
{

/** Everything the sample editor can do to a click, other than tuning (which is
    applied at playback time by changing the voice's read increment). */
struct EditSettings
{
    float start = 0.0f;        // normalised trim start (0-1)
    float end = 1.0f;          // normalised trim end   (0-1)
    float gainDb = 0.0f;
    float attackMs = 0.0f;
    float decayMs = 0.0f;
    float sustainLevel = 1.0f;     // 0-1; 1 with no decay leaves the sample alone
    float releaseMs = 0.0f;
    float highPassHz = 20.0f;      // 20 Hz disables the filter
    float lowPassHz = 20000.0f;    // 20 kHz disables the filter
    bool reverse = false;
    bool normalise = false;

    bool operator== (const EditSettings& other) const noexcept
    {
        return start == other.start
            && end == other.end
            && gainDb == other.gainDb
            && attackMs == other.attackMs
            && decayMs == other.decayMs
            && sustainLevel == other.sustainLevel
            && releaseMs == other.releaseMs
            && highPassHz == other.highPassHz
            && lowPassHz == other.lowPassHz
            && reverse == other.reverse
            && normalise == other.normalise;
    }

    bool operator!= (const EditSettings& other) const noexcept { return ! (*this == other); }
};

/** The ADSR value at a point in time, for a clip of the given length.

    The renderer and the envelope display both call this, so what is drawn is
    always exactly what is applied. The envelope is anchored to the trimmed
    sample: attack and decay run from the start, sustain holds the body, and
    release ends precisely at the last sample. Very short fades are folded in at
    both ends so a click can never begin or end on a discontinuity.
*/
double envelopeValueAt (double timeSeconds, double lengthSeconds, const EditSettings& settings);

/** Holds one of the plugin's two click sounds: the raw audio it was loaded
    from, plus the ability to bake the current edit settings into a buffer that
    the audio thread can play directly.

    All members are only ever touched from the message thread.
*/
class ClickSlot
{
public:
    ClickSlot() = default;

    /** Replaces the contents with one of the built-in clicks. */
    bool loadStock (int stockIndex);

    /** Replaces the contents with an audio file from disk. */
    bool loadUserFile (const juce::File& file, juce::AudioFormatManager& formatManager);

    const juce::AudioBuffer<float>& getSource() const noexcept { return source; }
    double getSourceRate() const noexcept { return sourceRate; }
    bool isEmpty() const noexcept { return source.getNumSamples() == 0; }

    juce::String getDisplayName() const { return displayName; }
    juce::File getUserFile() const { return userFile; }

    /** Bakes `settings` into a new buffer. Never returns null. */
    RenderedClick::Ptr render (const EditSettings& settings) const;

private:
    juce::AudioBuffer<float> source;
    double sourceRate = 48000.0;
    juce::String displayName { "Empty" };
    juce::File userFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickSlot)
};

} // namespace click
