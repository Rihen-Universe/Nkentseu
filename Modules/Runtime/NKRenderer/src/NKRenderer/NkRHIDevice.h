#pragma once
// =============================================================================
// NkRHIDevice.h — Interface abstraite du device RHI
//
// NkRHIDevice est l'interface principale que le moteur de rendu utilise.
// Il crée et détruit les ressources (buffers, textures, pipelines…)
// et fournit un mécanisme de command buffers pour enregistrer les draws.
//
// L'implémentation concrète (NkVulkanDevice, NkOpenGLDevice…)
// traduit ces appels en API natifs.
// =============================================================================

#include "NkRHI.h"
#include "NKWindow/Core/NkSurface.h"
#include "NKWindow/Graphics/NkGraphicsContextConfig.h"
#include <functional>
#include <memory>

namespace nkentseu { namespace rhi {

// ---------------------------------------------------------------------------
// NkRHICommandBuffer — séquence de commandes enregistrables
// ---------------------------------------------------------------------------
class NkRHICommandBuffer {
public:
    virtual ~NkRHICommandBuffer() = default;

    // ── Cycle ────────────────────────────────────────────────────────────────
    virtual void Begin() = 0;
    virtual void End()   = 0;

    // ── Render Pass ──────────────────────────────────────────────────────────
    virtual void BeginRenderPass(NkRenderPassHandle  rp,
                                 NkFramebufferHandle fb,
                                 const NkClearValue* clears,
                                 NkU32               clearCount) = 0;
    virtual void EndRenderPass()   = 0;
    virtual void NextSubpass()     = 0;

    // ── État du pipeline ─────────────────────────────────────────────────────
    virtual void BindPipeline  (NkPipelineHandle pipeline) = 0;
    virtual void SetViewport   (const NkViewport& vp)      = 0;
    virtual void SetScissor    (const NkScissor&  sc)      = 0;

    // ── Ressources ───────────────────────────────────────────────────────────
    virtual void BindVertexBuffer (NkU32 binding, NkBufferHandle buf, NkU64 offset = 0) = 0;
    virtual void BindIndexBuffer  (NkBufferHandle buf, NkIndexType type, NkU64 offset = 0) = 0;
    virtual void BindDescriptorSet(NkU32 setIndex, NkDescriptorSetHandle ds,
                                   NkPipelineHandle pipeline) = 0;
    virtual void PushConstants    (NkPipelineHandle pipeline,
                                   NkShaderStage    stages,
                                   NkU32            offset,
                                   NkU32            size,
                                   const void*      data)  = 0;

    // ── Draw ─────────────────────────────────────────────────────────────────
    virtual void Draw        (const NkDrawCall&        cmd) = 0;
    virtual void DrawIndexed (const NkDrawIndexedCall& cmd) = 0;
    virtual void DrawIndirect(NkBufferHandle buf, NkU64 offset, NkU32 count) = 0;

    // ── Compute ──────────────────────────────────────────────────────────────
    virtual void Dispatch(NkU32 x, NkU32 y, NkU32 z) = 0;

    // ── Transferts / Synchronisation ─────────────────────────────────────────
    virtual void CopyBuffer (NkBufferHandle  src, NkBufferHandle  dst,
                              NkU64 srcOff, NkU64 dstOff, NkU64 size) = 0;
    virtual void CopyTexture(NkTextureHandle src, NkTextureHandle dst) = 0;
    virtual void UploadBuffer(NkBufferHandle  dst, const void* data,
                               NkU64 offset, NkU64 size) = 0;
    virtual void UploadTexture(NkTextureHandle dst, const void* data,
                                NkU32 mip, NkU32 layer,
                                NkU32 x, NkU32 y, NkU32 w, NkU32 h) = 0;

    // ── Pipeline barriers (abstraction simplifiée) ───────────────────────────
    virtual void TextureBarrier(NkTextureHandle tex,
                                 bool fromWrite, bool toRead) = 0;
    virtual void BufferBarrier (NkBufferHandle  buf,
                                 bool fromWrite, bool toRead) = 0;
};

using NkRHICommandBufferPtr = std::unique_ptr<NkRHICommandBuffer>;

// ---------------------------------------------------------------------------
// NkRHIDevice — interface principale
// ---------------------------------------------------------------------------
class NkRHIDevice {
public:
    virtual ~NkRHIDevice() = default;

    // ── Initialisation ───────────────────────────────────────────────────────
    virtual bool Init   (const NkSurfaceDesc& surface,
                         const NkGraphicsContextConfig& config) = 0;
    virtual void Shutdown() = 0;

    // ── Frame ────────────────────────────────────────────────────────────────
    /// Acquiert l'image suivante. Retourne false si recreate nécessaire.
    virtual bool BeginFrame() = 0;
    /// Soumet le command buffer principal et présente.
    virtual void EndFrame  (NkRHICommandBuffer& cmd) = 0;

    /// Reconstruit la swapchain après un resize.
    virtual bool Recreate(NkU32 width, NkU32 height) = 0;

    // ── Ressources ───────────────────────────────────────────────────────────

    virtual NkBufferHandle          CreateBuffer         (const NkBufferDesc&          desc) = 0;
    virtual NkTextureHandle         CreateTexture        (const NkTextureDesc&         desc) = 0;
    virtual NkSamplerHandle         CreateSampler        (const NkSamplerDesc&         desc) = 0;
    virtual NkShaderHandle          CreateShader         (const NkShaderDesc&          desc) = 0;
    virtual NkDescriptorLayoutHandle CreateDescriptorLayout(const NkDescriptorLayoutDesc& desc) = 0;
    virtual NkDescriptorSetHandle   AllocDescriptorSet   (NkDescriptorLayoutHandle layout) = 0;
    virtual NkRenderPassHandle      CreateRenderPass     (const NkRenderPassDesc&     desc) = 0;
    virtual NkFramebufferHandle     CreateFramebuffer    (const NkFramebufferDesc&    desc) = 0;
    virtual NkPipelineHandle        CreatePipeline       (const NkPipelineDesc&       desc) = 0;

    virtual void DestroyBuffer         (NkBufferHandle)          = 0;
    virtual void DestroyTexture        (NkTextureHandle)         = 0;
    virtual void DestroySampler        (NkSamplerHandle)         = 0;
    virtual void DestroyShader         (NkShaderHandle)          = 0;
    virtual void DestroyDescriptorLayout(NkDescriptorLayoutHandle) = 0;
    virtual void FreeDescriptorSet     (NkDescriptorSetHandle)   = 0;
    virtual void DestroyRenderPass     (NkRenderPassHandle)      = 0;
    virtual void DestroyFramebuffer    (NkFramebufferHandle)     = 0;
    virtual void DestroyPipeline       (NkPipelineHandle)        = 0;

    // ── Mise à jour des descripteurs ─────────────────────────────────────────
    virtual void UpdateDescriptorSet(NkDescriptorSetHandle ds,
                                     const NkDescriptorWrite* writes,
                                     NkU32 writeCount) = 0;

    // ── Mappage CPU/GPU ──────────────────────────────────────────────────────
    virtual void* MapBuffer  (NkBufferHandle buf) = 0;
    virtual void  UnmapBuffer(NkBufferHandle buf) = 0;

    // ── Command buffers ──────────────────────────────────────────────────────
    /// Crée un command buffer secondaire (pour le multithreading)
    virtual NkRHICommandBufferPtr CreateCommandBuffer(bool secondary = false) = 0;

    /// Command buffer principal de la frame courante (prêt entre BeginFrame/EndFrame)
    virtual NkRHICommandBuffer& GetCurrentCommandBuffer() = 0;

    // ── Swapchain ────────────────────────────────────────────────────────────
    virtual NkTextureHandle  GetSwapchainTexture(NkU32 index) = 0;
    virtual NkU32            GetSwapchainImageCount() const    = 0;
    virtual NkU32            GetCurrentSwapchainIndex() const  = 0;
    virtual NkRenderPassHandle GetSwapchainRenderPass()        = 0;
    virtual NkFramebufferHandle GetCurrentFramebuffer()        = 0;
    virtual NkU32            GetWidth()  const = 0;
    virtual NkU32            GetHeight() const = 0;

    // ── API ──────────────────────────────────────────────────────────────────
    virtual NkGraphicsApi GetApi() const = 0;

    // ── Attente GPU ──────────────────────────────────────────────────────────
    virtual void WaitIdle() = 0;
};

using NkRHIDevicePtr = std::unique_ptr<NkRHIDevice>;

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
class NkRHIDeviceFactory {
public:
    static NkRHIDevicePtr Create(const NkSurfaceDesc&           surface,
                                  const NkGraphicsContextConfig& config);
};

}} // namespace nkentseu::rhi