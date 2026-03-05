#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, VirtualAllocateAndFree) {
    NkVirtualAllocator allocator;
    void* p = allocator.Allocate(4096u, NK_MEMORY_DEFAULT_ALIGNMENT);
    ASSERT_NOT_NULL(p);
    allocator.Deallocate(p);
}

TEST_CASE(NKMemoryAllocator, VirtualAllocateTagAndFree) {
    NkVirtualAllocator allocator;
    void* p = allocator.AllocateVirtual(8192u, "test-virtual");
    ASSERT_NOT_NULL(p);
    allocator.FreeVirtual(p);
}
