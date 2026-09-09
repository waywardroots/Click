#include "ClickLibrary.h"

#include <BinaryData.h>

namespace click::library
{

namespace
{
    struct StockEntry
    {
        const char* fileName;
        const char* displayName;
    };

    // Must stay in sync with tools/generate_clicks.py.
    constexpr StockEntry stockEntries[] =
    {
        { "01_studio_beep.wav",    "Studio Beep"    },
        { "02_wood_block.wav",     "Wood Block"     },
        { "03_rimshot.wav",        "Rimshot"        },
        { "04_cowbell.wav",        "Cowbell"        },
        { "05_stick_count.wav",    "Stick Count"    },
        { "06_clave.wav",          "Clave"          },
        { "07_soft_tick.wav",      "Soft Tick"      },
        { "08_noise_burst.wav",    "Noise Burst"    },
        { "09_metronome_bell.wav", "Metronome Bell" },
        { "10_digital_blip.wav",   "Digital Blip"   },
    };

    static_assert ((int) (sizeof (stockEntries) / sizeof (stockEntries[0])) == numStockClicks,
                   "stock click table size must match numStockClicks");

    /** Longest sample we will hold in a slot. Anything beyond this is truncated
        so that a stray drag-and-drop of a full song cannot exhaust memory. */
    constexpr int maxSampleFrames = 10 * 192000;

    /** BinaryData mangles resource names, so resolve them via the original
        filename table instead of guessing the mangled symbol. */
    const char* findResource (const juce::String& fileName, int& sizeInBytes)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (fileName == BinaryData::originalFilenames[i])
                return BinaryData::getNamedResource (BinaryData::namedResourceList[i], sizeInBytes);
        }

        sizeInBytes = 0;
        return nullptr;
    }

    double readFromReader (juce::AudioFormatReader* reader, juce::AudioBuffer<float>& destination)
    {
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return 0.0;

        const auto frames = (int) juce::jmin ((juce::int64) maxSampleFrames, reader->lengthInSamples);
        const auto channels = (int) juce::jmax (1u, juce::jmin (2u, reader->numChannels));

        destination.setSize (channels, frames, false, true, false);
        reader->read (&destination, 0, frames, 0, true, channels > 1);

        return reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
    }
} // namespace

juce::String getStockName (int index)
{
    if (juce::isPositiveAndBelow (index, numStockClicks))
        return stockEntries[index].displayName;

    return {};
}

juce::String getStockFileName (int index)
{
    if (juce::isPositiveAndBelow (index, numStockClicks))
        return stockEntries[index].fileName;

    return {};
}

double loadStockClick (int index, juce::AudioBuffer<float>& destination)
{
    if (! juce::isPositiveAndBelow (index, numStockClicks))
        return 0.0;

    int dataSize = 0;
    const auto* data = findResource (stockEntries[index].fileName, dataSize);

    if (data == nullptr || dataSize <= 0)
        return 0.0;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (
        wavFormat.createReaderFor (new juce::MemoryInputStream (data, (size_t) dataSize, false), true));

    return readFromReader (reader.get(), destination);
}

double loadFile (const juce::File& file,
                 juce::AudioBuffer<float>& destination,
                 juce::AudioFormatManager& formatManager)
{
    if (! file.existsAsFile())
        return 0.0;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    return readFromReader (reader.get(), destination);
}

} // namespace click::library
