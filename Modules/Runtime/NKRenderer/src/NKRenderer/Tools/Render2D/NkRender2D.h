#pragma once
// =============================================================================
// NkRender2D.h
// Renderer 2D : sprites, formes géométriques, texte, nine-slice, tilemap.
// Batching automatique — une seule texture = un batch = un draw call.
//
// Utilisation typique :
//   renderer.Render2D(cmd, w, h, [&](NkRender2D& r) {
//       r.DrawSprite({10,10,100,100}, albedoTex);
//       r.FillRect  ({200,200,80,40}, NkColorF::Red());
//       r.DrawText  (font, "Hello!", {300,50}, NkColorF::White(), 18.f);
//   });
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Resources/NkVertexFormats.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkFont;
        class NkResourceManager;

        // =============================================================================
        // Configuration
        // =============================================================================
        struct NkRender2DConfig {
            uint32      maxVertices = 65536;
            uint32      maxIndices  = 98304;
            bool        yFlip       = false;   // true = GL (Y+ vers le haut)
            const char* debugName   = "NkRender2D";
        };

        // =============================================================================
        // Flags de sprite
        // =============================================================================
        enum class Nk2DFlags : uint32 {
            NK_NONE   = 0,
            NK_FLIP_X = 1 << 0,
            NK_FLIP_Y = 1 << 1,
            NK_NO_TEX = 1 << 2,
        };
        inline Nk2DFlags operator|(Nk2DFlags a, Nk2DFlags b) {
            return (Nk2DFlags)((uint32)a | (uint32)b);
        }
        inline bool NK2D(Nk2DFlags f, Nk2DFlags bit) {
            return ((uint32)f & (uint32)bit) != 0;
        }

        // =============================================================================
        // NkRender2D
        // =============================================================================
        class NkRender2D {
           public:
                NkRender2D()  = default;
                ~NkRender2D() { Shutdown(); }

                NkRender2D(const NkRender2D&)            = delete;
                NkRender2D& operator=(const NkRender2D&) = delete;

                // ── Init ──────────────────────────────────────────────────────────────────
                bool Init    (NkIDevice* device, NkResourceManager* resources,
                               NkRenderTargetHandle swapRP,
                               const NkRender2DConfig& cfg = {});
                void Shutdown();
                bool IsReady() const { return mReady; }

                // ── Frame — appelé automatiquement par NkRenderer::Render2D ──────────────
                // camera=nullptr → pixel-space (1 unit = 1 pixel, origine haut-gauche)
                void Begin(NkICommandBuffer* cmd, uint32 fbW, uint32 fbH,
                            const NkCamera2D* camera = nullptr);
                void End  ();
                void Flush();

                // ── Clip ──────────────────────────────────────────────────────────────────
                void PushClip  (const NkRectF& r);
                void PopClip   ();
                void ResetClip ();

                // ── Sprites ───────────────────────────────────────────────────────────────
                void DrawSprite(const NkRectF& dst, NkTextureHandle tex,
                                 const NkColorF& tint = NkColorF::White(),
                                 const NkRectF& uv = {0,0,1,1},
                                 Nk2DFlags flags = Nk2DFlags::NK_NONE);

                void DrawSpriteRotated(const NkRectF& dst, NkTextureHandle tex,
                                        float32 angleDeg,
                                        const NkVec2f& pivot = {0.5f,0.5f},
                                        const NkColorF& tint = NkColorF::White(),
                                        const NkRectF& uv = {0,0,1,1});

                void DrawNineSlice(const NkRectF& dst, NkTextureHandle tex,
                                    float32 bL, float32 bR, float32 bT, float32 bB,
                                    const NkColorF& tint = NkColorF::White());

                // ── Formes remplies ───────────────────────────────────────────────────────
                void FillRect      (const NkRectF& r, const NkColorF& c);
                void FillRectGradH (const NkRectF& r, const NkColorF& cL,
                                    const NkColorF& cR);
                void FillRectGradV (const NkRectF& r, const NkColorF& cT,
                                    const NkColorF& cB);
                void FillRoundRect (const NkRectF& r, const NkColorF& c,
                                    float32 radius, uint32 seg = 8);
                void FillCircle    (const NkVec2f& center, float32 radius,
                                    const NkColorF& c, uint32 seg = 32);
                void FillTriangle  (const NkVec2f& a, const NkVec2f& b,
                                    const NkVec2f& c_, const NkColorF& col);
                void FillPolygon   (const NkVec2f* pts, uint32 n,
                                    const NkColorF& c);

                // ── Contours ──────────────────────────────────────────────────────────────
                void DrawRect      (const NkRectF& r, const NkColorF& c,
                                    float32 thick = 1.f);
                void DrawRoundRect (const NkRectF& r, const NkColorF& c,
                                    float32 radius, float32 thick = 1.f,
                                    uint32 seg = 8);
                void DrawCircle    (const NkVec2f& center, float32 radius,
                                    const NkColorF& c, float32 thick = 1.f,
                                    uint32 seg = 32);
                void DrawLine      (const NkVec2f& a, const NkVec2f& b,
                                    const NkColorF& c, float32 thick = 1.f);
                void DrawPolyline  (const NkVec2f* pts, uint32 n,
                                    const NkColorF& c, float32 thick = 1.f,
                                    bool closed = false);
                void DrawBezier    (const NkVec2f& p0, const NkVec2f& p1,
                                    const NkVec2f& p2, const NkVec2f& p3,
                                    const NkColorF& c, float32 thick = 1.f,
                                    uint32 steps = 24);
                void DrawArrow     (const NkVec2f& from, const NkVec2f& to,
                                    const NkColorF& c, float32 thick = 1.f,
                                    float32 headSz = 10.f);

                // ── Image (alias pratique pour DrawSprite sans flip/uv) ────────────────
                void DrawImage(NkTextureHandle tex, const NkRectF& dst,
                                const NkRectF& uv = {0,0,1,1},
                                const NkColorF& tint = NkColorF::White());

                // ── Texte ─────────────────────────────────────────────────────────────────
                void DrawText        (NkFont* font, const char* text,
                                      const NkVec2f& pos,
                                      const NkColorF& c = NkColorF::White(),
                                      float32 size = 0.f, float32 maxW = 0.f,
                                      float32 lineSpacing = 1.2f);
                void DrawTextCentered(NkFont* font, const char* text,
                                      const NkRectF& rect,
                                      const NkColorF& c = NkColorF::White(),
                                      float32 size = 0.f);
                void DrawTextRight   (NkFont* font, const char* text,
                                      const NkRectF& rect,
                                      const NkColorF& c = NkColorF::White(),
                                      float32 size = 0.f);
                float32 MeasureText  (NkFont* font, const char* text,
                                      float32 size = 0.f) const;

                // ── Tilemap ───────────────────────────────────────────────────────────────
                // Dessiner une grille de tiles depuis un atlas (tileset)
                // tiles[row*cols+col] = index dans l'atlas (0-based)
                void DrawTilemap(const int32* tiles, uint32 cols, uint32 rows,
                                  NkTextureHandle atlas,
                                  uint32 tileW, uint32 tileH,
                                  uint32 atlasColumns,
                                  const NkVec2f& origin = {0,0},
                                  const NkColorF& tint = NkColorF::White());

                // ── Statistiques ──────────────────────────────────────────────────────────
                struct Stats {
                    uint32 drawCalls = 0, vertices = 0,
                           indices = 0, texSwaps = 0;
                };
                const Stats& GetStats() const { return mLastStats; }

           private:
                // ── Batch ─────────────────────────────────────────────────────────────────
                struct Batch {
                    NkTextureHandle tex;
                    uint32          idxStart   = 0;
                    uint32          idxCount   = 0;
                    NkScissorI      scissor;
                    bool            hasScissor = false;
                };

                NkIDevice*         mDev       = nullptr;
                NkResourceManager* mResources = nullptr;
                NkICommandBuffer*  mCmd       = nullptr;
                bool               mReady     = false;
                NkRender2DConfig   mCfg;

                // GPU resources (IDs RHI opaques — pas d'inclusion NKRHI)
                uint64             mShaderRHI    = 0;
                uint64             mVBORHI       = 0;
                uint64             mIBORHI       = 0;
                uint64             mUBORHI       = 0;
                uint64             mPipelineRHI  = 0;
                uint64             mDescLayoutRHI= 0;
                uint64             mDescSetRHI   = 0;
                uint64             mSamplerRHI   = 0;

                // Batch CPU
                NkVector<NkVertex2D> mVerts;
                NkVector<uint32>     mIdx;
                NkVector<Batch>      mBatches;
                NkTextureHandle      mCurTex;
                NkScissorI           mCurScissor;
                bool                 mHasScissor = false;

                // Clip stack
                NkVector<NkRectF>    mClipStack;

                // Frame state
                uint32   mFBW = 0, mFBH = 0;
                NkMat4f  mVP;

                // Stats
                Stats    mLastStats{};
                Stats    mCurStats{};

                // ── Helpers internes ──────────────────────────────────────────────────────
                bool     CreatePipeline(uint64 rhiRenderPass);
                void     EnsureTex     (NkTextureHandle tex);
                void     AddQuad       (const NkVertex2D& v0, const NkVertex2D& v1,
                                        const NkVertex2D& v2, const NkVertex2D& v3);
                void     AddTri        (const NkVertex2D& a, const NkVertex2D& b,
                                        const NkVertex2D& c);
                void     FlushInternal ();
                void     NewBatch      (NkTextureHandle tex);
        };

    } // namespace renderer
} // namespace nkentseu
