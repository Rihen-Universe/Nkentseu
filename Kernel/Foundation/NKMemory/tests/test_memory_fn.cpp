#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkFunction.h"

using namespace nkentseu::memory;
using namespace nkentseu;

TEST_CASE(NKMemoryFunction, CopyMoveCompareAndPattern) {
    nk_uint8 src[32] = {};
    nk_uint8 dst[32] = {};

    NkSet(src, 0x7A, sizeof(src));
    NkCopy(dst, src, sizeof(src));
    ASSERT_EQUAL(0, NkCompare(src, dst, sizeof(src)));

    dst[10] = 0x01;
    ASSERT_NOT_EQUAL(0, NkCompare(src, dst, sizeof(src)));

    void* found = NkFind(src, 0x7A, sizeof(src));
    ASSERT_NOT_NULL(found);

    nk_uint8 pattern[3] = {0x7A, 0x7A, 0x7A};
    void* patternPos = NkSearchPattern(src, sizeof(src), pattern, sizeof(pattern));
    ASSERT_NOT_NULL(patternPos);

    NkReverse(dst, sizeof(dst));
    NkRotate(dst, sizeof(dst), 5);
}
