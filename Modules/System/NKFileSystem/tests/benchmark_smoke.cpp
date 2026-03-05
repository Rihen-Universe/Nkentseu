#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKFileSystem/NkPath.h"

#include <chrono>
#include <cstdio>

using namespace nkentseu::entseu;

TEST_CASE(NKFileSystemBenchmark, PathCombineLoop) {
    constexpr int kIters = 300000;

    volatile std::size_t sink = 0;
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIters; ++i) {
        NkPath p = NkPath::Combine("root", "file.txt");
        sink += p.ToString().Length();
    }
    const auto t1 = std::chrono::high_resolution_clock::now();

    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    ASSERT_TRUE(ns > 0.0);
    std::printf("\n[NKFileSystem Benchmark] NkPath::Combine loop: %.2f ns total (sink=%zu)\n", ns, static_cast<std::size_t>(sink));
}
