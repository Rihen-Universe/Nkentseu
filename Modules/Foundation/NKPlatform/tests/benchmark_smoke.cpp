#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKPlatform/NkPlatformConfig.h"
#include "NKPlatform/NkCPUFeatures.h"
#include "NKPlatform/NkEndianness.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <cstdio>
#include <thread>

using namespace nkentseu::platform;

TEST_CASE(NKPlatformBenchmark, LogicalCoresNkVsStdThread) {
    volatile unsigned int sink = 0u;

    nkentseu::benchmark::BenchmarkOptions options;
    options.mIterations = 30000u;
    options.mWarmup = 500u;
    options.mStableMode = true;

    auto nkResult = RUN_BENCHMARK_WITH_OPTIONS("NKPlatform.NkGetLogicalCoreCount", [&]() {
        sink = sink + static_cast<unsigned int>(NkGetLogicalCoreCount());
    }, options);

    auto stdResult = RUN_BENCHMARK_WITH_OPTIONS("NKPlatform.std::thread::hardware_concurrency", [&]() {
        const unsigned int value = std::thread::hardware_concurrency();
        sink = sink + ((value == 0u) ? 1u : value);
    }, options);

    ASSERT_TRUE(nkResult.mMeanTimeMs > 0.0);
    ASSERT_TRUE(stdResult.mMeanTimeMs > 0.0);
    const double logicalRatio = nkResult.mMeanTimeMs / stdResult.mMeanTimeMs;
    ASSERT_TRUE(logicalRatio > 0.0);
    ASSERT_TRUE(logicalRatio < 8.0);

    if (auto reporter = nkentseu::test::TestRunner::GetInstance().GetPerformanceReporter()) {
        reporter->OnBenchmarkComplete(nkResult);
        reporter->OnBenchmarkComplete(stdResult);
    }

    NK_FOUNDATION_LOG_INFO("[NKPlatform Benchmark] NkGetLogicalCoreCount: %.6f ms | std::thread::hardware_concurrency: %.6f ms (sink=%u)",
                           nkResult.mMeanTimeMs, stdResult.mMeanTimeMs, sink);
}

TEST_CASE(NKPlatformBenchmark, ByteSwap32NkVsManual) {
    volatile uint32_t sink = 0u;

    auto manualByteSwap32 = [](uint32_t value) -> uint32_t {
        return ((value >> 24) & 0x000000FFu) |
               ((value >> 8)  & 0x0000FF00u) |
               ((value << 8)  & 0x00FF0000u) |
               ((value << 24) & 0xFF000000u);
    };

    nkentseu::benchmark::BenchmarkOptions options;
    options.mIterations = 60000u;
    options.mWarmup = 1000u;
    options.mStableMode = true;

    uint32_t value = 0x12345678u;
    auto nkResult = RUN_BENCHMARK_WITH_OPTIONS("NKPlatform.NkByteSwap32", [&]() {
        value = NkByteSwap32(value + 17u);
        sink = sink ^ value;
    }, options);

    value = 0x12345678u;
    auto manualResult = RUN_BENCHMARK_WITH_OPTIONS("NKPlatform.ManualByteSwap32", [&]() {
        value = manualByteSwap32(value + 17u);
        sink = sink ^ value;
    }, options);

    ASSERT_TRUE(nkResult.mMeanTimeMs > 0.0);
    ASSERT_TRUE(manualResult.mMeanTimeMs > 0.0);
    const double byteswapRatio = nkResult.mMeanTimeMs / manualResult.mMeanTimeMs;
    ASSERT_TRUE(byteswapRatio > 0.0);
    ASSERT_TRUE(byteswapRatio < 5.0);

    if (auto reporter = nkentseu::test::TestRunner::GetInstance().GetPerformanceReporter()) {
        reporter->OnBenchmarkComplete(nkResult);
        reporter->OnBenchmarkComplete(manualResult);
    }

    NK_FOUNDATION_LOG_INFO("[NKPlatform Benchmark] NkByteSwap32: %.6f ms | Manual: %.6f ms (sink=%u)",
                           nkResult.mMeanTimeMs, manualResult.mMeanTimeMs, sink);
}
