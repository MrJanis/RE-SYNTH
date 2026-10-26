# Spectral-Wavetable Engine — v0.3 technical notes

## Input and pitch

- Only single-note WAV/AIFF input <=15 seconds is accepted. Stereo imports are currently mixed to mono.
- YIN-style difference/cumulative-mean-normalized-difference analysis over up to three partially offset 8,192-sample windows estimates F0; an agreement vote rejects unstable roots.
- Automatic note assignment rounds F0 to the nearest MIDI note. The user can manually reassign the root (this changes the note-to-pitch mapping but does not reanalyze the imported audio).
- The voice frequency for note N is `detected_Hz * 2^((N - assigned_root_MIDI)/12)` before pitch bend; this preserves the actual imported-note tuning. Full tuning normalization is a future option.

## Offline spectral model

1. Normalize input audio by its peak value (the original absolute recorded gain is discarded).
2. Choose up to 8 snapshots, denser at the beginning: nominal centers at 0.025, 0.065, 0.15, 0.32, 0.65, 1.3, 2.6 seconds and ~88% of total duration. Shorter notes have fewer frames.
3. At each frame, compute an 8,192-sample Hann-window FFT, locally interpolate logarithmic-magnitude peak-bin positions, and extract the top 256 peaks.
4. Merge peaks that represent the same frequency across snapshots, retaining the strongest candidate; choose up to 128 significant candidates.
5. For each retained candidate, record its frequency as a ratio to detected F0 and its amplitude at every snapshot by matching near-frequency spectral peaks.
6. Save time locations and per-peak magnitudes directly in the `.rssound` or DAW state. Do not store raw source audio or generated wavetable cache.

Known limitations: frequency estimates are approximate; snapshot magnitude envelopes are not phase coherent with the source. Partial association is nearest-frequency, not a full sinusoidal trajectory tracker. One FFT window length under-resolves low frequencies while smearing short transients. Better multiresolution tracking is a future goal.

## Hybrid wavetable playback

A partial with ratio within 2.5 cents of an integer harmonic 1..128 is assigned to the wavetable. All other partials keep a separate phase accumulator. The timbral model is frame-interpolated during each voice's lifetime.

At cache-build time, RE-SYNTH generates `frameCount x 14` tables, each with 1,024 samples. The harmonic ceiling set is `[1,2,3,4,6,8,12,16,24,32,48,64,96,128]`. Each band includes only partials at or below its ceiling.

At playback, the selected band must not exceed `floor(0.48 * host_sample_rate / played_fundamental_Hz)`. The engine linearly interpolates both cyclic table phase and frames along time; independent inharmonic partials use a 4,096-entry sine lookup and per-frame amplitude interpolation. All independent partials also receive their own Nyquist checks.

**CPU and memory:** generated tables use no per-sample allocation and are shared by the 16 MIDI voices. At 8 frames they occupy 448 KiB, with an additional 16 KiB shared sine table = 464 KiB per sound. A simpler/shorter model uses fewer temporal frames. The compact descriptor model is roughly a few kilobytes, excluding the host tree metadata.

## DAW state and migration

- State root `RESYNTH` contains a `PARAMETERS` child (APVTS) and `SOUND` child (model).
- Sound schema 3: `name`, `category`, `rootMidi`, `rootHz`, `frameCount`, `t0..t7`, one `PARTIAL` child per peak with `ratio` and `a0..a7`.
- On restore, a cache is rebuilt deterministically from partials and snapshots. This does not require a WAV/AIFF file or a local preset library.
- Legacy v0.1/v0.2 partials (`amp`, `decay`) migrate to a 6-frame synthetic envelope (a compatibility approximation). Confirm with real old DAW projects on target hosts.

## Important next engineering milestones

1. **Platform compile/DAW smoke QA** on macOS and Windows, all supported formats, automation, sustain pedal, program recall, voice stealing, audio reset, latency.
2. **Thread-safe import and model handover** with immutable prebuilt sound models published in a real-time-safe structure and reclaimed off the audio callback.
3. **Fidelity**: multiresolution partial/phase tracking, transient and colored residual-noise modeling, formant-preserving pitch transformation, source/resynth audition.
4. **Better octave bands**: crossfade between neighboring mip tables to avoid discontinuities.
5. **Performance**: vectorized/additive oscillator kernels, GPU-free time-frequency view, per-host profiling and fixed-budget voice policies.
