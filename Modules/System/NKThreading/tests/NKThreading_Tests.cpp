// ============================================================
// FILE: NKThreading_Tests.cpp
// DESCRIPTION: Unit tests for NKThreading module
// ============================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkTestFramework.h"
#include <thread>
#include <mutex>

using namespace nkentseu;
using namespace nkentseu::test;

static uint32_t g_counter = 0;
static std::mutex g_mutex;

bool Test_Mutex_Lock(TestContext& ctx) {
    // Test basic mutex locking
    std::mutex m;
    {
        std::lock_guard<std::mutex> lock(m);
        g_counter++;
    }
    NKTEST_ASSERT(ctx, g_counter == 1, "Mutex lock protects data");
    return true;
}

bool Test_Thread_Creation(TestContext& ctx) {
    // Test thread creation
    int result = 0;
    std::thread t([&result]() { result = 42; });
    t.join();
    NKTEST_ASSERT(ctx, result == 42, "Thread executes function");
    return true;
}

bool Test_Concurrent_Access(TestContext& ctx) {
    // Test concurrent access with mutex
    uint32_t counter = 0;
    std::mutex m;
    
    auto increment_func = [&counter, &m]() {
        for (int i = 0; i < 10; i++) {
            std::lock_guard<std::mutex> lock(m);
            counter++;
        }
    };
    
    std::thread t1(increment_func);
    std::thread t2(increment_func);
    
    t1.join();
    t2.join();
    
    NKTEST_ASSERT(ctx, counter == 20, "Concurrent access with mutex is safe");
    return true;
}

int main() {
    TestContext ctx("NKThreading");
    ctx.PrintHeader();

    printf("--- Threading Primitives ---\n");
    NKTEST_RUN(ctx, Test_Mutex_Lock, "Mutex locking");
    NKTEST_RUN(ctx, Test_Thread_Creation, "Thread creation");
    NKTEST_RUN(ctx, Test_Concurrent_Access, "Concurrent access");

    ctx.PrintSummary();
    return ctx.tests_failed == 0 ? 0 : 1;
}

