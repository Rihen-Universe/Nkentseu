/**
 * @file    NkFontSimpleAdapter.h
 * @brief   Adaptation simplifiée du système NKFont basée sur les algorithmes d'ImGui
 * 
 * Cet adaptateur expose les glyphes de l'atlas NKFont de manière direct et simple,
 * similaire à comment ImGui gère les polices. Cela facilite le rendu simple du texte
 * via OpenGL sans complexité supplémentaire.
 * 
 * INSPIRATION : ImGui's ImFontAtlas approach
 *  - Chaque glyphe a une position (x,y) et des dimensions (w,h) dans l'atlas
 *  - Les UV sont calculées comme (x/texWidth, y/texHeight), (x+w)/texWidth, (y+h)/texHeight
 *  - Un appel simple GetGlyphQuad() retourne les 4 vertices (triangle strip)
 *  - Support simple du kerning et des métriques
 */

#pragma once
#include "NKFont/Core/NkFontLibrary.h"
#include "NKFont/Core/NkFontFace.h"
#include <unordered_map>

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────
    //  NkSimpleGlyphInfo — Information de rendu simple pour un glyphe
    // ─────────────────────────────────────────────────────────────────────────

    struct NkSimpleGlyphInfo {
        float uvX0, uvY0, uvX1, uvY1;    // Coordonnées texture normalisées [0,1]
        float advanceX;                  // Avance horizontale pour le kerning
        float bearingX, bearingY;        // Décalages de placement
        float width, height;             // Dimensions du glyphe rastérisé
    };

    // ─────────────────────────────────────────────────────────────────────────
    //  NkFontSimpleAdapter — Interface simple pour le rendu texte
    // ─────────────────────────────────────────────────────────────────────────

    class NkFontSimpleAdapter {
    public:
        NkFontSimpleAdapter() = default;
        
        /** 
         * @brief Initialise l'adaptateur à partir d'une NkFontLibrary
         * @param lib Référence à la bibliothèque de polices
         * @param fontId ID de la police (obtenu via LoadFromFile)
         * @param pointSize Taille de la police en points
         * @param dpi Résolution source (96 = standard)
         * @return true si l'initialisation a réussi
         */
        bool Init(NkFontLibrary& lib,  nk_uint32 fontId, float pointSize, float dpi = 96.f) {
            if (!lib.IsInitialized()) return false;
            
            mLib = &lib;
            mFontId = fontId;
            mPointSize = pointSize;
            mDpi = dpi;
            
            // Pré-rastérise plages communes
            NkRasterizeParams params = NkRasterizeParams::ForBitmap(2u);  // 2x SSAA
            lib.PrerasterizeRange(fontId, NkUnicodeRange::Ascii(),  pointSize, dpi, params);
            lib.PrerasterizeRange(fontId, NkUnicodeRange::Latin1(), pointSize, dpi, params);
            
            // Upload atlas
            lib.UploadAtlas();
            
            // Cache les informations de glyphes pour accès rapide
            return CacheGlyphInfo();
        }
        
        /**
         * @brief Récupère les informations de rendu pour un caractère
         * @param codepoint Point de code Unicode
         * @param[out] outInfo Information de glyphe
         * @return true si le glyphe est disponible
         */
        bool GetGlyphInfo(nk_codepoint codepoint, NkSimpleGlyphInfo& outInfo) const {
            if (!mLib) return false;
            
            // Cherche d'abord dans le cache
            auto it = mGlyphInfoCache.find(codepoint);
            if (it != mGlyphInfoCache.end()) {
                outInfo = it->second;
                return true;
            }
            
            // Sinon, charger à la demande
            NkRasterizeParams params = NkRasterizeParams::ForBitmap(2u);
            const NkCachedGlyph* cachedGlyph = nullptr;
            
            NkFontResult res = mLib->GetGlyph(mFontId, codepoint, mPointSize, mDpi, params, cachedGlyph);
            if (!res.IsOk() || !cachedGlyph || !cachedGlyph->isValid || !cachedGlyph->isInAtlas) {
                return false;
            }
            
            // Remplit les infos
            outInfo.uvX0 = cachedGlyph->uvX0;
            outInfo.uvY0 = cachedGlyph->uvY0;
            outInfo.uvX1 = cachedGlyph->uvX1;
            outInfo.uvY1 = cachedGlyph->uvY1;
            outInfo.advanceX = (float)cachedGlyph->advanceX / 64.f;  // Convertir depuis 1/64 px
            outInfo.bearingX = (float)cachedGlyph->bearingX;
            outInfo.bearingY = (float)cachedGlyph->bearingY;
            outInfo.width = (float)cachedGlyph->width;
            outInfo.height = (float)cachedGlyph->height;
            
            // Cache pour prochaine fois
            mGlyphInfoCache[codepoint] = outInfo;
            return true;
        }
        
        /**
         * @brief Récupère la texture de l'atlas (pour binding GL)
         * @return GLuint pour glBindTexture(GL_TEXTURE_2D, ...)
         */
        uint32_t GetAtlasTextureHandle() const {
            // L'utilisateur devra stocker le GLuint dans un deuxième callback
            // Ici, on retourne 0 — l'utilisateur gérera le texture ID via SetAtlasTextureHandle()
            return mAtlasTextureHandle;
        }
        
        void SetAtlasTextureHandle(uint32_t texHandle) {
            mAtlasTextureHandle = texHandle;
        }
        
        /**
         * @brief Récupère les dimensions de l'atlas
         */
        void GetAtlasDimensions(uint32_t& outWidth, uint32_t& outHeight) const {
            outWidth = mLib->Atlas().PageCount() > 0 
                ? mLib->Atlas().Page(0).width() 
                : 0u;
            outHeight = mLib->Atlas().PageCount() > 0 
                ? mLib->Atlas().Page(0).height()
                : 0u;
        }
        
        float GetFontHeight() const {
            NkFontFace* face = mLib ? mLib->GetFace(mFontId) : nullptr;
            if (!face) return mPointSize;
            
            // Approximation : hauteur ≈ taille en points
            return mPointSize;
        }

    private:
        NkFontLibrary* mLib = nullptr;
        nk_uint32 mFontId = 0u;
        float mPointSize = 12.f;
        float mDpi = 96.f;
        uint32_t mAtlasTextureHandle = 0u;
        
        // Cache des informations de glyphes pour performances
        mutable std::unordered_map<nk_codepoint, NkSimpleGlyphInfo> mGlyphInfoCache;
        
        /**
         * @brief Pré-cache les informations des glyphes ASCII communs
         */
        bool CacheGlyphInfo() {
            if (!mLib) return false;
            
            NkRasterizeParams params = NkRasterizeParams::ForBitmap(2u);
            
            // Pré-cache les glyphes ASCII (32-126)
            for (nk_codepoint cp = 32; cp <= 126; ++cp) {
                const NkCachedGlyph* cachedGlyph = nullptr;
                if (mLib->GetGlyph(mFontId, cp, mPointSize, mDpi, params, cachedGlyph).IsOk() 
                    && cachedGlyph && cachedGlyph->isValid && cachedGlyph->isInAtlas) {
                    
                    NkSimpleGlyphInfo info;
                    info.uvX0 = cachedGlyph->uvX0;
                    info.uvY0 = cachedGlyph->uvY0;
                    info.uvX1 = cachedGlyph->uvX1;
                    info.uvY1 = cachedGlyph->uvY1;
                    info.advanceX = (float)cachedGlyph->advanceX / 64.f;
                    info.bearingX = (float)cachedGlyph->bearingX;
                    info.bearingY = (float)cachedGlyph->bearingY;
                    info.width = (float)cachedGlyph->width;
                    info.height = (float)cachedGlyph->height;
                    
                    mGlyphInfoCache[cp] = info;
                }
            }
            
            return mGlyphInfoCache.size() > 0;
        }
    };

} // namespace nkentseu
