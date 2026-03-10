#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKRenderer/NkRenderer.h"
#include "NKLogger/NkLog.h"

#if defined(True)
#undef True
#endif
#if defined(False)
#undef False
#endif

#include <chrono>

using namespace nkentseu;

TEST_CASE(NKRendererBenchmark, PackColorLoop) {
    constexpr int kIters = 1000000;

    volatile NkU32 sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        sink ^= NkRenderer::PackColor(
            static_cast<NkU8>(i),
            static_cast<NkU8>(i >> 1),
            static_cast<NkU8>(i >> 2),
            255
        );
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    logger.Info("[NKRenderer Benchmark] PackColor loop: {0} ns total (sink={1})",
                ns, static_cast<unsigned int>(sink));
}
