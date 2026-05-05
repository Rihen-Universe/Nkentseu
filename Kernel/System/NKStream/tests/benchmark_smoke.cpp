#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKStream/NkBinaryStream.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu;

static double NkToNs(const timespec& ts) {
    return (static_cast<double>(ts.tv_sec) * 1000000000.0) + static_cast<double>(ts.tv_nsec);
}

TEST_CASE(NKStreamBenchmark, BinaryStreamBulkWrite) {
    constexpr int kIters = 200000;

    NkBinaryStream stream(static_cast<usize>(kIters * static_cast<int>(sizeof(int)) + 64));
    timespec t0 {};
    timespec t1 {};
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < kIters; ++i) {
        stream.Write(&i, 1);
    }
    timespec_get(&t1, TIME_UTC);

    const double ns = NkToNs(t1) - NkToNs(t0);
    ASSERT_TRUE(ns > 0.0);
    NK_FOUNDATION_LOG_INFO("[NKStream Benchmark] NkBinaryStream write loop: %.2f ns total (size=%zu)",
                           ns, static_cast<size_t>(stream.Size()));
}
