#pragma once
// =============================================================================
// NkRender2D.h
// Outil de rendu 2D : sprites, shapes, texte, tilemaps, effets.
//
// Features :
//   - Batch automatique (minimize les draw calls)
//   - Sprites avec atlas (NkSpriteAtlas)
//   - Formes géométriques (rect, cercle, ligne, polygone, bézier)
//   - Texte bitmap et TTF
//   - Nine-slice pour UI
//   - Tilemap orthogonale et isométrique
//   - Effets 2D : particules 2D, distortion, chromatic aberration
//   - Camera 2D avec zoom + pan + bounds
//   - Coordonnées pixels (top-left) ou normalisées
//
// Usage :
//   auto& r2d = *renderer->GetRender2D();
//   r2d.Begin(camera2D);
//   r2d.DrawSprite(sprite, {100, 200});
//   r2d.DrawRect({50,50,200,100}, color);
//   r2d.DrawText("Hello!", font, {10,10}, 24.f, WHITE);
//   r2d.End();
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkRectangle.h"

namespace nkentseu {

    // =============================================================================
    // Couleur inline (RGBA packed)
    // =============================================================================
    using NkColor32 = math::NkColor;

    // =============================================================================
    // Camera 2D
    // =============================================================================
    struct NkCamera2D {
        math::NkVec2  position   = {0, 0};    // centre du monde affiché
        float32       zoom       = 1.f;
        float32       rotation   = 0.f;        // radians
        math::NkVec2  viewSize   = {1280, 720};

        // Transforme un point monde → écran
        math::NkVec2 WorldToScreen(math::NkVec2 world) const;
        math::NkVec2 ScreenToWorld(math::NkVec2 screen) const;

        // Matrice de projection orthographique
        math::NkMat4 GetProjectionMatrix() const;
    };

    // =============================================================================
    // Sprite
    // =============================================================================
    struct NkSprite {
        NkTexId         texture    = NK_INVALID_TEX;
        math::NkVec2    uvMin      = {0, 0};   // coordonnées UV dans l'atlas
        math::NkVec2    uvMax      = {1, 1};
        math::NkVec2    pivot      = {0.5f, 0.5f}; // (0,0)=top-left (0.5,0.5)=centre
        math::NkVec2    size       = {1, 1};   // taille naturelle en pixels
        bool            flipX      = false;
        bool            flipY      = false;

        // Crée un sprite depuis une texture entière
        static NkSprite FromTexture(NkTexId tex, float32 w = 0.f, float32 h = 0.f);

        // Crée un sprite depuis un atlas
        static NkSprite FromAtlas(NkTexId atlas, math::NkVec2 uvMin, math::NkVec2 uvMax, math::NkVec2 size);
    };

    // =============================================================================
    // Atlas de sprites (spritesheet)
    // =============================================================================
    class NkSpriteAtlas {
        public:
            NkSpriteAtlas() = default;
            NkSpriteAtlas(NkTexId texture, uint32 cellW, uint32 cellH,
                        uint32 columns, uint32 rows,
                        uint32 texW, uint32 texH);

            NkSprite GetSprite(uint32 column, uint32 row) const;
            NkSprite GetSprite(uint32 linearIndex)        const;
            uint32   GetCount() const { return mColumns * mRows; }

        private:
            NkTexId  mTexture = NK_INVALID_TEX;
            uint32   mCellW=0, mCellH=0, mTexW=0, mTexH=0;
            uint32   mColumns=0, mRows=0;
    };

    // =============================================================================
    // Paramètres de dessin d'un sprite
    // =============================================================================
    struct NkDrawSpriteParams {
        math::NkVec2  position    = {0, 0};
        math::NkVec2  scale       = {1, 1};
        float32       rotation    = 0.f;       // radians
        NkColor32     tint        = NkColor32::White;
        int32         zOrder      = 0;
        float32       depth       = 0.f;       // 0..1
    };

    // =============================================================================
    // Paramètres de dessin de texte
    // =============================================================================
    struct NkDrawTextParams {
        math::NkVec2  position    = {0, 0};
        float32       size        = 16.f;
        NkColor32     color       = NkColor32::White;
        NkColor32     outlineColor= NkColor32::Black;
        float32       outlineWidth= 0.f;
        float32       lineSpacing = 1.2f;
        bool          bold        = false;
        bool          italic      = false;
        int32         align       = 0;   // 0=left 1=center 2=right
        float32       maxWidth    = 0.f; // 0=pas de wrap
    };

    // =============================================================================
    // NkRender2D
    // =============================================================================
    class NkRender2D {
        public:
            explicit NkRender2D(NkIDevice* device, NkTextureLibrary* texLib);
            ~NkRender2D();

            bool Initialize(NkRenderPassHandle renderPass, uint32 maxVertices = 131072);
            void Shutdown();
            void OnResize(uint32 width, uint32 height);

            // ── Frame ─────────────────────────────────────────────────────────────────
            void Begin(const NkCamera2D& camera);
            void Begin(uint32 viewW, uint32 viewH);  // caméra par défaut (pixel space)
            void End();                               // flush le batch
            void Flush();                             // flush partiel (ex: entre layers)

            // ── Sprites ───────────────────────────────────────────────────────────────
            void DrawSprite(const NkSprite& sprite, math::NkVec2 position,
                            NkColor32 tint = NkColor32::White,
                            float32 rotation = 0.f, math::NkVec2 scale = {1,1});
            void DrawSprite(const NkSprite& sprite, const NkDrawSpriteParams& params);
            void DrawTexture(NkTexId tex, math::NkVec2 position, math::NkVec2 size,
                            NkColor32 tint = NkColor32::White);
            void DrawTextureRegion(NkTexId tex,
                                    math::NkVec2 destPos, math::NkVec2 destSize,
                                    math::NkVec2 srcPos,  math::NkVec2 srcSize,
                                    NkColor32 tint = NkColor32::White);

            // Nine-slice (pour UI, boutons)
            void DrawNineSlice(NkTexId tex,
                                math::NkVec2 position, math::NkVec2 size,
                                float32 borderL, float32 borderR,
                                float32 borderT, float32 borderB,
                                NkColor32 tint = NkColor32::White);

            // ── Formes géométriques ───────────────────────────────────────────────────
            void DrawRect(math::NkVec2 pos, math::NkVec2 size, NkColor32 color,
                        float32 rotation = 0.f);
            void DrawRectOutline(math::NkVec2 pos, math::NkVec2 size, NkColor32 color,
                                float32 thickness = 1.f);
            void DrawRoundedRect(math::NkVec2 pos, math::NkVec2 size, float32 radius,
                                NkColor32 color, uint32 segments = 8);
            void DrawCircle(math::NkVec2 center, float32 radius, NkColor32 color,
                            uint32 segments = 32);
            void DrawCircleOutline(math::NkVec2 center, float32 radius, NkColor32 color,
                                    float32 thickness = 1.f, uint32 segments = 32);
            void DrawLine(math::NkVec2 from, math::NkVec2 to, NkColor32 color,
                        float32 thickness = 1.f);
            void DrawArrow(math::NkVec2 from, math::NkVec2 to, NkColor32 color,
                            float32 thickness = 1.f, float32 headSize = 10.f);
            void DrawTriangle(math::NkVec2 a, math::NkVec2 b, math::NkVec2 c,
                            NkColor32 color);
            void DrawPolygon(const math::NkVec2* points, uint32 count,
                            NkColor32 color, bool filled = true);
            void DrawBezier(math::NkVec2 p0, math::NkVec2 p1,
                            math::NkVec2 p2, math::NkVec2 p3,
                            NkColor32 color, float32 thickness = 1.f,
                            uint32 segments = 32);

            // Dégradé
            void DrawGradientRect(math::NkVec2 pos, math::NkVec2 size,
                                NkColor32 topLeft, NkColor32 topRight,
                                NkColor32 botLeft, NkColor32 botRight);

            // ── Texte ─────────────────────────────────────────────────────────────────
            // fontId : identifiant retourné par NkUIFontManager ou un système de polices TTF
            void DrawText(const NkString& text, uint32 fontId,
                        math::NkVec2 position, float32 size = 16.f,
                        NkColor32 color = NkColor32::White);
            void DrawText(const NkString& text, uint32 fontId,
                        const NkDrawTextParams& params);
            math::NkVec2 MeasureText(const NkString& text, uint32 fontId, float32 size) const;

            // ── Tilemap ───────────────────────────────────────────────────────────────
            struct Tilemap {
                const uint32*    tiles       = nullptr;   // indices dans l'atlas
                uint32           width       = 0;
                uint32           height      = 0;
                NkSpriteAtlas*   atlas       = nullptr;
                float32          tileWidth   = 32.f;
                float32          tileHeight  = 32.f;
            };

            void DrawTilemap(const Tilemap& tilemap,
                            math::NkVec2 worldOffset = {0,0},
                            NkColor32 tint = NkColor32::White);
            void DrawTilemapIsometric(const Tilemap& tilemap,
                                    math::NkVec2 worldOffset = {0,0});

            // ── Contrôle du rendu ─────────────────────────────────────────────────────
            void SetBlendMode(bool alpha, bool additive = false);
            void PushClipRect(math::NkVec2 pos, math::NkVec2 size);
            void PopClipRect();
            void SetZOrder(int32 z)    { mCurrentZ = z; }
            void SetDepth(float32 d)   { mCurrentDepth = d; }
            void SetLayer(uint32 layer);   // sépare l'espace dans le batch (background/sprites/UI)

            // ── Statistiques ──────────────────────────────────────────────────────────
            uint32 GetBatchCount()   const { return mBatchCount; }
            uint32 GetDrawCallCount()const { return mDrawCallCount; }
            uint32 GetVertexCount()  const { return mVertexCount; }

            // ── Internal (appelé par NkRenderer) ──────────────────────────────────────
            void SubmitBatch(NkICommandBuffer* cmd);

        private:
            struct Vertex2D {
                math::NkVec2  pos;
                math::NkVec2  uv;
                uint32        color;
                float32       texId; // index dans le tableau de textures du batch
            };

            struct Batch {
                NkTexId       texture     = NK_INVALID_TEX;
                uint32        vertexStart = 0;
                uint32        indexStart  = 0;
                uint32        indexCount  = 0;
                bool          alpha       = true;
                bool          additive    = false;
            };

            NkIDevice*         mDevice;
            NkTextureLibrary*  mTexLib;
            NkICommandBuffer*  mCmdBuffer = nullptr;

            NkVector<Vertex2D> mVertices;
            NkVector<uint32>   mIndices;
            NkVector<Batch>    mBatches;
            NkBufferHandle     mVBO, mIBO;
            uint64             mVBOCap = 0, mIBOCap = 0;

            NkPipelineHandle   mPipelineAlpha;
            NkPipelineHandle   mPipelineAdditive;
            NkPipelineHandle   mPipelineOpaque;
            NkDescSetHandle    mLayout;
            NkShaderHandle     mShader;
            NkBufferHandle     mCameraUBO;
            NkDescSetHandle    mCameraDescSet;
            NkSamplerHandle    mSampler;

            NkCamera2D         mCamera;
            int32              mCurrentZ = 0;
            float32            mCurrentDepth = 0.f;
            uint32             mCurrentLayer = 0;

            uint32 mBatchCount    = 0;
            uint32 mDrawCallCount = 0;
            uint32 mVertexCount   = 0;

            NkVector<math::NkVec4> mClipStack;

            void AddQuad(math::NkVec2 p0, math::NkVec2 p1,
                        math::NkVec2 p2, math::NkVec2 p3,
                        math::NkVec2 uv0, math::NkVec2 uv1,
                        math::NkVec2 uv2, math::NkVec2 uv3,
                        uint32 color, NkTexId tex);

            Batch& GetOrCreateBatch(NkTexId tex, bool alpha, bool additive);
            void FlushBatch(NkICommandBuffer* cmd, const Batch& batch);
            void EnsureCapacity(uint32 extraVertices, uint32 extraIndices);
    };

} // namespace nkentseu