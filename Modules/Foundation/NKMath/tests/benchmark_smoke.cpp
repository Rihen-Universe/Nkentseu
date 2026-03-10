#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"
#include "NKPlatform/NkFoundationLog.h"

#include <ctime>
#include <cmath>
#include <cstdio>

using namespace nkentseu::math;

TEST_CASE(NKMathBenchmark, TrigonometryLoopVsStd) {
    constexpr int kIters = 500000;

    volatile float nkSink = 0.0f;
    volatile float stdSink = 0.0f;

    const clock_t nkT0 = std::clock();
    for (int i = 0; i < kIters; ++i) {
        float x = static_cast<float>(i) * 0.001f;
        nkSink = nkSink + NkSin(x) + NkCos(x);
    }
    const clock_t nkT1 = std::clock();

    const clock_t stdT0 = std::clock();
    for (int i = 0; i < kIters; ++i) {
        float x = static_cast<float>(i) * 0.001f;
        stdSink = stdSink + static_cast<float>(std::sin(x) + std::cos(x));
    }
    const clock_t stdT1 = std::clock();

    const double nkNs =
        (static_cast<double>(nkT1 - nkT0) * 1000000000.0) /
        static_cast<double>(CLOCKS_PER_SEC);
    const double stdNs =
        (static_cast<double>(stdT1 - stdT0) * 1000000000.0) /
        static_cast<double>(CLOCKS_PER_SEC);
    ASSERT_TRUE(nkNs > 0.0);
    ASSERT_TRUE(stdNs > 0.0);

    const float delta = nkSink - stdSink;
    ASSERT_TRUE(delta < 1.0f && delta > -1.0f);

    NK_FOUNDATION_LOG_INFO("[NKMath Benchmark] NkMath vs std::sin/std::cos");
    NK_FOUNDATION_LOG_INFO("  NkMath : %.2f ns total (sink=%f)", nkNs, nkSink);
    NK_FOUNDATION_LOG_INFO("  STL    : %.2f ns total (sink=%f)", stdNs, stdSink);
}
