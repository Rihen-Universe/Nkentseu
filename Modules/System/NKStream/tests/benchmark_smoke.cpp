#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKStream/NkBinaryStream.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu;

TEST_CASE(NKStreamBenchmark, BinaryStreamBulkWrite) {
    constexpr int kIters = 200000;

    NkBinaryStream stream(static_cast<usize>(kIters * static_cast<int>(sizeof(int)) + 64));
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        stream.Write(&i, 1);
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKStream Benchmark] NkBinaryStream write loop: %.2f ns total (size=%zu)\n", ns, static_cast<std::size_t>(stream.Size()));
}
