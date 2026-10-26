#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../Source/ReconstructionQA.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    constexpr int sr=44100,n=22050;
    std::vector<float> a(n),b(n),c(n);
    for(int i=0;i<n;++i) {
        a[i]=0.3f*std::sin(6.283185307179586*220*i/sr);
        b[i]=a[i]*0.25f;
        c[i]=0.3f*std::sin(6.283185307179586*330*i/sr);
    }
    auto exact=rs::qa::evaluate(a,a,sr);
    auto adjusted=rs::qa::evaluate(a,b,sr);
    auto wrong=rs::qa::evaluate(a,c,sr);
    assert(exact.spectralErrorDb<0.0001);
    assert(adjusted.spectralErrorDb<0.001);
    assert(std::abs(adjusted.gain-4.f)<0.0001f);
    assert(wrong.spectralErrorDb>5.0);
    assert(wrong.waveformErrorDb>0);
    std::cout<<"A/B QA metrics passed; wrong-pitch spectral error " <<wrong.spectralErrorDb<<" dB\n";
}
