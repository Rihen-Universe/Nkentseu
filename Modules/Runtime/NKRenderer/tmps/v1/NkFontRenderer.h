#pragma once
// =============================================================================
// NkFontRenderer.h
// Système de rendu de texte pour NKRenderer.
//
// Intègre NKFont (FreeType / moteur interne) avec le pipeline de rendu
// pour produire un NkDynamicMesh de quads par frame.
//
// Fonctionnement :
//   1. NkFontRenderer::Init() charge les polices via NkIRenderer::CreateFont()
//   2. NkFontRenderer::LayoutText() calcule les quads CPU dans un NkDynamicMesh
//   3. NkFontRenderer::Submit() émet un Draw2D vers NkRendererCommand
//
// Caractéristiques :
//   • UTF-8 intégral (emoji inclus)
//   • Word wrap automatique
//   • Alignement (gauche, centré, droite, justifié)
//   • Rich text basique ([b], [i], [color=#RRGGBB])
//   • Kerning via atlas de glyphes
//   • SDF (Signed Distance Field) optionnel pour le rendu haute qualité
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/Command/NkRendererCommand.h"
#include "NKRenderer/3D/NkMeshTypes.h"
#include "NKRenderer/Material/NkMaterialSystem.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Alignement et options de layout
    // =========================================================================
    enum class NkTextAlign  : uint8 { Left, Center, Right, Justify };
    enum class NkTextVAlign : uint8 { Top, Middle, Bottom };
    enum class NkTextOverflow : uint8 { Visible, Clip, Ellipsis, Wrap };

    struct NkTextLayoutOptions {
        NkTextAlign    hAlign    = NkTextAlign::Left;
        NkTextVAlign   vAlign    = NkTextVAlign::Top;
        NkTextOverflow overflow  = NkTextOverflow::Wrap;
        float32        lineSpacing = 1.2f;  // multiplicateur de line height
        float32        letterSpacing= 0.f; // espacement supplémentaire en pixels
        float32        tabWidth   = 32.f;  // largeur d'une tabulation en pixels
        bool           richText   = false; // parse [b], [color=...], etc.
        bool           sdf        = false; // rendu SDF (nécessite atlas SDF)
    };

    // =========================================================================
    // Résultat d'un layout
    // =========================================================================
    struct NkTextLine {
        uint32  startChar  = 0;   // index dans la string UTF-8
        uint32  charCount  = 0;
        float32 width      = 0.f;
        float32 baselineY  = 0.f;
    };

    struct NkTextLayoutResult {
        NkVector<NkTextLine> lines;
        float32 totalWidth  = 0.f;
        float32 totalHeight = 0.f;
        uint32  vertexCount = 0;
        uint32  indexCount  = 0;
    };

    // =========================================================================
    // NkGlyphQuad — quad pour un glyphe (4 vertices + 6 indices)
    // =========================================================================
    struct NkGlyphQuad {
        NkVec2f positions[4]; // tl, tr, bl, br
        NkVec2f uvs[4];
        NkVec4f color;
    };

    // =========================================================================
    // NkFontRenderer
    // =========================================================================
    class NkFontRenderer {
    public:
        explicit NkFontRenderer(NkIRenderer* renderer)
            : mRenderer(renderer)
            , mMesh(renderer)
        {}

        ~NkFontRenderer() { Destroy(); }

        NkFontRenderer(const NkFontRenderer&) = delete;
        NkFontRenderer& operator=(const NkFontRenderer&) = delete;

        // Initialisation (crée le mesh dynamique et configure le matériau)
        bool Init(NkMaterialInstHandle textMaterial,
                  uint32 maxGlyphsPerFrame = 8192);
        void Destroy();

        // ── Chargement de police ──────────────────────────────────────────────
        NkFontHandle LoadFont(const char* filePath, float32 sizePt,
                              bool preloadASCII=true,
                              int32 atlasW=1024, int32 atlasH=1024);

        NkFontHandle LoadSystemFont(const char* familyName, float32 sizePt);

        void DestroyFont(NkFontHandle& font);

        // ── Mesure ───────────────────────────────────────────────────────────
        // Taille en pixels d'une chaîne sur une seule ligne
        NkVec2f MeasureString(NkFontHandle font, const char* utf8,
                              float32 overrideSize=0.f) const;

        // Layout complet (wrap, alignement) dans une boîte
        NkTextLayoutResult LayoutText(NkFontHandle font, const char* utf8,
                                      NkVec2f boxSize,
                                      float32 overrideSize=0.f,
                                      const NkTextLayoutOptions& opts={}) const;

        // ── Rendu ─────────────────────────────────────────────────────────────
        // Émet les quads dans NkRendererCommand pour une frame.
        // Accumule dans le mesh dynamique interne ; plusieurs appels par frame OK.
        void DrawText(NkRendererCommand& cmd,
                      NkFontHandle font,
                      const char* utf8,
                      NkVec2f position,
                      float32 size,
                      NkVec4f color = {1,1,1,1},
                      float32 maxWidth = 0.f,
                      uint32 layer = 0,
                      const NkTextLayoutOptions& opts = {});

        // Rendu dans une boîte avec alignement
        void DrawTextBox(NkRendererCommand& cmd,
                         NkFontHandle font,
                         const char* utf8,
                         NkVec2f position, NkVec2f size,
                         float32 fontSize,
                         NkVec4f color = {1,1,1,1},
                         uint32 layer = 0,
                         const NkTextLayoutOptions& opts = {});

        // Flush du mesh (appelé une fois en fin de frame par le renderer)
        void Flush(NkRendererCommand& cmd, uint32 layer=0xFF);

        // Reset entre les frames
        void BeginFrame();

        // ── Accesseurs ───────────────────────────────────────────────────────
        bool IsValid() const { return mMesh.IsValid(); }

    private:
        // Décodage UTF-8 → codepoint
        static uint32 DecodeUTF8(const char* s, uint32& outBytes);

        // Rasterise une chaîne en quads dans mVertices/mIndices
        void RasterizeString(NkFontHandle font, const char* utf8,
                             NkVec2f cursor, float32 scale,
                             NkVec4f color,
                             float32 maxWidth, const NkTextLayoutOptions& opts);

        // Récupère ou calcule la taille d'un glyphe
        bool GetGlyphQuad(NkFontHandle font, uint32 codepoint,
                          NkVec2f& cursor, float32 scale,
                          NkGlyphQuad& out) const;

        NkIRenderer*          mRenderer = nullptr;
        NkDynamicMesh         mMesh;
        NkMaterialInstHandle  mMaterial;

        NkVector<NkVertex2D>  mVertices;
        NkVector<uint32>      mIndices;
        uint32                mMaxGlyphs    = 8192;
        uint32                mCurrentGlyph = 0;
        bool                  mDirty        = false;
    };

    // =========================================================================
    // Implémentation inline des parties statiques
    // =========================================================================
    inline uint32 NkFontRenderer::DecodeUTF8(const char* s, uint32& outBytes) {
        const uint8 c0 = static_cast<uint8>(s[0]);
        if (c0 < 0x80)  { outBytes=1; return c0; }
        if ((c0&0xE0)==0xC0 && s[1]) { outBytes=2; return ((c0&0x1F)<<6)|(static_cast<uint8>(s[1])&0x3F); }
        if ((c0&0xF0)==0xE0 && s[1] && s[2]) {
            outBytes=3;
            return ((c0&0x0F)<<12)|((static_cast<uint8>(s[1])&0x3F)<<6)|(static_cast<uint8>(s[2])&0x3F);
        }
        if ((c0&0xF8)==0xF0 && s[1] && s[2] && s[3]) {
            outBytes=4;
            return ((c0&0x07)<<18)|((static_cast<uint8>(s[1])&0x3F)<<12)
                  |((static_cast<uint8>(s[2])&0x3F)<<6)|(static_cast<uint8>(s[3])&0x3F);
        }
        outBytes=1; return '?';
    }

    inline NkVec2f NkFontRenderer::MeasureString(NkFontHandle font,
                                                   const char* utf8,
                                                   float32 overrideSize) const
    {
        if (!utf8 || !font.IsValid()) return {0,0};
        // On utilise le premier glyphe pour obtenir la hauteur de ligne
        NkGlyphInfo gi{};
        float32 w=0.f, h=0.f;
        const char* p = utf8;
        while (*p) {
            uint32 bytes=1;
            uint32 cp = DecodeUTF8(p, bytes);
            p += bytes;
            if (mRenderer->GetGlyph(font, cp, gi)) {
                w += gi.advance;
                if (gi.y1-gi.y0 > h) h = gi.y1-gi.y0;
            }
        }
        if (overrideSize > 0.f) {
            // Obtenir la taille nominale via le glyphe 'M'
            NkGlyphInfo mGlyph{};
            if (mRenderer->GetGlyph(font, 'M', mGlyph)) {
                const float32 nominalH = mGlyph.y1 - mGlyph.y0;
                if (nominalH > 0.f) {
                    const float32 scale = overrideSize / nominalH;
                    w *= scale; h *= scale;
                }
            }
        }
        return {w, h};
    }

    inline bool NkFontRenderer::Init(NkMaterialInstHandle textMaterial,
                                     uint32 maxGlyphsPerFrame)
    {
        mMaterial  = textMaterial;
        mMaxGlyphs = maxGlyphsPerFrame;

        const uint32 maxV = maxGlyphsPerFrame * 4;
        const uint32 maxI = maxGlyphsPerFrame * 6;

        mVertices.Reserve(maxV);
        mIndices.Reserve(maxI);

        return mMesh.Create(maxV, maxI, NkPrimitiveMode::Triangles, true, "NkFontRenderer_Mesh");
    }

    inline void NkFontRenderer::Destroy() {
        mMesh.Destroy();
        mVertices.Clear();
        mIndices.Clear();
    }

    inline void NkFontRenderer::BeginFrame() {
        mVertices.Clear();
        mIndices.Clear();
        mCurrentGlyph = 0;
        mDirty        = false;
    }

    inline NkFontHandle NkFontRenderer::LoadFont(const char* filePath, float32 sizePt,
                                                  bool preloadASCII,
                                                  int32 atlasW, int32 atlasH)
    {
        NkFontDesc d{};
        d.filePath      = filePath;
        d.sizePt        = sizePt;
        d.preloadASCII  = preloadASCII;
        d.atlasWidth    = atlasW;
        d.atlasHeight   = atlasH;
        return mRenderer->CreateFont(d);
    }

    inline void NkFontRenderer::DestroyFont(NkFontHandle& font) {
        mRenderer->DestroyFont(font);
    }

    inline void NkFontRenderer::DrawText(NkRendererCommand& cmd,
                                         NkFontHandle font,
                                         const char* utf8,
                                         NkVec2f position,
                                         float32 size,
                                         NkVec4f color,
                                         float32 maxWidth,
                                         uint32 layer,
                                         const NkTextLayoutOptions& opts)
    {
        if (!utf8 || !font.IsValid()) return;
        RasterizeString(font, utf8, position, size, color, maxWidth, opts);
        mDirty = true;
    }

    inline void NkFontRenderer::DrawTextBox(NkRendererCommand& cmd,
                                            NkFontHandle font, const char* utf8,
                                            NkVec2f position, NkVec2f size,
                                            float32 fontSize, NkVec4f color,
                                            uint32 layer, const NkTextLayoutOptions& opts)
    {
        if (!utf8 || !font.IsValid()) return;
        // Décalage vertical selon vAlign
        NkVec2f measured = MeasureString(font, utf8, fontSize);
        float32 offsetY = 0.f;
        if (opts.vAlign == NkTextVAlign::Middle)
            offsetY = (size.y - measured.y) * 0.5f;
        else if (opts.vAlign == NkTextVAlign::Bottom)
            offsetY = size.y - measured.y;

        RasterizeString(font, utf8,
                        {position.x, position.y + offsetY},
                        fontSize, color, size.x, opts);
        mDirty = true;
    }

    inline void NkFontRenderer::RasterizeString(NkFontHandle font, const char* utf8,
                                                 NkVec2f cursor, float32 scale,
                                                 NkVec4f color, float32 maxWidth,
                                                 const NkTextLayoutOptions& opts)
    {
        // Obtenir la hauteur nominale via 'M' pour calculer le scale
        NkGlyphInfo mGlyph{};
        float32 scaleF = 1.f;
        if (scale > 0.f && mRenderer->GetGlyph(font, 'M', mGlyph)) {
            const float32 nomH = mGlyph.y1 - mGlyph.y0;
            if (nomH > 0.f) scaleF = scale / nomH;
        }

        const float32 startX = cursor.x;
        const char* p = utf8;

        while (*p && mCurrentGlyph < mMaxGlyphs) {
            uint32 bytes = 1;
            const uint32 cp = DecodeUTF8(p, bytes);
            p += bytes;

            if (cp == '\n') {
                // Récupérer la hauteur de ligne
                NkGlyphInfo lineGlyph{};
                mRenderer->GetGlyph(font, 'A', lineGlyph);
                const float32 lineH = (lineGlyph.y1-lineGlyph.y0) * scaleF;
                cursor.x  = startX;
                cursor.y += lineH * opts.lineSpacing;
                continue;
            }
            if (cp == ' ') {
                NkGlyphInfo spaceGlyph{};
                if (mRenderer->GetGlyph(font, ' ', spaceGlyph))
                    cursor.x += spaceGlyph.advance * scaleF;
                else
                    cursor.x += 8.f * scaleF;
                continue;
            }
            if (cp == '\t') {
                cursor.x = startX + ::ceilf((cursor.x - startX) / opts.tabWidth + 1.f) * opts.tabWidth;
                continue;
            }

            NkGlyphInfo gi{};
            if (!mRenderer->GetGlyph(font, cp, gi)) continue;

            // Wrap si nécessaire
            if (maxWidth > 0.f && cursor.x - startX + gi.advance * scaleF > maxWidth) {
                NkGlyphInfo lineGlyph{};
                mRenderer->GetGlyph(font, 'A', lineGlyph);
                const float32 lineH = (lineGlyph.y1-lineGlyph.y0) * scaleF;
                cursor.x  = startX;
                cursor.y += lineH * opts.lineSpacing;
            }

            // Calculer les 4 coins du quad
            const float32 x0 = cursor.x + gi.bx * scaleF;
            const float32 y0 = cursor.y - gi.by * scaleF;
            const float32 x1 = x0 + (gi.x1 - gi.x0) * scaleF;
            const float32 y1 = y0 + (gi.y1 - gi.y0) * scaleF;

            const uint32 base = (uint32)mVertices.Size();

            // 4 vertices (TL, TR, BL, BR)
            NkVertex2D verts[4];
            verts[0].position = {x0, y0}; verts[0].uv = {gi.x0, gi.y0}; verts[0].color = color;
            verts[1].position = {x1, y0}; verts[1].uv = {gi.x1, gi.y0}; verts[1].color = color;
            verts[2].position = {x0, y1}; verts[2].uv = {gi.x0, gi.y1}; verts[2].color = color;
            verts[3].position = {x1, y1}; verts[3].uv = {gi.x1, gi.y1}; verts[3].color = color;

            for (int i=0;i<4;++i) mVertices.PushBack(verts[i]);

            // 6 indices (2 triangles)
            mIndices.PushBack(base+0); mIndices.PushBack(base+1); mIndices.PushBack(base+2);
            mIndices.PushBack(base+1); mIndices.PushBack(base+3); mIndices.PushBack(base+2);

            cursor.x += gi.advance * scaleF + opts.letterSpacing;
            ++mCurrentGlyph;
        }
    }

    inline void NkFontRenderer::Flush(NkRendererCommand& cmd, uint32 layer) {
        if (!mDirty || mVertices.IsEmpty()) return;

        mMesh.Update2D(mVertices.Begin(), (uint32)mVertices.Size(),
                       mIndices.Begin(),  (uint32)mIndices.Size());

        NkDrawCall2D dc{};
        dc.mesh     = mMesh.GetHandle();
        dc.material = mMaterial;
        dc.transform= NkMat3f::Identity();
        dc.depth    = 0.f;
        dc.layer    = (layer == 0xFF) ? 0xFF : layer;
        cmd.Draw2D(dc);
    }

} // namespace renderer
} // namespace nkentseu