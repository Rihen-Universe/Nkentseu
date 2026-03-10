#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkTextFormat.h"

namespace nkentseu {
struct NkVec2fTest {
    float x;
    float y;
};

inline NkString NKToString(const NkVec2fTest& value, NkStringView props) {
    if (props == "csv") {
        return NkFormatIndexed("{0:.2},{1:.2}", value.x, value.y);
    }
    return NkFormatIndexed("({0:.2}, {1:.2})", value.x, value.y);
}
} // namespace nkentseu

using namespace nkentseu;

TEST_CASE(NKLoggerIndexedFormat, ReplacesIndexedPlaceholders) {
    const NkString out = NkFormatIndexed("A={0} B={1}", 42, "ok");
    ASSERT_TRUE(out == "A=42 B=ok");
}

TEST_CASE(NKLoggerIndexedFormat, AppliesBuiltinProperties) {
    ASSERT_TRUE(NkFormatIndexed("{0:hex}", 255) == "ff");
    ASSERT_TRUE(NkFormatIndexed("{0:.2}", 3.14159) == "3.14");
    ASSERT_TRUE(NkFormatIndexed("{0:upper}", "abc") == "ABC");
}

TEST_CASE(NKLoggerIndexedFormat, KeepsUnknownPlaceholderWhenIndexMissing) {
    const NkString out = NkFormatIndexed("value={1}", 7);
    ASSERT_TRUE(out == "value={1}");
}

TEST_CASE(NKLoggerIndexedFormat, SupportsEscapedBraces) {
    const NkString out = NkFormatIndexed("{{0}} => {0}", 9);
    ASSERT_TRUE(out == "{0} => 9");
}

TEST_CASE(NKLoggerIndexedFormat, SupportsUserTypeViaNKToString) {
    const NkVec2fTest value{1.5f, 2.25f};
    ASSERT_TRUE(NkFormatIndexed("{0:csv}", value) == "1.50,2.25");
    ASSERT_TRUE(NkFormatIndexed("v={0}", value) == "v=(1.50, 2.25)");
}
