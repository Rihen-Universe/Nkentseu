// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontSizeCache.h
// DESCRIPTION: Cache de tailles de fontes — permet de changer la taille
//              dynamiquement sans recréer l'atlas entier.
//
// Stratégie :
//   - L'atlas principal est construit une fois avec toutes les tailles prévues.
//   - Le cache associe (chemin + taille) → NkFont*.
//   - Quand une taille est demandée et absente, elle est ajoutée et l'atlas
//     est reconstruit (rebuild partiel ou complet selon la stratégie choisie).
//
// Alternative sans rebuild : affichage par scale GPU.
//   - On rastérise à une grande taille (ex: 64px) et on scale par matrice.
//   - Résultat net à toutes les tailles inférieures, flou à taille supérieure.
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKFONT_CORE_NKFONTSIZECACHE_H_INCLUDED
#define NK_NKFONT_CORE_NKFONTSIZECACHE_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkFontTypes.h"
#include "NkFontDetect.h"
#include "NKFont/NkFont.h"

namespace nkentseu {

    struct NkFontAtlas;
    struct NkFont;

    // ============================================================
    // NkFontSizeEntry — une entrée du cache
    // ============================================================

    /**
     * @brief Une fonte rastérisée à une taille spécifique dans le cache.
     */
    struct NkFontSizeEntry {
        char         fontPath[256] = {};   ///< Chemin vers le fichier TTF.
        nkft_float32 sizePx        = 0.f; ///< Taille en pixels.
        NkFont*      font          = nullptr;
        NkFontProfile profile;             ///< Profil détecté (bitmap/vectoriel).
        nkft_uint64  lastUsedFrame = 0;    ///< Pour LRU eviction.
        bool         isValid       = false;
    };

    // ============================================================
    // NkFontSizeCache
    // ============================================================

    /**
     * @brief Gestionnaire de cache de fontes multi-tailles.
     *
     * Permet d'obtenir une NkFont* à n'importe quelle taille de façon
     * transparente. Si la taille n'est pas encore dans le cache, elle
     * est ajoutée et l'atlas est reconstruit.
     *
     * @note Le rebuild de l'atlas invalide toutes les textures GPU.
     *       L'application doit être notifiée pour re-uploader la texture.
     *
     * @example Usage
     * @code
     * NkFontSizeCache cache;
     * cache.Init(&atlas, "Roboto-Regular.ttf");
     *
     * // Première fois : rastérise 18px et rebuild l'atlas
     * NkFont* f18 = cache.GetFont(18.f);
     *
     * // Deuxième fois : retourne depuis le cache, pas de rebuild
     * NkFont* f18b = cache.GetFont(18.f);
     *
     * // Nouvelle taille : rebuild
     * NkFont* f24 = cache.GetFont(24.f);
     * if (cache.NeedsAtlasRebuild()) {
     *     // Re-upload la texture GPU
     *     UploadAtlasToGPU(cache.GetAtlas());
     *     cache.ClearRebuildFlag();
     * }
     * @endcode
     */
    class NkFontSizeCache {
        public:
            static constexpr nkft_int32 MAX_CACHED_SIZES = 16;

            NkFontSizeCache()  = default;
            ~NkFontSizeCache() = default;

            // Interdiction de copie
            NkFontSizeCache(const NkFontSizeCache&)            = delete;
            NkFontSizeCache& operator=(const NkFontSizeCache&) = delete;

            // ─── Initialisation ───────────────────────────────────────────────────

            /**
             * @brief Attache le cache à un atlas et une police.
             *
             * @param atlas      Atlas géré par ce cache.
             * @param fontPath   Chemin vers le fichier TTF.
             * @param preloadSizes  Tailles à préchauffer immédiatement (optionnel).
             * @param preloadCount  Nombre de tailles à préchauffer.
             */
            void Init(NkFontAtlas* atlas,
                    const char* fontPath,
                    const nkft_float32* preloadSizes = nullptr,
                    nkft_int32 preloadCount = 0);

            // ─── Accès aux fontes ─────────────────────────────────────────────────

            /**
             * @brief Retourne une NkFont* à la taille demandée.
             *
             * Si la taille n'est pas dans le cache :
             *   1. Ajoute la fonte à l'atlas.
             *   2. Marque l'atlas comme nécessitant un rebuild.
             *   3. Retourne nullptr jusqu'au prochain appel après BuildAtlas().
             *
             * @param sizePx   Taille en pixels (sera snappée si police bitmap).
             * @param frameIdx Index de frame courant (pour LRU).
             */
            NkFont* GetFont(nkft_float32 sizePx, nkft_uint64 frameIdx = 0);

            /**
             * @brief Construit (ou reconstruit) l'atlas si nécessaire.
             * @return true si l'atlas a été reconstruit (texture GPU à re-uploader).
             */
            bool BuildAtlasIfNeeded();

            // ─── État ─────────────────────────────────────────────────────────────

            /** @brief true si l'atlas a été reconstruit depuis le dernier ClearRebuildFlag(). */
            bool NeedsGpuUpload() const { return mNeedsGpuUpload; }

            /** @brief À appeler après avoir re-uploadé la texture GPU. */
            void ClearGpuUploadFlag() { mNeedsGpuUpload = false; }

            /** @brief Accède à l'atlas sous-jacent. */
            NkFontAtlas* GetAtlas() const { return mAtlas; }

            /** @brief Nombre de tailles actuellement en cache. */
            nkft_int32 GetCachedCount() const { return mCount; }

            /** @brief Retourne le profil détecté de la fonte. */
            const NkFontProfile& GetProfile() const { return mProfile; }

            /**
             * @brief Indique si le filtre GPU doit être NEAREST (pour cette fonte).
             * Pratique pour configurer le sampler automatiquement.
             */
            bool ShouldUseNearestFilter() const {
                return NkFontDetector::RecommendsNearestFilter(mProfile);
            }

        private:
            NkFontAtlas*     mAtlas    = nullptr;
            char             mFontPath[256] = {};
            NkFontProfile    mProfile;
            bool             mProfileReady   = false;
            bool             mNeedsRebuild   = false;
            bool             mNeedsGpuUpload = false;
            nkft_int32       mCount          = 0;

            NkFontSizeEntry  mEntries[MAX_CACHED_SIZES];

            bool HasSize(nkft_float32 sizePx, nkft_int32* outIdx = nullptr) const;
            void EvictLRU();
    };

    // ============================================================
    // NkFontScaleRenderer — affichage à taille variable sans rebuild
    // ============================================================

    /**
     * @brief Utilitaire pour afficher du texte à une taille arbitraire
     *        sans reconstruire l'atlas.
     *
     * Principe : on rastérise à une taille de référence (ex: 64px) et
     * on applique un facteur d'échelle dans le calcul des quads.
     * Résultat net pour les réductions (scale < 1), flou pour les agrandissements.
     *
     * Pour un rendu parfait à toute taille, il faudrait implémenter SDF
     * (Signed Distance Fields) — voir NkFontSdf.h (futur).
     *
     * @example
     * @code
     * // Référence : fonte rastérisée à 64px
     * NkFont* fontRef = atlas.GetFont(64.f);
     *
     * // Affichage à 24px par scale
     * float scale = 24.f / 64.f;
     * NkFontScaleRenderer::DrawText2D(renderer, fontRef, "Hello", px, py, scale, ...);
     * @endcode
     */
    struct NkFontScaleRenderer {

        /**
         * @brief Calcule le facteur d'échelle pour afficher une fonte de
         *        référence à une taille cible.
         *
         * @param fontRefSizePx  Taille de la fonte rastérisée (ex: 64.f).
         * @param targetSizePx   Taille souhaitée à l'écran (ex: 18.f).
         */
        static nkft_float32 ComputeScale(nkft_float32 fontRefSizePx,
                                         nkft_float32 targetSizePx) {
            if (fontRefSizePx <= 0.f) return 1.f;
            return targetSizePx / fontRefSizePx;
        }

        /**
         * @brief Calcule la largeur d'un texte affiché avec scale.
         */
        static nkft_float32 CalcTextSizeX(const NkFont* font,
                                          const char* text,
                                          nkft_float32 scale) {
            if (!font || !text) return 0.f;
            return font->CalcTextSizeX(text) * scale;
        }

        /**
         * @brief Retourne les dimensions du quad de rendu pour un glyphe scalé.
         *
         * @param glyph   Glyphe source.
         * @param curX    Curseur X courant (en pixels écran).
         * @param curY    Ligne de base Y (en pixels écran).
         * @param scale   Facteur d'échelle.
         * @param outX0/Y0/X1/Y1  Coins du quad en pixels écran.
         * @param outAdvanceX  Avance à ajouter à curX.
         */
        static void GetScaledGlyphQuad(const NkFontGlyph* glyph,
                                       nkft_float32 curX, nkft_float32 curY,
                                       nkft_float32 scale,
                                       nkft_float32* outX0, nkft_float32* outY0,
                                       nkft_float32* outX1, nkft_float32* outY1,
                                       nkft_float32* outAdvanceX) {
            if (!glyph) return;
            if (outX0) *outX0 = curX + glyph->x0 * scale;
            if (outY0) *outY0 = curY + glyph->y0 * scale;
            if (outX1) *outX1 = curX + glyph->x1 * scale;
            if (outY1) *outY1 = curY + glyph->y1 * scale;
            if (outAdvanceX) *outAdvanceX = glyph->advanceX * scale;
        }
    };

} // namespace nkentseu

#endif // NK_NKFONT_CORE_NKFONTSIZECACHE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================