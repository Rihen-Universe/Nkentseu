#pragma once
// =============================================================================
// NkIDevice.h
// Interface abstraite du device RHI — le coeur de la couche 1.
//
// Le NkIDevice est LA porte d'entrée de toutes les opérations GPU :
//   - Création/destruction de toutes les ressources
//   - Création des command buffers
//   - Soumission et synchronisation
//   - Upload/Download de données
//   - Informations sur les capacités GPU
//
// Architecture :
//   NkIGraphicsContext (couche 0)  →  NkIDevice (couche 1)
//
// Un NkIDevice est créé depuis un NkIGraphicsContext existant.
// Il partage le même device natif (VkDevice, ID3D12Device, etc.)
// sans en créer un nouveau — zéro overhead de création.
//
// Thread safety :
//   - CreateXxx / DestroyXxx : thread-safe (mutex interne par défaut)
//   - CreateCommandBuffer     : thread-safe
//   - Submit                  : sérialisé par queue interne
//   - Map/Unmap               : thread-safe
// =============================================================================
#include "NkRHI_Descs.h"
#include "../Commands/NkICommandBuffer.h"
#include "../../NkFinal_work/Core/NkIGraphicsContext.h"
#include <functional>

namespace nkentseu {

// =============================================================================
// Capacités du device
// =============================================================================
struct NkDeviceCaps {
    // Limites
    uint32  maxTextureDim2D         = 0;
    uint32  maxTextureDim3D         = 0;
    uint32  maxTextureCubeSize      = 0;
    uint32  maxTextureArrayLayers   = 0;
    uint32  maxColorAttachments     = 8;
    uint32  maxUniformBufferRange   = 65536;
    uint32  maxStorageBufferRange   = 0;
    uint32  maxPushConstantBytes    = 128;
    uint32  maxVertexAttributes     = 16;
    uint32  maxVertexBindings       = 16;
    uint32  maxComputeGroupSizeX    = 1024;
    uint32  maxComputeGroupSizeY    = 1024;
    uint32  maxComputeGroupSizeZ    = 64;
    uint32  maxComputeSharedMemory  = 32768;
    uint32  maxDescriptorSets       = 4;
    uint32  maxSamplerAnisotropy    = 16;
    uint32  minUniformBufferAlign   = 256;
    uint32  minStorageBufferAlign   = 16;
    uint64  vramBytes               = 0;

    // Fonctionnalités
    bool    tessellationShaders     = false;
    bool    geometryShaders         = false;
    bool    meshShaders             = false;         // DX12/VK 1.2+/Metal 3
    bool    computeShaders          = false;
    bool    drawIndirect            = false;
    bool    drawIndirectCount       = false;         // DX12/VK 1.2+
    bool    multiDrawIndirect       = false;
    bool    bindlessTextures        = false;         // DX12/VK descriptor indexing
    bool    rayTracing              = false;         // DX12 DXR / VK KHR_ray_tracing
    bool    variableRateShading     = false;
    bool    conservativeRasterization = false;
    bool    depthClipControl        = false;
    bool    shaderFloat16           = false;
    bool    shaderInt16             = false;
    bool    shaderAtomicInt64       = false;
    bool    shaderAtomicFloat       = false;
    bool    timestampQueries        = false;
    bool    pipelineStats           = false;
    bool    multiViewport           = false;
    bool    independentBlend        = false;
    bool    logicOp                 = false;
    bool    textureCompressionBC    = false;
    bool    textureCompressionETC2  = false;
    bool    textureCompressionASTC  = false;
    bool    nonPowerOfTwoTextures   = true;
    bool    mipGenInShader          = true;

    // MSAA supportés
    bool    msaa2x  = false;
    bool    msaa4x  = false;
    bool    msaa8x  = false;
    bool    msaa16x = false;
};

// =============================================================================
// Infos d'upload (pour NkIDevice::WriteBuffer/WriteTexture)
// =============================================================================
struct NkMappedMemory {
    void*   ptr    = nullptr;
    uint64  size   = 0;
    uint32  rowPitch   = 0;
    uint32  depthPitch = 0;
    bool    IsValid() const { return ptr != nullptr; }
};

// =============================================================================
// Frame context (synchronisation par frame en vol)
// =============================================================================
struct NkFrameContext {
    uint32       frameIndex     = 0;    // 0..maxFramesInFlight-1
    uint64       frameNumber    = 0;    // compteur global croissant
    NkFenceHandle frameFence;           // signalé quand le GPU a fini ce frame
};

// =============================================================================
// NkIDevice — interface abstraite complète
// =============================================================================
class NkIDevice {
public:
    virtual ~NkIDevice() = default;

    // ── Cycle de vie ─────────────────────────────────────────────────────────
    // Créer depuis un contexte graphique existant (partage le device natif)
    virtual bool         Initialize(NkIGraphicsContext* ctx)   = 0;
    virtual void         Shutdown()                             = 0;
    virtual bool         IsValid()                       const  = 0;
    virtual NkGraphicsApi GetApi()                       const  = 0;
    virtual const NkDeviceCaps& GetCaps()                const  = 0;

    // =========================================================================
    // Buffers
    // =========================================================================
    virtual NkBufferHandle  CreateBuffer (const NkBufferDesc& desc)             = 0;
    virtual void            DestroyBuffer(NkBufferHandle& handle)               = 0;

    // Upload synchrone depuis CPU (bloque jusqu'à completion)
    virtual bool WriteBuffer(NkBufferHandle buf,
                              const void* data, uint64 size,
                              uint64 offset=0)                                   = 0;

    // Upload via staging buffer (async, non-bloquant)
    virtual bool WriteBufferAsync(NkBufferHandle buf,
                                   const void* data, uint64 size,
                                   uint64 offset=0)                              = 0;

    // Readback synchrone (bloque)
    virtual bool ReadBuffer(NkBufferHandle buf, void* out,
                             uint64 size, uint64 offset=0)                       = 0;

    // Map/Unmap (pour Upload/Readback buffers)
    virtual NkMappedMemory MapBuffer(NkBufferHandle buf,
                                      uint64 offset=0, uint64 size=0)           = 0;
    virtual void           UnmapBuffer(NkBufferHandle buf)                      = 0;

    // =========================================================================
    // Textures
    // =========================================================================
    virtual NkTextureHandle  CreateTexture (const NkTextureDesc& desc)          = 0;
    virtual void             DestroyTexture(NkTextureHandle& handle)            = 0;

    // Upload d'une texture 2D entière (mip 0, layer 0)
    virtual bool WriteTexture(NkTextureHandle tex,
                               const void* pixels,
                               uint32 rowPitch=0)                               = 0;

    // Upload d'un mip/layer spécifique
    virtual bool WriteTextureRegion(NkTextureHandle tex,
                                     const void* pixels,
                                     uint32 x, uint32 y, uint32 z,
                                     uint32 width, uint32 height, uint32 depth,
                                     uint32 mipLevel=0, uint32 arrayLayer=0,
                                     uint32 rowPitch=0)                         = 0;

    // Génération automatique des mip-maps (synchrone)
    virtual bool GenerateMipmaps(NkTextureHandle tex,
                                  NkFilter filter=NkFilter::Linear)             = 0;

    // =========================================================================
    // Samplers
    // =========================================================================
    virtual NkSamplerHandle  CreateSampler (const NkSamplerDesc& desc)          = 0;
    virtual void             DestroySampler(NkSamplerHandle& handle)            = 0;

    // =========================================================================
    // Shaders
    // =========================================================================
    virtual NkShaderHandle   CreateShader (const NkShaderDesc& desc)            = 0;
    virtual void             DestroyShader(NkShaderHandle& handle)              = 0;

    // =========================================================================
    // Pipelines (compilés et cachés)
    // =========================================================================
    virtual NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc& desc) = 0;
    virtual NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc& desc)  = 0;
    virtual void             DestroyPipeline(NkPipelineHandle& handle)                  = 0;

    // Pipeline cache — sérialise/désérialise le cache compilé sur disque
    // Permet de sauter la recompilation au prochain lancement
    virtual bool SavePipelineCache (const char* path) { (void)path; return false; }
    virtual bool LoadPipelineCache (const char* path) { (void)path; return false; }

    // =========================================================================
    // Render Passes & Framebuffers
    // =========================================================================
    virtual NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc& desc)    = 0;
    virtual void                DestroyRenderPass (NkRenderPassHandle& handle)      = 0;

    virtual NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc& desc)   = 0;
    virtual void                DestroyFramebuffer(NkFramebufferHandle& handle)     = 0;

    // Framebuffer du swapchain (fenêtre principale) pour le frame courant
    virtual NkFramebufferHandle GetSwapchainFramebuffer() const                     = 0;
    virtual NkRenderPassHandle  GetSwapchainRenderPass()  const                     = 0;
    virtual NkFormat            GetSwapchainFormat()      const                     = 0;
    virtual NkFormat            GetSwapchainDepthFormat() const                     = 0;
    virtual uint32              GetSwapchainWidth()       const                     = 0;
    virtual uint32              GetSwapchainHeight()      const                     = 0;

    // =========================================================================
    // Descriptor Sets
    // =========================================================================
    // Layout (schéma de binding — partagé entre plusieurs sets)
    virtual NkDescSetHandle CreateDescriptorSetLayout(
        const NkDescriptorSetLayoutDesc& desc)                                      = 0;
    virtual void            DestroyDescriptorSetLayout(NkDescSetHandle& handle)     = 0;

    // Set (instance concrète d'un layout, mise à jour avec les vraies ressources)
    virtual NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)     = 0;
    virtual void            FreeDescriptorSet    (NkDescSetHandle& handle)          = 0;

    // Écrire les ressources dans le set
    virtual void UpdateDescriptorSets(const NkDescriptorWrite* writes,
                                       uint32 count)                                = 0;

    // Shortcut pour un seul buffer uniform
    void BindUniformBuffer(NkDescSetHandle set, uint32 binding,
                            NkBufferHandle buf, uint64 range=0) {
        NkDescriptorWrite w{};
        w.set=set; w.binding=binding; w.type=NkDescriptorType::UniformBuffer;
        w.buffer=buf; w.bufferRange=range;
        UpdateDescriptorSets(&w,1);
    }
    // Shortcut texture + sampler
    void BindTextureSampler(NkDescSetHandle set, uint32 binding,
                             NkTextureHandle tex, NkSamplerHandle samp) {
        NkDescriptorWrite w{};
        w.set=set; w.binding=binding;
        w.type=NkDescriptorType::CombinedImageSampler;
        w.texture=tex; w.sampler=samp;
        UpdateDescriptorSets(&w,1);
    }

    // =========================================================================
    // Command Buffers
    // =========================================================================
    virtual NkICommandBuffer* CreateCommandBuffer(
        NkCommandBufferType type=NkCommandBufferType::Graphics)                     = 0;
    virtual void              DestroyCommandBuffer(NkICommandBuffer*& cb)           = 0;

    // =========================================================================
    // Soumission & Synchronisation
    // =========================================================================
    // Soumettre un ou plusieurs command buffers
    virtual void Submit(NkICommandBuffer* const* cbs, uint32 count,
                        NkFenceHandle signalFence = NkFenceHandle::Null())          = 0;

    // Soumettre et présenter en une seule opération (cas le plus courant)
    virtual void SubmitAndPresent(NkICommandBuffer* cb)                             = 0;

    // Synchronisation CPU/GPU
    virtual NkFenceHandle CreateFence(bool signaled=false)                          = 0;
    virtual void          DestroyFence(NkFenceHandle& handle)                       = 0;
    virtual bool          WaitFence(NkFenceHandle fence, uint64 timeoutNs=UINT64_MAX) = 0;
    virtual bool          IsFenceSignaled(NkFenceHandle fence)                      = 0;
    virtual void          ResetFence(NkFenceHandle fence)                           = 0;

    // Attendre que TOUT le GPU soit idle (flush complet — utiliser avec parcimonie)
    virtual void WaitIdle()                                                         = 0;

    // =========================================================================
    // Frame management (triple buffering, frame index, etc.)
    // =========================================================================
    virtual void              BeginFrame(NkFrameContext& frame)                     = 0;
    virtual void              EndFrame  (NkFrameContext& frame)                     = 0;
    virtual uint32            GetFrameIndex()                              const     = 0;
    virtual uint32            GetMaxFramesInFlight()                       const     = 0;
    virtual uint64            GetFrameNumber()                             const     = 0;

    // =========================================================================
    // Resize (fenêtre redimensionnée)
    // =========================================================================
    virtual void OnResize(uint32 width, uint32 height)                              = 0;

    // =========================================================================
    // Queries GPU (timing, stats pipeline)
    // =========================================================================
    virtual void  BeginTimestampQuery(uint32 index) {}
    virtual void  EndTimestampQuery  (uint32 index) {}
    virtual bool  GetTimestampResults(uint64* outNs, uint32 count) { return false; }
    virtual float GetTimestampPeriodNs() { return 1.f; }

    // =========================================================================
    // Accès natif (pour interop, extensions, débogage)
    // =========================================================================
    virtual void* GetNativeDevice()         const = 0;  // VkDevice, ID3D12Device*, etc.
    virtual void* GetNativeCommandQueue()   const = 0;
    virtual void* GetNativePhysicalDevice() const { return nullptr; }

    // =========================================================================
    // Statistiques et débogage
    // =========================================================================
    struct FrameStats {
        uint32 drawCalls=0, dispatchCalls=0;
        uint32 triangles=0, vertices=0;
        uint32 pipelineChanges=0, descriptorSetChanges=0;
        uint64 gpuMemoryUsedBytes=0;
        float  gpuTimeMs=0.f;
        float  cpuSubmitTimeMs=0.f;
    };
    virtual const FrameStats& GetLastFrameStats() const {
        static FrameStats s; return s;
    }
    virtual void ResetFrameStats() {}

    // Nommage des ressources pour les outils de débogage
    virtual void SetDebugName(NkBufferHandle  buf,  const char* name) {}
    virtual void SetDebugName(NkTextureHandle tex,  const char* name) {}
    virtual void SetDebugName(NkPipelineHandle pipe,const char* name) {}
};

} // namespace nkentseu
