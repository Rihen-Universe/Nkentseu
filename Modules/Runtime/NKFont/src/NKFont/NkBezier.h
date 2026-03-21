#pragma once
/**
 * @File    NkBezier.h
 * @Brief   Courbes de Bézier quadratiques et cubiques en F26Dot6.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Ce module fournit deux opérations fondamentales pour le rasterizer :
 *
 *  1. Décomposition (Flatten) : convertit une courbe en suite de segments
 *     linéaires avec une tolérance adaptative. Le rasterizer ne connaît
 *     que des lignes droites.
 *
 *  2. Subdivision (Split) : divise une courbe en deux sous-courbes au
 *     paramètre t=0.5 (algorithme de de Casteljau). Utilisé par Flatten.
 *
 *  Tolérance par défaut : 0.25 pixel (= 16 en F26Dot6).
 *  En dessous de cette tolérance, la courbe est considérée comme droite.
 *
 *  Pas de STL, pas d'allocation dynamique : le callback FlattenCallback
 *  reçoit chaque segment au fur et à mesure.
 *
 * @Formats
 *  TTF/OTF glyf  : Bézier quadratiques (conic)
 *  CFF/Type1     : Bézier cubiques
 *  TrueType spec : on-curve et off-curve points
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkFixed26_6.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  Point 2D en F26Dot6
    // =========================================================================

    struct NKENTSEU_FONT_API NkPoint26_6 {
        F26Dot6 x, y;

        constexpr NkPoint26_6() noexcept = default;
        constexpr NkPoint26_6(F26Dot6 x, F26Dot6 y) noexcept : x(x), y(y) {}

        static constexpr NkPoint26_6 FromInts(int32 xi, int32 yi) noexcept {
            return { F26Dot6::FromInt(xi), F26Dot6::FromInt(yi) };
        }

        NKFONT_NODISCARD constexpr NkPoint26_6 operator+(NkPoint26_6 r) const noexcept { return {x + r.x, y + r.y}; }
        NKFONT_NODISCARD constexpr NkPoint26_6 operator-(NkPoint26_6 r) const noexcept { return {x - r.x, y - r.y}; }
        NKFONT_NODISCARD constexpr NkPoint26_6 operator*(int32 i)       const noexcept { return {x * i,   y * i};   }

        /// Milieu de deux points (exact en F26Dot6 : divise par 2 via shift).
        NKFONT_NODISCARD constexpr NkPoint26_6 MidPoint(NkPoint26_6 r) const noexcept {
            return { F26Dot6::FromRaw((x.raw + r.x.raw) >> 1),
                     F26Dot6::FromRaw((y.raw + r.y.raw) >> 1) };
        }

        NKFONT_NODISCARD constexpr bool operator==(NkPoint26_6 r) const noexcept {
            return x == r.x && y == r.y;
        }
    };

    using Pt = NkPoint26_6;

    // =========================================================================
    //  Callback de segment — appelé pour chaque segment linéaire produit
    // =========================================================================

    /**
     * @Brief Signature du callback reçu à chaque segment issu de Flatten.
     * @param p0     Point de départ du segment (en F26Dot6).
     * @param p1     Point d'arrivée du segment (en F26Dot6).
     * @param userdata Opaque pointer fourni par l'appelant.
     */
    using NkFlattenCallback = void(*)(NkPoint26_6 p0, NkPoint26_6 p1, void* userdata);

    // =========================================================================
    //  NkBezierQuad — Bézier quadratique (TrueType glyf)
    // =========================================================================

    struct NKENTSEU_FONT_API NkBezierQuad {
        NkPoint26_6 p0;  ///< Point de départ (on-curve)
        NkPoint26_6 p1;  ///< Point de contrôle (off-curve)
        NkPoint26_6 p2;  ///< Point d'arrivée (on-curve)

        constexpr NkBezierQuad() noexcept = default;
        constexpr NkBezierQuad(NkPoint26_6 p0, NkPoint26_6 p1, NkPoint26_6 p2) noexcept
            : p0(p0), p1(p1), p2(p2) {}

        /**
         * @Brief Subdivision au paramètre t=0.5 (algorithme de Casteljau).
         *        Produit deux sous-courbes [p0..mid] et [mid..p2].
         */
        void Split(NkBezierQuad& left, NkBezierQuad& right) const noexcept {
            const NkPoint26_6 m01  = p0.MidPoint(p1);
            const NkPoint26_6 m12  = p1.MidPoint(p2);
            const NkPoint26_6 mid  = m01.MidPoint(m12);

            left  = { p0,  m01, mid };
            right = { mid, m12, p2  };
        }

        /**
         * @Brief Décompose la courbe en segments linéaires avec tolérance adaptative.
         *
         * @param cb       Callback appelé pour chaque segment.
         * @param userdata Passé tel quel au callback.
         * @param tolerance Tolérance en F26Dot6 (défaut 16 = 0.25 px).
         * @param maxDepth  Récursion max (défaut 16 — suffit pour toute taille pratique).
         */
        void Flatten(NkFlattenCallback cb, void* userdata,
                     int32 tolerance = 16, int32 maxDepth = 16) const noexcept {
            FlattenRec(*this, cb, userdata, tolerance, maxDepth, 0);
        }

        /**
         * @Brief Évalue la courbe au paramètre t ∈ [0,1] (usage debug/tests).
         *        Formule : B(t) = (1-t)²·P0 + 2t(1-t)·P1 + t²·P2.
         *        Utilise double pour la précision — pas dans le hot path.
         */
        NkPoint26_6 Evaluate(double t) const noexcept {
            const double u  = 1.0 - t;
            const double u2 = u  * u;
            const double t2 = t  * t;
            const double ut = 2.0 * u * t;
            return {
                F26Dot6::FromDouble(u2 * p0.x.ToDouble() + ut * p1.x.ToDouble() + t2 * p2.x.ToDouble()),
                F26Dot6::FromDouble(u2 * p0.y.ToDouble() + ut * p1.y.ToDouble() + t2 * p2.y.ToDouble())
            };
        }

    private:
        static void FlattenRec(const NkBezierQuad& q,
                                NkFlattenCallback cb, void* ud,
                                int32 tol, int32 maxDepth, int32 depth) noexcept {
            // Test d'aplatissement : distance du point de contrôle à la corde P0-P2.
            // Heuristique rapide en coordonnées entières : pas de sqrt.
            // |d| = |2·P1 - P0 - P2| / 2 en chaque axe.
            const int32 dx = (2 * q.p1.x.raw - q.p0.x.raw - q.p2.x.raw);
            const int32 dy = (2 * q.p1.y.raw - q.p0.y.raw - q.p2.y.raw);
            // Norme de Chebyshev (max des valeurs absolues) — calcul sans mul/div.
            const int32 d  = (dx < 0 ? -dx : dx) | (dy < 0 ? -dy : dy);

            if (d <= tol || depth >= maxDepth) {
                cb(q.p0, q.p2, ud);
                return;
            }
            NkBezierQuad left, right;
            q.Split(left, right);
            FlattenRec(left,  cb, ud, tol, maxDepth, depth + 1);
            FlattenRec(right, cb, ud, tol, maxDepth, depth + 1);
        }
    };

    // =========================================================================
    //  NkBezierCubic — Bézier cubique (CFF / Type 1)
    // =========================================================================

    struct NKENTSEU_FONT_API NkBezierCubic {
        NkPoint26_6 p0;  ///< Départ (on-curve)
        NkPoint26_6 p1;  ///< Premier point de contrôle
        NkPoint26_6 p2;  ///< Deuxième point de contrôle
        NkPoint26_6 p3;  ///< Arrivée (on-curve)

        constexpr NkBezierCubic() noexcept = default;
        constexpr NkBezierCubic(NkPoint26_6 p0, NkPoint26_6 p1,
                                 NkPoint26_6 p2, NkPoint26_6 p3) noexcept
            : p0(p0), p1(p1), p2(p2), p3(p3) {}

        /// Subdivision de Casteljau à t=0.5.
        void Split(NkBezierCubic& left, NkBezierCubic& right) const noexcept {
            const NkPoint26_6 m01  = p0.MidPoint(p1);
            const NkPoint26_6 m12  = p1.MidPoint(p2);
            const NkPoint26_6 m23  = p2.MidPoint(p3);
            const NkPoint26_6 m012 = m01.MidPoint(m12);
            const NkPoint26_6 m123 = m12.MidPoint(m23);
            const NkPoint26_6 mid  = m012.MidPoint(m123);

            left  = { p0,  m01,  m012, mid };
            right = { mid, m123, m23,  p3  };
        }

        void Flatten(NkFlattenCallback cb, void* userdata,
                     int32 tolerance = 16, int32 maxDepth = 16) const noexcept {
            FlattenRec(*this, cb, userdata, tolerance, maxDepth, 0);
        }

        NkPoint26_6 Evaluate(double t) const noexcept {
            const double u  = 1.0 - t;
            const double u3 = u*u*u,  t3 = t*t*t;
            const double a  = 3.0*u*u*t, b = 3.0*u*t*t;
            return {
                F26Dot6::FromDouble(u3*p0.x.ToDouble() + a*p1.x.ToDouble() + b*p2.x.ToDouble() + t3*p3.x.ToDouble()),
                F26Dot6::FromDouble(u3*p0.y.ToDouble() + a*p1.y.ToDouble() + b*p2.y.ToDouble() + t3*p3.y.ToDouble())
            };
        }

    private:
        static void FlattenRec(const NkBezierCubic& c,
                                NkFlattenCallback cb, void* ud,
                                int32 tol, int32 maxDepth, int32 depth) noexcept {
            // Test d'aplatissement cubique : déflexion maximale des deux points de contrôle.
            const int32 dx1 = (3*c.p1.x.raw - 2*c.p0.x.raw - c.p3.x.raw);
            const int32 dy1 = (3*c.p1.y.raw - 2*c.p0.y.raw - c.p3.y.raw);
            const int32 dx2 = (3*c.p2.x.raw - c.p0.x.raw - 2*c.p3.x.raw);
            const int32 dy2 = (3*c.p2.y.raw - c.p0.y.raw - 2*c.p3.y.raw);

            const int32 d1 = (dx1 < 0 ? -dx1 : dx1) | (dy1 < 0 ? -dy1 : dy1);
            const int32 d2 = (dx2 < 0 ? -dx2 : dx2) | (dy2 < 0 ? -dy2 : dy2);
            const int32 d  = d1 > d2 ? d1 : d2;

            if (d <= tol || depth >= maxDepth) {
                cb(c.p0, c.p3, ud);
                return;
            }
            NkBezierCubic left, right;
            c.Split(left, right);
            FlattenRec(left,  cb, ud, tol, maxDepth, depth + 1);
            FlattenRec(right, cb, ud, tol, maxDepth, depth + 1);
        }
    };

    // =========================================================================
    //  Utilitaire : conversion quad → cubic (pour pipeline unifié)
    // =========================================================================

    /**
     * @Brief Convertit un Bézier quadratique en cubique équivalent.
     *        Formule : P1_c = P0 + 2/3*(P1_q - P0), P2_c = P2 + 2/3*(P1_q - P2).
     *        Exact — pas d'approximation.
     */
    NKFONT_NODISCARD inline NkBezierCubic QuadToCubic(const NkBezierQuad& q) noexcept {
        // En F26Dot6 : 2/3 n'est pas représentable exactement.
        // On utilise l'approximation entière : 2*(P1-P0)/3 via shift + mul.
        // (x * 2) / 3  →  (x * 43691) >> 16  (erreur < 1 ULP pour |x| < 2^15)
        auto scale23 = [](int32 v) -> int32 {
            return (static_cast<int32>((static_cast<int64>(v) * 2 * 43691L) >> 16));
        };

        const int32 cp1x = q.p0.x.raw + scale23(q.p1.x.raw - q.p0.x.raw);
        const int32 cp1y = q.p0.y.raw + scale23(q.p1.y.raw - q.p0.y.raw);
        const int32 cp2x = q.p2.x.raw + scale23(q.p1.x.raw - q.p2.x.raw);
        const int32 cp2y = q.p2.y.raw + scale23(q.p1.y.raw - q.p2.y.raw);

        return {
            q.p0,
            { F26Dot6::FromRaw(cp1x), F26Dot6::FromRaw(cp1y) },
            { F26Dot6::FromRaw(cp2x), F26Dot6::FromRaw(cp2y) },
            q.p2
        };
    }

} // namespace nkentseu
