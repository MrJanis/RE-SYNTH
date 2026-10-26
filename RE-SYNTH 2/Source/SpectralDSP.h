#pragma once
// RE-SYNTH v0.4 portable oscillator-bank DSP. No sound wavetables or allocations
// in renderSample(). Spectral models are immutable while played by a voice.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rs::dsp {
constexpr int maxPartials = 128;  // Current CPU-budget limit, NOT an acoustic limit.
constexpr int maxFrames = 128;
constexpr int noiseBands = 8;
constexpr int envelopeBins = 48; // Log-frequency spectral envelope; no waveform tables.
constexpr double tau = 6.28318530717958647692;
constexpr std::array<float, noiseBands> noiseCentersHz {95.f, 190.f, 380.f, 760.f, 1520.f, 3040.f, 6080.f, 12160.f};

struct Partial {
    float ratio = 1.f;                  // Nominal (possibly inharmonic) / root frequency.
    float initialPhase = 0.f;           // Phase in radians at time zero, estimated offline.
    std::array<float, maxFrames> levels {};        // Linear amplitude, zero for absence.
    std::array<float, maxFrames> ratios {};        // Frequency trajectory, 0 = use ratio.
};
struct NoiseBand {
    std::array<float, maxFrames> levels {};         // Residual RMS band envelopes.
};
struct SpectralData {
    int count = 0;
    int frameCount = 0;
    std::array<float, maxFrames> times {};
    std::array<Partial, maxPartials> partials {};
    std::array<NoiseBand, noiseBands> noise {};
    std::array<std::array<float, envelopeBins>, maxFrames> envelope {};
};
struct Blend { int a=0, b=0; float fraction=0.f; };
inline Blend frameBlend(const SpectralData& d, double seconds) noexcept {
    if (d.frameCount < 2 || seconds <= d.times[0]) return {};
    const int last = d.frameCount - 1;
    if (seconds >= d.times[last]) return {last,last,0.f};
    // 128 points maximum; in future use a frame index cursor per voice.
    int lo=0, hi=last;
    while (hi-lo>1) { const int mid=(lo+hi)/2; if(seconds<d.times[mid])hi=mid;else lo=mid; }
    const double length=std::max(1.e-7, static_cast<double>(d.times[hi])-d.times[lo]);
    return {lo,hi,static_cast<float>(std::clamp((seconds-d.times[lo])/length,0.,1.))};
}
inline float mix(float x,float y,float a) noexcept {return x+(y-x)*a;}
inline float lerpLevel(const Partial& p, Blend b) noexcept {
    return mix(p.levels[b.a], p.levels[b.b], b.fraction);
}
inline float lerpRatio(const Partial& p, Blend b) noexcept {
    const float a = p.ratios[b.a]>0.f ? p.ratios[b.a] : p.ratio;
    const float c = p.ratios[b.b]>0.f ? p.ratios[b.b] : p.ratio;
    return mix(a,c,b.fraction);
}
// Lookup-free high-accuracy 11th-order sine, range-reduced to [-pi/2, pi/2].
// This is NOT a sound wavetable; every tracked partial is individually synthesized.
inline double fastSinCycles(double phaseCycles) noexcept {
    double x;
    if (phaseCycles<0.25) x=tau*phaseCycles;
    else if(phaseCycles<0.75) x=tau*(0.5-phaseCycles);
    else x=tau*(phaseCycles-1.0);
    const double xx=x*x;
    // Horner polynomial: max absolute error under ~6e-8 on the reduced range.
    return x*(1.0+xx*(-1.0/6.0+xx*(1.0/120.0+xx*(-1.0/5040.0+xx*(1.0/362880.0-xx/39916800.0)))));
}
inline float nyquistGuard(double hz,double sampleRate) noexcept {
    if(hz<=0 || hz>=sampleRate*0.49) return 0.f;
    if(hz<=sampleRate*0.45) return 1.f;
    return static_cast<float>((sampleRate*0.49-hz)/(sampleRate*0.04));
}
// Smooth envelope is stored in absolute frequency, helping pitch-transposed notes
// maintain broad formant character. Calculated OFFLINE, not on the audio thread.
inline double envelopeHz(int bin) noexcept {
    return 55.0*std::exp2(8.6*static_cast<double>(bin)/(envelopeBins-1));
}
inline void buildSpectralEnvelope(SpectralData& data,double rootHz) {
    const int frames=std::min(data.frameCount,maxFrames);
    for(int f=0;f<frames;++f) {
        for(int b=0;b<envelopeBins;++b) {
            const double target=std::log2(envelopeHz(b));
            double energy=0.;
            for(int p=0;p<std::min(data.count,maxPartials);++p) {
                const auto& part=data.partials[p];
                const double ratio=part.ratios[f]>0.?part.ratios[f]:part.ratio;
                const double freq=rootHz*ratio;
                if(freq<=0)continue;
                const double octaves=(std::log2(freq)-target)/0.50;
                if(std::abs(octaves)>3.5)continue;
                const double amp=part.levels[f];
                energy+=amp*amp*std::exp(-0.5*octaves*octaves);
            }
            data.envelope[f][b]=static_cast<float>(std::sqrt(energy));
        }
    }
}
inline float envelopeAt(const SpectralData& data, Blend blend, double hz) noexcept {
    const double pos=std::clamp(std::log2(std::max(1.,hz)/55.)*
                      ((envelopeBins-1)/8.6),0.,static_cast<double>(envelopeBins-1));
    const int a=static_cast<int>(pos),b=std::min(a+1,envelopeBins-1);
    const float frac=static_cast<float>(pos-a);
    const float atStart=mix(data.envelope[blend.a][a],data.envelope[blend.a][b],frac);
    const float atEnd=mix(data.envelope[blend.b][a],data.envelope[blend.b][b],frac);
    return mix(atStart,atEnd,blend.fraction);
}
struct Bandpass {
    double b0=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    float scale=1.f;
    void prepare(double sr,double center) noexcept {
        const double hz=std::min(center,sr*0.445);
        const double w=tau*hz/sr;
        const double alpha=std::sin(w)/(2.0*1.15);
        const double inv=1.0/(1.0+alpha);
        b0=alpha*inv; b2=-b0;
        a1=-2.0*std::cos(w)*inv; a2=(1.0-alpha)*inv;
        z1=z2=0.;
        // Approximate energy compensation; capped for stability at low frequencies.
        scale=static_cast<float>(std::min(10.0,std::sqrt(sr/std::max(100.0,2.5*hz))));
    }
    float process(float x) noexcept {
        const double y=b0*x+z1;
        z1=z2-a1*y;
        z2=b2*x-a2*y;
        return static_cast<float>(y);
    }
};
class AdditiveRenderer {
public:
    void prepare(double sampleRate) noexcept {
        sr=std::max(8000.0,sampleRate);
        for(int b=0;b<noiseBands;++b) filters[b].prepare(sr,noiseCentersHz[b]);
        remaining=0;
    }
    void setOriginalRoot(double hz) noexcept { originalRootHz=std::max(1.,hz); }
    void setTimbreLock(float amount) noexcept { timbreLock=std::clamp(amount,0.f,1.f); }
    void reset(const SpectralData& model, uint32_t seed=0x87654321u) noexcept {
        phase.fill(0.);
        for(int p=0;p<model.count && p<maxPartials;++p)
        {
            const double start=static_cast<double>(model.partials[p].initialPhase)/tau;
            phase[p]=start-std::floor(start);
        }
        randomState=seed==0?0x87654321u:seed;
        for(int b=0;b<noiseBands;++b) filters[b].z1=filters[b].z2=0.;
        remaining=0;lastFundamental=-1.;
    }
    float renderSample(const SpectralData& model,double fundamentalHz,double seconds) noexcept {
        if(remaining<=0 || std::abs(fundamentalHz-lastFundamental)>1.e-8) {
            updateControl(model,fundamentalHz,seconds);
        }
        --remaining;
        double sum=0.;
        const int count=std::min(model.count,maxPartials);
        for(int p=0;p<count;++p) {
            const float amp=amplitude[p];
            const double frequency=hz[p];
            if(amp>1.e-6f) {
                const float band=nyquistGuard(frequency,sr);
                if(band>0.f)sum+=fastSinCycles(phase[p])*amp*band;
            }
            phase[p]+=frequency/sr;
            phase[p]-=std::floor(phase[p]);
            amplitude[p]+=ampDelta[p];
            hz[p]+=hzDelta[p];
        }
        if(hasNoise) {
            randomState^=randomState<<13; randomState^=randomState>>17; randomState^=randomState<<5;
            const float white=static_cast<float>((randomState&0x00ffffff)/8388607.5 - 1.0);
            for(int b=0;b<noiseBands;++b) {
                if(noiseCentersHz[b]<sr*0.48)
                    sum+=filters[b].process(white)*static_cast<double>(filters[b].scale*noiseLevel[b]);
                noiseLevel[b]+=noiseDelta[b];
            }
        }
        return static_cast<float>(sum);
    }
    size_t stateBytes() const noexcept {return sizeof(*this);}
private:
    static constexpr int controlPeriod=32; // ~0.7ms at 44.1k; smooth interpolation.
    void updateControl(const SpectralData& model,double fundamentalHz,double seconds) noexcept {
        lastFundamental=fundamentalHz;
        remaining=controlPeriod;
        const auto now=frameBlend(model,seconds);
        const auto next=frameBlend(model,seconds+controlPeriod/sr);
        const float inverse=1.f/controlPeriod;
        const int count=std::min(model.count,maxPartials);
        for(int p=0;p<count;++p) {
            const auto& part=model.partials[p];
            const auto corrected=[&](Blend frame) noexcept {
                const float amp=lerpLevel(part,frame);
                if(timbreLock<0.0001f || amp<1.e-7f) return amp;
                const double srcHz=std::max(1.,originalRootHz*lerpRatio(part,frame));
                const double dstHz=std::max(1.,fundamentalHz*lerpRatio(part,frame));
                const float original=envelopeAt(model,frame,srcHz);
                if(original<1.e-6f) return amp;
                const float shifted=envelopeAt(model,frame,dstHz);
                const float ratio=std::clamp(std::sqrt(std::max(0.f,shifted)/original),0.4f,1.8f);
                return amp*(1.f+timbreLock*(ratio-1.f));
            };
            amplitude[p]=corrected(now);
            ampDelta[p]=(corrected(next)-amplitude[p])*inverse;
            hz[p]=fundamentalHz*lerpRatio(part,now);
            hzDelta[p]=(fundamentalHz*lerpRatio(part,next)-hz[p])/controlPeriod;
        }
        hasNoise=false;
        for(int b=0;b<noiseBands;++b) {
            noiseLevel[b]=mix(model.noise[b].levels[now.a],model.noise[b].levels[now.b],now.fraction);
            const float end=mix(model.noise[b].levels[next.a],model.noise[b].levels[next.b],next.fraction);
            noiseDelta[b]=(end-noiseLevel[b])*inverse;
            hasNoise |= noiseLevel[b]>1.e-7f || end>1.e-7f;
        }
    }
    double sr=44100.,lastFundamental=-1.,originalRootHz=261.625565;
    float timbreLock=0.35f;
    int remaining=0;
    bool hasNoise=false;
    std::array<double,maxPartials> phase {},hz {},hzDelta {};
    std::array<float,maxPartials> amplitude {},ampDelta {};
    std::array<Bandpass,noiseBands> filters {};
    std::array<float,noiseBands> noiseLevel {},noiseDelta {};
    uint32_t randomState=0x87654321u;
};
inline std::vector<float> renderTestTone(const SpectralData& data,double fundamentalHz,double sr,int samples) {
    std::vector<float> output(static_cast<size_t>(std::max(0,samples)));
    AdditiveRenderer engine;
    engine.prepare(sr);engine.setOriginalRoot(fundamentalHz);engine.setTimbreLock(0.f);engine.reset(data);
    for(int i=0;i<samples;++i) output[static_cast<size_t>(i)]=engine.renderSample(data,fundamentalHz,i/sr);
    return output;
}
} // namespace rs::dsp
