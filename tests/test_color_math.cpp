#ifdef NDEBUG
#undef NDEBUG // Keep regression assertions active in Release builds.
#endif
#include "../Source/ColorMath.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace rs::color;
    constexpr float lsb = 1.0f / 2048.0f;
    assert(quantize12(0.0f) == 0.0f);
    assert(quantize12(-1.0f) == -1.0f);
    assert(quantize12(1.0f) == 1.0f - lsb);
    assert(quantize12(100.0f) == 1.0f - lsb);
    assert(quantize12(-100.0f) == -1.0f);
    assert(quantize12(std::nanf("")) == 0.0f);
    for (int k=-2000; k<=2000; k+=37) {
        const auto x = static_cast<float>(k) / 2300.0f;
        assert(std::abs(quantize12(x) - x) <= 0.5f * lsb + 1.0e-6f);
        assert(std::abs(quantize12(x) + quantize12(-x)) < 1.0e-6f);
    }
    auto checkHold = [](int hostRate, double targetRate) {
        FractionalHold hold;
        int changes=0;
        float previous=-1.f;
        for (int n=0; n<hostRate; ++n) {
            float left = static_cast<float>(n+1), right=-left;
            hold.tick(left, right, hostRate, targetRate);
            if (n == 0 || left != previous) ++changes;
            previous=left;
            assert(left == -right);
        }
        assert(std::abs(changes - targetRate) <= 2.0);
    };
    checkHold(48000, 26040.0);
    checkHold(96000, 40000.0);
    assert(asymmetricSaturation(0.f, 2.0f) == 0.f);
    for (float x : {-1.0f,-0.4f,0.0f,0.4f,1.0f}) {
        assert(std::isfinite(symmetricSaturation(x, 2.f)));
        assert(std::isfinite(asymmetricSaturation(x, 2.f)));
    }
    std::cout << "ColorMath tests passed: 12-bit quantization, fractional clock, saturation.\n";
}
