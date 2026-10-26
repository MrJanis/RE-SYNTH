# RE-SYNTH v0.4 QA checklist

## Automated tests, portable C++

- [x] Dynamic sinusoidal level/frequency interpolation
- [x] Phase-continuous oscillator generation and reference sine accuracy
- [x] Inharmonic ratio preservation
- [x] Partial mute/soft bandlimiting above Nyquist
- [x] Evolving residual noise generator, finite output
- [x] Frame-to-frame peak linking, births/deaths, rejection of one-off spurs
- [x] YIN-style pitch detector regressions
- [x] Master color mathematical DSP regressions
- [x] Worst-case synthetic 16-voice additive render benchmark (informational, not a real-time guarantee)

## Plugin validation (not run; SDK/toolchains unavailable here)

- [ ] Build Windows VST3 using JUCE 8 + Visual Studio
- [ ] Build macOS VST3 + AU using JUCE 8 + Xcode
- [ ] Run pluginval for each binary
- [ ] Test MIDI polyphony, note stealing, bend, automation, sustain pedal in DAWs
- [ ] Confirm silence/no artifacts from partial birth/death & pedal note-off
- [ ] Test import, Accept, close/reopen project and `.rssound` v4 save/load
- [ ] Test reading v0.1–v0.3 `.rssound` and DAW state
- [ ] Verify no file handle or raw audio allocation persists after acceptance
- [ ] Capture CPU/RAM measurements at 44.1, 48, and 96 kHz, varying buffer sizes
- [ ] Replace UI-thread blocking analysis with background worker before release

## Sound-quality evaluation (not yet completed)

- [ ] Level-matched original vs generated sound at source pitch
- [ ] Compare attacks, spectral-envelope evolution, residual spectral texture
- [ ] Objective errors: log-spectral distance, transient error, reconstruction SNR
- [ ] Compare multiple pitches C1–C8 against independently recorded notes when available
- [ ] Listening evaluations for piano, bell, bass, flute, pad, strings, brass, electric piano
- [ ] Verify residual does not reintroduce tonal leakage or phase cancellation artifacts
- [ ] Verify stereo extension and expression mapping (future milestone)
