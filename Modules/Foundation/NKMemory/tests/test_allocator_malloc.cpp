#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkUtils.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, MallocAlignmentAndReallocate) {
    NkMallocAllocator allocator;
    void* p = allocator.Allocate(256u, 64u);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(NkIsAlignedPtr(p, 64u));

    void* p2 = allocator.Reallocate(p, 256u, 1024u, 64u);
    ASSERT_NOT_NULL(p2);
    ASSERT_TRUE(NkIsAlignedPtr(p2, 64u));
    allocator.Deallocate(p2);
}

TEST_CASE(NKMemoryAllocator, NewAllocatorAllocateAndFree) {
    NkNewAllocator allocator;
    void* p = allocator.Allocate(512u, 32u);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(NkIsAlignedPtr(p, 32u));
    allocator.Deallocate(p);
}
