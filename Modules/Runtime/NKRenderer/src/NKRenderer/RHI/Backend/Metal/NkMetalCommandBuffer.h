#pragma once
// =============================================================================
// NkCommandBuffer_Metal.h — MTLCommandBuffer + encoders Metal
// =============================================================================
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"
#ifdef NK_RHI_METAL_ENABLED
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace nkentseu {

class NkDevice_Metal;

class NkCommandBuffer_Metal final : public NkICommandBuffer {
public:
    NkCommandBuffer_Metal(NkDevice_Metal* dev, NkCommandBufferType type);
    ~NkCommandBuffer_Metal() override;

    void Begin()  override;
    void End()    override;
    void Reset()  override;
    bool IsValid()              const override { return mCmdBuf != nullptr; }
    NkCommandBufferType GetType() const override { return mType; }

    void CommitAndWait();
    void CommitAndPresent(void* drawable);

    void BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb, const NkRect2D& area) override;
    void EndRenderPass() override;
    void SetViewport (const NkViewport& vp) override;
    void SetViewports(const NkViewport* vps, uint32 n) override;
    void SetScissor  (const NkRect2D& r) override;
    void SetScissors (const NkRect2D* r, uint32 n) override;
    void BindGraphicsPipeline(NkPipelineHandle p) override;
    void BindComputePipeline (NkPipelineHandle p) override;
    void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
    void PushConstants(NkShaderStage stages, uint32 offset, uint32 size, const void* data) override;
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
    void CopyBufferToTexture(NkBufferHandle, NkTextureHandle, const NkBufferTextureCopyRegion&) override;
    void CopyTextureToBuffer(NkTextureHandle, NkBufferHandle, const NkBufferTextureCopyRegion&) override;
    void CopyTexture      (NkTextureHandle s, NkTextureHandle d, const NkTextureCopyRegion& r) override;
    void BlitTexture      (NkTextureHandle, NkTextureHandle, const NkTextureCopyRegion&, NkFilter) override;
    void Barrier          (const NkBufferBarrier*, uint32, const NkTextureBarrier*, uint32) override {} // Metal gère implicitement
    void GenerateMipmaps  (NkTextureHandle tex, NkFilter f) override;
    void BeginDebugGroup  (const char* name, float r, float g, float b) override;
    void EndDebugGroup    () override;
    void InsertDebugLabel (const char* name) override;

private:
    void EndCurrentEncoder();

    NkDevice_Metal*   mDev  = nullptr;
    NkCommandBufferType mType;
    void* mCmdBuf             = nullptr; // id<MTLCommandBuffer>
    void* mRenderEncoder      = nullptr; // id<MTLRenderCommandEncoder>
    void* mComputeEncoder     = nullptr; // id<MTLComputeCommandEncoder>
    void* mBlitEncoder        = nullptr; // id<MTLBlitCommandEncoder>

    // État courant pour les draw calls
    void* mCurrentIndexBuffer = nullptr; // id<MTLBuffer>
    uint64 mIndexOffset       = 0;
    bool   mIndexUint32       = true;
    MTLPrimitiveType mPrimitive = MTLPrimitiveTypeTriangle;
};

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
