#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkTextFormat.h"

#include <string>

namespace nkentseu {
struct NkVec2fTest {
    float x;
    float y;
};

inline std::string NKToString(const NkVec2fTest& value, std::string_view props) {
    if (props == "csv") {
        return NkFormatIndexed("{0:.2},{1:.2}", value.x, value.y);
    }
    return NkFormatIndexed("({0:.2}, {1:.2})", value.x, value.y);
}
} // namespace nkentseu

using namespace nkentseu;

TEST_CASE(NKLoggerIndexedFormat, ReplacesIndexedPlaceholders) {
    const std::string out = NkFormatIndexed("A={0} B={1}", 42, "ok");
    ASSERT_EQUAL(std::string("A=42 B=ok"), out);
}

TEST_CASE(NKLoggerIndexedFormat, AppliesBuiltinProperties) {
    ASSERT_EQUAL(std::string("ff"), NkFormatIndexed("{0:hex}", 255));
    ASSERT_EQUAL(std::string("3.14"), NkFormatIndexed("{0:.2}", 3.14159));
    ASSERT_EQUAL(std::string("ABC"), NkFormatIndexed("{0:upper}", "abc"));
}

TEST_CASE(NKLoggerIndexedFormat, KeepsUnknownPlaceholderWhenIndexMissing) {
    const std::string out = NkFormatIndexed("value={1}", 7);
    ASSERT_EQUAL(std::string("value={1}"), out);
}

TEST_CASE(NKLoggerIndexedFormat, SupportsEscapedBraces) {
    const std::string out = NkFormatIndexed("{{0}} => {0}", 9);
    ASSERT_EQUAL(std::string("{0} => 9"), out);
}

TEST_CASE(NKLoggerIndexedFormat, SupportsUserTypeViaNKToString) {
    const NkVec2fTest value{1.5f, 2.25f};
    ASSERT_EQUAL(std::string("1.50,2.25"), NkFormatIndexed("{0:csv}", value));
    ASSERT_EQUAL(std::string("v=(1.50, 2.25)"), NkFormatIndexed("v={0}", value));
}

