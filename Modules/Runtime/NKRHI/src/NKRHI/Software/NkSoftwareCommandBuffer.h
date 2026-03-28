#pragma once
// =============================================================================
// NkSoftwareCommandBuffer.h — Command buffer enregistrement + replay CPU
// =============================================================================
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKCore/NkTraits.h"
#include <cstring>

namespace nkentseu {

class NkSoftwareDevice;

// Chaque commande est un NkFunction<void(NkSoftwareDevice*)>
// enregistrée à l'appel de Begin()/End() et rejouée dans Execute()
class NkSoftwareCommandBuffer final : public NkICommandBuffer {
public:
    NkSoftwareCommandBuffer(NkSoftwareDevice* dev, NkCommandBufferType type);
    ~NkSoftwareCommandBuffer() override = default;

    bool Begin()  override { mCommands.Clear(); ResetRecordedState(); mRecording = true; return true; }
    void End()    override { mRecording = false; }
    void Reset()  override { mCommands.Clear(); ResetRecordedState(); mRecording = false; }
    bool IsValid()              const override { return true; }
    NkCommandBufferType GetType() const override { return mType; }

    // Replay toutes les commandes enregistrées
    void Execute(NkSoftwareDevice* dev);

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
    void Barrier          (const NkBufferBarrier*, uint32, const NkTextureBarrier*, uint32) override {}
    void GenerateMipmaps  (NkTextureHandle tex, NkFilter f) override;
    void BeginDebugGroup  (const char* name, float, float, float) override;
    void EndDebugGroup    () override;
    void InsertDebugLabel (const char* name) override;

private:
    using Cmd = NkFunction<void(NkSoftwareDevice*)>;
    void Push(Cmd&& c) { if (mRecording) mCommands.PushBack(traits::NkMove(c)); }
    void ResetRecordedState() {
        mCurrentFramebufferId = 0;
        mBoundPipelineId = 0;
        mBoundDescSetId = 0;
        mBoundIndexBufferId = 0;
        mBoundIndexOffset = 0;
        mBoundIndexUint32 = true;
        mScissorRect = {0, 0, 0, 0};
        mScissorEnabled = false;
        for (uint32 i = 0; i < 8; ++i) {
            mBoundVertexBufferIds[i] = 0;
            mBoundVertexOffsets[i] = 0;
        }
    }

    NkSoftwareDevice*      mDev;
    NkCommandBufferType mType;
    bool              mRecording = false;
    NkVector<Cmd>  mCommands;

    // Etat capture pendant l'enregistrement; snapshotte au moment des Draw/Dispatch.
    uint64 mCurrentFramebufferId = 0;
    uint64 mBoundPipelineId = 0;
    uint64 mBoundDescSetId = 0;
    uint64 mBoundVertexBufferIds[8]{};
    uint64 mBoundVertexOffsets[8]{};
    uint64 mBoundIndexBufferId = 0;
    uint64 mBoundIndexOffset = 0;
    bool   mBoundIndexUint32 = true;
    NkRect2D mScissorRect = {0, 0, 0, 0};
    bool mScissorEnabled = false;
};

} // namespace nkentseu
