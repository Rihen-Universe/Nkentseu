#pragma once
// =============================================================================
// NkSoftwareRenderer2D.h — CPU software renderer backend (SIMD optimisé)
// =============================================================================
#include "NKContext/Renderer/Batch/NkBatchRenderer2D.h"
#include "NKContext/Graphics/Software/NkSoftwareContext.h"

namespace nkentseu {
    namespace renderer {

        class NkSoftwareRenderer2D final : public NkBatchRenderer2D {
        public:
            NkSoftwareRenderer2D()  = default;
            ~NkSoftwareRenderer2D() override { if (IsValid()) Shutdown(); }

            bool Initialize(NkIGraphicsContext* ctx) override;
            void Shutdown()                          override;
            bool IsValid()                   const   override { return mIsValid; }
            void Clear(const NkColor2D& col)         override;

        protected:
            void BeginBackend() override;
            void EndBackend()   override;

            void SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                               const NkVertex2D* verts, uint32 vCount,
                               const uint32*     idx,   uint32 iCount) override;

            void UploadProjection(const float32[16]) override {}
            void Draw(const NkSprite& sprite) override;

        private:
            NkIGraphicsContext* mCtx     = nullptr;
            NkSoftwareContext*  mSWCtx   = nullptr;
            bool                mIsValid = false;

            void BlitTexture(NkSoftwareFramebuffer& fb,
                             const NkTexture* tex,
                             const NkRect2i& srcRect,
                             int32 dstX, int32 dstY,
                             int32 dstW, int32 dstH,
                             const NkColor2D& tint);

            // Scan-line rasterizer SIMD + BGRA-direct
            void RasterizeTriangle(NkSoftwareFramebuffer& fb,
                                   const NkVertex2D& v0,
                                   const NkVertex2D& v1,
                                   const NkVertex2D& v2,
                                   const NkTexture*  tex,
                                   NkBlendMode       blendMode = NkBlendMode::NK_ALPHA);
        };

    } // namespace renderer
} // namespace nkentseu