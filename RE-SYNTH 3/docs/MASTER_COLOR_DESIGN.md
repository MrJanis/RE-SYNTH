# RE-SYNTH — Master Color architecture (v0.2 prototype)

This stage runs **after per-sound effects** and before the master volume control.
It can be selected and automated from the DAW; all seven mode indices remain the
same as v0.1. A new automatable `colorDrive` parameter ranges 0–1 (default 0.5).

**Important:** These are engineering *starting points*, not claims of measured
or licensed emulation. Sonic calibration needs hardware captures, level-matched
null/difference tests, circuit/transfer measurements, and listening tests.

| Mode | Implemented in v0.2 | Validation or further work |
| --- | --- | --- |
| Clean | Bit-identical dry signal **after a latency-matched delay**, 4x oversampling skipped to save CPU | Verify delay alignment on physical DAW hosts |
| SSL | Stereo-linked feed-forward peak-detected VCA gain reduction, ~30ms attack, 2:1 ratio, dual-speed release, low/high shelves | Soft knee, exact knee/detector behaviour, original auto-release calibration, actual reference-matched curve |
| Neve | Oversampled (4x) asymmetric `tanh` transfer, gentle warm low shelf and top rolloff | Real 1073 transformer/hysteresis and Class-A harmonic transfer measurements |
| MPC60 | Fractional 40kHz sample-and-hold, symmetric signed 12-bit quantization, lowpass | Original nonlinear 12-bit representation, converter and analog filter measurements |
| MPC3000 | Oversampled mild saturation, low-frequency weight | Accurate converter/filter model; historical 44.1kHz / 16-bit linear behaviour |
| SP-1200 | Fractional 26.04kHz sample-and-hold, symmetric 12-bit quantizer, cascaded lowpass filters (4-pole overall), mild resonance | The historical SSM2044 is a ladder-type filter. Cascaded SVFs **do not** reproduce its nonlinear response; actual filtering varied with hardware output routing |
| ASR-10 | Oversampled asymmetric saturation, mild 2.8kHz shelf, 4-pole high-frequency rolloff | ASR-10 29.7619kHz / 44.1kHz modes, true converter and interpolation mapping, proper measured reconstruction response |

## Intentional aliasing vs. anti-aliasing

RE-SYNTH's partial oscillator bank rejects pitches above host Nyquist; this
protects the **source synthesis** from foldover. The MPC60/SP-1200 coloration
then applies unfiltered 40k/26.04kHz rate reduction **on purpose**, reproducing
some lower-fidelity alias/imaging character. The two goals are complementary.

Four-times oversampling occurs for analog-style saturation and SSL dynamics
(and transparently for non-Clean modes). The output is returned to the host
sample rate before the intentional sampler rate reduction. This keeps nonlinear
harmonics cleaner before the sample-rate artifacts are applied.

No allocation, lock, or audio-file I/O is performed in `MasterColor::process`.
Four-times oversampling is initialized in `prepareToPlay`, and the same
reported latency is preserved across modes by delaying the Clean signal.
Host block sizes must not exceed the maximum passed to `prepareToPlay`; this
assumption should be verified under target DAWs before a product release.

## Quantization

The common single-line example:

```cpp
std::floor(sample * 2048.0f) / 2048.0f
```

produces a persistent negative bias for almost all input amplitudes.
`rs::color::quantize12()` instead rounds to the nearest of the **4096 signed
levels** (-2048..2047), after input clipping. It is standalone in
`Source/ColorMath.h` and can be tested without JUCE.

## Hardware references (public)

- Rossum Electro-Music SP-1200 12-bit, 26.04 kHz, SSM2044 filter:
  https://www.rossum-electro.com/pages/education-sp-1200
- Akai MPC60 service manual: 40 kHz, nonlinear 12-bit sample format:
  https://www.polynominal.com/akai-mpc60/akai-mpc60-service-manual.pdf
- Akai MPC3000 manual: 16-bit linear, 44.1 kHz sampling:
  https://www.mpcstuff.com/content/MPC_Manuals/MPC3000_manual.pdf
- Ensoniq ASR-10 manual: 29.7619 kHz and 44.1 kHz modes:
  https://ensoniq.manymanuals.com/digital-pianos/asr-10/instruction-manual-11191/169
- AMS Neve 1073 product page: transformer Class-A circuit and EQ specifications:
  https://www.ams-neve.com/outboard/1073-range/1073-mic-preamp-equaliser/
- SSL G-Series Auto Release and stereo compressor circuit overview:
  https://www.solidstatelogic.com/assets/uploads/downloads/SSL_BiGSiX_User_Guide_V1.2_s.pdf
- JUCE TPT SVF (single instance 12dB/octave):
  https://docs.juce.com/develop/classjuce_1_1dsp_1_1StateVariableTPTFilter.html
- JUCE DSP 4x oversampling and host latency:
  https://docs.juce.com/master/classjuce_1_1dsp_1_1Oversampling.html

## Test commands

Portable math helper smoke tests (does *not* compile the JUCE plugin):

```bash
c++ -std=c++20 -Wall -Wextra -Werror tests/test_color_math.cpp -o test_color_math
./test_color_math
```

The math tests exercise quantizer symmetry/range, fractional hold rate at 48k
and 96k hosts, and saturation finite outputs. The source must still be compiled
with JUCE 8 and tested as VST3/AU on Windows/macOS.
