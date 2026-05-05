#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkContainerAllocator.h"
#include "NKPlatform/NkFoundationLog.h"

#include <chrono>
#include <stdio.h>

using namespace nkentseu::memory;
using namespace nkentseu;

namespace {

static nk_uint32 NextRand(nk_uint32& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

static nk_size RandRange(nk_uint32& state, nk_size minValue, nk_size maxValue) {
    if (maxValue <= minValue) {
        return minValue;
    }
    const nk_uint32 value = NextRand(state);
    const nk_size span = maxValue - minValue + 1u;
    return minValue + (static_cast<nk_size>(value) % span);
}

} // namespace

TEST_CASE(NKMemoryBenchmark, ContainerAllocatorVsMallocSmallObjects) {
    constexpr nk_size kSamples = 20000u;
    constexpr nk_size kSlots = 512u;

    NkContainerAllocator containerAllocator(nullptr, 64u * 1024u);
    NkMallocAllocator mallocAllocator;

    void* containerSlots[kSlots] = {};
    void* mallocSlots[kSlots] = {};

    nk_uint32 rngContainer = 0xC0FFEE01u;
    nk_uint32 rngMalloc = 0xC0FFEE01u;

    nk_size containerFailures = 0u;
    nk_size mallocFailures = 0u;

    const auto containerBegin = std::chrono::high_resolution_clock::now();
    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size slot = RandRange(rngContainer, 0u, kSlots - 1u);
        if (containerSlots[slot]) {
            containerAllocator.Deallocate(containerSlots[slot]);
            containerSlots[slot] = nullptr;
        }

        const nk_size size = RandRange(rngContainer, 24u, 512u);
        void* ptr = containerAllocator.Allocate(size, 16u);
        if (!ptr) {
            ++containerFailures;
        }
        containerSlots[slot] = ptr;
    }
    const auto containerEnd = std::chrono::high_resolution_clock::now();

    const auto mallocBegin = std::chrono::high_resolution_clock::now();
    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size slot = RandRange(rngMalloc, 0u, kSlots - 1u);
        if (mallocSlots[slot]) {
            mallocAllocator.Deallocate(mallocSlots[slot]);
            mallocSlots[slot] = nullptr;
        }

        const nk_size size = RandRange(rngMalloc, 24u, 512u);
        void* ptr = mallocAllocator.Allocate(size, 16u);
        if (!ptr) {
            ++mallocFailures;
        }
        mallocSlots[slot] = ptr;
    }
    const auto mallocEnd = std::chrono::high_resolution_clock::now();

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

    ASSERT_TRUE(containerFailures == 0u);
    ASSERT_TRUE(mallocFailures == 0u);
    ASSERT_TRUE(containerNs > 0.0);
    ASSERT_TRUE(mallocNs > 0.0);

    NK_FOUNDATION_LOG_INFO("[NKMemory Benchmark] NkContainerAllocator vs NkMallocAllocator (small churn)");
    NK_FOUNDATION_LOG_INFO("  NkContainerAllocator: %.2f ns/op", containerNs);
    NK_FOUNDATION_LOG_INFO("  NkMallocAllocator   : %.2f ns/op", mallocNs);
}
