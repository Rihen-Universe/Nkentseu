#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkContainerAllocator.h"
#include "NKMemory/NkFunction.h"

#include <chrono>
#include <cstdio>
#include "NKPlatform/NkFoundationLog.h"
#include <new>

using namespace nkentseu;
using namespace nkentseu::memory;

namespace {

volatile nk_uint64 gNkMemoryBenchSink = 0u;

template <typename Func>
double NkMeasureMeanNs(Func&& func, nk_size iterations) {
    const auto t0 = std::chrono::steady_clock::now();
    for (nk_size i = 0u; i < iterations; ++i) {
        func();
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double totalNs = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return totalNs / static_cast<double>(iterations);
}

nk_uint32 NkNextRand(nk_uint32& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

nk_size NkRandRange(nk_uint32& state, nk_size minValue, nk_size maxValue) {
    if (maxValue <= minValue) {
        return minValue;
    }

    const nk_uint32 value = NkNextRand(state);
    const nk_size span = maxValue - minValue + 1u;
    return minValue + (static_cast<nk_size>(value) % span);
}

void NkBenchMallocAllocatorVsOperatorNewDelete() {
    constexpr nk_size kIterations = 200000u;
    constexpr nk_size kAllocSize = 64u;

    NkMallocAllocator nkAllocator;
    nk_size nkFailures = 0u;
    const double nkNs = NkMeasureMeanNs([&]() {
        void* p = nkAllocator.Allocate(kAllocSize, 16u);
        if (!p) {
            ++nkFailures;
            return;
        }
        NkSet(p, 0xA5, kAllocSize);
        gNkMemoryBenchSink = gNkMemoryBenchSink +
                             static_cast<nk_uint64>(static_cast<nk_uint8*>(p)[0]);
        nkAllocator.Deallocate(p);
    }, kIterations);

    nk_size stlFailures = 0u;
    const double stlNs = NkMeasureMeanNs([&]() {
        void* p = ::operator new(static_cast<std::size_t>(kAllocSize), std::nothrow);
        if (!p) {
            ++stlFailures;
            return;
        }
        NkSet(p, 0x5A, kAllocSize);
        gNkMemoryBenchSink = gNkMemoryBenchSink +
                             static_cast<nk_uint64>(static_cast<nk_uint8*>(p)[0]);
        ::operator delete(p);
    }, kIterations);

    NK_FOUNDATION_LOG_INFO("\n[NKMemory Bench] NkMallocAllocator vs operator new/delete\n");
    NK_FOUNDATION_LOG_INFO("  NkMallocAllocator mean ns/op : %.4f\n", nkNs);
    NK_FOUNDATION_LOG_INFO("  operator new/delete ns/op    : %.4f\n", stlNs);
    NK_FOUNDATION_LOG_INFO("  ratio Nk/std                 : %.4f\n",
                (stlNs > 0.0) ? (nkNs / stlNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  failures Nk/std              : %llu / %llu\n",
                static_cast<unsigned long long>(nkFailures),
                static_cast<unsigned long long>(stlFailures));
}

void NkBenchContainerAllocatorVsMallocChurn() {
    constexpr nk_size kSamples = 200000u;
    constexpr nk_size kSlots = 1024u;

    NkContainerAllocator containerAllocator(nullptr, 128u * 1024u);
    NkMallocAllocator mallocAllocator;

    void* containerSlots[kSlots] = {};
    void* mallocSlots[kSlots] = {};

    nk_uint32 rngContainer = 0xC0FFEE01u;
    nk_uint32 rngMalloc = 0xC0FFEE01u;

    nk_size containerFailures = 0u;
    const auto containerBegin = std::chrono::steady_clock::now();
    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size slot = NkRandRange(rngContainer, 0u, kSlots - 1u);
        if (containerSlots[slot]) {
            containerAllocator.Deallocate(containerSlots[slot]);
            containerSlots[slot] = nullptr;
        }

        const nk_size size = NkRandRange(rngContainer, 24u, 512u);
        void* ptr = containerAllocator.Allocate(size, 16u);
        if (!ptr) {
            ++containerFailures;
        } else {
            NkSet(ptr, 0xCD, size);
            gNkMemoryBenchSink = gNkMemoryBenchSink +
                                 static_cast<nk_uint64>(static_cast<nk_uint8*>(ptr)[0]);
        }
        containerSlots[slot] = ptr;
    }
    const auto containerEnd = std::chrono::steady_clock::now();

    nk_size mallocFailures = 0u;
    const auto mallocBegin = std::chrono::steady_clock::now();
    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size slot = NkRandRange(rngMalloc, 0u, kSlots - 1u);
        if (mallocSlots[slot]) {
            mallocAllocator.Deallocate(mallocSlots[slot]);
            mallocSlots[slot] = nullptr;
        }

        const nk_size size = NkRandRange(rngMalloc, 24u, 512u);
        void* ptr = mallocAllocator.Allocate(size, 16u);
        if (!ptr) {
            ++mallocFailures;
        } else {
            NkSet(ptr, 0x3C, size);
            gNkMemoryBenchSink = gNkMemoryBenchSink +
                                 static_cast<nk_uint64>(static_cast<nk_uint8*>(ptr)[0]);
        }
        mallocSlots[slot] = ptr;
    }
    const auto mallocEnd = std::chrono::steady_clock::now();

    for (nk_size i = 0u; i < kSlots; ++i) {
        if (containerSlots[i]) {
            containerAllocator.Deallocate(containerSlots[i]);
        }
        if (mallocSlots[i]) {
            mallocAllocator.Deallocate(mallocSlots[i]);
        }
    }

    const double containerNs =
        std::chrono::duration<double, std::nano>(containerEnd - containerBegin).count() /
        static_cast<double>(kSamples);
    const double mallocNs =
        std::chrono::duration<double, std::nano>(mallocEnd - mallocBegin).count() /
        static_cast<double>(kSamples);

    NK_FOUNDATION_LOG_INFO("\n[NKMemory Bench] NkContainerAllocator vs NkMallocAllocator (small churn)\n");
    NK_FOUNDATION_LOG_INFO("  NkContainerAllocator ns/op  : %.4f\n", containerNs);
    NK_FOUNDATION_LOG_INFO("  NkMallocAllocator ns/op     : %.4f\n", mallocNs);
    NK_FOUNDATION_LOG_INFO("  ratio container/malloc      : %.4f\n",
                (mallocNs > 0.0) ? (containerNs / mallocNs) : 0.0);
    NK_FOUNDATION_LOG_INFO("  failures container/malloc   : %llu / %llu\n",
                static_cast<unsigned long long>(containerFailures),
                static_cast<unsigned long long>(mallocFailures));
}

} // namespace

int main() {
    NK_FOUNDATION_LOG_INFO("\n[NKMemory Bench] start\n");
    NkBenchMallocAllocatorVsOperatorNewDelete();
    NkBenchContainerAllocatorVsMallocChurn();
    NK_FOUNDATION_LOG_INFO("\n[NKMemory Bench] sink=%llu\n",
                static_cast<unsigned long long>(gNkMemoryBenchSink));
    return 0;
}

