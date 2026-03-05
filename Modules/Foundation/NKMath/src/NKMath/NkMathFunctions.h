// ============================================================
// FILE: NkMathFunctions.h
// DESCRIPTION: Core scalar math API for Nkentseu
// ============================================================
// Design goals:
// - Keep the existing NKMath.cpp API stable
// - Remove duplicated constants
// - Provide clearer naming groups (constants, angle helpers)
// - Minimize dependencies (no STL containers/algorithms)
// ============================================================

#pragma once

#ifndef NKENTSEU_NKMATH_NKMATHFUNCTIONS_H_INCLUDED
#define NKENTSEU_NKMATH_NKMATHFUNCTIONS_H_INCLUDED

#include "NKCore/NkTypes.h"

namespace nkentseu {
namespace math {

#ifndef NK_FORCE_INLINE
#   define NK_FORCE_INLINE inline
#endif

    // -------------------------------------------------------------------------
    // Scalar aliases expected by NKMath.cpp
    // -------------------------------------------------------------------------
    using nk_float  = float32;
    using nk_double = float64;

    // -------------------------------------------------------------------------
    // Constants (canonical names)
    // -------------------------------------------------------------------------
    namespace constants {
        // Math
        constexpr nk_float  kPiF        = 3.14159265358979323846f;
        constexpr nk_double kPi         = 3.14159265358979323846;
        constexpr nk_float  kHalfPiF    = 1.57079632679489661923f;
        constexpr nk_double kHalfPi     = 1.57079632679489661923;
        constexpr nk_float  kQuarterPiF = 0.78539816339744830961f;
        constexpr nk_double kQuarterPi  = 0.78539816339744830961;
        constexpr nk_float  kTauF       = 6.28318530717958647692f;
        constexpr nk_double kTau        = 6.28318530717958647692;

        constexpr nk_float  kInvPiF     = 0.31830988618379067154f;
        constexpr nk_double kInvPi      = 0.31830988618379067154;
        constexpr nk_float  kEpsilonF   = 1.0e-6f;
        constexpr nk_double kEpsilonD   = 1.0e-15;

        constexpr nk_float  kEF         = 2.71828182845904523536f;
        constexpr nk_double kE          = 2.71828182845904523536;
        constexpr nk_float  kLn2F       = 0.69314718055994530942f;
        constexpr nk_double kLn2        = 0.69314718055994530942;
        constexpr nk_float  kLn10F      = 2.30258509299404568402f;
        constexpr nk_double kLn10       = 2.30258509299404568402;
        constexpr nk_float  kSqrt2F     = 1.41421356237309504880f;
        constexpr nk_double kSqrt2      = 1.41421356237309504880;
        constexpr nk_float  kSqrt3F     = 1.73205080756887729353f;
        constexpr nk_double kSqrt3      = 1.73205080756887729353;

        // Physics/Chemistry
        constexpr nk_double kGravity      = 9.80665;
        constexpr nk_double kLightSpeed   = 299792458.0;
        constexpr nk_double kGravConstant = 6.67430e-11;
        constexpr nk_double kPlanck       = 6.62607015e-34;
        constexpr nk_double kBoltzmann    = 1.380649e-23;
        constexpr nk_double kAvogadro     = 6.02214076e23;
        constexpr nk_double kGasConstant  = 8.314462618;
    } // namespace constants

    // -------------------------------------------------------------------------
    // Compatibility constants (legacy names used in NKMath.cpp and older code)
    // -------------------------------------------------------------------------
    constexpr nk_float  NK_PI_F     = constants::kPiF;
    constexpr nk_double NK_PI_D     = constants::kPi;
    constexpr nk_float  NK_PI_2_F   = constants::kHalfPiF;
    constexpr nk_double NK_PI_2_D   = constants::kHalfPi;
    constexpr nk_float  NK_PI_4_F   = constants::kQuarterPiF;
    constexpr nk_double NK_PI_4_D   = constants::kQuarterPi;
    constexpr nk_float  NK_2PI_F    = constants::kTauF;
    constexpr nk_double NK_2PI_D    = constants::kTau;
    constexpr nk_float  NK_INV_PI_F = constants::kInvPiF;
    constexpr nk_double NK_INV_PI_D = constants::kInvPi;
    constexpr nk_float  NK_E_F      = constants::kEF;
    constexpr nk_double NK_E_D      = constants::kE;
    constexpr nk_float  NK_LN2_F    = constants::kLn2F;
    constexpr nk_double NK_LN2_D    = constants::kLn2;
    constexpr nk_float  NK_LN10_F   = constants::kLn10F;
    constexpr nk_double NK_LN10_D   = constants::kLn10;

    constexpr nk_float  PI_F        = constants::kPiF;
    constexpr nk_double PI          = constants::kPi;

    // Existing exported physics constants
    constexpr nk_double NK_C        = constants::kLightSpeed;
    constexpr nk_double NK_G        = constants::kGravConstant;
    constexpr nk_double NK_H        = constants::kPlanck;
    constexpr nk_double NK_K_B      = constants::kBoltzmann;
    constexpr nk_double NK_NA       = constants::kAvogadro;
    constexpr nk_double NK_R        = constants::kGasConstant;
    constexpr nk_double NK_G_ACCEL  = constants::kGravity;

    // -------------------------------------------------------------------------
    // Angle helpers
    // -------------------------------------------------------------------------
    constexpr nk_float  DEG_TO_RAD_F = constants::kPiF / 180.0f;
    constexpr nk_double DEG_TO_RAD   = constants::kPi  / 180.0;
    constexpr nk_float  RAD_TO_DEG_F = 180.0f / constants::kPiF;
    constexpr nk_double RAD_TO_DEG   = 180.0  / constants::kPi;

    NK_FORCE_INLINE nk_float RadiansFromDegrees(nk_float degrees) noexcept {
        return degrees * DEG_TO_RAD_F;
    }
    NK_FORCE_INLINE nk_double RadiansFromDegrees(nk_double degrees) noexcept {
        return degrees * DEG_TO_RAD;
    }
    NK_FORCE_INLINE nk_float DegreesFromRadians(nk_float radians) noexcept {
        return radians * RAD_TO_DEG_F;
    }
    NK_FORCE_INLINE nk_double DegreesFromRadians(nk_double radians) noexcept {
        return radians * RAD_TO_DEG;
    }

    // Legacy aliases
    NK_FORCE_INLINE nk_float ToRadians(nk_float degrees) noexcept {
        return RadiansFromDegrees(degrees);
    }
    NK_FORCE_INLINE nk_double ToRadians(nk_double degrees) noexcept {
        return RadiansFromDegrees(degrees);
    }
    NK_FORCE_INLINE nk_float ToDegrees(nk_float radians) noexcept {
        return DegreesFromRadians(radians);
    }
    NK_FORCE_INLINE nk_double ToDegrees(nk_double radians) noexcept {
        return DegreesFromRadians(radians);
    }

    // -------------------------------------------------------------------------
    // Basics
    // -------------------------------------------------------------------------
    NK_FORCE_INLINE nk_int32 Abs(nk_int32 v) noexcept { return v < 0 ? -v : v; }
    NK_FORCE_INLINE nk_int64 Abs(nk_int64 v) noexcept { return v < 0 ? -v : v; }
    NK_FORCE_INLINE nk_float Fabs(nk_float v) noexcept { return v < 0.0f ? -v : v; }
    NK_FORCE_INLINE nk_double Fabs(nk_double v) noexcept { return v < 0.0 ? -v : v; }

    template <typename T>
    NK_FORCE_INLINE T Min(T a, T b) noexcept { return (a < b) ? a : b; }

    template <typename T>
    NK_FORCE_INLINE T Max(T a, T b) noexcept { return (a > b) ? a : b; }

    template <typename T>
    NK_FORCE_INLINE T Clamp(T v, T lo, T hi) noexcept {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    template <typename T>
    NK_FORCE_INLINE T Saturate(T v) noexcept {
        return Clamp<T>(v, static_cast<T>(0), static_cast<T>(1));
    }

    // -------------------------------------------------------------------------
    // Rounding
    // -------------------------------------------------------------------------
    nk_float Floor(nk_float x) noexcept;
    nk_double Floor(nk_double x) noexcept;
    nk_float Ceil(nk_float x) noexcept;
    nk_double Ceil(nk_double x) noexcept;
    nk_float Trunc(nk_float x) noexcept;
    nk_double Trunc(nk_double x) noexcept;
    nk_float Round(nk_float x) noexcept;
    nk_double Round(nk_double x) noexcept;

    // -------------------------------------------------------------------------
    // Roots / exponent / logs
    // -------------------------------------------------------------------------
    nk_float Sqrt(nk_float x) noexcept;
    nk_double Sqrt(nk_double x) noexcept;
    nk_float Rsqrt(nk_float x) noexcept;
    nk_double Rsqrt(nk_double x) noexcept;
    nk_float Cbrt(nk_float x) noexcept;
    nk_double Cbrt(nk_double x) noexcept;

    nk_float Exp(nk_float x) noexcept;
    nk_double Exp(nk_double x) noexcept;
    nk_float Log(nk_float x) noexcept;
    nk_double Log(nk_double x) noexcept;

    NK_FORCE_INLINE nk_float Log10(nk_float x) noexcept { return Log(x) / NK_LN10_F; }
    NK_FORCE_INLINE nk_double Log10(nk_double x) noexcept { return Log(x) / NK_LN10_D; }
    NK_FORCE_INLINE nk_float Log2(nk_float x) noexcept { return Log(x) / NK_LN2_F; }
    NK_FORCE_INLINE nk_double Log2(nk_double x) noexcept { return Log(x) / NK_LN2_D; }

    nk_float Pow(nk_float x, nk_float y) noexcept;
    nk_double Pow(nk_double x, nk_double y) noexcept;
    nk_float PowInt(nk_float x, nk_int32 n) noexcept;
    nk_double PowInt(nk_double x, nk_int32 n) noexcept;

    // -------------------------------------------------------------------------
    // Trigonometry
    // -------------------------------------------------------------------------
    nk_float Sin(nk_float x) noexcept;
    nk_double Sin(nk_double x) noexcept;
    nk_float Cos(nk_float x) noexcept;
    nk_double Cos(nk_double x) noexcept;

    NK_FORCE_INLINE nk_float Tan(nk_float x) noexcept {
        const nk_float c = Cos(x);
        return c != 0.0f ? Sin(x) / c : 0.0f;
    }
    NK_FORCE_INLINE nk_double Tan(nk_double x) noexcept {
        const nk_double c = Cos(x);
        return c != 0.0 ? Sin(x) / c : 0.0;
    }

    nk_float Atan(nk_float x) noexcept;
    nk_double Atan(nk_double x) noexcept;
    nk_float Atan2(nk_float y, nk_float x) noexcept;
    nk_double Atan2(nk_double y, nk_double x) noexcept;
    nk_float Asin(nk_float a) noexcept;
    nk_double Asin(nk_double a) noexcept;
    nk_float Acos(nk_float a) noexcept;
    nk_double Acos(nk_double a) noexcept;

    // -------------------------------------------------------------------------
    // Hyperbolic
    // -------------------------------------------------------------------------
    nk_float Sinh(nk_float x) noexcept;
    nk_double Sinh(nk_double x) noexcept;
    nk_float Cosh(nk_float x) noexcept;
    nk_double Cosh(nk_double x) noexcept;
    nk_float Tanh(nk_float x) noexcept;
    nk_double Tanh(nk_double x) noexcept;

    // -------------------------------------------------------------------------
    // Floating-point utils
    // -------------------------------------------------------------------------
    nk_float Fmod(nk_float x, nk_float y) noexcept;
    nk_double Fmod(nk_double x, nk_double y) noexcept;
    nk_float Frexp(nk_float x, nk_int32* exp) noexcept;
    nk_double Frexp(nk_double x, nk_int32* exp) noexcept;
    nk_float Ldexp(nk_float x, nk_int32 exp) noexcept;
    nk_double Ldexp(nk_double x, nk_int32 exp) noexcept;
    nk_float Modf(nk_float x, nk_float* iptr) noexcept;
    nk_double Modf(nk_double x, nk_double* iptr) noexcept;

    struct Ldiv {
        nk_int64 quot;
        nk_int64 rem;
    };
    using DivResult64 = Ldiv;

    DivResult64 DivI64(nk_int64 numerator, nk_int64 denominator) noexcept;

    // Proposed clearer alias
    NK_FORCE_INLINE DivResult64 DivMod64(nk_int64 numerator, nk_int64 denominator) noexcept {
        return DivI64(numerator, denominator);
    }

    // -------------------------------------------------------------------------
    // Integer/bit utils
    // -------------------------------------------------------------------------
    nk_uint64 Gcd(nk_uint64 a, nk_uint64 b) noexcept;

    NK_FORCE_INLINE nk_uint64 Lcm(nk_uint64 a, nk_uint64 b) noexcept {
        return (a / Gcd(a, b)) * b;
    }

    NK_FORCE_INLINE nk_bool IsPowerOf2(nk_uint64 x) noexcept {
        return x && !(x & (x - 1));
    }

    nk_uint64 NextPowerOf2(nk_uint64 x) noexcept;
    nk_uint32 Clz(nk_uint32 x) noexcept;
    nk_uint32 Clz(nk_uint64 x) noexcept;
    nk_uint32 Ctz(nk_uint32 x) noexcept;
    nk_uint32 Ctz(nk_uint64 x) noexcept;
    nk_uint32 Popcount(nk_uint32 x) noexcept;
    nk_uint32 Popcount(nk_uint64 x) noexcept;

    // -------------------------------------------------------------------------
    // Interpolation
    // -------------------------------------------------------------------------
    template <typename T>
    NK_FORCE_INLINE T Lerp(T a, T b, nk_float t) noexcept {
        return a + (b - a) * t;
    }

    // Proposed clearer alias
    template <typename T>
    NK_FORCE_INLINE T Mix(T a, T b, nk_float t) noexcept {
        return Lerp(a, b, t);
    }

    nk_float Smoothstep(nk_float t) noexcept;
    nk_double Smoothstep(nk_double t) noexcept;
    nk_float Smootherstep(nk_float t) noexcept;
    nk_double Smootherstep(nk_double t) noexcept;

} // namespace math
} // namespace nkentseu

#endif // NKENTSEU_NKMATH_NKMATHFUNCTIONS_H_INCLUDED
