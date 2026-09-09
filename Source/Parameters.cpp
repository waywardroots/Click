#include "Parameters.h"
#include "ClickLibrary.h"

namespace click::params
{

namespace
{
    constexpr const char* slotPrefixes[numSlots] = { "acc", "reg" };

    constexpr int paramVersion = 1;

    juce::ParameterID id (const juce::String& text) { return { text, paramVersion }; }

    juce::NormalisableRange<float> frequencyRange (float low, float high, float defaultSkewCentre)
    {
        juce::NormalisableRange<float> range { low, high };
        range.setSkewForCentre (defaultSkewCentre);
        return range;
    }

    /** Envelope stage lengths are a share of the trimmed clip, so the full
        travel of every knob is useful whatever the sample's length. */
    juce::NormalisableRange<float> portionRange()
    {
        return { 0.0f, 100.0f, 0.1f };
    }
} // namespace

juce::String slotPrefix (int slot)
{
    return juce::isPositiveAndBelow (slot, numSlots) ? slotPrefixes[slot] : "acc";
}

juce::String slotParam (int slot, const char* suffix)
{
    return slotPrefix (slot) + suffix;
}

juce::String stepParam (int step)
{
    return "step" + juce::String (step).paddedLeft ('0', 2);
}

juce::StringArray divisionNames()
{
    return { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
}

double beatsPerStep (int divisionIndex)
{
    switch (divisionIndex)
    {
        case 0:  return 1.0;          // 1/4
        case 1:  return 0.5;          // 1/8
        case 2:  return 1.0 / 3.0;    // 1/8 triplet
        case 3:  return 0.25;         // 1/16
        case 4:  return 1.0 / 6.0;    // 1/16 triplet
        case 5:  return 0.125;        // 1/32
        default: return 0.5;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (
        id (hostSync), "Host Sync", true));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        id (tempo), "Tempo",
        juce::NormalisableRange<float> { 20.0f, 300.0f, 0.01f }, 120.0f,
        juce::AudioParameterFloatAttributes().withLabel ("BPM")));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        id (stepCount), "Steps", 1, maxSteps, 8));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        id (division), "Division", divisionNames(), 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        id (swing), "Swing",
        juce::NormalisableRange<float> { 0.0f, 0.75f, 0.001f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
                                          { return juce::String (value * 100.0f, 1) + " %"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        id (outputGain), "Output",
        juce::NormalisableRange<float> { -60.0f, 12.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // Silences the audio only. The sequencer, the voices and the playhead all
    // carry on exactly as they were.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        id (mute), "Mute", false));

    // --- the pattern ------------------------------------------------------
    const juce::StringArray stepChoices { "Off", "Regular", "Accent" };

    for (int step = 0; step < maxSteps; ++step)
    {
        // A four-on-the-floor default: accent on the downbeat, regular elsewhere.
        const auto defaultChoice = (step % 4 == 0) ? 2 : 1;

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            id (stepParam (step)),
            "Step " + juce::String (step + 1),
            stepChoices,
            defaultChoice));
    }

    // --- the two editable sound slots -------------------------------------
    juce::StringArray sourceChoices;

    for (int i = 0; i < library::numStockClicks; ++i)
        sourceChoices.add (library::getStockName (i));

    sourceChoices.add ("User Sample");

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto name = (slot == accentSlot) ? juce::String ("Accent ") : juce::String ("Regular ");

        // Accent defaults to the bell, regular to the studio beep.
        const auto defaultSource = (slot == accentSlot) ? 8 : 0;

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            id (slotParam (slot, source)), name + "Sound", sourceChoices, defaultSource));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, startTrim)), name + "Start",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f }, 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, endTrim)), name + "End",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0001f }, 1.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, gain)), name + "Level",
            juce::NormalisableRange<float> { -36.0f, 12.0f, 0.1f }, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, tune)), name + "Tune",
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("st")));

        // Stage lengths are a percentage of the trimmed clip rather than an
        // absolute time, so no part of a knob's travel is wasted on a short
        // click. The defaults are a no-op: no attack or release, and full
        // sustain, so a freshly loaded sample is left exactly as it is.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, attack)), name + "Attack",
            portionRange(), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, decay)), name + "Decay",
            portionRange(), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, sustain)), name + "Sustain",
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, release)), name + "Release",
            portionRange(), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, highPass)), name + "High Pass",
            frequencyRange (20.0f, 2000.0f, 200.0f), 20.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            id (slotParam (slot, lowPass)), name + "Low Pass",
            frequencyRange (200.0f, 20000.0f, 2500.0f), 20000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            id (slotParam (slot, reverse)), name + "Reverse", false));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            id (slotParam (slot, normalise)), name + "Normalise", false));
    }

    return layout;
}

} // namespace click::params
