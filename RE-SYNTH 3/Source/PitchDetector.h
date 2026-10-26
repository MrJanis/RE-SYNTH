#pragma once
// Portable YIN-style monophonic pitch detection. Offline analysis only.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace rs::pitch {
inline double estimateWindow(const std::vector<float>& audio, double sampleRate, size_t start) {
    if (start>=audio.size() || sampleRate<=0) return 0.;
    const int available=static_cast<int>(std::min<size_t>(8192,audio.size()-start));
    if (available<1024) return 0.;
    const int minLag=std::max(2,static_cast<int>(sampleRate/1500.));
    const int maxLag=std::min(available/2,static_cast<int>(sampleRate/45.));
    if (minLag>=maxLag) return 0.;
    // Fixed inner-window length across all lags makes the difference
    // normalization more stable on decaying recordings.
    const int compare=available-maxLag;
    std::vector<double> cmnd(static_cast<size_t>(maxLag+1),1.);
    double total=0.;
    for (int lag=1;lag<=maxLag;++lag) {
        double diff=0.;
        for (int i=0;i<compare;++i) {
            const double a=audio[start+static_cast<size_t>(i)];
            const double b=audio[start+static_cast<size_t>(i+lag)];
            const double delta=a-b;
            diff+=delta*delta;
        }
        total+=diff;
        cmnd[static_cast<size_t>(lag)]= total>1.e-14 ? diff*lag/total : 1.;
    }
    int bestLag=0;
    constexpr double threshold=0.16;
    // Choose the FIRST convincing valley, not a later octave/subharmonic lag.
    for(int lag=minLag;lag<maxLag-1;++lag) {
        if(cmnd[static_cast<size_t>(lag)]<threshold) {
            while(lag<maxLag-1 && cmnd[static_cast<size_t>(lag+1)]<cmnd[static_cast<size_t>(lag)]) ++lag;
            bestLag=lag; break;
        }
    }
    if(bestLag==0) {
        auto it=std::min_element(cmnd.begin()+minLag,cmnd.end());
        if(it==cmnd.end() || *it>0.25) return 0.;
        bestLag=static_cast<int>(it-cmnd.begin());
    }
    double lag=bestLag;
    if(bestLag>1 && bestLag<maxLag) {
        const double left=cmnd[static_cast<size_t>(bestLag-1)];
        const double center=cmnd[static_cast<size_t>(bestLag)];
        const double right=cmnd[static_cast<size_t>(bestLag+1)];
        const double curvature=left-2*center+right;
        if(std::abs(curvature)>1.e-12)
            lag += std::clamp(0.5*(left-right)/curvature,-0.5,0.5);
    }
    return lag>0 ? sampleRate/lag : 0.;
}
inline double estimatePitch(const std::vector<float>& audio, double sampleRate) {
    std::array<double,3> estimates {};
    int detected=0;
    for(double pos : {0.09,0.19,0.34}) {
        const size_t start=static_cast<size_t>(std::min<double>(audio.size()*pos,sampleRate*0.25));
        const double hz=estimateWindow(audio,sampleRate,start);
        if(hz>0) estimates[detected++]=hz;
    }
    if(detected<2) return 0.;
    // Sorting network for only three entries; avoids a general sort on a tiny array.
    if (estimates[0]>estimates[1]) std::swap(estimates[0],estimates[1]);
    if (detected==3) {
        if (estimates[1]>estimates[2]) std::swap(estimates[1],estimates[2]);
        if (estimates[0]>estimates[1]) std::swap(estimates[0],estimates[1]);
    }
    const double median=estimates[detected/2];
    int agreement=0;
    for(int i=0;i<detected;++i)
        if(std::abs(12.*std::log2(estimates[i]/median))<0.65) ++agreement;
    return agreement>=2 ? median : 0.;
}
} // namespace rs::pitch
