#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKCore/NkTypes.h"
#include "NKCore/NkOptional.h"
#include "NKCore/NkVariant.h"
#include "NKCore/NkAtomic.h"
#include "NKCore/NkBits.h"

using namespace nkentseu;


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

TEST_CASE(NKCoreSmoke, OptionalSwapAndGetIf) {
    NkOptional<int> a;
    NkOptional<int> b;

    a.Emplace(10);
    b.Emplace(20);
    a.Swap(b);

    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());
    ASSERT_EQUAL(20, a.Value());
    ASSERT_EQUAL(10, b.Value());

    b.Reset();
    ASSERT_NULL(b.GetIf());
    ASSERT_EQUAL(99, b.ValueOr(99));
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

TEST_CASE(NKCoreSmoke, VariantSwapAndGetIf) {
    NkVariant<int, float> a(7);
    NkVariant<int, float> b(2.5f);

    a.Swap(b);

    ASSERT_TRUE(a.HoldsAlternative<float>());
    ASSERT_TRUE(b.HoldsAlternative<int>());
    ASSERT_NOT_NULL(a.GetIf<float>());
    ASSERT_NOT_NULL(b.GetIf<int>());
    ASSERT_NEAR(2.5f, a.Get<float>(), 0.0001f);
    ASSERT_EQUAL(7, b.Get<int>());
}

TEST_CASE(NKCoreSmoke, AtomicCounterAndCompareExchange) {
    NkAtomicInt32 counter(0);

    ASSERT_EQUAL(0, static_cast<int>(counter.Load()));
    ASSERT_EQUAL(0, static_cast<int>(counter.FetchAdd(5)));
    ASSERT_EQUAL(5, static_cast<int>(counter.Load()));

    nk_int32 expected = 5;
    ASSERT_TRUE(counter.CompareExchangeStrong(expected, 9));
    ASSERT_EQUAL(9, static_cast<int>(counter.Load()));

    expected = 5;
    ASSERT_FALSE(counter.CompareExchangeStrong(expected, 11));
    ASSERT_EQUAL(9, static_cast<int>(expected));
}

TEST_CASE(NKCoreSmoke, BitsUtilities) {
    ASSERT_EQUAL(4, static_cast<int>(NkBits::CountBits(static_cast<nk_uint32>(0xAAu))));
    ASSERT_EQUAL(8, static_cast<int>(NkBits::CountTrailingZeros(static_cast<nk_uint32>(0x100u))));
    ASSERT_EQUAL(1u, static_cast<unsigned int>(NkBits::NextPowerOfTwo(static_cast<nk_uint32>(0u))));
    ASSERT_EQUAL(4u, static_cast<unsigned int>(NkBits::NextPowerOfTwo(static_cast<nk_uint32>(3u))));
    ASSERT_EQUAL(1024u, static_cast<unsigned int>(NkBits::NextPowerOfTwo(static_cast<nk_uint32>(1000u))));

    const nk_uint32 x = 0x12345678u;
    const nk_uint32 y = NkBits::RotateLeft(x, 8);
    ASSERT_EQUAL(0x34567812u, static_cast<unsigned int>(y));
    ASSERT_EQUAL(x, static_cast<nk_uint32>(NkBits::RotateRight(y, 8)));
}

TEST_CASE(NKCoreSmoke, ByteWrapperOperators) {
    Byte a = Byte::from(0xF0u);
    Byte b = Byte::from(0x0Fu);
    Byte c = a | b;

    ASSERT_EQUAL(0xFFu, static_cast<unsigned int>(static_cast<uint8>(c)));
}
