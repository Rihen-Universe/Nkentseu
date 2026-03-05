#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkMemory.h"

using namespace nkentseu::memory;

TEST_CASE(NKMemorySystem, AllocateFreeAndStats) {
    NkMemorySystem& memorySystem = NkMemorySystem::Instance();
    memorySystem.Initialize(nullptr);

    const NkMemoryStats before = memorySystem.GetStats();
    void* p = memorySystem.Allocate(256u, 16u, __FILE__, __LINE__, __func__, "system-test");
    ASSERT_NOT_NULL(p);

    const NkMemoryStats during = memorySystem.GetStats();
    ASSERT_TRUE(during.liveAllocations >= before.liveAllocations + 1u);
    ASSERT_TRUE(during.liveBytes >= before.liveBytes + 256u);

    memorySystem.Free(p);
    const NkMemoryStats after = memorySystem.GetStats();
    ASSERT_TRUE(after.liveAllocations <= during.liveAllocations - 1u);
}
