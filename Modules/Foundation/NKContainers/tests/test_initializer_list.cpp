#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"

using namespace nkentseu;


TEST_CASE(NKContainersInitializerList, ManualConstruction) {
    const int raw[4] = {7, 8, 9, 10};
    NkInitializerList<int> list(raw, 4);

    ASSERT_EQUAL(4, static_cast<int>(list.Size()));
    ASSERT_EQUAL(7, list.Front());
    ASSERT_EQUAL(10, list.Back());
    ASSERT_EQUAL(9, list[2]);
}

TEST_CASE(NKContainersInitializerList, VectorConstructorFromInitializerList) {
    const int raw[3] = {100, 200, 300};
    NkInitializerList<int> list(raw, 3);
    NkVector<int> values(list);

    ASSERT_EQUAL(3, static_cast<int>(values.Size()));
    ASSERT_EQUAL(100, values[0]);
    ASSERT_EQUAL(200, values[1]);
    ASSERT_EQUAL(300, values[2]);
}
