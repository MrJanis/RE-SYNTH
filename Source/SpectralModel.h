#pragma once
#include <JuceHeader.h>
#include "SpectralDSP.h"
#include "ReconstructionQA.h"
#include <memory>

namespace rs {
constexpr int maxPartials = dsp::maxPartials;
constexpr int maxVoices = 16;
struct Model {
    juce::String name {"Init Sine"};
    juce::String category {"Synth"};
    int rootMidi = 60;
    double rootHz = 261.625565;
    dsp::SpectralData spectrum;
    Model();
};
struct AnalysisResult {
    std::shared_ptr<const Model> model;
    juce::String error;
    std::vector<float> waveform;
    std::vector<float> sourceAudio; // analysis preview only; never persisted
    std::vector<float> resynthAudio; // computed once during analysis; never persisted
    qa::Report quality;
    double duration=0.;
    double sampleRate=0.;
};
// OFFLINE ONLY. Never perform analysis in an audio callback.
AnalysisResult analyzeFile(const juce::File& file);
juce::ValueTree modelToTree(const Model& model);
std::shared_ptr<const Model> modelFromTree(const juce::ValueTree& tree);
}
