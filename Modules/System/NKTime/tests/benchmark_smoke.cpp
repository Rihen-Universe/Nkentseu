#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKTime/NkClock.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu;

TEST_CASE(NKTimeBenchmark, ClockNowCallCost) {
    constexpr int kIters = 500000;

    volatile core::int64 sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        sink ^= NkClock::Now().Nanoseconds;
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKTime Benchmark] NkClock::Now loop: %.2f ns total (sink=%lld)\n", ns, static_cast<long long>(sink));
}
