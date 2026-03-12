#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKRenderer/Deprecate/NkRenderer.h"
#include "NKLogger/NkLog.h"

#if defined(True)
#undef True
#endif
#if defined(False)
#undef False
#endif

#include <ctime>

using namespace nkentseu;

TEST_CASE(NKRendererBenchmark, PackColorLoop) {
    constexpr int kIters = 1000000;

    volatile uint32 sink = 0;
    const clock_t t0 = std::clock();
    for (int i = 0; i < kIters; ++i) {
        sink ^= NkRenderer::PackColor(
            static_cast<uint8>(i),
            static_cast<uint8>(i >> 1),
            static_cast<uint8>(i >> 2),
            255
        );
    }
    const clock_t t1 = std::clock();
    const double ns =
        (static_cast<double>(t1 - t0) * 1000000000.0) /
        static_cast<double>(CLOCKS_PER_SEC);
    ASSERT_TRUE(ns > 0.0);
    logger.Info("[NKRenderer Benchmark] PackColor loop: {0} ns total (sink={1})",
                ns, static_cast<unsigned int>(sink));
}
