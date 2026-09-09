#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Parameters.h"
#include "ClickSlot.h"
#include "VoicePool.h"

namespace click
{

/** Click - a step-sequenced click / metronome generator with an on-board
    sample editor.

    The plugin holds two sound slots (accent and regular). Each can be loaded
    from the ten built-in clicks or from any audio file on disk, and each has an
    independent set of destructive-looking but fully non-destructive edits.

    Threading model:
      - ClickSlot and the render pool are message-thread only.
      - The audio thread reads `activeRender[slot]`, takes a reference-counted
        copy, and plays it. Renders are only destroyed once they have been
        unreferenced for several message-thread ticks.
*/
class ClickAudioProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster,
                            private juce::Timer
{
public:
    ClickAudioProcessor();
    ~ClickAudioProcessor() override;

    // --- AudioProcessor ---------------------------------------------------
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    // Driven by the CMake NEEDS_MIDI_* flags so the wrapper's declared buses
    // and these answers can never disagree. Ableton (and auval) reject an
    // instrument that reports no MIDI input, so Click accepts MIDI and ignores
    // it rather than claiming to have no input at all.
    bool acceptsMidi() const override { return JucePlugin_WantsMidiInput != 0; }
    bool producesMidi() const override { return JucePlugin_ProducesMidiOutput != 0; }
    bool isMidiEffect() const override { return JucePlugin_IsMidiEffect != 0; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- editor support (message thread) ----------------------------------
    juce::AudioProcessorValueTreeState apvts;

    const ClickSlot& getSlot (int slot) const { return slots[(size_t) slot]; }

    /** The baked buffer currently being played for a slot, for display. */
    RenderedClick::Ptr getRenderedClip (int slot) const;

    /** Loads an audio file into a slot and switches the slot's source to it. */
    bool loadUserSample (int slot, const juce::File& file);

    /** Writes the edited click (including tuning) out as a 24-bit WAV. */
    bool exportSlot (int slot, const juce::File& destination);

    /** Plays a slot once, so the user can hear their edits while stopped. */
    void auditionSlot (int slot);

    /** Index of the step most recently reached by the sequencer, or -1. */
    int getDisplayStep() const noexcept { return displayStep.load(); }

    /** The slot's current edit settings, as the renderer sees them. */
    EditSettings readSettings (int slot) const;

    /** True while the sequencer is advancing. */
    bool isRunning() const noexcept { return running.load(); }

    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

private:
    void timerCallback() override;

    /** Output gain including the mute, so prepareToPlay and processBlock
        cannot disagree about the level. */
    float currentOutputGain() const;

    int readSourceIndex (int slot) const;
    void reloadSource (int slot);
    void renderSlot (int slot);
    void collectGarbage();
    void applyPendingState();

    void triggerSlot (int slot, float velocity);
    void fireStep (int patternIndex);

    juce::AudioFormatManager formatManager;

    std::array<ClickSlot, (size_t) numSlots> slots;
    std::array<EditSettings, (size_t) numSlots> cachedSettings;
    std::array<int, (size_t) numSlots> cachedSourceIndex { -1, -1 };
    std::array<RenderedClick::Ptr, (size_t) numSlots> currentRender;
    std::array<std::atomic<RenderedClick*>, (size_t) numSlots> activeRender;

    /** Owns every render until it is provably unreferenced. */
    juce::ReferenceCountedArray<RenderedClick> renderPool;

    VoicePool voices;
    double deviceRate = 44100.0;

    // cached parameter pointers
    std::atomic<float>* hostSyncParam = nullptr;
    std::atomic<float>* tempoParam = nullptr;
    std::atomic<float>* stepCountParam = nullptr;
    std::atomic<float>* divisionParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* muteParam = nullptr;
    std::array<std::atomic<float>*, (size_t) maxSteps> stepParams {};
    std::array<std::atomic<float>*, (size_t) numSlots> tuneParams {};

    juce::SmoothedValue<float> outputGain;

    // Hosts may call setStateInformation off the message thread, so the actual
    // sample loading and rendering is deferred to the timer.
    juce::CriticalSection pendingLock;
    juce::StringArray pendingUserFiles;
    std::atomic<bool> stateReloadPending { false };

    // sequencer state (audio thread)
    double freeRunBeats = 0.0;
    std::atomic<int> displayStep { -1 };
    std::atomic<bool> running { false };
    std::atomic<int> auditionRequest { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickAudioProcessor)
};

} // namespace click
