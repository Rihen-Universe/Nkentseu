#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu::math;

TEST_CASE(NKMathBenchmark, TrigonometryLoop) {
    constexpr int kIters = 500000;

    volatile float sink = 0.0f;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        float x = static_cast<float>(i) * 0.001f;
        sink += Sin(x) + Cos(x);
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);

    std::printf("\n[NKMath Benchmark] sin+cos loop: %.2f ns total (sink=%f)\n", ns, sink);
}
