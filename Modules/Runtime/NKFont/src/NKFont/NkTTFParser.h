#pragma once
/**
 * @File    NkTTFParser.h
 * @Brief   Parser TTF/OTF from scratch — tables essentielles.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkTTFParser lit un fichier .ttf/.otf en mémoire et expose les données
 *  parsées via des structures POD stockées dans un NkMemArena.
 *
 *  Tables parsées :
 *    head  — version, unitsPerEm, bbox global, indexToLocFormat
 *    hhea  — métriques horizontales globales (ascender, descender, lineGap)
 *    maxp  — nombre de glyphes, limites
 *    hmtx  — advance width + lsb par glyphe
 *    cmap  — mapping Unicode → glyphe (formats 4, 12)
 *    loca  — table d'offsets des glyphes
 *    glyf  — contours de glyphes (simple + composite)
 *    kern  — paires de crénage format 0
 *    name  — noms de la police (famille, style, copyright)
 *    OS/2  — métriques typographiques étendues
 *    post  — informations PostScript (isFixedPitch, italicAngle)
 *    cvt   — Control Value Table (hinting)
 *    fpgm  — Font Program (hinting)
 *    prep  — CVT Program (hinting)
 *    GSUB  — substitutions (ligatures, variantes)
 *    GPOS  — positionnement avancé (kerning GPOS)
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkStreamReader.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkFixed26_6.h"
#include "NKFont/NkBezier.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  Constantes
    // =========================================================================

    static constexpr uint32 kTTF_SFVERSION_TRUE = 0x00010000u;
    static constexpr uint32 kTTF_SFVERSION_OTF  = MakeTag('O','T','T','O');
    static constexpr uint32 kTTF_SFVERSION_TRUE2 = MakeTag('t','r','u','e');
    static constexpr uint32 kTTF_SFVERSION_TYPE1 = MakeTag('t','y','p','1');
    static constexpr uint32 kTTF_SFVERSION_WOFF  = MakeTag('w','O','F','F');

    static constexpr uint16 kTTF_MAX_GLYPHS      = 65535u;
    static constexpr uint16 kTTF_MAX_COMPONENTS  = 64u;
    static constexpr uint16 kTTF_MAX_CONTOURS    = 256u;
    static constexpr uint16 kTTF_MAX_POINTS      = 4096u;

    // Flags des points TrueType
    static constexpr uint8 kPointFlag_OnCurve    = 0x01;
    static constexpr uint8 kPointFlag_XShortVec  = 0x02;
    static constexpr uint8 kPointFlag_YShortVec  = 0x04;
    static constexpr uint8 kPointFlag_Repeat     = 0x08;
    static constexpr uint8 kPointFlag_XSameOrPos = 0x10;
    static constexpr uint8 kPointFlag_YSameOrPos = 0x20;
    static constexpr uint8 kPointFlag_Cubic      = 0x01;

    // Flags des composants
    static constexpr uint16 kCompFlag_ArgsAreWords    = 0x0001;
    static constexpr uint16 kCompFlag_ArgsAreXY       = 0x0002;
    static constexpr uint16 kCompFlag_RoundXYToGrid   = 0x0004;
    static constexpr uint16 kCompFlag_WeHaveScale     = 0x0008;
    static constexpr uint16 kCompFlag_MoreComponents  = 0x0020;
    static constexpr uint16 kCompFlag_WeHaveXYScale   = 0x0040;
    static constexpr uint16 kCompFlag_WeHave2x2       = 0x0080;
    static constexpr uint16 kCompFlag_WeHaveInstr     = 0x0100;
    static constexpr uint16 kCompFlag_UseMyMetrics    = 0x0200;
    static constexpr uint16 kCompFlag_OverlapCompound = 0x0400;

    // =========================================================================
    //  Structures de résultat (POD, stockées dans NkMemArena)
    // =========================================================================

    /// Entrée de la table des tables (table directory)
    struct NkTTFTableRecord {
        uint32 tag;
        uint32 checksum;
        uint32 offset;
        uint32 length;
    };

    /// Table head
    struct NkTTFHead {
        uint16 majorVersion;
        uint16 minorVersion;
        int32  fontRevision;      // F16.16
        uint16 unitsPerEm;        // [16, 16384]
        int16  xMin, yMin;        // Bbox global en design units
        int16  xMax, yMax;
        uint16 macStyle;          // Bold=1, Italic=2, Condensed=32, Extended=64
        uint16 lowestRecPPEM;
        int16  fontDirectionHint;
        int16  indexToLocFormat;  // 0 = short offsets, 1 = long offsets
        int16  glyphDataFormat;
    };

    /// Table hhea
    struct NkTTFHhea {
        int16  ascender;
        int16  descender;
        int16  lineGap;
        uint16 advanceWidthMax;
        int16  minLeftSideBearing;
        int16  minRightSideBearing;
        int16  xMaxExtent;
        int16  caretSlopeRise;
        int16  caretSlopeRun;
        int16  caretOffset;
        int16  metricDataFormat;
        uint16 numberOfHMetrics;
    };

    /// Table maxp
    struct NkTTFMaxp {
        uint16 numGlyphs;
        uint16 maxPoints;
        uint16 maxContours;
        uint16 maxCompositePoints;
        uint16 maxCompositeContours;
        uint16 maxZones;
        uint16 maxTwilightPoints;
        uint16 maxStorage;
        uint16 maxFunctionDefs;
        uint16 maxInstructionDefs;
        uint16 maxStackElements;
        uint16 maxSizeOfInstructions;
        uint16 maxComponentElements;
        uint16 maxComponentDepth;
    };

    /// Métriques d'un seul glyphe (depuis hmtx)
    struct NkTTFHMetric {
        uint16 advanceWidth;
        int16  lsb;  // left side bearing
    };

    /// Table OS/2 (champs essentiels)
    struct NkTTFOS2 {
        int16  xAvgCharWidth;
        uint16 usWeightClass;    // 100-900
        uint16 usWidthClass;     // 1-9
        int16  sTypoAscender;
        int16  sTypoDescender;
        int16  sTypoLineGap;
        uint16 usWinAscent;
        uint16 usWinDescent;
        int16  sxHeight;
        int16  sCapHeight;
        uint16 usDefaultChar;
        uint16 usBreakChar;
        uint16 fsType;           // embedding flags
    };

    /// Table post
    struct NkTTFPost {
        int32  italicAngle;      // F16.16
        int16  underlinePosition;
        int16  underlineThickness;
        uint32 isFixedPitch;
        uint32 minMemType42;
        uint32 maxMemType42;
    };

    /// Entrée de la table name
    struct NkTTFNameRecord {
        uint16 platformId;
        uint16 encodingId;
        uint16 languageId;
        uint16 nameId;
        uint16 length;
        uint16 offset;
        const char* string;  // Pointeur vers la chaîne UTF-16 (après parsing)
    };

    /// Résultat du parsing de la table name
    struct NkTTFNameTable {
        NkTTFNameRecord* records;     // Tableau des enregistrements
        uint16           numRecords;  // Nombre d'enregistrements
        const uint8*     stringData;  // Données brutes des chaînes
        
        // Chaînes communes (UTF-8 converties)
        char familyName[64];   ///< Nom de la famille (nameId=1, platform=3, encoding=1)
        char styleName[32];    ///< Nom du style (nameId=2, platform=3, encoding=1)
        char fullName[128];    ///< Nom complet (nameId=4)
        char copyright[256];   ///< Copyright (nameId=0)
    };

    /// Un point de contour (on-curve ou off-curve)
    struct NkTTFPoint {
        int16 x, y;      // coordonnées en design units
        uint8 flags;     // kPointFlag_*
    };

    /// Un contour simple (une forme fermée)
    struct NkTTFContour {
        NkTTFPoint* points;    // alloués dans l'arena
        uint16      numPoints;
    };

    /// Résultat du parsing d'un glyphe simple
    struct NkTTFSimpleGlyph {
        NkTTFContour* contours;    // alloués dans l'arena
        uint16        numContours;
        int16         xMin, yMin;
        int16         xMax, yMax;
        uint8*        instructions;
        uint16        instructionLength;
    };

    /// Un composant d'un glyphe composite
    struct NkTTFComponent {
        uint16      glyphIndex;
        int32       dx, dy;        // translation en design units
        NkMatrix2x2 transform;     // transformation (identité si absente)
        uint16      flags;
        bool        hasTransform;
        bool        useMyMetrics;
    };

    /// Résultat du parsing d'un glyphe composite
    struct NkTTFCompositeGlyph {
        NkTTFComponent* components;
        uint16          numComponents;
        int16           xMin, yMin;
        int16           xMax, yMax;
        uint8*          instructions;
        uint16          instructionLength;
    };

    /// Résultat unifié du parsing d'un glyphe
    struct NkTTFGlyphData {
        int16 numContours;  // > 0 simple, < 0 composite, == 0 vide (espace)
        union {
            NkTTFSimpleGlyph    simple;
            NkTTFCompositeGlyph composite;
        };
    };

    /// Paire de crénage (table kern format 0)
    struct NkTTFKernPair {
        uint16 left;
        uint16 right;
        int16  value;  // en design units
    };

    /// Entrée cmap : plage de codepoints Unicode → glyphe
    struct NkTTFCmapRange {
        uint32 startCode;
        uint32 endCode;
        int32  idDelta;     // pour format 4
        uint32 glyphId;     // pour format 12
        bool   isDelta;     // true = format 4, false = format 12
    };

    /// Résultat global du parsing
    struct NKENTSEU_FONT_API NkTTFFont {
        // Tables parsées
        NkTTFHead     head;
        NkTTFHhea     hhea;
        NkTTFMaxp     maxp;
        NkTTFOS2      os2;
        NkTTFPost     post;
        NkTTFNameTable name;    // Table name avec informations de police

        // Métriques horizontales
        NkTTFHMetric* hmetrics;     // [numGlyphs]
        uint16        numHMetrics;

        // Table loca (offsets des glyphes)
        uint32*       locaOffsets;  // [numGlyphs + 1]

        // Table cmap
        NkTTFCmapRange* cmapRanges;
        uint32          numCmapRanges;

        // Table kern
        NkTTFKernPair* kernPairs;
        uint32         numKernPairs;

        // Tables de bytecode hinting (pointeurs dans le buffer source)
        const uint8*  cvtData;
        uint32        cvtLength;
        const uint8*  fpgmData;
        uint32        fpgmLength;
        const uint8*  prepData;
        uint32        prepLength;

        // Buffer source (non owning — doit rester valide pendant la durée de vie)
        const uint8*  rawData;
        usize         rawSize;

        // Offset de la table glyf dans rawData
        uint32        glyfOffset;
        uint32        glyfLength;

        // Flags
        bool          hasCFF;       // true si OTF avec table CFF
        bool          hasKern;
        bool          hasGPOS;
        bool          hasGSUB;
        bool          isValid;

        // Résout un codepoint Unicode en glyphe ID. Retourne 0 si absent.
        NKFONT_NODISCARD uint16 GetGlyphId(uint32 codepoint) const noexcept;

        // Lit les métriques d'un glyphe.
        NKFONT_NODISCARD NkTTFHMetric GetHMetric(uint16 glyphId) const noexcept;

        // Calcule l'offset d'un glyphe dans la table glyf.
        NKFONT_NODISCARD uint32 GetGlyfOffset(uint16 glyphId) const noexcept;
    };

    // =========================================================================
    //  NkTTFParser
    // =========================================================================

    class NKENTSEU_FONT_API NkTTFParser {
    public:

        /**
         * @Brief Parse un fichier TTF/OTF en mémoire.
         * @param data    Pointeur vers les données du fichier (doit rester valide).
         * @param size    Taille du fichier en octets.
         * @param arena   Arena pour toutes les allocations intermédiaires.
         * @param out     Structure de résultat (remplie par Parse).
         * @return true si succès, false si données corrompues ou format non supporté.
         */
        static bool Parse(
            const uint8*  data,
            usize         size,
            NkMemArena&   arena,
            NkTTFFont&    out
        ) noexcept;

        /**
         * @Brief Parse un glyphe individuel depuis la table glyf.
         * @param font    Police parsée par Parse().
         * @param glyphId ID du glyphe à parser.
         * @param arena   Arena scratch pour les allocations temporaires.
         * @param out     Résultat du parsing.
         * @return true si succès.
         */
        static bool ParseGlyph(
            const NkTTFFont& font,
            uint16           glyphId,
            NkMemArena&      arena,
            NkTTFGlyphData&  out
        ) noexcept;

        /**
         * @Brief Décompose un glyphe en contours NkPoint26_6 prêts pour le rasterizer.
         *        Résout les composites récursivement. Applique la transformation.
         *        Convertit les coordonnées design units → F26Dot6 à la taille donnée.
         * @param font      Police parsée.
         * @param glyphId   Glyphe à décomposer.
         * @param ppem      Taille en pixels par em.
         * @param arena     Arena scratch.
         * @param outPoints Tableau de points alloués dans arena.
         * @param outTags   Flags des points (kPointFlag_OnCurve etc.).
         * @param outContourEnds  Indice du dernier point de chaque contour.
         * @param outNumPoints    Nombre total de points.
         * @param outNumContours  Nombre de contours.
         * @return true si succès.
         */
        static bool DecomposeGlyph(
            const NkTTFFont& font,
            uint16           glyphId,
            uint16           ppem,
            NkMemArena&      arena,
            NkPoint26_6*&    outPoints,
            uint8*&          outTags,
            uint16*&         outContourEnds,
            uint16&          outNumPoints,
            uint16&          outNumContours
        ) noexcept;

    private:
        // ── Parsers de tables individuelles ───────────────────────────────────
        static bool ParseTableDirectory(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena, uint16& numTables, NkTTFTableRecord*& records) noexcept;
       
        // Fonctions SANS arena
        static bool ParseHead (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParseHhea (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParseMaxp (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParseOS2  (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParsePost (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParseCvt  (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParseFpgm (NkStreamReader& s, NkTTFFont& out) noexcept;
        static bool ParsePrep (NkStreamReader& s, NkTTFFont& out) noexcept;

        // Fonctions AVEC arena
        static bool ParseHmtx (NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;
        static bool ParseLoca (NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;
        static bool ParseCmap (NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;
        static bool ParseKern (NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;
        static bool ParseName (NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;

        // ── Parsers cmap formats ──────────────────────────────────────────────
        static bool ParseCmapFormat4 (NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;
        static bool ParseCmapFormat12(NkStreamReader& s, NkTTFFont& out, NkMemArena& arena) noexcept;

        // ── Parsing glyphe ────────────────────────────────────────────────────
        static bool ParseSimpleGlyph   (NkStreamReader& s, int16 numContours, NkTTFSimpleGlyph& out, NkMemArena& arena) noexcept;
        static bool ParseCompositeGlyph(NkStreamReader& s, NkTTFCompositeGlyph& out, NkMemArena& arena) noexcept;

        // ── Décomposition récursive composite ─────────────────────────────────
        static bool DecomposeGlyphRec(
            const NkTTFFont& font,
            uint16 glyphId,
            const NkMatrix2x2& transform,
            int32 dx, int32 dy,
            uint16 ppem,
            NkMemArena& arena,
            NkPoint26_6* points, uint8* tags, uint16* contourEnds,
            uint16& pointCount, uint16& contourCount,
            uint16 maxPoints, uint16 maxContours,
            int32 depth
        ) noexcept;

        // ── Utilitaires ───────────────────────────────────────────────────────
        static uint32 FindTable(const NkTTFTableRecord* records, uint16 numTables, uint32 tag) noexcept;
        static uint16 BinarySearchKern(const NkTTFKernPair* pairs, uint32 count, uint16 left, uint16 right) noexcept;
        static void ConvertUTF16ToUTF8(const uint16* src, usize srcLen, char* dst, usize dstSize) noexcept;
    };

} // namespace nkentseu