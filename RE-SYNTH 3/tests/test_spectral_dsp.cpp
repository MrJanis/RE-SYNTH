#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../Source/SpectralDSP.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
float rms(const std::vector<float>& v,int a,int b) {
    double energy=0.;
    for(int i=a;i<b;++i)energy+=v[static_cast<size_t>(i)]*v[static_cast<size_t>(i)];
    return static_cast<float>(std::sqrt(energy/(b-a)));
}
}
int main() {
    using namespace rs::dsp;
    SpectralData m;
    m.count=3; m.frameCount=2;m.times[0]=0.f;m.times[1]=0.5f;
    m.partials[0].ratio=1.f;m.partials[0].levels[0]=0.8f;m.partials[0].levels[1]=0.2f;
    m.partials[1].ratio=2.f;m.partials[1].levels[0]=0.3f;m.partials[1].levels[1]=0.f;
    m.partials[2].ratio=2.21f;m.partials[2].levels[0]=0.15f;m.partials[2].levels[1]=0.15f;
    m.partials[2].ratios[1]=2.25f;
    auto mid=frameBlend(m,0.25);
    assert(mid.a==0 && mid.b==1 && std::abs(mid.fraction-0.5f)<1e-6f);
    assert(std::abs(lerpRatio(m.partials[2],mid)-2.23f)<0.0001);
    buildSpectralEnvelope(m,440.);
    const auto env=frameBlend(m,0.25);
    assert(envelopeAt(m,env,440.)>0.1f);
    AdditiveRenderer unlocked, locked;
    unlocked.prepare(44100.);locked.prepare(44100.);
    unlocked.setOriginalRoot(440.);locked.setOriginalRoot(440.);
    unlocked.setTimbreLock(0.f);locked.setTimbreLock(1.f);
    unlocked.reset(m,123);locked.reset(m,123);
    // Envelope lock must leave original-pitch reconstruction invariant.
    for(int i=0;i<3000;++i) {
        float a=unlocked.renderSample(m,440.,i/44100.);
        float b=locked.renderSample(m,440.,i/44100.);
        assert(std::abs(a-b)<0.0001f);
    }
    const auto note=renderTestTone(m,440.,44100.,44100);
    assert(rms(note,1000,4000)>rms(note,30000,35000)*1.9f);
    assert(std::all_of(note.begin(),note.end(),[](float x){return std::isfinite(x);}));
    // The polynomial oscillator must agree with sine, including quadrant boundaries.
    for(int i=0;i<=10000;++i) {
        const double phase=i/10000.;
        assert(std::abs(fastSinCycles(phase)-std::sin(tau*phase))<1.e-6);
    }
    SpectralData sine; sine.count=1;sine.frameCount=1;sine.partials[0].ratio=1.f;sine.partials[0].levels[0]=1.f;
    auto wave=renderTestTone(sine,440.,44100.,44100);
    for(int i=0;i<100;++i)assert(std::abs(wave[static_cast<size_t>(i)]-std::sin(tau*440*i/44100))<1e-5);
    SpectralData high; high.count=1;high.frameCount=1;high.partials[0].ratio=8.f;high.partials[0].levels[0]=1.f;
    assert(rms(renderTestTone(high,6000,44100,1024),0,1024)<1e-7f); // Nyquist rejection
    high.partials[0].ratio=2.321f;
    assert(rms(renderTestTone(high,440.,44100,1024),0,1024)>0.5f); // Noninteger partial
    SpectralData noise;
    noise.frameCount=2;noise.times[1]=0.5f;
    noise.noise[3].levels[0]=0.15f;noise.noise[3].levels[1]=0.01f;
    auto residual=renderTestTone(noise,440.,44100,44100);
    assert(rms(residual,1000,4000)>rms(residual,30000,35000)*3.f);
    assert(std::all_of(residual.begin(),residual.end(),[](float x){return std::isfinite(x);}));
    std::cout << "Spectral DSP passed: phase continuity, evolving levels/ratios, inharmonic tracking, Nyquist, colored noise.\n";
}
