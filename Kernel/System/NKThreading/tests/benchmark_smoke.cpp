#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKThreading/NkMutex.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu::threading;

static double NkToNs(const timespec& ts) {
    return (static_cast<double>(ts.tv_sec) * 1000000000.0) + static_cast<double>(ts.tv_nsec);
}

TEST_CASE(NKThreadingBenchmark, MutexLockUnlockLoop) {
    constexpr int kIters = 500000;

    NkMutex mutex;
    int sink = 0;

    timespec t0 {};
    timespec t1 {};
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < kIters; ++i) {
        mutex.Lock();
        sink += i;
        mutex.Unlock();
    }
    timespec_get(&t1, TIME_UTC);

    const double ns = NkToNs(t1) - NkToNs(t0);
    ASSERT_TRUE(ns > 0.0);
    ASSERT_TRUE(sink != 0);
    NK_FOUNDATION_LOG_INFO("[NKThreading Benchmark] mutex lock/unlock: %.2f ns total (sink=%d)", ns, sink);
}
