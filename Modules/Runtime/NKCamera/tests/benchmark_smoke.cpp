#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCamera/NkCameraTypes.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu;

TEST_CASE(NKCameraBenchmark, PixelFormatToStringLoop) {
    constexpr int kIters = 1000000;

    volatile std::size_t sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        sink += std::char_traits<char>::length(
            NkCameraPixelFormatToString(NkPixelFormat::NK_PIXEL_RGBA8)
        );
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKCamera Benchmark] pixel-format stringify: %.2f ns total (sink=%zu)\n", ns, static_cast<std::size_t>(sink));
}
