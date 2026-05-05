#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKCore/NkAtomic.h"

#include <thread>

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
    const nk_uint32 v = NextRand(state);
    const nk_size span = maxValue - minValue + 1u;
    return minValue + (static_cast<nk_size>(v) % span);
}

} // namespace

TEST_CASE(NKMemoryStress, MallocMultiThread) {
    NkAtomicInt failures(0);

    auto worker = [&failures](nk_uint32 seed) {
        NkMallocAllocator allocator;
        nk_uint32 rng = seed;
        for (int i = 0; i < 20000; ++i) {
            const nk_size size = RandRange(rng, 16u, 2048u);
            void* p = allocator.Allocate(size, 16u);
            if (!p) {
                ++failures;
                continue;
            }
            const nk_size touch = (size < 64u) ? size : 64u;
            NkSet(p, i & 0xFF, touch);
            allocator.Deallocate(p);
        }
    };

    std::thread t1(worker, 0x11111111u);
    std::thread t2(worker, 0x22222222u);
    std::thread t3(worker, 0x33333333u);
    std::thread t4(worker, 0x44444444u);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    ASSERT_EQUAL(0, failures.Load());
}
