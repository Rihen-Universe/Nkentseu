#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkLogMessage.h"

using namespace nkentseu;

TEST_CASE(NKLoggerSmoke, LogLevelRoundTrip) {
    ASSERT_EQUAL(NkLogLevel::NK_INFO, NkStringToLogLevel("info"));
    ASSERT_EQUAL(NkLogLevel::NK_WARN, NkStringToLogLevel("warning"));
    ASSERT_TRUE(NkString(NkLogLevelToShortString(NkLogLevel::NK_ERROR)) == "ERR");
}

TEST_CASE(NKLoggerSmoke, FormatterIncludesMessageAndLevel) {
    NkFormatter formatter("[%L] %v");
    NkLogMessage msg(NkLogLevel::NK_DEBUG, "hello-log", "", 0, "", "test");

    NkString out = formatter.Format(msg);
    ASSERT_TRUE(out.Find("hello-log") != NkString::npos);
    ASSERT_TRUE(out.Find("DBG") != NkString::npos || out.Find("debug") != NkString::npos);
}
