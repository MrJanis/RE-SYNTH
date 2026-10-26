#pragma once
#include <algorithm>
#include <cmath>

// Small portable DSP building blocks, separately testable without JUCE.
namespace rs::color {
inline float finiteOrZero(float x) noexcept { return std::isfinite(x) ? x : 0.0f; }

inline float quantize12(float x) noexcept {
    // Signed 12-bit uniform PCM: -2048 .. +2047, normalized to [-1, 1).
    const float clamped = std::clamp(finiteOrZero(x), -1.0f, 2047.0f / 2048.0f);
    return std::round(clamped * 2048.0f) / 2048.0f;
}

inline float symmetricSaturation(float x, float gain) noexcept {
    return std::tanh(x * gain) / std::max(1.0e-3f, std::tanh(gain));
}

inline float asymmetricSaturation(float x, float gain) noexcept {
    constexpr float bias = 0.13f;
    return (std::tanh(x * gain + bias) - std::tanh(bias)) /
           std::max(1.0e-3f, std::tanh(gain));
}

struct FractionalHold {
    double phase = 0.0;
    float heldL = 0.0f, heldR = 0.0f;
    bool primed = false;
    void reset() noexcept { phase = 0.0; heldL = heldR = 0.0f; primed = false; }

    void tick(float& left, float& right, double originalRate, double targetRate) noexcept {
        // Zero-order hold, with fractional-rate stepping (not bandlimited).
        if (targetRate >= originalRate) { heldL = left; heldR = right; primed = true; return; }
        phase += targetRate / originalRate;
        if (!primed || phase >= 1.0) {
            phase -= std::floor(phase);
            heldL = left;
            heldR = right;
            primed = true;
        }
        left = heldL;
        right = heldR;
    }
};
} // namespace rs::color
