#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkMemory.h"

using namespace nkentseu::memory;
using namespace nkentseu;

namespace {
struct ProfileGcNode final : NkGcObject {
    static int DestroyedCount;
    ~ProfileGcNode() override { ++DestroyedCount; }
};
int ProfileGcNode::DestroyedCount = 0;
} // namespace

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

TEST_CASE(NKMemorySystem, HashIndexedTrackingUnderChurn) {
    NkMemorySystem& memorySystem = NkMemorySystem::Instance();
    memorySystem.Initialize(nullptr);

    constexpr nk_size kCount = 2048u;
    void* pointers[kCount] = {};

    const NkMemoryStats before = memorySystem.GetStats();

    for (nk_size i = 0u; i < kCount; ++i) {
        const nk_size bytes = 16u + ((i % 31u) * 13u);
        pointers[i] = memorySystem.Allocate(bytes, 16u, __FILE__, __LINE__, __func__, "index-churn");
        ASSERT_NOT_NULL(pointers[i]);
    }

    for (nk_size i = 0u; i < kCount; i += 2u) {
        memorySystem.Free(pointers[i]);
        pointers[i] = nullptr;
    }
    for (nk_size i = 1u; i < kCount; i += 2u) {
        memorySystem.Free(pointers[i]);
        pointers[i] = nullptr;
    }

    const NkMemoryStats after = memorySystem.GetStats();
    ASSERT_TRUE(after.liveAllocations == before.liveAllocations);
}

TEST_CASE(NKMemorySystem, CreateGcDestroyGcAndProfile) {
    NkMemorySystem& memorySystem = NkMemorySystem::Instance();
    memorySystem.Initialize(nullptr);

    const nk_size beforeCount = memorySystem.GetGcCount();
    NkGarbageCollector* gameplayGc = memorySystem.CreateGc(nullptr);
    ASSERT_NOT_NULL(gameplayGc);
    ASSERT_TRUE(memorySystem.GetGcCount() == beforeCount + 1u);

    NkGcProfile profile{};
    ASSERT_TRUE(memorySystem.GetGcProfile(gameplayGc, &profile));
    ASSERT_FALSE(profile.IsDefault);
    ASSERT_NOT_NULL(profile.Allocator);
    ASSERT_EQUAL(0, static_cast<int>(profile.ObjectCount));

    ASSERT_TRUE(memorySystem.SetGcName(gameplayGc, "Gameplay"));
    NkGcProfile named{};
    ASSERT_TRUE(memorySystem.GetGcProfile(gameplayGc, &named));
    ASSERT_EQUAL(0, NkCompare(named.Name, "Gameplay", 8u));

    ProfileGcNode::DestroyedCount = 0;
    ProfileGcNode* node = gameplayGc->New<ProfileGcNode>();
    ASSERT_NOT_NULL(node);

    NkGcProfile withObject{};
    ASSERT_TRUE(memorySystem.GetGcProfile(gameplayGc, &withObject));
    ASSERT_EQUAL(1, static_cast<int>(withObject.ObjectCount));

    gameplayGc->Collect();
    ASSERT_TRUE(ProfileGcNode::DestroyedCount >= 1);

    NkGcProfile afterCollect{};
    ASSERT_TRUE(memorySystem.GetGcProfile(gameplayGc, &afterCollect));
    ASSERT_EQUAL(0, static_cast<int>(afterCollect.ObjectCount));

    ASSERT_TRUE(memorySystem.DestroyGc(gameplayGc));
    ASSERT_TRUE(memorySystem.GetGcCount() == beforeCount);
    ASSERT_FALSE(memorySystem.DestroyGc(&memorySystem.Gc()));
}
