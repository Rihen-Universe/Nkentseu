#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKWindow/Core/NkTypes.h"
#include "NKLogger/NkLog.h"

#include <ctime>

using namespace nkentseu;

TEST_CASE(NKWindowBenchmark, RendererApiToStringLoop) {
    constexpr int kIters = 1000000;

    volatile std::size_t sink = 0;
    const clock_t t0 = std::clock();
    for (int i = 0; i < kIters; ++i) {
        sink += std::char_traits<char>::length(
            NkGraphicsApiToString(static_cast<NkGraphicsApi>(i % static_cast<int>(NkGraphicsApi::NK_GFX_API_RENDERER_API_MAX)))
        );
    }
    const clock_t t1 = std::clock();
    const double ns =
        (static_cast<double>(t1 - t0) * 1000000000.0) /
        static_cast<double>(CLOCKS_PER_SEC);
    ASSERT_TRUE(ns > 0.0);
    logger.Info("[NKWindow Benchmark] renderer API stringify: {0} ns total (sink={1})",
                ns, static_cast<std::size_t>(sink));
}
