#pragma once
/**
 * @File    NkType1Parser.h
 * @Brief   Parser PostScript Type 1 — formats PFB et PFA.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Type 1 est un format de police PostScript vectoriel. Les contours sont
 *  encodés comme des programmes PostScript "charstring" (version 1, distinct
 *  de Type 2 / CFF).
 *
 *  Deux formats de fichier :
 *    PFB (Printer Font Binary) : segments binaires avec headers.
 *    PFA (Printer Font ASCII)  : texte ASCII, parties binaires en hex.
 *
 *  Pipeline :
 *    1. Décodage PFB/PFA → flux PostScript brut
 *    2. Parsing du dictionnaire PostScript (FontInfo, Encoding, CharStrings)
 *    3. Déchiffrement eexec (clé 55665) → segment privé
 *    4. Déchiffrement charstring (clé 4330) → bytecode Type 1
 *    5. Interpréteur Type 1 → contours NkCFFGlyph (réutilisé)
 *
 *  Opérateurs Type 1 supportés :
 *    hstem, vstem, vmoveto, rlineto, hlineto, vlineto,
 *    rrcurveto, closepath, callsubr, return, endchar,
 *    rmoveto, hmoveto, vhcurveto, hvcurveto,
 *    dotsection, seac, sbw, div, callothersubr, pop, setcurrentpoint
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkCFFParser.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    struct NKENTSEU_FONT_API NkType1Font {
        // Métriques globales
        int32   unitsPerEm   = 1000; ///< Généralement 1000 pour Type 1
        int32   fontBBox[4]  = {};   ///< xMin, yMin, xMax, yMax
        int32   italicAngle  = 0;
        int32   underlinePosition  = -100;
        int32   underlineThickness = 50;
        bool    isFixedPitch = false;

        // CharStrings (partagent la structure CFF)
        NkCFFFont cff;        ///< Réutilise l'infrastructure CFF pour les glyphes

        // Buffer décodé (owning — alloué dans l'arena)
        uint8*  decodedData  = nullptr;
        usize   decodedSize  = 0;
        bool    isValid      = false;

        // Private Dict
        int32   defaultWidthX = 0;
        int32   nominalWidthX = 0;

        // Subrs (appels de sous-routines dans les charstrings)
        static constexpr int32 MAX_SUBRS = 256;
        struct SubrData { const uint8* data; int32 size; };
        SubrData subrs[MAX_SUBRS] = {};
        int32    numSubrs = 0;

        // Multiple Master (blend)
        static constexpr int32 MAX_MASTERS = 16;
        float   weightVector[MAX_MASTERS] = {1.f};
        int32   numMasters  = 1;

        // Standard Encoding (pour seac)
        const char* standardEncoding[256] = {};
    };

    class NKENTSEU_FONT_API NkType1Parser {
    public:
        static bool Parse(
            const uint8* data,
            usize        size,
            NkMemArena&  arena,
            NkType1Font& out
        ) noexcept;

        /// Interprète un charstring Type 1 → contours (réutilise NkCFFGlyph)
        static bool ParseGlyph(
            const NkType1Font& font,
            uint32 glyphId,
            NkMemArena& arena,
            NkCFFGlyph& out
        ) noexcept;

    private:
        // Décodage PFB/PFA
        static bool DecodePFB(const uint8* data, usize size,
                               NkMemArena& arena, uint8*& out, usize& outSize) noexcept;
        static bool DecodePFA(const uint8* data, usize size,
                               NkMemArena& arena, uint8*& out, usize& outSize) noexcept;

        // Déchiffrement eexec (algorithme RC4-like Type 1)
        static void EexecDecrypt(uint8* data, usize size, uint16 key) noexcept;

        // Déchiffrement charstring
        static void CharstringDecrypt(uint8* data, usize size, uint16 key) noexcept;

        // Parsing PostScript simplifié
        static bool ParseFontDict(const uint8* data, usize size,
                                   NkType1Font& out, NkMemArena& arena) noexcept;
        static bool ParsePrivateDict(const uint8* data, usize size,
                                      NkType1Font& out, NkMemArena& arena) noexcept;

        // Interpréteur Type 1 (distinct de Type 2)
        struct T1State {
            float  stack[24];
            int32  stackTop;
            float  x, y;
            float  sbx, sby;          // side bearing
            float  advanceWidth;
            bool   widthRead;
            bool   inPath;
            NkCFFParser::T2State* t2; // Réutilise le builder de contours CFF
        };

        static bool InterpretType1(
            const uint8* cs, uint32 csLen,
            const NkType1Font& font,
            T1State& state,
            int32 depth
        ) noexcept;
    };

} // namespace nkentseu
