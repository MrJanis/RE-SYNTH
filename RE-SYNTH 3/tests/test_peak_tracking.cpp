#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../Source/PeakTracking.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
int main() {
    using namespace rs::tracking;
    std::vector<std::vector<Peak>> frames(10);
    for(int i=0;i<10;++i) {
        frames[static_cast<size_t>(i)].push_back({440.f+0.5f*i,0.5f,0.f});
        if(i>=3 && i<=8)frames[static_cast<size_t>(i)].push_back({555.5f+0.8f*i,0.23f,0.f});
        frames[static_cast<size_t>(i)].push_back({1000.f+130.f*i,0.01f,0.f});
    }
    auto tracks=linkPeaks(frames);
    assert(tracks.size()==2);
    assert(tracks[0].events.size()==10);
    assert(tracks[1].events.size()==6);
    assert(tracks[1].amplitudeAt(0)==0.f);
    assert(std::abs(tracks[0].frequencyAt(4)-442.f)<1.e-5f);
    assert(tracks[1].amplitudeAt(9)==0.f);
    std::cout<<"Peak tracks passed: drift, nonharmonic birth/death, noise rejection.\n";
}
