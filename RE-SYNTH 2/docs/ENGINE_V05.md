# v0.5 architecture and QA

## Playback representation

No stored sound-specific wavetable frames. Each partial has a dynamic frequency ratio, amplitude trajectory and initial phase. The carrier remains one independently phased oscillator per partial. A control step of 32 samples linearly interpolates amplitude/frequency. Noise residual remains eight independent filters fed by seeded pseudorandom excitation.

## Timbre Lock

Offline analysis builds a 48-bin octave-spaced spectral amplitude envelope per analysis frame. Each bin estimates broad spectral energy from detected partial trajectories using a Gaussian kernel in log2 frequency. On transposition, the oscillator partial amplitude can be adjusted by the ratio of envelope energy at target frequency to its source frequency. Ratios are bounded to avoid instability. `timbreLock=0` disables this correction; `1` applies the full bounded estimate. This affects perceived formant stability but is **an approximation**, not a physical instrument model. Since the envelopes are derived from partials when loading a sound, no extra cache needs to be serialized.

## Offline quality diagnostic

On import, compute a full-length resynthesized example at the detected root and original input sample rate. Subtract the additive component to model noise, rerender including that residual, then measure two features: RMS-normalization and RMS log-spectrum deviation in dB. These diagnostics cannot establish perceptual transparency because phase, transient shape, stochastic residual texture and psychoacoustic masking differ. Both signals are retained temporarily to support original/resynth audition. After Accept/closing the editor, source PCM and the rendered preview are discarded.

## Native build path

GitHub Actions workflow has independent Windows and macOS builders, pinned to JUCE 8.0.16-security. It packages developer-test VST3 bundles plus AU component on macOS. Artifacts are unsigned on Windows and ad-hoc signed on macOS. Final notarization, installer PKG/MSI, pluginval/auval and DAW smoke tests are still outstanding.
