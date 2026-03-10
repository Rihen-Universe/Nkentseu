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
    static constexpr NkU32 kMaxSlots = 4096;
    T      slots[kMaxSlots]{};
    bool   used [kMaxSlots]{};
    NkU32  freeHead = 0;

    NkU32 Alloc() {
        for (NkU32 i = freeHead; i < kMaxSlots; ++i) {
            if (!used[i]) { used[i] = true; freeHead = i + 1; return i + 1; }
        }
        return 0; // pool plein
    }
    void Free(NkU32 id) {
        if (id == 0 || id > kMaxSlots) return;
        used[id - 1] = false;
        slots[id - 1] = {};
        if (id - 1 < freeHead) freeHead = id - 1;
    }
    T* Get(NkU32 id) {
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
    NkU64   size       = 0;
    void*   mapped     = nullptr; // != nullptr si mappé
    NkBufferUsage usage{};
    NkMemoryType  memory{};
};

struct VkTextureEntry {
    void*  image       = nullptr; // VkImage
    void*  imageView   = nullptr; // VkImageView
    void*  allocation  = nullptr; // VkDeviceMemory
    NkU32  width = 0, height = 0, depth = 1;
    NkU32  mipLevels = 1, arrayLayers = 1;
    NkU32  format      = 0;      // VkFormat (NkU32)
    NkU32  layout      = 0;      // VkImageLayout courant
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
    NkU32 width = 0, height = 0;
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
                         const NkClearValue* clears, NkU32 count) override;
    void EndRenderPass()  override;
    void NextSubpass()    override;

    void BindPipeline  (NkPipelineHandle pipeline) override;
    void SetViewport   (const NkViewport& vp)      override;
    void SetScissor    (const NkScissor&  sc)      override;

    void BindVertexBuffer (NkU32 binding, NkBufferHandle buf, NkU64 offset) override;
    void BindIndexBuffer  (NkBufferHandle buf, NkIndexType type, NkU64 offset) override;
    void BindDescriptorSet(NkU32 setIndex, NkDescriptorSetHandle ds,
                           NkPipelineHandle pipeline) override;
    void PushConstants    (NkPipelineHandle pipeline, NkShaderStage stages,
                           NkU32 offset, NkU32 size, const void* data) override;

    void Draw        (const NkDrawCall&        cmd) override;
    void DrawIndexed (const NkDrawIndexedCall& cmd) override;
    void DrawIndirect(NkBufferHandle buf, NkU64 offset, NkU32 count) override;

    void Dispatch(NkU32 x, NkU32 y, NkU32 z) override;

    void CopyBuffer  (NkBufferHandle  src, NkBufferHandle  dst,
                      NkU64 srcOff, NkU64 dstOff, NkU64 size) override;
    void CopyTexture (NkTextureHandle src, NkTextureHandle dst) override;
    void UploadBuffer(NkBufferHandle dst, const void* data,
                      NkU64 offset, NkU64 size) override;
    void UploadTexture(NkTextureHandle dst, const void* data,
                       NkU32 mip, NkU32 layer,
                       NkU32 x, NkU32 y, NkU32 w, NkU32 h) override;

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
    static constexpr NkU32 MAX_FRAMES_IN_FLIGHT = 2;

    NkVulkanDevice()  = default;
    ~NkVulkanDevice() override;

    // ── Init / Shutdown ──────────────────────────────────────────────────────
    bool Init   (const NkSurfaceDesc& surface,
                 const NkGraphicsContextConfig& config) override;
    void Shutdown() override;

    // ── Frame ────────────────────────────────────────────────────────────────
    bool BeginFrame() override;
    void EndFrame  (NkRHICommandBuffer& cmd) override;
    bool Recreate  (NkU32 width, NkU32 height) override;

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
                              NkU32 count) override;

    void* MapBuffer  (NkBufferHandle) override;
    void  UnmapBuffer(NkBufferHandle) override;

    // ── Command buffers ──────────────────────────────────────────────────────
    NkRHICommandBufferPtr CreateCommandBuffer(bool secondary) override;
    NkRHICommandBuffer&   GetCurrentCommandBuffer() override;

    // ── Swapchain ────────────────────────────────────────────────────────────
    NkTextureHandle     GetSwapchainTexture(NkU32 index) override;
    NkU32               GetSwapchainImageCount() const override;
    NkU32               GetCurrentSwapchainIndex() const override;
    NkRenderPassHandle  GetSwapchainRenderPass()  override;
    NkFramebufferHandle GetCurrentFramebuffer()   override;
    NkU32               GetWidth()  const override { return mWidth; }
    NkU32               GetHeight() const override { return mHeight; }
    NkGraphicsApi       GetApi()    const override { return NkGraphicsApi::Vulkan; }

    void WaitIdle() override;

private:
    // ── Helpers d'initialisation ─────────────────────────────────────────────
    bool CreateInstance    (const NkGraphicsContextConfig& cfg);
    bool CreateSurface     (const NkSurfaceDesc& surface);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain   (NkU32 width, NkU32 height);
    bool CreateSwapchainViews();
    bool CreateRenderPassInternal();
    bool CreateFramebuffersInternal();
    bool CreateCommandPool ();
    bool CreateCommandBuffers();
    bool CreateSyncObjects ();
    bool CreateDescriptorPool();
    bool CreateDepthBuffer (NkU32 width, NkU32 height);

    void DestroySwapchain();

    // Helpers mémoire
    NkU32 FindMemoryType(NkU32 typeFilter, NkU32 properties) const;
    bool  AllocateMemory(NkU64 size, NkU32 memTypeFilter,
                         NkU32 memProps, void** outMemory) const;
    NkBufferHandle CreateBufferInternal(NkU64 size, NkU32 vkUsage,
                                         NkU32 memProps);
    void CopyBufferImmediate(void* src, void* dst, NkU64 size);
    void TransitionImageLayout(void* image, NkU32 format,
                                NkU32 oldLayout, NkU32 newLayout,
                                NkU32 mipLevels = 1);
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

    NkU32  mGraphicsFamily = 0;
    NkU32  mPresentFamily  = 0;
    NkU32  mComputeFamily  = 0;

    // ── Swapchain ────────────────────────────────────────────────────────────
    void*  mSwapchain      = nullptr; // VkSwapchainKHR
    NkU32  mWidth  = 0, mHeight = 0;
    NkU32  mSwapchainFormat= 0;       // VkFormat

    static constexpr NkU32 kMaxSwapImages = 4;
    void*  mSwapImages   [kMaxSwapImages]{}; // VkImage
    void*  mSwapViews    [kMaxSwapImages]{}; // VkImageView
    void*  mSwapFBs      [kMaxSwapImages]{}; // VkFramebuffer
    NkU32  mSwapImageCount = 0;
    NkU32  mCurrentImage   = 0;

    // Depth buffer
    void*  mDepthImage     = nullptr;
    void*  mDepthView      = nullptr;
    void*  mDepthMemory    = nullptr;
    NkU32  mDepthFormat    = 0;

    // Render pass principal (swapchain)
    void*  mSwapRenderPass = nullptr;

    // ── Synchronisation ──────────────────────────────────────────────────────
    void*  mImageAvailable[MAX_FRAMES_IN_FLIGHT]{};   // VkSemaphore
    void*  mRenderFinished[MAX_FRAMES_IN_FLIGHT]{};   // VkSemaphore
    void*  mInFlight      [MAX_FRAMES_IN_FLIGHT]{};   // VkFence
    NkU32  mCurrentFrame   = 0;

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