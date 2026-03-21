//
// NkEulerAngle.h
// Triplet d'angles pitch/yaw/roll pour représenter une orientation 3D.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_EULERANGLE_H__
#define __NKENTSEU_EULERANGLE_H__

#include "NKMath/NkLegacySystem.h"
#include "NkAngle.h"

namespace nkentseu {

    namespace math {

        // =====================================================================
        // NkEulerAngle
        //
        // Convention d'application : Yaw (Y) → Pitch (X) → Roll (Z).
        // Chaque composante est un NkAngle (wrapping (-180, 180] intégré).
        //
        //   pitch : tangage — rotation autour de l'axe X
        //   yaw   : lacet   — rotation autour de l'axe Y
        //   roll  : roulis  — rotation autour de l'axe Z
        // =====================================================================

        struct NkEulerAngle {

            NkAngle pitch; ///< Tangage — axe X
            NkAngle yaw;   ///< Lacet   — axe Y
            NkAngle roll;  ///< Roulis  — axe Z

            // ── Constructeurs ─────────────────────────────────────────────────

            constexpr NkEulerAngle() noexcept = default;

            NkEulerAngle(const NkAngle& pitch, const NkAngle& yaw, const NkAngle& roll) noexcept
                : pitch(pitch), yaw(yaw), roll(roll) {}

            NkEulerAngle(const NkEulerAngle&) noexcept            = default;
            NkEulerAngle& operator=(const NkEulerAngle&) noexcept = default;

            // ── Comparaison ───────────────────────────────────────────────────

            bool operator==(const NkEulerAngle& o) const noexcept {
                return pitch == o.pitch && yaw == o.yaw && roll == o.roll;
            }

            bool operator!=(const NkEulerAngle& o) const noexcept {
                return !(*this == o);
            }

            // ── Arithmétique ──────────────────────────────────────────────────

            NkEulerAngle operator+(const NkEulerAngle& o) const noexcept {
                return { pitch + o.pitch, yaw + o.yaw, roll + o.roll };
            }

            NkEulerAngle operator-(const NkEulerAngle& o) const noexcept {
                return { pitch - o.pitch, yaw - o.yaw, roll - o.roll };
            }

            NkEulerAngle& operator+=(const NkEulerAngle& o) noexcept {
                pitch += o.pitch;
                yaw   += o.yaw;
                roll  += o.roll;
                return *this;
            }

            NkEulerAngle& operator-=(const NkEulerAngle& o) noexcept {
                pitch -= o.pitch;
                yaw   -= o.yaw;
                roll  -= o.roll;
                return *this;
            }

            NkEulerAngle operator*(float32 s) const noexcept {
                return { pitch * s, yaw * s, roll * s };
            }

            NkEulerAngle& operator*=(float32 s) noexcept {
                pitch *= s;
                yaw   *= s;
                roll  *= s;
                return *this;
            }

            friend NkEulerAngle operator*(float32 s, const NkEulerAngle& e) noexcept {
                return e * s;
            }

            // ── Incrément / Décrément ─────────────────────────────────────────

            NkEulerAngle& operator++()    noexcept { ++pitch; ++yaw; ++roll; return *this; }
            NkEulerAngle& operator--()    noexcept { --pitch; --yaw; --roll; return *this; }
            NkEulerAngle  operator++(int) noexcept { NkEulerAngle t(*this); ++(*this); return t; }
            NkEulerAngle  operator--(int) noexcept { NkEulerAngle t(*this); --(*this); return t; }

            // ── I/O ───────────────────────────────────────────────────────────

            NkString ToString() const {
                return NkFormat("euler({0}, {1}, {2})", pitch.Deg(), yaw.Deg(), roll.Deg());
            }

            friend NkString ToString(const NkEulerAngle& e) {
                return e.ToString();
            }

            friend std::ostream& operator<<(std::ostream& os, const NkEulerAngle& e) {
                return os << e.ToString().CStr();
            }

        }; // struct NkEulerAngle

    } // namespace math


    // =========================================================================
    // NkToString<NkEulerAngle>
    // =========================================================================

    template<>
    struct NkToString<math::NkEulerAngle> {
        static NkString Convert(const math::NkEulerAngle& v, const NkFormatProps& props) {
            return NkApplyFormatProps(v.ToString(), props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_EULERANGLE_H__