# RE-SYNTH v0.5.1 — native instrument + installer packaging

**Status: installer-builder SOURCE release; actual Windows/macOS installable executables are not included.** The GitHub Actions workflow is configured to compile the plugins on native runners and package a Windows `.exe` setup program plus a macOS `.pkg` installer. Neither native build nor installer has been executed or validated here. Both packages will be unsigned development builds.

## What the current synthesis engine implements

- **One WAV/AIFF note => playable 16-note-polyphonic MIDI instrument.** YIN-inspired root detection with manual MIDI-root override.
- STFT analysis with a Hann window and 87.5% overlap; sub-bin spectral peak picking and McAulay–Quatieri-style trajectory linking; maximum **128 partials and 128 temporal keyframes** in this prototype.
- Per-partial initial phase, evolving amplitudes/frequencies; residual synthesized from eight time-varying filtered-noise bands. **No sound wavetables**, no source waveform saved in a project or sound preset.
- **Original / Resynth A/B** buttons appear after importing and before accepting. Both previews are level matched and bypass effects/master color. Audition requires the DAW to be actively processing the plugin's audio (some hosts suspend processing when stopped). Preview source audio is released on Accept or when the editor closes.
- Automatic offline reconstruction comparison: **RMS level matching** and **1024-point Hann STFT log-spectral deviation in dB** (lower = closer). This is a diagnostic number, not a perceptual accuracy guarantee.
- **Timbre Lock** host-automatable parameter for pitch-dependent spectral envelope correction. Its envelope is analytically derived offline from partial trajectories. At the analyzed root note it does not alter tonal partial amplitude. The derived envelope is rebuilt from saved parameters on load; no extra waveform cache.
- Persistent self-contained `.rssound` mathematical sound models, full DAW project state, synth-targeted FX, seven master-color models, ADSR, MIDI pitch bend, 16-voice stealing and Nyquist partial suppression. Older `.rssound` schemas 1–4 remain readable.

## Getting actual Windows and macOS installer packages

**Important:** This archive does **not** itself contain working `.exe` or `.pkg` installers. Those require native plugin compilation on Windows/macOS. The GitHub Actions workflow builds the plugin and creates platform-specific installers **if the native builds succeed**.

1. Create a GitHub repository and upload the **contents of this `RE-SYNTH` folder**, including its hidden `.github` folder, to the repository root.
2. Open **Actions → Build RE-SYNTH installers (Windows EXE / macOS PKG) → Run workflow**.
3. After the workflow succeeds, download these artifacts:
   - `RE-SYNTH-Windows-Installer` contains **`RE-SYNTH-Setup-Windows.exe`**, an Inno Setup installer for Windows x64 that installs the VST3 to `C:\Program Files\Common Files\VST3\RE-SYNTH.vst3`.
   - `RE-SYNTH-macOS-Installer` contains **`RE-SYNTH-Installer-macOS.pkg`**, a macOS installer placing the VST3 in `/Library/Audio/Plug-Ins/VST3/` and the AU in `/Library/Audio/Plug-Ins/Components/`.
4. Run the correct installer on its target OS; restart your DAW and rescan plugins if needed.

**Developer-test security:** Both packages are **unsigned/not notarized**. Windows SmartScreen and macOS Gatekeeper may warn or block installation. Only test files built from source you trust; do not bypass warnings for untrusted downloads. Commercial distribution requires developer signing, macOS notarization, and DAW validation.

The workflow has **not been run in this environment**. Native compiler errors or host-compatibility issues may need further work. A successful native build does not guarantee that RE-SYNTH passes DAW validation.

### Create installers locally after compiling the plugin

- Windows: run `scripts/build-windows.ps1`, then `scripts/create-windows-installer.ps1` (requires Inno Setup 6). Output: `installers/RE-SYNTH-Setup-Windows.exe`.
- macOS: run `scripts/build-macos.sh`, then `scripts/create-macos-installer.sh` (requires Apple's `pkgbuild`). Output: `installers/RE-SYNTH-Installer-macOS.pkg`.

## Build locally from source

Requirements: CMake 3.22+, C++20, a C++ compiler, a checkout of JUCE 8.0.16-security, and native platform SDKs. JUCE has GPL/commercial licensing; review terms before distributing commercially.

**Windows:** Visual Studio 2022 C++ and PowerShell. Set `JUCE_DIR` to JUCE source folder, then run `./scripts/build-windows.ps1` from the project root.

**macOS:** Xcode and command-line tools. Set `JUCE_DIR` to JUCE source folder, then run `bash scripts/build-macos.sh`. Script builds universal arm64+x86_64 VST3 and AU, applies ad-hoc signatures for local testing, and makes a ZIP preserving macOS bundle metadata.

**Portable unit tests (no JUCE required):**

```bash
cmake -S tests -B build-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests --parallel 4
ctest --test-dir build-tests --output-on-failure
```

## Suggested DAW verification

- Load a fresh synth instance; play C4 and hold a chord; use MIDI sustain pedal, pitch bend, and note-off while recording audio.
- Import `Assets/TestNotes/C4_Evolving_Synth.wav`, check detected root, and compare **A: Original** vs **B: Resynth** while the DAW's audio engine is running. Spectral error is shown before Accept.
- Accept. Verify the UI no longer allows source audition. Save and reopen the DAW project, optionally on another computer with no sound library; verify the math-model instrument still works.
- Test 16-note polyphony, high/low octaves, 44.1/48/96 kHz host rates, buffer sizes 64–1024, AU/VST3 host validation and plugin scanning.
- Inspect CPU while sustaining a 16-voice chord and using several master colors. Current 128-partial engine can be computationally expensive; overload optimization and psychoacoustic quality are NOT complete.

## Current limitations

- **No native JUCE compile or DAW integration was possible in the authoring container.** CMake/CI are configured and unvalidated until native runners execute. If builds fail, capture the job log for compiler-specific fixes.
- The FFT analysis is synchronous on the UI thread and may momentarily freeze UI for long imports; this should be moved to a worker task after host testing.
- Quality is not proven transparent: one note cannot recover all velocity layers, physical string resonance, articulation, and recording-dependent stereo behavior. The source is mixed to mono, and phase tracking/transient residuals are first-generation approximations.
- Current host-preview audition depends on the DAW calling the plugin's audio process callback. A/B preview audio is kept *temporarily* until Accept or editor close; `.rssound` and project state contain no source PCM.
- Spectral-error measurement intentionally ignores quiet noise-floor bins and reports a level-matched log-magnitude difference, **not a validated perceptual score**.
- 16-voice worst-case tests should be profiled on the actual machine, particularly when running all 128 partial oscillators per note. The synth-focused FX and vintage-color implementations are first-pass hardware-inspired models rather than calibrated exact replicas.

See `docs/ENGINE_V04.md`, `docs/MASTER_COLOR_DESIGN.md`, and `docs/ENGINE_V05.md` for further technical detail.
