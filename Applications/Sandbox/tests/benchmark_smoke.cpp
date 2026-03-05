#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include <chrono>
#include <cstdio>

TEST_CASE(SandboxBenchmark, ColorPackingLoop) {
    constexpr int kIters = 1000000;

    volatile unsigned int sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        const unsigned int a = static_cast<unsigned int>(i);
        sink ^= ((a & 0xFFu) << 24) | (((a * 3u) & 0xFFu) << 16) | (((a * 7u) & 0xFFu) << 8) | 0xFFu;
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[Sandbox Benchmark] color packing: %.2f ns total (sink=%u)\n", ns, static_cast<unsigned int>(sink));
}
