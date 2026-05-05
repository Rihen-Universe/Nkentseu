#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, PoolCapacityAndReuse) {
    NkPoolAllocator allocator(64u, 4u);
    void* p1 = allocator.Allocate(64u, 8u);
    void* p2 = allocator.Allocate(64u, 8u);
    void* p3 = allocator.Allocate(64u, 8u);
    void* p4 = allocator.Allocate(64u, 8u);
    void* p5 = allocator.Allocate(64u, 8u);

    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_NOT_NULL(p3);
    ASSERT_NOT_NULL(p4);
    ASSERT_TRUE(p5 == nullptr);

    allocator.Deallocate(p2);
    void* p6 = allocator.Allocate(64u, 8u);
    ASSERT_NOT_NULL(p6);
}
