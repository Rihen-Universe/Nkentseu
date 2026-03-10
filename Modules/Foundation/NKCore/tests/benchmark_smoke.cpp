#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/NkOptional.h"
#include "NKCore/NkAtomic.h"
#include "NKPlatform/NkFoundationLog.h"

#include <cstdio>
#include <optional>
#include <atomic>

using namespace nkentseu;

TEST_CASE(NKCoreBenchmark, OptionalToggleVsStdOptional) {
    constexpr int kIters = 60000;

    NkOptional<int> nkValue;
    std::optional<int> stdValue;
    volatile int nkSink = 0;
    volatile int stdSink = 0;

    nkentseu::benchmark::BenchmarkOptions options;
    options.mIterations = static_cast<size_t>(kIters);
    options.mWarmup = 1000u;
    options.mStableMode = true;

    auto nkResult = RUN_BENCHMARK_WITH_OPTIONS("NKCore.NkOptional.Toggle", [&]() {
        nkValue.Emplace(42);
        nkSink = nkSink + nkValue.Value();
        nkValue.Reset();
    }, options);

    auto stdResult = RUN_BENCHMARK_WITH_OPTIONS("NKCore.std::optional.Toggle", [&]() {
        stdValue.emplace(42);
        stdSink = stdSink + *stdValue;
        stdValue.reset();
    }, options);

    ASSERT_TRUE(nkResult.mMeanTimeMs > 0.0);
    ASSERT_TRUE(stdResult.mMeanTimeMs > 0.0);
    ASSERT_EQUAL(static_cast<int>(nkSink), static_cast<int>(stdSink));

    const double ratio = nkResult.mMeanTimeMs / stdResult.mMeanTimeMs;
    ASSERT_TRUE(ratio > 0.0);
    ASSERT_TRUE(ratio < 12.0);

    if (auto reporter = nkentseu::test::TestRunner::GetInstance().GetPerformanceReporter()) {
        reporter->OnBenchmarkComplete(nkResult);
        reporter->OnBenchmarkComplete(stdResult);
    }

    NK_FOUNDATION_LOG_INFO("[NKCore Benchmark] Optional Toggle NK=%.6f ms STL=%.6f ms ratio=%.3f",
                           nkResult.mMeanTimeMs, stdResult.mMeanTimeMs, ratio);
}

TEST_CASE(NKCoreBenchmark, AtomicFetchAddNkVsStdAtomic) {
    constexpr int kIters = 80000;

    NkAtomicInt32 nkCounter(0);
    std::atomic<int> stdCounter(0);
    volatile int nkSink = 0;
    volatile int stdSink = 0;

    nkentseu::benchmark::BenchmarkOptions options;
    options.mIterations = static_cast<size_t>(kIters);
    options.mWarmup = 1000u;
    options.mStableMode = true;

    auto nkResult = RUN_BENCHMARK_WITH_OPTIONS("NKCore.NkAtomic.FetchAdd", [&]() {
        nkSink = nkSink + static_cast<int>(nkCounter.FetchAdd(1));
    }, options);

    auto stdResult = RUN_BENCHMARK_WITH_OPTIONS("NKCore.std::atomic.fetch_add", [&]() {
        stdSink = stdSink + stdCounter.fetch_add(1, std::memory_order_seq_cst);
    }, options);

    ASSERT_TRUE(nkResult.mMeanTimeMs > 0.0);
    ASSERT_TRUE(stdResult.mMeanTimeMs > 0.0);

    const double ratio = nkResult.mMeanTimeMs / stdResult.mMeanTimeMs;
    ASSERT_TRUE(ratio > 0.0);
    ASSERT_TRUE(ratio < 16.0);

    if (auto reporter = nkentseu::test::TestRunner::GetInstance().GetPerformanceReporter()) {
        reporter->OnBenchmarkComplete(nkResult);
        reporter->OnBenchmarkComplete(stdResult);
    }

    NK_FOUNDATION_LOG_INFO("[NKCore Benchmark] Atomic FetchAdd NK=%.6f ms STL=%.6f ms ratio=%.3f",
                           nkResult.mMeanTimeMs, stdResult.mMeanTimeMs, ratio);
}
