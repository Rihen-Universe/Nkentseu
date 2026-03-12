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
        // Constants (canonical names)
        // -------------------------------------------------------------------------
        namespace constants {
            // Math
            constexpr float32  kPiF        = 3.14159265358979323846f;
            constexpr float64 kPi         = 3.14159265358979323846;
            constexpr float32  kHalfPiF    = 1.57079632679489661923f;
            constexpr float64 kHalfPi     = 1.57079632679489661923;
            constexpr float32  kQuarterPiF = 0.78539816339744830961f;
            constexpr float64 kQuarterPi  = 0.78539816339744830961;
            constexpr float32  kTauF       = 6.28318530717958647692f;
            constexpr float64 kTau        = 6.28318530717958647692;

            constexpr float32  kInvPiF     = 0.31830988618379067154f;
            constexpr float64 kInvPi      = 0.31830988618379067154;
            constexpr float32  kEpsilonF   = 1.0e-6f;
            constexpr float64 kEpsilonD   = 1.0e-15;
            constexpr float32  kCompareAbsEpsilonF = 1.0e-6f;
            constexpr float64 kCompareAbsEpsilonD = 1.0e-12;
            constexpr float32  kCompareRelEpsilonF = 1.0e-5f;
            constexpr float64 kCompareRelEpsilonD = 1.0e-12;

            constexpr float32  kEF         = 2.71828182845904523536f;
            constexpr float64 kE          = 2.71828182845904523536;
            constexpr float32  kLn2F       = 0.69314718055994530942f;
            constexpr float64 kLn2        = 0.69314718055994530942;
            constexpr float32  kLn10F      = 2.30258509299404568402f;
            constexpr float64 kLn10       = 2.30258509299404568402;
            constexpr float32  kSqrt2F     = 1.41421356237309504880f;
            constexpr float64 kSqrt2      = 1.41421356237309504880;
            constexpr float32  kSqrt3F     = 1.73205080756887729353f;
            constexpr float64 kSqrt3      = 1.73205080756887729353;

            // Physics/Chemistry
            constexpr float64 kGravity      = 9.80665;
            constexpr float64 kLightSpeed   = 299792458.0;
            constexpr float64 kGravConstant = 6.67430e-11;
            constexpr float64 kPlanck       = 6.62607015e-34;
            constexpr float64 kBoltzmann    = 1.380649e-23;
            constexpr float64 kAvogadro     = 6.02214076e23;
            constexpr float64 kGasConstant  = 8.314462618;
        } // namespace constants

        // -------------------------------------------------------------------------
        // Compatibility constants (legacy names used in older code)
        // -------------------------------------------------------------------------
        constexpr float32  NK_PI_F     = constants::kPiF;
        constexpr float64 NK_PI_D     = constants::kPi;
        constexpr float32  NK_PI_2_F   = constants::kHalfPiF;
        constexpr float64 NK_PI_2_D   = constants::kHalfPi;
        constexpr float32  NK_PI_4_F   = constants::kQuarterPiF;
        constexpr float64 NK_PI_4_D   = constants::kQuarterPi;
        constexpr float32  NK_2PI_F    = constants::kTauF;
        constexpr float64 NK_2PI_D    = constants::kTau;
        constexpr float32  NK_INV_PI_F = constants::kInvPiF;
        constexpr float64 NK_INV_PI_D = constants::kInvPi;
        constexpr float32  NK_E_F      = constants::kEF;
        constexpr float64 NK_E_D      = constants::kE;
        constexpr float32  NK_LN2_F    = constants::kLn2F;
        constexpr float64 NK_LN2_D    = constants::kLn2;
        constexpr float32  NK_LN10_F   = constants::kLn10F;
        constexpr float64 NK_LN10_D   = constants::kLn10;
        constexpr float32  NK_COMPARE_ABS_EPSILON_F = constants::kCompareAbsEpsilonF;
        constexpr float64 NK_COMPARE_ABS_EPSILON_D = constants::kCompareAbsEpsilonD;
        constexpr float32  NK_COMPARE_REL_EPSILON_F = constants::kCompareRelEpsilonF;
        constexpr float64 NK_COMPARE_REL_EPSILON_D = constants::kCompareRelEpsilonD;

        constexpr float32  PI_F        = constants::kPiF;
        constexpr float64 PI          = constants::kPi;

        // Existing exported physics constants
        constexpr float64 NK_C        = constants::kLightSpeed;
        constexpr float64 NK_G        = constants::kGravConstant;
        constexpr float64 NK_H        = constants::kPlanck;
        constexpr float64 NK_K_B      = constants::kBoltzmann;
        constexpr float64 NK_NA       = constants::kAvogadro;
        constexpr float64 NK_R        = constants::kGasConstant;
        constexpr float64 NK_G_ACCEL  = constants::kGravity;

        // -------------------------------------------------------------------------
        // Angle helpers
        // -------------------------------------------------------------------------
        constexpr float32  DEG_TO_RAD_F = constants::kPiF / 180.0f;
        constexpr float64 DEG_TO_RAD   = constants::kPi  / 180.0;
        constexpr float32  RAD_TO_DEG_F = 180.0f / constants::kPiF;
        constexpr float64 RAD_TO_DEG   = 180.0  / constants::kPi;

        NK_FORCE_INLINE float32 NkRadiansFromDegrees(float32 degrees) noexcept {
            return degrees * DEG_TO_RAD_F;
        }
        NK_FORCE_INLINE float64 NkRadiansFromDegrees(float64 degrees) noexcept {
            return degrees * DEG_TO_RAD;
        }
        NK_FORCE_INLINE float32 NkDegreesFromRadians(float32 radians) noexcept {
            return radians * RAD_TO_DEG_F;
        }
        NK_FORCE_INLINE float64 NkDegreesFromRadians(float64 radians) noexcept {
            return radians * RAD_TO_DEG;
        }

        // Legacy aliases
        NK_FORCE_INLINE float32 NkToRadians(float32 degrees) noexcept {
            return NkRadiansFromDegrees(degrees);
        }
        NK_FORCE_INLINE float64 NkToRadians(float64 degrees) noexcept {
            return NkRadiansFromDegrees(degrees);
        }
        NK_FORCE_INLINE float32 NkToDegrees(float32 radians) noexcept {
            return NkDegreesFromRadians(radians);
        }
        NK_FORCE_INLINE float64 NkToDegrees(float64 radians) noexcept {
            return NkDegreesFromRadians(radians);
        }

        // -------------------------------------------------------------------------
        // Basics
        // -------------------------------------------------------------------------
        NK_FORCE_INLINE int32 NkAbs(int32 v) noexcept { return v < 0 ? -v : v; }
        NK_FORCE_INLINE int64 NkAbs(int64 v) noexcept { return v < 0 ? -v : v; }
        NK_FORCE_INLINE float32 NkFabs(float32 v) noexcept { return v < 0.0f ? -v : v; }
        NK_FORCE_INLINE float64 NkFabs(float64 v) noexcept { return v < 0.0 ? -v : v; }

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

        NK_FORCE_INLINE nk_bool NkIsNearlyZero(
            float32 value,
            float32 absEpsilon = constants::kCompareAbsEpsilonF) noexcept
        {
            return NkFabs(value) <= absEpsilon;
        }

        NK_FORCE_INLINE nk_bool NkIsNearlyZero(
            float64 value,
            float64 absEpsilon = constants::kCompareAbsEpsilonD) noexcept
        {
            return NkFabs(value) <= absEpsilon;
        }

        NK_FORCE_INLINE nk_bool NkNearlyEqual(
            float32 a,
            float32 b,
            float32 absEpsilon = constants::kCompareAbsEpsilonF,
            float32 relEpsilon = constants::kCompareRelEpsilonF) noexcept
        {
            const float32 diff = NkFabs(a - b);
            if (diff <= absEpsilon) return true;
            const float32 scale = NkMax(NkFabs(a), NkFabs(b));
            return diff <= (scale * relEpsilon);
        }

        NK_FORCE_INLINE nk_bool NkNearlyEqual(
            float64 a,
            float64 b,
            float64 absEpsilon = constants::kCompareAbsEpsilonD,
            float64 relEpsilon = constants::kCompareRelEpsilonD) noexcept
        {
            const float64 diff = NkFabs(a - b);
            if (diff <= absEpsilon) return true;
            const float64 scale = NkMax(NkFabs(a), NkFabs(b));
            return diff <= (scale * relEpsilon);
        }

        // -------------------------------------------------------------------------
        // Rounding
        // -------------------------------------------------------------------------
        float32 NkFloor(float32 x) noexcept;
        float64 NkFloor(float64 x) noexcept;
        float32 NkCeil(float32 x) noexcept;
        float64 NkCeil(float64 x) noexcept;
        float32 NkTrunc(float32 x) noexcept;
        float64 NkTrunc(float64 x) noexcept;
        float32 NkRound(float32 x) noexcept;
        float64 NkRound(float64 x) noexcept;

        // -------------------------------------------------------------------------
        // Roots / exponent / logs
        // -------------------------------------------------------------------------
        float32 NkSqrt(float32 x) noexcept;
        float64 NkSqrt(float64 x) noexcept;
        float32 NkRsqrt(float32 x) noexcept;
        float64 NkRsqrt(float64 x) noexcept;
        float32 NkCbrt(float32 x) noexcept;
        float64 NkCbrt(float64 x) noexcept;

        float32 NkExp(float32 x) noexcept;
        float64 NkExp(float64 x) noexcept;
        float32 NkLog(float32 x) noexcept;
        float64 NkLog(float64 x) noexcept;

        NK_FORCE_INLINE float32 NkLog10(float32 x) noexcept { return NkLog(x) / NK_LN10_F; }
        NK_FORCE_INLINE float64 NkLog10(float64 x) noexcept { return NkLog(x) / NK_LN10_D; }
        NK_FORCE_INLINE float32 NkLog2(float32 x) noexcept { return NkLog(x) / NK_LN2_F; }
        NK_FORCE_INLINE float64 NkLog2(float64 x) noexcept { return NkLog(x) / NK_LN2_D; }

        float32 NkPow(float32 x, float32 y) noexcept;
        float64 NkPow(float64 x, float64 y) noexcept;
        float32 NkPowInt(float32 x, int32 n) noexcept;
        float64 NkPowInt(float64 x, int32 n) noexcept;

        // -------------------------------------------------------------------------
        // Trigonometry
        // -------------------------------------------------------------------------
        float32 NkSin(float32 x) noexcept;
        float64 NkSin(float64 x) noexcept;
        float32 NkCos(float32 x) noexcept;
        float64 NkCos(float64 x) noexcept;

        NK_FORCE_INLINE float32 NkTan(float32 x) noexcept {
            const float32 c = NkCos(x);
            return c != 0.0f ? NkSin(x) / c : 0.0f;
        }
        NK_FORCE_INLINE float64 NkTan(float64 x) noexcept {
            const float64 c = NkCos(x);
            return c != 0.0 ? NkSin(x) / c : 0.0;
        }

        float32 NkAtan(float32 x) noexcept;
        float64 NkAtan(float64 x) noexcept;
        float32 NkAtan2(float32 y, float32 x) noexcept;
        float64 NkAtan2(float64 y, float64 x) noexcept;
        float32 NkAsin(float32 a) noexcept;
        float64 NkAsin(float64 a) noexcept;
        float32 NkAcos(float32 a) noexcept;
        float64 NkAcos(float64 a) noexcept;

        // -------------------------------------------------------------------------
        // Hyperbolic
        // -------------------------------------------------------------------------
        float32 NkSinh(float32 x) noexcept;
        float64 NkSinh(float64 x) noexcept;
        float32 NkCosh(float32 x) noexcept;
        float64 NkCosh(float64 x) noexcept;
        float32 NkTanh(float32 x) noexcept;
        float64 NkTanh(float64 x) noexcept;

        // -------------------------------------------------------------------------
        // Floating-point utils
        // -------------------------------------------------------------------------
        float32 NkFmod(float32 x, float32 y) noexcept;
        float64 NkFmod(float64 x, float64 y) noexcept;
        NK_FORCE_INLINE nk_bool NkIsFinite(float32 x) noexcept {
            union {
                float32 value;
                uint32 bits;
            } data = { x };
            return (data.bits & 0x7F800000u) != 0x7F800000u;
        }
        NK_FORCE_INLINE nk_bool NkIsFinite(float64 x) noexcept {
            union {
                float64 value;
                uint64 bits;
            } data = { x };
            return (data.bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
        }
        float32 NkFrexp(float32 x, int32* exp) noexcept;
        float64 NkFrexp(float64 x, int32* exp) noexcept;
        float32 NkLdexp(float32 x, int32 exp) noexcept;
        float64 NkLdexp(float64 x, int32 exp) noexcept;
        float32 NkModf(float32 x, float32* iptr) noexcept;
        float64 NkModf(float64 x, float64* iptr) noexcept;

        struct Ldiv {
            int64 quot;
            int64 rem;
        };
        using DivResult64 = Ldiv;

        DivResult64 NkDivI64(int64 numerator, int64 denominator) noexcept;

        // Proposed clearer alias
        NK_FORCE_INLINE DivResult64 NkDivMod64(int64 numerator, int64 denominator) noexcept {
            return NkDivI64(numerator, denominator);
        }

        // -------------------------------------------------------------------------
        // Integer/bit utils
        // -------------------------------------------------------------------------
        uint64 NkGcd(uint64 a, uint64 b) noexcept;

        NK_FORCE_INLINE uint64 NkLcm(uint64 a, uint64 b) noexcept {
            return (a / NkGcd(a, b)) * b;
        }

        NK_FORCE_INLINE nk_bool NkIsPowerOf2(uint64 x) noexcept {
            return x && !(x & (x - 1));
        }

        uint64 NkNextPowerOf2(uint64 x) noexcept;
        uint32 NkClz(uint32 x) noexcept;
        uint32 NkClz(uint64 x) noexcept;
        uint32 NkCtz(uint32 x) noexcept;
        uint32 NkCtz(uint64 x) noexcept;
        uint32 NkPopcount(uint32 x) noexcept;
        uint32 NkPopcount(uint64 x) noexcept;

        // -------------------------------------------------------------------------
        // Interpolation
        // -------------------------------------------------------------------------
        template <typename T>
        NK_FORCE_INLINE T NkLerp(T a, T b, float32 t) noexcept {
            return a + (b - a) * t;
        }

        // Proposed clearer alias
        template <typename T>
        NK_FORCE_INLINE T NkMix(T a, T b, float32 t) noexcept {
            return NkLerp(a, b, t);
        }

        float32 NkSmoothstep(float32 t) noexcept;
        float64 NkSmoothstep(float64 t) noexcept;
        float32 NkSmootherstep(float32 t) noexcept;
        float64 NkSmootherstep(float64 t) noexcept;

    } // namespace math
} // namespace nkentseu

#endif // NKENTSEU_NKMATH_NKFUNCTIONS_H_INCLUDED
