#pragma once
/**
 * @File    NkFontFace.h
 * @Brief   API publique — NkFontFace, NkGlyph, NkFontLibrary.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Hiérarchie :
 *    NkFontLibrary  — singleton, gestion mémoire globale, registre des faces
 *    NkFontFace     — une police à une taille donnée (ppem)
 *    NkGlyph        — résultat du rendu d'un glyphe : bitmap + métriques
 *
 *  Usage typique :
 * @code
 *   NkFontLibrary lib;
 *   lib.Init(8 * 1024 * 1024);  // 8 MiB arena permanente
 *
 *   NkFontFace* face = lib.LoadTTF(data, size, 16);  // 16px
 *   if (!face) { ... }
 *
 *   const NkGlyph* g = face->GetGlyph('A');
 *   // g->bitmap : pixels Gray8
 *   // g->metrics.xAdvance, bearingX, bearingY...
 *   // g->uv : coordonnées dans l'atlas GPU
 *
 *   NkGlyphRun run;
 *   face->ShapeText("Hello world", run);
 *   for (auto& gi : run) { ... }
 *
 *   lib.Destroy();
 * @endcode
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkTTFParser.h"
#include "NKFont/NkCFFParser.h"
#include "NKFont/NkBDFParser.h"
#include "NKFont/NkType1Parser.h"
#include "NKFont/NkWOFFParser.h"
#include "NKFont/NkHinter.h"
#include "NKFont/NkScanline.h"
#include "NKFont/NkShaper.h"
#include "NKFont/NkGlyphCache.h"
#include "NKFont/NkUnicode.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  NkGlyph — résultat de rendu d'un glyphe
    // =========================================================================

    struct NKENTSEU_FONT_API NkGlyph {
        uint16          glyphId;
        uint32          codepoint;
        NkGlyphMetrics  metrics;
        NkBitmap        bitmap;     ///< Pixels Gray8, coordonnées écran
        NkAtlasUV       uv;         ///< UV dans l'atlas (valides si inAtlas)
        bool            inAtlas;
        bool            isEmpty;    ///< Glyphe vide (espace, etc.)
    };

    // =========================================================================
    //  NkFontFormat — format détecté
    // =========================================================================

    enum class NkFontFormat : uint8 {
        Unknown = 0,
        TTF,       ///< TrueType
        OTF_CFF,   ///< OpenType avec contours CFF
        BDF,       ///< Bitmap Distribution Format
        PCF,       ///< Portable Compiled Format (X11)
        Type1,     ///< PostScript Type 1 (PFB/PFA)
        WOFF,      ///< Web Open Font Format v1
        WOFF2,     ///< Web Open Font Format v2
    };

    // =========================================================================
    //  NkFontFace — police à une taille donnée
    // =========================================================================

    class NKENTSEU_FONT_API NkFontFace {
    public:

        NkFontFace()  noexcept = default;
        ~NkFontFace() noexcept = default;

        NkFontFace(const NkFontFace&)            = delete;
        NkFontFace& operator=(const NkFontFace&) = delete;

        // ── Accès aux glyphes ────────────────────────────────────────────────

        /**
         * @Brief Obtient le rendu d'un glyphe par codepoint Unicode.
         *        Si absent du cache : rasterize et stocke dans l'atlas.
         * @param codepoint  Codepoint Unicode.
         * @param out        Glyphe résultant (bitmap + métriques + UV).
         * @return true si succès (false si glyphe absent de la police).
         */
        bool GetGlyph(uint32 codepoint, NkGlyph& out) noexcept;

        /**
         * @Brief Version par glyphe ID direct.
         */
        bool GetGlyphById(uint16 glyphId, NkGlyph& out) noexcept;

        // ── Métriques ────────────────────────────────────────────────────────

        NKFONT_NODISCARD int32 GetAscender()    const noexcept;
        NKFONT_NODISCARD int32 GetDescender()   const noexcept;
        NKFONT_NODISCARD int32 GetLineHeight()  const noexcept;
        NKFONT_NODISCARD int32 GetUnderlinePos()const noexcept;
        NKFONT_NODISCARD uint16 GetPPEM()       const noexcept { return mPPEM; }
        NKFONT_NODISCARD NkFontFormat GetFormat()const noexcept { return mFormat; }

        // ── Shaping ──────────────────────────────────────────────────────────

        /**
         * @Brief Pipeline complet : UTF-8 → NkGlyphRun.
         *        Applique bidi, GSUB, GPOS/kern.
         */
        bool ShapeText(
            const char*        text,
            NkGlyphRun&        out,
            const NkShaper::Options& opts = {}
        ) noexcept;

        bool ShapeText(
            const uint8* utf8, usize byteLen,
            NkGlyphRun& out,
            const NkShaper::Options& opts = {}
        ) noexcept;

        /**
         * @Brief Mesure la largeur d'un texte sans rasterizer.
         */
        NKFONT_NODISCARD F26Dot6 MeasureText(const char* text) noexcept;

        // ── Kerning ──────────────────────────────────────────────────────────

        NKFONT_NODISCARD F26Dot6 GetKerning(uint16 left, uint16 right) const noexcept;

        // ── Atlas ────────────────────────────────────────────────────────────

        NKFONT_NODISCARD NkTextureAtlas* GetAtlas() noexcept { return mAtlas; }

        /**
         * @Brief Précharge un ensemble de glyphes dans l'atlas.
         *        Utile pour les fontes de jeu avec charset connu.
         */
        void Preload(const uint32* codepoints, uint32 count) noexcept;
        void PreloadASCII() noexcept;  ///< Précharge U+0020-U+007E

        // ── Hinting ──────────────────────────────────────────────────────────

        void SetHintingEnabled(bool enabled) noexcept { mHintingEnabled = enabled; }
        NKFONT_NODISCARD bool IsHintingEnabled() const noexcept { return mHintingEnabled; }

    private:
        friend class NkFontLibrary;

        // Données de la police selon le format
        NkTTFFont    mTTF;
        NkCFFFont    mCFF;
        NkBDFFont    mBDF;
        NkType1Font  mType1;

        NkHintVM     mHintVM;
        NkTextureAtlas* mAtlas   = nullptr;
        NkMemArena*  mArena      = nullptr;
        uint16       mPPEM       = 0;
        NkFontFormat mFormat     = NkFontFormat::Unknown;
        bool         mHintingEnabled = true;
        bool         mIsValid    = false;

        bool RasterizeGlyph(uint16 glyphId, NkGlyph& out) noexcept;
        bool RasterizeTTF  (uint16 glyphId, NkGlyph& out) noexcept;
        bool RasterizeCFF  (uint16 glyphId, NkGlyph& out) noexcept;
        bool RasterizeBDF  (uint16 glyphId, NkGlyph& out) noexcept;

        NkShaper::Options MakeShaperOpts() const noexcept {
            NkShaper::Options o; o.ppem = mPPEM; return o;
        }
    };

    // =========================================================================
    //  NkFontLibrary — gestionnaire de fonts
    // =========================================================================

    class NKENTSEU_FONT_API NkFontLibrary {
    public:

        NkFontLibrary()  noexcept = default;
        ~NkFontLibrary() noexcept { Destroy(); }

        NkFontLibrary(const NkFontLibrary&)            = delete;
        NkFontLibrary& operator=(const NkFontLibrary&) = delete;

        /**
         * @Brief Initialise la bibliothèque.
         * @param permanentBytes  Arena permanente (fonts + cache).
         * @param scratchBytes    Arena scratch (rasterizer par frame).
         * @param atlasWidth/Height  Dimensions de l'atlas GPU.
         * @param atlasCacheCapacity Taille du LRU cache de l'atlas.
         */
        bool Init(
            usize  permanentBytes    = 8  * 1024 * 1024,
            usize  scratchBytes      = 2  * 1024 * 1024,
            int32  atlasWidth        = 1024,
            int32  atlasHeight       = 1024,
            uint32 atlasCacheCapacity= 2048
        ) noexcept;

        void Destroy() noexcept;

        // ── Chargement de polices ────────────────────────────────────────────

        /**
         * @Brief Charge un fichier TTF/OTF/BDF/PCF/PFB/WOFF depuis un buffer.
         *        Détecte le format automatiquement.
         * @param data   Buffer du fichier (doit rester valide pendant la durée de vie).
         * @param size   Taille du buffer.
         * @param ppem   Taille en pixels par em.
         * @return NkFontFace alloué dans l'arena, nullptr si échec.
         */
        NKFONT_NODISCARD NkFontFace* LoadFont(
            const uint8* data, usize size, uint16 ppem) noexcept;

        /// Charge un TTF/OTF.
        NKFONT_NODISCARD NkFontFace* LoadTTF(
            const uint8* data, usize size, uint16 ppem) noexcept;

        /// Charge un BDF (bitmap).
        NKFONT_NODISCARD NkFontFace* LoadBDF(
            const uint8* data, usize size) noexcept;

        /// Charge un WOFF (décompresse + parse TTF/OTF sous-jacent).
        /// Charge une face depuis un TTC (TrueType Collection).
        NKFONT_NODISCARD NkFontFace* LoadTTC(
            const uint8* data, usize size,
            int32 faceIndex=0, uint16 ppem=16) noexcept;
        NKFONT_NODISCARD static int32 GetTTCFaceCount(const uint8* data, usize size) noexcept;

        NKFONT_NODISCARD NkFontFace* LoadWOFF(
            const uint8* data, usize size, uint16 ppem) noexcept;

        // ── Accès aux ressources ─────────────────────────────────────────────

        NKFONT_NODISCARD NkTextureAtlas* GetAtlas()   noexcept { return &mAtlas; }
        NKFONT_NODISCARD NkMemArena&     GetArena()   noexcept { return mPermanent; }
        NKFONT_NODISCARD NkMemArena&     GetScratch() noexcept { return mScratch; }

        /// Doit être appelé au début de chaque frame.
        void BeginFrame() noexcept { mScratch.Reset(); }

    private:
        NkArenaPool   mPool;
        NkMemArena&   mPermanent = mPool.permanent;
        NkMemArena&   mScratch   = mPool.scratch;
        NkTextureAtlas mAtlas;

        static constexpr uint32 MAX_FACES = 32;
        NkFontFace*   mFaces[MAX_FACES] = {};
        uint32        mFaceCount = 0;
        bool          mIsValid   = false;

        NkFontFace* AllocFace() noexcept;
        static NkFontFormat DetectFormat(const uint8* data, usize size) noexcept;
    };

} // namespace nkentseu
