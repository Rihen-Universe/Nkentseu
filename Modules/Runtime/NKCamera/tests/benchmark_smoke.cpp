#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCamera/NkCameraTypes.h"
#include "NKLogger/NkLog.h"

#include <ctime>

using namespace nkentseu;

TEST_CASE(NKCameraBenchmark, PixelFormatToStringLoop) {
    constexpr int kIters = 1000000;

    volatile std::size_t sink = 0;
    const clock_t t0 = std::clock();
    for (int i = 0; i < kIters; ++i) {
        sink += std::char_traits<char>::length(
            NkCameraPixelFormatToString(NkPixelFormat::NK_PIXEL_RGBA8)
        );
    }
    const clock_t t1 = std::clock();
    const double ns =
        (static_cast<double>(t1 - t0) * 1000000000.0) /
        static_cast<double>(CLOCKS_PER_SEC);
    ASSERT_TRUE(ns > 0.0);
    logger.Info("[NKCamera Benchmark] pixel-format stringify: {0} ns total (sink={1})",
                ns, static_cast<std::size_t>(sink));
}
