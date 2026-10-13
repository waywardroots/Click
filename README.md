# Click

A step-sequenced click / metronome generator plugin (VST3, AU, standalone), built with JUCE.

Ten stock click sounds, a 32-step sequencer with accents and swing, and a built-in
sample editor so you can shape the click — or load your own WAV and edit that instead.

## Features

**Step sequencer**
- Up to 32 steps, each one *off*, *regular*, or *accent*
- Click a step to cycle it, drag across steps to paint, right-click to clear
- **Randomise** fills the active steps with a new pattern: the downbeat always
  lands, on-beat positions sound more often, and accents are kept to strong
  positions, so the result stays usable as a click
- Divisions: 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
- Swing (delays odd-numbered steps by up to 75% of a step)
- Locks to the host transport and tempo, or free-runs at its own BPM
- **Mute** silences the audio without stopping anything: the sequencer keeps
  running, the playhead keeps moving, and voices already sounding carry on, so
  unmuting drops you straight back in time rather than restarting
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
| Attack | 0–100% of clip | Rise from silence |
| Decay | 0–100% of clip | Falls to the sustain level |
| Sustain | 0–100% | Level held through the body of the click |
| Release | 0–100% of clip | Fade ending exactly on the last sample |

Envelope stage lengths are a percentage of the trimmed clip rather than an
absolute time, so the full travel of every control stays useful whether the
sample is a 45 ms tick or a 600 ms bell.

Decay falls from the peak *to the sustain level*, so with Sustain at 100% it has
nothing to do — the control is greyed out in that case. Lower Sustain to make
Decay bite.
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

## Builds

Every push builds Windows, macOS and Linux on GitHub Actions
(`.github/workflows/build.yml`). Installable plugins are attached to each run as
downloadable artefacts — open the **Actions** tab, pick a run, and grab
`Click-Windows`, `Click-macOS` or `Click-Linux` from the Artifacts section.
The Windows and macOS runs also produce installers, as
`Click-Windows-Installer` and `Click-macOS-Installer`.

| Platform | Formats | Notes |
| --- | --- | --- |
| Windows | VST3, Standalone | x64 |
| macOS | VST3, AU, Standalone | Universal (arm64 + x86_64), macOS 10.13+ |
| Linux | VST3, Standalone | x64 |

The macOS artefacts are **not code signed or notarised**. To load them locally
you will need to clear quarantine:

```bash
xattr -dr com.apple.quarantine Click.vst3 Click.component
```

### Installing

Windows and macOS have installers, built by the same CI run and attached as
`Click-Windows-Installer` and `Click-macOS-Installer`. Both present the EULA
first and stop if it is declined, and both let you choose what to install and
where it goes.

| Platform | Installer | Choices it offers |
| --- | --- | --- |
| Windows | `Click-1.0.0-Windows.exe` | VST3 and/or standalone, and the folder for each |
| macOS | `Click-1.0.0-macOS.pkg` | VST3, AU and/or standalone, installed for all users or just you |

The Windows installer defaults the plugin to `C:\Program Files\Common
Files\VST3\` and the application to `C:\Program Files\Fuzzy Audio\Click\`,
and the folder for each is editable on its own page of the wizard. It needs
an administrator, because the default plugin folder does. Uninstall from
*Apps & features*.

The macOS package offers **Install for all users** (`/Library/Audio/Plug-Ins`,
`/Applications`) or **Install for me only**, which writes the same layout under
your home folder. Those are the folders hosts scan, so the destination is a
choice between the two rather than a free-form path: a plugin anywhere else is
never found.

Installing by hand works too — drop the bundles in:

| Platform | VST3 | AU |
| --- | --- | --- |
| Windows | `C:\Program Files\Common Files\VST3\` | — |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `~/.vst3/` | — |

Click is an **instrument**, so it loads on a MIDI track. It generates its own
click from the sequencer and ignores any MIDI sent to it.

### Building the installers

Build Click first; both scripts package what CMake left in
`build/Click_artefacts/Release` and write to `build/installer`.

```bash
# Windows, with Inno Setup 6 installed
iscc packaging\windows\Click.iss

# macOS
packaging/macos/build-installer.sh
```

The Windows script takes `/DAppVersion=` and `/DArtefacts=` to override the
version and the artefact folder; the macOS script takes the artefact and output
folders as arguments. Both read the licence text from `packaging/eula.txt`.

Neither installer is signed, so Windows shows a SmartScreen warning and macOS
refuses the package until it is opened from the context menu. Signing them
needs certificates this repository does not carry:

```bash
# Windows, before distributing
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 \
    /f cert.pfx /p PASSWORD build\installer\Click-1.0.0-Windows.exe

# macOS: sign the bundles before packaging, then the package itself
codesign --force --timestamp --options runtime \
    --sign "Developer ID Application: ..." build/Click_artefacts/Release/VST3/Click.vst3
productsign --sign "Developer ID Installer: ..." \
    build/installer/Click-1.0.0-macOS.pkg Click-signed.pkg
xcrun notarytool submit Click-signed.pkg --keychain-profile PROFILE --wait
xcrun stapler staple Click-signed.pkg
```


## Building locally

Requires CMake 3.22+ and a C++17 compiler. JUCE 8.0.4 is fetched automatically.

```bash
git clone https://github.com/waywardroots/Click.git
cd Click
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Artefacts land in `build/Click_artefacts/Release/`.

Platform specifics:

- **Windows** — Visual Studio 2022 (Desktop C++ workload). The commands above
  work from any shell with CMake on `PATH`.
- **macOS** — Xcode command line tools. Builds universal by default; override
  with `-DCMAKE_OSX_ARCHITECTURES=arm64` for a single-architecture build.
- **Linux** — install the JUCE development packages first:

  ```bash
  sudo apt install libasound2-dev libfreetype-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
    libxinerama-dev libxrandr-dev libxrender-dev
  ```

To build against a JUCE checkout you already have (also useful offline):

```bash
cmake -B build -DCLICK_JUCE_PATH=/path/to/JUCE
cmake --build build
```

### Regenerating the stock clicks

The WAVs are committed, so this is only needed if you want to change them.
Requires `numpy`.

```bash
python3 tools/generate_clicks.py
```

## License keys

Click is unlocked by a key file, verified offline. The website signs a key with
the private half of Click's RSA pair; the plugin verifies it against the public
half embedded in `Source/PluginProcessor.cpp`. Nothing is contacted at runtime,
and a key works on any machine — the website is what enforces one key per sale.

Until a key is applied the editor is covered by the unlock screen, which takes
a pasted key and nothing else. Activation is stored in the plugin's settings
file, so it survives restarts.

| What changes per product | Click's value |
| --- | --- |
| `productId` / `appName` | `Click` |
| `activationToken` | `click-license-v1` |
| `publicKey` | **placeholder — not yet generated** |
| Signing key | this product's own private key, never reused |

### Generating the pair

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_KEYGEN=ON
cmake --build build --target Keygen
./build/Keygen_artefacts/Release/keygen makepair 1024
```

Put the `public:` half in `makeLicenseConfig()` in `Source/PluginProcessor.cpp`
and give the `private:` half to the website. Then sign a test key and check
that the verifier accepts it:

```bash
./build/Keygen_artefacts/Release/keygen sign \
    "Click" test@example.com "click-license-v1" "<PRIVATE>" "Test" > key.txt
./build/Keygen_artefacts/Release/keygen verify \
    "Click" "<PUBLIC>" "click-license-v1" key.txt
#   expect: applyKeyFile: ok / isUnlocked: YES
```

Finally paste `key.txt` into the plugin's unlock screen, and reopen the plugin
to confirm the activation persisted.

`tools/website/` holds the signer the website runs — the same algorithm in
TypeScript, plain Node and PHP — and `tools/website/README.md` is the spec it
must follow. The algorithm is deliberately not ordinary RSA signing: the XML
payload is signed whole, unhashed, read little-endian, and the key file is
comment lines then `#` then hex.

`python3 tools/website/verify-signer.py` checks the signer without needing a
build: it makes a throwaway RSA pair, signs a key with the Node signer, undoes
the transform the way the plugin does, and asserts the payload that falls out is
the expected flat `<key .../>` element.

The public key only verifies, so it is committed. **The private key signs: it
belongs on the server and must never reach this repository, the plugin binary
or a chat window.** This stops casual sharing; like any key scheme it does not
stop a determined cracker.

## Layout

```
CMakeLists.txt              build definition
logo.png                    icon drawn in the UI header (embedded via BinaryData)
End User License Agreement - FuzzyAudio - Click.pdf
                            the EULA, master copy
packaging/eula.txt          the same text, for the installers to display
packaging/windows/Click.iss the Inno Setup installer
packaging/windows/Click.ico installer and application icon, from logo.png
packaging/macos/distribution.xml
                            the macOS installer's panes and choices
packaging/macos/build-installer.sh
                            builds Click-<version>-macOS.pkg
resources/clicks/*.wav      the ten stock clicks (embedded via BinaryData)
tools/generate_clicks.py    regenerates the stock clicks
tools/make_eula_txt.py      regenerates packaging/eula.txt from the PDF
tools/keygen/keygen.cpp     makes the RSA pair, signs and verifies keys
tools/website/              the signer the website runs, its spec, and a self-test
modules/fuzzyaudio_licensing/
                            reusable unlock module: LicenseManager + UnlockGate
Source/
  Parameters.*              parameter IDs and the APVTS layout
  ClickLibrary.*            decoding of stock clicks and user files
  ClickSample.h             RenderedClick + the biquad used by the editor
  ClickSlot.*               one sound: source audio + offline edit rendering
  VoicePool.h               allocation-free polyphonic sample playback
  PluginProcessor.*         parameters, sequencer, state
  StepGrid.*                the pattern editor
  WaveformDisplay.*         waveform view with draggable trim handles
  EnvelopeDisplay.*         ADSR curve, drawn from the renderer's own maths
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

Click is distributed to end users under the **[Fuzzy Audio End User License
Agreement](End%20User%20License%20Agreement%20-%20FuzzyAudio%20-%20Click.pdf)**
(`End User License Agreement - FuzzyAudio - Click.pdf` in this repository). The
Application is licensed, not sold; the agreement is accepted by downloading,
installing or using it, and the Windows and macOS installers present it for
acceptance before any files are written.

The plugin source in this repository and the generated click samples are original
work, Copyright © 2026 Fuzzy Audio LLC. JUCE itself is fetched at build time and
carries [its own licence terms](https://juce.com/legal/juce-8-licence/) — review
them before distributing a binary. The VST3 portions are used under the MIT
licence from Steinberg Media Technologies GmbH, as recorded in section 5 of the
EULA.

### The installer copy of the EULA

Neither installer can display a PDF: Inno Setup's `LicenseFile` takes `.txt` or
`.rtf`, and the macOS `<license>` element takes `.txt`, `.rtf` or `.html`. So
`packaging/eula.txt` holds the same agreement as plain text — UTF-8 with a BOM
(Inno Setup only reads a `.txt` licence as UTF-8 when one is present) and CRLF
line endings, wrapped at 76 columns.

The PDF is the master copy. After changing it, regenerate the text and commit
both:

```bash
python3 tools/make_eula_txt.py      # needs pdftotext (poppler-utils)
```

The script reflows the extracted text and then checks its output word for word
against the PDF, so a silent drop or reordering fails the run rather than
reaching an installer.
