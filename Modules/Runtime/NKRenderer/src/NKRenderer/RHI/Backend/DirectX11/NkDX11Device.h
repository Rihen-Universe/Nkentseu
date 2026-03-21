#pragma once
// =============================================================================
// NkRHI_Device_DX11.h — Backend DirectX 11.1
// =============================================================================
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"

#ifdef NK_RHI_DX11_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

namespace nkentseu {
using threading::NkMutex;
using threading::NkScopedLock;

class NkCommandBuffer_DX11;

struct NkDX11Buffer  { ID3D11Buffer*  buf=nullptr; NkBufferDesc  desc; };
struct NkDX11Texture {
    ID3D11Texture2D*          tex=nullptr;
    ID3D11ShaderResourceView* srv=nullptr;
    ID3D11RenderTargetView*   rtv=nullptr;
    ID3D11DepthStencilView*   dsv=nullptr;
    ID3D11UnorderedAccessView*uav=nullptr;
    NkTextureDesc             desc;
    bool                      isSwapchain=false;
};
struct NkDX11Sampler { ID3D11SamplerState* ss=nullptr; };
struct NkDX11Shader  {
    ID3D11VertexShader*   vs=nullptr;
    ID3D11PixelShader*    ps=nullptr;
    ID3D11ComputeShader*  cs=nullptr;
    ID3D11GeometryShader* gs=nullptr;
    ID3DBlob* vsBlob=nullptr; // pour créer InputLayout
};
struct NkDX11Pipeline {
    ID3D11VertexShader*        vs=nullptr;
    ID3D11PixelShader*         ps=nullptr;
    ID3D11ComputeShader*       cs=nullptr;
    ID3D11InputLayout*         il=nullptr;
    ID3D11RasterizerState1*    rs=nullptr;
    ID3D11DepthStencilState*   dss=nullptr;
    ID3D11BlendState1*         bs=nullptr;
    D3D11_PRIMITIVE_TOPOLOGY   topology=D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bool                       isCompute=false;
    uint32                     strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT]={};
};
struct NkDX11RenderPass   { NkRenderPassDesc desc; };
struct NkDX11Framebuffer  {
    ID3D11RenderTargetView* rtvs[8]={};
    uint32 rtvCount=0;
    ID3D11DepthStencilView* dsv=nullptr;
    uint32 w=0, h=0;
};
struct NkDX11DescSetLayout { NkDescriptorSetLayoutDesc desc; };
struct NkDX11DescSet {
    struct Slot {
        enum Kind { None, Buffer, Texture, Sampler } kind=None;
        ID3D11Buffer*              buf=nullptr;
        ID3D11ShaderResourceView*  srv=nullptr;
        ID3D11UnorderedAccessView* uav=nullptr;
        ID3D11SamplerState*        ss =nullptr;
        uint32 slot=0;
        NkDescriptorType type{};
    };
    Slot slots[32]; uint32 count=0;
    uint64 layoutId=0;
};
struct NkDX11FenceObj { uint64 cpuValue=0; bool signaled=false; };

// =============================================================================
class NkDeviceDX11 final : public NkIDevice {
public:
    NkDeviceDX11()  = default;
    ~NkDeviceDX11() override;

    bool          Initialize(NkIGraphicsContext* ctx) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_DIRECTX11; }
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
    NkGpuFormat GetSwapchainFormat()      const override { return NkGpuFormat::NK_BGRA8_SRGB; }
    NkGpuFormat GetSwapchainDepthFormat() const override { return NkGpuFormat::NK_D32_FLOAT; }
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

    void   BeginFrame(NkFrameContext& frame) override;
    void   EndFrame  (NkFrameContext& frame) override;
    uint32 GetFrameIndex()        const override { return mFrameIndex; }
    uint32 GetMaxFramesInFlight() const override { return kMaxFrames; }
    uint64 GetFrameNumber()       const override { return mFrameNumber; }
    void   OnResize(uint32 w, uint32 h) override;

    void* GetNativeDevice()       const override { return mDevice; }
    void* GetNativeCommandQueue() const override { return mContext; }

    // ── Swapchain & Semaphores (NkIDevice virtuals) ───────────────────────────
    NkISwapchain*     CreateSwapchain(NkIGraphicsContext*, const NkSwapchainDesc&) override;
    void              DestroySwapchain(NkISwapchain*& sc)              override;
    NkSemaphoreHandle CreateGpuSemaphore()                                override;
    void              DestroySemaphore(NkSemaphoreHandle& h)           override;

    // Accès interne
    ID3D11Device1*        D3D()   const { return mDevice; }
    ID3D11DeviceContext1* Ctx()   const { return mContext; }
    ID3D11Buffer*  GetDXBuffer (uint64 id) const;
    ID3D11ShaderResourceView*  GetSRV(uint64 id) const;
    ID3D11UnorderedAccessView* GetUAV(uint64 id) const;
    const NkDX11Pipeline*  GetPipeline(uint64 id) const;
    const NkDX11DescSet*   GetDescSet (uint64 id) const;
    const NkDX11Framebuffer* GetFBO   (uint64 id) const;
    void  ApplyDescSet(uint64 id);
    void  ApplyPipeline(uint64 id);

private:
    void CreateSwapchain(uint32 w, uint32 h);
    void DestroySwapchain();
    void ResizeSwapchain(uint32 w, uint32 h);
    void QueryCaps();

    static DXGI_FORMAT      ToDXGIFormat(NkGpuFormat f, bool uav=false);
    static D3D11_FILTER     ToDX11Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool compare);
    static D3D11_TEXTURE_ADDRESS_MODE ToDX11Address(NkAddressMode a);
    static D3D11_COMPARISON_FUNC      ToDX11Compare(NkCompareOp op);
    static D3D11_BLEND                ToDX11Blend(NkBlendFactor f);
    static D3D11_BLEND_OP             ToDX11BlendOp(NkBlendOp op);
    static D3D11_STENCIL_OP           ToDX11StencilOp(NkStencilOp op);
    static D3D11_PRIMITIVE_TOPOLOGY   ToDX11Topology(NkPrimitiveTopology t);
    static const char*                ToDX11SemanticName(const char* s);

    uint64 NextId() { return ++mNextId; }
    NkAtomic<uint64> mNextId;

    ID3D11Device1*           mDevice    = nullptr;
    ID3D11DeviceContext1*    mContext   = nullptr;
    IDXGISwapChain1*         mSwapchain = nullptr;
    IDXGIFactory2*           mFactory   = nullptr;

    NkFramebufferHandle mSwapchainFB;
    NkRenderPassHandle  mSwapchainRP;

    NkUnorderedMap<uint64, NkDX11Buffer>       mBuffers;
    NkUnorderedMap<uint64, NkDX11Texture>      mTextures;
    NkUnorderedMap<uint64, NkDX11Sampler>      mSamplers;
    NkUnorderedMap<uint64, NkDX11Shader>       mShaders;
    NkUnorderedMap<uint64, NkDX11Pipeline>     mPipelines;
    NkUnorderedMap<uint64, NkDX11RenderPass>   mRenderPasses;
    NkUnorderedMap<uint64, NkDX11Framebuffer>  mFramebuffers;
    NkUnorderedMap<uint64, NkDX11DescSetLayout>mDescLayouts;
    NkUnorderedMap<uint64, NkDX11DescSet>      mDescSets;
    NkUnorderedMap<uint64, NkDX11FenceObj>     mFences;

    mutable NkMutex mMutex;
    NkIGraphicsContext* mCtx    = nullptr;
    NkDeviceCaps        mCaps   {};
    bool                mIsValid= false;
    uint32              mWidth=0, mHeight=0;
    uint32              mFrameIndex  = 0;
    uint64              mFrameNumber = 0;
    static constexpr uint32 kMaxFrames = 3;
    HWND                mHwnd   = nullptr;
};

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED

