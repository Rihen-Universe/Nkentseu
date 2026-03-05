#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::math;

TEST_CASE(NKMathSmoke, ScalarFunctions) {
    ASSERT_NEAR(3.0f, Sqrt(9.0f), 0.0001f);
    ASSERT_NEAR(8.0f, PowInt(2.0f, 3), 0.0001f);
    ASSERT_NEAR(180.0f, ToDegrees(PI_F), 0.001f);
    ASSERT_EQUAL(16u, static_cast<unsigned int>(NextPowerOf2(13u)));
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
