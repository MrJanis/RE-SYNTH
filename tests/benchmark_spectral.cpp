#include "../Source/SpectralDSP.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
int main() {
    rs::dsp::SpectralData m;
    m.count=128;m.frameCount=32;
    for(int f=0;f<m.frameCount;++f)m.times[f]=f*0.2f;
    for(int p=0;p<m.count;++p) {
        m.partials[p].ratio=p<96?static_cast<float>(p+1):static_cast<float>(p+1)+0.032f;
        for(int f=0;f<m.frameCount;++f)m.partials[p].levels[f]=0.8f/(p+1)*std::exp(-0.1f*f);
    }
    auto start=std::chrono::steady_clock::now();
    double checksum=0;
    for(int v=0;v<16;++v) {
        const auto samples=rs::dsp::renderTestTone(m,220.*std::pow(2.,v/12.),44100.,44100);
        for(float x:samples) checksum+=x;
    }
    const auto end=std::chrono::steady_clock::now();
    std::cout << "No sound-wavetable cache; additive oscillator state: " << sizeof(rs::dsp::AdditiveRenderer) << " bytes per voice\n";
    std::cout << "16 voices x 1s @ 44.1 kHz: " << std::chrono::duration<double,std::milli>(end-start).count() << " ms\n";
    std::cout << "Checksum: " << std::setprecision(10) << checksum << "\n";
}
