# Click

A step-sequenced click / metronome generator plugin (VST3 + standalone), built with JUCE.

Ten stock click sounds, a 32-step sequencer with accents and swing, and a built-in
sample editor so you can shape the click — or load your own WAV and edit that instead.

## Features

**Step sequencer**
- Up to 32 steps, each one *off*, *regular*, or *accent*
- Click a step to cycle it, drag across steps to paint, right-click to clear
- Divisions: 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
- Swing (delays odd-numbered steps by up to 75% of a step)
- Locks to the host transport and tempo, or free-runs at its own BPM
- Sample-accurate scheduling: steps fire at the right offset *inside* the buffer,
  not just at block boundaries

**Two editable sounds**

The accent and regular steps each play their own slot, and each slot is
independently loadable and editable.

**Ten stock clicks** — Studio Beep, Wood Block, Rimshot, Cowbell, Stick Count,
Clave, Soft Tick, Noise Burst, Metronome Bell, Digital Blip.

All ten are synthesised from scratch by `tools/generate_clicks.py` (no sampled or
third-party audio), rendered at 48 kHz / 24-bit, and compiled into the binary — so
the plugin has no runtime file dependencies.

**Sample editor** — per slot:

| Control | Range | Notes |
| --- | --- | --- |
| Start / End | 0–100% | Draggable handles on the waveform |
| Level | −36 to +12 dB | |
| Tune | ±24 semitones | Applied at playback; baked in on export |
| Attack | 0–50 ms | |
| Decay | 1–100% | 100% keeps the sample's natural tail |
| High Pass | 20 Hz–2 kHz | 20 Hz = off |
| Low Pass | 200 Hz–20 kHz | 20 kHz = off |
| Reverse | on/off | |
| Normalise | on/off | Peaks at −0.5 dBFS |

- **Load WAV…** replaces a slot with any audio file the host can decode
  (WAV, AIFF, FLAC, Ogg, MP3)
- **Save WAV…** writes the edited click out as a 24-bit WAV, tuning included
- **Audition** fires the slot once so you can hear edits while the transport is stopped

The waveform view shows the raw sample dimmed underneath and the edited result on
top, so the effect of the filters, envelope and trim is visible as you work.

Every control is a real automatable parameter, and the plugin saves its full state
(including the path of any user sample) with the session.

## Building

Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.4 is fetched automatically.

```bash
git clone https://github.com/waywardroots/Click.git
cd Click
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

To build against a JUCE checkout you already have (also useful offline):

```bash
cmake -B build -DCLICK_JUCE_PATH=/path/to/JUCE
cmake --build build
```

Artefacts land in `build/Click_artefacts/Release/`:
`VST3/Click.vst3` and `Standalone/Click`.

**Linux** also needs the usual JUCE development packages:

```bash
sudo apt install libasound2-dev libfreetype-dev libx11-dev libxcomposite-dev \
  libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev
```

### Regenerating the stock clicks

The WAVs are committed, so this is only needed if you want to change them.
Requires `numpy`.

```bash
python3 tools/generate_clicks.py
```

## Layout

```
CMakeLists.txt              build definition
resources/clicks/*.wav      the ten stock clicks (embedded via BinaryData)
tools/generate_clicks.py    regenerates the stock clicks
Source/
  Parameters.*              parameter IDs and the APVTS layout
  ClickLibrary.*            decoding of stock clicks and user files
  ClickSample.h             RenderedClick + the biquad used by the editor
  ClickSlot.*               one sound: source audio + offline edit rendering
  VoicePool.h               allocation-free polyphonic sample playback
  PluginProcessor.*         parameters, sequencer, state
  StepGrid.*                the pattern editor
  WaveformDisplay.*         waveform view with draggable trim handles
  PluginEditor.*            the UI
```

### Threading

Edits are baked into a new buffer on the message thread, never on the audio
thread. The audio thread reads an atomic pointer to the current render and takes
a reference-counted copy, so a voice keeps playing the exact buffer it started
with even if the sample is edited mid-note. Renders are only freed once they have
been unreferenced for several message-thread ticks, which closes the window where
the audio thread has read the pointer but not yet taken its reference.

## Licence

The plugin source in this repository and the generated click samples are original
work. JUCE itself is fetched at build time and carries [its own licence
terms](https://juce.com/legal/juce-8-licence/) — review them before distributing
a binary.
