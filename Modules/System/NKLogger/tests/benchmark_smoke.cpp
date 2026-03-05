#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkLogMessage.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu;

TEST_CASE(NKLoggerBenchmark, FormatterHotPath) {
    constexpr int kIters = 200000;

    NkFormatter formatter("[%Y-%m-%d %H:%M:%S.%e] [%L] %n: %v");
    NkLogMessage msg(NkLogLevel::NK_INFO, "benchmark-message", "bench.cpp", 12, "Run", "bench");

    volatile std::size_t sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        sink += formatter.Format(msg).size();
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKLogger Benchmark] format loop: %.2f ns total (sink=%zu)\n", ns, static_cast<std::size_t>(sink));
}
