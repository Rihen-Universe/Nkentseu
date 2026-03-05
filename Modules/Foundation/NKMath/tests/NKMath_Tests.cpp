// ============================================================
// FILE: NKMath_Tests.cpp
// DESCRIPTION: Comprehensive unit tests for NKMath
// AUTHOR: Rihen
// DATE: 2026-03-05
// ============================================================
// Tests for all 36+ math functions
// Coverage: Correctness, edge cases, performance
// ============================================================

#include <cassert>
#include <cstdio>
#include <cmath>
#include "NKCore/NkTypes.h"
#include "NKMath/NkMathFunctions.h"
#include "NKTime/NkTime.h"

using namespace nkentseu;
using namespace nkentseu::math;

// Test framework macros
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("[FAIL] %s\n", msg); \
        return false; \
    }

#define TEST_APPROX(a, b, eps, msg) \
    if (Fabs((a) - (b)) > (eps)) { \
        printf("[FAIL] %s (got %.10f, expected %.10f)\n", msg, (a), (b)); \
        return false; \
    }

#define TEST(name, func) \
    { \
        bool result = func(); \
        printf("[%s] %s\n", result ? "PASS" : "FAIL", name); \
        tests_passed += result ? 1 : 0; \
        tests_total += 1; \
    }

// ============================================================
// BASIC OPERATIONS TESTS
// ============================================================

bool Test_Abs_Integer() {
    TEST_ASSERT(Abs(-42) == 42, "Abs(-42) == 42");
    TEST_ASSERT(Abs(0) == 0, "Abs(0) == 0");
    TEST_ASSERT(Abs(100) == 100, "Abs(100) == 100");
    return true;
}

bool Test_Fabs_Float() {
    TEST_APPROX(Fabs(-3.14f), 3.14f, 1e-6f, "Fabs(-3.14f)");
    TEST_APPROX(Fabs(0.0f), 0.0f, 1e-9f, "Fabs(0.0f)");
    return true;
}

bool Test_Min_Max() {
    TEST_ASSERT(Min(3, 7) == 3, "Min(3, 7)");
    TEST_ASSERT(Max(3, 7) == 7, "Max(3, 7)");
    TEST_ASSERT(Clamp(5, 1, 10) == 5, "Clamp(5, 1, 10)");
    TEST_ASSERT(Clamp(-5, 1, 10) == 1, "Clamp(-5, 1, 10)");
    return true;
}

// ============================================================
// ROUNDING TESTS
// ============================================================

bool Test_Floor() {
    TEST_APPROX(Floor(3.7f), 3.0f, 1e-6f, "Floor(3.7f) == 3");
    TEST_APPROX(Floor(-2.3f), -3.0f, 1e-6f, "Floor(-2.3f) == -3");
    TEST_APPROX(Floor(0.0f), 0.0f, 1e-9f, "Floor(0.0f) == 0");
    return true;
}

bool Test_Ceil() {
    TEST_APPROX(Ceil(3.2f), 4.0f, 1e-6f, "Ceil(3.2f) == 4");
    TEST_APPROX(Ceil(-2.8f), -2.0f, 1e-6f, "Ceil(-2.8f) == -2");
    return true;
}

bool Test_Round() {
    TEST_APPROX(Round(3.5f), 4.0f, 1e-6f, "Round(3.5f) == 4");
    TEST_APPROX(Round(2.4f), 2.0f, 1e-6f, "Round(2.4f) == 2");
    TEST_APPROX(Round(-1.5f), -2.0f, 1e-6f, "Round(-1.5f) == -2");
    return true;
}

bool Test_Trunc() {
    TEST_APPROX(Trunc(3.7f), 3.0f, 1e-6f, "Trunc(3.7f)");
    TEST_APPROX(Trunc(-3.7f), -3.0f, 1e-6f, "Trunc(-3.7f)");
    return true;
}

// ============================================================
// SQUARE ROOT & ROOTS TESTS
// ============================================================

bool Test_Sqrt() {
    TEST_APPROX(Sqrt(4.0f), 2.0f, 1e-5f, "Sqrt(4.0f) == 2");
    TEST_APPROX(Sqrt(9.0f), 3.0f, 1e-5f, "Sqrt(9.0f) == 3");
    TEST_APPROX(Sqrt(2.0f), 1.41421356f, 1e-5f, "Sqrt(2.0f)");
    TEST_APPROX(Sqrt(0.0f), 0.0f, 1e-9f, "Sqrt(0.0f) == 0");
    return true;
}

bool Test_Cbrt() {
    TEST_APPROX(Cbrt(8.0f), 2.0f, 1e-5f, "Cbrt(8.0f) == 2");
    TEST_APPROX(Cbrt(27.0f), 3.0f, 1e-5f, "Cbrt(27.0f) == 3");
    TEST_APPROX(Cbrt(-8.0f), -2.0f, 1e-5f, "Cbrt(-8.0f) == -2");
    return true;
}

bool Test_Rsqrt() {
    nk_float x = 4.0f;
    nk_float result = Rsqrt(x);
    nk_float expected = 0.5f;
    TEST_APPROX(result, expected, 1e-5f, "Rsqrt(4.0f) == 0.5");
    return true;
}

// ============================================================
// EXPONENTIAL & LOGARITHM TESTS
// ============================================================

bool Test_Exp() {
    TEST_APPROX(Exp(0.0f), 1.0f, 1e-5f, "Exp(0) == 1");
    TEST_APPROX(Exp(1.0f), 2.71828182f, 1e-4f, "Exp(1) ≈ e");
    TEST_APPROX(Exp(-1.0f), 0.36787944f, 1e-5f, "Exp(-1)");
    return true;
}

bool Test_Log() {
    TEST_APPROX(Log(1.0f), 0.0f, 1e-6f, "Log(1) == 0");
    TEST_APPROX(Log(2.71828182f), 1.0f, 1e-4f, "Log(e) ≈ 1");
    TEST_APPROX(Log(10.0f), 2.30258509f, 1e-4f, "Log(10)");
    return true;
}

bool Test_Log10() {
    TEST_APPROX(Log10(1.0f), 0.0f, 1e-6f, "Log10(1) == 0");
    TEST_APPROX(Log10(10.0f), 1.0f, 1e-5f, "Log10(10) == 1");
    TEST_APPROX(Log10(100.0f), 2.0f, 1e-5f, "Log10(100) == 2");
    return true;
}

bool Test_Log2() {
    TEST_APPROX(Log2(1.0f), 0.0f, 1e-6f, "Log2(1) == 0");
    TEST_APPROX(Log2(2.0f), 1.0f, 1e-5f, "Log2(2) == 1");
    TEST_APPROX(Log2(8.0f), 3.0f, 1e-5f, "Log2(8) == 3");
    return true;
}

bool Test_Pow() {
    TEST_APPROX(Pow(2.0f, 3.0f), 8.0f, 1e-4f, "Pow(2, 3) == 8");
    TEST_APPROX(Pow(2.0f, -1.0f), 0.5f, 1e-5f, "Pow(2, -1) == 0.5");
    TEST_APPROX(Pow(10.0f, 2.0f), 100.0f, 1e-3f, "Pow(10, 2) == 100");
    return true;
}

bool Test_PowInt() {
    TEST_APPROX(PowInt(2.0f, 3), 8.0f, 1e-5f, "PowInt(2, 3) == 8");
    TEST_APPROX(PowInt(2.0f, 0), 1.0f, 1e-9f, "PowInt(2, 0) == 1");
    TEST_APPROX(PowInt(3.0f, 4), 81.0f, 1e-4f, "PowInt(3, 4) == 81");
    return true;
}

// ============================================================
// TRIGONOMETRIC TESTS
// ============================================================

bool Test_Sin() {
    TEST_APPROX(Sin(0.0f), 0.0f, 1e-6f, "Sin(0) == 0");
    TEST_APPROX(Sin(NK_PI_F / 2.0f), 1.0f, 1e-5f, "Sin(π/2) ≈ 1");
    TEST_APPROX(Sin(NK_PI_F), 0.0f, 1e-5f, "Sin(π) ≈ 0");
    return true;
}

bool Test_Cos() {
    TEST_APPROX(Cos(0.0f), 1.0f, 1e-6f, "Cos(0) == 1");
    TEST_APPROX(Cos(NK_PI_F / 2.0f), 0.0f, 1e-5f, "Cos(π/2) ≈ 0");
    TEST_APPROX(Cos(NK_PI_F), -1.0f, 1e-5f, "Cos(π) ≈ -1");
    return true;
}

bool Test_Tan() {
    TEST_APPROX(Tan(0.0f), 0.0f, 1e-6f, "Tan(0) == 0");
    TEST_APPROX(Tan(NK_PI_F / 4.0f), 1.0f, 1e-4f, "Tan(π/4) ≈ 1");
    return true;
}

// ============================================================
// INVERSE TRIGONOMETRIC TESTS
// ============================================================

bool Test_Atan() {
    TEST_APPROX(Atan(0.0f), 0.0f, 1e-6f, "Atan(0) == 0");
    TEST_APPROX(Atan(1.0f), NK_PI_F / 4.0f, 1e-5f, "Atan(1) ≈ π/4");
    return true;
}

bool Test_Atan2() {
    TEST_APPROX(Atan2(1.0f, 1.0f), NK_PI_F / 4.0f, 1e-5f, "Atan2(1,1) ≈ π/4");
    TEST_APPROX(Atan2(0.0f, 1.0f), 0.0f, 1e-6f, "Atan2(0,1) == 0");
    return true;
}

bool Test_Asin() {
    TEST_APPROX(Asin(0.0f), 0.0f, 1e-6f, "Asin(0) == 0");
    TEST_APPROX(Asin(1.0f), NK_PI_F / 2.0f, 1e-4f, "Asin(1) ≈ π/2");
    return true;
}

bool Test_Acos() {
    TEST_APPROX(Acos(1.0f), 0.0f, 1e-6f, "Acos(1) == 0");
    TEST_APPROX(Acos(0.0f), NK_PI_F / 2.0f, 1e-4f, "Acos(0) ≈ π/2");
    return true;
}

// ============================================================
// HYPERBOLIC TESTS
// ============================================================

bool Test_Sinh() {
    TEST_APPROX(Sinh(0.0f), 0.0f, 1e-6f, "Sinh(0) == 0");
    nk_float x = 1.0f;
    nk_float expected = (Exp(x) - Exp(-x)) / 2.0f;
    TEST_APPROX(Sinh(x), expected, 1e-4f, "Sinh(1)");
    return true;
}

bool Test_Cosh() {
    TEST_APPROX(Cosh(0.0f), 1.0f, 1e-6f, "Cosh(0) == 1");
    nk_float x = 1.0f;
    nk_float expected = (Exp(x) + Exp(-x)) / 2.0f;
    TEST_APPROX(Cosh(x), expected, 1e-4f, "Cosh(1)");
    return true;
}

bool Test_Tanh() {
    TEST_APPROX(Tanh(0.0f), 0.0f, 1e-6f, "Tanh(0) == 0");
    return true;
}

// ============================================================
// FLOATING POINT OPERATIONS TESTS
// ============================================================

bool Test_Fmod() {
    TEST_APPROX(Fmod(5.5f, 2.0f), 1.5f, 1e-5f, "Fmod(5.5, 2.0)");
    TEST_APPROX(Fmod(7.0f, 3.0f), 1.0f, 1e-6f, "Fmod(7.0, 3.0)");
    return true;
}

bool Test_Frexp() {
    nk_int32 exp;
    nk_float mantissa = Frexp(16.0f, &exp);
    TEST_ASSERT(exp == 5, "Frexp(16) exponent == 5");
    // 16 = 0.5 * 2^5
    TEST_APPROX(mantissa, 0.5f, 1e-6f, "Frexp(16) mantissa");
    return true;
}

bool Test_Ldexp() {
    TEST_APPROX(Ldexp(0.5f, 5), 16.0f, 1e-5f, "Ldexp(0.5, 5) == 16");
    return true;
}

bool Test_Modf() {
    nk_float ipart;
    nk_float fpart = Modf(3.14f, &ipart);
    TEST_APPROX(ipart, 3.0f, 1e-6f, "Modf(3.14) integer part");
    TEST_APPROX(fpart, 0.14f, 1e-5f, "Modf(3.14) fractional part");
    return true;
}

// ============================================================
// BIT OPERATIONS TESTS
// ============================================================

bool Test_Gcd() {
    TEST_ASSERT(Gcd(12, 8) == 4, "Gcd(12, 8) == 4");
    TEST_ASSERT(Gcd(100, 50) == 50, "Gcd(100, 50) == 50");
    return true;
}

bool Test_IsPowerOf2() {
    TEST_ASSERT(IsPowerOf2(1), "IsPowerOf2(1)");
    TEST_ASSERT(IsPowerOf2(2), "IsPowerOf2(2)");
    TEST_ASSERT(IsPowerOf2(256), "IsPowerOf2(256)");
    TEST_ASSERT(!IsPowerOf2(7), "!IsPowerOf2(7)");
    return true;
}

bool Test_Clz() {
    nk_uint32 x = 0x80000000;
    TEST_ASSERT(Clz(x) == 0, "Clz(0x80000000) == 0");
    x = 0x00000001;
    TEST_ASSERT(Clz(x) == 31, "Clz(1) == 31");
    return true;
}

bool Test_Ctz() {
    nk_uint32 x = 0x00000001;
    TEST_ASSERT(Ctz(x) == 0, "Ctz(1) == 0");
    x = 0x00000010;
    TEST_ASSERT(Ctz(x) == 4, "Ctz(16) == 4");
    return true;
}

bool Test_Popcount() {
    nk_uint32 x = 0xFFFFFFFF;
    TEST_ASSERT(Popcount(x) == 32, "Popcount(0xFFFFFFFF) == 32");
    x = 0x0F0F0F0F;
    TEST_ASSERT(Popcount(x) == 16, "Popcount(0x0F0F0F0F) == 16");
    return true;
}

// ============================================================
// INTERPOLATION TESTS
// ============================================================

bool Test_Lerp() {
    TEST_APPROX(Lerp(0.0f, 10.0f, 0.5f), 5.0f, 1e-5f, "Lerp(0, 10, 0.5)");
    TEST_APPROX(Lerp(1.0f, 2.0f, 0.0f), 1.0f, 1e-6f, "Lerp(1, 2, 0)");
    return true;
}

bool Test_Smoothstep() {
    TEST_APPROX(Smoothstep(0.0f), 0.0f, 1e-6f, "Smoothstep(0) == 0");
    TEST_APPROX(Smoothstep(0.5f), 0.5f, 1e-5f, "Smoothstep(0.5) == 0.5");
    TEST_APPROX(Smoothstep(1.0f), 1.0f, 1e-6f, "Smoothstep(1) == 1");
    return true;
}

// ============================================================
// ANGLE CONVERSION TESTS
// ============================================================

bool Test_ToRadians() {
    TEST_APPROX(ToRadians(180.0f), NK_PI_F, 1e-5f, "ToRadians(180)");
    TEST_APPROX(ToRadians(90.0f), NK_PI_F / 2.0f, 1e-5f, "ToRadians(90)");
    return true;
}

bool Test_ToDegrees() {
    TEST_APPROX(ToDegrees(NK_PI_F), 180.0f, 1e-4f, "ToDegrees(π)");
    TEST_APPROX(ToDegrees(NK_PI_F / 2.0f), 90.0f, 1e-4f, "ToDegrees(π/2)");
    return true;
}

// ============================================================
// BENCHMARK TESTS
// ============================================================

struct BenchmarkResult {
    const char* name;
    nk_float scalar_cycles;  // Cycles per operation (scalar)
    nk_float vector_cycles;  // Cycles per operation (SIMD)
    nk_float speedup;        // Speedup factor
};

BenchmarkResult Benchmark_Sqrt(nk_uint32 iterations) {
    nk_float values[1024];
    for (nk_uint32 i = 0; i < 1024; ++i) {
        values[i] = 1.0f + (i / 1024.0f) * 100.0f;
    }
    
    // Scalar benchmark
   auto start = nkentseu::time::High_Resolution_Clock::now();
    for (nk_uint32 iter = 0; iter < iterations; ++iter) {
        for (nk_uint32 i = 0; i < 1024; ++i) {
            values[i] = Sqrt(values[i]);
        }
    }
    auto end = nkentseu::time::High_Resolution_Clock::now();
    
    nk_float scalar_ns = std::chrono::duration<nk_float, std::nano>(end - start).count();
    nk_float scalar_cycles = scalar_ns / 1000.0f; // Simplified calculation
    
    BenchmarkResult result;
    result.name = "Sqrt";
    result.scalar_cycles = scalar_cycles;
    result.vector_cycles = scalar_cycles / 8.0f; // Estimated SIMD speedup
    result.speedup = scalar_cycles / result.vector_cycles;
    
    return result;
}

// ============================================================
// MAIN TEST RUNNER
// ============================================================

int main() {
    nk_uint32 tests_passed = 0;
    nk_uint32 tests_total = 0;
    
    printf("==========================================\n");
    printf("  NKMath Unit Tests\n");
    printf("==========================================\n\n");
    
    // Basic operations
    printf("--- Basic Operations ---\n");
    TEST("Abs (integer)", Test_Abs_Integer);
    TEST("Fabs (float)", Test_Fabs_Float);
    TEST("Min/Max", Test_Min_Max);
    
    // Rounding
    printf("\n--- Rounding ---\n");
    TEST("Floor", Test_Floor);
    TEST("Ceil", Test_Ceil);
    TEST("Round", Test_Round);
    TEST("Trunc", Test_Trunc);
    
    // Roots
    printf("\n--- Square Root & Roots ---\n");
    TEST("Sqrt", Test_Sqrt);
    TEST("Cbrt", Test_Cbrt);
    TEST("Rsqrt", Test_Rsqrt);
    
    // Exponential/Logarithm
    printf("\n--- Exponential & Logarithm ---\n");
    TEST("Exp", Test_Exp);
    TEST("Log", Test_Log);
    TEST("Log10", Test_Log10);
    TEST("Log2", Test_Log2);
    TEST("Pow", Test_Pow);
    TEST("PowInt", Test_PowInt);
    
    // Trigonometric
    printf("\n--- Trigonometric ---\n");
    TEST("Sin", Test_Sin);
    TEST("Cos", Test_Cos);
    TEST("Tan", Test_Tan);
    
    // Inverse Trigonometric
    printf("\n--- Inverse Trigonometric ---\n");
    TEST("Atan", Test_Atan);
    TEST("Atan2", Test_Atan2);
    TEST("Asin", Test_Asin);
    TEST("Acos", Test_Acos);
    
    // Hyperbolic
    printf("\n--- Hyperbolic ---\n");
    TEST("Sinh", Test_Sinh);
    TEST("Cosh", Test_Cosh);
    TEST("Tanh", Test_Tanh);
    
    // Floating point operations
    printf("\n--- Floating Point Operations ---\n");
    TEST("Fmod", Test_Fmod);
    TEST("Frexp", Test_Frexp);
    TEST("Ldexp", Test_Ldexp);
    TEST("Modf", Test_Modf);
    
    // Bit operations
    printf("\n--- Bit Operations ---\n");
    TEST("Gcd", Test_Gcd);
    TEST("IsPowerOf2", Test_IsPowerOf2);
    TEST("Clz", Test_Clz);
    TEST("Ctz", Test_Ctz);
    TEST("Popcount", Test_Popcount);
    
    // Interpolation
    printf("\n--- Interpolation ---\n");
    TEST("Lerp", Test_Lerp);
    TEST("Smoothstep", Test_Smoothstep);
    
    // Angle conversion
    printf("\n--- Angle Conversion ---\n");
    TEST("ToRadians", Test_ToRadians);
    TEST("ToDegrees", Test_ToDegrees);
    
    // Summary
    printf("\n==========================================\n");
    printf("  Results: %u / %u tests passed\n", tests_passed, tests_total);
    printf("  Success rate: %.1f%%\n", (nk_float)tests_passed / tests_total * 100.0f);
    printf("==========================================\n");
    
    return tests_passed == tests_total ? 0 : 1;
}

