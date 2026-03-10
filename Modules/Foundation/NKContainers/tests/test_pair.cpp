#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"

using namespace nkentseu;

TEST_CASE(NKContainersPair, ConstructionAndComparison) {
    NkPair<int, int> a(1, 2);
    NkPair<int, int> b(1, 2);
    NkPair<int, int> c(2, 1);

    ASSERT_TRUE(a == b);
    ASSERT_TRUE(a != c);
    ASSERT_TRUE(a < c);
    ASSERT_TRUE(c > a);
}

TEST_CASE(NKContainersPair, HelpersAndSwap) {
    NkPair<int, int> a = NkMakePair(10, 20);
    NkPair<int, int> b = NkMakePair(30, 40);

    ASSERT_EQUAL(10, NkGetFirst(a));
    ASSERT_EQUAL(20, NkGetSecond(a));

    NkSwap(a, b);
    ASSERT_EQUAL(30, a.First);
    ASSERT_EQUAL(40, a.Second);
    ASSERT_EQUAL(10, b.First);
    ASSERT_EQUAL(20, b.Second);
}
