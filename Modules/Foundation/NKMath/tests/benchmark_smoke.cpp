#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <cmath>
#include <cstdio>

using namespace nkentseu::math;

TEST_CASE(NKMathBenchmark, TrigonometryLoopVsStd) {
    constexpr int kIters = 500000;

    volatile float nkSink = 0.0f;
    volatile float stdSink = 0.0f;

    const auto nkT0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        float x = static_cast<float>(i) * 0.001f;
        nkSink = nkSink + NkSin(x) + NkCos(x);
    }
    const auto nkT1 = std::chrono::high_resolution_clock::now();

    const auto stdT0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        float x = static_cast<float>(i) * 0.001f;
        stdSink = stdSink + static_cast<float>(std::sin(x) + std::cos(x));
    }
    const auto stdT1 = std::chrono::high_resolution_clock::now();

    const double nkNs = std::chrono::duration<double, std::nano>(nkT1 - nkT0).count();
    const double stdNs = std::chrono::duration<double, std::nano>(stdT1 - stdT0).count();
    ASSERT_TRUE(nkNs > 0.0);
    ASSERT_TRUE(stdNs > 0.0);

    const float delta = nkSink - stdSink;
    ASSERT_TRUE(delta < 1.0f && delta > -1.0f);

    NK_FOUNDATION_LOG_INFO("[NKMath Benchmark] NkMath vs std::sin/std::cos");
    NK_FOUNDATION_LOG_INFO("  NkMath : %.2f ns total (sink=%f)", nkNs, nkSink);
    NK_FOUNDATION_LOG_INFO("  STL    : %.2f ns total (sink=%f)", stdNs, stdSink);
}
