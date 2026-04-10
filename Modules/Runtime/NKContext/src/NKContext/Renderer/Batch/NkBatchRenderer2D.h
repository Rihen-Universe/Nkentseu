#pragma once
// =============================================================================
// NkBatchRenderer2D.h — Shared vertex batching system for all 2D backends
//
// Architecture:
//   • Accumulates NkVertex2D + indices in CPU-side arrays.
//   • Groups draws by (texture, blendMode) to minimize GPU state changes.
//   • When the batch is full or a state change is forced, calls FlushBatch()
//     which is implemented by each backend via a virtual method.
//
// Each backend (OpenGL, Vulkan, DX11, DX12, Software) extends this class and
// implements UploadBatch() + SubmitBatch() + BeginBatch() + EndBatch().
// =============================================================================
#include "NKContext/Renderer/Core/NkIRenderer2D.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        class NkTexture;

        // ── A single draw group (one GPU draw call) ──────────────────────────────
        struct NkBatchGroup {
            const NkTexture* texture    = nullptr;
            NkBlendMode      blendMode  = NkBlendMode::NK_ALPHA;
            uint32           indexStart = 0;
            uint32           indexCount = 0;
        };

        // =========================================================================
        class NkBatchRenderer2D : public NkIRenderer2D {
            public:
                // Maximum vertices / indices before an automatic flush
                static constexpr uint32 kMaxVertices = 65536;
                static constexpr uint32 kMaxIndices  = kMaxVertices * 6 / 4; // ~98304

                NkBatchRenderer2D();
                ~NkBatchRenderer2D() override = default;

                // ── NkIRenderer2D ─────────────────────────────────────────────────────
                bool Begin() override;
                void End()   override;
                void Flush() override;

                void SetView(const NkView2D& view) override;
                NkView2D GetView()        const override { return mCurrentView; }
                NkView2D GetDefaultView() const override { return mDefaultView; }

                void SetViewport(const NkRect2i& vp) override { mViewport = vp; }
                NkRect2i GetViewport()               const override { return mViewport; }

                void SetBlendMode(NkBlendMode mode) override;
                NkBlendMode GetBlendMode()          const override { return mBlendMode; }

                // Drawables
                void Draw(const NkSprite& sprite) override;
                void Draw(const NkText&   text)   override;

                // Primitives
                void DrawPoint        (NkVec2f pos, const NkColor2D& col, float32 size)              override;
                void DrawLine         (NkVec2f a, NkVec2f b, const NkColor2D& col, float32 thick)   override;
                void DrawRect         (NkRect2f r, const NkColor2D& col, float32 outline, const NkColor2D& oc) override;
                void DrawFilledRect   (NkRect2f r, const NkColor2D& col)                             override;
                void DrawCircle       (NkVec2f c, float32 radius, const NkColor2D& col, uint32 segs, float32 outline, const NkColor2D& oc) override;
                void DrawFilledCircle (NkVec2f c, float32 radius, const NkColor2D& col, uint32 segs) override;
                void DrawTriangle     (NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D& col, float32 outline, const NkColor2D& oc) override;
                void DrawFilledTriangle(NkVec2f a, NkVec2f b, NkVec2f c, const NkColor2D& col)      override;
                void DrawVertices     (const NkVertex2D* verts, uint32 vCount, const uint32* idx, uint32 iCount, const NkTexture* tex) override;

                // Stats
                NkRenderStats2D GetStats() const override { return mStats; }
                void ResetStats() override { mStats = {}; }

                // Coordinate conversion
                NkVec2f MapPixelToCoords(NkVec2i pixel) const override;
                NkVec2i MapCoordsToPixel(NkVec2f point) const override;

            protected:
                // ── Backend-specific hooks ────────────────────────────────────────────
                // Called once at the start of a frame to set up GPU state.
                virtual void BeginBackend() {}
                // Called after all batches are submitted. End render pass, present, etc.
                virtual void EndBackend()   {}
                // Upload mVertices / mIndices to a GPU buffer and issue a draw call.
                // 'groups' lists the draw groups (one per texture/blend change).
                virtual void SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                        const NkVertex2D* vertices, uint32 vertexCount,
                                        const uint32*     indices,  uint32 indexCount) = 0;
                // Builds and uploads the orthographic projection from the current view.
                virtual void UploadProjection(const float32 proj[16]) = 0;

                // ── Batch state ───────────────────────────────────────────────────────
                NkVector<NkVertex2D>  mVertices;
                NkVector<uint32>      mIndices;
                NkVector<NkBatchGroup> mGroups;

                NkView2D        mCurrentView;
                NkView2D        mDefaultView;
                NkRect2i        mViewport;
                NkBlendMode     mBlendMode = NkBlendMode::NK_ALPHA;
                NkRenderStats2D mStats;
                bool            mInFrame   = false;

            private:
                // Ensure the current texture/blend group is open; close old one if changed.
                void EnsureGroup(const NkTexture* tex, NkBlendMode blend);
                void PushQuad(NkVec2f tl, NkVec2f tr, NkVec2f br, NkVec2f bl,
                            NkVec2f uvTL, NkVec2f uvBR,
                            const NkColor2D& color,
                            const NkTexture* texture);

                const NkTexture* mCurrentTexture = nullptr;

                // Scratch for circle/polygon generation
                NkVector<NkVec2f> mScratchPoly;
        };

    } // namespace renderer
} // namespace nkentseu