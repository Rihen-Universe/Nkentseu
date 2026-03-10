#include "NKCore/NkOptional.h"
#include "NKCore/NkAtomic.h"
#include "NKCore/NkBits.h"

#include <chrono>
#include <cstdio>
#include "NKPlatform/NkFoundationLog.h"
#include <optional>
#include <atomic>



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

    volatile int sink = 0;

    NkOptional<int> nkOpt;
    std::optional<int> stdOpt;

    const double nkOptionalNs = MeasureMeanNs([&]() {
        nkOpt.Emplace(7);
        sink = sink + nkOpt.Value();
        nkOpt.Reset();
    }, kIters);

    const double stdOptionalNs = MeasureMeanNs([&]() {
        stdOpt.emplace(7);
        sink = sink + *stdOpt;
        stdOpt.reset();
    }, kIters);

    NkAtomicInt32 nkAtomic(0);
    std::atomic<int> stdAtomic(0);

    const double nkAtomicNs = MeasureMeanNs([&]() {
        sink = sink + static_cast<int>(nkAtomic.FetchAdd(1));
    }, kIters);

    const double stdAtomicNs = MeasureMeanNs([&]() {
        sink = sink + stdAtomic.fetch_add(1, std::memory_order_seq_cst);
    }, kIters);

    uint32_t valueA = 0x12345678u;
    const double nkByteSwapNs = MeasureMeanNs([&]() {
        valueA = NkBits::ByteSwap32(valueA + 19u);
        sink = sink ^ static_cast<int>(valueA);
    }, kIters);

    auto manualByteSwap32 = [](uint32_t value) -> uint32_t {
        return ((value >> 24) & 0x000000FFu) |
               ((value >> 8)  & 0x0000FF00u) |
               ((value << 8)  & 0x00FF0000u) |
               ((value << 24) & 0xFF000000u);
    };

    uint32_t valueB = 0x12345678u;
    const double manualByteSwapNs = MeasureMeanNs([&]() {
        valueB = manualByteSwap32(valueB + 19u);
        sink = sink ^ static_cast<int>(valueB);
    }, kIters);

    NK_FOUNDATION_LOG_INFO("\n[NKCore Bench] mean ns/op (lower is better)\n");
    NK_FOUNDATION_LOG_INFO("  NkOptional toggle                     : %.4f ns\n", nkOptionalNs);
    NK_FOUNDATION_LOG_INFO("  std::optional toggle                  : %.4f ns\n", stdOptionalNs);
    NK_FOUNDATION_LOG_INFO("  NkAtomic FetchAdd                     : %.4f ns\n", nkAtomicNs);
    NK_FOUNDATION_LOG_INFO("  std::atomic fetch_add                  : %.4f ns\n", stdAtomicNs);
    NK_FOUNDATION_LOG_INFO("  NkBits::ByteSwap32                    : %.4f ns\n", nkByteSwapNs);
    NK_FOUNDATION_LOG_INFO("  ManualByteSwap32                      : %.4f ns\n", manualByteSwapNs);
    NK_FOUNDATION_LOG_INFO("  ratio optional (Nk/std)               : %.4f\n",
                (stdOptionalNs > 0.0) ? (nkOptionalNs / stdOptionalNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  ratio atomic (Nk/std)                 : %.4f\n",
                (stdAtomicNs > 0.0) ? (nkAtomicNs / stdAtomicNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  ratio byteswap (Nk/manual)            : %.4f\n",
                (manualByteSwapNs > 0.0) ? (nkByteSwapNs / manualByteSwapNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  sink                                  : %d\n", sink);

    return 0;
}

