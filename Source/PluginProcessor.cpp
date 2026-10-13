#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace click
{

namespace
{
    juce::Identifier userFileProperty (int slot)
    {
        return juce::Identifier (params::slotPrefix (slot) + "UserFile");
    }

    /** Click's RSA public half, from `keygen makepair`. It can only verify
        keys, never create them, so it is safe to commit and ship; the private
        half signs them and lives on the website alone.

        The exponent is 5, which is what JUCE picked for this pair - not a
        constant. Replacing this key means replacing the website's signing key
        at the same moment, and re-issuing every key already sold. */
    constexpr const char* licensePublicKey =
        "5,8598c288e14265057f8aadf9fbdf54184fa5ae012829eddbe4825293ace2cdde"
        "1044ee631478b021ad22b028d9a75dd1dfe145e48fea53ad9d67da19394d4c97"
        "ddfed409ccb63ee14a9a0a43c117fb5af10f67d3adc33b01a2d009cffe38fe30"
        "e2e67b41c7e74ff0cab6e07079b1eca596d7bb22b638c194d4bfe7eb8437f083";

    fuzzyaudio::LicenseManager::Config makeLicenseConfig()
    {
        fuzzyaudio::LicenseManager::Config c;
        c.productId       = "Click";
        c.publicKey       = licensePublicKey;
        c.vendorName      = "Fuzzy Audio";
        c.appName         = "Click";
        c.websiteName     = "fuzzyaudio.example";
        c.websiteURL      = juce::URL ("https://fuzzyaudio.example");
        c.activationToken = "click-license-v1";   // unique to Click

        // A key that is not two hex halves silently half-works: BigInteger
        // ignores non-hex characters, so a placeholder parses to whatever
        // digits are buried in it, the transform runs on a nonsense modulus,
        // and every customer's key comes back as "Credentials are invalid".
        jassert (c.publicKey.fromFirstOccurrenceOf (",", false, false)
                            .containsOnly ("0123456789abcdefABCDEF"));

        return c;
    }
}

ClickAudioProcessor::ClickAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CLICK", params::createLayout()),
      license (makeLicenseConfig())
{
    formatManager.registerBasicFormats();

    for (auto& pointer : activeRender)
        pointer.store (nullptr);

    hostSyncParam   = apvts.getRawParameterValue (params::hostSync);
    tempoParam      = apvts.getRawParameterValue (params::tempo);
    stepCountParam  = apvts.getRawParameterValue (params::stepCount);
    divisionParam   = apvts.getRawParameterValue (params::division);
    swingParam      = apvts.getRawParameterValue (params::swing);
    outputGainParam = apvts.getRawParameterValue (params::outputGain);
    muteParam       = apvts.getRawParameterValue (params::mute);

    for (int step = 0; step < maxSteps; ++step)
        stepParams[(size_t) step] = apvts.getRawParameterValue (params::stepParam (step));

    for (int slot = 0; slot < numSlots; ++slot)
    {
        tuneParams[(size_t) slot] = apvts.getRawParameterValue (params::slotParam (slot, params::tune));

        reloadSource (slot);
        renderSlot (slot);
    }

    startTimerHz (30);
}

ClickAudioProcessor::~ClickAudioProcessor()
{
    stopTimer();
}

// ---------------------------------------------------------------------------
// audio
// ---------------------------------------------------------------------------

float ClickAudioProcessor::currentOutputGain() const
{
    if (muteParam->load() > 0.5f)
        return 0.0f;

    return juce::Decibels::decibelsToGain (outputGainParam->load(), -60.0f);
}

void ClickAudioProcessor::prepareToPlay (double sampleRate, int)
{
    deviceRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    voices.prepare (deviceRate);
    outputGain.reset (deviceRate, 0.02);
    outputGain.setCurrentAndTargetValue (currentOutputGain());

    freeRunBeats = 0.0;
    displayStep.store (-1);
}

void ClickAudioProcessor::releaseResources()
{
    voices.reset();
}

bool ClickAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void ClickAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    buffer.clear();

    if (numSamples <= 0)
        return;

    if (const auto requested = auditionRequest.exchange (-1); requested >= 0)
        triggerSlot (requested, 1.0f);

    const auto syncToHost = hostSyncParam->load() > 0.5f;

    auto bpm = (double) tempoParam->load();
    auto playing = true;
    auto positionInBeats = freeRunBeats;

    if (syncToHost)
    {
        playing = false;

        if (auto* playHead = getPlayHead())
        {
            if (const auto position = playHead->getPosition())
            {
                if (const auto hostBpm = position->getBpm())
                    if (*hostBpm > 0.0)
                        bpm = *hostBpm;

                if (const auto ppq = position->getPpqPosition())
                {
                    positionInBeats = *ppq;
                    playing = position->getIsPlaying();
                }
            }
        }
    }

    running.store (playing);

    bpm = juce::jlimit (20.0, 300.0, bpm);

    const auto blockBeats = bpm / 60.0 / deviceRate * (double) numSamples;

    auto cursor = 0;

    if (playing)
    {
        const auto beatsPerStepValue = params::beatsPerStep ((int) std::round (divisionParam->load()));
        const auto count = juce::jlimit (1, maxSteps, (int) std::round (stepCountParam->load()));
        const auto swingAmount = juce::jlimit (0.0, 0.75, (double) swingParam->load());

        const auto startStep = positionInBeats / beatsPerStepValue;
        const auto endStep = (positionInBeats + blockBeats) / beatsPerStepValue;
        const auto span = endStep - startStep;

        // The half-open range [startStep, endStep) is contiguous from block to
        // block, so steps are never fired twice or skipped, and loops or
        // transport jumps simply land somewhere else in the pattern.
        if (span > 1.0e-9)
        {
            const auto firstIndex = (juce::int64) std::floor (startStep) - 1;
            const auto lastIndex = (juce::int64) std::ceil (endStep) + 1;

            for (auto index = firstIndex; index <= lastIndex; ++index)
            {
                const auto scheduled = (double) index + (index % 2 != 0 ? swingAmount : 0.0);

                if (scheduled < startStep || scheduled >= endStep)
                    continue;

                const auto offset = juce::jlimit (0, numSamples - 1,
                                                  (int) ((scheduled - startStep) / span * (double) numSamples));

                if (offset > cursor)
                {
                    voices.render (buffer, cursor, offset - cursor);
                    cursor = offset;
                }

                fireStep ((int) (((index % count) + count) % count));
            }
        }
    }

    voices.render (buffer, cursor, numSamples - cursor);

    if (! syncToHost)
        freeRunBeats += blockBeats;

    // Muting only zeroes the gain: the sequencer, the voices and the playhead
    // all keep running, and the smoothed ramp keeps the transition click-free.
    outputGain.setTargetValue (currentOutputGain());
    outputGain.applyGain (buffer, numSamples);
}

void ClickAudioProcessor::fireStep (int patternIndex)
{
    if (! juce::isPositiveAndBelow (patternIndex, maxSteps))
        return;

    displayStep.store (patternIndex);

    const auto state = (StepState) (int) std::round (stepParams[(size_t) patternIndex]->load());

    if (state == StepState::off)
        return;

    triggerSlot (state == StepState::accent ? accentSlot : regularSlot, 1.0f);
}

void ClickAudioProcessor::triggerSlot (int slot, float velocity)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    auto* raw = activeRender[(size_t) slot].load();

    if (raw == nullptr)
        return;

    // Taking a reference here keeps the buffer alive for the whole voice, even
    // if the user edits the slot mid-note.
    RenderedClick::Ptr clip (raw);

    const auto semitones = (double) tuneParams[(size_t) slot]->load();
    voices.trigger (clip, velocity, std::pow (2.0, semitones / 12.0));
}

void ClickAudioProcessor::auditionSlot (int slot)
{
    auditionRequest.store (slot);
}

void ClickAudioProcessor::randomisePattern()
{
    const auto count = juce::jlimit (1, maxSteps, (int) std::round (stepCountParam->load()));

    for (int step = 0; step < maxSteps; ++step)
    {
        auto state = StepState::off;

        if (step < count)
        {
            const auto roll = random.nextFloat();

            if (step == 0)
            {
                // Always land the downbeat, so the result is still usable as a
                // click rather than an arbitrary scatter of hits.
                state = StepState::accent;
            }
            else if (step % 4 == 0)
            {
                // Strong positions sound more often, and are the only places
                // that pick up further accents.
                state = roll < 0.25f ? StepState::accent
                      : roll < 0.90f ? StepState::regular
                                     : StepState::off;
            }
            else
            {
                state = roll < 0.55f ? StepState::regular : StepState::off;
            }
        }

        if (auto* parameter = apvts.getParameter (params::stepParam (step)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (int) state));
            parameter->endChangeGesture();
        }
    }
}

// ---------------------------------------------------------------------------
// slots and rendering (message thread)
// ---------------------------------------------------------------------------

void ClickAudioProcessor::timerCallback()
{
    auto changed = false;

    if (stateReloadPending.exchange (false))
    {
        applyPendingState();
        changed = true;
    }

    for (int slot = 0; slot < numSlots; ++slot)
    {
        if (readSourceIndex (slot) != cachedSourceIndex[(size_t) slot])
        {
            reloadSource (slot);
            renderSlot (slot);
            changed = true;
        }
        else if (readSettings (slot) != cachedSettings[(size_t) slot])
        {
            renderSlot (slot);
            changed = true;
        }
    }

    collectGarbage();

    if (changed)
        sendChangeMessage();
}

int ClickAudioProcessor::readSourceIndex (int slot) const
{
    return (int) std::round (apvts.getRawParameterValue (params::slotParam (slot, params::source))->load());
}

EditSettings ClickAudioProcessor::readSettings (int slot) const
{
    const auto value = [this, slot] (const char* suffix)
    {
        return apvts.getRawParameterValue (params::slotParam (slot, suffix))->load();
    };

    EditSettings settings;
    settings.start = value (params::startTrim);
    settings.end = value (params::endTrim);
    settings.gainDb = value (params::gain);
    settings.attackPortion = value (params::attack) * 0.01f;
    settings.decayPortion = value (params::decay) * 0.01f;
    settings.sustainLevel = value (params::sustain) * 0.01f;
    settings.releasePortion = value (params::release) * 0.01f;
    settings.highPassHz = value (params::highPass);
    settings.lowPassHz = value (params::lowPass);
    settings.reverse = value (params::reverse) > 0.5f;
    settings.normalise = value (params::normalise) > 0.5f;

    return settings;
}

void ClickAudioProcessor::reloadSource (int slot)
{
    const auto index = readSourceIndex (slot);
    cachedSourceIndex[(size_t) slot] = index;

    if (index < library::numStockClicks)
    {
        slots[(size_t) slot].loadStock (index);
        return;
    }

    // "User Sample" selected - restore the remembered file if we still have it.
    const auto file = slots[(size_t) slot].getUserFile();

    if (file.existsAsFile())
        slots[(size_t) slot].loadUserFile (file, formatManager);
}

void ClickAudioProcessor::renderSlot (int slot)
{
    const auto settings = readSettings (slot);
    cachedSettings[(size_t) slot] = settings;

    auto rendered = slots[(size_t) slot].render (settings);

    renderPool.add (rendered.get());
    currentRender[(size_t) slot] = rendered;
    activeRender[(size_t) slot].store (rendered.get());
}

void ClickAudioProcessor::applyPendingState()
{
    juce::StringArray paths;

    {
        const juce::ScopedLock lock (pendingLock);
        paths = pendingUserFiles;
    }

    // Restore the user samples first so that reloading the sources below finds
    // them already in place.
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const juce::File file (paths[slot]);

        if (file.existsAsFile())
            slots[(size_t) slot].loadUserFile (file, formatManager);
    }

    for (int slot = 0; slot < numSlots; ++slot)
    {
        reloadSource (slot);
        renderSlot (slot);
    }
}

void ClickAudioProcessor::collectGarbage()
{
    const auto isLive = [this] (const RenderedClick* object)
    {
        for (const auto& pointer : activeRender)
            if (pointer.load() == object)
                return true;

        return false;
    };

    for (int i = renderPool.size(); --i >= 0;)
    {
        auto* object = renderPool.getObjectPointerUnchecked (i);

        if (object == nullptr)
            continue;

        // Reference count of one means only the pool still holds it. Waiting a
        // few ticks before freeing closes the window where the audio thread has
        // read the raw pointer but not yet taken its reference.
        if (! isLive (object) && object->getReferenceCount() == 1)
        {
            if (++object->idleTicks > 3)
                renderPool.remove (i);
        }
        else
        {
            object->idleTicks = 0;
        }
    }
}

RenderedClick::Ptr ClickAudioProcessor::getRenderedClip (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return nullptr;

    return currentRender[(size_t) slot];
}

bool ClickAudioProcessor::loadUserSample (int slot, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return false;

    if (! slots[(size_t) slot].loadUserFile (file, formatManager))
        return false;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            apvts.getParameter (params::slotParam (slot, params::source))))
    {
        choice->beginChangeGesture();
        choice->setValueNotifyingHost (choice->convertTo0to1 ((float) library::numStockClicks));
        choice->endChangeGesture();
    }

    // Mark the source as already handled so the timer does not reload it.
    cachedSourceIndex[(size_t) slot] = library::numStockClicks;

    renderSlot (slot);
    sendChangeMessage();

    return true;
}

bool ClickAudioProcessor::exportSlot (int slot, const juce::File& destination)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return false;

    auto clip = currentRender[(size_t) slot];

    if (clip == nullptr || clip->buffer.getNumSamples() < 2)
        return false;

    // Bake the tuning in, so the exported file matches what the plugin plays.
    const auto ratio = std::pow (2.0, (double) tuneParams[(size_t) slot]->load() / 12.0);
    const auto& source = clip->buffer;
    const auto numChannels = source.getNumChannels();
    const auto sourceFrames = source.getNumSamples();
    const auto outputFrames = juce::jmax (1, (int) std::floor ((double) (sourceFrames - 1) / ratio));

    juce::AudioBuffer<float> output (numChannels, outputFrames);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* in = source.getReadPointer (channel);
        auto* out = output.getWritePointer (channel);

        for (int i = 0; i < outputFrames; ++i)
        {
            const auto position = (double) i * ratio;
            const auto index = juce::jlimit (0, sourceFrames - 1, (int) position);
            const auto nextIndex = juce::jmin (sourceFrames - 1, index + 1);
            const auto frac = (float) (position - (double) index);

            out[i] = in[index] + frac * (in[nextIndex] - in[index]);
        }
    }

    destination.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (destination.createOutputStream());

    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (stream.get(), clip->sourceRate,
                                   (unsigned int) numChannels, 24, {}, 0));

    if (writer == nullptr)
        return false;

    stream.release(); // the writer owns the stream from here on
    return writer->writeFromAudioSampleBuffer (output, 0, outputFrames);
}

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

void ClickAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    for (int slot = 0; slot < numSlots; ++slot)
        state.setProperty (userFileProperty (slot),
                           slots[(size_t) slot].getUserFile().getFullPathName(),
                           nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ClickAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    {
        const juce::ScopedLock lock (pendingLock);

        pendingUserFiles.clear();

        for (int slot = 0; slot < numSlots; ++slot)
            pendingUserFiles.add (tree.getProperty (userFileProperty (slot)).toString());
    }

    apvts.replaceState (tree);

    // The slots themselves are reloaded on the message thread by the timer.
    stateReloadPending.store (true);
}

juce::AudioProcessorEditor* ClickAudioProcessor::createEditor()
{
    return new ClickAudioProcessorEditor (*this);
}

} // namespace click

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new click::ClickAudioProcessor();
}
