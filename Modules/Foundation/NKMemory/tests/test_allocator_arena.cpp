#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkAllocator.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryAllocator, ArenaMarkerAndFreeToMarker) {
    NkArenaAllocator allocator(2048u);
    void* p1 = allocator.Allocate(256u, 16u);
    ASSERT_NOT_NULL(p1);

    const auto marker = allocator.CreateMarker();
    void* p2 = allocator.Allocate(256u, 16u);
    ASSERT_NOT_NULL(p2);

    allocator.FreeToMarker(marker);
    void* p3 = allocator.Allocate(256u, 16u);
    ASSERT_NOT_NULL(p3);
}
