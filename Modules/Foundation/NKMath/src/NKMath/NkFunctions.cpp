// ============================================================
// FILE: NkFunctions.cpp
// DESCRIPTION: Implementation of core math functions
// ============================================================

#include "NkFunctions.h"

#include <math.h>

namespace nkentseu {
namespace math {

    nk_float NkFloor(nk_float x) noexcept {
        return static_cast<nk_float>(floor(x));
    }

    nk_double NkFloor(nk_double x) noexcept {
        return floor(x);
    }

    nk_float NkCeil(nk_float x) noexcept {
        return static_cast<nk_float>(ceil(x));
    }

    nk_double NkCeil(nk_double x) noexcept {
        return ceil(x);
    }

    nk_float NkTrunc(nk_float x) noexcept {
        return static_cast<nk_float>(trunc(x));
    }

    nk_double NkTrunc(nk_double x) noexcept {
        return trunc(x);
    }

    nk_float NkRound(nk_float x) noexcept {
        return static_cast<nk_float>(round(x));
    }

    nk_double NkRound(nk_double x) noexcept {
        return round(x);
    }

    nk_float NkSqrt(nk_float x) noexcept {
        return (x <= 0.0f) ? 0.0f : static_cast<nk_float>(sqrt(x));
    }

    nk_double NkSqrt(nk_double x) noexcept {
        return (x <= 0.0) ? 0.0 : sqrt(x);
    }

    nk_float NkRsqrt(nk_float x) noexcept {
        return (x <= 0.0f) ? 0.0f : (1.0f / static_cast<nk_float>(sqrt(x)));
    }

    nk_double NkRsqrt(nk_double x) noexcept {
        return (x <= 0.0) ? 0.0 : (1.0 / sqrt(x));
    }

    nk_float NkCbrt(nk_float x) noexcept {
        return static_cast<nk_float>(cbrt(x));
    }

    nk_double NkCbrt(nk_double x) noexcept {
        return cbrt(x);
    }

    nk_float NkExp(nk_float x) noexcept {
        if (x < -87.0f) {
            return 0.0f;
        }
        if (x > 88.0f) {
            return 3.4028235e38f;
        }
        return static_cast<nk_float>(exp(x));
    }

    nk_double NkExp(nk_double x) noexcept {
        if (x < -708.0) {
            return 0.0;
        }
        if (x > 709.0) {
            return 1.7976931348623157e308;
        }
        return exp(x);
    }

    nk_float NkLog(nk_float x) noexcept {
        return (x <= 0.0f) ? -3.4028235e38f : static_cast<nk_float>(log(x));
    }

    nk_double NkLog(nk_double x) noexcept {
        return (x <= 0.0) ? -1.0e308 : log(x);
    }

    nk_float NkPow(nk_float x, nk_float y) noexcept {
        if (x == 0.0f) {
            return (y > 0.0f) ? 0.0f : 1.0f;
        }
        if (x < 0.0f) {
            const nk_float yInt = static_cast<nk_float>(static_cast<nk_int32>(y));
            if (y != yInt) {
                return 0.0f;
            }
        }
        return static_cast<nk_float>(pow(x, y));
    }

    nk_double NkPow(nk_double x, nk_double y) noexcept {
        if (x == 0.0) {
            return (y > 0.0) ? 0.0 : 1.0;
        }
        if (x < 0.0) {
            const nk_double yInt = static_cast<nk_double>(static_cast<nk_int64>(y));
            if (y != yInt) {
                return 0.0;
            }
        }
        return pow(x, y);
    }

    nk_float NkPowInt(nk_float x, nk_int32 n) noexcept {
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

    nk_double NkPowInt(nk_double x, nk_int32 n) noexcept {
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

    nk_float NkSin(nk_float x) noexcept {
        return static_cast<nk_float>(sin(x));
    }

    nk_double NkSin(nk_double x) noexcept {
        return sin(x);
    }

    nk_float NkCos(nk_float x) noexcept {
        return static_cast<nk_float>(cos(x));
    }

    nk_double NkCos(nk_double x) noexcept {
        return cos(x);
    }

    nk_float NkAtan(nk_float x) noexcept {
        return static_cast<nk_float>(atan(x));
    }

    nk_double NkAtan(nk_double x) noexcept {
        return atan(x);
    }

    nk_float NkAtan2(nk_float y, nk_float x) noexcept {
        return static_cast<nk_float>(atan2(y, x));
    }

    nk_double NkAtan2(nk_double y, nk_double x) noexcept {
        return atan2(y, x);
    }

    nk_float NkAsin(nk_float a) noexcept {
        if (NkFabs(a) > 1.0f) {
            return 0.0f;
        }
        return static_cast<nk_float>(asin(a));
    }

    nk_double NkAsin(nk_double a) noexcept {
        if (NkFabs(a) > 1.0) {
            return 0.0;
        }
        return asin(a);
    }

    nk_float NkAcos(nk_float a) noexcept {
        if (NkFabs(a) > 1.0f) {
            return 0.0f;
        }
        return static_cast<nk_float>(acos(a));
    }

    nk_double NkAcos(nk_double a) noexcept {
        if (NkFabs(a) > 1.0) {
            return 0.0;
        }
        return acos(a);
    }

    nk_float NkSinh(nk_float x) noexcept {
        return static_cast<nk_float>(sinh(x));
    }

    nk_double NkSinh(nk_double x) noexcept {
        return sinh(x);
    }

    nk_float NkCosh(nk_float x) noexcept {
        return static_cast<nk_float>(cosh(x));
    }

    nk_double NkCosh(nk_double x) noexcept {
        return cosh(x);
    }

    nk_float NkTanh(nk_float x) noexcept {
        if (x > 10.0f) {
            return 1.0f;
        }
        if (x < -10.0f) {
            return -1.0f;
        }
        return static_cast<nk_float>(tanh(x));
    }

    nk_double NkTanh(nk_double x) noexcept {
        if (x > 20.0) {
            return 1.0;
        }
        if (x < -20.0) {
            return -1.0;
        }
        return tanh(x);
    }

    nk_float NkFmod(nk_float x, nk_float y) noexcept {
        if (y == 0.0f) {
            return x;
        }
        return static_cast<nk_float>(fmod(x, y));
    }

    nk_double NkFmod(nk_double x, nk_double y) noexcept {
        if (y == 0.0) {
            return x;
        }
        return fmod(x, y);
    }

    nk_float NkFrexp(nk_float x, nk_int32* exp) noexcept {
        return static_cast<nk_float>(frexp(x, exp));
    }

    nk_double NkFrexp(nk_double x, nk_int32* exp) noexcept {
        return frexp(x, exp);
    }

    nk_float NkLdexp(nk_float x, nk_int32 exp) noexcept {
        return static_cast<nk_float>(ldexp(x, exp));
    }

    nk_double NkLdexp(nk_double x, nk_int32 exp) noexcept {
        return ldexp(x, exp);
    }

    nk_float NkModf(nk_float x, nk_float* iptr) noexcept {
        return static_cast<nk_float>(modf(x, iptr));
    }

    nk_double NkModf(nk_double x, nk_double* iptr) noexcept {
        return modf(x, iptr);
    }

    DivResult64 NkDivI64(nk_int64 numerator, nk_int64 denominator) noexcept {
        if (denominator == 0) {
            return {0, numerator};
        }
        return {numerator / denominator, numerator % denominator};
    }

    nk_uint64 NkGcd(nk_uint64 a, nk_uint64 b) noexcept {
        while (b != 0u) {
            const nk_uint64 t = b;
            b = a % b;
            a = t;
        }
        return a;
    }

    nk_uint64 NkNextPowerOf2(nk_uint64 x) noexcept {
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

    nk_uint32 NkClz(nk_uint32 x) noexcept {
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

    nk_uint32 NkClz(nk_uint64 x) noexcept {
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

    nk_uint32 NkCtz(nk_uint32 x) noexcept {
        if (x == 0u) {
            return 32u;
        }

#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_ctz(x));
#else
        return 31u - NkClz(x & static_cast<nk_uint32>(-static_cast<nk_int32>(x)));
#endif
    }

    nk_uint32 NkCtz(nk_uint64 x) noexcept {
        if (x == 0u) {
            return 64u;
        }

#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_ctzll(static_cast<unsigned long long>(x)));
#else
        return 63u - NkClz(x & static_cast<nk_uint64>(-static_cast<nk_int64>(x)));
#endif
    }

    nk_uint32 NkPopcount(nk_uint32 x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_popcount(x));
#else
        x = x - ((x >> 1u) & 0x55555555u);
        x = (x & 0x33333333u) + ((x >> 2u) & 0x33333333u);
        return ((x + (x >> 4u)) & 0x0F0F0F0Fu) * 0x01010101u >> 24u;
#endif
    }

    nk_uint32 NkPopcount(nk_uint64 x) noexcept {
#if defined(__clang__) || defined(__GNUC__)
        return static_cast<nk_uint32>(__builtin_popcountll(static_cast<unsigned long long>(x)));
#else
        x = x - ((x >> 1u) & 0x5555555555555555ull);
        x = (x & 0x3333333333333333ull) + ((x >> 2u) & 0x3333333333333333ull);
        return static_cast<nk_uint32>(((x + (x >> 4u)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull >> 56u);
#endif
    }

    nk_float NkSmoothstep(nk_float t) noexcept {
        if (t < 0.0f) {
            return 0.0f;
        }
        if (t > 1.0f) {
            return 1.0f;
        }
        return t * t * (3.0f - 2.0f * t);
    }

    nk_double NkSmoothstep(nk_double t) noexcept {
        if (t < 0.0) {
            return 0.0;
        }
        if (t > 1.0) {
            return 1.0;
        }
        return t * t * (3.0 - 2.0 * t);
    }

    nk_float NkSmootherstep(nk_float t) noexcept {
        if (t < 0.0f) {
            return 0.0f;
        }
        if (t > 1.0f) {
            return 1.0f;
        }
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    nk_double NkSmootherstep(nk_double t) noexcept {
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
