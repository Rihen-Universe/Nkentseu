#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKReflection/NkType.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu::core::reflection;

TEST_CASE(NKReflectionBenchmark, TypeOfLookupLoop) {
    constexpr int kIters = 1000000;

    volatile nkentseu::nk_size sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        sink += NkTypeOf<nkentseu::nk_uint32>().GetSize();
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKReflection Benchmark] NkTypeOf loop: %.2f ns total (sink=%zu)\n", ns, static_cast<std::size_t>(sink));
}
