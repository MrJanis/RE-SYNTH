#include "PluginEditor.h"

namespace {
const auto bg=juce::Colour(0xff10171e), panel=juce::Colour(0xff1b2731), cyan=juce::Colour(0xff5ce0d5), orange=juce::Colour(0xffffa33b);
}
ReSynthAudioProcessorEditor::ReSynthAudioProcessorEditor(ReSynthAudioProcessor& p) : AudioProcessorEditor(&p), processor(p), keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
    setSize(800,600);
    title.setText("◈  RE-SYNTH",juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(29.f).withStyle("Bold")));
    title.setColour(juce::Label::textColourId,orange);
    addAndMakeVisible(title);
    status.setText("Ready — import one pitched note",juce::dontSendNotification);
    status.setColour(juce::Label::textColourId,cyan);addAndMakeVisible(status);
    sampleInfo.setText("No sample loaded",juce::dontSendNotification);addAndMakeVisible(sampleInfo);
    rootInfo.setText("Root: C4 (default sine)",juce::dontSendNotification);addAndMakeVisible(rootInfo);
    addAndMakeVisible(importButton); addAndMakeVisible(acceptButton);addAndMakeVisible(saveButton);
    addAndMakeVisible(viewButton);
    addAndMakeVisible(originalButton);addAndMakeVisible(resynthButton);addAndMakeVisible(stopButton);
    originalButton.setEnabled(false);resynthButton.setEnabled(false);stopButton.setEnabled(false);
    qaInfo.setText("A/B preview available after importing a note",juce::dontSendNotification);
    qaInfo.setColour(juce::Label::textColourId,cyan);
    qaInfo.setFont(juce::Font(juce::FontOptions(11.f)));
    addAndMakeVisible(qaInfo);
    originalButton.onClick=[this]{processor.startPreview(true);};
    resynthButton.onClick=[this]{processor.startPreview(false);};
    stopButton.onClick=[this]{processor.stopPreview();};
    addAndMakeVisible(loadButton);
    loadButton.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Load a RE-SYNTH sound",juce::File{},"*.rssound");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
            [safe=juce::Component::SafePointer<ReSynthAudioProcessorEditor>(this)](const juce::FileChooser& fc){
                if(safe && fc.getResult().existsAsFile())safe->loadSound(fc.getResult());
            });
    };
    viewButton.onClick=[this]{spectralView=!spectralView;viewButton.setButtonText(spectralView ? "VIEW: EVOLUTION" : "VIEW: PARTIALS");repaint();};
    for (int midi=0;midi<128;++midi)
        rootSelector.addItem(juce::MidiMessage::getMidiNoteName(midi,true,true,3),midi+1);
    if(auto current=processor.currentModel())rootSelector.setSelectedId(current->rootMidi+1,juce::dontSendNotification);
    rootSelector.onChange=[this]{updateRootNote(rootSelector.getSelectedId()-1);};
    rootSelector.setTooltip("Manual root note override. The analysis itself is not recalculated.");
    addAndMakeVisible(rootSelector);
    acceptButton.setEnabled(false);
    importButton.onClick=[this] {
        chooser=std::make_unique<juce::FileChooser>("Choose a pitched note",juce::File{},"*");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
          [safe=juce::Component::SafePointer<ReSynthAudioProcessorEditor>(this)](const juce::FileChooser& fc) {
            if(safe && fc.getResult().existsAsFile()) safe->importFile(fc.getResult());
          });
    };
    acceptButton.onClick=[this]{accept();};saveButton.onClick=[this]{saveSound();};
    for(auto name:ReSynthAudioProcessor::effectNames()) effects.addItem(name,effects.getNumItems()+1);
    for(auto name:juce::StringArray{"Clean","SSL","Neve","MPC60","MPC3000","SP-1200","ASR-10"}) colors.addItem(name,colors.getNumItems()+1);
    for(auto name:juce::StringArray{"Keys","Brass","Woodwinds","Strings","Bass","Drum","Percussion","Synth","Vocal","FX"}) category.addItem(name,category.getNumItems()+1);
    category.setSelectedId(8);addAndMakeVisible(category);
    addAndMakeVisible(effects);addAndMakeVisible(colors);
    fxAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"fxType",effects);
    colorAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters,"color",colors);
    setupSlider(volume,"volume","VOL");setupSlider(colorDrive,"colorDrive","DRIVE");setupSlider(attack,"attack","ATK");setupSlider(decay,"decay","DEC");
    setupSlider(sustain,"sustain","SUS");setupSlider(release,"release","REL");
    setupSlider(timbreLock,"timbreLock","TIMBRE");
    setupSlider(fx1,"fx1","CTRL 1");setupSlider(fx2,"fx2","CTRL 2");setupSlider(fx3,"fx3","MIX / CTRL 3");
    midiInfo.setText("MIDI: ready",juce::dontSendNotification);
    midiInfo.setColour(juce::Label::textColourId,cyan); addAndMakeVisible(midiInfo);
    keyboard.setAvailableRange(60,72);
    keyboard.setLowestVisibleKey(60.f);
    keyboard.setScrollButtonsVisible(false);
    keyboard.setKeyWidth(65.f);
    keyboard.setMidiChannel(1);
    addAndMakeVisible(keyboard);
    pitchWheel.setSliderStyle(juce::Slider::LinearVertical);
    pitchWheel.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    pitchWheel.setRange(-1.0,1.0,0.001);
    pitchWheel.setValue(0.0,juce::dontSendNotification);
    pitchWheel.setTooltip("Pitch bend +/-2 semitones, spring return");
    pitchWheel.onValueChange=[this]{
        const double v=pitchWheel.getValue();
        processor.setUiPitchWheel(8192+static_cast<int>(std::lround(v*(v<0?8192.0:8191.0))));
    };
    pitchWheel.onDragEnd=[this]{pitchWheel.setValue(0.0,juce::sendNotificationSync);};
    addAndMakeVisible(pitchWheel);
    modWheel.setSliderStyle(juce::Slider::LinearVertical);
    modWheel.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    modWheel.setRange(0.0,1.0,0.001);
    modWheel.setValue(0.0,juce::dontSendNotification);
    modWheel.setTooltip("MIDI CC1 modulation: vibrato depth");
    modWheel.onValueChange=[this]{processor.setUiModWheel(static_cast<int>(std::lround(modWheel.getValue()*127.0)));};
    addAndMakeVisible(modWheel);
    for(auto* b : {&envelopeTab,&effectsTab,&masterTab})addAndMakeVisible(*b);
    addAndMakeVisible(octaveDown);
    addAndMakeVisible(octaveUp);
    addAndMakeVisible(octaveValue);
    octaveValue.setJustificationType(juce::Justification::centred);
    octaveValue.setColour(juce::Label::textColourId,orange);
    octaveValue.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
    octaveDown.setTooltip("Shift on-screen keyboard down one octave (minimum -3)");
    octaveUp.setTooltip("Shift on-screen keyboard up one octave (maximum +3)");
    octaveDown.onClick=[this]{setKeyboardOctave(octaveOffset-1);};
    octaveUp.onClick=[this]{setKeyboardOctave(octaveOffset+1);};
    setKeyboardOctave(0);
    envelopeTab.onClick=[this]{showControlsPage(0);};
    effectsTab.onClick=[this]{showControlsPage(1);};
    masterTab.onClick=[this]{showControlsPage(2);};
    showControlsPage(0);
    resized();
    startTimerHz(12);
}
ReSynthAudioProcessorEditor::~ReSynthAudioProcessorEditor() {
    processor.keyboardState.allNotesOff(1);
    processor.setUiPitchWheel(8192);
    processor.installPreview({}); // discard source audio if editor closes without Accept
}
void ReSynthAudioProcessorEditor::setupSlider(juce::Slider& s,const juce::String& id,const juce::String& label) {
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,19);
    s.setColour(juce::Slider::rotarySliderFillColourId,cyan);
    s.setName(label);s.setTooltip(label+" — double click to reset");
    addAndMakeVisible(s);
    auto* attachment=sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(processor.parameters,id,s));
    juce::ignoreUnused(attachment);
    s.setDoubleClickReturnValue(true,s.getValue());
}
void ReSynthAudioProcessorEditor::setKeyboardOctave(int offset) {
    octaveOffset=juce::jlimit(-3,3,offset);
    const int low=60+12*octaveOffset;
    processor.keyboardState.allNotesOff(1);
    keyboard.setAvailableRange(low,low+12);
    keyboard.setLowestVisibleKey(static_cast<float>(low));
    octaveValue.setText((octaveOffset>0 ? "+" : "") + juce::String(octaveOffset),juce::dontSendNotification);
    octaveDown.setEnabled(octaveOffset>-3);
    octaveUp.setEnabled(octaveOffset<3);
    repaint();
}
void ReSynthAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(bg);
    g.setColour(panel);
    for(const auto r : {juce::Rectangle<float>(16,60,768,143),juce::Rectangle<float>(16,209,768,146),juce::Rectangle<float>(16,361,768,111),juce::Rectangle<float>(16,479,768,110)})
        g.fillRoundedRectangle(r,9.f);
    g.setColour(orange);
    g.setFont(juce::Font(juce::FontOptions(12.f).withStyle("Bold")));
    g.drawText("1  IMPORT A NOTE",28,67,200,19,juce::Justification::left);
    g.drawText("2  A/B COMPARE",28,155,220,19,juce::Justification::left);
    g.drawText("SPECTRAL ANALYSIS",28,214,250,20,juce::Justification::left);
    g.drawText("PLAY THE INSTRUMENT",211,483,240,22,juce::Justification::left);
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(11.f);
    g.drawText("CATEGORY",521,72,110,17,juce::Justification::left);
    g.drawText("ROOT NOTE",658,72,110,17,juce::Justification::left);
    g.drawText("PITCH",29,565,76,18,juce::Justification::centred);
    g.drawText("MOD",104,565,76,18,juce::Justification::centred);
    g.drawText("C4 - C5  |  One-octave MIDI keyboard",219,567,542,18,juce::Justification::centred);
    g.setColour(cyan.withAlpha(0.75f));
    auto bounds=juce::Rectangle<float>(30,247,740,95);
    if (!waveform.empty()) {
        float step=bounds.getWidth()/static_cast<float>(waveform.size());
        for(size_t i=0;i<waveform.size();++i) {
            auto h=waveform[i]*bounds.getHeight()*0.47f;
            float x=bounds.getX()+static_cast<float>(i)*step;
            g.drawVerticalLine(static_cast<int>(x),bounds.getCentreY()-h,bounds.getCentreY()+h);
        }
    } else if(auto m=processor.currentModel()) {
        const auto& spec=m->spectrum;
        if(spectralView) {
            // Offline time-frequency spectrogram of the accepted mathematical model.
            // Columns interpolate analysis frames; rows are logarithmic frequency bins.
            constexpr int rows=48, columns=120;
            const float cellW=bounds.getWidth()/columns;
            const float cellH=bounds.getHeight()/rows;
            const float maximumTime=spec.times[std::max(0,spec.frameCount-1)];
            for(int col=0;col<columns;++col) {
                const float t=maximumTime*(col+0.5f)/columns;
                auto blend=rs::dsp::frameBlend(spec,t);
                std::array<float,rows> energies {};
                for(int p=0;p<spec.count;++p) {
                    const float hz=static_cast<float>(m->rootHz)*spec.partials[p].ratio;
                    if(hz<30 || hz>20000)continue;
                    const int row=juce::jlimit(0,rows-1,static_cast<int>(std::log2(hz/30.f)/std::log2(20000.f/30.f)*rows));
                    energies[static_cast<size_t>(row)]+=rs::dsp::lerpLevel(spec.partials[p],blend);
                }
                for(int row=0;row<rows;++row) {
                    const float intensity=juce::jlimit(0.f,1.f,std::sqrt(energies[static_cast<size_t>(row)]));
                    const auto colour=juce::Colour::fromHSV(0.75f-0.63f*intensity,0.90f,0.13f+0.87f*intensity,1.f);
                    g.setColour(colour);
                    g.fillRect(bounds.getX()+col*cellW,bounds.getBottom()-(row+1)*cellH,cellW+0.3f,cellH+0.3f);
                }
            }
        } else {
            for(int i=0;i<spec.count;++i) {
                const float ratio=spec.partials[i].ratio;
                const float x=bounds.getX()+std::log2(std::max(1.0f,ratio)+1.f)/7.f*bounds.getWidth();
                const float h=spec.partials[i].levels[0]*bounds.getHeight()*0.8f;
                g.fillRect(x,bounds.getBottom()-h,2.5f,h);
            }
        }
    }

    g.setFont(11.f);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    if (controlsPage == 0) {
        const char* names[]={"ATTACK","DECAY","SUSTAIN","RELEASE"};
        for(int i=0;i<4;++i)g.drawText(names[i],35+i*113,398,100,15,juce::Justification::centred);
        g.drawText("Envelope (ADSR)",500,407,250,20,juce::Justification::centred);
    } else if(controlsPage==1) {
        g.drawText("EFFECT TYPE",31,398,190,16,juce::Justification::centred);
        const char* names[]={"CONTROL 1","CONTROL 2","MIX / CTRL 3"};
        for(int i=0;i<3;++i)g.drawText(names[i],257+i*120,398,112,15,juce::Justification::centred);
    } else {
        g.drawText("MASTER COLOR",31,398,190,16,juce::Justification::centred);
        const char* names[]={"VOLUME","COLOR DRIVE","TIMBRE LOCK"};
        for(int i=0;i<3;++i)g.drawText(names[i],250+i*135,398,114,15,juce::Justification::centred);
    }
}
void ReSynthAudioProcessorEditor::resized() {
    title.setBounds(18,5,265,50);
    status.setBounds(292,12,482,34);
    importButton.setBounds(29,93,188,26);
    loadButton.setBounds(226,93,121,26);
    acceptButton.setBounds(354,93,149,26);
    category.setBounds(518,93,120,26);
    rootSelector.setBounds(650,93,119,26);
    sampleInfo.setBounds(30,125,411,24);
    rootInfo.setBounds(450,125,316,24);
    originalButton.setBounds(29,177,123,22);
    resynthButton.setBounds(159,177,131,22);
    stopButton.setBounds(296,177,73,22);
    qaInfo.setBounds(379,176,392,23);
    viewButton.setBounds(618,213,148,26);
    envelopeTab.setBounds(29,365,122,27);
    effectsTab.setBounds(159,365,122,27);
    masterTab.setBounds(289,365,122,27);
    const std::array<juce::Slider*,4> env{&attack,&decay,&sustain,&release};
    for(int i=0;i<4;++i)env[static_cast<size_t>(i)]->setBounds(39+i*113,413,93,52);
    effects.setBounds(30,417,188,28);
    const std::array<juce::Slider*,3> fxs{&fx1,&fx2,&fx3};
    for(int i=0;i<3;++i)fxs[static_cast<size_t>(i)]->setBounds(258+i*120,413,100,52);
    colors.setBounds(30,417,188,28);
    const std::array<juce::Slider*,3> master{&volume,&colorDrive,&timbreLock};
    for(int i=0;i<3;++i)master[static_cast<size_t>(i)]->setBounds(255+i*135,413,105,52);
    octaveDown.setBounds(28,488,50,23);
    octaveValue.setBounds(79,488,48,23);
    octaveUp.setBounds(128,488,50,23);
    pitchWheel.setBounds(40,515,53,47);
    modWheel.setBounds(112,515,53,47);
    midiInfo.setBounds(582,483,180,19);
    keyboard.setBounds(209,512,553,53);
}
bool ReSynthAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {return files.size()>0 && juce::File(files[0]).hasFileExtension("wav;aif;aiff");}
void ReSynthAudioProcessorEditor::filesDropped(const juce::StringArray& files,int,int) {if(!files.isEmpty())importFile(juce::File(files[0]));}
void ReSynthAudioProcessorEditor::importFile(const juce::File& file) {
    processor.stopPreview();
    status.setText("Analyzing " + file.getFileName() + "...",juce::dontSendNotification);
    auto result=rs::analyzeFile(file);
    if (!result.model) {status.setText("Analysis failed: "+result.error,juce::dontSendNotification);return;}
    pending=result.model;waveform=std::move(result.waveform);hasPending=true;acceptButton.setEnabled(true);
    const double quality=result.quality.spectralErrorDb;
    qaInfo.setText("Log-spectrum deviation: "+juce::String(quality,1)+" dB (lower is better)",juce::dontSendNotification);
    auto preview=std::make_shared<ReSynthAudioProcessor::PreviewAudio>();
    preview->sourceSampleRate=result.sampleRate;
    preview->original=std::move(result.sourceAudio);
    preview->resynthesized=std::move(result.resynthAudio);
    float loudest=0.f;
    const float gain=result.quality.gain;
    for(const float x:preview->original)loudest=std::max(loudest,std::abs(x));
    for(const float x:preview->resynthesized)loudest=std::max(loudest,std::abs(x*gain));
    const float scale=0.8f/std::max(0.8f,loudest);
    for(float& x:preview->original)x*=scale;
    for(float& x:preview->resynthesized)x*=scale*gain;
    processor.installPreview(std::move(preview));
    originalButton.setEnabled(true);resynthButton.setEnabled(true);stopButton.setEnabled(true);
    sampleInfo.setText(juce::String(result.duration,2)+" s  |  "+juce::String(result.sampleRate,0)+" Hz",juce::dontSendNotification);
    rootInfo.setText("Detected: "+juce::MidiMessage::getMidiNoteName(pending->rootMidi,true,true,3),juce::dontSendNotification);
    rootSelector.setSelectedId(pending->rootMidi+1,juce::dontSendNotification);
    status.setText("Analysis complete — accept resynthesis",juce::dontSendNotification);
    repaint();
}
void ReSynthAudioProcessorEditor::accept() {
    if(!pending)return;
    auto editable=std::make_shared<rs::Model>(*pending);
    // Use detected source filename as the default sound name; category is user-selectable.
    editable->category=category.getText();
    processor.installModel(editable);pending.reset();hasPending=false;waveform.clear();acceptButton.setEnabled(false);
    processor.installPreview({});
    originalButton.setEnabled(false);resynthButton.setEnabled(false);stopButton.setEnabled(false);
    qaInfo.setText("Accepted: preview audio purged from memory",juce::dontSendNotification);
    status.setText("Accepted: "+editable->name+" — source audio released",juce::dontSendNotification);
    repaint();
}
void ReSynthAudioProcessorEditor::loadSound(const juce::File& file) {
    if(!file.hasFileExtension("rssound")) return;
    auto input=file.createInputStream();
    if(!input) {status.setText("Cannot read sound file",juce::dontSendNotification);return;}
    auto tree=juce::ValueTree::readFromStream(*input);
    auto restored=rs::modelFromTree(tree);
    if(!restored) {status.setText("Invalid RE-SYNTH sound file",juce::dontSendNotification);return;}
    processor.installModel(restored);
    processor.installPreview({});
    originalButton.setEnabled(false);resynthButton.setEnabled(false);stopButton.setEnabled(false);
    qaInfo.setText("Saved model loaded; original audio not retained",juce::dontSendNotification);
    pending.reset();waveform.clear();hasPending=false;acceptButton.setEnabled(false);
    rootSelector.setSelectedId(restored->rootMidi+1,juce::dontSendNotification);
    const int categoryIndex=juce::StringArray{"Keys","Brass","Woodwinds","Strings","Bass","Drum","Percussion","Synth","Vocal","FX"}.indexOf(restored->category);
    if(categoryIndex>=0) category.setSelectedId(categoryIndex+1,juce::dontSendNotification);
    rootInfo.setText("Loaded root:",juce::dontSendNotification);
    sampleInfo.setText(juce::String(restored->spectrum.count)+" partials, "+juce::String(restored->spectrum.frameCount)+" frames",juce::dontSendNotification);
    status.setText("Loaded: "+restored->name,juce::dontSendNotification);
    repaint();
}
void ReSynthAudioProcessorEditor::updateRootNote(int midi) {
    if(midi<0 || midi>127) return;
    if(pending) {
        auto modified=std::make_shared<rs::Model>(*pending);
        modified->rootMidi=midi;
        pending=std::move(modified);
    } else if(auto active=processor.currentModel()) {
        auto modified=std::make_shared<rs::Model>(*active);
        modified->rootMidi=midi;
        processor.installModel(std::move(modified));
    }
    rootInfo.setText("Root assignment:",juce::dontSendNotification);
    repaint();
}
void ReSynthAudioProcessorEditor::saveSound() {
    auto m=processor.currentModel();if(!m)return;
    auto library=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("RE-SYNTH").getChildFile("Sounds");
    if(!library.createDirectory()){status.setText("Could not create library folder",juce::dontSendNotification);return;}
    auto dest=library.getChildFile(juce::File::createLegalFileName(m->name)+".rssound");
    juce::MemoryOutputStream stream;
    rs::modelToTree(*m).writeToStream(stream);
    if(dest.replaceWithData(stream.getData(),stream.getDataSize()))status.setText("Saved: "+dest.getFileName(),juce::dontSendNotification);
    else status.setText("Sound save failed",juce::dontSendNotification);
}
void ReSynthAudioProcessorEditor::timerCallback(){int v=processor.lastNote.exchange(-1);if(v>=0&&v!=note){note=v;status.setText("MIDI: "+juce::MidiMessage::getMidiNoteName(v,true,true,3),juce::dontSendNotification);}}
