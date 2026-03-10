#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKReflection/NkType.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu::reflection;

namespace {
double NkToNs(const timespec& ts) {
    return static_cast<double>(ts.tv_sec) * 1000000000.0 + static_cast<double>(ts.tv_nsec);
}
} // namespace

TEST_CASE(NKReflectionBenchmark, TypeOfLookupLoop) {
    constexpr int kIters = 1000000;

    volatile nkentseu::nk_size sink = 0;
    timespec t0{};
    timespec t1{};
    (void)::timespec_get(&t0, TIME_UTC);
    for (int i = 0; i < kIters; ++i) {
        sink += NkTypeOf<nkentseu::nk_uint32>().GetSize();
    }
    (void)::timespec_get(&t1, TIME_UTC);

    const double ns = NkToNs(t1) - NkToNs(t0);
    ASSERT_TRUE(ns > 0.0);
    NK_FOUNDATION_LOG_INFO("[NKReflection Benchmark] NkTypeOf loop: %.2f ns total (sink=%zu)", ns, static_cast<size_t>(sink));
}
