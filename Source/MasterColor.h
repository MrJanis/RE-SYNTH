#pragma once
#include <JuceHeader.h>
#include "ColorMath.h"
#include <array>
#include <memory>
#include <vector>

// RE-SYNTH's real-time master output coloration bank.
// The algorithms are hardware-*inspired*: component/circuit calibration against
// physical reference units remains a separate validation task.
// No allocation, locking, or parameter-tree access occurs in process().
class MasterColor final {
public:
    enum class Mode : int { clean, ssl, neve, mpc60, mpc3000, sp1200, asr10 };

    void prepare(double hostSampleRate, int maximumBlockSize);
    void reset() noexcept;
    int latencySamples() const noexcept { return latency; }
    void process(juce::AudioBuffer<float>& buffer, Mode mode, float drive) noexcept;

private:
    void applyHighRateColor(juce::dsp::AudioBlock<float>& block, Mode mode, float drive) noexcept;
    void processLowRate(juce::AudioBuffer<float>& buffer, Mode mode, float drive) noexcept;
    void prepareFilter(juce::dsp::IIR::Filter<float>& filter, double rate, int maxBlock);
    void processStereoIIR(juce::dsp::IIR::Filter<float>& leftFilter,
                          juce::dsp::IIR::Filter<float>& rightFilter,
                          float& l, float& r) noexcept;

    double sr = 44100.0;
    int latency = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    rs::color::FractionalHold spHold, mpcHold;
    // Transparent, latency-matched bypass path; allocated only at prepare().
    std::vector<float> cleanDelayL, cleanDelayR;
    size_t cleanDelayPosition = 0;
    juce::dsp::StateVariableTPTFilter<float> spPoleA, spPoleB, mpcOutput,
                                            asrAntiImageA, asrAntiImageB;
    juce::dsp::IIR::Filter<float> neveLowL, neveLowR, neveTopL, neveTopR,
                                   mpc3LowL, mpc3LowR, asrShelfL, asrShelfR,
                                   sslBassL, sslBassR, sslAirL, sslAirR;
    double compDetector = 0.0, slowGainHistory = 0.0;
};
