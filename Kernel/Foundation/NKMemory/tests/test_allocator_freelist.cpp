#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, FreeListCoalescing) {
    NkFreeListAllocator allocator(1024u * 1024u);
    void* a = allocator.Allocate(128u * 1024u, 16u);
    void* b = allocator.Allocate(128u * 1024u, 16u);
    void* c = allocator.Allocate(128u * 1024u, 16u);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    allocator.Deallocate(a);
    allocator.Deallocate(b);
    allocator.Deallocate(c);

    void* big = allocator.Allocate(256u * 1024u, 16u);
    ASSERT_NOT_NULL(big);
}
