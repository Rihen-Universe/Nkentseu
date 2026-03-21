#pragma once
/**
 * @File    NkShaper.h
 * @Brief   Shaping OpenType — GSUB (substitutions) + GPOS (positionnement).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Le shaping transforme une séquence de glyphes en une séquence
 *  visuellement correcte pour le script et la langue.
 *
 *  GSUB — substitutions supportées :
 *    Lookup type 1 : Single substitution (glyphe → glyphe)
 *    Lookup type 3 : Alternate substitution
 *    Lookup type 4 : Ligature substitution (séquence → glyphe)
 *    Lookup type 6 : Chained context substitution (contexte de 3 séquences)
 *
 *  GPOS — positionnement supporté :
 *    Lookup type 1 : Single adjustment (ValueRecord par glyphe)
 *    Lookup type 2 : Pair adjustment (kerning GPOS)
 *    Lookup type 4 : Mark-to-Base attachment
 *    Lookup type 6 : Mark-to-Mark attachment
 *
 *  NkKerning : encapsule les deux sources de kerning
 *    - Table kern format 0 (classique TrueType)
 *    - GPOS lookup type 2 (OpenType moderne)
 *
 *  Pipeline de shaping :
 *    1. UTF-8 → codepoints (NkUnicode)
 *    2. Bidi resolve (NkUnicode)
 *    3. Codepoints → glyphe IDs (NkTTFFont::GetGlyphId)
 *    4. GSUB : substitutions de ligatures et variantes
 *    5. GPOS : ajustements de position et kerning
 *    6. Sortie : NkGlyphRun
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkTTFParser.h"
#include "NKFont/NkUnicode.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkFixed26_6.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  NkGlyphInfo — glyphe positionné après shaping
    // =========================================================================

    struct NkGlyphInfo {
        uint16  glyphId;      ///< ID dans la police
        uint32  codepoint;    ///< Codepoint Unicode d'origine
        F26Dot6 xAdvance;     ///< Avance horizontale (kerning inclus)
        F26Dot6 yAdvance;     ///< Avance verticale (0 pour LTR)
        F26Dot6 xOffset;      ///< Décalage X (GPOS mark attachment)
        F26Dot6 yOffset;      ///< Décalage Y (GPOS mark attachment)
        uint8   level;        ///< Niveau bidi
        bool    isRTL;        ///< Ce glyphe est dans un run RTL
        bool    isMark;       ///< Marque diacritique (Mn, Mc, Me)
        bool    isLigature;   ///< Résultat d'une ligature GSUB
        uint8   ligComponents; ///< Nombre de codepoints combinés
    };

    // =========================================================================
    //  NkGlyphRun — séquence de glyphes prête pour le rendu
    // =========================================================================

    struct NKENTSEU_FONT_API NkGlyphRun {
        NkGlyphInfo* glyphs;
        uint32       numGlyphs;
        F26Dot6      totalAdvance;  ///< Largeur totale du run
        bool         isRTL;

        NKFONT_NODISCARD NkGlyphInfo& operator[](uint32 i) noexcept { return glyphs[i]; }
        NKFONT_NODISCARD const NkGlyphInfo& operator[](uint32 i) const noexcept { return glyphs[i]; }
    };

    // =========================================================================
    //  NkKerning
    // =========================================================================

    class NKENTSEU_FONT_API NkKerning {
    public:
        /**
         * @Brief Retourne le kerning entre deux glyphes consécutifs.
         *        Cherche d'abord dans GPOS (si parsé), sinon dans kern.
         * @param font    Police parsée.
         * @param left    Glyphe gauche.
         * @param right   Glyphe droit.
         * @param ppem    Taille courante (pour conversion design→pixels).
         * @return Décalage X en F26Dot6 (négatif = rapprochement).
         */
        NKFONT_NODISCARD static F26Dot6 GetKerning(
            const NkTTFFont& font,
            uint16 left, uint16 right,
            uint16 ppem
        ) noexcept;

    private:
        static F26Dot6 GetKernTable(const NkTTFFont& font,
                                     uint16 left, uint16 right, uint16 ppem) noexcept;
        static F26Dot6 GetGPOSKern(const NkTTFFont& font,
                                    uint16 left, uint16 right, uint16 ppem,
                                    NkMemArena& tmpArena) noexcept;
    };

    // =========================================================================
    //  NkGSUB — moteur de substitution
    // =========================================================================

    class NKENTSEU_FONT_API NkGSUB {
    public:
        /**
         * @Brief Applique les substitutions GSUB à un tableau de glyphe IDs.
         * @param font       Police parsée.
         * @param glyphs     Tableau de glyphe IDs (modifié en place).
         * @param codepoints Codepoints correspondants.
         * @param count      Nombre de glyphes (peut diminuer après ligatures).
         * @param outCount   Nombre de glyphes après substitution.
         * @param arena      Arena scratch.
         * @param features   Masque de features actives (bit 0=liga, bit 1=calt, etc.)
         */
        static bool Apply(
            const NkTTFFont& font,
            uint16*          glyphs,
            uint32*          codepoints,
            uint32           count,
            uint32&          outCount,
            NkMemArena&      arena,
            uint32           features = 0xFFFFFFFFu
        ) noexcept;

    private:
        // Helpers lecteur GSUB
        struct Coverage {
            const uint8* data;
            uint32       size;
            int32 Lookup(uint16 glyphId) const noexcept; // -1 si absent
        };

        static Coverage ReadCoverage(NkStreamReader& s) noexcept;
        static bool ApplyLookup(
            NkStreamReader& lookupData,
            uint16* glyphs, uint32* cps,
            uint32& count, uint32 pos,
            NkMemArena& arena
        ) noexcept;
        static bool ApplySingle   (NkStreamReader& s, uint16& glyph) noexcept;
        static bool ApplyLigature (NkStreamReader& s, uint16* glyphs,
                                    uint32& count, uint32 pos) noexcept;
    };

    // =========================================================================
    //  NkShaper — pipeline complet
    // =========================================================================

    class NKENTSEU_FONT_API NkShaper {
    public:

        struct Options {
            uint32  features    = 0xFFFFFFFFu; ///< Masque GSUB features
            bool    enableKern  = true;
            bool    enableBidi  = true;
            int32   baseLevel   = -1;          ///< -1 = auto-détecté
            uint16  ppem        = 16;
        };

        /**
         * @Brief Pipeline complet : UTF-8 → NkGlyphRun.
         * @param font    Police parsée.
         * @param utf8    Texte en UTF-8.
         * @param byteLen Longueur du texte.
         * @param arena   Arena pour le résultat et les buffers.
         * @param out     Run de glyphes résultant.
         * @param opts    Options de shaping.
         */
        static bool Shape(
            const NkTTFFont& font,
            const uint8*     utf8,
            usize            byteLen,
            NkMemArena&      arena,
            NkGlyphRun&      out,
            const Options&   opts = {}
        ) noexcept;

        /// Version pratique avec C-string (null-terminated).
        static bool Shape(
            const NkTTFFont& font,
            const char*      text,
            NkMemArena&      arena,
            NkGlyphRun&      out,
            const Options&   opts = {}
        ) noexcept;
    };

} // namespace nkentseu
