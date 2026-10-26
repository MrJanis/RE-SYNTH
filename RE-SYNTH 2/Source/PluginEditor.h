#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReSynthAudioProcessorEditor final : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer {
public:
    explicit ReSynthAudioProcessorEditor(ReSynthAudioProcessor&);
    ~ReSynthAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
private:
    ReSynthAudioProcessor& processor;
    juce::TextButton importButton{"IMPORT WAV / AIFF"}, acceptButton{"ACCEPT RESYNTHESIS"}, saveButton{"SAVE SOUND"}, loadButton{"LOAD SOUND"}, viewButton{"VIEW: PARTIALS"}, originalButton{"A: ORIGINAL"}, resynthButton{"B: RESYNTH"}, stopButton{"STOP"};
    juce::ComboBox effects, colors, category, rootSelector;
    juce::Label title, status, sampleInfo, rootInfo, qaInfo;
    juce::Slider volume, colorDrive, timbreLock, attack, decay, sustain, release, fx1, fx2, fx3;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fxAttachment, colorAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const rs::Model> pending;
    std::vector<float> waveform;
    bool hasPending=false;
    bool spectralView=false;
    int note=-1;
    void importFile(const juce::File&);
    void accept();
    void saveSound();
    void loadSound(const juce::File& file);
    void updateRootNote(int midi);
    void timerCallback() override;
    void setupSlider(juce::Slider&,const juce::String&,const juce::String&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReSynthAudioProcessorEditor)
};
