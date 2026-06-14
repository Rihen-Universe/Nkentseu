#pragma once
// =============================================================================
// NkDirectX11CommandBuffer.h — Deferred Context DX11
// Utilise un ID3D11DeviceContext différé pour l'enregistrement,
// puis ExecuteCommandList sur le contexte immédiat lors du Submit.
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#ifdef NK_RHI_DX11_ENABLED
#include <d3d11_1.h>
#include <vector>
#include <functional>

namespace nkentseu {

    class NkDirectX11Device;

    class NkDirectX11CommandBuffer final : public NkICommandBuffer {
        public:
            NkDirectX11CommandBuffer(NkDirectX11Device* dev, NkCommandBufferType type);
            ~NkDirectX11CommandBuffer() override;

            bool Begin()  override;
            void End()    override;
            void Reset()  override;
            bool IsValid()              const override { return mDeferred != nullptr; }
            NkCommandBufferType GetType() const override { return mType; }
            void Execute(NkDirectX11Device* dev);

            bool BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb, const NkRect2D& area) override;
            void EndRenderPass() override {}
            void SetViewport (const NkViewport& vp) override;
            void SetViewports(const NkViewport* vps, uint32 n) override;
            void SetScissor  (const NkRect2D& r) override;
            void SetScissors (const NkRect2D* r, uint32 n) override;
            // SetClearColor/Depth = un clear est DEMANDÉ pour la prochaine passe (loadOp
            // CLEAR). BeginRenderPass ne clear QUE si demandé, puis remet le flag à false.
            // Avant : BeginRenderPass clearait INCONDITIONNELLEMENT → une passe LOAD
            // (ex. Overlay2D sur le swapchain) effaçait l'image déjà composée → écran noir.
            void SetClearColor(float r, float g, float b, float a = 1.f) override {
                mClearColor[0]=r; mClearColor[1]=g; mClearColor[2]=b; mClearColor[3]=a;
                mPendingClearColor=true;
            }
            void SetClearDepth(float depth = 1.f, uint32 stencil = 0) override {
                mClearDepth=depth; mClearStencil=stencil;
                mPendingClearDepth=true;
            }
            void BindGraphicsPipeline(NkPipelineHandle p) override;
            void BindComputePipeline (NkPipelineHandle p) override;
            void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
            void PushConstants(NkShaderStage stages, uint32 offset, uint32 size, const void* data) override;
            void UpdateBuffer(NkBufferHandle, uint64, uint64, const void*) override {} // TODO: ID3D11DeviceContext::UpdateSubresource
            void BindVertexBuffer (uint32 b, NkBufferHandle buf, uint64 off) override;
            void BindVertexBuffers(uint32 first, const NkBufferHandle* bufs, const uint64* offs, uint32 n) override;
            void BindIndexBuffer  (NkBufferHandle buf, NkIndexFormat fmt, uint64 off) override;
            void Draw             (uint32 v, uint32 i, uint32 fv, uint32 fi) override;
            void DrawIndexed      (uint32 idx, uint32 inst, uint32 fi, int32 vo, uint32 fInst) override;
            void DrawIndirect     (NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;
            void DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;
            void Dispatch         (uint32 gx, uint32 gy, uint32 gz) override;
            void DispatchIndirect (NkBufferHandle buf, uint64 off) override;
            void CopyBuffer       (NkBufferHandle s, NkBufferHandle d, const NkBufferCopyRegion& r) override;
            void CopyBufferToTexture(NkBufferHandle, NkTextureHandle, const NkBufferTextureCopyRegion&) override {}
            void CopyTextureToBuffer(NkTextureHandle, NkBufferHandle, const NkBufferTextureCopyRegion&) override {}
            void CopyTexture      (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r) override;
            void BlitTexture      (NkTextureHandle, NkTextureHandle, const NkTextureCopyRegion&, NkFilter) override {}
            void Barrier          (const NkBufferBarrier*, uint32, const NkTextureBarrier*, uint32) override {} // DX11 implicite
            void GenerateMipmaps  (NkTextureHandle, NkFilter) override;
            void BeginDebugGroup  (const char* name, float, float, float) override;
            void EndDebugGroup    () override;
            void InsertDebugLabel (const char* name) override;

        private:
            NkDirectX11Device*       mDev  = nullptr;
            ID3D11DeviceContext1* mDeferred = nullptr;
            ID3D11CommandList*    mCmdList  = nullptr;
            NkCommandBufferType   mType;
            UINT mVertexStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
            float  mClearColor[4] = {0.f, 0.f, 0.f, 1.f};
            float  mClearDepth    = 1.f;
            uint32 mClearStencil  = 0;
            bool   mPendingClearColor = false; // un clear couleur a été demandé (loadOp CLEAR)
            bool   mPendingClearDepth = false;
            uint8  mPushCpu[256]  = {}; // shadow CPU des push constants (uploadé en b13)
    };

} // namespace nkentseu
#endif
