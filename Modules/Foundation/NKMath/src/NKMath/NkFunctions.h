// ============================================================
// FILE: NkFunctions.h
// DESCRIPTION: Core scalar math API for Nkentseu
// ============================================================
// Design goals:
// - Keep the existing API stable while enforcing Nk* naming
// - Remove duplicated constants
// - Provide clearer naming groups (constants, angle helpers)
// - Minimize dependencies (no STL containers/algorithms)
// ============================================================

#pragma once

#ifndef NKENTSEU_NKMATH_NKFUNCTIONS_H_INCLUDED
#define NKENTSEU_NKMATH_NKFUNCTIONS_H_INCLUDED

#include "NKCore/NkTypes.h"

// NkMacros.h may define legacy function-like macros (NkMin/NkMax/NkClamp).
// Undefine them here so NkMath keeps callable Nk* functions.
#ifdef NkMin
#undef NkMin
#endif
#ifdef NkMax
#undef NkMax
#endif
#ifdef NkClamp
#undef NkClamp
#endif
#ifdef NkAbs
#undef NkAbs
#endif

namespace nkentseu {
namespace math {

#ifndef NK_FORCE_INLINE
#   define NK_FORCE_INLINE inline
#endif

    // -------------------------------------------------------------------------
    // Scalar aliases expected by NkFunctions.cpp
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
    // Compatibility constants (legacy names used in older code)
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

    NK_FORCE_INLINE nk_float NkRadiansFromDegrees(nk_float degrees) noexcept {
        return degrees * DEG_TO_RAD_F;
    }
    NK_FORCE_INLINE nk_double NkRadiansFromDegrees(nk_double degrees) noexcept {
        return degrees * DEG_TO_RAD;
    }
    NK_FORCE_INLINE nk_float NkDegreesFromRadians(nk_float radians) noexcept {
        return radians * RAD_TO_DEG_F;
    }
    NK_FORCE_INLINE nk_double NkDegreesFromRadians(nk_double radians) noexcept {
        return radians * RAD_TO_DEG;
    }

    // Legacy aliases
    NK_FORCE_INLINE nk_float NkToRadians(nk_float degrees) noexcept {
        return NkRadiansFromDegrees(degrees);
    }
    NK_FORCE_INLINE nk_double NkToRadians(nk_double degrees) noexcept {
        return NkRadiansFromDegrees(degrees);
    }
    NK_FORCE_INLINE nk_float NkToDegrees(nk_float radians) noexcept {
        return NkDegreesFromRadians(radians);
    }
    NK_FORCE_INLINE nk_double NkToDegrees(nk_double radians) noexcept {
        return NkDegreesFromRadians(radians);
    }

    // -------------------------------------------------------------------------
    // Basics
    // -------------------------------------------------------------------------
    NK_FORCE_INLINE nk_int32 NkAbs(nk_int32 v) noexcept { return v < 0 ? -v : v; }
    NK_FORCE_INLINE nk_int64 NkAbs(nk_int64 v) noexcept { return v < 0 ? -v : v; }
    NK_FORCE_INLINE nk_float NkFabs(nk_float v) noexcept { return v < 0.0f ? -v : v; }
    NK_FORCE_INLINE nk_double NkFabs(nk_double v) noexcept { return v < 0.0 ? -v : v; }

    template <typename T>
    NK_FORCE_INLINE T NkMin(T a, T b) noexcept { return (a < b) ? a : b; }

    template <typename T>
    NK_FORCE_INLINE T NkMax(T a, T b) noexcept { return (a > b) ? a : b; }

    template <typename T>
    NK_FORCE_INLINE T NkClamp(T v, T lo, T hi) noexcept {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    template <typename T>
    NK_FORCE_INLINE T NkSaturate(T v) noexcept {
        return NkClamp<T>(v, static_cast<T>(0), static_cast<T>(1));
    }

    // -------------------------------------------------------------------------
    // Rounding
    // -------------------------------------------------------------------------
    nk_float NkFloor(nk_float x) noexcept;
    nk_double NkFloor(nk_double x) noexcept;
    nk_float NkCeil(nk_float x) noexcept;
    nk_double NkCeil(nk_double x) noexcept;
    nk_float NkTrunc(nk_float x) noexcept;
    nk_double NkTrunc(nk_double x) noexcept;
    nk_float NkRound(nk_float x) noexcept;
    nk_double NkRound(nk_double x) noexcept;

    // -------------------------------------------------------------------------
    // Roots / exponent / logs
    // -------------------------------------------------------------------------
    nk_float NkSqrt(nk_float x) noexcept;
    nk_double NkSqrt(nk_double x) noexcept;
    nk_float NkRsqrt(nk_float x) noexcept;
    nk_double NkRsqrt(nk_double x) noexcept;
    nk_float NkCbrt(nk_float x) noexcept;
    nk_double NkCbrt(nk_double x) noexcept;

    nk_float NkExp(nk_float x) noexcept;
    nk_double NkExp(nk_double x) noexcept;
    nk_float NkLog(nk_float x) noexcept;
    nk_double NkLog(nk_double x) noexcept;

    NK_FORCE_INLINE nk_float NkLog10(nk_float x) noexcept { return NkLog(x) / NK_LN10_F; }
    NK_FORCE_INLINE nk_double NkLog10(nk_double x) noexcept { return NkLog(x) / NK_LN10_D; }
    NK_FORCE_INLINE nk_float NkLog2(nk_float x) noexcept { return NkLog(x) / NK_LN2_F; }
    NK_FORCE_INLINE nk_double NkLog2(nk_double x) noexcept { return NkLog(x) / NK_LN2_D; }

    nk_float NkPow(nk_float x, nk_float y) noexcept;
    nk_double NkPow(nk_double x, nk_double y) noexcept;
    nk_float NkPowInt(nk_float x, nk_int32 n) noexcept;
    nk_double NkPowInt(nk_double x, nk_int32 n) noexcept;

    // -------------------------------------------------------------------------
    // Trigonometry
    // -------------------------------------------------------------------------
    nk_float NkSin(nk_float x) noexcept;
    nk_double NkSin(nk_double x) noexcept;
    nk_float NkCos(nk_float x) noexcept;
    nk_double NkCos(nk_double x) noexcept;

    NK_FORCE_INLINE nk_float NkTan(nk_float x) noexcept {
        const nk_float c = NkCos(x);
        return c != 0.0f ? NkSin(x) / c : 0.0f;
    }
    NK_FORCE_INLINE nk_double NkTan(nk_double x) noexcept {
        const nk_double c = NkCos(x);
        return c != 0.0 ? NkSin(x) / c : 0.0;
    }

    nk_float NkAtan(nk_float x) noexcept;
    nk_double NkAtan(nk_double x) noexcept;
    nk_float NkAtan2(nk_float y, nk_float x) noexcept;
    nk_double NkAtan2(nk_double y, nk_double x) noexcept;
    nk_float NkAsin(nk_float a) noexcept;
    nk_double NkAsin(nk_double a) noexcept;
    nk_float NkAcos(nk_float a) noexcept;
    nk_double NkAcos(nk_double a) noexcept;

    // -------------------------------------------------------------------------
    // Hyperbolic
    // -------------------------------------------------------------------------
    nk_float NkSinh(nk_float x) noexcept;
    nk_double NkSinh(nk_double x) noexcept;
    nk_float NkCosh(nk_float x) noexcept;
    nk_double NkCosh(nk_double x) noexcept;
    nk_float NkTanh(nk_float x) noexcept;
    nk_double NkTanh(nk_double x) noexcept;

    // -------------------------------------------------------------------------
    // Floating-point utils
    // -------------------------------------------------------------------------
    nk_float NkFmod(nk_float x, nk_float y) noexcept;
    nk_double NkFmod(nk_double x, nk_double y) noexcept;
    NK_FORCE_INLINE nk_bool NkIsFinite(nk_float x) noexcept {
        union {
            nk_float value;
            nk_uint32 bits;
        } data = { x };
        return (data.bits & 0x7F800000u) != 0x7F800000u;
    }
    NK_FORCE_INLINE nk_bool NkIsFinite(nk_double x) noexcept {
        union {
            nk_double value;
            nk_uint64 bits;
        } data = { x };
        return (data.bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
    }
    nk_float NkFrexp(nk_float x, nk_int32* exp) noexcept;
    nk_double NkFrexp(nk_double x, nk_int32* exp) noexcept;
    nk_float NkLdexp(nk_float x, nk_int32 exp) noexcept;
    nk_double NkLdexp(nk_double x, nk_int32 exp) noexcept;
    nk_float NkModf(nk_float x, nk_float* iptr) noexcept;
    nk_double NkModf(nk_double x, nk_double* iptr) noexcept;

    struct Ldiv {
        nk_int64 quot;
        nk_int64 rem;
    };
    using DivResult64 = Ldiv;

    DivResult64 NkDivI64(nk_int64 numerator, nk_int64 denominator) noexcept;

    // Proposed clearer alias
    NK_FORCE_INLINE DivResult64 NkDivMod64(nk_int64 numerator, nk_int64 denominator) noexcept {
        return NkDivI64(numerator, denominator);
    }

    // -------------------------------------------------------------------------
    // Integer/bit utils
    // -------------------------------------------------------------------------
    nk_uint64 NkGcd(nk_uint64 a, nk_uint64 b) noexcept;

    NK_FORCE_INLINE nk_uint64 NkLcm(nk_uint64 a, nk_uint64 b) noexcept {
        return (a / NkGcd(a, b)) * b;
    }

    NK_FORCE_INLINE nk_bool NkIsPowerOf2(nk_uint64 x) noexcept {
        return x && !(x & (x - 1));
    }

    nk_uint64 NkNextPowerOf2(nk_uint64 x) noexcept;
    nk_uint32 NkClz(nk_uint32 x) noexcept;
    nk_uint32 NkClz(nk_uint64 x) noexcept;
    nk_uint32 NkCtz(nk_uint32 x) noexcept;
    nk_uint32 NkCtz(nk_uint64 x) noexcept;
    nk_uint32 NkPopcount(nk_uint32 x) noexcept;
    nk_uint32 NkPopcount(nk_uint64 x) noexcept;

    // -------------------------------------------------------------------------
    // Interpolation
    // -------------------------------------------------------------------------
    template <typename T>
    NK_FORCE_INLINE T NkLerp(T a, T b, nk_float t) noexcept {
        return a + (b - a) * t;
    }

    // Proposed clearer alias
    template <typename T>
    NK_FORCE_INLINE T NkMix(T a, T b, nk_float t) noexcept {
        return NkLerp(a, b, t);
    }

    nk_float NkSmoothstep(nk_float t) noexcept;
    nk_double NkSmoothstep(nk_double t) noexcept;
    nk_float NkSmootherstep(nk_float t) noexcept;
    nk_double NkSmootherstep(nk_double t) noexcept;

} // namespace math
} // namespace nkentseu

#endif // NKENTSEU_NKMATH_NKFUNCTIONS_H_INCLUDED
