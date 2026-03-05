#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

#include <chrono>
#include <stdio.h>

using namespace nkentseu::memory;
using namespace nkentseu;

namespace {

struct BenchResult {
    const char* name;
    double avgNs;
    double p99Ns;
    nk_size failures;
    nk_size samples;
};

static nk_uint32 NextRand(nk_uint32& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

static nk_size RandRange(nk_uint32& state, nk_size minValue, nk_size maxValue) {
    if (maxValue <= minValue) {
        return minValue;
    }
    const nk_uint32 v = NextRand(state);
    const nk_size span = maxValue - minValue + 1u;
    return minValue + (static_cast<nk_size>(v) % span);
}

template <nk_size N>
static void InsertionSort(nk_int64 (&values)[N], nk_size count) {
    if (count <= 1u) {
        return;
    }
    for (nk_size i = 1u; i < count; ++i) {
        const nk_int64 key = values[i];
        nk_size j = i;
        while (j > 0u && values[j - 1u] > key) {
            values[j] = values[j - 1u];
            --j;
        }
        values[j] = key;
    }
}

template <nk_size N>
static BenchResult MakeBenchResult(const char* name, nk_int64 (&samples)[N], nk_size failures) {
    nk_size valid = 0u;
    nk_int64 total = 0;
    for (nk_size i = 0u; i < N; ++i) {
        if (samples[i] > 0) {
            total += samples[i];
            ++valid;
        }
    }

    BenchResult result{};
    result.name = name;
    result.failures = failures;
    result.samples = valid;
    result.avgNs = valid ? static_cast<double>(total) / static_cast<double>(valid) : 0.0;
    result.p99Ns = 0.0;

    if (valid > 0u) {
        InsertionSort(samples, valid);
        nk_size idx = (valid * 99u) / 100u;
        if (idx >= valid) {
            idx = valid - 1u;
        }
        result.p99Ns = static_cast<double>(samples[idx]);
    }

    return result;
}

template <typename AllocatorT>
static BenchResult BenchSimpleAllocateFree(const char* name,
                                           AllocatorT& allocator,
                                           nk_size minSize,
                                           nk_size maxSize,
                                           nk_size alignment) {
    constexpr nk_size kSamples = 2048u;
    nk_int64 samples[kSamples] = {};
    nk_uint32 rng = 0x1234ABCDu;
    nk_size failures = 0u;

    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size size = RandRange(rng, minSize, maxSize);
        const auto t0 = std::chrono::high_resolution_clock::now();
        void* ptr = allocator.Allocate(size, alignment);
        if (ptr) {
            allocator.Deallocate(ptr);
        } else {
            ++failures;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        samples[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }

    return MakeBenchResult(name, samples, failures);
}

static BenchResult BenchLinearAllocator() {
    constexpr nk_size kSamples = 2048u;
    nk_int64 samples[kSamples] = {};
    nk_uint32 rng = 0xAA551122u;
    nk_size failures = 0u;

    NkLinearAllocator allocator(2u * 1024u * 1024u);
    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size size = RandRange(rng, 64u, 4096u);
        const auto t0 = std::chrono::high_resolution_clock::now();
        void* ptr = allocator.Allocate(size, 16u);
        if (!ptr) {
            allocator.Reset();
            ptr = allocator.Allocate(size, 16u);
        }
        if (!ptr) {
            ++failures;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        samples[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }

    return MakeBenchResult("NkLinearAllocator", samples, failures);
}

static BenchResult BenchFreeListAllocator() {
    constexpr nk_size kSamples = 2048u;
    constexpr nk_size kSlots = 256u;
    nk_int64 samples[kSamples] = {};
    void* slots[kSlots] = {};
    nk_uint32 rng = 0xCAFEBABEu;
    nk_size failures = 0u;

    NkFreeListAllocator allocator(4u * 1024u * 1024u);

    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size slot = RandRange(rng, 0u, kSlots - 1u);
        if (slots[slot]) {
            allocator.Deallocate(slots[slot]);
            slots[slot] = nullptr;
        }

        const nk_size size = RandRange(rng, 32u, 4096u);
        const auto t0 = std::chrono::high_resolution_clock::now();
        void* ptr = allocator.Allocate(size, 16u);
        if (!ptr) {
            ++failures;
        }
        slots[slot] = ptr;
        const auto t1 = std::chrono::high_resolution_clock::now();
        samples[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }

    for (nk_size i = 0u; i < kSlots; ++i) {
        if (slots[i]) {
            allocator.Deallocate(slots[i]);
        }
    }

    return MakeBenchResult("NkFreeListAllocator", samples, failures);
}

static BenchResult BenchBuddyAllocator() {
    constexpr nk_size kSamples = 2048u;
    nk_int64 samples[kSamples] = {};
    nk_uint32 rng = 0xB16B00B5u;
    nk_size failures = 0u;

    NkBuddyAllocator allocator(4u * 1024u * 1024u, 32u);
    for (nk_size i = 0u; i < kSamples; ++i) {
        const nk_size size = RandRange(rng, 32u, 4096u);
        const auto t0 = std::chrono::high_resolution_clock::now();
        void* ptr = allocator.Allocate(size, 16u);
        if (ptr) {
            allocator.Deallocate(ptr);
        } else {
            ++failures;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        samples[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }

    return MakeBenchResult("NkBuddyAllocator", samples, failures);
}

static BenchResult BenchVirtualAllocator() {
    constexpr nk_size kSamples = 256u;
    nk_int64 samples[kSamples] = {};
    nk_size failures = 0u;

    NkVirtualAllocator allocator;
    for (nk_size i = 0u; i < kSamples; ++i) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        void* ptr = allocator.Allocate(4096u, NK_MEMORY_DEFAULT_ALIGNMENT);
        if (ptr) {
            allocator.Deallocate(ptr);
        } else {
            ++failures;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        samples[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }

    return MakeBenchResult("NkVirtualAllocator", samples, failures);
}

} // namespace

TEST_CASE(NKMemoryBenchmark, AllocatorP99) {
    NkMallocAllocator mallocAllocator;
    NkPoolAllocator poolAllocator(128u, 4096u);

    BenchResult results[6] = {};
    results[0] = BenchSimpleAllocateFree("NkMallocAllocator", mallocAllocator, 32u, 2048u, 16u);
    results[1] = BenchLinearAllocator();
    results[2] = BenchSimpleAllocateFree("NkPoolAllocator", poolAllocator, 64u, 128u, 16u);
    results[3] = BenchFreeListAllocator();
    results[4] = BenchBuddyAllocator();
    results[5] = BenchVirtualAllocator();

    printf("\n[NKMemory Benchmark] avg/p99 en nanosecondes\n");
    for (const BenchResult& result : results) {
        printf("  - %-20s avg=%10.2f ns  p99=%10.2f ns  failures=%llu/%llu\n",
               result.name,
               result.avgNs,
               result.p99Ns,
               static_cast<unsigned long long>(result.failures),
               static_cast<unsigned long long>(result.samples));
        ASSERT_TRUE(result.samples > 0u);
        ASSERT_TRUE(result.avgNs > 0.0);
        ASSERT_TRUE(result.p99Ns > 0.0);
    }
}
