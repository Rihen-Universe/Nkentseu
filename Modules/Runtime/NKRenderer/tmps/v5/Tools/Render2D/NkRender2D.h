#pragma once
// =============================================================================
// NkRender2D.h  — NKRenderer v4.0  (Tools/Render2D/)
// Sprites, shapes, 9-slice, clip stack, batching automatique.
// =============================================================================
#include "../../Core/NkRendererTypes.h"
#include "../../Core/NkTextureLibrary.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
namespace renderer {

    class NkRender2D {
    public:
        NkRender2D() = default;
        ~NkRender2D();

        bool Init(NkIDevice* device, NkTextureLibrary* texLib,
                   uint32 maxVerts = 65536);
        void Shutdown();

        // ── Frame ──────────────────────────────────────────────────────────────
        void Begin(NkICommandBuffer* cmd, uint32 w, uint32 h,
                    float32 cX=0, float32 cY=0, float32 zoom=1.f, float32 rotDeg=0.f);
        void End();
        void FlushPending(NkICommandBuffer* cmd);

        // ── Sprites ────────────────────────────────────────────────────────────
        void DrawSprite(NkRectF dst, NkTexHandle tex,
                         NkVec4f tint={1,1,1,1}, NkRectF uv={0,0,1,1});
        void DrawSpriteRotated(NkRectF dst, NkTexHandle tex,
                                 float32 angleDeg, NkVec2f pivot={0.5f,0.5f},
                                 NkVec4f tint={1,1,1,1}, NkRectF uv={0,0,1,1});
        void DrawNineSlice(NkRectF dst, NkTexHandle tex,
                            float32 left, float32 top, float32 right, float32 bottom,
                            NkVec4f tint={1,1,1,1});

        // ── Formes ────────────────────────────────────────────────────────────
        void FillRect      (NkRectF r,   NkVec4f color);
        void FillRectGradH (NkRectF r,   NkVec4f left, NkVec4f right);
        void FillRectGradV (NkRectF r,   NkVec4f top,  NkVec4f bottom);
        void FillRoundRect (NkRectF r,   NkVec4f color, float32 radius);
        void FillCircle    (NkVec2f c,   float32 radius, NkVec4f color, uint32 segs=32);
        void FillTriangle  (NkVec2f a, NkVec2f b, NkVec2f c, NkVec4f color);
        void DrawRect      (NkRectF r,   NkVec4f color, float32 thick=1.f);
        void DrawRoundRect (NkRectF r,   NkVec4f color, float32 radius, float32 thick=1.f);
        void DrawCircle    (NkVec2f c,   float32 radius, NkVec4f color, float32 thick=1.f, uint32 segs=32);
        void DrawLine      (NkVec2f a, NkVec2f b, NkVec4f color, float32 thick=1.f);
        void DrawArc       (NkVec2f c, float32 r, float32 a0, float32 a1, NkVec4f color, float32 thick=1.f);
        void DrawPolyline  (const NkVec2f* pts, uint32 n, NkVec4f color, float32 thick=1.f, bool closed=false);
        void DrawBezier    (NkVec2f p0, NkVec2f p1, NkVec2f p2, NkVec2f p3,
                             NkVec4f color, float32 thick=1.f, uint32 segs=32);

        // ── Image générale ────────────────────────────────────────────────────
        void DrawImage(NkTexHandle tex, NkRectF dst, NkVec4f tint={1,1,1,1});

        // ── Clip regions ──────────────────────────────────────────────────────
        void PushClip(NkRectF rect);
        void PopClip();

        // ── Blend / Layer ─────────────────────────────────────────────────────
        void SetBlendMode(NkBlendMode mode);
        void SetLayer(uint8 layer);

        // ── Stats ─────────────────────────────────────────────────────────────
        uint32 GetBatchCount()  const { return mBatchCount; }
        uint32 GetVertexCount() const { return mVertCount; }

    private:
        struct Vert2D {
            NkVec2f pos; NkVec2f uv; uint32 color; uint32 texIdx;
        };
        struct Batch {
            NkTexHandle tex; NkBlendMode blend; uint8 layer;
            uint32 vStart; uint32 vCount;
        };

        NkIDevice*        mDevice   = nullptr;
        NkTextureLibrary* mTexLib   = nullptr;
        NkICommandBuffer* mCmd      = nullptr;
        NkVector<Vert2D>  mVerts;
        NkVector<Batch>   mBatches;
        NkVector<NkRectF> mClipStack;
        NkBufferHandle    mVBO, mIBO;
        NkPipelineHandle  mPipeAlpha, mPipeAdd, mPipeOpaque;
        NkBlendMode       mBlend   = NkBlendMode::NK_ALPHA;
        uint8             mLayer   = 0;
        bool              mInFrame = false;
        uint32            mW=0, mH=0;
        NkMat4f           mOrtho;
        uint32            mBatchCount=0, mVertCount=0;

        uint32 PackColor(NkVec4f c) {
            uint8 r=(uint8)(c.x*255); uint8 g=(uint8)(c.y*255);
            uint8 b=(uint8)(c.z*255); uint8 a=(uint8)(c.w*255);
            return ((uint32)a<<24)|((uint32)b<<16)|((uint32)g<<8)|r;
        }
        void Flush();
        void PushQuad(NkVec2f tl,NkVec2f tr,NkVec2f br,NkVec2f bl,
                       NkVec2f uvTL,NkVec2f uvTR,NkVec2f uvBR,NkVec2f uvBL,
                       NkVec4f color, NkTexHandle tex);
    };

} // namespace renderer
} // namespace nkentseu
