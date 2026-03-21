#pragma once
// =============================================================================
// NkSoftwareDevice.h — Backend Software Rasterizer (CPU)
// Version corrigée et complète.
//
// Changements vs version originale :
//  - Utilise NkSoftwareTypes.h pour tous les types internes
//  - NkSWVertex unifié (NkVec4f/3f/2f au lieu de NkSWVec4/3/2)
//  - NkSWRasterizer complet avec clipping homogène
//  - Present() copie dans NkSoftwareContext::GetBackBuffer(), puis EndFrame/Present
//  - Pas de NkNativeContext::GetSoftwareBackBuffer() (n'existe pas)
//  - FBO courant géré dans Execute() du command buffer
//  - cpuFragFn/cpuVertFn dans NkShaderDesc pour passer les lambdas
// =============================================================================
#include "NkSoftwareTypes.h"
#include "NkSoftwareRasterizer.h"
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"

namespace nkentseu {

class NkSoftwareCommandBuffer;
class NkSoftwareSwapchain;

// =============================================================================
class NkSoftwareDevice final : public NkIDevice {
public:
    NkSoftwareDevice()  = default;
    ~NkSoftwareDevice() override;

    bool          Initialize(NkIGraphicsContext* ctx) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_SOFTWARE; }
    const NkDeviceCaps& GetCaps()               const override { return mCaps; }

    // ── Buffers ───────────────────────────────────────────────────────────────
    NkBufferHandle  CreateBuffer (const NkBufferDesc&)         override;
    void            DestroyBuffer(NkBufferHandle&)             override;
    bool WriteBuffer     (NkBufferHandle,const void*,uint64,uint64) override;
    bool WriteBufferAsync(NkBufferHandle,const void*,uint64,uint64) override;
    bool ReadBuffer      (NkBufferHandle,void*,uint64,uint64)       override;
    NkMappedMemory MapBuffer  (NkBufferHandle,uint64,uint64)        override;
    void           UnmapBuffer(NkBufferHandle)                      override;

    // ── Textures ──────────────────────────────────────────────────────────────
    NkTextureHandle  CreateTexture (const NkTextureDesc&)      override;
    void             DestroyTexture(NkTextureHandle&)           override;
    bool WriteTexture(NkTextureHandle,const void*,uint32)       override;
    bool WriteTextureRegion(NkTextureHandle,const void*,
                            uint32,uint32,uint32,uint32,uint32,
                            uint32,uint32,uint32,uint32)        override;
    bool GenerateMipmaps(NkTextureHandle,NkFilter)              override;

    // ── Samplers ──────────────────────────────────────────────────────────────
    NkSamplerHandle  CreateSampler (const NkSamplerDesc&)      override;
    void             DestroySampler(NkSamplerHandle&)           override;

    // ── Shaders ───────────────────────────────────────────────────────────────
    NkShaderHandle   CreateShader (const NkShaderDesc&)        override;
    void             DestroyShader(NkShaderHandle&)             override;

    // ── Pipelines ─────────────────────────────────────────────────────────────
    NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc&) override;
    NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc&)  override;
    void             DestroyPipeline(NkPipelineHandle&)                    override;

    // ── Render Passes & Framebuffers ──────────────────────────────────────────
    NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc&)   override;
    void                DestroyRenderPass (NkRenderPassHandle&)        override;
    NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc&)  override;
    void                DestroyFramebuffer(NkFramebufferHandle&)       override;

    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFB; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
    NkGpuFormat GetSwapchainFormat()      const override { return NkGpuFormat::NK_RGBA8_UNORM; }
    NkGpuFormat GetSwapchainDepthFormat() const override { return NkGpuFormat::NK_D32_FLOAT;   }
    uint32 GetSwapchainWidth()  const override { return mWidth;  }
    uint32 GetSwapchainHeight() const override { return mHeight; }

    // ── Descriptor Sets ───────────────────────────────────────────────────────
    NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc&) override;
    void            DestroyDescriptorSetLayout(NkDescSetHandle&)                override;
    NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)         override;
    void            FreeDescriptorSet    (NkDescSetHandle&)                     override;
    void            UpdateDescriptorSets(const NkDescriptorWrite*, uint32)      override;

    // ── Command Buffers ───────────────────────────────────────────────────────
    NkICommandBuffer* CreateCommandBuffer (NkCommandBufferType)  override;
    void              DestroyCommandBuffer(NkICommandBuffer*&)   override;

    // ── Submit ────────────────────────────────────────────────────────────────
    void Submit(NkICommandBuffer* const*, uint32, NkFenceHandle) override;
    void SubmitAndPresent(NkICommandBuffer*)                      override;

    // ── Fences ────────────────────────────────────────────────────────────────
    NkFenceHandle CreateFence(bool signaled) override;
    void          DestroyFence(NkFenceHandle&) override;
    bool          WaitFence(NkFenceHandle,uint64) override;
    bool          IsFenceSignaled(NkFenceHandle)  override;
    void          ResetFence(NkFenceHandle)       override;
    void          WaitIdle() override {}

    // ── Swapchain ─────────────────────────────────────────────────────────────
    NkISwapchain* CreateSwapchain (NkIGraphicsContext*,const NkSwapchainDesc&) override;
    void          DestroySwapchain(NkISwapchain*&)                             override;

    // ── Semaphores ────────────────────────────────────────────────────────────
    NkSemaphoreHandle CreateGpuSemaphore()                    override;
    void              DestroySemaphore(NkSemaphoreHandle&)    override;

    // ── Frame ─────────────────────────────────────────────────────────────────
    void   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return 1; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }
    void   OnResize(uint32 w, uint32 h) override;

    void*  GetNativeDevice()       const override { return nullptr; }
    void*  GetNativeCommandQueue() const override { return nullptr; }

    // ── Accès interne (utilisé par NkSoftwareCommandBuffer) ───────────────────
    NkSWBuffer*      GetBuf     (uint64 id);
    NkSWTexture*     GetTex     (uint64 id);
    NkSWSampler*     GetSamp    (uint64 id);
    NkSWShader*      GetShader  (uint64 id);
    NkSWPipeline*    GetPipe    (uint64 id);
    NkSWDescSet*     GetDescSet (uint64 id);
    NkSWFramebuffer* GetFBO     (uint64 id);
    NkSWRenderPass*  GetRP      (uint64 id);

    // ── Présentation ──────────────────────────────────────────────────────────
    // Copie le backbuffer RHI vers le backbuffer du NkSoftwareContext, puis
    // declenche EndFrame/Present cote contexte.
    void Present();

    // Accès direct aux pixels du backbuffer (lecture seule — pour debug/screenshots)
    const uint8* BackbufferPixels() const;
    uint32 BackbufferWidth()  const { return mWidth; }
    uint32 BackbufferHeight() const { return mHeight; }

    // ── Accès au contexte (pour NkSoftwareSwapchain) ──────────────────────────
    NkIGraphicsContext* GetContext() const { return mCtx; }

private:
    void   CreateSwapchainObjects();
    uint64 NextId() { return mNextId.FetchAdd(1) + 1; }

    NkAtomic<uint64> mNextId { 0 };

    NkUnorderedMap<uint64, NkSWBuffer>        mBuffers;
    NkUnorderedMap<uint64, NkSWTexture>       mTextures;
    NkUnorderedMap<uint64, NkSWSampler>       mSamplers;
    NkUnorderedMap<uint64, NkSWShader>        mShaders;
    NkUnorderedMap<uint64, NkSWPipeline>      mPipelines;
    NkUnorderedMap<uint64, NkSWRenderPass>    mRenderPasses;
    NkUnorderedMap<uint64, NkSWFramebuffer>   mFramebuffers;
    NkUnorderedMap<uint64, NkSWDescSetLayout> mDescLayouts;
    NkUnorderedMap<uint64, NkSWDescSet>       mDescSets;
    NkUnorderedMap<uint64, NkSWFence>         mFences;
    NkUnorderedMap<uint64, NkSWSemaphore>     mSemaphores;

    NkFramebufferHandle mSwapchainFB;
    NkRenderPassHandle  mSwapchainRP;

    mutable NkMutex     mMutex;
    NkIGraphicsContext* mCtx     = nullptr;
    NkDeviceCaps        mCaps    {};
    bool                mIsValid = false;
    uint32              mWidth   = 0;
    uint32              mHeight  = 0;
    uint32              mFrameIndex  = 0;
    uint64              mFrameNumber = 0;
};

} // namespace nkentseu


