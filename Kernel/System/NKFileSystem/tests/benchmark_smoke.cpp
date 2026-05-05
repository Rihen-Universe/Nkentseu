#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKFileSystem/NkPath.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu::entseu;

static double NkToNs(const timespec& ts) {
    return (static_cast<double>(ts.tv_sec) * 1000000000.0) + static_cast<double>(ts.tv_nsec);
}

TEST_CASE(NKFileSystemBenchmark, PathCombineLoop) {
    constexpr int kIters = 300000;

    nkentseu::usize sink = 0;
    timespec t0 {};
    timespec t1 {};
    timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < kIters; ++i) {
        NkPath p = NkPath::Combine("root", "file.txt");
        sink += p.ToString().Length();
    }
    timespec_get(&t1, TIME_UTC);

    const double ns = NkToNs(t1) - NkToNs(t0);
    ASSERT_TRUE(ns > 0.0);
    ASSERT_TRUE(sink > 0);
    NK_FOUNDATION_LOG_INFO("[NKFileSystem Benchmark] NkPath::Combine loop: %.2f ns total (sink=%zu)",
                           ns, static_cast<size_t>(sink));
}
