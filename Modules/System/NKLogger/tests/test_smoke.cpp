#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkLogMessage.h"

#include <string>

using namespace nkentseu;

TEST_CASE(NKLoggerSmoke, LogLevelRoundTrip) {
    ASSERT_EQUAL(NkLogLevel::NK_INFO, NkStringToLogLevel("info"));
    ASSERT_EQUAL(NkLogLevel::NK_WARN, NkStringToLogLevel("warning"));
    ASSERT_EQUAL(std::string("ERR"), std::string(NkLogLevelToShortString(NkLogLevel::NK_ERROR)));
}

TEST_CASE(NKLoggerSmoke, FormatterIncludesMessageAndLevel) {
    NkFormatter formatter("[%L] %v");
    NkLogMessage msg(NkLogLevel::NK_DEBUG, "hello-log", "", 0, "", "test");

    std::string out = formatter.Format(msg);
    ASSERT_TRUE(out.find("hello-log") != std::string::npos);
    ASSERT_TRUE(out.find("DBG") != std::string::npos || out.find("debug") != std::string::npos);
}
