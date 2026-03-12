// ============================================================
// FILE: NkFunctions.cpp
// DESCRIPTION: Implementation of core math functions
// ============================================================

#include "NkFunctions.h"

#include <math.h>

namespace nkentseu {
    namespace math {

        float32 NkFloor(float32 x) noexcept {
            return static_cast<float32>(floor(x));
        }

        float64 NkFloor(float64 x) noexcept {
            return floor(x);
        }

        float32 NkCeil(float32 x) noexcept {
            return static_cast<float32>(ceil(x));
        }

        float64 NkCeil(float64 x) noexcept {
            return ceil(x);
        }

        float32 NkTrunc(float32 x) noexcept {
            return static_cast<float32>(trunc(x));
        }

        float64 NkTrunc(float64 x) noexcept {
            return trunc(x);
        }

        float32 NkRound(float32 x) noexcept {
            return static_cast<float32>(round(x));
        }

        float64 NkRound(float64 x) noexcept {
            return round(x);
        }

        float32 NkSqrt(float32 x) noexcept {
            return (x <= 0.0f) ? 0.0f : static_cast<float32>(sqrt(x));
        }

        float64 NkSqrt(float64 x) noexcept {
            return (x <= 0.0) ? 0.0 : sqrt(x);
        }

        float32 NkRsqrt(float32 x) noexcept {
            return (x <= 0.0f) ? 0.0f : (1.0f / static_cast<float32>(sqrt(x)));
        }

        float64 NkRsqrt(float64 x) noexcept {
            return (x <= 0.0) ? 0.0 : (1.0 / sqrt(x));
        }

        float32 NkCbrt(float32 x) noexcept {
            return static_cast<float32>(cbrt(x));
        }

        float64 NkCbrt(float64 x) noexcept {
            return cbrt(x);
        }

        float32 NkExp(float32 x) noexcept {
            if (x < -87.0f) {
                return 0.0f;
            }
            if (x > 88.0f) {
                return 3.4028235e38f;
            }
            return static_cast<float32>(exp(x));
        }

        float64 NkExp(float64 x) noexcept {
            if (x < -708.0) {
                return 0.0;
            }
            if (x > 709.0) {
                return 1.7976931348623157e308;
            }
            return exp(x);
        }

        float32 NkLog(float32 x) noexcept {
            return (x <= 0.0f) ? -3.4028235e38f : static_cast<float32>(log(x));
        }

        float64 NkLog(float64 x) noexcept {
            return (x <= 0.0) ? -1.0e308 : log(x);
        }

        float32 NkPow(float32 x, float32 y) noexcept {
            if (x == 0.0f) {
                return (y > 0.0f) ? 0.0f : 1.0f;
            }
            if (x < 0.0f) {
                const float32 yInt = static_cast<float32>(static_cast<int32>(y));
                if (y != yInt) {
                    return 0.0f;
                }
            }
            return static_cast<float32>(pow(x, y));
        }

        float64 NkPow(float64 x, float64 y) noexcept {
            if (x == 0.0) {
                return (y > 0.0) ? 0.0 : 1.0;
            }
            if (x < 0.0) {
                const float64 yInt = static_cast<float64>(static_cast<int64>(y));
                if (y != yInt) {
                    return 0.0;
                }
            }
            return pow(x, y);
        }

        float32 NkPowInt(float32 x, int32 n) noexcept {
            if (n == 0) {
                return 1.0f;
            }

            nk_bool negativeExp = n < 0;
            uint32 exp = static_cast<uint32>(negativeExp ? -n : n);
            float32 base = x;
            float32 result = 1.0f;

            while (exp > 0u) {
                if ((exp & 1u) != 0u) {
                    result *= base;
                }
                base *= base;
                exp >>= 1u;
            }

            return negativeExp ? (1.0f / result) : result;
        }

        float64 NkPowInt(float64 x, int32 n) noexcept {
            if (n == 0) {
                return 1.0;
            }

            nk_bool negativeExp = n < 0;
            uint32 exp = static_cast<uint32>(negativeExp ? -n : n);
            float64 base = x;
            float64 result = 1.0;

            while (exp > 0u) {
                if ((exp & 1u) != 0u) {
                    result *= base;
                }
                base *= base;
                exp >>= 1u;
            }

            return negativeExp ? (1.0 / result) : result;
        }

        float32 NkSin(float32 x) noexcept {
            return static_cast<float32>(sin(x));
        }

        float64 NkSin(float64 x) noexcept {
            return sin(x);
        }

        float32 NkCos(float32 x) noexcept {
            return static_cast<float32>(cos(x));
        }

        float64 NkCos(float64 x) noexcept {
            return cos(x);
        }

        float32 NkAtan(float32 x) noexcept {
            return static_cast<float32>(atan(x));
        }

        float64 NkAtan(float64 x) noexcept {
            return atan(x);
        }

        float32 NkAtan2(float32 y, float32 x) noexcept {
            return static_cast<float32>(atan2(y, x));
        }

        float64 NkAtan2(float64 y, float64 x) noexcept {
            return atan2(y, x);
        }

        float32 NkAsin(float32 a) noexcept {
            if (NkFabs(a) > 1.0f) {
                return 0.0f;
            }
            return static_cast<float32>(asin(a));
        }

        float64 NkAsin(float64 a) noexcept {
            if (NkFabs(a) > 1.0) {
                return 0.0;
            }
            return asin(a);
        }

        float32 NkAcos(float32 a) noexcept {
            if (NkFabs(a) > 1.0f) {
                return 0.0f;
            }
            return static_cast<float32>(acos(a));
        }

        float64 NkAcos(float64 a) noexcept {
            if (NkFabs(a) > 1.0) {
                return 0.0;
            }
            return acos(a);
        }

        float32 NkSinh(float32 x) noexcept {
            return static_cast<float32>(sinh(x));
        }

        float64 NkSinh(float64 x) noexcept {
            return sinh(x);
        }

        float32 NkCosh(float32 x) noexcept {
            return static_cast<float32>(cosh(x));
        }

        float64 NkCosh(float64 x) noexcept {
            return cosh(x);
        }

        float32 NkTanh(float32 x) noexcept {
            if (x > 10.0f) {
                return 1.0f;
            }
            if (x < -10.0f) {
                return -1.0f;
            }
            return static_cast<float32>(tanh(x));
        }

        float64 NkTanh(float64 x) noexcept {
            if (x > 20.0) {
                return 1.0;
            }
            if (x < -20.0) {
                return -1.0;
            }
            return tanh(x);
        }

        float32 NkFmod(float32 x, float32 y) noexcept {
            if (y == 0.0f) {
                return x;
            }
            return static_cast<float32>(fmod(x, y));
        }

        float64 NkFmod(float64 x, float64 y) noexcept {
            if (y == 0.0) {
                return x;
            }
            return fmod(x, y);
        }

        float32 NkFrexp(float32 x, int32* exp) noexcept {
            return static_cast<float32>(frexp(x, exp));
        }

        float64 NkFrexp(float64 x, int32* exp) noexcept {
            return frexp(x, exp);
        }

        float32 NkLdexp(float32 x, int32 exp) noexcept {
            return static_cast<float32>(ldexp(x, exp));
        }

        float64 NkLdexp(float64 x, int32 exp) noexcept {
            return ldexp(x, exp);
        }

        float32 NkModf(float32 x, float32* iptr) noexcept {
            return static_cast<float32>(modf(x, iptr));
        }

        float64 NkModf(float64 x, float64* iptr) noexcept {
            return modf(x, iptr);
        }

        DivResult64 NkDivI64(int64 numerator, int64 denominator) noexcept {
            if (denominator == 0) {
                return {0, numerator};
            }
            return {numerator / denominator, numerator % denominator};
        }

        uint64 NkGcd(uint64 a, uint64 b) noexcept {
            while (b != 0u) {
                const uint64 t = b;
                b = a % b;
                a = t;
            }
            return a;
        }

        uint64 NkNextPowerOf2(uint64 x) noexcept {
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

        uint32 NkClz(uint32 x) noexcept {
            if (x == 0u) {
                return 32u;
            }

    #if defined(__clang__) || defined(__GNUC__)
            return static_cast<uint32>(__builtin_clz(x));
    #else
            uint32 n = 0u;
            if (x <= 0x0000FFFFu) { n += 16u; x <<= 16u; }
            if (x <= 0x00FFFFFFu) { n += 8u; x <<= 8u; }
            if (x <= 0x0FFFFFFFu) { n += 4u; x <<= 4u; }
            if (x <= 0x3FFFFFFFu) { n += 2u; x <<= 2u; }
            if (x <= 0x7FFFFFFFu) { n += 1u; }
            return n;
    #endif
        }

        uint32 NkClz(uint64 x) noexcept {
            if (x == 0u) {
                return 64u;
            }

    #if defined(__clang__) || defined(__GNUC__)
            return static_cast<uint32>(__builtin_clzll(static_cast<unsigned long long>(x)));
    #else
            uint32 n = 0u;
            if (x <= 0x00000000FFFFFFFFull) { n += 32u; x <<= 32u; }
            if (x <= 0x0000FFFFFFFFFFFFull) { n += 16u; x <<= 16u; }
            if (x <= 0x00FFFFFFFFFFFFFFull) { n += 8u; x <<= 8u; }
            if (x <= 0x0FFFFFFFFFFFFFFFull) { n += 4u; x <<= 4u; }
            if (x <= 0x3FFFFFFFFFFFFFFFull) { n += 2u; x <<= 2u; }
            if (x <= 0x7FFFFFFFFFFFFFFFull) { n += 1u; }
            return n;
    #endif
        }

        uint32 NkCtz(uint32 x) noexcept {
            if (x == 0u) {
                return 32u;
            }

    #if defined(__clang__) || defined(__GNUC__)
            return static_cast<uint32>(__builtin_ctz(x));
    #else
            return 31u - NkClz(x & static_cast<uint32>(-static_cast<int32>(x)));
    #endif
        }

        uint32 NkCtz(uint64 x) noexcept {
            if (x == 0u) {
                return 64u;
            }

    #if defined(__clang__) || defined(__GNUC__)
            return static_cast<uint32>(__builtin_ctzll(static_cast<unsigned long long>(x)));
    #else
            return 63u - NkClz(x & static_cast<uint64>(-static_cast<int64>(x)));
    #endif
        }

        uint32 NkPopcount(uint32 x) noexcept {
    #if defined(__clang__) || defined(__GNUC__)
            return static_cast<uint32>(__builtin_popcount(x));
    #else
            x = x - ((x >> 1u) & 0x55555555u);
            x = (x & 0x33333333u) + ((x >> 2u) & 0x33333333u);
            return ((x + (x >> 4u)) & 0x0F0F0F0Fu) * 0x01010101u >> 24u;
    #endif
        }

        uint32 NkPopcount(uint64 x) noexcept {
    #if defined(__clang__) || defined(__GNUC__)
            return static_cast<uint32>(__builtin_popcountll(static_cast<unsigned long long>(x)));
    #else
            x = x - ((x >> 1u) & 0x5555555555555555ull);
            x = (x & 0x3333333333333333ull) + ((x >> 2u) & 0x3333333333333333ull);
            return static_cast<uint32>(((x + (x >> 4u)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull >> 56u);
    #endif
        }

        float32 NkSmoothstep(float32 t) noexcept {
            if (t < 0.0f) {
                return 0.0f;
            }
            if (t > 1.0f) {
                return 1.0f;
            }
            return t * t * (3.0f - 2.0f * t);
        }

        float64 NkSmoothstep(float64 t) noexcept {
            if (t < 0.0) {
                return 0.0;
            }
            if (t > 1.0) {
                return 1.0;
            }
            return t * t * (3.0 - 2.0 * t);
        }

        float32 NkSmootherstep(float32 t) noexcept {
            if (t < 0.0f) {
                return 0.0f;
            }
            if (t > 1.0f) {
                return 1.0f;
            }
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        float64 NkSmootherstep(float64 t) noexcept {
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
