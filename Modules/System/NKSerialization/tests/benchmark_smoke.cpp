#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include <chrono>
#include <cstdio>
#include <string>

TEST_CASE(NKSerializationBenchmark, PlaceholderStringPipeline) {
    constexpr int kIters = 300000;

    std::string payload;
    payload.reserve(64);

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        payload.clear();
        payload += "{\"v\":";
        payload += std::to_string(i);
        payload += "}";
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKSerialization Benchmark] placeholder encode loop: %.2f ns total\n", ns);
}
