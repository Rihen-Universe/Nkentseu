//
// NkColor.h
// Couleur RGBA sur 8 bits par composante + helpers HSV.
// Les couleurs nommées sont des constantes statiques (pas des méthodes).
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_COLOR_H__
#define __NKENTSEU_COLOR_H__

#include "NKMath/NkLegacySystem.h"
#include "NKMath/NkFunctions.h"
#include "NKMath/NkVec.h"

namespace nkentseu {

    namespace math {

        // =====================================================================
        // NkHSV
        //
        // Représentation teinte/saturation/valeur.
        //   hue        : [0, 360)
        //   saturation : [0, 100]
        //   value      : [0, 100]
        // =====================================================================

        struct NkHSV {

            float32 hue;        ///< Teinte en degrés [0, 360)
            float32 saturation; ///< Saturation [0, 100]
            float32 value;      ///< Valeur/luminosité [0, 100]

            // ── Constructeurs ─────────────────────────────────────────────────

            constexpr NkHSV() noexcept
                : hue(0.0f), saturation(0.0f), value(0.0f) {}

            NkHSV(float32 h, float32 s, float32 v) noexcept
                : hue(NkClamp(h, 0.0f, 360.0f))
                , saturation(NkClamp(s, 0.0f, 100.0f))
                , value(NkClamp(v, 0.0f, 100.0f)) {}

            // ── Opérateurs ────────────────────────────────────────────────────

            friend NkHSV operator+(const NkHSV& a, const NkHSV& b) noexcept {
                return { a.hue + b.hue, a.saturation + b.saturation, a.value + b.value };
            }

            friend NkHSV operator-(const NkHSV& a, const NkHSV& b) noexcept {
                return { a.hue - b.hue, a.saturation - b.saturation, a.value - b.value };
            }

        }; // struct NkHSV


        // =====================================================================
        // NkColor
        //
        // Couleur RGBA 8 bits par composante.
        // Stockage interne : r, g, b, a ∈ [0, 255].
        //
        // Les couleurs nommées sont des `static const NkColor` — elles sont
        // initialisées une seule fois (ODR-safe avec inline static depuis C++17).
        // =====================================================================

        constexpr float32 c1_255 = 1.0f / 255.0f;

        class NkColor {
            public:

                uint8 r; ///< Rouge   [0, 255]
                uint8 g; ///< Vert    [0, 255]
                uint8 b; ///< Bleu    [0, 255]
                uint8 a; ///< Alpha   [0, 255]

                // ── Constructeurs ─────────────────────────────────────────────────

                constexpr NkColor() noexcept
                    : r(0), g(0), b(0), a(255) {}

                constexpr NkColor(uint8 r, uint8 g, uint8 b, uint8 a = 255) noexcept
                    : r(r), g(g), b(b), a(a) {}

                /// Depuis uint32 RRGGBBAA (big-endian, comme les codes HTML)
                NkColor(uint32 rgba) noexcept
                    : r(static_cast<uint8>((rgba >> 24) & 0xFF))
                    , g(static_cast<uint8>((rgba >> 16) & 0xFF))
                    , b(static_cast<uint8>((rgba >>  8) & 0xFF))
                    , a(static_cast<uint8>( rgba         & 0xFF)) {}

                NkColor(const NkVector4f& v) noexcept
                    : r(static_cast<uint8>(NkClamp(v.r, 0.0f, 1.0f) * 255.0f))
                    , g(static_cast<uint8>(NkClamp(v.g, 0.0f, 1.0f) * 255.0f))
                    , b(static_cast<uint8>(NkClamp(v.b, 0.0f, 1.0f) * 255.0f))
                    , a(static_cast<uint8>(NkClamp(v.a, 0.0f, 1.0f) * 255.0f)) {}

                NkColor(const NkVector3f& v) noexcept
                    : r(static_cast<uint8>(NkClamp(v.r, 0.0f, 1.0f) * 255.0f))
                    , g(static_cast<uint8>(NkClamp(v.g, 0.0f, 1.0f) * 255.0f))
                    , b(static_cast<uint8>(NkClamp(v.b, 0.0f, 1.0f) * 255.0f))
                    , a(255) {}

                NkColor(const NkColor&) noexcept            = default;
                NkColor& operator=(const NkColor&) noexcept = default;

                // ── Conversions ───────────────────────────────────────────────────

                static NK_FORCE_INLINE float32 ToFloat(uint8 v) noexcept {
                    return static_cast<float32>(v) * c1_255;
                }

                explicit operator NkVector4f() const noexcept {
                    return { r * c1_255, g * c1_255, b * c1_255, a * c1_255 };
                }

                explicit operator NkVector3f() const noexcept {
                    return { r * c1_255, g * c1_255, b * c1_255 };
                }

                explicit operator uint32() const noexcept { return ToUint32A(); }

                uint32 ToUint32A() const noexcept {
                    return (static_cast<uint32>(r) << 24)
                        | (static_cast<uint32>(g) << 16)
                        | (static_cast<uint32>(b) <<  8)
                        |  static_cast<uint32>(a);
                }

                uint32 ToUint32() const noexcept {
                    return (static_cast<uint32>(r) << 24)
                        | (static_cast<uint32>(g) << 16)
                        | (static_cast<uint32>(b) <<  8)
                        | 255u;
                }

                // ── HSV ───────────────────────────────────────────────────────────

                static NkColor FromHSV(const NkHSV& hsv) noexcept;
                NkHSV          ToHSV()                   const noexcept;

                void SetHue(float32 h)        noexcept { NkHSV hsv = ToHSV(); hsv.hue        = NkClamp(h, 0.0f, 360.0f); *this = FromHSV(hsv); }
                void SetSaturation(float32 s) noexcept { NkHSV hsv = ToHSV(); hsv.saturation = NkClamp(s, 0.0f, 100.0f); *this = FromHSV(hsv); }
                void SetValue(float32 v)      noexcept { NkHSV hsv = ToHSV(); hsv.value      = NkClamp(v, 0.0f, 100.0f); *this = FromHSV(hsv); }

                void IncreaseHue(float32 d)        noexcept { NkHSV h = ToHSV(); h.hue        = NkClamp(h.hue        + d, 0.0f, 360.0f); *this = FromHSV(h); }
                void DecreaseHue(float32 d)        noexcept { IncreaseHue(-d); }
                void IncreaseSaturation(float32 d) noexcept { NkHSV h = ToHSV(); h.saturation = NkClamp(h.saturation + d, 0.0f, 100.0f); *this = FromHSV(h); }
                void DecreaseSaturation(float32 d) noexcept { IncreaseSaturation(-d); }
                void IncreaseValue(float32 d)      noexcept { NkHSV h = ToHSV(); h.value      = NkClamp(h.value      + d, 0.0f, 100.0f); *this = FromHSV(h); }
                void DecreaseValue(float32 d)      noexcept { IncreaseValue(-d); }

                // ── Ajustements ───────────────────────────────────────────────────

                NkColor Darken(float32 amount) const noexcept {
                    return {
                        static_cast<uint8>(static_cast<float32>(r) * (1.0f - amount)),
                        static_cast<uint8>(static_cast<float32>(g) * (1.0f - amount)),
                        static_cast<uint8>(static_cast<float32>(b) * (1.0f - amount)),
                        a
                    };
                }

                NkColor Lighten(float32 amount) const noexcept {
                    return {
                        static_cast<uint8>(NkMin(static_cast<float32>(r) + amount * (255.0f - static_cast<float32>(r)), 255.0f)),
                        static_cast<uint8>(NkMin(static_cast<float32>(g) + amount * (255.0f - static_cast<float32>(g)), 255.0f)),
                        static_cast<uint8>(NkMin(static_cast<float32>(b) + amount * (255.0f - static_cast<float32>(b)), 255.0f)),
                        a
                    };
                }

                NkColor ToGrayscale() const noexcept {
                    uint8 v = static_cast<uint8>(0.299f * r + 0.587f * g + 0.114f * b);
                    return { v, v, v, 255 };
                }

                NkColor ToGrayscaleWithAlpha() const noexcept {
                    uint8 v = static_cast<uint8>(0.299f * r + 0.587f * g + 0.114f * b);
                    return { v, v, v, a };
                }

                // ── Interpolation linéaire ────────────────────────────────────────

                NkColor Lerp(NkColor other, float32 t) const noexcept {
                    return {
                        static_cast<uint8>(static_cast<float32>(r) + (static_cast<float32>(other.r) - static_cast<float32>(r)) * t),
                        static_cast<uint8>(static_cast<float32>(g) + (static_cast<float32>(other.g) - static_cast<float32>(g)) * t),
                        static_cast<uint8>(static_cast<float32>(b) + (static_cast<float32>(other.b) - static_cast<float32>(b)) * t),
                        static_cast<uint8>(static_cast<float32>(a) + (static_cast<float32>(other.a) - static_cast<float32>(a)) * t)
                    };
                }

                // ── WithAlpha ────────────────────────────────────────────────────

                NkColor WithAlpha(int32 alpha) const noexcept {
                    return { r, g, b, static_cast<uint8>(alpha) };
                }

                // ── ToU32 (ABGR — format DrawList) ───────────────────────────────

                uint32 ToU32() const noexcept {
                    return (static_cast<uint32>(a) << 24)
                        | (static_cast<uint32>(b) << 16)
                        | (static_cast<uint32>(g) <<  8)
                        |  static_cast<uint32>(r);
                }

                // ── Opérateurs bit-à-bit ──────────────────────────────────────────

                NkColor operator&(const NkColor& o) const noexcept { return NkColor(ToUint32A() & o.ToUint32A()); }
                NkColor operator|(const NkColor& o) const noexcept { return NkColor(ToUint32A() | o.ToUint32A()); }
                NkColor operator^(const NkColor& o) const noexcept { return NkColor(ToUint32A() ^ o.ToUint32A()); }

                // ── Comparaison ───────────────────────────────────────────────────

                bool IsEqual(const NkColor& o) const noexcept {
                    return r == o.r && g == o.g && b == o.b && a == o.a;
                }

                friend bool operator==(const NkColor& a, const NkColor& b) noexcept { return a.IsEqual(b); }
                friend bool operator!=(const NkColor& a, const NkColor& b) noexcept { return !a.IsEqual(b); }

                // ── Constructeurs flottants ───────────────────────────────────────

                static NK_FORCE_INLINE NkColor RGBf(float32 r, float32 g, float32 b) noexcept {
                    return {
                        static_cast<uint8>(NkClamp(r, 0.0f, 1.0f) * 255.0f),
                        static_cast<uint8>(NkClamp(g, 0.0f, 1.0f) * 255.0f),
                        static_cast<uint8>(NkClamp(b, 0.0f, 1.0f) * 255.0f),
                        255
                    };
                }

                static NK_FORCE_INLINE NkColor RGBAf(float32 r, float32 g, float32 b, float32 a) noexcept {
                    return {
                        static_cast<uint8>(NkClamp(r, 0.0f, 1.0f) * 255.0f),
                        static_cast<uint8>(NkClamp(g, 0.0f, 1.0f) * 255.0f),
                        static_cast<uint8>(NkClamp(b, 0.0f, 1.0f) * 255.0f),
                        static_cast<uint8>(NkClamp(a, 0.0f, 1.0f) * 255.0f)
                    };
                }

                // ── Couleurs aléatoires ───────────────────────────────────────────

                static NkColor RandomRGB()  noexcept;
                static NkColor RandomRGBA() noexcept;

                // ── Lookup par nom ────────────────────────────────────────────────

                static const NkColor& FromName(const NkString& name) noexcept;

                // ── I/O ───────────────────────────────────────────────────────────

                NkString ToString() const {
                    return NkFormat("({0}, {1}, {2}, {3})",
                        static_cast<int32>(r),
                        static_cast<int32>(g),
                        static_cast<int32>(b),
                        static_cast<int32>(a));
                }

                friend NkString ToString(const NkColor& c) {
                    return c.ToString();
                }

                friend std::ostream& operator<<(std::ostream& os, const NkColor& c) {
                    return os << c.ToString().CStr();
                }

                // ── Factories de base (inline, pas de static const) ──────────────

                static NkColor White()  noexcept { return {255,255,255,255}; }
                static NkColor Black(int32 alpha=255) noexcept { return {0,0,0,static_cast<uint8>(alpha)}; }
                static NkColor Transparent() noexcept { return {0,0,0,0}; }
                static NkColor Gray(int32 v, int32 alpha=255) noexcept {
                    return { static_cast<uint8>(v), static_cast<uint8>(v), static_cast<uint8>(v), static_cast<uint8>(alpha) };
                }

                // ── Couleurs nommées (static const) ───────────────────────────────
                //
                // Définies dans NkColor.cpp via RGBf / RGBAf.
                // Usage : NkColor::Red, NkColor::Blue, etc.
                //
                static const NkColor Red;
                static const NkColor Green;
                static const NkColor Blue;
                static const NkColor Yellow;
                static const NkColor Cyan;
                static const NkColor Magenta;
                static const NkColor Orange;
                static const NkColor Pink;
                static const NkColor Purple;
                static const NkColor DarkGray;
                static const NkColor Lime;
                static const NkColor Teal;
                static const NkColor Brown;
                static const NkColor SaddleBrown;
                static const NkColor Olive;
                static const NkColor Maroon;
                static const NkColor Navy;
                static const NkColor Indigo;
                static const NkColor Turquoise;
                static const NkColor Silver;
                static const NkColor Gold;
                static const NkColor SkyBlue;
                static const NkColor ForestGreen;
                static const NkColor SteelBlue;
                static const NkColor DarkSlateGray;
                static const NkColor Chocolate;
                static const NkColor HotPink;
                static const NkColor SlateBlue;
                static const NkColor RoyalBlue;
                static const NkColor Tomato;
                static const NkColor MediumSeaGreen;
                static const NkColor DarkOrange;
                static const NkColor MediumPurple;
                static const NkColor CornflowerBlue;
                static const NkColor DarkGoldenrod;
                static const NkColor DodgerBlue;
                static const NkColor MediumVioletRed;
                static const NkColor Peru;
                static const NkColor MediumAquamarine;
                static const NkColor DarkTurquoise;
                static const NkColor MediumSlateBlue;
                static const NkColor YellowGreen;
                static const NkColor LightCoral;
                static const NkColor DarkSlateBlue;
                static const NkColor DarkOliveGreen;
                static const NkColor Firebrick;
                static const NkColor MediumOrchid;
                static const NkColor RosyBrown;
                static const NkColor DarkCyan;
                static const NkColor CadetBlue;
                static const NkColor PaleVioletRed;
                static const NkColor DeepPink;
                static const NkColor LawnGreen;
                static const NkColor MediumSpringGreen;
                static const NkColor MediumTurquoise;
                static const NkColor PaleGreen;
                static const NkColor DarkKhaki;
                static const NkColor MediumBlue;
                static const NkColor MidnightBlue;
                static const NkColor NavajoWhite;
                static const NkColor DarkSalmon;
                static const NkColor MediumCoral;
                static const NkColor DefaultBackground;
                static const NkColor CharcoalBlack;
                static const NkColor SlateGray;
                static const NkColor SkyBlueRef;
                static const NkColor DuckBlue;

        }; // class NkColor

    } // namespace math


    // =========================================================================
    // NkToString<NkColor>
    // =========================================================================

    template<>
    struct NkToString<math::NkColor> {
        static NkString Convert(const math::NkColor& c, const NkFormatProps& props) {
            return NkApplyFormatProps(c.ToString(), props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_COLOR_H__