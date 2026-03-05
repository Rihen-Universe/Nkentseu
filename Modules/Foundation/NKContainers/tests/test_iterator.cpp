#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"

using namespace nkentseu::core;

TEST_CASE(NKContainersIterator, AdvanceDistanceNextPrev) {
    int data[5] = {10, 20, 30, 40, 50};
    int* it = data;

    NkAdvance(it, 2);
    ASSERT_EQUAL(30, *it);

    const auto distance = NkDistance(data, data + 5);
    ASSERT_EQUAL(5, static_cast<int>(distance));

    int* next = NkNext(data, 3);
    int* prev = NkPrev(data + 4, 2);
    ASSERT_EQUAL(40, *next);
    ASSERT_EQUAL(30, *prev);
}

TEST_CASE(NKContainersIterator, ReverseIterator) {
    int data[4] = {1, 2, 3, 4};
    NkReverseIterator<int*> rit(data + 4);

    ASSERT_EQUAL(4, *rit);
    ++rit;
    ASSERT_EQUAL(3, *rit);
    ++rit;
    ASSERT_EQUAL(2, *rit);
}
