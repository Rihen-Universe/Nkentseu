#pragma once
// =============================================================================
// NkICommandBuffer.h
// Interface abstraite du command buffer RHI.
//
// Le command buffer est l'objet central du RHI :
//   - Enregistre toutes les commandes GPU (draw, dispatch, copy, barriers)
//   - Thread-safe à l'enregistrement : plusieurs threads peuvent enregistrer
//     des command buffers différents en parallèle
//   - Soumis au device via NkIDevice::Submit()
//
// Cycle de vie :
//   Begin() → [commandes] → End() → Submit() → [WaitFence si besoin]
//   Reset() → Begin() → ...
//
// Types :
//   Graphics   — draw calls, render passes, copy
//   Compute    — dispatch compute, copy, barriers
//   Transfer   — copy uniquement (DMA engine dédié sur certains GPU)
// =============================================================================
#include "NKRenderer/RHI/Core/NkRhiTypes.h"
#include "NKRenderer/RHI/Core/NkRhiDescs.h"

namespace nkentseu {

enum class NkCommandBufferType : uint32 {
    NK_GRAPHICS,   // peut aussi faire compute et copy
    NK_COMPUTE,    // compute + copy uniquement (queue dédiée)
    NK_TRANSFER,   // copy uniquement (DMA engine)
};

class NkICommandBuffer {
public:
    virtual ~NkICommandBuffer() = default;

    // ── Cycle de vie ─────────────────────────────────────────────────────────
    virtual void Begin()                     = 0;
    virtual void End()                       = 0;
    virtual void Reset()                     = 0;
    virtual bool IsValid()             const = 0;
    virtual NkCommandBufferType GetType() const = 0;

    // =========================================================================
    // Render Pass
    // =========================================================================
    virtual void BeginRenderPass(NkRenderPassHandle rp,
                                  NkFramebufferHandle fb,
                                  const NkRect2D& area)         = 0;
    virtual void EndRenderPass()                                 = 0;

    // ── Sous-passes (Vulkan subpasses / Metal render encoder chains) ──
    virtual void NextSubpass() {}  // optionnel — no-op sur DX/GL

    // =========================================================================
    // Viewport & Scissor
    // =========================================================================
    virtual void SetViewport(const NkViewport& vp)               = 0;
    virtual void SetViewports(const NkViewport* vps, uint32 n)   = 0;
    virtual void SetScissor(const NkRect2D& rect)                = 0;
    virtual void SetScissors(const NkRect2D* rects, uint32 n)    = 0;

    // =========================================================================
    // Pipeline & Descriptors
    // =========================================================================
    virtual void BindGraphicsPipeline(NkPipelineHandle pipeline) = 0;
    virtual void BindComputePipeline (NkPipelineHandle pipeline) = 0;

    // Descriptor sets — binding uniforms, textures, storage buffers
    // set     : numéro de set (0=par frame, 1=par pass, 2=par material, 3=par objet)
    // offsets : pour les descriptors dynamiques (UniformBufferDynamic)
    virtual void BindDescriptorSet(NkDescSetHandle set, uint32 setIndex,
                                    uint32* dynamicOffsets=nullptr,
                                    uint32  dynamicOffsetCount=0)    = 0;

    // Push constants — données petites passées directement dans le command buffer
    // Plus rapide que les uniform buffers pour les données qui changent par draw call
    virtual void PushConstants(NkShaderStage stages,
                                uint32 offset,
                                uint32 size,
                                const void* data)                = 0;

    // =========================================================================
    // Vertex & Index Buffers
    // =========================================================================
    virtual void BindVertexBuffer(uint32 binding,
                                   NkBufferHandle buf,
                                   uint64 offset=0)              = 0;
    virtual void BindVertexBuffers(uint32 firstBinding,
                                    const NkBufferHandle* bufs,
                                    const uint64* offsets,
                                    uint32 count)                = 0;
    virtual void BindIndexBuffer(NkBufferHandle buf,
                                  NkIndexFormat fmt,
                                  uint64 offset=0)               = 0;

    // =========================================================================
    // Draw calls
    // =========================================================================
    virtual void Draw(uint32 vertexCount,
                      uint32 instanceCount=1,
                      uint32 firstVertex=0,
                      uint32 firstInstance=0)                    = 0;

    virtual void DrawIndexed(uint32 indexCount,
                              uint32 instanceCount=1,
                              uint32 firstIndex=0,
                              int32  vertexOffset=0,
                              uint32 firstInstance=0)            = 0;

    // Indirect — args dans un buffer GPU (occlusion culling, etc.)
    virtual void DrawIndirect(NkBufferHandle argsBuffer,
                               uint64 offset,
                               uint32 drawCount,
                               uint32 stride=sizeof(NkDrawIndirectArgs)) = 0;

    virtual void DrawIndexedIndirect(NkBufferHandle argsBuffer,
                                      uint64 offset,
                                      uint32 drawCount,
                                      uint32 stride=sizeof(NkDrawIndexedIndirectArgs)) = 0;

    // Multi-draw indirect avec count GPU (DX12/Vulkan/Metal)
    virtual void DrawIndirectCount(NkBufferHandle argsBuffer, uint64 argsOffset,
                                    NkBufferHandle countBuffer, uint64 countOffset,
                                    uint32 maxDrawCount) {}

    // =========================================================================
    // Compute dispatch
    // =========================================================================
    virtual void Dispatch(uint32 groupsX, uint32 groupsY=1, uint32 groupsZ=1) = 0;

    virtual void DispatchIndirect(NkBufferHandle argsBuffer, uint64 offset) = 0;

    // =========================================================================
    // Copies
    // =========================================================================
    virtual void CopyBuffer(NkBufferHandle src, NkBufferHandle dst,
                             const NkBufferCopyRegion& region)   = 0;

    virtual void CopyBufferToTexture(NkBufferHandle  src,
                                      NkTextureHandle dst,
                                      const NkBufferTextureCopyRegion& region) = 0;

    virtual void CopyTextureToBuffer(NkTextureHandle src,
                                      NkBufferHandle  dst,
                                      const NkBufferTextureCopyRegion& region) = 0;

    virtual void CopyTexture(NkTextureHandle src, NkTextureHandle dst,
                              const NkTextureCopyRegion& region)  = 0;

    // Blit avec filtrage (resize, mip generation)
    virtual void BlitTexture(NkTextureHandle src, NkTextureHandle dst,
                              const NkTextureCopyRegion& region,
                              NkFilter filter=NkFilter::NK_LINEAR)   = 0;

    // =========================================================================
    // Resource barriers (transitions d'état)
    // =========================================================================
    virtual void Barrier(const NkBufferBarrier*  bufBarriers,  uint32 bufCount,
                          const NkTextureBarrier* texBarriers,  uint32 texCount) = 0;

    // Shortcut : une seule texture
    void TextureBarrier(NkTextureHandle tex,
                        NkResourceState before, NkResourceState after,
                        NkPipelineStage src=NkPipelineStage::NK_ALL_COMMANDS,
                        NkPipelineStage dst=NkPipelineStage::NK_ALL_COMMANDS) {
        NkTextureBarrier b{tex,before,after,src,dst};
        Barrier(nullptr,0,&b,1);
    }

    // Shortcut : une seule barrière UAV (après compute write)
    void UAVBarrier(NkBufferHandle buf) {
        NkBufferBarrier b{buf,NkResourceState::NK_UNORDERED_ACCESS,
                          NkResourceState::NK_UNORDERED_ACCESS};
        Barrier(&b,1,nullptr,0);
    }

    // =========================================================================
    // Génération de mip-maps GPU
    // =========================================================================
    virtual void GenerateMipmaps(NkTextureHandle texture,
                                  NkFilter filter=NkFilter::NK_LINEAR) = 0;

    // =========================================================================
    // Debug markers (RenderDoc, PIX, Xcode GPU Frame Capture)
    // =========================================================================
    virtual void BeginDebugGroup(const char* name, float r=1,float g=1,float b=1) {}
    virtual void EndDebugGroup()   {}
    virtual void InsertDebugLabel(const char* name) {}

    // =========================================================================
    // Clear explicite (hors render pass — pour les textures storage)
    // =========================================================================
    virtual void ClearTexture(NkTextureHandle texture,
                               const NkClearValue& value,
                               uint32 baseMip=0, uint32 mipCount=UINT32_MAX,
                               uint32 baseLayer=0, uint32 layerCount=UINT32_MAX) {}

    virtual void ClearBuffer(NkBufferHandle buffer,
                              uint32 value=0,
                              uint64 offset=0, uint64 size=UINT64_MAX) {}

    // =========================================================================
    // Timestamp queries (GPU timing)
    // =========================================================================
    virtual void WriteTimestamp(uint32 queryIndex) {}
    virtual void ResetQueryPool(uint32 firstQuery, uint32 count) {}
};

} // namespace nkentseu
