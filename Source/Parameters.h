#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace click
{

/** Maximum number of steps in the sequencer pattern. */
inline constexpr int maxSteps = 32;

/** The plugin holds two independently editable click sounds. */
inline constexpr int numSlots = 2;

enum SlotIndex
{
    accentSlot = 0,
    regularSlot = 1
};

/** What a single step of the sequencer does. */
enum class StepState
{
    off = 0,
    regular,
    accent
};

namespace params
{
    // --- global -----------------------------------------------------------
    inline constexpr const char* hostSync   = "hostSync";
    inline constexpr const char* tempo      = "tempo";
    inline constexpr const char* stepCount  = "stepCount";
    inline constexpr const char* division   = "division";
    inline constexpr const char* swing      = "swing";
    inline constexpr const char* outputGain = "outputGain";

    // --- per slot (suffixed with the slot prefix) --------------------------
    inline constexpr const char* source    = "Source";
    inline constexpr const char* startTrim = "Start";
    inline constexpr const char* endTrim   = "End";
    inline constexpr const char* gain      = "Gain";
    inline constexpr const char* tune      = "Tune";
    inline constexpr const char* attack    = "Attack";
    inline constexpr const char* decay     = "Decay";
    inline constexpr const char* highPass  = "HighPass";
    inline constexpr const char* lowPass   = "LowPass";
    inline constexpr const char* reverse   = "Reverse";
    inline constexpr const char* normalise = "Normalise";

    /** Prefix used to build the parameter IDs for a slot. */
    juce::String slotPrefix (int slot);

    /** Full parameter ID for a slot parameter, e.g. "accGain". */
    juce::String slotParam (int slot, const char* suffix);

    /** Parameter ID for a step, e.g. "step07". */
    juce::String stepParam (int step);

    /** Number of beats occupied by one step for the given division index. */
    double beatsPerStep (int divisionIndex);

    /** Human readable division names, in parameter order. */
    juce::StringArray divisionNames();

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace params

} // namespace click
