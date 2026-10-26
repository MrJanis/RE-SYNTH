#ifdef NDEBUG
#undef NDEBUG // Keep regression assertions active in Release builds.
#endif
#include "../Source/PitchDetector.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    constexpr double tau=6.2831853071795864769;
    constexpr int sr=44100;
    for(double pitch : {55.,110.,261.625565,440.,880.,1320.}) {
        std::vector<float> note(44100);
        for(int i=0;i<sr;++i) {
            const double t=i/static_cast<double>(sr);
            const double decay=std::exp(-0.35*t);
            // Rich harmonic and time-varying note, non-integer number of cycles.
            note[static_cast<size_t>(i)]=static_cast<float>(decay*(0.8*std::sin(tau*pitch*t)+0.2*std::sin(tau*2*pitch*t)));
        }
        const double detected=rs::pitch::estimatePitch(note,sr);
        std::cout << pitch << " -> " << detected << " Hz\n";
        assert(std::abs(12.*std::log2(detected/pitch))<0.05);
    }
    std::vector<float> silence(sr,0.f);
    assert(rs::pitch::estimatePitch(silence,sr)==0.);
    std::cout << "YIN-style pitch detector tests passed.\n";
}
