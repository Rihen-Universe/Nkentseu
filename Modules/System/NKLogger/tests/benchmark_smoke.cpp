#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkLogMessage.h"

#include <cstdio>
#include <ctime>

using namespace nkentseu;

// namespace {
// double NkToNs(const timespec& ts) {
//     return static_cast<double>(ts.tv_sec) * 1000000000.0 + static_cast<double>(ts.tv_nsec);
// }
// } // namespace

// TEST_CASE(NKLoggerBenchmark, FormatterHotPath) {
//     constexpr int kIters = 200000;

//     NkFormatter formatter("[%Y-%m-%d %H:%M:%S.%e] [%L] %n: %v");
//     NkLogMessage msg(NkLogLevel::NK_INFO, "benchmark-message", "bench.cpp", 12, "Run", "bench");

//     volatile usize sink = 0;
//     timespec t0{};
//     timespec t1{};
//     (void)::timespec_get(&t0, TIME_UTC);
//     for (int i = 0; i < kIters; ++i) {
//         sink += formatter.Format(msg).Size();
//     }
//     (void)::timespec_get(&t1, TIME_UTC);

//     const double ns = NkToNs(t1) - NkToNs(t0);
//     ASSERT_TRUE(ns > 0.0);
//     ::printf("\n[NKLogger Benchmark] format loop: %.2f ns total (sink=%zu)\n", ns, static_cast<size_t>(sink));
// }
