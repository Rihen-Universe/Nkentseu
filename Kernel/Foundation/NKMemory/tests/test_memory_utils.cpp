#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkUtils.h"

using namespace nkentseu::memory;
using namespace nkentseu;

TEST_CASE(NKMemoryUtils, AlignmentHelpers) {
    ASSERT_EQUAL(16, static_cast<int>(NkAlignUp(13u, 8u)));
    ASSERT_EQUAL(8, static_cast<int>(NkAlignDown(13u, 8u)));
    ASSERT_TRUE(NkIsPowerOfTwo(64u));
    ASSERT_FALSE(NkIsPowerOfTwo(63u));
}

TEST_CASE(NKMemoryUtils, RawMemoryOps) {
    nk_uint8 a[16] = {};
    nk_uint8 b[16] = {};

    NkMemSet(a, 0x5A, sizeof(a));
    NkMemCopy(b, a, sizeof(a));
    ASSERT_EQUAL(0, NkMemCompare(a, b, sizeof(a)));

    b[0] = 0x00;
    ASSERT_NOT_EQUAL(0, NkMemCompare(a, b, sizeof(a)));

    NkMemZero(b, sizeof(b));
    ASSERT_EQUAL(0, b[0]);
    ASSERT_EQUAL(0, b[15]);
}
