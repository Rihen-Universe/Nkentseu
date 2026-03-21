#pragma once
// =============================================================================
// NkRHI_Device_VK.h — Backend Vulkan du NkIDevice
// Vulkan 1.2+ avec KHR_dynamic_rendering optionnel
// =============================================================================
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/String/NkString.h"

#ifdef NK_RHI_VK_ENABLED
#include <vulkan/vulkan.h>
#endif

namespace nkentseu {
using threading::NkMutex;
using threading::NkScopedLock;

#ifdef NK_RHI_VK_ENABLED

class NkVulkanCommandBuffer;

// =============================================================================
// Allocateur mémoire Vulkan simplifié (sans VMA)
// =============================================================================
struct NkVkAllocation {
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize   offset = 0;
    VkDeviceSize   size   = 0;
    void*          mapped = nullptr; // != nullptr si persistently mapped
};

// =============================================================================
// Objets internes
// =============================================================================
struct NkVkBuffer {
    VkBuffer        buffer = VK_NULL_HANDLE;
    NkVkAllocation  alloc;
    NkBufferDesc    desc;
    NkResourceState state = NkResourceState::NK_COMMON;
};

struct NkVkTexture {
    VkImage         image  = VK_NULL_HANDLE;
    VkImageView     view   = VK_NULL_HANDLE;
    NkVkAllocation  alloc;
    NkTextureDesc   desc;
    VkImageLayout   layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool            isSwapchain = false;
};

struct NkVkSampler { VkSampler sampler = VK_NULL_HANDLE; };

struct NkVkShader {
    struct Stage {
        VkShaderModule module = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage{};
        NkString entryPoint = "main";
    };
    NkVector<Stage> stages;
};

struct NkVkPipeline {
    VkPipeline            pipeline = VK_NULL_HANDLE;
    VkPipelineLayout      layout   = VK_NULL_HANDLE;
    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    bool isCompute = false;
};

struct NkVkRenderPass { VkRenderPass renderPass = VK_NULL_HANDLE; NkRenderPassDesc desc; };
struct NkVkFramebuffer { VkFramebuffer framebuffer = VK_NULL_HANDLE; uint32 w=0, h=0; };

struct NkVkDescSetLayout {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    NkDescriptorSetLayoutDesc desc;
};

struct NkVkDescSet {
    VkDescriptorSet set = VK_NULL_HANDLE;
    uint64 layoutId = 0;
};

struct NkVkFenceObj {
    VkFence fence = VK_NULL_HANDLE;
};

// =============================================================================
// Frame data (triple buffering)
// =============================================================================
struct NkVkFrameData {
    VkCommandPool   cmdPool        = VK_NULL_HANDLE;
    VkCommandBuffer cmdBuffer      = VK_NULL_HANDLE;
    VkFence         inFlightFence  = VK_NULL_HANDLE;
    VkSemaphore     imageAvailable = VK_NULL_HANDLE;
    VkSemaphore     renderFinished = VK_NULL_HANDLE;
    bool            inUse          = false;
};

// =============================================================================
// NkVulkanDevice
// =============================================================================
class NkVulkanDevice final : public NkIDevice {
public:
    NkVulkanDevice()  = default;
    ~NkVulkanDevice() override;

    bool          Initialize(NkIGraphicsContext* ctx) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_VULKAN; }
    const NkDeviceCaps& GetCaps()               const override { return mCaps; }

    // ── Buffers ───────────────────────────────────────────────────────────────
    NkBufferHandle  CreateBuffer (const NkBufferDesc& d)                      override;
    void            DestroyBuffer(NkBufferHandle& h)                          override;
    bool WriteBuffer(NkBufferHandle buf,const void* data,uint64 sz,uint64 off)override;
    bool WriteBufferAsync(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) override;
    bool ReadBuffer(NkBufferHandle buf,void* out,uint64 sz,uint64 off)        override;
    NkMappedMemory MapBuffer(NkBufferHandle buf,uint64 off,uint64 sz)         override;
    void           UnmapBuffer(NkBufferHandle buf)                            override;

    // ── Textures ──────────────────────────────────────────────────────────────
    NkTextureHandle  CreateTexture (const NkTextureDesc& d)                   override;
    void             DestroyTexture(NkTextureHandle& h)                        override;
    bool WriteTexture(NkTextureHandle t,const void* p,uint32 rp)              override;
    bool WriteTextureRegion(NkTextureHandle t,const void* p,
                             uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
                             uint32 mip,uint32 layer,uint32 rp)               override;
    bool GenerateMipmaps(NkTextureHandle t, NkFilter f)                       override;

    // ── Samplers ──────────────────────────────────────────────────────────────
    NkSamplerHandle  CreateSampler (const NkSamplerDesc& d)                   override;
    void             DestroySampler(NkSamplerHandle& h)                        override;

    // ── Shaders ───────────────────────────────────────────────────────────────
    NkShaderHandle   CreateShader (const NkShaderDesc& d)                     override;
    void             DestroyShader(NkShaderHandle& h)                          override;

    // ── Pipelines ─────────────────────────────────────────────────────────────
    NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d)  override;
    NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc& d)   override;
    void             DestroyPipeline(NkPipelineHandle& h)                     override;

    // ── Render Passes & Framebuffers ──────────────────────────────────────────
    NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc& d)         override;
    void                DestroyRenderPass (NkRenderPassHandle& h)              override;
    NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc& d)        override;
    void                DestroyFramebuffer(NkFramebufferHandle& h)             override;
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFBs[mCurrentImageIdx]; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
    NkGpuFormat            GetSwapchainFormat()      const override;
    NkGpuFormat            GetSwapchainDepthFormat() const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32              GetSwapchainWidth()        const override { return mWidth; }
    uint32              GetSwapchainHeight()       const override { return mHeight; }

    // ── Descriptor Sets ───────────────────────────────────────────────────────
    NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) override;
    void            DestroyDescriptorSetLayout(NkDescSetHandle& h)                override;
    NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)           override;
    void            FreeDescriptorSet    (NkDescSetHandle& h)                     override;
    void            UpdateDescriptorSets(const NkDescriptorWrite* w, uint32 n)   override;

    // ── Command Buffers ───────────────────────────────────────────────────────
    NkICommandBuffer* CreateCommandBuffer(NkCommandBufferType t)                  override;
    void              DestroyCommandBuffer(NkICommandBuffer*& cb)                 override;

    // ── Submit & Sync ─────────────────────────────────────────────────────────
    void Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence)     override;
    void SubmitAndPresent(NkICommandBuffer* cb)                                   override;
    NkFenceHandle CreateFence(bool signaled)                                      override;
    void          DestroyFence(NkFenceHandle& h)                                  override;
    bool          WaitFence(NkFenceHandle f, uint64 timeoutNs)                    override;
    bool          IsFenceSignaled(NkFenceHandle f)                                override;
    void          ResetFence(NkFenceHandle f)                                     override;
    void          WaitIdle()                                                      override;

    // ── Frame ─────────────────────────────────────────────────────────────────
    void   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return kMaxFrames; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }

    void OnResize(uint32 w, uint32 h)     override;

    void* GetNativeDevice()         const override { return mDevice; }
    void* GetNativeCommandQueue()   const override { return mGraphicsQueue; }
    void* GetNativePhysicalDevice() const override { return mPhysicalDevice; }

    // ── Swapchain (NkISwapchain lifecycle) ────────────────────────────────────
    NkISwapchain* CreateSwapchain(NkIGraphicsContext* ctx,
                                   const NkSwapchainDesc& desc) override;
    void          DestroySwapchain(NkISwapchain*& sc)            override;

    // ── Semaphores GPU ────────────────────────────────────────────────────────
    NkSemaphoreHandle CreateGpuSemaphore()                          override;
    void              DestroySemaphore(NkSemaphoreHandle& handle) override;

    // ── Accès interne (pour NkVulkanCommandBuffer) ───────────────────────────────
    VkDevice         GetVkDevice()  const { return mDevice; }
    VkRenderPass     GetVkRP(uint64 id) const;
    VkFramebuffer    GetVkFB(uint64 id) const;
    VkBuffer         GetVkBuffer(uint64 id) const;
    VkImage          GetVkImage (uint64 id) const;
    VkImageView      GetVkImageView(uint64 id) const;
    VkSampler        GetVkSampler(uint64 id) const;
    VkPipeline       GetVkPipeline(uint64 id) const;
    VkPipelineLayout GetVkPipelineLayout(uint64 id) const;
    VkDescriptorSet  GetVkDescSet(uint64 id) const;
    bool             IsComputePipeline(uint64 id) const;
    NkTextureDesc    GetTextureDesc(uint64 id) const;
    NkBufferDesc     GetBufferDesc(uint64 id)  const;

    // Exécution immédiate (upload, etc.)
    VkCommandBuffer BeginOneShot();
    void            EndOneShot(VkCommandBuffer cmd);

    // ── Accesseurs pour NkVulkanSwapchain ────────────────────────────────────
    VkSemaphore      GetVkSemaphore(NkSemaphoreHandle h) const;
    VkFence          GetVkFence    (NkFenceHandle h)      const;
    VkSwapchainKHR   GetVkSwapchain() const { return mSwapchain; }
    VkQueue          GetVkPresentQueue() const { return mPresentQueue; }
    VkFormat         GetVkSwapchainFormatNative() const { return mSwapFormat; }
    uint32           GetVkSwapchainImageCount() const { return (uint32)mSwapImages.Size(); }
    uint32               GetSwapchainFramebufferCount() const { return (uint32)mSwapchainFBs.Size(); }
    NkFramebufferHandle  GetSwapchainFramebufferAt(uint32 i) const {
        return (i < (uint32)mSwapchainFBs.Size()) ? mSwapchainFBs[i] : NkFramebufferHandle::Null();
    }

private:
    void QueryCaps();
    void CreateSwapchain(uint32 w, uint32 h);
    void DestroySwapchain();
    void RecreateSwapchain(uint32 w, uint32 h);
    void CreateSwapchainRenderPassAndFramebuffers();
    void DestroySwapchainFramebuffers();

    // ── Méthodes internes sans verrou (évitent la réentrance du mutex) ────────
    NkBufferHandle      CreateBufferInternal  (const NkBufferDesc& d);
    void                DestroyBufferInternal (NkBufferHandle& h);
    bool                WriteBufferInternal   (NkBufferHandle buf, const void* data, uint64 sz, uint64 off);
    NkTextureHandle     CreateTextureInternal (const NkTextureDesc& d);
    void                DestroyTextureInternal(NkTextureHandle& h);
    NkRenderPassHandle  CreateRenderPassInternal  (const NkRenderPassDesc& d);
    void                DestroyRenderPassInternal (NkRenderPassHandle& h);
    NkFramebufferHandle CreateFramebufferInternal (const NkFramebufferDesc& d);
    void                DestroyFramebufferInternal(NkFramebufferHandle& h);

    // Allocation mémoire
    NkVkAllocation AllocateMemory(VkMemoryRequirements req, VkMemoryPropertyFlags props);
    void           FreeMemory(NkVkAllocation& alloc);
    uint32         FindMemoryType(uint32 filter, VkMemoryPropertyFlags props) const;

    // Transitions d'image
    void TransitionImage(VkCommandBuffer cmd, VkImage img,
                          VkImageLayout from, VkImageLayout to,
                          VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT,
                          uint32 baseMip=0, uint32 mips=1,
                          uint32 baseLayer=0, uint32 layers=1);

    // Conversions Vk
    static VkFormat          ToVkFormat(NkGpuFormat f);
    static VkImageType       ToVkImageType(NkTextureType t);
    static VkImageViewType   ToVkImageViewType(NkTextureType t, bool cube, uint32 layers);
    static VkSampleCountFlagBits ToVkSamples(NkSampleCount s);
    static VkFilter          ToVkFilter(NkFilter f);
    static VkSamplerMipmapMode ToVkMipFilter(NkMipFilter f);
    static VkSamplerAddressMode ToVkAddressMode(NkAddressMode a);
    static VkCompareOp       ToVkCompareOp(NkCompareOp op);
    static VkBlendFactor     ToVkBlendFactor(NkBlendFactor f);
    static VkBlendOp         ToVkBlendOp(NkBlendOp op);
    static VkPrimitiveTopology ToVkTopology(NkPrimitiveTopology t);
    static VkPolygonMode     ToVkPolygonMode(NkFillMode f);
    static VkCullModeFlags   ToVkCullMode(NkCullMode c);
    static VkFrontFace       ToVkFrontFace(NkFrontFace f);
    static VkStencilOp       ToVkStencilOp(NkStencilOp op);
    static VkShaderStageFlagBits ToVkShaderStage(NkShaderStage s);
    static VkDescriptorType  ToVkDescriptorType(NkDescriptorType t);
    static VkAttachmentLoadOp  ToVkLoadOp(NkLoadOp op);
    static VkAttachmentStoreOp ToVkStoreOp(NkStoreOp op);
    static VkImageLayout     ToVkImageLayout(NkResourceState s);
    static VkVertexInputRate ToVkInputRate(bool instanced);
    static VkFormat          ToVkVertexFormat(NkVertexFormat f);

    uint64 NextId() { return ++mNextId; }
    NkAtomic<uint64> mNextId;

    // Vulkan core
    VkInstance       mInstance       = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice         mDevice         = VK_NULL_HANDLE;
    VkQueue          mGraphicsQueue  = VK_NULL_HANDLE;
    VkQueue          mComputeQueue   = VK_NULL_HANDLE;
    VkQueue          mTransferQueue  = VK_NULL_HANDLE;
    VkQueue          mPresentQueue   = VK_NULL_HANDLE;
    uint32           mGraphicsFamily = UINT32_MAX;
    uint32           mComputeFamily  = UINT32_MAX;
    uint32           mPresentFamily  = UINT32_MAX;
    uint32           mTransferFamily = UINT32_MAX;
    VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

    // Swapchain
    VkSurfaceKHR     mSurface     = VK_NULL_HANDLE;
    VkSwapchainKHR   mSwapchain   = VK_NULL_HANDLE;
    VkFormat         mSwapFormat  = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D       mSwapExtent  {};
    NkVector<VkImage>     mSwapImages;
    NkVector<VkImageView> mSwapViews;
    NkVector<uint64>      mSwapColorTexIds;
    uint32           mCurrentImageIdx = 0;

    // Depth swapchain
    uint64           mDepthTexId = 0;

    // Render pass & FBOs swapchain
    NkRenderPassHandle  mSwapchainRP;
    NkVector<NkFramebufferHandle> mSwapchainFBs;

    // Descriptor pool global
    VkDescriptorPool mDescPool = VK_NULL_HANDLE;

    // Command pool one-shot (uploads)
    VkCommandPool mOneShotPool = VK_NULL_HANDLE;

    // Frame data (triple buffering)
    static constexpr uint32 kMaxFrames = 3;
    NkVkFrameData mFrames[kMaxFrames];
    uint32        mFrameIndex  = 0;
    uint64        mFrameNumber = 0;
    bool          mFrameAcquired = false; // true entre BeginFrame et EndFrame

    // Ressources
    NkUnorderedMap<uint64, NkVkBuffer>       mBuffers;
    NkUnorderedMap<uint64, NkVkTexture>      mTextures;
    NkUnorderedMap<uint64, NkVkSampler>      mSamplers;
    NkUnorderedMap<uint64, NkVkShader>       mShaders;
    NkUnorderedMap<uint64, NkVkPipeline>     mPipelines;
    NkUnorderedMap<uint64, NkVkRenderPass>   mRenderPasses;
    NkUnorderedMap<uint64, NkVkFramebuffer>  mFramebuffers;
    NkUnorderedMap<uint64, NkVkDescSetLayout>mDescLayouts;
    NkUnorderedMap<uint64, NkVkDescSet>      mDescSets;
    NkUnorderedMap<uint64, NkVkFenceObj>     mFences;
    NkUnorderedMap<uint64, VkSemaphore>      mSemaphores;

    mutable NkMutex mMutex;
    NkIGraphicsContext* mCtx = nullptr;
    NkDeviceCaps        mCaps{};
    bool                mIsValid = false;
    uint32              mWidth=0, mHeight=0;
    bool                mOwnsDevice = false;
    bool                mOwnsSwapchain = false;
};

#else
// Stub quand Vulkan n'est pas disponible
class NkVulkanDevice final : public NkIDevice {
public:
    bool Initialize(NkIGraphicsContext*) override { return false; }
    void Shutdown() override {}
    bool IsValid() const override { return false; }
    NkGraphicsApi GetApi() const override { return NkGraphicsApi::NK_API_VULKAN; }
    const NkDeviceCaps& GetCaps() const override { static NkDeviceCaps c; return c; }
    NkBufferHandle  CreateBuffer (const NkBufferDesc&)  override { return {}; }
    void            DestroyBuffer(NkBufferHandle&)       override {}
    bool WriteBuffer(NkBufferHandle,const void*,uint64,uint64) override { return false; }
    bool WriteBufferAsync(NkBufferHandle,const void*,uint64,uint64) override { return false; }
    bool ReadBuffer(NkBufferHandle,void*,uint64,uint64)  override { return false; }
    NkMappedMemory MapBuffer(NkBufferHandle,uint64,uint64) override { return {}; }
    void UnmapBuffer(NkBufferHandle) override {}
    NkTextureHandle CreateTexture(const NkTextureDesc&)  override { return {}; }
    void DestroyTexture(NkTextureHandle&)                override {}
    bool WriteTexture(NkTextureHandle,const void*,uint32) override { return false; }
    bool WriteTextureRegion(NkTextureHandle,const void*,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32) override { return false; }
    bool GenerateMipmaps(NkTextureHandle,NkFilter) override { return false; }
    NkSamplerHandle CreateSampler(const NkSamplerDesc&) override { return {}; }
    void DestroySampler(NkSamplerHandle&) override {}
    NkShaderHandle  CreateShader(const NkShaderDesc&)   override { return {}; }
    void DestroyShader(NkShaderHandle&) override {}
    NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc&) override { return {}; }
    NkPipelineHandle CreateComputePipeline(const NkComputePipelineDesc&)   override { return {}; }
    void DestroyPipeline(NkPipelineHandle&) override {}
    NkRenderPassHandle  CreateRenderPass(const NkRenderPassDesc&)   override { return {}; }
    void                DestroyRenderPass(NkRenderPassHandle&)       override {}
    NkFramebufferHandle CreateFramebuffer(const NkFramebufferDesc&) override { return {}; }
    void                DestroyFramebuffer(NkFramebufferHandle&)     override {}
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return {}; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return {}; }
    NkGpuFormat GetSwapchainFormat()      const override { return NkGpuFormat::NK_BGRA8_SRGB; }
    NkGpuFormat GetSwapchainDepthFormat() const override { return NkGpuFormat::NK_D32_FLOAT; }
    uint32   GetSwapchainWidth()       const override { return 0; }
    uint32   GetSwapchainHeight()      const override { return 0; }
    NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc&) override { return {}; }
    void            DestroyDescriptorSetLayout(NkDescSetHandle&) override {}
    NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle) override { return {}; }
    void            FreeDescriptorSet(NkDescSetHandle&)    override {}
    void            UpdateDescriptorSets(const NkDescriptorWrite*,uint32) override {}
    NkICommandBuffer* CreateCommandBuffer(NkCommandBufferType) override { return nullptr; }
    void DestroyCommandBuffer(NkICommandBuffer*&) override {}
    void Submit(NkICommandBuffer* const*,uint32,NkFenceHandle) override {}
    void SubmitAndPresent(NkICommandBuffer*) override {}
    NkFenceHandle CreateFence(bool) override { return {}; }
    void DestroyFence(NkFenceHandle&) override {}
    bool WaitFence(NkFenceHandle,uint64) override { return false; }
    bool IsFenceSignaled(NkFenceHandle)  override { return false; }
    void ResetFence(NkFenceHandle) override {}
    void WaitIdle() override {}
    void BeginFrame(NkFrameContext&) override {}
    void EndFrame(NkFrameContext&)   override {}
    uint32 GetFrameIndex()        const override { return 0; }
    uint32 GetMaxFramesInFlight() const override { return 1; }
    uint64 GetFrameNumber()       const override { return 0; }
    void OnResize(uint32,uint32)   override {}
    void* GetNativeDevice()        const override { return nullptr; }
    void* GetNativeCommandQueue()  const override { return nullptr; }
    NkISwapchain*     CreateSwapchain(NkIGraphicsContext*,const NkSwapchainDesc&) override { return nullptr; }
    void              DestroySwapchain(NkISwapchain*& sc) override { (void)sc; }
    NkSemaphoreHandle CreateGpuSemaphore()                   override { return {}; }
    void              DestroySemaphore(NkSemaphoreHandle&) override {}
};
#endif // NK_RHI_VK_ENABLED

} // namespace nkentseu

