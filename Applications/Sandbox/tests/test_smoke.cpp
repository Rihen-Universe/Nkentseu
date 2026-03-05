#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

TEST_CASE(SandboxSmoke, RuntimeTypesAccessible) {
    const int width = 1920;
    const int height = 1080;
    ASSERT_TRUE(width > 0);
    ASSERT_TRUE(height > 0);
    ASSERT_EQUAL(2073600, width * height);
}
