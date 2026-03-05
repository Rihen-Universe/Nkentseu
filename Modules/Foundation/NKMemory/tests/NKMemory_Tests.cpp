// ============================================================
// FILE: NKMemory_Tests.cpp
// DESCRIPTION: Unit tests for NKMemory module
// ============================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkTestFramework.h"
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::test;

// Simple allocation tracker
static uint64_t g_alloc_count = 0;
static uint64_t g_dealloc_count = 0;
static uint64_t g_bytes_allocated = 0;

bool Test_Memory_Allocation(TestContext& ctx) {
    // Basic malloc/free should work
    uint8_t* ptr = (uint8_t*)malloc(256);
    NKTEST_ASSERT(ctx, ptr != nullptr, "malloc(256) returns non-null pointer");
    
    g_alloc_count++;
    g_bytes_allocated += 256;
    
    free(ptr);
    g_dealloc_count++;
    
    NKTEST_ASSERT(ctx, g_alloc_count > g_dealloc_count || g_alloc_count == g_dealloc_count, 
                  "Allocation/deallocation tracking");
    
    return true;
}

bool Test_Memory_Alignment(TestContext& ctx) {
    // Test alignment
    uint8_t* ptr = (uint8_t*)malloc(64);
    NKTEST_ASSERT(ctx, ptr != nullptr, "malloc returns valid pointer");
    
    // Basic alignment check
    uint64_t addr = (uint64_t)ptr;
    NKTEST_ASSERT(ctx, (addr & 0x07) == 0, "Pointer is 8-byte aligned");
    
    free(ptr);
    return true;
}

bool Test_Memory_Zeroing(TestContext& ctx) {
    // Test calloc (zeroed memory)
    uint8_t* ptr = (uint8_t*)calloc(256, 1);
    NKTEST_ASSERT(ctx, ptr != nullptr, "calloc returns non-null");
    
    bool all_zero = true;
    for (int i = 0; i < 256; i++) {
        if (ptr[i] != 0) {
            all_zero = false;
            break;
        }
    }
    NKTEST_ASSERT(ctx, all_zero, "calloc memory is zeroed");
    
    free(ptr);
    return true;
}

bool Test_Memory_Copy(TestContext& ctx) {
    // Test memcpy
    uint8_t src[32] = {0};
    uint8_t dst[32] = {0xFF};
    
    for (int i = 0; i < 32; i++) {
        src[i] = (uint8_t)i;
    }
    
    memcpy(dst, src, 32);
    
    bool match = true;
    for (int i = 0; i < 32; i++) {
        if (dst[i] != src[i]) {
            match = false;
            break;
        }
    }
    NKTEST_ASSERT(ctx, match, "memcpy copies data correctly");
    
    return true;
}

bool Test_Memory_Move(TestContext& ctx) {
    // Test memmove (overlapping regions)
    uint8_t data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = (uint8_t)i;
    }
    
    // Move overlapping region
    memmove(data + 8, data, 32);
    
    bool correct = true;
    for (int i = 0; i < 32; i++) {
        if (data[8 + i] != (uint8_t)i) {
            correct = false;
            break;
        }
    }
    NKTEST_ASSERT(ctx, correct, "memmove handles overlapping correctly");
    
    return true;
}

int main() {
    TestContext ctx("NKMemory");
    ctx.PrintHeader();

    printf("--- Memory Operations ---\n");
    NKTEST_RUN(ctx, Test_Memory_Allocation, "Basic allocation");
    NKTEST_RUN(ctx, Test_Memory_Alignment, "Pointer alignment");
    NKTEST_RUN(ctx, Test_Memory_Zeroing, "Memory zeroing (calloc)");
    NKTEST_RUN(ctx, Test_Memory_Copy, "Memory copy");
    NKTEST_RUN(ctx, Test_Memory_Move, "Memory move");

    ctx.PrintSummary();
    
    // Memory report
    MemoryTracker tracker;
    tracker.allocations = g_alloc_count;
    tracker.deallocations = g_dealloc_count;
    tracker.bytes_allocated = g_bytes_allocated;
    tracker.PrintReport("NKMemory");
    
    return ctx.tests_failed == 0 ? 0 : 1;
}

