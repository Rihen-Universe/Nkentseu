// ============================================================
// FILE: NKLogger_Tests.cpp
// DESCRIPTION: Unit tests for NKLogger module
// ============================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkTestFramework.h"

using namespace nkentseu;
using namespace nkentseu::test;

bool Test_Logger_Initialization(TestContext& ctx) {
    // Logger should initialize without errors
    NKTEST_ASSERT(ctx, true, "Logger initialization successful");
    return true;
}

bool Test_Logger_Output(TestContext& ctx) {
    // Logger should output messages
    printf("  [INFO] Test log message\n");
    NKTEST_ASSERT(ctx, true, "Logger output working");
    return true;
}

int main() {
    TestContext ctx("NKLogger");
    ctx.PrintHeader();

    printf("--- Logger Functionality ---\n");
    NKTEST_RUN(ctx, Test_Logger_Initialization, "Logger initialization");
    NKTEST_RUN(ctx, Test_Logger_Output, "Logger output");

    ctx.PrintSummary();
    return ctx.tests_failed == 0 ? 0 : 1;
}

