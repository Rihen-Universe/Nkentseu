#pragma once
// =============================================================================
// NkRenderer2D.h  —  Renderer 2D : sprites, formes, texte, batching auto
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkVertex.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRenderDevice.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
namespace render {

class NkFont;

// =============================================================================
// Config
// =============================================================================
struct NkRenderer2DConfig {
    uint32      maxVertices = 65536;
    uint32      maxIndices  = 98304;
    bool        yFlip       = false; // true = GL (Y+ vers le haut)
    const char* debugName   = "NkRenderer2D";
};

// =============================================================================
// Draw flags
// =============================================================================
enum class Nk2DFlags : uint32 {
    NK_NONE    = 0,
    NK_FLIP_X  = 1<<0,
    NK_FLIP_Y  = 1<<1,
    NK_NO_TEX  = 1<<2,
};
inline Nk2DFlags operator|(Nk2DFlags a,Nk2DFlags b){return(Nk2DFlags)((uint32)a|(uint32)b);}
inline bool NK2D(Nk2DFlags f,Nk2DFlags bit){return((uint32)f&(uint32)bit)!=0;}

// =============================================================================
// NkRenderer2D
// =============================================================================
class NkRenderer2D {
public:
    NkRenderer2D()=default;
    ~NkRenderer2D(){Shutdown();}
    NkRenderer2D(const NkRenderer2D&)=delete;
    NkRenderer2D& operator=(const NkRenderer2D&)=delete;

    // ── Init ──────────────────────────────────────────────────────────────────
    bool Init(NkRenderDevice* dev, NkRenderPassHandle swapRP,
              const NkRenderer2DConfig& cfg = {});
    void Shutdown();
    bool IsReady() const { return mDev && mReady; }

    // ── Frame ─────────────────────────────────────────────────────────────────
    // camera=nullptr → pixel-space (1 unit = 1 pixel, origine haut-gauche)
    void Begin(NkICommandBuffer* cmd, uint32 fbW, uint32 fbH,
               const NkCamera2D* camera = nullptr);
    void End();
    void Flush();

    // ── Clip ──────────────────────────────────────────────────────────────────
    void PushClip(const NkRectF& r);
    void PopClip();
    void ResetClip();

    // ── Sprites ───────────────────────────────────────────────────────────────
    void DrawSprite(const NkRectF& dst, NkTexturePtr tex,
                    const NkColorF& tint = NkColorF::White(),
                    const NkRectF& uv = {0,0,1,1},
                    Nk2DFlags flags = Nk2DFlags::NK_NONE);

    void DrawSpriteRotated(const NkRectF& dst, NkTexturePtr tex,
                            float32 angleDeg,
                            const NkVec2f& pivot = {0.5f,0.5f},
                            const NkColorF& tint = NkColorF::White(),
                            const NkRectF& uv = {0,0,1,1});

    void DrawNineSlice(const NkRectF& dst, NkTexturePtr tex,
                       float32 bL, float32 bR, float32 bT, float32 bB,
                       const NkColorF& tint = NkColorF::White());

    // ── Formes ────────────────────────────────────────────────────────────────
    void FillRect      (const NkRectF& r, const NkColorF& c);
    void FillRectH     (const NkRectF& r, const NkColorF& cL, const NkColorF& cR);
    void FillRectV     (const NkRectF& r, const NkColorF& cT, const NkColorF& cB);
    void FillRoundRect (const NkRectF& r, const NkColorF& c, float32 radius, uint32 seg=8);
    void FillCircle    (const NkVec2f& center, float32 radius, const NkColorF& c, uint32 seg=32);
    void FillTriangle  (const NkVec2f& a, const NkVec2f& b, const NkVec2f& c_, const NkColorF& col);

    void DrawRect      (const NkRectF& r, const NkColorF& c, float32 thick=1.f);
    void DrawRoundRect (const NkRectF& r, const NkColorF& c, float32 radius, float32 thick=1.f, uint32 seg=8);
    void DrawCircle    (const NkVec2f& center, float32 radius, const NkColorF& c, float32 thick=1.f, uint32 seg=32);
    void DrawLine      (const NkVec2f& a, const NkVec2f& b, const NkColorF& c, float32 thick=1.f);
    void DrawPolyline  (const NkVec2f* pts, uint32 n, const NkColorF& c, float32 thick=1.f, bool closed=false);
    void DrawBezier    (const NkVec2f& p0, const NkVec2f& p1,
                        const NkVec2f& p2, const NkVec2f& p3,
                        const NkColorF& c, float32 thick=1.f, uint32 steps=24);
    void DrawArrow     (const NkVec2f& from, const NkVec2f& to,
                        const NkColorF& c, float32 thick=1.f, float32 headSz=10.f);

    // ── Image ─────────────────────────────────────────────────────────────────
    void DrawImage(NkTexturePtr tex, const NkRectF& dst,
                   const NkRectF& uv={0,0,1,1},
                   const NkColorF& tint=NkColorF::White());

    // ── Texte ─────────────────────────────────────────────────────────────────
    void DrawText        (NkFont* font, const char* text, const NkVec2f& pos,
                          const NkColorF& c=NkColorF::White(), float32 size=0.f,
                          float32 maxW=0.f, float32 lineSpacing=1.2f);
    void DrawTextCentered(NkFont* font, const char* text, const NkRectF& rect,
                          const NkColorF& c=NkColorF::White(), float32 size=0.f);
    void DrawTextRight   (NkFont* font, const char* text, const NkRectF& rect,
                          const NkColorF& c=NkColorF::White(), float32 size=0.f);
    float32 MeasureText (NkFont* font, const char* text, float32 size=0.f) const;

    // ── Stats ─────────────────────────────────────────────────────────────────
    struct Stats { uint32 drawCalls=0, vertices=0, indices=0, texSwaps=0; };
    const Stats& GetStats() const { return mLastStats; }

private:
    // ── Batch ─────────────────────────────────────────────────────────────────
    struct Batch {
        NkTexturePtr  tex;
        uint32        idxStart = 0;
        uint32        idxCount = 0;
        NkScissorI    scissor;
        bool          hasScissor = false;
    };

    NkRenderDevice*      mDev    = nullptr;
    NkICommandBuffer*    mCmd    = nullptr;
    bool                 mReady  = false;
    NkRenderer2DConfig   mCfg;

    // GPU resources
    NkShaderPtr          mShader;
    NkMeshPtr            mBatchMesh;
    NkUniformPtr         mViewportUBO;
    NkDescSetHandle      mDescLayout;
    NkPipelineHandle     mPipeline;

    // Batch CPU
    NkVector<NkVertex2D> mVerts;
    NkVector<uint32>     mIdx;
    NkVector<Batch>      mBatches;
    NkTexturePtr         mCurTex;
    NkScissorI           mCurScissor;
    bool                 mHasScissor = false;

    // Frame state
    uint32  mFBW=0, mFBH=0;
    NkMat4f mVP;

    // Stats
    Stats mLastStats{};
    Stats mCurStats{};

    // ── Helpers ───────────────────────────────────────────────────────────────
    bool CreatePipeline(NkRenderPassHandle rp);
    void EnsureTex(NkTexturePtr tex);
    void AddQuad(const NkVertex2D& v0,const NkVertex2D& v1,
                 const NkVertex2D& v2,const NkVertex2D& v3);
    void AddTri (const NkVertex2D& a, const NkVertex2D& b, const NkVertex2D& c);
    void FlushInternal();
    void NewBatch(NkTexturePtr tex);
    void UpdateBatchCount(uint32 addIdx);
};

} // namespace render
} // namespace nkentseu