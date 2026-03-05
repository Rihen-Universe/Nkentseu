#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKThreading/NkMutex.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu::threading;

TEST_CASE(NKThreadingBenchmark, MutexLockUnlockLoop) {
    constexpr int kIters = 500000;

    NkMutex mutex;
    volatile int sink = 0;

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        mutex.Lock();
        sink += i;
        mutex.Unlock();
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKThreading Benchmark] mutex lock/unlock: %.2f ns total (sink=%d)\n", ns, sink);
}
