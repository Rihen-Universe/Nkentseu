#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/NkInvoke.h"
#include "NKCore/NkOptional.h"
#include "NKCore/NkVariant.h"

TEST_CASE(NKCoreUtility, OptionalVariantInvokeSmoke) {
    nkentseu::NkOptional<int> maybe;
    ASSERT_FALSE(maybe.HasValue());
    maybe.Emplace(42);
    ASSERT_TRUE(maybe.HasValue());
    ASSERT_EQUAL(42, maybe.Value());

    nkentseu::NkVariant<int, float> value;
    value.Emplace<int>(7);
    ASSERT_TRUE(value.HoldsAlternative<int>());
    ASSERT_EQUAL(7, value.Get<int>());

    struct Worker {
        int base = 3;
        int Add(int x) { return base + x; }
    };

    Worker worker;
    const int result = nkentseu::NkInvoke(&Worker::Add, worker, 9);
    ASSERT_EQUAL(12, result);

    int& ref = nkentseu::NkInvoke(&Worker::base, worker);
    ref = 10;
    ASSERT_EQUAL(10, worker.base);
}
