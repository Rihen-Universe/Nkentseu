#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkLog.h"

#include <ctime>

TEST_CASE(SandboxBenchmark, ColorPackingLoop) {
    constexpr int kIters = 1000000;

    volatile unsigned int sink = 0;
    const clock_t t0 = std::clock();
    for (int i = 0; i < kIters; ++i) {
        const unsigned int a = static_cast<unsigned int>(i);
        sink ^= ((a & 0xFFu) << 24) | (((a * 3u) & 0xFFu) << 16) | (((a * 7u) & 0xFFu) << 8) | 0xFFu;
    }
    const clock_t t1 = std::clock();

    const double ns =
        (static_cast<double>(t1 - t0) * 1000000000.0) /
        static_cast<double>(CLOCKS_PER_SEC);
    ASSERT_TRUE(ns > 0.0);
    logger.Info("[Sandbox Benchmark] color packing: {0} ns total (sink={1})",
                ns, static_cast<unsigned int>(sink));
}
