#pragma once
/**
 * @File    NkFixed26_6.h
 * @Brief   Virgule fixe 26.6 et 16.16 — format natif TrueType/FreeType.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkFixed26_6  : Q26.6  — coordonnées de contours, métriques de glyphes.
 *  NkFixed16_16 : Q16.16 — matrices de transformation, vecteurs de projection hinter.
 *  NkMatrix2x2  : matrice 2×2 en F16.16 pour les glyphes composites et le hinter.
 *
 *  Précision 26.6 : 1/64 px. Plage entière : ±33 554 431.
 *  Précision 16.16 : 1/65536. Plage entière : ±32 767.
 *
 *  Toutes les opérations sont constexpr et sans branche.
 *  Zéro dépendance externe.
 */

#include "NKFont/NkFontExport.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  NkFixed26_6
    // =========================================================================

    struct NKENTSEU_FONT_API NkFixed26_6 {

        int32 raw = 0;

        static constexpr int32 SHIFT     = 6;
        static constexpr int32 ONE       = 1 << SHIFT;
        static constexpr int32 HALF      = ONE >> 1;
        static constexpr int32 FRAC_MASK = ONE - 1;
        static constexpr int32 INT_MASK  = ~FRAC_MASK;

        constexpr NkFixed26_6() noexcept = default;

        NKFONT_NODISCARD static constexpr NkFixed26_6 FromRaw   (int32  r) noexcept { NkFixed26_6 f; f.raw = r; return f; }
        NKFONT_NODISCARD static constexpr NkFixed26_6 FromInt   (int32  i) noexcept { return FromRaw(i << SHIFT); }
        NKFONT_NODISCARD static           NkFixed26_6 FromFloat (float  v) noexcept { return FromRaw(static_cast<int32>(v * static_cast<float> (ONE))); }
        NKFONT_NODISCARD static           NkFixed26_6 FromDouble(double v) noexcept { return FromRaw(static_cast<int32>(v * static_cast<double>(ONE))); }

        NKFONT_NODISCARD static constexpr NkFixed26_6 Zero()     noexcept { return FromRaw(0);    }
        NKFONT_NODISCARD static constexpr NkFixed26_6 One()      noexcept { return FromRaw(ONE);  }
        NKFONT_NODISCARD static constexpr NkFixed26_6 Half()     noexcept { return FromRaw(HALF); }
        NKFONT_NODISCARD static constexpr NkFixed26_6 MinusOne() noexcept { return FromRaw(-ONE); }

        // Conversions
        NKFONT_NODISCARD constexpr int32  ToInt()    const noexcept { return raw >> SHIFT; }
        NKFONT_NODISCARD constexpr int32  Round()    const noexcept { return (raw + HALF) >> SHIFT; }
        NKFONT_NODISCARD constexpr int32  Floor()    const noexcept { return raw >> SHIFT; }
        NKFONT_NODISCARD constexpr int32  Ceil()     const noexcept { return (raw + FRAC_MASK) >> SHIFT; }
        NKFONT_NODISCARD constexpr int32  Frac()     const noexcept { return raw & FRAC_MASK; }
        NKFONT_NODISCARD           float  ToFloat()  const noexcept { return static_cast<float> (raw) / static_cast<float> (ONE); }
        NKFONT_NODISCARD           double ToDouble() const noexcept { return static_cast<double>(raw) / static_cast<double>(ONE); }

        // Arithmétique
        NKFONT_NODISCARD constexpr NkFixed26_6 operator+(NkFixed26_6 r) const noexcept { return FromRaw(raw + r.raw); }
        NKFONT_NODISCARD constexpr NkFixed26_6 operator-(NkFixed26_6 r) const noexcept { return FromRaw(raw - r.raw); }
        NKFONT_NODISCARD constexpr NkFixed26_6 operator-()              const noexcept { return FromRaw(-raw); }
        NKFONT_NODISCARD constexpr NkFixed26_6 operator*(int32 i)       const noexcept { return FromRaw(raw * i); }
        NKFONT_NODISCARD constexpr NkFixed26_6 operator/(int32 i)       const noexcept { return FromRaw(raw / i); }

        NKFONT_NODISCARD constexpr NkFixed26_6 operator*(NkFixed26_6 r) const noexcept {
            return FromRaw(static_cast<int32>((static_cast<int64>(raw) * r.raw) >> SHIFT));
        }
        NKFONT_NODISCARD constexpr NkFixed26_6 operator/(NkFixed26_6 r) const noexcept {
            return FromRaw(static_cast<int32>((static_cast<int64>(raw) << SHIFT) / r.raw));
        }

        constexpr NkFixed26_6& operator+=(NkFixed26_6 r) noexcept { raw += r.raw; return *this; }
        constexpr NkFixed26_6& operator-=(NkFixed26_6 r) noexcept { raw -= r.raw; return *this; }
        constexpr NkFixed26_6& operator*=(NkFixed26_6 r) noexcept { *this = *this * r; return *this; }
        constexpr NkFixed26_6& operator/=(NkFixed26_6 r) noexcept { *this = *this / r; return *this; }
        constexpr NkFixed26_6& operator*=(int32 i)       noexcept { raw *= i; return *this; }
        constexpr NkFixed26_6& operator/=(int32 i)       noexcept { raw /= i; return *this; }

        // Comparaisons
        NKFONT_NODISCARD constexpr bool operator==(NkFixed26_6 r) const noexcept { return raw == r.raw; }
        NKFONT_NODISCARD constexpr bool operator!=(NkFixed26_6 r) const noexcept { return raw != r.raw; }
        NKFONT_NODISCARD constexpr bool operator< (NkFixed26_6 r) const noexcept { return raw <  r.raw; }
        NKFONT_NODISCARD constexpr bool operator<=(NkFixed26_6 r) const noexcept { return raw <= r.raw; }
        NKFONT_NODISCARD constexpr bool operator> (NkFixed26_6 r) const noexcept { return raw >  r.raw; }
        NKFONT_NODISCARD constexpr bool operator>=(NkFixed26_6 r) const noexcept { return raw >= r.raw; }

        // Utilitaires
        NKFONT_NODISCARD constexpr NkFixed26_6 Abs()        const noexcept { return FromRaw(raw < 0 ? -raw : raw); }
        NKFONT_NODISCARD constexpr NkFixed26_6 FloorFixed() const noexcept { return FromRaw(raw & INT_MASK); }
        NKFONT_NODISCARD constexpr NkFixed26_6 CeilFixed()  const noexcept { return FromRaw((raw + FRAC_MASK) & INT_MASK); }
        NKFONT_NODISCARD constexpr NkFixed26_6 FracFixed()  const noexcept { return FromRaw(raw & FRAC_MASK); }
        NKFONT_NODISCARD constexpr bool IsZero()            const noexcept { return raw == 0; }
        NKFONT_NODISCARD constexpr bool IsNegative()        const noexcept { return raw <  0; }
        NKFONT_NODISCARD constexpr bool IsPositive()        const noexcept { return raw >  0; }
    };

    NKFONT_NODISCARD constexpr NkFixed26_6 operator*(int32 i, NkFixed26_6 f) noexcept { return f * i; }

    using F26Dot6 = NkFixed26_6;

    // =========================================================================
    //  NkFixed16_16
    // =========================================================================

    struct NKENTSEU_FONT_API NkFixed16_16 {
        int32 raw = 0;

        static constexpr int32 SHIFT     = 16;
        static constexpr int32 ONE       = 1 << SHIFT;
        static constexpr int32 HALF      = ONE >> 1;
        static constexpr int32 FRAC_MASK = ONE - 1;

        NKFONT_NODISCARD static constexpr NkFixed16_16 FromRaw   (int32  r) noexcept { NkFixed16_16 f; f.raw = r; return f; }
        NKFONT_NODISCARD static constexpr NkFixed16_16 FromInt   (int32  i) noexcept { return FromRaw(i << SHIFT); }
        NKFONT_NODISCARD static           NkFixed16_16 FromFloat (float  v) noexcept { return FromRaw(static_cast<int32>(v * static_cast<float> (ONE))); }
        NKFONT_NODISCARD static           NkFixed16_16 FromDouble(double v) noexcept { return FromRaw(static_cast<int32>(v * static_cast<double>(ONE))); }

        NKFONT_NODISCARD static constexpr NkFixed16_16 Zero() noexcept { return FromRaw(0);   }
        NKFONT_NODISCARD static constexpr NkFixed16_16 One()  noexcept { return FromRaw(ONE); }

        NKFONT_NODISCARD constexpr int32  ToInt()    const noexcept { return raw >> SHIFT; }
        NKFONT_NODISCARD constexpr int32  Round()    const noexcept { return (raw + HALF) >> SHIFT; }
        NKFONT_NODISCARD           float  ToFloat()  const noexcept { return static_cast<float> (raw) / static_cast<float> (ONE); }
        NKFONT_NODISCARD           double ToDouble() const noexcept { return static_cast<double>(raw) / static_cast<double>(ONE); }

        NKFONT_NODISCARD constexpr F26Dot6 ToF26Dot6() const noexcept {
            return F26Dot6::FromRaw(raw >> (SHIFT - F26Dot6::SHIFT));
        }

        NKFONT_NODISCARD constexpr NkFixed16_16 operator+(NkFixed16_16 r) const noexcept { return FromRaw(raw + r.raw); }
        NKFONT_NODISCARD constexpr NkFixed16_16 operator-(NkFixed16_16 r) const noexcept { return FromRaw(raw - r.raw); }
        NKFONT_NODISCARD constexpr NkFixed16_16 operator-()               const noexcept { return FromRaw(-raw); }
        NKFONT_NODISCARD constexpr NkFixed16_16 operator*(int32 i)        const noexcept { return FromRaw(raw * i); }

        NKFONT_NODISCARD constexpr NkFixed16_16 operator*(NkFixed16_16 r) const noexcept {
            return FromRaw(static_cast<int32>((static_cast<int64>(raw) * r.raw) >> SHIFT));
        }
        NKFONT_NODISCARD constexpr NkFixed16_16 operator/(NkFixed16_16 r) const noexcept {
            return FromRaw(static_cast<int32>((static_cast<int64>(raw) << SHIFT) / r.raw));
        }

        constexpr NkFixed16_16& operator+=(NkFixed16_16 r) noexcept { raw += r.raw; return *this; }
        constexpr NkFixed16_16& operator-=(NkFixed16_16 r) noexcept { raw -= r.raw; return *this; }
        constexpr NkFixed16_16& operator*=(NkFixed16_16 r) noexcept { *this = *this * r; return *this; }

        NKFONT_NODISCARD constexpr bool operator==(NkFixed16_16 r) const noexcept { return raw == r.raw; }
        NKFONT_NODISCARD constexpr bool operator< (NkFixed16_16 r) const noexcept { return raw <  r.raw; }
        NKFONT_NODISCARD constexpr bool operator<=(NkFixed16_16 r) const noexcept { return raw <= r.raw; }
        NKFONT_NODISCARD constexpr bool operator> (NkFixed16_16 r) const noexcept { return raw >  r.raw; }
        NKFONT_NODISCARD constexpr bool operator>=(NkFixed16_16 r) const noexcept { return raw >= r.raw; }

        NKFONT_NODISCARD constexpr NkFixed16_16 Abs() const noexcept { return FromRaw(raw < 0 ? -raw : raw); }
    };

    using F16Dot16 = NkFixed16_16;

    // Conversion croisée
    NKFONT_NODISCARD constexpr NkFixed16_16 ToF16Dot16(NkFixed26_6 f) noexcept {
        return NkFixed16_16::FromRaw(f.raw << (NkFixed16_16::SHIFT - NkFixed26_6::SHIFT));
    }

    // =========================================================================
    //  NkMatrix2x2
    // =========================================================================

    struct NKENTSEU_FONT_API NkMatrix2x2 {
        F16Dot16 xx, xy;
        F16Dot16 yx, yy;

        NKFONT_NODISCARD static constexpr NkMatrix2x2 Identity() noexcept {
            NkMatrix2x2 m;
            m.xx = F16Dot16::FromInt(1); m.xy = F16Dot16::Zero();
            m.yx = F16Dot16::Zero();     m.yy = F16Dot16::FromInt(1);
            return m;
        }

        void Transform(F26Dot6& x, F26Dot6& y) const noexcept {
            const int64 nx = (static_cast<int64>(xx.raw) * x.raw
                            + static_cast<int64>(xy.raw) * y.raw) >> F16Dot16::SHIFT;
            const int64 ny = (static_cast<int64>(yx.raw) * x.raw
                            + static_cast<int64>(yy.raw) * y.raw) >> F16Dot16::SHIFT;
            x.raw = static_cast<int32>(nx);
            y.raw = static_cast<int32>(ny);
        }

        NKFONT_NODISCARD NkMatrix2x2 operator*(const NkMatrix2x2& r) const noexcept {
            NkMatrix2x2 m;
            m.xx = xx * r.xx + xy * r.yx;
            m.xy = xx * r.xy + xy * r.yy;
            m.yx = yx * r.xx + yy * r.yx;
            m.yy = yx * r.xy + yy * r.yy;
            return m;
        }

        NKFONT_NODISCARD constexpr bool IsIdentity() const noexcept {
            return xx.raw == F16Dot16::ONE && xy.raw == 0
                && yx.raw == 0             && yy.raw == F16Dot16::ONE;
        }
    };

} // namespace nkentseu
