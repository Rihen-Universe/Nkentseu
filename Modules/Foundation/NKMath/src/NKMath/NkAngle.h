//
// NkAngle.h
// Représentation d'un angle en degrés, wrappé dans (-180, 180].
// Conversion en radians à la demande (pas de cache, thread-safe).
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_ANGLE_H__
#define __NKENTSEU_ANGLE_H__

#include "NKMath/NkLegacySystem.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

    namespace math {

        // =====================================================================
        // NkAngle
        //
        // Stocke l'angle en degrés internes.
        // Chaque mutation wrape la valeur dans (-180, 180].
        // Rad() = simple multiplication par DEG2RAD — pas de static local.
        // =====================================================================

        class NkAngle {
			public:

				// ── Constructeurs ─────────────────────────────────────────────────

				constexpr NkAngle() noexcept
					: mDeg(0.0f) {}

				explicit NkAngle(float32 degrees) noexcept
					: mDeg(Wrap(degrees)) {}

				NkAngle(const NkAngle&) noexcept            = default;
				NkAngle& operator=(const NkAngle&) noexcept = default;

				// ── Fabrique ──────────────────────────────────────────────────────

				/// Crée un NkAngle depuis une valeur en radians
				static NK_FORCE_INLINE NkAngle FromRad(float32 radians) noexcept {
					return NkAngle(radians * static_cast<float32>(NkRAD2DEG));
				}

				/// Alias legacy
				static NK_FORCE_INLINE NkAngle FromRadian(float32 radians) noexcept {
					return FromRad(radians);
				}

				// ── Accesseurs ────────────────────────────────────────────────────

				/// Retourne la valeur en degrés
				NK_FORCE_INLINE float32 Deg() const noexcept { return mDeg; }

				/// Retourne la valeur en radians (calculé à la demande)
				NK_FORCE_INLINE float32 Rad() const noexcept {
					return mDeg * static_cast<float32>(NkDEG2RAD);
				}

				/// Modifie la valeur en degrés (wrapping automatique)
				void Deg(float32 d) noexcept { mDeg = Wrap(d); }

				/// Modifie la valeur en radians (wrapping automatique)
				void Rad(float32 r) noexcept { mDeg = Wrap(r * static_cast<float32>(NkRAD2DEG)); }

				/// Conversion implicite vers float32 (degrés) — pour NkSin, NkCos, etc.
				NK_FORCE_INLINE operator float32() const noexcept { return mDeg; }

				// ── Opérateurs unaires ────────────────────────────────────────────

				NK_FORCE_INLINE NkAngle operator-() const noexcept {
					return NkAngle(-mDeg);
				}

				// ── NkAngle op NkAngle ────────────────────────────────────────────

				NK_FORCE_INLINE NkAngle  operator+(const NkAngle& o) const noexcept { return NkAngle(mDeg + o.mDeg); }
				NK_FORCE_INLINE NkAngle  operator-(const NkAngle& o) const noexcept { return NkAngle(mDeg - o.mDeg); }
				NK_FORCE_INLINE NkAngle  operator*(const NkAngle& o) const noexcept { return NkAngle(mDeg * o.mDeg); }
				NK_FORCE_INLINE NkAngle  operator/(const NkAngle& o) const noexcept { return NkAngle(mDeg / o.mDeg); }
				NK_FORCE_INLINE NkAngle& operator+=(const NkAngle& o) noexcept { mDeg = Wrap(mDeg + o.mDeg); return *this; }
				NK_FORCE_INLINE NkAngle& operator-=(const NkAngle& o) noexcept { mDeg = Wrap(mDeg - o.mDeg); return *this; }
				NK_FORCE_INLINE NkAngle& operator*=(const NkAngle& o) noexcept { mDeg = Wrap(mDeg * o.mDeg); return *this; }
				NK_FORCE_INLINE NkAngle& operator/=(const NkAngle& o) noexcept { mDeg = Wrap(mDeg / o.mDeg); return *this; }

				// ── NkAngle op float32 ────────────────────────────────────────────

				NK_FORCE_INLINE NkAngle  operator+(float32 d) const noexcept { return NkAngle(mDeg + d); }
				NK_FORCE_INLINE NkAngle  operator-(float32 d) const noexcept { return NkAngle(mDeg - d); }
				NK_FORCE_INLINE NkAngle  operator*(float32 s) const noexcept { return NkAngle(mDeg * s); }
				NK_FORCE_INLINE NkAngle  operator/(float32 s) const noexcept { return NkAngle(mDeg / s); }
				NK_FORCE_INLINE NkAngle& operator+=(float32 d) noexcept { mDeg = Wrap(mDeg + d); return *this; }
				NK_FORCE_INLINE NkAngle& operator-=(float32 d) noexcept { mDeg = Wrap(mDeg - d); return *this; }
				NK_FORCE_INLINE NkAngle& operator*=(float32 s) noexcept { mDeg = Wrap(mDeg * s); return *this; }
				NK_FORCE_INLINE NkAngle& operator/=(float32 s) noexcept { mDeg = Wrap(mDeg / s); return *this; }

				// ── float32 op NkAngle (friends) ──────────────────────────────────

				friend NK_FORCE_INLINE NkAngle operator+(float32 d, const NkAngle& a) noexcept { return NkAngle(d + a.mDeg); }
				friend NK_FORCE_INLINE NkAngle operator-(float32 d, const NkAngle& a) noexcept { return NkAngle(d - a.mDeg); }
				friend NK_FORCE_INLINE NkAngle operator*(float32 s, const NkAngle& a) noexcept { return NkAngle(s * a.mDeg); }
				friend NK_FORCE_INLINE NkAngle operator/(float32 s, const NkAngle& a) noexcept { return NkAngle(s / a.mDeg); }

				// ── Incrément / Décrément (pas de 1 degré) ────────────────────────

				NK_FORCE_INLINE NkAngle& operator++()    noexcept { mDeg = Wrap(mDeg + 1.0f); return *this; }
				NK_FORCE_INLINE NkAngle& operator--()    noexcept { mDeg = Wrap(mDeg - 1.0f); return *this; }
				NK_FORCE_INLINE NkAngle  operator++(int) noexcept { NkAngle t(*this); ++(*this); return t; }
				NK_FORCE_INLINE NkAngle  operator--(int) noexcept { NkAngle t(*this); --(*this); return t; }

				// ── Comparaison ───────────────────────────────────────────────────

				friend NK_FORCE_INLINE bool operator==(const NkAngle& a, const NkAngle& b) noexcept {
					return NkFabs(a.mDeg - b.mDeg) <= static_cast<float32>(NkEpsilon);
				}

				friend NK_FORCE_INLINE bool operator!=(const NkAngle& a, const NkAngle& b) noexcept { return !(a == b); }
				friend NK_FORCE_INLINE bool operator< (const NkAngle& a, const NkAngle& b) noexcept { return a.mDeg <  b.mDeg; }
				friend NK_FORCE_INLINE bool operator<=(const NkAngle& a, const NkAngle& b) noexcept { return a.mDeg <= b.mDeg; }
				friend NK_FORCE_INLINE bool operator> (const NkAngle& a, const NkAngle& b) noexcept { return a.mDeg >  b.mDeg; }
				friend NK_FORCE_INLINE bool operator>=(const NkAngle& a, const NkAngle& b) noexcept { return a.mDeg >= b.mDeg; }

				// ── I/O ───────────────────────────────────────────────────────────

				NkString ToString() const {
					return NkFormat("{0}_deg", mDeg);
				}

				friend NkString ToString(const NkAngle& a) {
					return a.ToString();
				}

				friend std::ostream& operator<<(std::ostream& os, const NkAngle& a) {
					return os << a.ToString().CStr();
				}

			private:

				float32 mDeg; ///< Valeur interne en degrés, dans (-180, 180]

				/// Wrapping canonique dans (-180, 180]
				static NK_FORCE_INLINE float32 Wrap(float32 d) noexcept {
					while (d >  180.0f) { d -= 360.0f; }
					while (d <= -180.0f) { d += 360.0f; }
					return d;
				}

        }; // class NkAngle

    } // namespace math


    // =========================================================================
    // NkToString<NkAngle>
    // =========================================================================

    template<>
    struct NkToString<math::NkAngle> {
        static NkString Convert(const math::NkAngle& v, const NkFormatProps& props) {
            return NkApplyFormatProps(v.ToString(), props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_ANGLE_H__