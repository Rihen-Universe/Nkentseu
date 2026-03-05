#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKContainers/NkContainers.h"
#include "NKContainers/Views/NkSpan.h"

using namespace nkentseu::core;

TEST_CASE(NKContainersSpan, ViewsAndIndexing) {
    int raw[5] = {1, 2, 3, 4, 5};
    NkSpan<int> span(raw);

    ASSERT_EQUAL(5, static_cast<int>(span.Size()));
    ASSERT_EQUAL(1, span.Front());
    ASSERT_EQUAL(5, span.Back());
    ASSERT_EQUAL(3, span[2]);

    NkSpan<int> first = span.First(2);
    NkSpan<int> last = span.Last(2);
    NkSpan<int> middle = span.Subspan(1, 3);

    ASSERT_EQUAL(2, static_cast<int>(first.Size()));
    ASSERT_EQUAL(1, first[0]);
    ASSERT_EQUAL(2, first[1]);

    ASSERT_EQUAL(2, static_cast<int>(last.Size()));
    ASSERT_EQUAL(4, last[0]);
    ASSERT_EQUAL(5, last[1]);

    ASSERT_EQUAL(3, static_cast<int>(middle.Size()));
    ASSERT_EQUAL(2, middle[0]);
    ASSERT_EQUAL(4, middle[2]);
}

TEST_CASE(NKContainersSpan, ConstructFromVector) {
    NkVector<int> values;
    values.PushBack(11);
    values.PushBack(22);
    values.PushBack(33);

    NkSpan<int> span(values);
    ASSERT_EQUAL(3, static_cast<int>(span.Size()));
    ASSERT_EQUAL(11, span[0]);
    ASSERT_EQUAL(22, span[1]);
    ASSERT_EQUAL(33, span[2]);
}
