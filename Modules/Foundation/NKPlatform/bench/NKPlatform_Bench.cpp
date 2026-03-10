#include "NKPlatform/NkCPUFeatures.h"
#include "NKPlatform/NkEndianness.h"
#include "NKPlatform/NkPlatformConfig.h"

#include <chrono>
#include <cstdio>
#include "NKPlatform/NkFoundationLog.h"
#include <thread>

using namespace nkentseu::platform;

namespace {

template <typename Func>
double MeasureMeanNs(Func&& func, std::size_t iterations) {
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        func();
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double totalNs = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return totalNs / static_cast<double>(iterations);
}

} // namespace

int main() {
    constexpr std::size_t kIters = 1000000u;
    volatile unsigned int sink = 0u;

    const double nkLogicalNs = MeasureMeanNs([&]() {
        sink = sink + static_cast<unsigned int>(NkGetLogicalCoreCount());
    }, kIters);

    const double stdLogicalNs = MeasureMeanNs([&]() {
        const unsigned int value = std::thread::hardware_concurrency();
        sink = sink + ((value == 0u) ? 1u : value);
    }, kIters);

    uint32_t valueA = 0x12345678u;
    const double nkByteSwapNs = MeasureMeanNs([&]() {
        valueA = NkByteSwap32(valueA + 13u);
        sink = sink ^ valueA;
    }, kIters);

    auto manualByteSwap32 = [](uint32_t value) -> uint32_t {
        return ((value >> 24) & 0x000000FFu) |
               ((value >> 8)  & 0x0000FF00u) |
               ((value << 8)  & 0x00FF0000u) |
               ((value << 24) & 0xFF000000u);
    };

    uint32_t valueB = 0x12345678u;
    const double manualByteSwapNs = MeasureMeanNs([&]() {
        valueB = manualByteSwap32(valueB + 13u);
        sink = sink ^ valueB;
    }, kIters);

    NK_FOUNDATION_LOG_INFO("\n[NKPlatform Bench] mean ns/op (lower is better)\n");
    NK_FOUNDATION_LOG_INFO("  NkGetLogicalCoreCount                  : %.4f ns\n", nkLogicalNs);
    NK_FOUNDATION_LOG_INFO("  std::thread::hardware_concurrency      : %.4f ns\n", stdLogicalNs);
    NK_FOUNDATION_LOG_INFO("  NkByteSwap32                           : %.4f ns\n", nkByteSwapNs);
    NK_FOUNDATION_LOG_INFO("  ManualByteSwap32                       : %.4f ns\n", manualByteSwapNs);
    NK_FOUNDATION_LOG_INFO("  ratio logical (Nk/std)                 : %.4f\n",
                (stdLogicalNs > 0.0) ? (nkLogicalNs / stdLogicalNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  ratio byteswap (Nk/manual)             : %.4f\n",
                (manualByteSwapNs > 0.0) ? (nkByteSwapNs / manualByteSwapNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  sink                                   : %u\n", sink);

    return 0;
}

