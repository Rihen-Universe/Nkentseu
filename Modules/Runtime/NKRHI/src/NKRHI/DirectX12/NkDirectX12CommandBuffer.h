#pragma once
// =============================================================================
// NkDirectX12CommandBuffer.h — ID3D12GraphicsCommandList wrapper
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#ifdef NK_RHI_DX12_ENABLED
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace nkentseu {

class NkDirectX12Device;

class NkDirectX12CommandBuffer final : public NkICommandBuffer {
public:
    NkDirectX12CommandBuffer(NkDirectX12Device* dev, NkCommandBufferType type);
    ~NkDirectX12CommandBuffer() override;

    bool Begin()  override;
    void End()    override;
    void Reset()  override;
    bool IsValid()              const override { return mCmdList != nullptr; }
    NkCommandBufferType GetType() const override { return mType; }
    ID3D12GraphicsCommandList* GetCmdList() const { return mCmdList.Get(); }

    bool BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb, const NkRect2D& area) override;
    void EndRenderPass() override;

    void SetViewport (const NkViewport& vp) override;
    void SetViewports(const NkViewport* vps, uint32 n) override;
    void SetScissor  (const NkRect2D& r) override;
    void SetScissors (const NkRect2D* r, uint32 n) override;

    void BindGraphicsPipeline(NkPipelineHandle p) override;
    void BindComputePipeline (NkPipelineHandle p) override;
    void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
    void PushConstants(NkShaderStage stages, uint32 offset, uint32 size, const void* data) override;

    void BindVertexBuffer (uint32 binding, NkBufferHandle buf, uint64 offset) override;
    void BindVertexBuffers(uint32 first, const NkBufferHandle* bufs, const uint64* offs, uint32 n) override;
    void BindIndexBuffer  (NkBufferHandle buf, NkIndexFormat fmt, uint64 offset) override;

    void Draw             (uint32 v, uint32 i, uint32 fv, uint32 fi) override;
    void DrawIndexed      (uint32 idx, uint32 inst, uint32 fi, int32 vo, uint32 fInst) override;
    void DrawIndirect     (NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;
    void DrawIndexedIndirect(NkBufferHandle buf, uint64 off, uint32 cnt, uint32 stride) override;

    void Dispatch         (uint32 gx, uint32 gy, uint32 gz) override;
    void DispatchIndirect (NkBufferHandle buf, uint64 off) override;

    void CopyBuffer         (NkBufferHandle s, NkBufferHandle d, const NkBufferCopyRegion& r) override;
    void CopyBufferToTexture(NkBufferHandle s, NkTextureHandle d, const NkBufferTextureCopyRegion& r) override;
    void CopyTextureToBuffer(NkTextureHandle s, NkBufferHandle d, const NkBufferTextureCopyRegion& r) override;
    void CopyTexture        (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r) override;
    void BlitTexture        (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r, NkFilter f) override;

    void Barrier(const NkBufferBarrier* bb, uint32 bc, const NkTextureBarrier* tb, uint32 tc) override;
    void GenerateMipmaps(NkTextureHandle, NkFilter) override {}

    void BeginDebugGroup (const char* name, float r, float g, float b) override;
    void EndDebugGroup   () override;
    void InsertDebugLabel(const char* name) override;

private:
    NkDirectX12Device*                     mDev = nullptr;
    ComPtr<ID3D12GraphicsCommandList>   mCmdList;
    ComPtr<ID3D12CommandAllocator>      mAllocator; // propre (non partagé avec frames)
    NkCommandBufferType                 mType;
    bool                               mIsCompute = false;
    bool                               mRecording = false;
    uint64                              mActiveColorTexIds[8]{};
    uint32                              mActiveColorCount = 0;
    uint64                              mActiveDepthTexId = 0;
    uint32                              mVertexStrides[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]{};
};

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
