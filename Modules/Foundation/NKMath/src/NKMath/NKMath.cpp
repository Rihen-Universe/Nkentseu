// ============================================================
// FILE: NkMath.cpp
// DESCRIPTION: Implementation of core math functions
// ============================================================

#include "NkMathFunctions.h"

#include <cmath>

namespace nkentseu {
namespace math {

    nk_float Floor(nk_float x) noexcept {
        return static_cast<nk_float>(std::floor(x));
    }

    nk_double Floor(nk_double x) noexcept {
        return std::floor(x);
    }

    nk_float Ceil(nk_float x) noexcept {
        return static_cast<nk_float>(std::ceil(x));
    }

    nk_double Ceil(nk_double x) noexcept {
        return std::ceil(x);
    }

    nk_float Trunc(nk_float x) noexcept {
        return static_cast<nk_float>(std::trunc(x));
    }

    nk_double Trunc(nk_double x) noexcept {
        return std::trunc(x);
    }

    nk_float Round(nk_float x) noexcept {
        return static_cast<nk_float>(std::round(x));
    }

    nk_double Round(nk_double x) noexcept {
        return std::round(x);
    }

    nk_float Sqrt(nk_float x) noexcept {
        return (x <= 0.0f) ? 0.0f : static_cast<nk_float>(std::sqrt(x));
    }

    nk_double Sqrt(nk_double x) noexcept {
        return (x <= 0.0) ? 0.0 : std::sqrt(x);
    }

    nk_float Rsqrt(nk_float x) noexcept {
        return (x <= 0.0f) ? 0.0f : (1.0f / static_cast<nk_float>(std::sqrt(x)));
    }

    nk_double Rsqrt(nk_double x) noexcept {
        return (x <= 0.0) ? 0.0 : (1.0 / std::sqrt(x));
    }

    nk_float Cbrt(nk_float x) noexcept {
        return static_cast<nk_float>(std::cbrt(x));
    }

    nk_double Cbrt(nk_double x) noexcept {
        return std::cbrt(x);
    }

    nk_float Exp(nk_float x) noexcept {
        if (x < -87.0f) {
            return 0.0f;
        }
        if (x > 88.0f) {
            return 3.4028235e38f;
        }
        return static_cast<nk_float>(std::exp(x));
    }

    nk_double Exp(nk_double x) noexcept {
        if (x < -708.0) {
            return 0.0;
        }
        if (x > 709.0) {
            return 1.7976931348623157e308;
        }
        return std::exp(x);
    }

    nk_float Log(nk_float x) noexcept {
        return (x <= 0.0f) ? -3.4028235e38f : static_cast<nk_float>(std::log(x));
    }

    nk_double Log(nk_double x) noexcept {
        return (x <= 0.0) ? -1.0e308 : std::log(x);
    }

    nk_float Pow(nk_float x, nk_float y) noexcept {
        if (x == 0.0f) {
            return (y > 0.0f) ? 0.0f : 1.0f;
        }
        if (x < 0.0f) {
            const nk_float yInt = static_cast<nk_float>(static_cast<nk_int32>(y));
            if (y != yInt) {
                return 0.0f;
            }
        }
        return static_cast<nk_float>(std::pow(x, y));
    }

    nk_double Pow(nk_double x, nk_double y) noexcept {
        if (x == 0.0) {
            return (y > 0.0) ? 0.0 : 1.0;
        }
        if (x < 0.0) {
            const nk_double yInt = static_cast<nk_double>(static_cast<nk_int64>(y));
            if (y != yInt) {
                return 0.0;
            }
        }
        return std::pow(x, y);
    }

    nk_float PowInt(nk_float x, nk_int32 n) noexcept {
        if (n == 0) {
            return 1.0f;
        }

        nk_bool negativeExp = n < 0;
        nk_uint32 exp = static_cast<nk_uint32>(negativeExp ? -n : n);
        nk_float base = x;
        nk_float result = 1.0f;

        while (exp > 0u) {
            if ((exp & 1u) != 0u) {
                result *= base;
            }
            base *= base;
            exp >>= 1u;
        }

        return negativeExp ? (1.0f / result) : result;
    }

    nk_double PowInt(nk_double x, nk_int32 n) noexcept {
        if (n == 0) {
            return 1.0;
        }

        nk_bool negativeExp = n < 0;
        nk_uint32 exp = static_cast<nk_uint32>(negativeExp ? -n : n);
        nk_double base = x;
        nk_double result = 1.0;

        while (exp > 0u) {
            if ((exp & 1u) != 0u) {
                result *= base;
            }
            base *= base;
            exp >>= 1u;
        }

        return negativeExp ? (1.0 / result) : result;
    }

    nk_float Sin(nk_float x) noexcept {
        return static_cast<nk_float>(std::sin(x));
    }

    nk_double Sin(nk_double x) noexcept {
        return std::sin(x);
    }

    nk_float Cos(nk_float x) noexcept {
        return static_cast<nk_float>(std::cos(x));
    }

    nk_double Cos(nk_double x) noexcept {
        return std::cos(x);
    }

    nk_float Atan(nk_float x) noexcept {
        return static_cast<nk_float>(std::atan(x));
    }

    nk_double Atan(nk_double x) noexcept {
        return std::atan(x);
    }

    nk_float Atan2(nk_float y, nk_float x) noexcept {
        return static_cast<nk_float>(std::atan2(y, x));
    }

    nk_double Atan2(nk_double y, nk_double x) noexcept {
        return std::atan2(y, x);
    }

    nk_float Asin(nk_float a) noexcept {
        if (Fabs(a) > 1.0f) {
            return 0.0f;
        }
        return static_cast<nk_float>(std::asin(a));
    }

    nk_double Asin(nk_double a) noexcept {
        if (Fabs(a) > 1.0) {
            return 0.0;
        }
        return std::asin(a);
    }

    nk_float Acos(nk_float a) noexcept {
        if (Fabs(a) > 1.0f) {
            return 0.0f;
        }
        return static_cast<nk_float>(std::acos(a));
    }

    nk_double Acos(nk_double a) noexcept {
        if (Fabs(a) > 1.0) {
            return 0.0;
        }
        return std::acos(a);
    }

    nk_float Sinh(nk_float x) noexcept {
        return static_cast<nk_float>(std::sinh(x));
    }

    nk_double Sinh(nk_double x) noexcept {
        return std::sinh(x);
    }

    nk_float Cosh(nk_float x) noexcept {
        return static_cast<nk_float>(std::cosh(x));
    }

    nk_double Cosh(nk_double x) noexcept {
        return std::cosh(x);
    }

    nk_float Tanh(nk_float x) noexcept {
        if (x > 10.0f) {
            return 1.0f;
        }
        if (x < -10.0f) {
            return -1.0f;
        }
        return static_cast<nk_float>(std::tanh(x));
    }

    nk_double Tanh(nk_double x) noexcept {
        if (x > 20.0) {
            return 1.0;
        }
        if (x < -20.0) {
            return -1.0;
        }
        return std::tanh(x);
    }

    nk_float Fmod(nk_float x, nk_float y) noexcept {
        if (y == 0.0f) {
            return x;
        }
        return static_cast<nk_float>(std::fmod(x, y));
    }

    nk_double Fmod(nk_double x, nk_double y) noexcept {
        if (y == 0.0) {
            return x;
        }
        return std::fmod(x, y);
    }

    nk_float Frexp(nk_float x, nk_int32* exp) noexcept {
        return static_cast<nk_float>(std::frexp(x, exp));
    }

    nk_double Frexp(nk_double x, nk_int32* exp) noexcept {
        return std::frexp(x, exp);
    }

    nk_float Ldexp(nk_float x, nk_int32 exp) noexcept {
        return static_cast<nk_float>(std::ldexp(x, exp));
    }

    nk_double Ldexp(nk_double x, nk_int32 exp) noexcept {
        return std::ldexp(x, exp);
    }

    nk_float Modf(nk_float x, nk_float* iptr) noexcept {
        return static_cast<nk_float>(std::modf(x, iptr));
    }

    nk_double Modf(nk_double x, nk_double* iptr) noexcept {
        return std::modf(x, iptr);
    }

    DivResult64 DivI64(nk_int64 numerator, nk_int64 denominator) noexcept {
        if (denominator == 0) {
            return {0, numerator};
        }
        return {numerator / denominator, numerator % denominator};
    }

    nk_uint64 Gcd(nk_uint64 a, nk_uint64 b) noexcept {
        while (b != 0u) {
            const nk_uint64 t = b;
            b = a % b;
            a = t;
        }
        return a;
    }

    nk_uint64 NextPowerOf2(nk_uint64 x) noexcept {
        if (x == 0u) {
            return 1u;
        }

        --x;
        x |= x >> 1u;
        x |= x >> 2u;
        x |= x >> 4u;
        x |= x >> 8u;
        x |= x >> 16u;
        x |= x >> 32u;
        return x + 1u;
    }

    nk_uint32 Clz(nk_uint32 x) noexcept {
        if (x == 0u) {
            return 32u;
        }

#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_clz(x));
#else
        nk_uint32 n = 0u;
        if (x <= 0x0000FFFFu) { n += 16u; x <<= 16u; }
        if (x <= 0x00FFFFFFu) { n += 8u; x <<= 8u; }
        if (x <= 0x0FFFFFFFu) { n += 4u; x <<= 4u; }
        if (x <= 0x3FFFFFFFu) { n += 2u; x <<= 2u; }
        if (x <= 0x7FFFFFFFu) { n += 1u; }
        return n;
#endif
    }

    nk_uint32 Clz(nk_uint64 x) noexcept {
        if (x == 0u) {
            return 64u;
        }

#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_clzll(static_cast<unsigned long long>(x)));
#else
        nk_uint32 n = 0u;
        if (x <= 0x00000000FFFFFFFFull) { n += 32u; x <<= 32u; }
        if (x <= 0x0000FFFFFFFFFFFFull) { n += 16u; x <<= 16u; }
        if (x <= 0x00FFFFFFFFFFFFFFull) { n += 8u; x <<= 8u; }
        if (x <= 0x0FFFFFFFFFFFFFFFull) { n += 4u; x <<= 4u; }
        if (x <= 0x3FFFFFFFFFFFFFFFull) { n += 2u; x <<= 2u; }
        if (x <= 0x7FFFFFFFFFFFFFFFull) { n += 1u; }
        return n;
#endif
    }

    nk_uint32 Ctz(nk_uint32 x) noexcept {
        if (x == 0u) {
            return 32u;
        }

#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_ctz(x));
#else
        return 31u - Clz(x & static_cast<nk_uint32>(-static_cast<nk_int32>(x)));
#endif
    }

    nk_uint32 Ctz(nk_uint64 x) noexcept {
        if (x == 0u) {
            return 64u;
        }

#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_ctzll(static_cast<unsigned long long>(x)));
#else
        return 63u - Clz(x & static_cast<nk_uint64>(-static_cast<nk_int64>(x)));
#endif
    }

    nk_uint32 Popcount(nk_uint32 x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_popcount(x));
#else
        x = x - ((x >> 1u) & 0x55555555u);
        x = (x & 0x33333333u) + ((x >> 2u) & 0x33333333u);
        return ((x + (x >> 4u)) & 0x0F0F0F0Fu) * 0x01010101u >> 24u;
#endif
    }

    nk_uint32 Popcount(nk_uint64 x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_popcountll(static_cast<unsigned long long>(x)));
#else
        x = x - ((x >> 1u) & 0x5555555555555555ull);
        x = (x & 0x3333333333333333ull) + ((x >> 2u) & 0x3333333333333333ull);
        return static_cast<nk_uint32>(((x + (x >> 4u)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull >> 56u);
#endif
    }

    nk_float Smoothstep(nk_float t) noexcept {
        if (t < 0.0f) {
            return 0.0f;
        }
        if (t > 1.0f) {
            return 1.0f;
        }
        return t * t * (3.0f - 2.0f * t);
    }

    nk_double Smoothstep(nk_double t) noexcept {
        if (t < 0.0) {
            return 0.0;
        }
        if (t > 1.0) {
            return 1.0;
        }
        return t * t * (3.0 - 2.0 * t);
    }

    nk_float Smootherstep(nk_float t) noexcept {
        if (t < 0.0f) {
            return 0.0f;
        }
        if (t > 1.0f) {
            return 1.0f;
        }
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    nk_double Smootherstep(nk_double t) noexcept {
        if (t < 0.0) {
            return 0.0;
        }
        if (t > 1.0) {
            return 1.0;
        }
        return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
    }

} // namespace math
} // namespace nkentseu
