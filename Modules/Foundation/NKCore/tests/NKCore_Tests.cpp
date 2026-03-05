// ============================================================
// FILE: NKCore_Tests.cpp
// DESCRIPTION: Unit tests for NKCore module
// ============================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkTestFramework.h"

using namespace nkentseu;
using namespace nkentseu::test;

bool Test_Types_Integer(TestContext& ctx) {
    NKTEST_ASSERT(ctx, sizeof(nk_int8) == 1, "sizeof(nk_int8) == 1");
    NKTEST_ASSERT(ctx, sizeof(nk_int16) == 2, "sizeof(nk_int16) == 2");
    NKTEST_ASSERT(ctx, sizeof(nk_int32) == 4, "sizeof(nk_int32) == 4");
    NKTEST_ASSERT(ctx, sizeof(nk_int64) == 8, "sizeof(nk_int64) == 8");
    return true;
}

bool Test_Types_Unsigned(TestContext& ctx) {
    NKTEST_ASSERT(ctx, sizeof(nk_uint8) == 1, "sizeof(nk_uint8) == 1");
    NKTEST_ASSERT(ctx, sizeof(nk_uint16) == 2, "sizeof(nk_uint16) == 2");
    NKTEST_ASSERT(ctx, sizeof(nk_uint32) == 4, "sizeof(nk_uint32) == 4");
    NKTEST_ASSERT(ctx, sizeof(nk_uint64) == 8, "sizeof(nk_uint64) == 8");
    return true;
}

bool Test_Types_Float(TestContext& ctx) {
    NKTEST_ASSERT(ctx, sizeof(nk_float) == 4, "sizeof(nk_float) == 4");
    NKTEST_ASSERT(ctx, sizeof(nk_double) == 8, "sizeof(nk_double) == 8");
    return true;
}

bool Test_Types_Bool(TestContext& ctx) {
    NKTEST_ASSERT(ctx, sizeof(nk_bool) >= 1, "sizeof(nk_bool) >= 1");
    nk_bool t = NK_TRUE;
    nk_bool f = NK_FALSE;
    NKTEST_ASSERT(ctx, t == NK_TRUE, "nk_bool true assignment");
    NKTEST_ASSERT(ctx, f == NK_FALSE, "nk_bool false assignment");
    return true;
}

bool Test_Types_Pointers(TestContext& ctx) {
    NKTEST_ASSERT(ctx, sizeof(nk_ptr) == sizeof(void*), "sizeof(nk_ptr) == sizeof(void*)");
    nk_ptr null = NK_NULL;
    NKTEST_ASSERT(ctx, null == NK_NULL, "nk_ptr null assignment");
    return true;
}

int main() {
    TestContext ctx("NKCore");
    ctx.PrintHeader();

    // Type system tests
    printf("--- Type System ---\n");
    NKTEST_RUN(ctx, Test_Types_Integer, "Integer types");
    NKTEST_RUN(ctx, Test_Types_Unsigned, "Unsigned types");
    NKTEST_RUN(ctx, Test_Types_Float, "Float types");
    NKTEST_RUN(ctx, Test_Types_Bool, "Bool type");
    NKTEST_RUN(ctx, Test_Types_Pointers, "Pointer types");

    ctx.PrintSummary();
    return ctx.tests_failed == 0 ? 0 : 1;
}

