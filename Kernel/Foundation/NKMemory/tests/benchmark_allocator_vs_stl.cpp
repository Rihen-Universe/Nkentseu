#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <new>
#include <stdio.h>

using namespace nkentseu::memory;
using namespace nkentseu;

namespace {

template <typename Fn>
double MeasureNs(Fn&& fn) {
    const auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

} // namespace

TEST_CASE(NKMemoryBenchmark, MallocAllocatorVsOperatorNewDelete) {
    constexpr int kIterations = 200000;
    constexpr nk_size kAllocSize = 64u;

    nk_uint64 nkChecksum = 0;
    int nkFailures = 0;
    const double nkTimeNs = MeasureNs([&]() {
        NkMallocAllocator allocator;
        for (int i = 0; i < kIterations; ++i) {
            void* p = allocator.Allocate(kAllocSize, 16u);
            if (!p) {
                ++nkFailures;
                continue;
            }
            NkSet(p, i & 0xFF, kAllocSize);
            nkChecksum += static_cast<nk_uint64>(static_cast<nk_uint8*>(p)[0]);
            allocator.Deallocate(p);
        }
    });

    nk_uint64 stlChecksum = 0;
    int stlFailures = 0;
    const double stlTimeNs = MeasureNs([&]() {
        for (int i = 0; i < kIterations; ++i) {
            void* p = ::operator new(static_cast<std::size_t>(kAllocSize), std::nothrow);
            if (!p) {
                ++stlFailures;
                continue;
            }
            NkSet(p, i & 0xFF, kAllocSize);
            stlChecksum += static_cast<nk_uint64>(static_cast<nk_uint8*>(p)[0]);
            ::operator delete(p);
        }
    });

    ASSERT_EQUAL(0, nkFailures);
    ASSERT_EQUAL(0, stlFailures);
    ASSERT_TRUE(nkChecksum == stlChecksum);
    ASSERT_TRUE(nkTimeNs > 0.0);
    ASSERT_TRUE(stlTimeNs > 0.0);

    NK_FOUNDATION_LOG_INFO("[NKMemory Benchmark] NkMallocAllocator vs operator new/delete");
    NK_FOUNDATION_LOG_INFO("  NkMallocAllocator  : %.2f ns total", nkTimeNs);
    NK_FOUNDATION_LOG_INFO("  operator new/delete: %.2f ns total", stlTimeNs);
}
