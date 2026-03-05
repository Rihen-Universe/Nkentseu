#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformConfig.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu::platform;

TEST_CASE(NKPlatformBenchmark, ConfigReadHotPath) {
    constexpr unsigned int kIters = 1000000u;

    volatile unsigned int sink = 0u;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (unsigned int i = 0; i < kIters; ++i) {
        const PlatformConfig& cfg = GetPlatformConfig();
        sink += static_cast<unsigned int>(cfg.cacheLineSize + cfg.maxPathLength + (cfg.is64Bit ? 1 : 0));
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);

    std::printf("\n[NKPlatform Benchmark] config read loop: %.2f ns total (sink=%u)\n", ns, sink);
}
