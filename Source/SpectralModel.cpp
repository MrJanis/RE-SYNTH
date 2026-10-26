#include "SpectralModel.h"
#include "PitchDetector.h"
#include "PeakTracking.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

namespace rs {
Model::Model() {
    spectrum.count=1;
    spectrum.frameCount=2;
    spectrum.times[0]=0.f; spectrum.times[1]=1.f;
    spectrum.partials[0].ratio=1.f;
    spectrum.partials[0].levels[0]=0.7f;
    spectrum.partials[0].levels[1]=0.7f;
    dsp::buildSpectralEnvelope(spectrum,rootHz);
}
namespace {
constexpr double pi=juce::MathConstants<double>::pi;
constexpr double twoPi=2.0*pi;

class FrameAnalyzer {
public:
    explicit FrameAnalyzer(int order) : size(1<<order),fft(order),data(static_cast<size_t>(size*2)),window(static_cast<size_t>(size)) {
        for(int i=0;i<size;++i) window[static_cast<size_t>(i)]=static_cast<float>(0.5-0.5*std::cos(twoPi*i/(size-1)));
    }
    int fftSize() const noexcept {return size;}
    void transform(const std::vector<float>& audio,int center) {
        std::fill(data.begin(),data.end(),0.f);
        windowEnergy=0.; windowSum=0.;
        const long long start=static_cast<long long>(center)-size/2;
        for(int i=0;i<size;++i) {
            const long long s=start+i;
            if(s<0 || s>=static_cast<long long>(audio.size()))continue;
            const float w=window[static_cast<size_t>(i)];
            data[static_cast<size_t>(i)]=audio[static_cast<size_t>(s)]*w;
            windowEnergy+=w*w;windowSum+=w;
        }
        fft.performRealOnlyForwardTransform(data.data());
    }
    std::vector<tracking::Peak> getPeaks(double sr,int center,int limit=96) const {
        std::vector<tracking::Peak> peaks;
        float maximum=0.f;
        const int maxBin=static_cast<int>(size*0.48);
        for(int k=2;k<maxBin;++k) maximum=std::max(maximum,magnitude(k));
        const float threshold=std::max(0.00005f,maximum*0.012f);
        for(int k=2;k<maxBin-1;++k) {
            const float m=magnitude(k),l=magnitude(k-1),r=magnitude(k+1);
            if(m<l || m<=r || m<threshold)continue;
            const double ll=std::log(std::max(l,1.e-12f));
            const double cc=std::log(std::max(m,1.e-12f));
            const double rr=std::log(std::max(r,1.e-12f));
            const double curvature=ll-2*cc+rr;
            const double shift=std::abs(curvature)>1.e-8 ? std::clamp(0.5*(ll-rr)/curvature,-0.5,0.5) : 0.;
            const double freq=(k+shift)*sr/size;
            if(freq<30. || freq>=sr*0.49)continue;
            const double phaseAtStart=std::atan2(data[static_cast<size_t>(2*k+1)],data[static_cast<size_t>(2*k)])+pi/2.0;
            const double phaseAtCenter=std::remainder(phaseAtStart+twoPi*freq*(size*0.5)/sr,twoPi);
            const double amplitude=2.0*m/std::max(1.0,windowSum);
            peaks.push_back({static_cast<float>(freq),static_cast<float>(amplitude),static_cast<float>(phaseAtCenter)});
        }
        std::sort(peaks.begin(),peaks.end(),[](auto a,auto b){return a.amplitude>b.amplitude;});
        if(static_cast<int>(peaks.size())>limit)peaks.resize(static_cast<size_t>(limit));
        return peaks;
    }
    std::array<float,dsp::noiseBands> getBandRMS(double sr) const {
        std::array<double,dsp::noiseBands> energy {};
        for(int k=1;k<size/2;++k) {
            const double hz=k*sr/size;
            int best=0;double diff=1.e12;
            for(int b=0;b<dsp::noiseBands;++b) {
                const double d=std::abs(std::log2(std::max(1.0,hz)/dsp::noiseCentersHz[b]));
                if(d<diff){diff=d;best=b;}
            }
            energy[static_cast<size_t>(best)]+=static_cast<double>(data[static_cast<size_t>(2*k)])*data[static_cast<size_t>(2*k)]
                                                 +static_cast<double>(data[static_cast<size_t>(2*k+1)])*data[static_cast<size_t>(2*k+1)];
        }
        std::array<float,dsp::noiseBands> rms {};
        const double divisor=std::max(1.0,size*windowEnergy);
        for(int b=0;b<dsp::noiseBands;++b)
            rms[static_cast<size_t>(b)]=static_cast<float>(std::sqrt(2.0*energy[static_cast<size_t>(b)]/divisor));
        return rms;
    }
private:
    float magnitude(int bin) const noexcept {
        return std::hypot(data[static_cast<size_t>(2*bin)],data[static_cast<size_t>(2*bin+1)]);
    }
    int size;
    juce::dsp::FFT fft;
    std::vector<float> data,window;
    double windowEnergy=1.,windowSum=1.;
};
std::vector<int> chooseKeyframes(int total) {
    std::vector<int> indices;
    if(total<=dsp::maxFrames) {
        for(int i=0;i<total;++i)indices.push_back(i);
        return indices;
    }
    constexpr int attackFrames=16;
    for(int i=0;i<attackFrames;++i)indices.push_back(i);
    const int remaining=dsp::maxFrames-attackFrames;
    for(int i=1;i<=remaining;++i) {
        const int frame=attackFrames-1+static_cast<int>(std::lround((total-attackFrames)*static_cast<double>(i)/remaining));
        if(frame>indices.back())indices.push_back(std::min(total-1,frame));
    }
    return indices;
}
}
AnalysisResult analyzeFile(const juce::File& file) {
    AnalysisResult out;
    if(!file.hasFileExtension("wav;wave;aif;aiff")){out.error="Choose an uncompressed .wav, .aif or .aiff note.";return out;}
    juce::AudioFormatManager fm;fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if(!reader || reader->lengthInSamples<2048 || reader->sampleRate<=0) {
        out.error="Could not read a usable audio note.";return out;
    }
    if(reader->lengthInSamples>static_cast<juce::int64>(reader->sampleRate*15.)){
        out.error="Prototype limit: one note no longer than 15 seconds.";return out;
    }
    const int n=static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),n);
    if(!reader->read(&buffer,0,n,0,true,true)){out.error="Audio read failed.";return out;}
    std::vector<float> mono(static_cast<size_t>(n));
    float peak=0.f;
    for(int i=0;i<n;++i) {
        float sum=0.f;
        for(int ch=0;ch<buffer.getNumChannels();++ch)sum+=buffer.getSample(ch,i);
        mono[static_cast<size_t>(i)]=sum/buffer.getNumChannels();
        peak=std::max(peak,std::abs(mono[static_cast<size_t>(i)]));
    }
    if(peak<0.00001f){out.error="The selected note is silent.";return out;}
    for(float& x:mono)x/=peak;
    out.waveform.resize(400);
    for(size_t i=0;i<out.waveform.size();++i) {
        const auto lo=i*mono.size()/out.waveform.size();
        const auto hi=(i+1)*mono.size()/out.waveform.size();
        float top=0.f;
        for(auto j=lo;j<hi;++j)top=std::max(top,std::abs(mono[j]));
        out.waveform[i]=top;
    }
    out.duration=n/reader->sampleRate;
    out.sampleRate=reader->sampleRate;
    const double pitchGuess=pitch::estimatePitch(mono,reader->sampleRate);
    out.pitchAutoDetected=std::isfinite(pitchGuess) && pitchGuess>=45. && pitchGuess<=1500.;
    // Uncertain pitch should not be treated as an unsupported WAV file.
    // Allow spectral analysis with an explicit manual MIDI root assignment.
    // Frequencies still retain their measured absolute spectral positions.
    const double rootHz=out.pitchAutoDetected ? pitchGuess : 261.625565;
    auto model=std::make_shared<Model>();
    model->name=file.getFileNameWithoutExtension();
    model->rootHz=rootHz;
    model->rootMidi=juce::jlimit(0,127,static_cast<int>(std::lround(69+12*std::log2(rootHz/440.))));
    const double sr=reader->sampleRate;
    int order=11;
    while(order<14 && (1<<order)<sr*16.0/rootHz) ++order;
    FrameAnalyzer analyzer(order);
    const int hop=analyzer.fftSize()/8; // 87.5% Hann overlap
    const int analysisCount=1+(n-1)/hop;
    std::vector<std::vector<tracking::Peak>> peaks;
    peaks.reserve(static_cast<size_t>(analysisCount));
    for(int f=0;f<analysisCount;++f) {
        analyzer.transform(mono,f*hop);
        peaks.push_back(analyzer.getPeaks(sr,f*hop));
    }
    auto tracks=tracking::linkPeaks(peaks);
    if(tracks.empty()){out.error="No stable sinusoidal trajectories detected.";return out;}
    if(tracks.size()>dsp::maxPartials) tracks.resize(dsp::maxPartials);
    auto keys=chooseKeyframes(analysisCount);
    auto& spec=model->spectrum;
    spec.count=static_cast<int>(tracks.size());
    spec.frameCount=static_cast<int>(keys.size());
    for(int j=0;j<spec.frameCount;++j)
        spec.times[j]=static_cast<float>(static_cast<double>(keys[static_cast<size_t>(j)]*hop)/sr);
    for(int p=0;p<spec.count;++p) {
        const auto& tr=tracks[static_cast<size_t>(p)];
        auto& dst=spec.partials[p];
        dst.ratio=tr.events.front().peak.frequency/static_cast<float>(rootHz);
        const auto& start=tr.events.front();
        const double time=static_cast<double>(start.frame*hop)/sr;
        dst.initialPhase=static_cast<float>(std::remainder(start.peak.phase-twoPi*start.peak.frequency*time,twoPi));
        for(int j=0;j<spec.frameCount;++j) {
            const int idx=keys[static_cast<size_t>(j)];
            dst.levels[j]=std::clamp(tr.amplitudeAt(idx),0.f,2.f);
            dst.ratios[j]=std::max(0.01f,tr.frequencyAt(idx)/static_cast<float>(rootHz));
        }
    }
    dsp::buildSpectralEnvelope(spec,rootHz);
    // True deterministic subtraction, performed OFFLINE before measuring noise.
    // The residual is never stored as waveform audio in the accepted sound.
    const auto reconstructed=dsp::renderTestTone(spec,rootHz,sr,n);
    std::vector<float> residual(mono.size());
    for(int i=0;i<n;++i)
        residual[static_cast<size_t>(i)]=mono[static_cast<size_t>(i)]-reconstructed[static_cast<size_t>(i)];
    // Use a shorter Hann window for noise envelope to better retain onsets.
    FrameAnalyzer noiseAnalyzer(11);
    for(int j=0;j<spec.frameCount;++j) {
        const int center=keys[static_cast<size_t>(j)]*hop;
        noiseAnalyzer.transform(residual,center);
        const auto bands=noiseAnalyzer.getBandRMS(sr);
        for(int b=0;b<dsp::noiseBands;++b)
            spec.noise[b].levels[j]=std::clamp(bands[static_cast<size_t>(b)]*0.75f,0.f,0.5f);
    }
    // QA is evaluated with the noise model enabled, after additive subtraction.
    // Noise is stochastic; spectral error is a diagnostic, not a transparency guarantee.
    out.resynthAudio=dsp::renderTestTone(spec,rootHz,sr,n);
    out.quality=qa::evaluate(mono,out.resynthAudio,sr);
    out.sourceAudio=std::move(mono); // held only until Accept/Cancel
    out.model=model;
    return out;
}
juce::ValueTree modelToTree(const Model& m) {
    juce::ValueTree root("SOUND");
    root.setProperty("schema",5,nullptr);
    root.setProperty("name",m.name,nullptr);
    root.setProperty("category",m.category,nullptr);
    root.setProperty("rootMidi",m.rootMidi,nullptr);
    root.setProperty("rootHz",m.rootHz,nullptr);
    root.setProperty("frameCount",m.spectrum.frameCount,nullptr);
    for(int j=0;j<m.spectrum.frameCount;++j)root.setProperty("t"+juce::String(j),m.spectrum.times[j],nullptr);
    for(int i=0;i<m.spectrum.count;++i) {
        juce::ValueTree p("PARTIAL");
        p.setProperty("ratio",m.spectrum.partials[i].ratio,nullptr);
        p.setProperty("phase",m.spectrum.partials[i].initialPhase,nullptr);
        for(int j=0;j<m.spectrum.frameCount;++j) {
            p.setProperty("a"+juce::String(j),m.spectrum.partials[i].levels[j],nullptr);
            p.setProperty("f"+juce::String(j),m.spectrum.partials[i].ratios[j],nullptr);
        }
        root.addChild(p,-1,nullptr);
    }
    for(int b=0;b<dsp::noiseBands;++b) {
        juce::ValueTree band("NOISE");
        band.setProperty("band",b,nullptr);
        for(int j=0;j<m.spectrum.frameCount;++j)
            band.setProperty("a"+juce::String(j),m.spectrum.noise[b].levels[j],nullptr);
        root.addChild(band,-1,nullptr);
    }
    // Pitch-dependent envelope can be reconstructed from tracked partials. We do
    // not need to serialize this derived cache, which keeps .rssound small.
    return root;
}
std::shared_ptr<const Model> modelFromTree(const juce::ValueTree& tree) {
    if(!tree.hasType("SOUND"))return {};
    auto m=std::make_shared<Model>();
    m->name=tree.getProperty("name","Imported").toString();
    m->category=tree.getProperty("category","Synth").toString();
    m->rootMidi=juce::jlimit(0,127,static_cast<int>(tree.getProperty("rootMidi",60)));
    m->rootHz=static_cast<double>(tree.getProperty("rootHz",261.625565));
    if(!std::isfinite(m->rootHz) || m->rootHz<=0.) m->rootHz=261.625565;
    auto& d=m->spectrum;
    const int schema=static_cast<int>(tree.getProperty("schema",0));
    if(schema>=3) {
        d.frameCount=juce::jlimit(1,dsp::maxFrames,static_cast<int>(tree.getProperty("frameCount",1)));
        for(int f=0;f<d.frameCount;++f) {
            const float t=static_cast<float>(tree.getProperty("t"+juce::String(f),f*0.3f));
            d.times[f]=std::max(f>0 ? d.times[f-1]+0.0001f : 0.f,std::isfinite(t)?t:f*0.3f);
        }
    } else {
        d.frameCount=6;
        constexpr std::array<float,6> times {0.f,0.08f,0.25f,0.55f,1.1f,2.2f};
        std::copy(times.begin(),times.end(),d.times.begin());
    }
    for(int i=0;i<tree.getNumChildren();++i) {
        const auto child=tree.getChild(i);
        if(child.hasType("NOISE") && schema>=4) {
            const int b=static_cast<int>(child.getProperty("band",-1));
            if(b>=0 && b<dsp::noiseBands)
                for(int f=0;f<d.frameCount;++f) {
                    const float a=static_cast<float>(child.getProperty("a"+juce::String(f),0.f));
                    d.noise[b].levels[f]=std::isfinite(a)?juce::jlimit(0.f,0.5f,a):0.f;
                }
            continue;
        }
        if(!child.hasType("PARTIAL") || d.count>=dsp::maxPartials)continue;
        auto& partial=d.partials[d.count++];
        const float ratio=static_cast<float>(child.getProperty("ratio",1.f));
        partial.ratio=std::isfinite(ratio) && ratio>0.f?ratio:1.f;
        if(schema>=3) {
            for(int f=0;f<d.frameCount;++f) {
                const float a=static_cast<float>(child.getProperty("a"+juce::String(f),0.f));
                partial.levels[f]=std::isfinite(a)?juce::jlimit(0.f,2.f,a):0.f;
                const float freq=static_cast<float>(child.getProperty("f"+juce::String(f),partial.ratio));
                partial.ratios[f]=schema>=4 && std::isfinite(freq) && freq>0.f ? freq : partial.ratio;
            }
            const float phase=static_cast<float>(child.getProperty("phase",0.f));
            partial.initialPhase=std::isfinite(phase)?phase:0.f;
        } else {
            const float amp=static_cast<float>(child.getProperty("amp",0.f));
            const float decay=std::max(0.05f,static_cast<float>(child.getProperty("decay",2.f)));
            for(int f=0;f<d.frameCount;++f) {
                partial.levels[f]=juce::jlimit(0.f,2.f,amp*std::exp(-d.times[f]/decay));
                partial.ratios[f]=partial.ratio;
            }
        }
    }
    if(d.count==0)return {};
    dsp::buildSpectralEnvelope(d,m->rootHz);
    return m;
}
} // namespace rs
