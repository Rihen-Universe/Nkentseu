#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::math;

TEST_CASE(NKMathSmoke, ScalarFunctions) {
    ASSERT_NEAR(3.0f, NkSqrt(9.0f), 0.0001f);
    ASSERT_NEAR(8.0f, NkPowInt(2.0f, 3), 0.0001f);
    ASSERT_NEAR(180.0f, NkToDegrees(PI_F), 0.001f);
    ASSERT_EQUAL(16u, static_cast<unsigned int>(NkNextPowerOf2(13u)));
}

TEST_CASE(NKMathSmoke, VectorAndRectTypes) {
    NkVec2f v1(2.0f, 3.0f);
    NkVec2f v2(1.0f, 5.0f);

    NkVec2f v3 = v1 + v2;
    ASSERT_NEAR(3.0f, v3.x, 0.0001f);
    ASSERT_NEAR(8.0f, v3.y, 0.0001f);

    NkRect r(10, 20, 100, 50);
    ASSERT_TRUE(r.Contains(20, 40));
    ASSERT_FALSE(r.Contains(200, 40));
}

TEST_CASE(NKMathSmoke, BitAndIntegerUtilities) {
    ASSERT_TRUE(NkIsPowerOf2(16u));
    ASSERT_FALSE(NkIsPowerOf2(18u));

    ASSERT_EQUAL(1u, static_cast<unsigned int>(NkNextPowerOf2(0u)));
    ASSERT_EQUAL(16u, static_cast<unsigned int>(NkNextPowerOf2(13u)));

    ASSERT_EQUAL(31u, static_cast<unsigned int>(NkClz(static_cast<nkentseu::nk_uint32>(1u))));
    ASSERT_EQUAL(0u, static_cast<unsigned int>(NkCtz(static_cast<nkentseu::nk_uint32>(1u))));
    ASSERT_EQUAL(4u, static_cast<unsigned int>(NkPopcount(static_cast<nkentseu::nk_uint32>(0b10110100u))));
}

TEST_CASE(NKMathSmoke, DivisionAndInterpolationEdges) {
    const DivResult64 normal = NkDivI64(10, 3);
    ASSERT_EQUAL(3, static_cast<int>(normal.quot));
    ASSERT_EQUAL(1, static_cast<int>(normal.rem));

    const DivResult64 divideByZero = NkDivI64(10, 0);
    ASSERT_EQUAL(0, static_cast<int>(divideByZero.quot));
    ASSERT_EQUAL(10, static_cast<int>(divideByZero.rem));

    ASSERT_NEAR(0.0f, NkSmoothstep(-0.25f), 0.0001f);
    ASSERT_NEAR(1.0f, NkSmoothstep(2.0f), 0.0001f);
    ASSERT_NEAR(0.5f, NkSmoothstep(0.5f), 0.0001f);

    ASSERT_NEAR(0.0f, NkSmootherstep(-0.25f), 0.0001f);
    ASSERT_NEAR(1.0f, NkSmootherstep(2.0f), 0.0001f);
    ASSERT_NEAR(0.5f, NkSmootherstep(0.5f), 0.0001f);
}
