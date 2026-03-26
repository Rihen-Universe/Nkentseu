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
            void BindGraphicsPipeline(NkPipelineHandle p) override;
            void BindComputePipeline (NkPipelineHandle p) override;
            void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
            void PushConstants(NkShaderStage, uint32, uint32, const void*) override {} // émulé via CB
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
    };

} // namespace nkentseu
#endif
