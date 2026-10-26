#pragma once
#include <JuceHeader.h>
#include "SpectralModel.h"
#include "MasterColor.h"
#include <atomic>

class ReSynthAudioProcessor final : public juce::AudioProcessor {
public:
    ReSynthAudioProcessor();
    ~ReSynthAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RE-SYNTH"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    struct PreviewAudio {
        std::vector<float> original, resynthesized;
        double sourceSampleRate=44100.0;
    };
    void installModel(std::shared_ptr<const rs::Model>);
    void installPreview(std::shared_ptr<const PreviewAudio> p);
    void startPreview(bool original);
    void stopPreview();
    std::shared_ptr<const rs::Model> currentModel() const { return std::atomic_load(&sound); }
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::StringArray effectNames();
    std::atomic<int> lastNote { -1 };
    // MIDI keyboard state serves both on-screen keys and incoming DAW MIDI.
    juce::MidiKeyboardState keyboardState;
    void setUiPitchWheel(int value) { uiPitchWheel.store(juce::jlimit(0,16383,value)); }
    void setUiModWheel(int value) { uiModWheel.store(juce::jlimit(0,127,value)); }
    std::atomic<int> incomingPitchWheel { 8192 };
    std::atomic<int> incomingModWheel { 0 };
private:
    std::atomic<int> uiPitchWheel {8192}, uiModWheel {0};
    int appliedUiPitch = 8192, appliedUiMod = 0;
    juce::Synthesiser synth;
    std::shared_ptr<const rs::Model> sound;
    std::shared_ptr<const PreviewAudio> previewSound;
    std::atomic<int> previewMode {0}; // 0 off; 1 original; 2 resynth
    std::atomic<unsigned> previewGeneration {0};
    unsigned playingGeneration=0;
    double previewPosition=0.0;
    juce::CriticalSection stateLock;
    double sr = 44100.0, fxPhase = 0.0, filterL = 0.0, filterR = 0.0, wahEnvelope = 0.0;
    std::vector<float> delayL, delayR;
    int delayIndex = 0;
    MasterColor masterColor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReSynthAudioProcessor)
};
