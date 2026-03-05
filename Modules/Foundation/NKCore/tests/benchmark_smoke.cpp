#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/NkOptional.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu::core;

TEST_CASE(NKCoreBenchmark, OptionalToggle) {
    constexpr int kIters = 1000000;

    NkOptional<int> value;
    volatile int sink = 0;

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        value.Emplace(i);
        sink += value.Value();
        value.Reset();
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);

    std::printf("\n[NKCore Benchmark] NkOptional toggle: %.2f ns total (sink=%d)\n", ns, sink);
}
