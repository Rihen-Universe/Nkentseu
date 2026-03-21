#pragma once
/**
 * @File    NkGlyphCache.h
 * @Brief   Cache LRU de glyphes rastérisés + atlas GPU-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkGlyphCache maintient un cache LRU de NkCachedGlyph.
 *  Clé : (glyphId, ppem, flags).
 *  Valeur : bitmap Gray8 + métriques + coordonnées UV dans l'atlas.
 *
 *  NkAtlasPacker : algorithme shelf (étagère) pour packer les glyphes
 *  dans un atlas 2D de taille fixe. Chaque étagère a une hauteur fixe
 *  et des glyphes sont ajoutés de gauche à droite jusqu'à débordement.
 *
 *  NkTextureAtlas : vue finale sur l'atlas (uint8* + dimensions + UV).
 *  Prête à être uploadée sur le GPU via OpenGL/Vulkan/D3D.
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkBitmap.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkFixed26_6.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  NkGlyphMetrics — métriques de rendu d'un glyphe
    // =========================================================================

    struct NkGlyphMetrics {
        F26Dot6 xAdvance;     ///< Avance horizontale
        F26Dot6 yAdvance;     ///< Avance verticale (0 pour écriture LTR)
        int32   bearingX;     ///< Distance du point d'origine au bord gauche (px)
        int32   bearingY;     ///< Distance de la baseline au bord supérieur (px)
        int32   width;        ///< Largeur du bitmap en pixels
        int32   height;       ///< Hauteur du bitmap en pixels
    };

    // =========================================================================
    //  UV dans l'atlas
    // =========================================================================

    struct NkAtlasUV {
        float u0, v0;  ///< Coin supérieur gauche (normalisé [0,1])
        float u1, v1;  ///< Coin inférieur droit
        int32 atlasX, atlasY; ///< Position en pixels dans l'atlas
        int32 atlasW, atlasH;
        int32 atlasPage; ///< Indice de la page de l'atlas (multi-page)
    };

    // =========================================================================
    //  NkCachedGlyph — entrée du cache
    // =========================================================================

    struct NkCachedGlyph {
        uint16        glyphId;
        uint16        ppem;
        uint8         flags;      ///< Réservé (antialiasing mode, hinting flags)
        NkGlyphMetrics metrics;
        NkBitmap      bitmap;     ///< Pixels dans l'arena permanente
        NkAtlasUV     uv;         ///< Position dans l'atlas
        bool          inAtlas;    ///< true si le glyphe a été packé dans l'atlas

        // Liens LRU (liste doublement chaînée)
        NkCachedGlyph* prev;
        NkCachedGlyph* next;
    };

    // =========================================================================
    //  NkGlyphCache — cache LRU
    // =========================================================================

    class NKENTSEU_FONT_API NkGlyphCache {
    public:

        /**
         * @Brief Initialise le cache.
         * @param arena    Arena permanente pour les entrées et les bitmaps.
         * @param capacity Nombre maximum d'entrées dans le cache.
         */
        void Init(NkMemArena& arena, uint32 capacity) noexcept;

        /**
         * @Brief Cherche un glyphe dans le cache.
         * @return Pointeur vers l'entrée, nullptr si absent.
         */
        NKFONT_NODISCARD NkCachedGlyph* Find(
            uint16 glyphId, uint16 ppem, uint8 flags = 0) const noexcept;

        /**
         * @Brief Insère ou met à jour un glyphe dans le cache.
         *        Si le cache est plein, évince le LRU.
         * @return Pointeur vers l'entrée insérée (peut avoir évincé une autre).
         */
        NkCachedGlyph* Insert(
            uint16 glyphId, uint16 ppem, uint8 flags,
            const NkGlyphMetrics& metrics,
            const NkBitmap& bitmap,
            NkMemArena& arena
        ) noexcept;

        /// Marque une entrée comme récemment utilisée (move-to-front).
        void Touch(NkCachedGlyph* entry) noexcept;

        /// Vide le cache (reset du LRU, les bitmaps restent dans l'arena).
        void Clear() noexcept;

        NKFONT_NODISCARD uint32 Size()     const noexcept { return mSize; }
        NKFONT_NODISCARD uint32 Capacity() const noexcept { return mCapacity; }
        NKFONT_NODISCARD float  HitRate()  const noexcept {
            const uint64 total = mHits + mMisses;
            return total > 0 ? static_cast<float>(mHits) / total : 0.f;
        }

    private:
        NkCachedGlyph** mBuckets  = nullptr;  ///< Hash table (open addressing)
        NkCachedGlyph*  mLRUHead  = nullptr;  ///< Most recently used
        NkCachedGlyph*  mLRUTail  = nullptr;  ///< Least recently used
        NkCachedGlyph*  mFreeList = nullptr;  ///< Entrées libres
        uint32          mCapacity = 0;
        uint32          mSize     = 0;
        uint32          mBucketCount = 0;
        uint64          mHits     = 0;
        uint64          mMisses   = 0;

        static uint32 HashKey(uint16 glyphId, uint16 ppem, uint8 flags) noexcept {
            return (static_cast<uint32>(glyphId) * 2654435761u)
                 ^ (static_cast<uint32>(ppem)    * 40503u)
                 ^ flags;
        }

        void LRU_Remove(NkCachedGlyph* e) noexcept;
        void LRU_PushFront(NkCachedGlyph* e) noexcept;
        NkCachedGlyph* LRU_Evict() noexcept;
    };

    // =========================================================================
    //  NkAtlasPacker — algorithme shelf
    // =========================================================================

    class NKENTSEU_FONT_API NkAtlasPacker {
    public:

        /**
         * @Brief Initialise le packer.
         * @param atlas     Bitmap de l'atlas (pré-alloué).
         * @param padding   Padding entre les glyphes en pixels (défaut 1).
         */
        void Init(const NkBitmap& atlas, int32 padding = 1) noexcept;

        /**
         * @Brief Tente d'insérer un glyphe dans l'atlas.
         * @param glyph Bitmap du glyphe à insérer.
         * @param outX, outY Position d'insertion dans l'atlas.
         * @return true si succès, false si l'atlas est plein.
         */
        bool Pack(const NkBitmap& glyph, int32& outX, int32& outY) noexcept;

        /// Reset complet de l'atlas (efface tous les glyphes).
        void Reset() noexcept;

        NKFONT_NODISCARD bool IsFull() const noexcept { return mFull; }
        NKFONT_NODISCARD float Usage() const noexcept;

    private:
        struct Shelf {
            int32 y;        ///< Y de début de l'étagère
            int32 height;   ///< Hauteur de l'étagère
            int32 xCursor;  ///< X courant (prochain glyphe)
        };

        static constexpr int32 MAX_SHELVES = 64;
        Shelf  mShelves[MAX_SHELVES];
        int32  mNumShelves = 0;
        NkBitmap mAtlas;
        int32  mPadding = 1;
        bool   mFull    = false;

        Shelf* FindShelf(int32 h) noexcept;
        bool   AddShelf (int32 h, int32& shelfY) noexcept;
    };

    // =========================================================================
    //  NkTextureAtlas — atlas GPU-ready
    // =========================================================================

    struct NKENTSEU_FONT_API NkTextureAtlas {
        NkBitmap      bitmap;     ///< Données pixels (Gray8)
        NkAtlasPacker packer;     ///< Gestionnaire d'espace
        NkGlyphCache  cache;      ///< Cache LRU associé
        bool          dirty;      ///< true si l'atlas a changé depuis le dernier upload GPU
        int32         page;       ///< Indice de page (multi-atlas)

        /**
         * @Brief Initialise un atlas de taille fixe.
         * @param arena    Arena permanente.
         * @param width    Largeur en pixels (recommandé : 512 ou 1024).
         * @param height   Hauteur en pixels.
         * @param cacheCapacity  Taille du LRU cache.
         */
        bool Init(NkMemArena& arena, int32 width, int32 height,
                  uint32 cacheCapacity = 1024) noexcept;

        /**
         * @Brief Ajoute un glyphe à l'atlas (bitmap + métriques).
         *        Met à jour le cache et les UVs.
         * @return true si succès (false si atlas plein).
         */
        bool AddGlyph(
            uint16 glyphId, uint16 ppem,
            const NkGlyphMetrics& metrics,
            const NkBitmap& glyphBitmap,
            NkMemArena& arena
        ) noexcept;

        /**
         * @Brief Cherche un glyphe dans l'atlas (via le cache).
         * @return nullptr si absent — nécessite un rasterize + AddGlyph.
         */
        NKFONT_NODISCARD const NkCachedGlyph* FindGlyph(
            uint16 glyphId, uint16 ppem) const noexcept;
    };

} // namespace nkentseu
