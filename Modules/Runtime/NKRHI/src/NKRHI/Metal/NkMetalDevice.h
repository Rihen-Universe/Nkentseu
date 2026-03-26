#pragma once
// =============================================================================
// NkRHI_Device_Metal.h — Backend Apple Metal (macOS / iOS)
// Utilise l'API Metal via Objective-C++ (.mm)
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"

#ifdef NK_RHI_METAL_ENABLED
#ifdef __OBJC__
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace nkentseu {

class NkMetalCommandBuffer;

// Wrappers opaques (compatibles C++ pur si pas en .mm)
#ifdef __OBJC__
using NkMTLDevice           = id<MTLDevice>;
using NkMTLCommandQueue     = id<MTLCommandQueue>;
using NkMTLBuffer           = id<MTLBuffer>;
using NkMTLTexture          = id<MTLTexture>;
using NkMTLSamplerState     = id<MTLSamplerState>;
using NkMTLRenderPipelineState   = id<MTLRenderPipelineState>;
using NkMTLComputePipelineState  = id<MTLComputePipelineState>;
using NkMTLDepthStencilState     = id<MTLDepthStencilState>;
using NkMTLFunction          = id<MTLFunction>;
using NkMTLLibrary           = id<MTLLibrary>;
using NkCAMetalLayer         = CAMetalLayer*;
using NkCAMetalDrawable      = id<CAMetalDrawable>;
#else
using NkMTLDevice            = void*;
using NkMTLCommandQueue      = void*;
using NkMTLBuffer            = void*;
using NkMTLTexture           = void*;
using NkMTLSamplerState      = void*;
using NkMTLRenderPipelineState   = void*;
using NkMTLComputePipelineState  = void*;
using NkMTLDepthStencilState     = void*;
using NkMTLFunction          = void*;
using NkMTLLibrary           = void*;
using NkCAMetalLayer         = void*;
using NkCAMetalDrawable      = void*;
#endif

struct NkMetalBuffer  {
    NkMTLBuffer buf = nullptr;
    NkBufferDesc desc;
};
struct NkMetalTexture {
    NkMTLTexture tex = nullptr;
    NkTextureDesc desc;
    bool isSwapchain = false;
};
struct NkMetalSampler { NkMTLSamplerState ss = nullptr; };

struct NkMetalShader {
    NkMTLFunction vert = nullptr;
    NkMTLFunction frag = nullptr;
    NkMTLFunction comp = nullptr;
};

struct NkMetalPipeline {
    NkMTLRenderPipelineState  rpso = nullptr;
    NkMTLComputePipelineState cpso = nullptr;
    NkMTLDepthStencilState    dss  = nullptr;
    bool isCompute = false;
    // Rasterizer state (Metal n'a pas d'objet RS, stocker pour application manuelle)
    bool  frontFaceCCW = true;
    int   cullMode     = 0; // 0=none,1=front,2=back
    bool  depthClip    = true;
    float depthBiasConst=0, depthBiasSlope=0, depthBiasClamp=0;
};

struct NkMetalRenderPass   { NkRenderPassDesc desc; };
struct NkMetalFramebuffer  {
    NkTextureHandle colorAttachments[8];
    uint32 colorCount = 0;
    NkTextureHandle depthAttachment;
    uint32 w=0, h=0;
};
struct NkMetalDescSetLayout { NkDescriptorSetLayoutDesc desc; };
struct NkMetalDescSet {
    struct Binding { uint32 slot=0; NkDescriptorType type{}; uint64 bufId=0; uint64 texId=0; uint64 sampId=0; };
    NkVector<Binding> bindings;
    uint64 layoutId=0;
};
struct NkMetalFence { bool signaled=false; };

// =============================================================================
class NkMetalDevice final : public NkIDevice {
public:
    NkMetalDevice()  = default;
    ~NkMetalDevice() override;

    bool          Initialize(const NkDeviceInitInfo& init) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_METAL; }
    const NkDeviceCaps& GetCaps()               const override { return mCaps; }

    NkBufferHandle  CreateBuffer (const NkBufferDesc& d)                      override;
    void            DestroyBuffer(NkBufferHandle& h)                          override;
    bool WriteBuffer(NkBufferHandle,const void*,uint64,uint64)                override;
    bool WriteBufferAsync(NkBufferHandle,const void*,uint64,uint64)           override;
    bool ReadBuffer(NkBufferHandle,void*,uint64,uint64)                       override;
    NkMappedMemory MapBuffer(NkBufferHandle,uint64,uint64)                    override;
    void           UnmapBuffer(NkBufferHandle)                                override;

    NkTextureHandle  CreateTexture (const NkTextureDesc& d)                   override;
    void             DestroyTexture(NkTextureHandle& h)                        override;
    bool WriteTexture(NkTextureHandle,const void*,uint32)                     override;
    bool WriteTextureRegion(NkTextureHandle,const void*,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32,uint32) override;
    bool GenerateMipmaps(NkTextureHandle, NkFilter)                           override;

    NkSamplerHandle  CreateSampler (const NkSamplerDesc& d)                   override;
    void             DestroySampler(NkSamplerHandle& h)                        override;

    NkShaderHandle   CreateShader (const NkShaderDesc& d)                     override;
    void             DestroyShader(NkShaderHandle& h)                          override;

    NkPipelineHandle CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d)  override;
    NkPipelineHandle CreateComputePipeline (const NkComputePipelineDesc& d)   override;
    void             DestroyPipeline(NkPipelineHandle& h)                     override;

    NkRenderPassHandle  CreateRenderPass  (const NkRenderPassDesc& d)         override;
    void                DestroyRenderPass (NkRenderPassHandle& h)              override;
    NkFramebufferHandle CreateFramebuffer (const NkFramebufferDesc& d)        override;
    void                DestroyFramebuffer(NkFramebufferHandle& h)             override;
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFB; }
    NkRenderPassHandle  GetSwapchainRenderPass()  const override { return mSwapchainRP; }
    NkGPUFormat GetSwapchainFormat()      const override { return NkGPUFormat::NK_BGRA8_SRGB; }
    NkGPUFormat GetSwapchainDepthFormat() const override { return NkGPUFormat::NK_D32_FLOAT; }
    uint32   GetSwapchainWidth()       const override { return mWidth; }
    uint32   GetSwapchainHeight()      const override { return mHeight; }

    NkDescSetHandle CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) override;
    void            DestroyDescriptorSetLayout(NkDescSetHandle& h)                override;
    NkDescSetHandle AllocateDescriptorSet(NkDescSetHandle layoutHandle)           override;
    void            FreeDescriptorSet    (NkDescSetHandle& h)                     override;
    void            UpdateDescriptorSets(const NkDescriptorWrite* w, uint32 n)   override;

    NkICommandBuffer* CreateCommandBuffer(NkCommandBufferType t)                  override;
    void              DestroyCommandBuffer(NkICommandBuffer*& cb)                 override;

    void Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence)     override;
    void SubmitAndPresent(NkICommandBuffer* cb)                                   override;
    NkFenceHandle CreateFence(bool signaled)  override;
    void DestroyFence(NkFenceHandle& h)       override;
    bool WaitFence(NkFenceHandle f,uint64 to) override;
    bool IsFenceSignaled(NkFenceHandle f)     override;
    void ResetFence(NkFenceHandle f)          override;
    void WaitIdle()                           override;

    bool   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return MAX_FRAMES; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }
    void   OnResize(uint32 w, uint32 h) override;

    void* GetNativeDevice()       const override { return mDevice; }
    void* GetNativeCommandQueue() const override { return mQueue; }

    // Accès interne
    NkMTLDevice         MtlDevice()  const { return mDevice; }
    NkMTLCommandQueue   MtlQueue()   const { return mQueue;  }
    NkMTLBuffer         GetMTLBuffer (uint64 id) const;
    NkMTLTexture        GetMTLTexture(uint64 id) const;
    NkMTLSamplerState   GetMTLSampler(uint64 id) const;
    const NkMetalPipeline*   GetPipeline(uint64 id) const;
    const NkMetalDescSet*    GetDescSet (uint64 id) const;
    const NkMetalFramebuffer* GetFBO    (uint64 id) const;
    NkCAMetalDrawable   CurrentDrawable() const { return mCurrentDrawable; }

private:
    void CreateSwapchainObjects();
    void QueryCaps();

    uint64 NextId() { return ++mNextId; }
    NkAtomic<uint64> mNextId{0};

    NkMTLDevice        mDevice   = nullptr;
    NkMTLCommandQueue  mQueue    = nullptr;
    NkCAMetalLayer     mLayer    = nullptr;
    NkCAMetalDrawable  mCurrentDrawable = nullptr;

    NkFramebufferHandle mSwapchainFB;
    NkRenderPassHandle  mSwapchainRP;
    NkTextureHandle     mDepthTex;

    NkUnorderedMap<uint64, NkMetalBuffer>       mBuffers;
    NkUnorderedMap<uint64, NkMetalTexture>      mTextures;
    NkUnorderedMap<uint64, NkMetalSampler>      mSamplers;
    NkUnorderedMap<uint64, NkMetalShader>       mShaders;
    NkUnorderedMap<uint64, NkMetalPipeline>     mPipelines;
    NkUnorderedMap<uint64, NkMetalRenderPass>   mRenderPasses;
    NkUnorderedMap<uint64, NkMetalFramebuffer>  mFramebuffers;
    NkUnorderedMap<uint64, NkMetalDescSetLayout>mDescLayouts;
    NkUnorderedMap<uint64, NkMetalDescSet>      mDescSets;
    NkUnorderedMap<uint64, NkMetalFence>        mFences;

    mutable threading::NkMutex  mMutex;
    NkDeviceInitInfo    mInit   {};
    NkDeviceCaps        mCaps   {};
    bool                mIsValid= false;
    uint32              mWidth=0, mHeight=0;
    uint32              mFrameIndex  = 0;
    uint64              mFrameNumber = 0;
    static constexpr uint32 MAX_FRAMES = 3;
};

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
