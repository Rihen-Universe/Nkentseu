#pragma once
/**
 * @File    NkOutline.h
 * @Brief   Représentation vectorielle intermédiaire entre parser et rasterizer.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkOutline est la structure de données centrale du pipeline de rendu.
 *  Elle reçoit les contours depuis NkTTFParser/NkCFFParser et les fournit
 *  au rasterizer sous forme de segments linéaires (après flattening Bézier).
 *
 *  Pipeline :
 *    Parser → NkOutline (points F26Dot6) → Flatten → NkEdgeList (segments)
 *
 *  Un contour est une suite de points avec tags :
 *    TAG_ON_CURVE  : point sur la courbe
 *    TAG_CONIC     : point de contrôle quadratique (TrueType)
 *    TAG_CUBIC     : point de contrôle cubique (CFF/Type1)
 *
 *  Reconstruction des courbes implicites TrueType :
 *    Deux points off-curve consécutifs → on-curve implicite au milieu.
 *
 *  Coordonnées : F26Dot6, origine bas-gauche (convention TrueType).
 *  Le rasterizer flip Y vers haut-gauche (convention écran).
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkFixed26_6.h"
#include "NKFont/NkBezier.h"
#include "NKFont/NkMemArena.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // ── Tags de points ────────────────────────────────────────────────────────
    static constexpr uint8 NK_TAG_ON_CURVE = 0x01;
    static constexpr uint8 NK_TAG_CONIC    = 0x00; // off-curve quadratique
    static constexpr uint8 NK_TAG_CUBIC    = 0x02; // off-curve cubique

    // =========================================================================
    //  NkEdge — segment linéaire orienté pour le rasterizer
    // =========================================================================

    /**
     * @Brief Un segment de droite entre deux points entiers (pixels).
     *        Seules les arêtes non-horizontales sont conservées.
     *        Le sens (montant/descendant) encode le winding.
     */
    struct NkEdge {
        int32 x0, y0;   ///< Point de départ en pixels entiers
        int32 x1, y1;   ///< Point d'arrivée en pixels entiers
        int32 dx;        ///< x1 - x0
        int32 dy;        ///< y1 - y0 (toujours != 0)
        int32 winding;  ///< +1 si montant, -1 si descendant
        F26Dot6 xCurrent; ///< Position X courante à la scanline active (en F26Dot6)
        F26Dot6 xStep;    ///< Incrément X par scanline (dx/dy en F26Dot6)
    };

    // =========================================================================
    //  NkEdgeList — liste d'arêtes triées
    // =========================================================================

    struct NKENTSEU_FONT_API NkEdgeList {
        NkEdge*  edges;
        uint32   numEdges;
        uint32   capacity;
        int32    yMin;     ///< Y minimum de toutes les arêtes (en pixels)
        int32    yMax;     ///< Y maximum

        void Init(NkMemArena& arena, uint32 cap) noexcept {
            edges    = arena.Alloc<NkEdge>(cap);
            numEdges = 0;
            capacity = edges ? cap : 0;
            yMin     = 0x7FFFFFFF;
            yMax     = -0x7FFFFFFF;
        }

        bool Add(const NkEdge& e) noexcept {
            if (numEdges >= capacity) return false;
            edges[numEdges++] = e;
            if (e.y0 < yMin) yMin = e.y0 < e.y1 ? e.y0 : e.y1;
            if (e.y1 > yMax) yMax = e.y0 > e.y1 ? e.y0 : e.y1;
            return true;
        }

        /// Tri par y0 croissant (insertion sort — liste courte)
        void SortByY() noexcept;
    };

    // =========================================================================
    //  NkOutline
    // =========================================================================

    struct NKENTSEU_FONT_API NkOutline {

        NkPoint26_6* points;       ///< Tableau de tous les points
        uint8*       tags;         ///< Tag de chaque point (NK_TAG_*)
        uint16*      contourEnds;  ///< Indice du dernier point de chaque contour
        uint16       numPoints;
        uint16       numContours;

        // Bounding box en F26Dot6
        F26Dot6 xMin, yMin, xMax, yMax;

        // ── Construction ─────────────────────────────────────────────────────

        static NkOutline Empty() noexcept {
            NkOutline o; o.points = nullptr; o.tags = nullptr;
            o.contourEnds = nullptr; o.numPoints = 0; o.numContours = 0;
            o.xMin = o.yMin = o.xMax = o.yMax = F26Dot6::Zero();
            return o;
        }

        /**
         * @Brief Construit depuis les données de NkTTFParser::DecomposeGlyph.
         */
        static NkOutline FromDecomposed(
            NkPoint26_6* points, uint8* tags,
            uint16* contourEnds, uint16 numPoints, uint16 numContours
        ) noexcept {
            NkOutline o;
            o.points      = points;
            o.tags        = tags;
            o.contourEnds = contourEnds;
            o.numPoints   = numPoints;
            o.numContours = numContours;
            o.ComputeBBox();
            return o;
        }

        // ── Transformation ────────────────────────────────────────────────────

        /**
         * @Brief Applique une translation à tous les points.
         */
        void Translate(F26Dot6 dx, F26Dot6 dy) noexcept {
            for (uint16 i = 0; i < numPoints; ++i) {
                points[i].x += dx;
                points[i].y += dy;
            }
            xMin += dx; xMax += dx;
            yMin += dy; yMax += dy;
        }

        /**
         * @Brief Flipe l'axe Y (conversion TrueType → écran).
         *        Applique y' = yBaseline - y pour chaque point.
         */
        void FlipY(F26Dot6 yBaseline) noexcept {
            for (uint16 i = 0; i < numPoints; ++i)
                points[i].y = yBaseline - points[i].y;
            const F26Dot6 newYMin = yBaseline - yMax;
            const F26Dot6 newYMax = yBaseline - yMin;
            yMin = newYMin; yMax = newYMax;
        }

        // ── Calcul de la bounding box ─────────────────────────────────────────

        void ComputeBBox() noexcept {
            if (numPoints == 0) return;
            xMin = xMax = points[0].x;
            yMin = yMax = points[0].y;
            for (uint16 i = 1; i < numPoints; ++i) {
                if (points[i].x < xMin) xMin = points[i].x;
                if (points[i].x > xMax) xMax = points[i].x;
                if (points[i].y < yMin) yMin = points[i].y;
                if (points[i].y > yMax) yMax = points[i].y;
            }
        }

        // ── Décomposition en arêtes ───────────────────────────────────────────

        /**
         * @Brief Décompose tous les contours en arêtes linéaires.
         *        Reconstruit les courbes implicites TrueType.
         *        Résultat dans NkEdgeList.
         * @param tolerance  Tolérance Bézier en F26Dot6 (défaut 8 = 0.125 px).
         */
        bool Flatten(NkEdgeList& edges, NkMemArena& arena,
                     int32 tolerance = 8) const noexcept;

    private:
        void FlattenContour(uint16 start, uint16 end,
                             NkEdgeList& edges, int32 tolerance) const noexcept;
        static void AddEdge(NkEdgeList& edges,
                             NkPoint26_6 p0, NkPoint26_6 p1) noexcept;

        struct FlattenCtx { NkEdgeList* edges; int32 tolerance; };
        static void OnSegment(NkPoint26_6 p0, NkPoint26_6 p1, void* ud) noexcept;
    };

} // namespace nkentseu
