#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKTime/NkChrono.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu;

static double NkToNs(const timespec& ts) {
    return (static_cast<double>(ts.tv_sec) * 1000000000.0) + static_cast<double>(ts.tv_nsec);
}

TEST_CASE(NKTimeBenchmark, ClockNowCallCost) {
    constexpr int kIters = 500000;

    int64 sink = 0;
    timespec t0 {};
    timespec t1 {};
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < kIters; ++i) {
        sink ^= static_cast<int64>(NkChrono::Now().ToNanoseconds());
    }
    timespec_get(&t1, TIME_UTC);

    const double ns = NkToNs(t1) - NkToNs(t0);
    ASSERT_TRUE(ns > 0.0);
    ASSERT_TRUE(sink != 0);
    NK_FOUNDATION_LOG_INFO("[NKTime Benchmark] NkChrono::Now loop: %.2f ns total (sink=%lld)",
                           ns, static_cast<long long>(sink));
}
