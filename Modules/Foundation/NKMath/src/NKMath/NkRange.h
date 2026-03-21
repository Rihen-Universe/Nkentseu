//
// NkRange.h
// Intervalle [min, max] générique — sans allocation dynamique.
// Copyright (c) 2024 Rihen. All rights reserved.
//

#pragma once
#ifndef __NKENTSEU_RANGE_H__
#define __NKENTSEU_RANGE_H__

#include "NKMath/NkLegacySystem.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

    namespace math {

        // =====================================================================
        // NkRangeT<T>
        //
        // Intervalle fermé [min, max] générique.
        // Split() retourne ses résultats par out-parameters (pas de std::vector).
        // =====================================================================

        template<typename T>
        struct NkRangeT {

            public:

                // ── Constructeurs ─────────────────────────────────────────────────

                constexpr NkRangeT() noexcept
                    : mMin(T(0)), mMax(T(0)) {}

                /// Construit l'intervalle [a, b] en triant automatiquement
                constexpr NkRangeT(T a, T b) noexcept
                    : mMin(a <= b ? a : b), mMax(a <= b ? b : a) {}

                NkRangeT(const NkRangeT&) noexcept            = default;
                NkRangeT& operator=(const NkRangeT&) noexcept = default;

                // ── Accesseurs ────────────────────────────────────────────────────

                NK_FORCE_INLINE T GetMin() const noexcept { return mMin; }
                NK_FORCE_INLINE T GetMax() const noexcept { return mMax; }

                void SetMin(T v) noexcept {
                    mMin = v;
                    if (mMin > mMax) {
                        T t = mMax;
                        mMax = mMin;
                        mMin = t;
                    }
                }

                void SetMax(T v) noexcept {
                    mMax = v;
                    if (mMax < mMin) {
                        T t = mMin;
                        mMin = mMax;
                        mMax = t;
                    }
                }

                // ── Requêtes ──────────────────────────────────────────────────────

                NK_FORCE_INLINE T    Len()                       const noexcept { return mMax - mMin; }
                NK_FORCE_INLINE T    Center()                    const noexcept { return (mMin + mMax) / T(2); }
                NK_FORCE_INLINE bool IsEmpty()                   const noexcept { return mMin == mMax; }
                NK_FORCE_INLINE bool Contains(T v)               const noexcept { return v >= mMin && v <= mMax; }
                NK_FORCE_INLINE bool Contains(const NkRangeT& o) const noexcept { return mMin <= o.mMin && mMax >= o.mMax; }
                NK_FORCE_INLINE bool Overlaps(const NkRangeT& o) const noexcept { return mMin <= o.mMax && mMax >= o.mMin; }

                NK_FORCE_INLINE T Clamp(T v) const noexcept {
                    return (v < mMin) ? mMin : (v > mMax) ? mMax : v;
                }

                // ── Mutations ─────────────────────────────────────────────────────

                void Shift(T delta)  noexcept { mMin += delta; mMax += delta; }
                void Expand(T margin) noexcept { mMin -= margin; mMax += margin; }

                // ── Opérations statiques ──────────────────────────────────────────

                /// Intersection de deux intervalles
                static NkRangeT Intersect(const NkRangeT& a, const NkRangeT& b) noexcept {
                    return { NkMax(a.mMin, b.mMin), NkMin(a.mMax, b.mMax) };
                }

                /// Union (plus petit intervalle contenant les deux)
                static NkRangeT Union(const NkRangeT& a, const NkRangeT& b) noexcept {
                    return { NkMin(a.mMin, b.mMin), NkMax(a.mMax, b.mMax) };
                }

                /// Alias sémantique de Union
                static NkRangeT Hull(const NkRangeT& a, const NkRangeT& b) noexcept {
                    return Union(a, b);
                }

                /// Découpe l'intervalle r en deux à la valeur 'at'.
                /// Si 'at' n'est pas dans r, les deux sous-intervalles sont égaux à r.
                static void Split(const NkRangeT& r, T at,
                                NkRangeT& outLeft, NkRangeT& outRight) noexcept {
                    if (r.Contains(at)) {
                        outLeft  = { r.mMin, at };
                        outRight = { at, r.mMax };
                    } else {
                        outLeft  = r;
                        outRight = r;
                    }
                }

                // ── Comparaison ───────────────────────────────────────────────────

                bool operator==(const NkRangeT& o) const noexcept {
                    return mMin == o.mMin && mMax == o.mMax;
                }

                bool operator!=(const NkRangeT& o) const noexcept {
                    return !(*this == o);
                }

                // ── I/O ───────────────────────────────────────────────────────────

                NkString ToString() const {
                    return NkFormat("[{0}, {1}]", mMin, mMax);
                }

                friend NkString ToString(const NkRangeT& r) {
                    return r.ToString();
                }

                friend std::ostream& operator<<(std::ostream& os, const NkRangeT& r) {
                    return os << r.ToString().CStr();
                }

            private:

                T mMin; ///< Borne inférieure (≤ mMax)
                T mMax; ///< Borne supérieure (≥ mMin)

        }; // struct NkRangeT


        // =====================================================================
        // Aliases de types
        // =====================================================================

        using NkRange       = NkRangeT<float32>;
        using NkRangeFloat  = NkRangeT<float32>;
        using NkRangeDouble = NkRangeT<float64>;
        using NkRangeInt    = NkRangeT<int32>;
        using NkRangeUInt   = NkRangeT<uint32>;

    } // namespace math


    // =========================================================================
    // NkToString<NkRangeT>
    // =========================================================================

    template<typename T>
    struct NkToString<math::NkRangeT<T>> {
        static NkString Convert(const math::NkRangeT<T>& r, const NkFormatProps& props) {
            return NkApplyFormatProps(r.ToString(), props);
        }
    };

} // namespace nkentseu

#endif // __NKENTSEU_RANGE_H__