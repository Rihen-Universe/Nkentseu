#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, BuddyBasic) {
    NkBuddyAllocator allocator(1024u * 1024u, 32u);
    void* p = allocator.Allocate(1000u, 16u);
    ASSERT_NOT_NULL(p);
    allocator.Deallocate(p);
}
