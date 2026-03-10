#pragma once
// =============================================================================
// NkVulkanDevice.h — Implémentation Vulkan du NkRHIDevice
//
// Supporte : Windows (Win32), Linux (XLib/XCB/Wayland), macOS (MoltenVK),
//            iOS (MoltenVK), Android, Web (via vulkan.js/Emscripten partiel).
//
// Architecture multi-plateforme :
//   - Instance extensions choisies par plateforme (#ifdef)
//   - Surface créée via les helpers NkWindow::GetSurfaceDesc()
//   - MAX_FRAMES_IN_FLIGHT = 2 (double buffering des command buffers)
//   - Resource pool interne avec handle IDs stables
// =============================================================================

#include "NkRHIDevice.h"
#include "NKPlatform/NkPlatformDetect.h"

// Vulkan header — on n'expose pas vulkan.h dans ce .h public.
// Les types Vulkan concrets sont uniquement dans le .cpp.
// Ici on ne stocke que des void* dans la struct interne.

namespace nkentseu { namespace rhi {

// ---------------------------------------------------------------------------
// Pool interne générique — alloue des handles stables
// ---------------------------------------------------------------------------
template<typename T>
struct NkPool {
    static constexpr uint32 kMaxSlots = 4096;
    T      slots[kMaxSlots]{};
    bool   used [kMaxSlots]{};
    uint32  freeHead = 0;

    uint32 Alloc() {
        for (uint32 i = freeHead; i < kMaxSlots; ++i) {
            if (!used[i]) { used[i] = true; freeHead = i + 1; return i + 1; }
        }
        return 0; // pool plein
    }
    void Free(uint32 id) {
        if (id == 0 || id > kMaxSlots) return;
        used[id - 1] = false;
        slots[id - 1] = {};
        if (id - 1 < freeHead) freeHead = id - 1;
    }
    T* Get(uint32 id) {
        if (id == 0 || id > kMaxSlots || !used[id-1]) return nullptr;
        return &slots[id - 1];
    }
};

// ---------------------------------------------------------------------------
// Types Vulkan stockés de façon opaque (void* dans le pool)
// Les vrais types VkXxx sont dans le .cpp
// ---------------------------------------------------------------------------
struct VkBufferEntry {
    void*   buffer     = nullptr; // VkBuffer
    void*   allocation = nullptr; // VkDeviceMemory ou VmaAllocation
    uint64   size       = 0;
    void*   mapped     = nullptr; // != nullptr si mappé
    NkBufferUsage usage{};
    NkMemoryType  memory{};
};

struct VkTextureEntry {
    void*  image       = nullptr; // VkImage
    void*  imageView   = nullptr; // VkImageView
    void*  allocation  = nullptr; // VkDeviceMemory
    uint32  width = 0, height = 0, depth = 1;
    uint32  mipLevels = 1, arrayLayers = 1;
    uint32  format      = 0;      // VkFormat (uint32)
    uint32  layout      = 0;      // VkImageLayout courant
    bool   isSwapchain = false;
};

struct VkSamplerEntry {
    void*  sampler = nullptr; // VkSampler
};

struct VkShaderEntry {
    void*         module = nullptr; // VkShaderModule
    NkShaderStage stage{};
    const char*   entry = "main";
};

struct VkDescriptorLayoutEntry {
    void* layout = nullptr; // VkDescriptorSetLayout
};

struct VkDescriptorSetEntry {
    void* set    = nullptr; // VkDescriptorSet
};

struct VkRenderPassEntry {
    void* renderPass = nullptr; // VkRenderPass
};

struct VkFramebufferEntry {
    void* framebuffer = nullptr; // VkFramebuffer
    uint32 width = 0, height = 0;
};

struct VkPipelineEntry {
    void*            pipeline = nullptr; // VkPipeline
    void*            layout   = nullptr; // VkPipelineLayout
    NkPipelineType   type{};
};

// ---------------------------------------------------------------------------
// NkVulkanCommandBuffer
// ---------------------------------------------------------------------------
class NkVulkanCommandBuffer final : public NkRHICommandBuffer {
public:
    explicit NkVulkanCommandBuffer(void* cmdBuf, void* device);

    void Begin()  override;
    void End()    override;

    void BeginRenderPass(NkRenderPassHandle rp, NkFramebufferHandle fb,
                         const NkClearValue* clears, uint32 count) override;
    void EndRenderPass()  override;
    void NextSubpass()    override;

    void BindPipeline  (NkPipelineHandle pipeline) override;
    void SetViewport   (const NkViewport& vp)      override;
    void SetScissor    (const NkScissor&  sc)      override;

    void BindVertexBuffer (uint32 binding, NkBufferHandle buf, uint64 offset) override;
    void BindIndexBuffer  (NkBufferHandle buf, NkIndexType type, uint64 offset) override;
    void BindDescriptorSet(uint32 setIndex, NkDescriptorSetHandle ds,
                           NkPipelineHandle pipeline) override;
    void PushConstants    (NkPipelineHandle pipeline, NkShaderStage stages,
                           uint32 offset, uint32 size, const void* data) override;

    void Draw        (const NkDrawCall&        cmd) override;
    void DrawIndexed (const NkDrawIndexedCall& cmd) override;
    void DrawIndirect(NkBufferHandle buf, uint64 offset, uint32 count) override;

    void Dispatch(uint32 x, uint32 y, uint32 z) override;

    void CopyBuffer  (NkBufferHandle  src, NkBufferHandle  dst,
                      uint64 srcOff, uint64 dstOff, uint64 size) override;
    void CopyTexture (NkTextureHandle src, NkTextureHandle dst) override;
    void UploadBuffer(NkBufferHandle dst, const void* data,
                      uint64 offset, uint64 size) override;
    void UploadTexture(NkTextureHandle dst, const void* data,
                       uint32 mip, uint32 layer,
                       uint32 x, uint32 y, uint32 w, uint32 h) override;

    void TextureBarrier(NkTextureHandle tex, bool fromWrite, bool toRead) override;
    void BufferBarrier (NkBufferHandle  buf, bool fromWrite, bool toRead) override;

    void* GetVkCommandBuffer() const { return mCmdBuf; }

    // Pools de ressources partagés avec NkVulkanDevice (non-owning)
    NkPool<VkBufferEntry>*         mBuffers     = nullptr;
    NkPool<VkTextureEntry>*        mTextures    = nullptr;
    NkPool<VkPipelineEntry>*       mPipelines   = nullptr;
    NkPool<VkDescriptorSetEntry>*  mDescSets    = nullptr;
    NkPool<VkDescriptorLayoutEntry>* mDescLayouts= nullptr;
    NkPool<VkRenderPassEntry>*     mRenderPasses= nullptr;
    NkPool<VkFramebufferEntry>*    mFramebuffers= nullptr;

private:
    void*  mCmdBuf = nullptr; // VkCommandBuffer
    void*  mDevice = nullptr; // VkDevice (pour les helpers inline)
};

// ---------------------------------------------------------------------------
// NkVulkanDevice
// ---------------------------------------------------------------------------
class NkVulkanDevice final : public NkRHIDevice {
public:
    static constexpr uint32 MAX_FRAMES_IN_FLIGHT = 2;

    NkVulkanDevice()  = default;
    ~NkVulkanDevice() override;

    // ── Init / Shutdown ──────────────────────────────────────────────────────
    bool Init   (const NkSurfaceDesc& surface,
                 const NkGraphicsContextConfig& config) override;
    void Shutdown() override;

    // ── Frame ────────────────────────────────────────────────────────────────
    bool BeginFrame() override;
    void EndFrame  (NkRHICommandBuffer& cmd) override;
    bool Recreate  (uint32 width, uint32 height) override;

    // ── Ressources ───────────────────────────────────────────────────────────
    NkBufferHandle           CreateBuffer          (const NkBufferDesc&)   override;
    NkTextureHandle          CreateTexture         (const NkTextureDesc&)  override;
    NkSamplerHandle          CreateSampler         (const NkSamplerDesc&)  override;
    NkShaderHandle           CreateShader          (const NkShaderDesc&)   override;
    NkDescriptorLayoutHandle CreateDescriptorLayout(const NkDescriptorLayoutDesc&) override;
    NkDescriptorSetHandle    AllocDescriptorSet    (NkDescriptorLayoutHandle) override;
    NkRenderPassHandle       CreateRenderPass      (const NkRenderPassDesc&) override;
    NkFramebufferHandle      CreateFramebuffer     (const NkFramebufferDesc&) override;
    NkPipelineHandle         CreatePipeline        (const NkPipelineDesc&)  override;

    void DestroyBuffer          (NkBufferHandle)          override;
    void DestroyTexture         (NkTextureHandle)         override;
    void DestroySampler         (NkSamplerHandle)         override;
    void DestroyShader          (NkShaderHandle)          override;
    void DestroyDescriptorLayout(NkDescriptorLayoutHandle) override;
    void FreeDescriptorSet      (NkDescriptorSetHandle)   override;
    void DestroyRenderPass      (NkRenderPassHandle)      override;
    void DestroyFramebuffer     (NkFramebufferHandle)     override;
    void DestroyPipeline        (NkPipelineHandle)        override;

    void UpdateDescriptorSet(NkDescriptorSetHandle ds,
                              const NkDescriptorWrite* writes,
                              uint32 count) override;

    void* MapBuffer  (NkBufferHandle) override;
    void  UnmapBuffer(NkBufferHandle) override;

    // ── Command buffers ──────────────────────────────────────────────────────
    NkRHICommandBufferPtr CreateCommandBuffer(bool secondary) override;
    NkRHICommandBuffer&   GetCurrentCommandBuffer() override;

    // ── Swapchain ────────────────────────────────────────────────────────────
    NkTextureHandle     GetSwapchainTexture(uint32 index) override;
    uint32               GetSwapchainImageCount() const override;
    uint32               GetCurrentSwapchainIndex() const override;
    NkRenderPassHandle  GetSwapchainRenderPass()  override;
    NkFramebufferHandle GetCurrentFramebuffer()   override;
    uint32               GetWidth()  const override { return mWidth; }
    uint32               GetHeight() const override { return mHeight; }
    NkGraphicsApi       GetApi()    const override { return NkGraphicsApi::Vulkan; }

    void WaitIdle() override;

private:
    // ── Helpers d'initialisation ─────────────────────────────────────────────
    bool CreateInstance    (const NkGraphicsContextConfig& cfg);
    bool CreateSurface     (const NkSurfaceDesc& surface);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain   (uint32 width, uint32 height);
    bool CreateSwapchainViews();
    bool CreateRenderPassInternal();
    bool CreateFramebuffersInternal();
    bool CreateCommandPool ();
    bool CreateCommandBuffers();
    bool CreateSyncObjects ();
    bool CreateDescriptorPool();
    bool CreateDepthBuffer (uint32 width, uint32 height);

    void DestroySwapchain();

    // Helpers mémoire
    uint32 FindMemoryType(uint32 typeFilter, uint32 properties) const;
    bool  AllocateMemory(uint64 size, uint32 memTypeFilter,
                         uint32 memProps, void** outMemory) const;
    NkBufferHandle CreateBufferInternal(uint64 size, uint32 vkUsage,
                                         uint32 memProps);
    void CopyBufferImmediate(void* src, void* dst, uint64 size);
    void TransitionImageLayout(void* image, uint32 format,
                                uint32 oldLayout, uint32 newLayout,
                                uint32 mipLevels = 1);
    void* BeginSingleTimeCommands();
    void  EndSingleTimeCommands(void* cmd);

    // ── État Vulkan (void* pour ne pas exposer vulkan.h dans ce header) ──────
    void*  mInstance       = nullptr; // VkInstance
    void*  mDebugMessenger = nullptr; // VkDebugUtilsMessengerEXT
    void*  mSurface        = nullptr; // VkSurfaceKHR
    void*  mPhysicalDevice = nullptr; // VkPhysicalDevice
    void*  mDevice         = nullptr; // VkDevice
    void*  mGraphicsQueue  = nullptr; // VkQueue
    void*  mPresentQueue   = nullptr; // VkQueue
    void*  mComputeQueue   = nullptr; // VkQueue
    void*  mCommandPool    = nullptr; // VkCommandPool
    void*  mDescriptorPool = nullptr; // VkDescriptorPool

    uint32  mGraphicsFamily = 0;
    uint32  mPresentFamily  = 0;
    uint32  mComputeFamily  = 0;

    // ── Swapchain ────────────────────────────────────────────────────────────
    void*  mSwapchain      = nullptr; // VkSwapchainKHR
    uint32  mWidth  = 0, mHeight = 0;
    uint32  mSwapchainFormat= 0;       // VkFormat

    static constexpr uint32 kMaxSwapImages = 4;
    void*  mSwapImages   [kMaxSwapImages]{}; // VkImage
    void*  mSwapViews    [kMaxSwapImages]{}; // VkImageView
    void*  mSwapFBs      [kMaxSwapImages]{}; // VkFramebuffer
    uint32  mSwapImageCount = 0;
    uint32  mCurrentImage   = 0;

    // Depth buffer
    void*  mDepthImage     = nullptr;
    void*  mDepthView      = nullptr;
    void*  mDepthMemory    = nullptr;
    uint32  mDepthFormat    = 0;

    // Render pass principal (swapchain)
    void*  mSwapRenderPass = nullptr;

    // ── Synchronisation ──────────────────────────────────────────────────────
    void*  mImageAvailable[MAX_FRAMES_IN_FLIGHT]{};   // VkSemaphore
    void*  mRenderFinished[MAX_FRAMES_IN_FLIGHT]{};   // VkSemaphore
    void*  mInFlight      [MAX_FRAMES_IN_FLIGHT]{};   // VkFence
    uint32  mCurrentFrame   = 0;

    // ── Command buffers principaux ───────────────────────────────────────────
    void*  mCmdBuffers[MAX_FRAMES_IN_FLIGHT]{};       // VkCommandBuffer

    // Wrappers RHI (non-owning sur mCmdBuffers)
    NkVulkanCommandBuffer* mCmdWrappers[MAX_FRAMES_IN_FLIGHT]{};

    // ── Pools de ressources ──────────────────────────────────────────────────
    NkPool<VkBufferEntry>          mBuffers;
    NkPool<VkTextureEntry>         mTextures;
    NkPool<VkSamplerEntry>         mSamplers;
    NkPool<VkShaderEntry>          mShaders;
    NkPool<VkDescriptorLayoutEntry>mDescLayouts;
    NkPool<VkDescriptorSetEntry>   mDescSets;
    NkPool<VkRenderPassEntry>      mRenderPasses;
    NkPool<VkFramebufferEntry>     mFramebuffers;
    NkPool<VkPipelineEntry>        mPipelines;

    // Handles swapchain exposés via NkTextureHandle
    NkTextureHandle  mSwapTextureHandles[kMaxSwapImages]{};
    NkRenderPassHandle mSwapRenderPassHandle;
    NkFramebufferHandle mSwapFBHandles[kMaxSwapImages]{};

    bool mInitialized    = false;
    bool mValidation     = false;
};

}} // namespace nkentseu::rhi