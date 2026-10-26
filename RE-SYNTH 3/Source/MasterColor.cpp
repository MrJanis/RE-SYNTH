#include "MasterColor.h"
#include <algorithm>
#include <cmath>

namespace {
float dbToLinear(float dB) noexcept { return std::pow(10.0f, dB / 20.0f); }
float safeAudio(float x) noexcept { return rs::color::finiteOrZero(x); }
}

void MasterColor::prepareFilter(juce::dsp::IIR::Filter<float>& f, double rate, int maxBlock) {
    f.prepare({rate, static_cast<juce::uint32>(maxBlock), 1});
    f.reset();
}

void MasterColor::prepare(double sampleRate, int maxBlock) {
    sr = std::max(8000.0, sampleRate);
    maxBlock = std::max(1, maxBlock);

    // The exact same oversampling pipeline runs in all modes to keep plugin
    // latency constant when the host automates the color choice.
    // Factor = 2^2 = 4x. Integer-latency compensation is enabled.
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing(static_cast<size_t>(maxBlock));
    latency = static_cast<int>(std::lround(oversampler->getLatencyInSamples()));
    cleanDelayL.assign(static_cast<size_t>(latency), 0.f);
    cleanDelayR.assign(static_cast<size_t>(latency), 0.f);
    cleanDelayPosition = 0;

    const juce::dsp::ProcessSpec spec {sr, static_cast<juce::uint32>(maxBlock), 2};
    for (auto* f : {&spPoleA, &spPoleB, &mpcOutput, &asrAntiImageA, &asrAntiImageB}) {
        f->prepare(spec);
        f->setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        f->setResonance(0.7071f);
    }
    spPoleA.setCutoffFrequency(static_cast<float>(std::min(9500.0, sr * 0.44)));
    spPoleB.setCutoffFrequency(static_cast<float>(std::min(9500.0, sr * 0.44)));
    mpcOutput.setCutoffFrequency(static_cast<float>(std::min(16000.0, sr * 0.44)));
    asrAntiImageA.setCutoffFrequency(static_cast<float>(std::min(17500.0, sr * 0.44)));
    asrAntiImageB.setCutoffFrequency(static_cast<float>(std::min(17500.0, sr * 0.44)));

    for (auto* f : {&neveLowL, &neveLowR, &neveTopL, &neveTopR,
                    &mpc3LowL, &mpc3LowR, &asrShelfL, &asrShelfR,
                    &sslBassL, &sslBassR, &sslAirL, &sslAirR}) prepareFilter(*f, sr, maxBlock);

    const auto assign = [](auto& l, auto& r, auto coeff) {
        l.coefficients = coeff;
        r.coefficients = coeff;
    };
    assign(neveLowL, neveLowR, juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 220.0, 0.707f, dbToLinear(1.4f)));
    assign(neveTopL, neveTopR, juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 9500.0, 0.707f, dbToLinear(-1.8f)));
    assign(mpc3LowL, mpc3LowR, juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 110.0, 0.707f, dbToLinear(0.9f)));
    assign(asrShelfL, asrShelfR, juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 2800.0, 0.707f, dbToLinear(1.7f)));
    assign(sslBassL, sslBassR, juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 100.0, 0.707f, dbToLinear(0.8f)));
    assign(sslAirL, sslAirR, juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 8500.0, 0.707f, dbToLinear(1.1f)));
    reset();
}

void MasterColor::reset() noexcept {
    if (oversampler) oversampler->reset();
    spHold.reset(); mpcHold.reset();
    std::fill(cleanDelayL.begin(), cleanDelayL.end(), 0.0f);
    std::fill(cleanDelayR.begin(), cleanDelayR.end(), 0.0f);
    cleanDelayPosition = 0;
    for (auto* f : {&spPoleA, &spPoleB, &mpcOutput, &asrAntiImageA, &asrAntiImageB}) f->reset();
    for (auto* f : {&neveLowL, &neveLowR, &neveTopL, &neveTopR,
                    &mpc3LowL, &mpc3LowR, &asrShelfL, &asrShelfR,
                    &sslBassL, &sslBassR, &sslAirL, &sslAirR}) f->reset();
    compDetector = slowGainHistory = 0.0;
}

void MasterColor::processStereoIIR(juce::dsp::IIR::Filter<float>& fL,
                                   juce::dsp::IIR::Filter<float>& fR,
                                   float& l, float& r) noexcept {
    l = fL.processSample(l);
    r = fR.processSample(r);
}

void MasterColor::applyHighRateColor(juce::dsp::AudioBlock<float>& b,
                                     Mode mode, float drive) noexcept {
    auto* l = b.getChannelPointer(0);
    auto* r = b.getChannelPointer(1);
    const auto rate = sr * 4.0;
    const float intensity = juce::jlimit(0.0f, 1.0f, drive);

    for (size_t i = 0; i < b.getNumSamples(); ++i) {
        float left = safeAudio(l[i]), right = safeAudio(r[i]);
        switch (mode) {
            case Mode::neve: {
                const float gain = 1.25f + 3.25f * intensity;
                left = rs::color::asymmetricSaturation(left, gain);
                right = rs::color::asymmetricSaturation(right, gain);
                break;
            }
            case Mode::mpc3000: {
                const float gain = 1.0f + 0.8f * intensity;
                left = rs::color::symmetricSaturation(left, gain);
                right = rs::color::symmetricSaturation(right, gain);
                break;
            }
            case Mode::asr10: {
                const float gain = 1.0f + 0.95f * intensity;
                left = rs::color::asymmetricSaturation(left, gain);
                right = rs::color::asymmetricSaturation(right, gain);
                break;
            }
            case Mode::ssl: {
                // Stereo-linked feed-forward peak detection, soft knee omitted
                // at this stage. Dual-speed program-dependent release mimics
                // the broad behaviour of the original Auto Release circuit.
                const double peak = std::max(std::abs(left), std::abs(right));
                const double attack = std::exp(-1.0 / (0.03 * rate));
                const double fastRelease = std::exp(-1.0 / (0.12 * rate));
                const double slowRelease = std::exp(-1.0 / (1.2 * rate));
                const double releaseMix = juce::jlimit(0.0, 1.0, slowGainHistory / 9.0);
                const double release = fastRelease * (1.0 - releaseMix) + slowRelease * releaseMix;
                compDetector = (peak > compDetector ? attack : release) * compDetector +
                               (1.0 - (peak > compDetector ? attack : release)) * peak;
                const double levelDb = 20.0 * std::log10(std::max(1.0e-9, compDetector));
                const double threshold = -21.0 + (1.0 - intensity) * 12.0;
                const double reduction = std::max(0.0, levelDb - threshold) * 0.5; // 2:1
                const double slowTrack = std::exp(-1.0 / (0.65 * rate));
                slowGainHistory = slowTrack * slowGainHistory + (1.0 - slowTrack) * reduction;
                const float gain = dbToLinear(static_cast<float>(-reduction + 0.7));
                left *= gain;
                right *= gain;
                break;
            }
            default: break;
        }
        l[i] = safeAudio(left);
        r[i] = safeAudio(right);
    }
}

void MasterColor::processLowRate(juce::AudioBuffer<float>& b, Mode mode, float drive) noexcept {
    const float intensity = juce::jlimit(0.0f, 1.0f, drive);
    // Cutoff and resonance vary gently with the color drive knob.
    const float spCutoff = static_cast<float>(std::min(sr * 0.44, 10500.0 - intensity * 4300.0));
    spPoleA.setCutoffFrequency(spCutoff);
    spPoleB.setCutoffFrequency(spCutoff);
    spPoleA.setResonance(0.75f + intensity * 0.15f);
    spPoleB.setResonance(0.75f);
    auto* l = b.getWritePointer(0);
    auto* r = b.getWritePointer(1);
    for (int n=0; n<b.getNumSamples(); ++n) {
        float left = safeAudio(l[n]), right = safeAudio(r[n]);
        switch (mode) {
            case Mode::sp1200: {
                // Actual historic SSM2044 is a ladder-type IC, and on the SP
                // specifically used on certain outputs. Cascaded TPT SVFs are
                // an approximation of a resonant 4-pole response, not a clone.
                spHold.tick(left, right, sr, 26040.0);
                left = rs::color::quantize12(left * (1.0f + intensity * 1.5f));
                right = rs::color::quantize12(right * (1.0f + intensity * 1.5f));
                left = spPoleB.processSample(0, spPoleA.processSample(0, left));
                right = spPoleB.processSample(1, spPoleA.processSample(1, right));
                break;
            }
            case Mode::mpc60: {
                mpcHold.tick(left, right, sr, 40000.0);
                left = rs::color::quantize12(left * (1.0f + intensity * 0.65f));
                right = rs::color::quantize12(right * (1.0f + intensity * 0.65f));
                left = mpcOutput.processSample(0, left);
                right = mpcOutput.processSample(1, right);
                break;
            }
            case Mode::neve:
                processStereoIIR(neveLowL, neveLowR, left, right);
                processStereoIIR(neveTopL, neveTopR, left, right);
                break;
            case Mode::mpc3000:
                processStereoIIR(mpc3LowL, mpc3LowR, left, right);
                break;
            case Mode::asr10:
                processStereoIIR(asrShelfL, asrShelfR, left, right);
                left = asrAntiImageB.processSample(0, asrAntiImageA.processSample(0, left));
                right = asrAntiImageB.processSample(1, asrAntiImageA.processSample(1, right));
                break;
            case Mode::ssl:
                processStereoIIR(sslBassL, sslBassR, left, right);
                processStereoIIR(sslAirL, sslAirR, left, right);
                break;
            default: break;
        }
        l[n] = safeAudio(left);
        r[n] = safeAudio(right);
    }
}

void MasterColor::process(juce::AudioBuffer<float>& buffer, Mode mode, float drive) noexcept {
    if (!oversampler || buffer.getNumChannels() != 2 || buffer.getNumSamples() <= 0) return;
    // Update the bypass delay history even while a colored mode plays; changing
    // modes can then switch to Clean without reading stale audio history.
    auto* sourceL = buffer.getWritePointer(0);
    auto* sourceR = buffer.getWritePointer(1);
    for (int i=0; i<buffer.getNumSamples(); ++i) {
        const float inL = sourceL[i], inR = sourceR[i];
        if (latency > 0) {
            const float dryL = cleanDelayL[cleanDelayPosition];
            const float dryR = cleanDelayR[cleanDelayPosition];
            cleanDelayL[cleanDelayPosition] = inL;
            cleanDelayR[cleanDelayPosition] = inR;
            if (++cleanDelayPosition >= cleanDelayL.size()) cleanDelayPosition = 0;
            if (mode == Mode::clean) {sourceL[i] = dryL; sourceR[i] = dryR;}
        }
    }
    // Bit-transparent at the same reported latency as the other modes.
    // Skip the expensive 4x processing entirely when Clean is selected.
    if (mode == Mode::clean) return;

    // Caller guarantees buffer length <= maxBlock passed to prepare().
    juce::dsp::AudioBlock<float> writable(buffer);
    const juce::dsp::AudioBlock<const float> input(writable);
    auto highRate = oversampler->processSamplesUp(input);
    applyHighRateColor(highRate, mode, drive);
    oversampler->processSamplesDown(writable);
    processLowRate(buffer, mode, drive);
}
