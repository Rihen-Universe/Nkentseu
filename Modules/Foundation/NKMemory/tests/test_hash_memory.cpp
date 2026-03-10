#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkHash.h"

using namespace nkentseu::memory;
using namespace nkentseu;

TEST_CASE(NKMemoryHash, PointerHashSetInsertErase) {
    NkPointerHashSet set;
    ASSERT_TRUE(set.Initialize(8u));

    nk_int32 values[128] = {};
    for (nk_size i = 0u; i < 128u; ++i) {
        ASSERT_TRUE(set.Insert(&values[i]));
    }
    ASSERT_TRUE(set.Size() == 128u);

    for (nk_size i = 0u; i < 128u; ++i) {
        ASSERT_TRUE(set.Contains(&values[i]));
    }

    for (nk_size i = 0u; i < 128u; i += 2u) {
        ASSERT_TRUE(set.Erase(&values[i]));
    }

    for (nk_size i = 0u; i < 128u; ++i) {
        if ((i & 1u) == 0u) {
            ASSERT_FALSE(set.Contains(&values[i]));
        } else {
            ASSERT_TRUE(set.Contains(&values[i]));
        }
    }

    set.Shutdown();
}

TEST_CASE(NKMemoryHash, PointerHashMapLookupAndUpdate) {
    NkPointerHashMap map;
    ASSERT_TRUE(map.Initialize(8u));

    nk_int32 keys[256] = {};
    nk_int32 values[256] = {};

    for (nk_size i = 0u; i < 256u; ++i) {
        values[i] = static_cast<nk_int32>(i * 3u);
        ASSERT_TRUE(map.Insert(&keys[i], &values[i]));
    }

    ASSERT_TRUE(map.Size() == 256u);

    for (nk_size i = 0u; i < 256u; ++i) {
        void* outValue = nullptr;
        ASSERT_TRUE(map.TryGet(&keys[i], &outValue));
        ASSERT_TRUE(outValue == &values[i]);
    }

    nk_int32 overrideValue = 1337;
    ASSERT_TRUE(map.Insert(&keys[42], &overrideValue));

    void* found = map.Find(&keys[42]);
    ASSERT_TRUE(found == &overrideValue);

    void* removed = nullptr;
    ASSERT_TRUE(map.Erase(&keys[42], &removed));
    ASSERT_TRUE(removed == &overrideValue);
    ASSERT_FALSE(map.Contains(&keys[42]));

    map.Clear();
    ASSERT_TRUE(map.Size() == 0u);

    map.Shutdown();
}
