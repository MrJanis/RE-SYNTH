# v0.4: Single-sample sinusoidal/stochastic resynthesis

## Pipeline

Audio import → root YIN-like detection → pitch-adaptive FFT size (2,048–16,384 samples, Hann) at 87.5% overlap → sub-bin peak interpolation → frequency-linked tracks with birth/death handling → select strongest 128 trajectories → downsample time trajectories into a 128-keyframe maximum model → reconstruct sine reference → time-domain subtraction → residual noise analysis into 8 log-spaced band envelopes → drop both source waveform and subtraction buffers.

At playback a fixed-size array of independent oscillators tracks frequency and amplitude over time. Phases accumulate continuously across every sample, including silent or Nyquist-rejected partials, so subsequent births don't jump phase. Control data is interpolated every 32 samples, while phases are incremented every sample. A polynomial sine approximation uses **no waveform table**. Frequencies above 0.49×sample rate are muted, with smooth attenuation between 0.45 and 0.49×sample rate to reduce boundary discontinuities. Per-voice residual generation uses a deterministic fast PRNG and a bank of resonant bandpass filters whose gain follows noise-envelope keyframes.

## Modeling distinctions

- STFT phase estimates are stored at the birth of each track and projected to time zero. Preserving original waveform phase perfectly would require improved phase-locked trajectory interpolation.
- Partial ratios evolve independently; closely spaced inharmonic modes are not forced to integer harmonics.
- White noise is filtered using band envelopes from **actual subtraction residual**, as opposed to simply gating broadband noise with the original spectrum. This is an initial stochastic approximation, not a model of precise transient phase.
- Pitch mapping scales tracked instantaneous frequencies in each voice, rather than fixing a harmonic series. Frequency- and pitch-dependent spectral-envelope preservation is planned for later validation.
- The analyzed mono sound has no built-in sample playback and projects are self-contained after acceptance.

## Future validation milestones

1. Build JUCE 8 VST3 and AU on target platforms; run pluginval + DAW smoke tests (sample rates, buffer sizes, automation, state).
2. Add direct A/B audition and level-matched reference vs resynth renders at original pitch; measure spectral convergence, log-spectral distance, transient error, and residual SNR by sound category.
3. Audio regression corpus: flute, synth bass, bright pad, bell, piano, brass, string, electric piano and a noisy attack. Quantify performance at 16-note polyphony on supported hardware.
4. Improve trajectory tracking with phase-prediction-aware assignment, rejection of harmonic interference, multi-resolution transient modeling, and stereo residual estimation.
5. Separate pitch-preserving spectral envelope from oscillator frequency mapping; evaluate formant/inharmonic transposition and velocity-response modeling from a single note.
6. Move analysis to a safe background worker, leaving model installation on the UI/message thread and all source buffers outside the real-time DSP callback.
