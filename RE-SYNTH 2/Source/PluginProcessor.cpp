#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace {
constexpr double twoPi = juce::MathConstants<double>::twoPi;
struct ModelSound final : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
class ModelVoice final : public juce::SynthesiserVoice {
public:
    explicit ModelVoice(const ReSynthAudioProcessor& o) : owner(o) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<ModelSound*>(s) != nullptr; }
    void startNote(int midi, float velocity, juce::SynthesiserSound*, int) override {
        model = owner.currentModel();
        if (!model) { clearCurrentNote(); return; }
        frequency = model->rootHz * std::pow(2.0, (midi - model->rootMidi) / 12.0);
        gain = velocity * 0.30;
        elapsed = 0;
        renderer.setOriginalRoot(model->rootHz);
        renderer.reset(model->spectrum, static_cast<uint32_t>(midi*7793+17));
        env.reset();
        env.noteOn();
    }
    void stopNote(float, bool tail) override {
        if (tail) env.noteOff();
        else { env.reset(); clearCurrentNote(); model.reset(); }
    }
    void pitchWheelMoved(int wheel) override {
        bend = std::pow(2.0, ((wheel - 8192) / 8192.0) * 2.0 / 12.0);
    }
    void controllerMoved(int, int) override {}
    void setVoiceSampleRate(double s) { env.setSampleRate(s); renderer.prepare(s); }
    void setEnvelope(const juce::ADSR::Parameters& p) { env.setParameters(p); }
    void setTimbreLock(float amount) { renderer.setTimbreLock(amount); }
    void renderNextBlock(juce::AudioBuffer<float>& output, int start, int count) override {
        if (!isVoiceActive() || !model) return;
        const double sampleRate=getSampleRate();
        if (sampleRate<=0) return;
        for(int n=0;n<count;++n) {
            const float ampEnv=env.getNextSample();
            if(!env.isActive()){clearCurrentNote();model.reset();break;}
            const double baseHz=frequency*bend;
            const float sample=renderer.renderSample(model->spectrum,baseHz,elapsed);
            // Clean gain stage, avoiding soft-clipping of the natural transient.
            const float outSample=sample*static_cast<float>(gain)*ampEnv;
            for(int ch=0;ch<output.getNumChannels();++ch)
                output.addSample(ch,start+n,outSample);
            elapsed+=1.0/sampleRate;
        }
    }
private:
    const ReSynthAudioProcessor& owner;
    std::shared_ptr<const rs::Model> model;
    juce::ADSR env;
    rs::dsp::AdditiveRenderer renderer;
    double frequency=440.0,bend=1.0,elapsed=0.0,gain=0.0;
};
float param(const juce::AudioProcessorValueTreeState& p, const char* id) {return p.getRawParameterValue(id)->load();}
}

juce::StringArray ReSynthAudioProcessor::effectNames() {
    return { "Bypass", "Chorus", "Jun Chorus", "Step Filter", "Tremolo", "Warm Saturator", "Overdrive", "Distortion", "Ring Mod", "Tape Echo", "Vinyl Sim", "Phaser", "Auto Wah" };
}
juce::AudioProcessorValueTreeState::ParameterLayout ReSynthAudioProcessor::createLayout() {
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<P>(juce::ParameterID{"volume", 1}, "Volume", juce::NormalisableRange<float>(0,1.5f,0.001f), 0.75f));
    layout.add(std::make_unique<P>(juce::ParameterID{"attack", 1}, "Attack", juce::NormalisableRange<float>(0.001f,2.f,0.001f,0.4f), 0.001f));
    layout.add(std::make_unique<P>(juce::ParameterID{"decay", 1}, "Decay", juce::NormalisableRange<float>(0.005f,3.f,0.001f,0.4f), 0.20f));
    layout.add(std::make_unique<P>(juce::ParameterID{"sustain", 1}, "Sustain", juce::NormalisableRange<float>(0,1.f,0.001f), 1.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"release", 1}, "Release", juce::NormalisableRange<float>(0.005f,5.f,0.001f,0.4f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"fxType",1},"Effect", effectNames(), 0));
    for (auto id : {"fx1","fx2","fx3"}) layout.add(std::make_unique<P>(juce::ParameterID{id,1}, id, juce::NormalisableRange<float>(0,1,0.001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"color",1}, "Master Color", juce::StringArray{"Clean","SSL","Neve","MPC60","MPC3000","SP-1200","ASR-10"}, 0));
    layout.add(std::make_unique<P>(juce::ParameterID{"colorDrive", 1}, "Color Drive", juce::NormalisableRange<float>(0.f,1.f,0.001f), 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID{"timbreLock", 1}, "Timbre Lock", juce::NormalisableRange<float>(0.f,1.f,0.001f), 0.35f));
    return layout;
}
ReSynthAudioProcessor::ReSynthAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createLayout()) {
    installModel(std::make_shared<rs::Model>());
    for (int i=0; i<rs::maxVoices; ++i) synth.addVoice(new ModelVoice(*this));
    synth.addSound(new ModelSound());
    synth.setNoteStealingEnabled(true);
}
void ReSynthAudioProcessor::installModel(std::shared_ptr<const rs::Model> m) {
    if (m) std::atomic_store(&sound, std::move(m));
}
void ReSynthAudioProcessor::installPreview(std::shared_ptr<const PreviewAudio> p) {
    stopPreview();
    std::atomic_store(&previewSound,std::move(p));
}
void ReSynthAudioProcessor::startPreview(bool original) {
    if(!std::atomic_load(&previewSound))return;
    previewMode.store(original?1:2,std::memory_order_release);
    previewGeneration.fetch_add(1,std::memory_order_acq_rel);
}
void ReSynthAudioProcessor::stopPreview() {
    previewMode.store(0,std::memory_order_release);
    previewGeneration.fetch_add(1,std::memory_order_acq_rel);
}
bool ReSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
void ReSynthAudioProcessor::prepareToPlay(double sampleRate, int maxBlockSize) {
    sr = sampleRate; synth.setCurrentPlaybackSampleRate(sampleRate);
    masterColor.prepare(sampleRate, maxBlockSize);
    setLatencySamples(masterColor.latencySamples());
    delayL.assign(static_cast<size_t>(sr * 2), 0.f);
    delayR.assign(static_cast<size_t>(sr * 2), 0.f);
    delayIndex=0; fxPhase=0; filterL=filterR=wahEnvelope=0;
    for (int i=0;i<synth.getNumVoices();++i) static_cast<ModelVoice*>(synth.getVoice(i))->setVoiceSampleRate(sr);
}
void ReSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    // A/B is an offline-analyzed, level-matched MONO preview. It bypasses all
    // instrument FX/master colors for meaningful reconstruction comparison.
    const int selected=previewMode.load(std::memory_order_acquire);
    if(selected!=0) {
        const auto p=std::atomic_load(&previewSound);
        if(p) {
            const unsigned generation=previewGeneration.load(std::memory_order_acquire);
            if(generation!=playingGeneration) {
                playingGeneration=generation;previewPosition=0;
                synth.allNotesOff(0,false);
            }
            const auto& samples=selected==1?p->original:p->resynthesized;
            const float volume=param(parameters,"volume");
            const double step=p->sourceSampleRate/std::max(8000.,sr);
            for(int i=0;i<buffer.getNumSamples() && previewPosition<samples.size();++i) {
                const auto index=static_cast<size_t>(previewPosition);
                const auto next=std::min(index+1,samples.size()-1);
                const float frac=static_cast<float>(previewPosition-index);
                const float sample=(samples[index]*(1.f-frac)+samples[next]*frac)*volume;
                for(int ch=0;ch<buffer.getNumChannels();++ch)buffer.setSample(ch,i,sample);
                previewPosition+=step;
            }
            if(previewPosition>=samples.size())previewMode.store(0,std::memory_order_release);
            return;
        }
    }
    juce::ADSR::Parameters adsr { param(parameters,"attack"), param(parameters,"decay"), param(parameters,"sustain"), param(parameters,"release") };
    const float timbre=param(parameters,"timbreLock");
    for (int i=0;i<synth.getNumVoices();++i) {
        auto* voice=static_cast<ModelVoice*>(synth.getVoice(i));
        voice->setEnvelope(adsr);
        voice->setTimbreLock(timbre);
    }
    for (const auto meta : midi) if (meta.getMessage().isNoteOn()) lastNote.store(meta.getMessage().getNoteNumber());
    synth.renderNextBlock(buffer,midi,0,buffer.getNumSamples());
    auto fx = static_cast<int>(param(parameters,"fxType"));
    float a=param(parameters,"fx1"), b=param(parameters,"fx2"), c=param(parameters,"fx3");
    const int col=static_cast<int>(param(parameters,"color"));
    const float colorDrive=param(parameters,"colorDrive");
    auto* left=buffer.getWritePointer(0); auto* right=buffer.getWritePointer(1);
    for (int i=0;i<buffer.getNumSamples();++i) {
        float l=left[i],r=right[i];
        double speed = (0.10 + a*6.0)/sr;
        fxPhase+=speed; if(fxPhase>=1.0) fxPhase-=1.0;
        float lfo=static_cast<float>(std::sin(twoPi*fxPhase));
        float mix=c*0.8f;
        switch(fx) {
            case 1: case 2: { // two gently modulated stereo delay taps
                int maxDelay=static_cast<int>(delayL.size());
                int d1=static_cast<int>((0.008 + (fx==2?0.013:0.018) + (1+lfo)*0.003*b)*sr);
                int d2=static_cast<int>((0.012 + (1-lfo)*0.004*b)*sr);
                l=l*(1-mix)+delayL[(delayIndex-d1+maxDelay)%maxDelay]*mix;
                r=r*(1-mix)+delayR[(delayIndex-d2+maxDelay)%maxDelay]*mix;
                break;
            }
            case 3: { float phase=std::floor(fxPhase*8.0f)/8.0f; float cutoff = 0.03f + 0.4f*phase*b; filterL+=cutoff*(l-filterL); filterR+=cutoff*(r-filterR); l=l*(1-mix)+static_cast<float>(filterL)*mix; r=r*(1-mix)+static_cast<float>(filterR)*mix; break; }
            case 4: { float level=1.f-mix*(0.5f+0.5f*lfo); l*=level;r*=level;break; }
            case 5: { float drive=1.0f+15*a; l=std::tanh(l*drive)/std::tanh(drive);r=std::tanh(r*drive)/std::tanh(drive);break; }
            case 6: { float drive=1.0f+30*a; l=std::tanh(l*drive)*0.8f; r=std::tanh(r*drive)*0.8f;break; }
            case 7: { float drive=1.0f+50*a; l=juce::jlimit(-b-0.05f,b+0.05f,l*drive)*0.7f;r=juce::jlimit(-b-0.05f,b+0.05f,r*drive)*0.7f;break; }
            case 8: { float carrier=std::sin(static_cast<float>(twoPi*fxPhase*(10.0+100*a))); l*=1-mix+mix*carrier;r*=1-mix+mix*carrier;break; }
            case 9: { int n=static_cast<int>(delayL.size()); int d=juce::jlimit(1,n-1,static_cast<int>((0.06+a*0.75)*sr)); float dl=delayL[(delayIndex-d+n)%n], dr=delayR[(delayIndex-d+n)%n];delayL[delayIndex]=std::tanh(l+dl*b*0.8f);delayR[delayIndex]=std::tanh(r+dr*b*0.8f);l=l*(1-mix)+dl*mix;r=r*(1-mix)+dr*mix;break; }
            case 10: { float wobble=1.0f+0.025f*a*lfo; l=std::tanh(l*wobble*(1+2*b));r=std::tanh(r*wobble*(1+2*b));break; }
            case 11: { float coeff=0.01f+0.4f*(0.5f+0.5f*lfo)*b;filterL+=coeff*(l-filterL);filterR+=coeff*(r-filterR);l=l*(1-mix)+static_cast<float>(l-2*filterL)*mix;r=r*(1-mix)+static_cast<float>(r-2*filterR)*mix;break; }
            case 12: {wahEnvelope=0.998*wahEnvelope+0.002*(std::abs(l)+std::abs(r))*0.5;float coeff=juce::jlimit(0.005f,0.8f,static_cast<float>(0.02+a*wahEnvelope*4*b));filterL+=coeff*(l-filterL);filterR+=coeff*(r-filterR);l=l*(1-mix)+static_cast<float>(l-filterL)*mix;r=r*(1-mix)+static_cast<float>(r-filterR)*mix;break;}
            default: break;
        }
        if(fx==1||fx==2) {delayL[delayIndex]=left[i];delayR[delayIndex]=right[i];}
        if(!delayL.empty()) {if (++delayIndex >= static_cast<int>(delayL.size())) delayIndex=0;}
        left[i]=l;right[i]=r;
    }
    masterColor.process(buffer, static_cast<MasterColor::Mode>(col), colorDrive);
    const float volume=param(parameters,"volume");
    buffer.applyGain(volume);
}
void ReSynthAudioProcessor::getStateInformation(juce::MemoryBlock& dst) {
    juce::ValueTree state("RESYNTH");
    state.addChild(parameters.copyState(),-1,nullptr);
    if (auto m=currentModel()) state.addChild(rs::modelToTree(*m),-1,nullptr);
    juce::MemoryOutputStream stream(dst,false); state.writeToStream(stream);
}
void ReSynthAudioProcessor::setStateInformation(const void* data,int size) {
    installPreview({});
    auto state=juce::ValueTree::readFromData(data,static_cast<size_t>(size));
    if(!state.hasType("RESYNTH")) return;
    if(auto p=state.getChildWithName("PARAMETERS");p.isValid()) parameters.replaceState(p);
    if(auto s=state.getChildWithName("SOUND");s.isValid()) installModel(rs::modelFromTree(s));
}
juce::AudioProcessorEditor* ReSynthAudioProcessor::createEditor() {
    return new ReSynthAudioProcessorEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ReSynthAudioProcessor();}
