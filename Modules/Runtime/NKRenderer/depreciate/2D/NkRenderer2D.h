#pragma once
// =============================================================================
// NkRenderer2D.h — Renderer 2D batch : sprites, formes, texte.
//
// Principe :
//   - Begin() démarre un batch.
//   - DrawXxx() accumule des quads dans le buffer CPU.
//   - End() flush → un seul draw call par texture (sort par texture + sortOrder).
//   - Gestion automatique des dépassements de batch (max_quads).
// =============================================================================
#include "NKRenderer/2D/NkSprite.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkShader.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFont/NkFontFace.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {

    // =========================================================================
    // NkVertex2D — vertex du batch 2D (layout : pos, uv, color)
    // =========================================================================
    struct NkVertex2D {
        float32 x, y;     // position monde
        float32 u, v;     // UV texture
        uint32  color;    // RGBA packed (8 bits par canal)
    };

    inline uint32 NkPackColor(float32 r, float32 g, float32 b, float32 a) noexcept {
        uint8 ri = (uint8)(r * 255.f + 0.5f);
        uint8 gi = (uint8)(g * 255.f + 0.5f);
        uint8 bi = (uint8)(b * 255.f + 0.5f);
        uint8 ai = (uint8)(a * 255.f + 0.5f);
        return ((uint32)ri) | ((uint32)gi<<8) | ((uint32)bi<<16) | ((uint32)ai<<24);
    }

    inline uint32 NkPackColor(const NkColor4f& c) noexcept {
        return NkPackColor(c.r, c.g, c.b, c.a);
    }

    // =========================================================================
    // NkRenderer2DDesc — paramètres de création
    // =========================================================================
    struct NkRenderer2DDesc {
        uint32  maxQuadsPerBatch = 10000;  // quads max avant flush auto
        bool    enableBlend      = true;   // alpha blending
        bool    enableDepthTest  = false;  // depth test (2D = généralement off)
        NkGPUFormat colorFormat  = NkGPUFormat::NK_RGBA8_SRGB;
        NkGPUFormat depthFormat  = NkGPUFormat::NK_D32_FLOAT;
    };

    // =========================================================================
    // NkRenderer2D — renderer 2D batch
    // =========================================================================
    class NKRENDERER_API NkRenderer2D {
    public:
        NkRenderer2D()  = default;
        ~NkRenderer2D() { Destroy(); }

        NkRenderer2D(const NkRenderer2D&)            = delete;
        NkRenderer2D& operator=(const NkRenderer2D&) = delete;

        // ── Cycle de vie ──────────────────────────────────────────────────────

        bool Init(NkIDevice* device,
                  NkRenderPassHandle renderPass,
                  const NkRenderer2DDesc& desc = {}) noexcept;
        void Destroy() noexcept;

        // ── Frame ─────────────────────────────────────────────────────────────

        void SetCamera(const NkCamera2D& cam) noexcept;
        void SetViewProjection(const NkMat4& vp) noexcept;

        /// Démarre une passe de rendu 2D.
        void Begin(NkICommandBuffer* cmd,
                   NkFramebufferHandle fb,
                   uint32 fbWidth, uint32 fbHeight) noexcept;

        /// Flush le batch courant et termine la passe.
        void End() noexcept;

        // ── Dessins ───────────────────────────────────────────────────────────

        // Sprite / texture
        void DrawSprite(const NkSprite& sprite) noexcept;
        void DrawTexture(NkTextureRef tex,
                         NkVec2 pos, NkVec2 size,
                         NkColor4f tint = {1,1,1,1},
                         NkUVRect uv    = NkUVRect::Full(),
                         float32  rot   = 0.f) noexcept;

        // Formes colorées (remplies)
        void DrawRect(NkVec2 pos, NkVec2 size,
                      NkColor4f color = {1,1,1,1},
                      float32   rot   = 0.f) noexcept;
        void DrawRoundedRect(NkVec2 pos, NkVec2 size,
                             float32 radius, NkColor4f color = {1,1,1,1}) noexcept;
        void DrawCircle(NkVec2 center, float32 radius,
                        NkColor4f color = {1,1,1,1},
                        uint32 segments = 32) noexcept;

        // Formes en contour (stroke)
        void DrawLine(NkVec2 a, NkVec2 b,
                      float32 thickness = 1.f,
                      NkColor4f color   = {1,1,1,1}) noexcept;
        void DrawRectOutline(NkVec2 pos, NkVec2 size,
                             float32 thickness = 1.f,
                             NkColor4f color   = {1,1,1,1}) noexcept;

        // ── Triangles libres ──────────────────────────────────────────────────

        /// Triangle opaque
        void DrawTriangle(NkVec2 v0, NkVec2 v1, NkVec2 v2,
                          NkColor4f color = {1,1,1,1}) noexcept;

        /// Triangle texturé avec UV personnalisés
        void DrawTriangleTextured(NkVec2 v0, NkVec2 v1, NkVec2 v2,
                                  NkVec2 uv0, NkVec2 uv1, NkVec2 uv2,
                                  NkTextureRef tex,
                                  NkColor4f tint = {1,1,1,1}) noexcept;

        // ── Polygones convexes ────────────────────────────────────────────────

        /// Polygone convexe rempli (fan depuis points[0])
        void DrawConvexPolygon(const NkVec2* points, uint32 count,
                               NkColor4f color = {1,1,1,1}) noexcept;

        /// Polygone convexe texturé
        void DrawConvexPolygonTextured(const NkVec2* points, const NkVec2* uvs,
                                       uint32 count, NkTextureRef tex,
                                       NkColor4f tint = {1,1,1,1}) noexcept;

        // ── Cercles étendus ────────────────────────────────────────────────────

        /// Cercle texturé (la texture est projetée sur un disque)
        void DrawCircleTextured(NkVec2 center, float32 radius,
                                NkTextureRef tex,
                                NkColor4f tint     = {1,1,1,1},
                                uint32    segments = 32) noexcept;

        // ── Arc ───────────────────────────────────────────────────────────────

        void DrawArc(NkVec2 center, float32 radius,
                     float32 startAngleDeg, float32 endAngleDeg,
                     float32   thickness = 1.f,
                     NkColor4f color     = {1,1,1,1},
                     uint32    segments  = 32) noexcept;

        // ── Courbes de Bézier ──────────────────────────────────────────────────

        /// Bézier cubique (p0,p1,p2,p3)
        void DrawBezierCubic(NkVec2 p0, NkVec2 p1, NkVec2 p2, NkVec2 p3,
                             float32   thickness = 1.f,
                             NkColor4f color     = {1,1,1,1},
                             uint32    segments  = 20) noexcept;

        /// Bézier quadratique (p0,control,p2)
        void DrawBezierQuadratic(NkVec2 p0, NkVec2 control, NkVec2 p2,
                                 float32   thickness = 1.f,
                                 NkColor4f color     = {1,1,1,1},
                                 uint32    segments  = 16) noexcept;

        // Texte 2D (bitmap font via NKFont)
        void DrawText(NkFontFace* font,
                      const char* text,
                      NkVec2 pos,
                      float32 scale        = 1.f,
                      NkColor4f color      = {1,1,1,1},
                      NkTextureRef fontAtlasTex = {}) noexcept;

        // ── Statistiques ──────────────────────────────────────────────────────

        struct Stats {
            uint32 drawCalls = 0;
            uint32 quadCount = 0;
            uint32 textureSwaps = 0;
        };
        const Stats& GetStats()  const noexcept { return mStats; }
        void ResetStats() noexcept { mStats = {}; }

    private:
        // ── Initialisation des pipelines ──────────────────────────────────────
        bool InitPipelines(NkRenderPassHandle rp) noexcept;
        bool InitBuffers()   noexcept;
        bool InitDescSets()  noexcept;

        // ── Batch helpers ─────────────────────────────────────────────────────
        void FlushBatch() noexcept;
        void EnsureCapacity(uint32 extraQuads) noexcept;
        void PushQuad(const NkVertex2D* verts4, NkTextureHandle tex, NkSamplerHandle samp) noexcept;

        // ── Quad builder ──────────────────────────────────────────────────────
        void BuildQuad(NkVertex2D* out4,
                       NkVec2 pos, NkVec2 size, float32 rot, NkVec2 pivot,
                       NkUVRect uv, uint32 color) const noexcept;

        // ── Shader source ─────────────────────────────────────────────────────
        static const char* s_VertGLSL;
        static const char* s_FragGLSL;
        static const char* s_VertHLSL;
        static const char* s_FragHLSL;

        // ── État ──────────────────────────────────────────────────────────────
        NkIDevice*        mDevice    = nullptr;
        NkICommandBuffer* mCmd       = nullptr;

        NkRenderer2DDesc  mDesc;
        NkMat4            mVP;

        // Pipelines
        NkShader          mShader;
        NkPipeline        mPipelineOpaque;
        NkPipeline        mPipelineAlpha;

        // Buffers GPU
        NkBufferHandle    mVBO;            // vertex buffer dynamique
        NkBufferHandle    mIBO;            // index buffer statique (indices quads pré-calculés)

        // Batch CPU
        struct BatchEntry {
            NkTextureHandle tex;
            NkSamplerHandle samp;
            uint32          quadStart;
            uint32          quadCount;
        };

        NkVector<NkVertex2D>  mVertices;   // buffer CPU → gpu
        NkVector<BatchEntry>  mBatches;    // tri par texture
        NkTextureHandle       mCurrentTex;
        NkSamplerHandle       mCurrentSamp;
        uint32                mCurrentBatchStart = 0;
        uint32                mQuadCount    = 0;
        uint32                mMaxQuads     = 0;

        // Descriptor set (texture courante)
        NkDescSetHandle   mDescSetLayout;
        NkDescSetHandle   mDescSet;
        NkBufferHandle    mCameraUBO;      // view-projection matrix

        Stats             mStats;
        bool              mInBatch  = false;
        bool              mIsValid  = false;
    };

} // namespace nkentseu
