#pragma once
// =============================================================================
// NkCommandBuffer_SW.h — Command buffer enregistrement + replay CPU
// =============================================================================
#include "../Commands/NkICommandBuffer.h"
#include <vector>
#include <functional>
#include <cstring>

namespace nkentseu {

class NkDevice_SW;

// Chaque commande est un std::function<void(NkDevice_SW*)>
// enregistrée à l'appel de Begin()/End() et rejouée dans Execute()
class NkCommandBuffer_SW final : public NkICommandBuffer {
public:
    NkCommandBuffer_SW(NkDevice_SW* dev, NkCommandBufferType type);
    ~NkCommandBuffer_SW() override = default;

    void Begin()  override { mCommands.clear(); mRecording = true; }
    void End()    override { mRecording = false; }
    void Reset()  override { mCommands.clear(); mRecording = false; }
    bool IsValid()              const override { return true; }
    NkCommandBufferType GetType() const override { return mType; }

    // Replay toutes les commandes enregistrées
    void Execute(NkDevice_SW* dev);

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
    void Barrier          (const NkBufferBarrier*, uint32, const NkTextureBarrier*, uint32) override {}
    void GenerateMipmaps  (NkTextureHandle tex, NkFilter f) override;
    void BeginDebugGroup  (const char* name, float, float, float) override;
    void EndDebugGroup    () override;
    void InsertDebugLabel (const char* name) override;

private:
    using Cmd = std::function<void(NkDevice_SW*)>;
    void Push(Cmd&& c) { if (mRecording) mCommands.push_back(std::move(c)); }

    NkDevice_SW*      mDev;
    NkCommandBufferType mType;
    bool              mRecording = false;
    std::vector<Cmd>  mCommands;
};

} // namespace nkentseu
