#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKWindow/Core/NkTypes.h"
#include "NKLogger/NkLog.h"

#include <chrono>

using namespace nkentseu;

TEST_CASE(NKWindowBenchmark, RendererApiToStringLoop) {
    constexpr int kIters = 1000000;

    volatile std::size_t sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        sink += std::char_traits<char>::length(
            NkRendererApiToString(static_cast<NkRendererApi>(i % static_cast<int>(NkRendererApi::NK_RENDERER_API_MAX)))
        );
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    logger.Info("[NKWindow Benchmark] renderer API stringify: {0} ns total (sink={1})",
                ns, static_cast<std::size_t>(sink));
}
