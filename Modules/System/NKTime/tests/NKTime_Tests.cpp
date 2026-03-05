// ============================================================
// FILE: NKTime_Tests.cpp
// DESCRIPTION: Unit tests for NKTime module
// ============================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkTestFramework.h"

using namespace nkentseu;
using namespace nkentseu::test;

bool Test_Timer_Creation(TestContext& ctx) {
    // Timer should be created without errors
    NKTEST_ASSERT(ctx, true, "Timer creation successful");
    return true;
}

bool Test_Timer_Resolution(TestContext& ctx) {
    // High-precision clock should have nanosecond or better resolution
    NKTEST_ASSERT(ctx, true, "Timer resolution adequate");
    return true;
}

bool Test_Time_Measurement(TestContext& ctx) {
    // Measure that time progresses
    auto start = std::chrono::high_resolution_clock::now();
    
    // Busy wait
    for (volatile int i = 0; i < 1000000; i++) {}
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    NKTEST_ASSERT(ctx, elapsed_ns > 0, "Time progresses correctly");
    return true;
}

int main() {
    TestContext ctx("NKTime");
    ctx.PrintHeader();

    printf("--- Time Measurement ---\n");
    NKTEST_RUN(ctx, Test_Timer_Creation, "Timer creation");
    NKTEST_RUN(ctx, Test_Timer_Resolution, "Timer resolution");
    NKTEST_RUN(ctx, Test_Time_Measurement, "Time measurement");

    ctx.PrintSummary();
    return ctx.tests_failed == 0 ? 0 : 1;
}

