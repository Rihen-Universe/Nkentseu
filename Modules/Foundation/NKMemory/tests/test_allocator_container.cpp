#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkContainerAllocator.h"
#include "NKMemory/NkUtils.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemoryContainerAllocator, SmallBlocksO1Freelist) {
    NkContainerAllocator allocator(nullptr, 16u * 1024u);

    void* p1 = allocator.Allocate(24u, 8u);
    ASSERT_NOT_NULL(p1);
    allocator.Deallocate(p1);

    void* p2 = allocator.Allocate(24u, 8u);
    ASSERT_NOT_NULL(p2);
    ASSERT_TRUE(p1 == p2);

    allocator.Deallocate(p2);

    const NkContainerAllocator::NkStats stats = allocator.GetStats();
    ASSERT_TRUE(stats.PageCount >= 1u);
    ASSERT_TRUE(stats.SmallUsedBlocks == 0u);
    ASSERT_TRUE(stats.SmallFreeBlocks >= 1u);
}

TEST_CASE(NKMemoryContainerAllocator, LargeBlocksAlignmentAndStats) {
    NkContainerAllocator allocator(nullptr, 16u * 1024u);

    void* p = allocator.Allocate(8192u, 64u);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(NkIsAlignedPtr(p, 64u));

    NkContainerAllocator::NkStats during = allocator.GetStats();
    ASSERT_TRUE(during.LargeAllocationCount == 1u);
    ASSERT_TRUE(during.LargeAllocationBytes >= 8192u);

    allocator.Deallocate(p);

    NkContainerAllocator::NkStats after = allocator.GetStats();
    ASSERT_TRUE(after.LargeAllocationCount == 0u);
}

TEST_CASE(NKMemoryContainerAllocator, ResetReclaimsPools) {
    NkContainerAllocator allocator(nullptr, 32u * 1024u);

    void* a = allocator.Allocate(64u, 8u);
    void* b = allocator.Allocate(96u, 8u);
    void* c = allocator.Allocate(16384u, 32u);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    allocator.Reset();

    const NkContainerAllocator::NkStats stats = allocator.GetStats();
    ASSERT_TRUE(stats.PageCount == 0u);
    ASSERT_TRUE(stats.SmallUsedBlocks == 0u);
    ASSERT_TRUE(stats.LargeAllocationCount == 0u);
}
