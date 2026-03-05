// ============================================================
// FILE: NKPlatform_Tests.cpp
// DESCRIPTION: Unit tests for NKPlatform module
// ============================================================

#include "NKPlatform/NkPlatform.h"
#include "NKCore/NkTestFramework.h"

using namespace nkentseu;
using namespace nkentseu::test;

bool Test_Platform_Detection(TestContext& ctx) {
    // Platform should be detected
    NKTEST_ASSERT(ctx, NK_PLATFORM_NAME != nullptr, "Platform name is available");
    
    // At least one platform should be active
    #if defined(NK_PLATFORM_WINDOWS)
        NKTEST_ASSERT(ctx, true, "Windows platform detected");
    #elif defined(NK_PLATFORM_LINUX)
        NKTEST_ASSERT(ctx, true, "Linux platform detected");
    #elif defined(NK_PLATFORM_MACOS)
        NKTEST_ASSERT(ctx, true, "macOS platform detected");
    #elif defined(NK_PLATFORM_ANDROID)
        NKTEST_ASSERT(ctx, true, "Android platform detected");
    #else
        NKTEST_ASSERT(ctx, false, "Unknown platform");
    #endif
    
    return true;
}

bool Test_Architecture_Detection(TestContext& ctx) {
    // Architecture should be detected
    #if defined(NK_PLATFORM_X86_64)
        NKTEST_ASSERT(ctx, true, "x86-64 architecture detected");
    #elif defined(NK_PLATFORM_ARM64)
        NKTEST_ASSERT(ctx, true, "ARM64 architecture detected");
    #elif defined(NK_PLATFORM_WASM32)
        NKTEST_ASSERT(ctx, true, "WebAssembly detected");
    #else
        NKTEST_ASSERT(ctx, false, "Unknown architecture");
    #endif
    
    return true;
}

bool Test_Compiler_Detection(TestContext& ctx) {
    // Compiler should be detected
    #if defined(NK_COMPILER_MSVC)
        NKTEST_ASSERT(ctx, true, "MSVC compiler detected");
    #elif defined(NK_COMPILER_CLANG)
        NKTEST_ASSERT(ctx, true, "Clang compiler detected");
    #elif defined(NK_COMPILER_GCC)
        NKTEST_ASSERT(ctx, true, "GCC compiler detected");
    #else
        NKTEST_ASSERT(ctx, false, "Unknown compiler");
    #endif
    
    return true;
}

bool Test_Endianness(TestContext& ctx) {
    // Endianness should be defined
    uint16_t test = 0x0102;
    uint8_t* p = (uint8_t*)&test;
    
    #if defined(NK_LITTLE_ENDIAN)
        NKTEST_ASSERT(ctx, p[0] == 0x02, "Little endian confirmed");
    #elif defined(NK_BIG_ENDIAN)
        NKTEST_ASSERT(ctx, p[0] == 0x01, "Big endian confirmed");
    #endif
    
    return true;
}

int main() {
    TestContext ctx("NKPlatform");
    ctx.PrintHeader();

    printf("--- Platform Detection ---\n");
    NKTEST_RUN(ctx, Test_Platform_Detection, "Platform detection");
    NKTEST_RUN(ctx, Test_Architecture_Detection, "Architecture detection");
    NKTEST_RUN(ctx, Test_Compiler_Detection, "Compiler detection");
    NKTEST_RUN(ctx, Test_Endianness, "Endianness detection");

    ctx.PrintSummary();
    return ctx.tests_failed == 0 ? 0 : 1;
}

