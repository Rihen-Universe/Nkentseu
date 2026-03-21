#pragma once
/**
 * @File    NkBDFParser.h
 * @Brief   Parser BDF (Bitmap Distribution Format) et PCF (Portable Compiled Format).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  BDF : format texte ASCII. Chaque glyphe est décrit par un header
 *  et une bitmap hexadécimale.
 *
 *  PCF : format binaire compilé de X11. Plus rapide à lire que BDF.
 *  Les deux formats partagent la même structure de sortie NkBDFFont.
 *
 *  Sortie : tableau de NkBDFGlyph avec bitmap pré-décodée (1 bit/pixel
 *  converti en uint8 pour compatibilité avec NkBitmap).
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkStreamReader.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkBitmap.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // ── Glyphe BDF/PCF ────────────────────────────────────────────────────────

    struct NkBDFGlyph {
        uint32   encoding;      ///< Codepoint Unicode
        int32    dwidthX;       ///< Advance width en pixels
        int32    dwidthY;       ///< Advance height (0 pour écriture horizontale)
        int32    bbxWidth;      ///< Largeur de la bounding box en pixels
        int32    bbxHeight;     ///< Hauteur de la bounding box
        int32    bbxOffX;       ///< Offset X du coin inférieur gauche
        int32    bbxOffY;       ///< Offset Y du coin inférieur gauche
        NkBitmap bitmap;        ///< Pixels Gray8 (0 ou 255) — alloués dans l'arena
    };

    /// Métriques globales BDF/PCF
    struct NkBDFMetrics {
        int32  pointSize;       ///< Taille en dix-points (ex: 120 = 12pt)
        int32  resolutionX;     ///< DPI horizontal
        int32  resolutionY;     ///< DPI vertical
        int32  fontAscent;      ///< Ascendante en pixels
        int32  fontDescent;     ///< Descendante en pixels
        uint32 defaultChar;     ///< Codepoint du glyphe par défaut
    };

    struct NKENTSEU_FONT_API NkBDFFont {
        NkBDFMetrics metrics;
        NkBDFGlyph*  glyphs;
        uint32       numGlyphs;
        bool         isValid;

        /// Recherche un glyphe par encoding (recherche linéaire — tables petites).
        NKFONT_NODISCARD const NkBDFGlyph* FindGlyph(uint32 encoding) const noexcept;
    };

    // =========================================================================
    //  NkBDFParser
    // =========================================================================

    class NKENTSEU_FONT_API NkBDFParser {
    public:
        static bool Parse(
            const uint8* data, usize size,
            NkMemArena& arena, NkBDFFont& out
        ) noexcept;

    private:
        static bool ParseLine(NkStreamReader& s, char* buf, usize bufSize) noexcept;
        static bool ParseInt (const char* s, int32& out) noexcept;
        static void HexRowToBitmap(const char* hex, NkBDFGlyph& glyph, int32 row) noexcept;
    };

    // =========================================================================
    //  NkPCFParser
    // =========================================================================

    class NKENTSEU_FONT_API NkPCFParser {
    public:
        static bool Parse(
            const uint8* data, usize size,
            NkMemArena& arena, NkBDFFont& out
        ) noexcept;

    private:
        static constexpr uint32 PCF_MAGIC        = 0x70636601u; // "\1fcp"
        static constexpr uint32 PCF_PROPERTIES   = 1 << 0;
        static constexpr uint32 PCF_METRICS      = 1 << 2;
        static constexpr uint32 PCF_BITMAPS      = 1 << 3;
        static constexpr uint32 PCF_BDF_ENCODINGS= 1 << 5;
        static constexpr uint32 PCF_BDF_ACCELERATORS = 1 << 8;

        static constexpr uint32 PCF_DEFAULT_FORMAT   = 0x00000000u;
        static constexpr uint32 PCF_BYTE_MASK        = 1 << 2;
        static constexpr uint32 PCF_BIT_MASK         = 1 << 3;
        static constexpr uint32 PCF_SCAN_UNIT_MASK   = 3;

        struct TableEntry {
            uint32 type;
            uint32 format;
            uint32 size;
            uint32 offset;
        };

        static bool ParseMetricsTable (NkStreamReader& s, uint32 format,
                                        NkBDFFont& out, NkMemArena& arena) noexcept;
        static bool ParseBitmapsTable (NkStreamReader& s, uint32 format,
                                        NkBDFFont& out, NkMemArena& arena) noexcept;
        static bool ParseEncodingTable(NkStreamReader& s, uint32 format,
                                        NkBDFFont& out) noexcept;
        static bool ParseAccelTable   (NkStreamReader& s, uint32 format,
                                        NkBDFFont& out) noexcept;
    };

} // namespace nkentseu
