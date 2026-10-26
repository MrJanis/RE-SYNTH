#pragma once
// RE-SYNTH: portable OFFLINE analysis/quality measures. No GUI/DAW dependencies.
// Measurements are diagnostic, not a psychoacoustic proof of indistinguishability.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace rs::qa {
struct Report {
    float gain = 1.f;          // gain applied to synthesized audio to RMS-match reference
    double originalRms = 0.0;
    double synthesizedRms = 0.0;
    double spectralErrorDb = 0.0; // RMS log-magnitude deviation (dB, lower = closer)
    double waveformErrorDb = 0.0; // RMS error relative to RMS reference (dB)
    int frames = 0;
};
inline double rms(const std::vector<float>& x) {
    if(x.empty()) return 0.0;
    double sum = 0.0;
    for(float v:x) sum += static_cast<double>(v)*v;
    return std::sqrt(sum/x.size());
}
inline void fft(std::vector<std::complex<double>>& a) {
    const int n=static_cast<int>(a.size());
    for(int i=1,j=0;i<n;++i){
        int bit=n>>1;
        while(j&bit){j^=bit;bit>>=1;}
        j^=bit;
        if(i<j) std::swap(a[i],a[j]);
    }
    for(int len=2;len<=n;len*=2){
        const double angle=-6.2831853071795864769/len;
        const std::complex<double> wlen(std::cos(angle),std::sin(angle));
        for(int i=0;i<n;i+=len){
            std::complex<double> w(1.,0.);
            for(int j=0;j<len/2;++j){
                const auto u=a[i+j],v=a[i+j+len/2]*w;
                a[i+j]=u+v;a[i+j+len/2]=u-v;w*=wlen;
            }
        }
    }
}
inline Report evaluate(const std::vector<float>& original,const std::vector<float>& synthesized,
                       double sampleRate) {
    Report result;
    const size_t n=std::min(original.size(),synthesized.size());
    if(n<64 || sampleRate<=0.) return result;
    result.originalRms=rms(original);
    result.synthesizedRms=rms(synthesized);
    result.gain=static_cast<float>(std::clamp(result.originalRms/std::max(1.e-12,result.synthesizedRms),0.01,100.0));
    double err=0.;
    for(size_t i=0;i<n;++i){
        const double v=original[i]-synthesized[i]*result.gain;
        err+=v*v;
    }
    result.waveformErrorDb=20.0*std::log10(std::max(1.e-9,std::sqrt(err/n))/std::max(1.e-9,result.originalRms));
    // STFT log-spectral difference, Hann 1024/256. Omit source bins below -70 dB.
    constexpr int fftSize=1024,hop=256;
    if(n<fftSize) return result;
    std::vector<std::complex<double>> a(fftSize),b(fftSize);
    double errorSum=0.;int bins=0;
    for(size_t start=0;start+fftSize<=n;start+=hop){
        double localMax=0.;
        for(int j=0;j<fftSize;++j){
            const double w=0.5-0.5*std::cos(6.2831853071795864769*j/(fftSize-1));
            a[j]=original[start+j]*w;
            b[j]=synthesized[start+j]*result.gain*w;
        }
        fft(a);fft(b);
        for(int k=2;k<fftSize/2;++k) localMax=std::max(localMax,std::abs(a[k]));
        const double floor=std::max(1.e-9,localMax*0.00031622777);
        for(int k=2;k<fftSize/2;++k){
            const double freq=sampleRate*k/fftSize;
            if(freq<60.0 || freq>std::min(16000.0,sampleRate*0.46))continue;
            const double ma=std::abs(a[k]);
            if(ma<floor) continue;
            const double mb=std::abs(b[k]);
            const double db=20.0*std::log10(std::max(mb,floor)/std::max(ma,floor));
            errorSum+=db*db; ++bins;
        }
        ++result.frames;
    }
    result.spectralErrorDb=bins?std::sqrt(errorSum/bins):0.0;
    return result;
}
} // namespace rs::qa
