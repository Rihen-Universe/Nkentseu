#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"

using namespace nkentseu::core;

TEST_CASE(NKContainersBTree, InsertAndSearch) {
    NkBTree<int> tree(3);
    ASSERT_TRUE(tree.IsEmpty());

    tree.Insert(10);
    tree.Insert(20);
    tree.Insert(5);
    tree.Insert(6);
    tree.Insert(12);

    ASSERT_EQUAL(5, static_cast<int>(tree.Size()));
    ASSERT_TRUE(tree.Search(10));
    ASSERT_TRUE(tree.Search(6));
    ASSERT_FALSE(tree.Search(99));
}

TEST_CASE(NKContainersBTree, BulkInsertAndSearch) {
    NkBTree<int> tree(5);

    for (int i = 0; i < 200; ++i) {
        tree.Insert(i * 3);
    }

    ASSERT_EQUAL(200, static_cast<int>(tree.Size()));
    ASSERT_TRUE(tree.Search(0));
    ASSERT_TRUE(tree.Search(297));
    ASSERT_TRUE(tree.Search(597));
    ASSERT_FALSE(tree.Search(598));
}
