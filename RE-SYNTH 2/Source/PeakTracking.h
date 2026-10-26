#pragma once
// JUCE-independent implementation of nearest-frequency, birth/death
// McAulay-Quatieri-style peak matching. Offline-only; allocations are allowed.
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace rs::tracking {
struct Peak {
    float frequency=0.f, amplitude=0.f, phase=0.f;
};
struct Observation {
    int frame=0;
    Peak peak;
};
struct Track {
    std::vector<Observation> events;
    int lastFrame=-1;
    double score=0.;
    float amplitudeAt(int frame) const noexcept {
        if(events.empty() || frame<events.front().frame || frame>events.back().frame) return 0.f;
        const auto it=std::lower_bound(events.begin(),events.end(),frame,[](const Observation& o,int f){return o.frame<f;});
        if(it==events.end()) return events.back().peak.amplitude;
        if(it->frame==frame || it==events.begin())return it->peak.amplitude;
        const auto& previous=*(it-1);
        const float fraction=static_cast<float>(frame-previous.frame)/(it->frame-previous.frame);
        return previous.peak.amplitude+(it->peak.amplitude-previous.peak.amplitude)*fraction;
    }
    float frequencyAt(int frame) const noexcept {
        if(events.empty()) return 0.f;
        const auto it=std::lower_bound(events.begin(),events.end(),frame,[](const Observation& o,int f){return o.frame<f;});
        if(it==events.end())return events.back().peak.frequency;
        if(it==events.begin())return it->peak.frequency;
        const auto& previous=*(it-1);
        const float fraction=static_cast<float>(frame-previous.frame)/(it->frame-previous.frame);
        return previous.peak.frequency+(it->peak.frequency-previous.peak.frequency)*fraction;
    }
};
inline std::vector<Track> linkPeaks(const std::vector<std::vector<Peak>>& frames) {
    std::vector<Track> tracks;
    std::vector<int> active;
    tracks.reserve(1024);
    for(int frame=0;frame<static_cast<int>(frames.size());++frame) {
        // Prune dead tracks from the searchable list (but preserve full history).
        active.erase(std::remove_if(active.begin(),active.end(),[&](int t){return frame-tracks[static_cast<size_t>(t)].lastFrame>2;}),active.end());
        for(const auto& peak:frames[static_cast<size_t>(frame)]) {
            if(peak.amplitude<=0 || peak.frequency<=0) continue;
            int best=-1;
            float distance=1.e12f;
            for(int idx:active) {
                auto& t=tracks[static_cast<size_t>(idx)];
                if(t.lastFrame==frame)continue;
                const float reference=t.events.back().peak.frequency;
                const float tolerance=std::max(18.f,reference*0.045f);
                const float delta=std::abs(reference-peak.frequency);
                if(delta<tolerance && delta<distance) {best=idx;distance=delta;}
            }
            if(best<0) {
                if(tracks.size()>=8192)continue; // runaway noisy recording safeguard.
                tracks.emplace_back();
                best=static_cast<int>(tracks.size()-1);
                active.push_back(best);
            }
            auto& matched=tracks[static_cast<size_t>(best)];
            matched.events.push_back({frame,peak});
            matched.lastFrame=frame;
            matched.score+=static_cast<double>(peak.amplitude)*peak.amplitude;
        }
    }
    // Events in a track are guaranteed ordered in time. Reject accidental
    // one-frame peaks to reduce false sinusoidal tracks from broadband noise.
    tracks.erase(std::remove_if(tracks.begin(),tracks.end(),[](const Track& t){return t.events.size()<3;}),tracks.end());
    std::stable_sort(tracks.begin(),tracks.end(),[](const Track& a,const Track& b){return a.score>b.score;});
    return tracks;
}
} // namespace rs::tracking
