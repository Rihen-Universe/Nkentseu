#pragma once
/**
 * @File    NkUnicode.h
 * @Brief   Tables Unicode, normalisation, algorithme bidi UAX#9.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Ce module fournit trois services :
 *
 *  1. Classification Unicode
 *     - Catégorie générale (Letter, Number, Punctuation, etc.)
 *     - Propriétés bidi (L, R, AL, EN, AN, NSM, etc.)
 *     - Mirroring (parenthèses, crochets)
 *     - Jointure (pour l'arabe/hébreu)
 *
 *  2. Normalisation
 *     - UTF-8 → codepoints uint32[]
 *     - UTF-16 → codepoints uint32[] (surrogate pairs)
 *     - Codepoints → UTF-8
 *
 *  3. Algorithme bidirectionnel Unicode (UAX#9, simplifié)
 *     - Résolution des niveaux bidi (L2R = pair, R2L = impair)
 *     - Réordonnancement visuel des runs
 *     - Résolution des types neutres (NSM, WS, ON)
 *
 *  Tables embarquées (compactes) :
 *    - Table de catégories sur 2 niveaux (256 + N entrées)
 *    - Table bidi type (même structure)
 *    - Pas de ICU, pas de table complète 1 Mo — couverture Latin + Arabe + Hébreu
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkMemArena.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  Types Unicode
    // =========================================================================

    enum class NkUniCategory : uint8 {
        Lu, Ll, Lt, Lm, Lo,          // Letters
        Mn, Mc, Me,                   // Marks
        Nd, Nl, No,                   // Numbers
        Pc, Pd, Ps, Pe, Pi, Pf, Po,  // Punctuation
        Sm, Sc, Sk, So,               // Symbols
        Zs, Zl, Zp,                   // Separators
        Cc, Cf, Cs, Co, Cn           // Others
    };

    enum class NkBidiType : uint8 {
        L   = 0,   ///< Left-to-Right
        R   = 1,   ///< Right-to-Left
        AL  = 2,   ///< Arabic Letter
        EN  = 3,   ///< European Number
        ES  = 4,   ///< European Separator
        ET  = 5,   ///< European Terminator
        AN  = 6,   ///< Arabic Number
        CS  = 7,   ///< Common Separator
        NSM = 8,   ///< Non-Spacing Mark
        BN  = 9,   ///< Boundary Neutral
        B   = 10,  ///< Paragraph Separator
        S   = 11,  ///< Segment Separator
        WS  = 12,  ///< Whitespace
        ON  = 13,  ///< Other Neutral
        LRE = 14, LRO = 15, RLE = 16, RLO = 17, PDF = 18,
        LRI = 19, RLI = 20, FSI = 21, PDI = 22
    };

    // =========================================================================
    //  NkUnicode — fonctions statiques
    // =========================================================================

    class NKENTSEU_FONT_API NkUnicode {
    public:

        // ── Classification ────────────────────────────────────────────────────

        NKFONT_NODISCARD static NkUniCategory GetCategory(uint32 cp) noexcept;
        NKFONT_NODISCARD static NkBidiType    GetBidiType (uint32 cp) noexcept;
        NKFONT_NODISCARD static bool          IsMirrored  (uint32 cp) noexcept;
        NKFONT_NODISCARD static uint32        GetMirror   (uint32 cp) noexcept;

        // Propriétés rapides
        NKFONT_NODISCARD static bool IsLetter     (uint32 cp) noexcept;
        NKFONT_NODISCARD static bool IsDigit      (uint32 cp) noexcept;
        NKFONT_NODISCARD static bool IsWhitespace (uint32 cp) noexcept;
        NKFONT_NODISCARD static bool IsRTL        (uint32 cp) noexcept; // R ou AL
        NKFONT_NODISCARD static bool IsArabic     (uint32 cp) noexcept;
        NKFONT_NODISCARD static bool IsHebrew     (uint32 cp) noexcept;
        NKFONT_NODISCARD static bool IsCombining  (uint32 cp) noexcept; // Mn, Mc, Me

        // ── Encodage UTF-8 ────────────────────────────────────────────────────

        /**
         * @Brief Décode une séquence UTF-8 en codepoints uint32.
         * @param utf8     Données UTF-8.
         * @param byteLen  Longueur en octets.
         * @param arena    Arena pour le tableau de sortie.
         * @param outCps   Tableau de codepoints alloué dans arena.
         * @param outLen   Nombre de codepoints.
         * @return false si séquence invalide (remplace par U+FFFD).
         */
        static bool UTF8Decode(
            const uint8*  utf8,
            usize         byteLen,
            NkMemArena&   arena,
            uint32*&      outCps,
            uint32&       outLen
        ) noexcept;

        /**
         * @Brief Encode un tableau de codepoints en UTF-8.
         * @return Nombre d'octets écrits dans dst.
         */
        static usize UTF8Encode(
            const uint32* codepoints,
            uint32        count,
            uint8*        dst,
            usize         dstSize
        ) noexcept;

        /**
         * @Brief Décode une séquence UTF-16 (little-endian) en codepoints.
         */
        static bool UTF16Decode(
            const uint16* utf16,
            usize         wordLen,
            NkMemArena&   arena,
            uint32*&      outCps,
            uint32&       outLen
        ) noexcept;

        // ── Algorithme bidi (UAX#9 simplifié) ────────────────────────────────

        /**
         * @Brief Résout les niveaux bidi d'une ligne de codepoints.
         * @param codepoints  Tableau de codepoints Unicode.
         * @param count       Nombre de codepoints.
         * @param arena       Arena pour les buffers intermédiaires.
         * @param outLevels   Niveaux bidi [0..count-1] (alloué dans arena).
         *                    Level pair = LTR, impair = RTL.
         * @param baseLevel   Niveau de base du paragraphe (0=LTR, 1=RTL).
         *                    -1 = auto-détecté (premier strong char).
         * @return true si succès.
         */
        static bool BidiResolve(
            const uint32* codepoints,
            uint32        count,
            NkMemArena&   arena,
            uint8*&       outLevels,
            int32         baseLevel = -1
        ) noexcept;

        /**
         * @Brief Réordonne visuellement un tableau d'indices selon les niveaux bidi.
         * @param levels      Niveaux bidi [0..count-1].
         * @param count       Nombre d'éléments.
         * @param arena       Arena scratch.
         * @param outOrder    Tableau d'indices réordonnés (alloué dans arena).
         */
        static bool BidiReorder(
            const uint8*  levels,
            uint32        count,
            NkMemArena&   arena,
            uint32*&      outOrder
        ) noexcept;

    private:
        // Tables compactes (définies dans le .cpp)
        static const uint8  kCategoryPage0[256];
        static const uint8  kBidiPage0[256];
        static uint8        LookupCategory(uint32 cp) noexcept;
        static uint8        LookupBidi    (uint32 cp) noexcept;

        // Décodage UTF-8 d'un seul codepoint
        static uint32 DecodeOneUTF8(const uint8* src, usize rem, uint32& advance) noexcept;

        // Bidi internal
        static int32  DetectBaseLevel(const uint32* cps, uint32 count) noexcept;
        static void   ResolveWeakTypes(NkBidiType* types, const uint8* levels,
                                        uint32 count) noexcept;
        static void   ResolveNeutralTypes(NkBidiType* types, const uint8* levels,
                                           uint32 count, int32 paraLevel) noexcept;
        static void   ResolveImplicitLevels(uint8* levels, const NkBidiType* types,
                                             uint32 count) noexcept;
    };

} // namespace nkentseu
