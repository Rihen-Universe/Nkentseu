#pragma once
/**
 * @File    NkScanline.h
 * @Brief   Rasterizer scanline avec antialiasing 256 niveaux.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Algorithme : scanline rendering avec accumulation de couverture.
 *  Inspiré de l'algorithme "Anti-Grain Geometry" et de FreeType.
 *
 *  Principe :
 *    1. Pour chaque scanline Y de yMin à yMax :
 *       a. Collecte les arêtes actives (AEL — Active Edge List)
 *       b. Trie par X croissant
 *       c. Parcourt les paires d'arêtes (winding rule)
 *       d. Accumule la couverture sub-pixel dans un buffer de spans
 *       e. Rend les spans dans le bitmap de sortie
 *
 *  Antialiasing : super-sampling vertical 4x (Y divisé en 4 sous-pixels).
 *    Chaque sous-scanline accumule 1/4 de couverture.
 *    Résultat : 0-255 niveaux de gris (4 × 64 = 256).
 *
 *  Winding rules supportés :
 *    - Non-zero (défaut TrueType) : fill si winding != 0
 *    - Even-odd (PostScript)      : fill si winding impair
 *
 *  Performance :
 *    - Zéro allocation dans la boucle interne (arena pré-alloué)
 *    - AEL limité à 256 arêtes actives simultanées
 *    - Spans accumulés en int16 (couverture × 4)
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkOutline.h"
#include "NKFont/NkBitmap.h"
#include "NKFont/NkMemArena.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // ── Winding rule ──────────────────────────────────────────────────────────
    enum class NkWindingRule : uint8 {
        NonZero = 0,   ///< TrueType, OpenType
        EvenOdd = 1,   ///< PostScript, SVG
    };

    // =========================================================================
    //  NkSpan — segment horizontal de couverture
    // =========================================================================

    struct NkSpan {
        int32 x;          ///< Position X de début
        int32 len;        ///< Longueur en pixels
        uint8 coverage;   ///< Couverture [0, 255]
    };

    // =========================================================================
    //  NkScanline
    // =========================================================================

    class NKENTSEU_FONT_API NkScanline {
    public:

        /**
         * @Brief Rasterize une NkOutline dans un NkBitmap.
         *
         * @param outline   Contours aplatis (depuis NkOutline::Flatten).
         * @param edges     Arêtes produites par Flatten (triées par Y).
         * @param bitmap    Bitmap de destination (doit être alloué).
         * @param arena     Arena scratch (buffer temporaire AEL + spans).
         * @param rule      Règle de remplissage (non-zero ou even-odd).
         * @param offsetX   Décalage X du glyphe dans le bitmap (en pixels).
         * @param offsetY   Décalage Y du glyphe dans le bitmap (en pixels).
         */
        static void Rasterize(
            const NkEdgeList& edges,
            NkBitmap&         bitmap,
            NkMemArena&       arena,
            NkWindingRule     rule    = NkWindingRule::NonZero,
            int32             offsetX = 0,
            int32             offsetY = 0
        ) noexcept;

        /**
         * @Brief Pipeline complet : Outline → Flatten → Rasterize.
         *        Crée le bitmap, alloue les arêtes, rasterize.
         */
        static bool RasterizeOutline(
            const NkOutline& outline,
            NkBitmap&        outBitmap,
            NkMemArena&      arena,
            NkWindingRule    rule       = NkWindingRule::NonZero,
            int32            tolerance  = 8
        ) noexcept;

    private:
        // ── Active Edge List ──────────────────────────────────────────────────
        struct ActiveEdge {
            F26Dot6  xCurrent;   ///< X courant en F26Dot6
            F26Dot6  xStep;      ///< Incrément par scanline
            int32    yEnd;       ///< Dernière scanline de cette arête
            int32    winding;    ///< +1 ou -1
        };

        static constexpr int32 MAX_ACTIVE_EDGES = 256;
        static constexpr int32 SUBPIXELS        = 4;    ///< Super-sampling vertical
        static constexpr int32 SUBPIXEL_SHIFT   = 2;    ///< log2(SUBPIXELS)
        static constexpr int32 COVERAGE_UNIT    = 256 / SUBPIXELS; ///< 64 par sous-pixel

        // ── Méthodes internes ─────────────────────────────────────────────────
        static void RasterizeScanline(
            ActiveEdge*  ael,
            int32        aelCount,
            int32        y,
            int32        subY,
            int32*       coverBuffer, ///< Buffer de couverture [width]
            int32        bufWidth,
            int32        offsetX,
            NkWindingRule rule
        ) noexcept;

        static void FlushCoverage(
            int32*    coverBuffer,
            int32     bufWidth,
            uint8*    rowPixels,
            int32     rowWidth
        ) noexcept;

        static void SortAEL(ActiveEdge* ael, int32 count) noexcept;
        static int32 UpdateAEL(ActiveEdge* ael, int32 count, int32 nextY) noexcept;
    };

} // namespace nkentseu
