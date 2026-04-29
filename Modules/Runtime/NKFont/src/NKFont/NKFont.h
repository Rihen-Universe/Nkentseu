// -----------------------------------------------------------------------------
// FICHIER: NKFont/NkFont.h
// DESCRIPTION: API publique NKFont V2.1
//
// Nouveautés V2.1 :
//   - sdfMode / sdfSpread : mode SDF pour rendu net à toute taille
//   - Support WOFF / OTF-CFF via NkFontParser
//   - kFontFragSDF : shader GLSL pour rendu SDF
//   - GetGlyphOutline : extraction des contours vectoriels pour mesh 3D
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKFONT_NKFONT_H
#define NK_NKFONT_NKFONT_H

#include "Core/NkFontTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFont/Core/NkFontParser.h"

namespace nkentseu {

    struct NkFontAtlas;
    struct NkFont;
    struct NkFontConfig;
    struct NkFontGlyph;

    // ============================================================
    // NkFontGlyph
    // ============================================================

    struct NkFontGlyph {
        NkFontCodepoint codepoint  = 0;
        nkft_float32    advanceX   = 0.f;
        nkft_float32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< Position quad (pixels écran, relative au curseur).
        nkft_float32 u0 = 0, v0 = 0, u1 = 0, v1 = 0; ///< UV dans l'atlas [0..1].
        bool visible = true;
    };

    // ============================================================
    // NkFontConfig
    // ============================================================

    struct NkFontConfig {
        const nkft_uint8* fontData      = nullptr;
        nkft_size         fontDataSize  = 0u;
        bool              fontDataOwned = false;
        nkft_int32        fontIndex     = 0;
        nkft_float32      sizePixels    = 13.f;
        nkft_int32        oversampleH   = 2;
        nkft_int32        oversampleV   = 1;
        bool              pixelSnapH    = false;
        math::NkVec2f           glyphOffset   = {0.f, 0.f};
        nkft_float32      glyphMinAdvanceX  = 0.f;
        nkft_float32      glyphMaxAdvanceX  = 1e9f;
        nkft_float32      glyphExtraSpacing = 0.f;
        const nkft_uint32* glyphRanges  = nullptr;
        NkFontCodepoint   glyphFallback = 0x003Fu;
        bool              mergeMode     = false;
        nkft_float32      rasterizerMultiply = 1.f;
        char              name[40]      = {};
    };

    // ============================================================
    // NkFontAtlasCustomRect
    // ============================================================

    struct NkFontAtlasCustomRect {
        nkft_uint16  width = 0, height = 0, x = 0, y = 0;
        nkft_float32 u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        bool         isPacked = false;
    };

    // ============================================================
    // NkFontAtlas
    // ============================================================

    static constexpr nkft_uint32 NK_FONT_ATLAS_MAX_FONTS        = 16u;
    static constexpr nkft_uint32 NK_FONT_ATLAS_MAX_CUSTOM_RECTS = 64u;

    struct NkFontAtlas {

            // ── Texture GPU ───────────────────────────────────────────────────────
            void*       texID      = nullptr;
            nkft_int32  texWidth   = 0, texHeight = 0;
            nkft_uint8* texPixels  = nullptr;
            bool        texReady   = false;

            // ── Paramètres ────────────────────────────────────────────────────────
            nkft_int32 texDesiredWidth = 0;
            nkft_int32 texGlyphPadding = 2; ///< Padding recommandé ≥ 2 pour éviter le bleeding.

            // ── Mode SDF ──────────────────────────────────────────────────────────
            bool       sdfMode   = false;
            nkft_int32 sdfSpread = 6;  ///< Rayon SDF en pixels (4-8 typique).

            // ── Fontes ────────────────────────────────────────────────────────────
            NkFont*       fonts      [NK_FONT_ATLAS_MAX_FONTS] = {};
            NkFontConfig  configs    [NK_FONT_ATLAS_MAX_FONTS];
            nkft_uint8*   fontDataBufs[NK_FONT_ATLAS_MAX_FONTS] = {};
            nkft_uint32   fontCount = 0u;

            // ── Rects custom ─────────────────────────────────────────────────────
            NkFontAtlasCustomRect customRects[NK_FONT_ATLAS_MAX_CUSTOM_RECTS];
            nkft_uint32           customRectCount = 0u;

            NkFontAtlas();
            ~NkFontAtlas();
            NkFontAtlas(const NkFontAtlas&)            = delete;
            NkFontAtlas& operator=(const NkFontAtlas&) = delete;

            // ── API ──────────────────────────────────────────────────────────────

            NkFont* AddFontFromFile(const char* path,
                                    nkft_float32 sizePixels,
                                    const NkFontConfig* cfg = nullptr);
            NkFont* AddFontFromMemory(const nkft_uint8* data,
                                    nkft_size dataSize,
                                    nkft_float32 sizePixels,
                                    const NkFontConfig* cfg = nullptr);
            NkFont* AddFontFromMemoryOwned(nkft_uint8* data,
                                        nkft_size dataSize,
                                        nkft_float32 sizePixels,
                                        const NkFontConfig* cfg = nullptr);

            nkft_int32 AddCustomRect(nkft_uint16 w, nkft_uint16 h);
            const NkFontAtlasCustomRect* GetCustomRect(nkft_int32 idx) const;

            static const nkft_uint32* GetGlyphRangesDefault();
            static const nkft_uint32* GetGlyphRangesLatinExtA();
            static const nkft_uint32* GetGlyphRangesCyrillic();
            static const nkft_uint32* GetGlyphRangesGreek();
            static const nkft_uint32* GetGlyphRangesChineseFull();

            bool Build();

            void GetTexDataAsAlpha8(nkft_uint8** op,
                                    nkft_int32* ow,
                                    nkft_int32* oh,
                                    nkft_int32* ob = nullptr);
            void GetTexDataAsRGBA32(nkft_uint8** op,
                                    nkft_int32* ow,
                                    nkft_int32* oh,
                                    nkft_int32* ob = nullptr);

            void ClearTexData();
            void Clear();
            bool IsBuilt() const { return texReady; }

        private:
            nkft_uint8* mTexPixelsRGBA = nullptr;
    };

    // ============================================================
    // NkFont
    // ============================================================

    static constexpr nkft_uint32 NK_FONT_MAX_GLYPHS  = 4096u;
    static constexpr nkft_uint32 NK_FONT_INDEX_SIZE = 256u;

    /**
     * @brief Sommet de contour vectoriel d'un glyphe.
     */
    struct NkFontOutlineVertex {
        nkft_float32 x, y;          ///< Position du sommet (en pixels écran, échelle appliquée).
        nkft_bool    isEndOfContour; ///< true si ce sommet termine un contour fermé.
    };

    // ============================================================
    // NkFontGlyph3DVertex — Vertex pour mesh 3D extrudé
    // ============================================================

    struct NkFontGlyph3DVertex {
        math::NkVec3f position;    // Position 3D monde
        math::NkVec3f normal;      // Normale unitaire
        math::NkVec2f uv;          // UV dans l'atlas (pour shader texturé)
        math::NkVec4f color;       // Couleur vertex (RGBA)
        nkft_uint32 glyphIndex;  // Index du glyphe dans la fonte
        nkft_uint32 faceType;    // 0=avant, 1=arrière, 2=latérale
    };

    // ============================================================
    // NkFontGlyphMesh3D — Mesh complet pour un glyphe extrudé
    // ============================================================

    struct NkFontGlyphMesh3D {
        NkVector<NkFontGlyph3DVertex> vertices;
        NkVector<nkft_uint16> indices;  // Optionnel : index buffer pour GPU
        nkft_float32 advanceX;          // Advance pour positionnement suivant
        nkft_float32 ascent, descent;   // Métriques verticales
    };

    // ============================================================
    // Callback pour extraction vertex-par-vertex
    // ============================================================

    using NkFont3DVertexCallback = void(*)(const NkFontGlyph3DVertex* vertex, usize vertexCount, void* userData);

    struct NkFont {
            NkFontAtlas* containerAtlas = nullptr;
            NkFontConfig config;
            nkft_float32 fontSize = 0, ascent = 0, descent = 0, lineAdvance = 0, scale = 1;
            NkFontGlyph  glyphs[NK_FONT_MAX_GLYPHS];
            nkft_uint32  glyphCount = 0u;
            nkft_uint16  indexLookup[NK_FONT_INDEX_SIZE];
            const NkFontGlyph* fallbackGlyph = nullptr;
            nkft_float32       fallbackAdvanceX = 0;

            // ── Stockage interne pour le parser (utilisé par GetGlyphOutline) ─────
            struct nkfont::NkFontFaceInfo* m_FaceInfo = nullptr;

            NkFont();
            ~NkFont() = default;

            const NkFontGlyph* FindGlyph(NkFontCodepoint c) const;
            const NkFontGlyph* FindGlyphNoFallback(NkFontCodepoint c) const;
            nkft_float32 GetCharAdvance(NkFontCodepoint c) const {
                const NkFontGlyph* g = FindGlyph(c);
                return g ? g->advanceX : fallbackAdvanceX;
            }
            nkft_float32 CalcTextSizeX(const char* text,
                                    const char* textEnd = nullptr) const;
            static NkFontCodepoint DecodeUTF8(const char** text, const char* textEnd);

            /**
             * @brief Récupère les contours vectoriels d'un glyphe.
             *
             * Les contours sont aplatis en segments de droite (tolérance 0.35 pixel)
             * et peuvent être utilisés pour générer des maillages 3D extrudés.
             *
             * @param cp           Codepoint Unicode.
             * @param outVertices  Vecteur de sommets (positions + marqueur de fin de contour).
             * @return true        Si le glyphe existe et ses contours ont été extraits.
             */
            bool GetGlyphOutlinePoints(NkFontCodepoint cp, NkVector<NkFontOutlineVertex>& outVertices) const;

        public:
            void BuildLookupTable();
            void AddGlyph(const NkFontGlyph& g);
            void AddGlyphSorted(const NkFontGlyph& g);
            nkft_int32 FindGlyphIndex(NkFontCodepoint c) const;
            NkFontCodepoint glyphSortedCodepoints[NK_FONT_MAX_GLYPHS];
            nkft_uint16     glyphSortedIndices   [NK_FONT_MAX_GLYPHS];
            nkft_uint32     sortedCount = 0u;
            friend struct NkFontAtlas;

        public:
            // --- MÉTHODES 3D EXTRUSION ---

            /**
             * @brief Génère le mesh 3D complet pour un glyphe.
             * @param cp           Codepoint Unicode.
             * @param scale        Échelle de transformation (pixels → monde).
             * @param extrusionDepth Profondeur d'extrusion en unités monde.
             * @param worldMatrix  Matrice monde pour positionner le glyphe.
             * @param color        Couleur RGBA appliquée à tous les vertices.
             * @return NkFontGlyphMesh3D contenant les vertices générés.
             */
            NkFontGlyphMesh3D GenerateGlyphMesh3D(
                NkFontCodepoint cp,
                nkft_float32 scale,
                nkft_float32 extrusionDepth,
                const math::NkMat4f& worldMatrix,
                const math::NkVec4f& color) const;

            /**
             * @brief Génère le mesh 3D pour une chaîne de texte complète.
             * @param text         Texte UTF-8 à extruder.
             * @param scale        Échelle de transformation.
             * @param extrusionDepth Profondeur d'extrusion.
             * @param worldMatrix  Matrice monde de base (sera translatée par caractère).
             * @param color        Couleur RGBA.
             * @return NkFontGlyphMesh3D contenant tous les vertices concaténés.
             */
            NkFontGlyphMesh3D GenerateTextMesh3D(
                const char* text,
                nkft_float32 scale,
                nkft_float32 extrusionDepth,
                const math::NkMat4f& worldMatrix,
                const math::NkVec4f& color) const;

            /**
             * @brief Version callback : appelle une fonction pour chaque vertex généré.
             *        Idéal pour streaming vers GPU sans allocation intermédiaire.
             * @param cp           Codepoint Unicode.
             * @param scale        Échelle de transformation.
             * @param extrusionDepth Profondeur d'extrusion.
             * @param worldMatrix  Matrice monde.
             * @param color        Couleur RGBA.
             * @param callback     Fonction appelée pour chaque vertex.
             * @param userData     Donnée utilisateur passée au callback.
             */
            void ForEachGlyph3DVertex(
                NkFontCodepoint cp,
                nkft_float32 scale,
                nkft_float32 extrusionDepth,
                const math::NkMat4f& worldMatrix,
                const math::NkVec4f& color,
                NkFont3DVertexCallback callback,
                void* userData) const;

            /**
             * @brief Version callback pour texte complet.
             */
            void ForEachText3DVertex(
                const char* text,
                nkft_float32 scale,
                nkft_float32 extrusionDepth,
                const math::NkMat4f& worldMatrix,
                const math::NkVec4f& color,
                NkFont3DVertexCallback callback,
                void* userData) const;
    };

    // ============================================================
    // UTF-8
    // ============================================================

    NkFontCodepoint NkFontDecodeUTF8(const char** str, const char* end);
    nkft_int32      NkFontEncodeUTF8(NkFontCodepoint c,
                                     char* out,
                                     nkft_int32 outSize);

    // ============================================================
    // Shaders GLSL prédéfinis
    // ============================================================

    /**
     * @brief Fragment shader pour rendu normal (atlas alpha 8 bits ou RGBA).
     * Utilisez le canal .a pour RGBA32.
     */
    static constexpr const char* kNkFontFragNormal = R"GLSL(
#version 460 core
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;
layout(binding=0) uniform sampler2D uAtlas;
void main() {
    float alpha = texture(uAtlas, vUV).a;
    if (alpha < 0.01) discard;
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}
)GLSL";

    /**
     * @brief Fragment shader SDF pour rendu net à toute taille.
     *
     * Utiliser avec un atlas généré en mode sdfMode=true.
     * Le paramètre uSmoothing contrôle l'antialiasing (typ. 0.1 / fontSize).
     */
    static constexpr const char* kNkFontFragSDF = R"GLSL(
#version 460 core
in vec2 vUV;
in vec4 vColor;
out vec4 fragColor;

layout(binding=0) uniform sampler2D uAtlas;
layout(binding=1) uniform float uSmoothing; // typ. 0.1/fontSize

void main() {
    float dist = texture(uAtlas, vUV).r;
    float alpha = smoothstep(0.5 - uSmoothing, 0.5 + uSmoothing, dist);
    if (alpha < 0.01) discard;
    fragColor = vec4(vColor.rgb, alpha * vColor.a);
}
)GLSL";

} // namespace nkentseu

#endif // NK_NKFONT_NKFONT_H

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================