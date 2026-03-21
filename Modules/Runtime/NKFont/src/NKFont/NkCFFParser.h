#pragma once
/**
 * @File    NkCFFParser.h
 * @Brief   Parser CFF/CFF2 (Compact Font Format) — OpenType PostScript outlines.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  CFF est le format de contours utilisé dans les fichiers .otf (OpenType
 *  avec outlines PostScript). Il remplace la table glyf de TrueType.
 *
 *  Structure CFF :
 *    Header → Name INDEX → Top DICT INDEX → String INDEX →
 *    Global Subr INDEX → Encodings → Charsets → CharStrings INDEX
 *
 *  Un CharString CFF est un programme PostScript Type 2 (stack machine).
 *  On implémente un interpréteur Type 2 complet.
 *
 *  Opérateurs Type 2 supportés :
 *    Contour : rmoveto, hmoveto, vmoveto
 *              rlineto, hlineto, vlineto
 *              rrcurveto, hhcurveto, vvcurveto, hvcurveto, vhcurveto
 *              rcurveline, rlinecurve, flex, flex1, hflex, hflex1
 *    Largeur : implicit width (premier argument de moveto)
 *    Hints   : hstem, vstem, hstemhm, vstemhm, hintmask, cntrmask
 *    Autres  : endchar, return, callsubr, callgsubr
 *              and, or, not, abs, add, sub, div, neg, eq, ifelse, random, mul, sqrt
 *              blend (CFF2 variable fonts — ignoré)
 *
 *  Sortie : NkTTFGlyphData réutilisée (contours en design units),
 *           compatible avec DecomposeGlyph du rasterizer.
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkStreamReader.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkFixed26_6.h"
#include "NKFont/NkBezier.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  Structures CFF
    // =========================================================================

    /// Entrée d'un INDEX CFF (offset + longueur dans le buffer)
    struct NkCFFIndexEntry {
        uint32 offset;
        uint32 length;
    };

    /// Top DICT — paramètres globaux de la police CFF
    struct NkCFFTopDict {
        int32  charStringsOffset;  ///< Offset du CharStrings INDEX
        int32  privateOffset;      ///< Offset du Private DICT
        int32  privateLength;      ///< Longueur du Private DICT
        int32  charsetOffset;      ///< Offset du Charset
        int32  encodingOffset;     ///< Offset de l'Encoding
        int32  defaultWidthX;     ///< Largeur par défaut (Private DICT)
        int32  nominalWidthX;     ///< Largeur nominale (Private DICT)
        int32  subrsOffset;       ///< Offset des Local Subrs (Private DICT)
        bool   isCIDFont;
    };

    /// Point de contour CFF (coordonnées en design units, float pour Type 2)
    struct NkCFFPoint {
        float x, y;
        bool  onCurve;  ///< false = point de contrôle cubique
    };

    /// Un contour CFF
    struct NkCFFContour {
        NkCFFPoint* points;
        uint32      numPoints;
    };

    /// Résultat du parsing d'un glyphe CFF
    struct NkCFFGlyph {
        NkCFFContour* contours;
        uint32        numContours;
        float         advanceWidth;
        float         lsb;
    };

    /// Résultat global du parsing CFF
    struct NKENTSEU_FONT_API NkCFFFont {
        NkCFFTopDict      topDict;
        NkCFFIndexEntry*  charStrings;   ///< INDEX des CharStrings
        uint32            numGlyphs;
        const uint8*      rawData;       ///< Pointeur dans le buffer CFF
        usize             rawSize;
        const uint8*      globalSubrs;   ///< Global Subr INDEX (brut)
        uint32            globalSubrsSize;
        const uint8*      localSubrs;    ///< Local Subr INDEX (brut)
        uint32            localSubrsSize;
        uint32            globalSubrBias; ///< Biais de l'index (dépend du count)
        uint32            localSubrBias;
        bool              isValid;
    };

    // =========================================================================
    //  NkCFFParser
    // =========================================================================

    class NKENTSEU_FONT_API NkCFFParser {
    public:

        /**
         * @Brief Parse la table CFF d'un fichier OTF.
         * @param cffData  Pointeur sur la table CFF (doit rester valide).
         * @param cffSize  Taille de la table CFF.
         * @param arena    Arena pour les allocations.
         * @param out      Résultat.
         */
        static bool Parse(
            const uint8* cffData,
            usize        cffSize,
            NkMemArena&  arena,
            NkCFFFont&   out
        ) noexcept;

        /**
         * @Brief Interprète le CharString d'un glyphe et produit ses contours.
         * @param font     Police CFF parsée.
         * @param glyphId  ID du glyphe.
         * @param arena    Arena scratch.
         * @param out      Glyphe résultant.
         */
        static bool ParseGlyph(
            const NkCFFFont& font,
            uint32           glyphId,
            NkMemArena&      arena,
            NkCFFGlyph&      out
        ) noexcept;

    private:
        // Parsing des structures CFF
        static bool ParseHeader   (NkStreamReader& s, uint32& hdrSize, uint8& offSize) noexcept;
        static bool ParseINDEX    (NkStreamReader& s, NkCFFIndexEntry*& entries, uint32& count, NkMemArena& arena) noexcept;
        static bool ParseTopDict  (const uint8* dictData, uint32 dictSize, NkCFFTopDict& out) noexcept;
        static bool ParsePrivateDict(const uint8* data, uint32 size, NkCFFTopDict& out) noexcept;
        static uint32 CalcSubrBias(uint32 count) noexcept;

        // Interpréteur Type 2
        struct T2State {
            float  stack[48];        // Stack Type 2 (max 48 pour CFF2)
            int32  stackTop;
            float  x, y;            // position courante
            float  defaultWidth;
            float  nominalWidth;
            float  advanceWidth;
            bool   widthRead;
            bool   inPath;

            // Hints
            uint8  hintMask[12];    // max 96 hints
            uint8  cntrMask[12];
            int32  numHints;

            // Contours en construction
            NkCFFPoint* points;
            uint32*     contourStarts;
            uint32      numPoints;
            uint32      numContours;
            uint32      maxPoints;
            uint32      maxContours;
            NkMemArena* arena;
        };

        static bool InterpretCharString(
            const uint8* cs, uint32 csLen,
            const NkCFFFont& font,
            T2State& state,
            int32 depth
        ) noexcept;

        static void T2_MoveTo(T2State& s, float dx, float dy) noexcept;
        static void T2_LineTo(T2State& s, float dx, float dy) noexcept;
        static void T2_CurveTo(T2State& s, float dx1, float dy1,
                                            float dx2, float dy2,
                                            float dx3, float dy3) noexcept;
        static void T2_ClosePath(T2State& s) noexcept;
        static void T2_PushPoint(T2State& s, float x, float y, bool onCurve) noexcept;

        static float T2_ReadNumber(NkStreamReader& sr, uint8 b0) noexcept;
        static bool  T2_CallSubr(const NkCFFFont& font, bool isGlobal,
                                  int32 idx, T2State& state, int32 depth) noexcept;
    };

} // namespace nkentseu
