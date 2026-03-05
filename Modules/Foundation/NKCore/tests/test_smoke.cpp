#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/NkTypes.h"
#include "NKCore/NkOptional.h"
#include "NKCore/NkVariant.h"

using namespace nkentseu;
using namespace nkentseu::core;

TEST_CASE(NKCoreSmoke, OptionalBasicLifecycle) {
    NkOptional<int> value;
    ASSERT_FALSE(value.HasValue());

    value = 42;
    ASSERT_TRUE(value.HasValue());
    ASSERT_EQUAL(42, value.Value());

    value.Reset();
    ASSERT_FALSE(value.HasValue());
    ASSERT_EQUAL(7, value.ValueOr(7));
}

TEST_CASE(NKCoreSmoke, VariantSetAndVisit) {
    NkVariant<int, float> v;
    v = 12;
    ASSERT_TRUE(v.HoldsAlternative<int>());
    ASSERT_EQUAL(12, v.Get<int>());

    v = 3.5f;
    ASSERT_TRUE(v.HoldsAlternative<float>());

    float visited = 0.0f;
    v.Visit([&](auto& x) {
        visited = static_cast<float>(x);
    });

    ASSERT_NEAR(3.5f, visited, 0.0001f);
}

TEST_CASE(NKCoreSmoke, ByteWrapperOperators) {
    Byte a = Byte::from(0xF0u);
    Byte b = Byte::from(0x0Fu);
    Byte c = a | b;

    ASSERT_EQUAL(0xFFu, static_cast<unsigned int>(static_cast<uint8>(c)));
}
