#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, LinearReset) {
    NkLinearAllocator allocator(1024u);
    void* p1 = allocator.Allocate(128u, 16u);
    void* p2 = allocator.Allocate(128u, 16u);
    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_TRUE(allocator.Used() >= 256u);

    allocator.Reset();
    ASSERT_EQUAL(0, static_cast<int>(allocator.Used()));

    void* p3 = allocator.Allocate(128u, 16u);
    ASSERT_NOT_NULL(p3);
}
