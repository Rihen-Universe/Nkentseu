#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, StackLifoDeallocation) {
    NkStackAllocator allocator(4096u);
    void* first = allocator.Allocate(256u, 16u);
    void* second = allocator.Allocate(256u, 16u);
    ASSERT_NOT_NULL(first);
    ASSERT_NOT_NULL(second);

    allocator.Deallocate(second);
    allocator.Deallocate(first);

    void* combined = allocator.Allocate(512u, 16u);
    ASSERT_NOT_NULL(combined);
}
