#pragma once
// =============================================================================
// NkRHI_Device_DX12.h — Backend DirectX 12
// D3D12 avec descriptor heaps, command allocators par frame,
// resource barriers explicites et pipeline state objects.
// =============================================================================
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include "NKContainers/Sequential/NkVector.h"
#include <queue>
#include "NKContainers/Functional/NkFunction.h"

#ifdef NK_RHI_DX12_ENABLED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

namespace nkentseu {

class NkDirectX12CommandBuffer;

// =============================================================================
// Descriptor Heap Manager (CPU + GPU ranges)
// =============================================================================
struct NkDX12DescHeap {
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  cpuBase{};
    D3D12_GPU_DESCRIPTOR_HANDLE  gpuBase{};
    UINT                         incrementSize = 0;
    UINT                         capacity      = 0;
    UINT                         allocated     = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE AllocCPU() {
        D3D12_CPU_DESCRIPTOR_HANDLE h = cpuBase;
        h.ptr += (SIZE_T)allocated * incrementSize;
        allocated++;
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE GPUFrom(UINT idx) const {
        D3D12_GPU_DESCRIPTOR_HANDLE h = gpuBase;
        h.ptr += (UINT64)idx * incrementSize;
        return h;
    }
    UINT IndexOf(D3D12_CPU_DESCRIPTOR_HANDLE h) const {
        return (UINT)((h.ptr - cpuBase.ptr) / incrementSize);
    }
};

// =============================================================================
// Internal resource structs
// =============================================================================
struct NkDX12Buffer {
    ComPtr<ID3D12Resource> resource;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
    void*    mapped = nullptr;  // persistently mapped si upload/readback
    NkBufferDesc desc;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    UINT cbvIdx = UINT_MAX;  // index dans le descriptor heap CBV/SRV/UAV
    UINT srvIdx = UINT_MAX;
    UINT uavIdx = UINT_MAX;
};

struct NkDX12Texture {
    ComPtr<ID3D12Resource> resource;
    NkTextureDesc          desc;
    UINT rtvIdx   = UINT_MAX;
    UINT dsvIdx   = UINT_MAX;
    UINT srvIdx   = UINT_MAX;
    UINT uavIdx   = UINT_MAX;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    bool isSwapchain = false;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
};

struct NkDX12Sampler {
    UINT heapIdx = UINT_MAX;
};

struct NkDX12ShaderStage {
    NkVector<uint8> bytecode;  // DXBC ou DXIL
    D3D12_SHADER_BYTECODE bc() const {
        return { bytecode.Data(), static_cast<SIZE_T>(bytecode.Size()) };
    }
};

struct NkDX12Shader {
    NkDX12ShaderStage vs, ps, cs, gs, hs, ds;
    ComPtr<ID3DBlob>  vsBlob; // pour InputLayout
};

struct NkDX12Pipeline {
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12RootSignature> rootSig;
    bool isCompute = false;
    D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint32 vertexStrides[D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
};

struct NkDX12RenderPass  { NkRenderPassDesc desc; };
struct NkDX12Framebuffer {
    UINT rtvIdxs[8]  = {};
    uint64 colorTexIds[8] = {};
    UINT rtvCount    = 0;
    UINT dsvIdx      = UINT_MAX;
    uint64 depthTexId = 0;
    uint32 w = 0, h  = 0;
};

struct NkDX12DescSetLayout { NkDescriptorSetLayoutDesc desc; };
struct NkDX12DescSet {
    struct Binding {
        uint32 slot  = 0;
        NkDescriptorType type{};
        uint64 bufId = 0;
        uint64 texId = 0;
        uint64 sampId= 0;
    };
    NkVector<Binding> bindings;
    uint64 layoutId = 0;
};

// =============================================================================
// Frame data (triple buffering avec command allocators séparés)
// =============================================================================
struct NkDX12FrameData {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Fence>            fence;
    UINT64                         fenceValue = 0;
    HANDLE                         fenceEvent = nullptr;

    void WaitAndReset(ID3D12CommandQueue* queue) {
        if (fence->GetCompletedValue() < fenceValue) {
            fence->SetEventOnCompletion(fenceValue, fenceEvent);
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        allocator->Reset();
    }
    void Signal(ID3D12CommandQueue* queue, UINT64 val) {
        fenceValue = val;
        queue->Signal(fence.Get(), val);
    }
};

// =============================================================================
// NkDirectX12Device
// =============================================================================
class NkDirectX12Device final : public NkIDevice {
public:
    NkDirectX12Device()  = default;
    ~NkDirectX12Device() override;

    bool          Initialize(const NkDeviceInitInfo& init) override;
    void          Shutdown()                          override;
    bool          IsValid()                     const override { return mIsValid; }
    NkGraphicsApi GetApi()                      const override { return NkGraphicsApi::NK_API_DIRECTX12; }
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
    NkFramebufferHandle GetSwapchainFramebuffer() const override { return mSwapchainFBs[mBackBufferIdx]; }
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

    void* GetNativeDevice()       const override { return mDevice.Get(); }
    void* GetNativeCommandQueue() const override { return mGraphicsQueue.Get(); }

    // ── Accès interne pour NkDirectX12CommandBuffer ──────────────────────────────
    ID3D12Device*           Dev()   const { return mDevice.Get(); }
    ID3D12CommandQueue*     Queue() const { return mGraphicsQueue.Get(); }
    ID3D12CommandAllocator* FrameAlloc() const { return mFrameData[mFrameIndex].allocator.Get(); }

    ID3D12Resource*     GetDX12Buffer (uint64 id) const;
    ID3D12Resource*     GetDX12Texture(uint64 id) const;
    D3D12_GPU_VIRTUAL_ADDRESS GetBufferGPUAddr(uint64 id) const;
    UINT GetBufferCbvIndex(uint64 id) const;
    UINT GetBufferSrvIndex(uint64 id) const;
    UINT GetBufferUavIndex(uint64 id) const;
    UINT GetTextureSrvIndex(uint64 id) const;
    UINT GetTextureUavIndex(uint64 id) const;
    UINT GetSamplerHeapIndex(uint64 id) const;
    bool IsSwapchainTexture(uint64 id) const;

    const NkDX12Pipeline*   GetPipeline(uint64 id) const;
    const NkDX12DescSet*    GetDescSet (uint64 id) const;
    const NkDX12Framebuffer* GetFBO    (uint64 id) const;

    NkDX12DescHeap& RtvHeap()     { return mRtvHeap; }
    NkDX12DescHeap& DsvHeap()     { return mDsvHeap; }
    NkDX12DescHeap& CbvSrvUavHeap() { return mCbvSrvUavHeap; }
    NkDX12DescHeap& SamplerHeap() { return mSamplerHeap; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(UINT idx) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(UINT idx) const;
    void TransitionResource(ID3D12GraphicsCommandList* cmd,
                             ID3D12Resource* res,
                             D3D12_RESOURCE_STATES from,
                             D3D12_RESOURCE_STATES to);
    void TransitionTextureState(ID3D12GraphicsCommandList* cmd,
                                uint64 textureId,
                                D3D12_RESOURCE_STATES to);

    // Execute one-shot (pour uploads)
    void ExecuteOneShot(NkFunction<void(ID3D12GraphicsCommandList*)> fn);

private:
    void CreateSwapchain(uint32 w, uint32 h);
    void DestroySwapchain();
    void ResizeSwapchain(uint32 w, uint32 h);
    void QueryCaps();
    void InitDescriptorHeaps();
    ComPtr<ID3D12RootSignature> CreateDefaultRootSignature(bool compute);

    static DXGI_FORMAT     ToDXGIFormat(NkGPUFormat f);
    static DXGI_FORMAT     ToDXGIVertexFormat(NkVertexFormat f);
    static D3D12_COMPARISON_FUNC     ToDX12Compare(NkCompareOp op);
    static D3D12_BLEND               ToDX12Blend(NkBlendFactor f);
    static D3D12_BLEND_OP            ToDX12BlendOp(NkBlendOp op);
    static D3D12_STENCIL_OP          ToDX12StencilOp(NkStencilOp op);
    static D3D12_PRIMITIVE_TOPOLOGY  ToDX12Topology(NkPrimitiveTopology t);
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12TopologyType(NkPrimitiveTopology t);
    static D3D12_FILTER              ToDX12Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp);
    static D3D12_TEXTURE_ADDRESS_MODE ToDX12Address(NkAddressMode a);
    static D3D12_RESOURCE_STATES     ToDX12State(NkResourceState s);

    uint64 NextId() { return ++mNextId; }
    NkAtomic<uint64> mNextId{0};

    ComPtr<ID3D12Device>       mDevice;
    ComPtr<ID3D12CommandQueue> mGraphicsQueue;
    ComPtr<ID3D12CommandQueue> mComputeQueue;
    ComPtr<IDXGISwapChain4>    mSwapchain;
    ComPtr<IDXGIFactory6>      mFactory;

    // Descriptor heaps
    NkDX12DescHeap mRtvHeap;
    NkDX12DescHeap mDsvHeap;
    NkDX12DescHeap mCbvSrvUavHeap;
    NkDX12DescHeap mSamplerHeap;

    // Swapchain
    NkVector<NkFramebufferHandle> mSwapchainFBs;
    NkRenderPassHandle mSwapchainRP;
    UINT               mBackBufferIdx = 0;
    uint64             mDepthTexId    = 0;

    // Frame data
    static constexpr uint32 MAX_FRAMES = 3;
    NkDX12FrameData mFrameData[MAX_FRAMES];
    uint32          mFrameIndex  = 0;
    uint64          mFrameNumber = 0;
    UINT64          mFenceValue  = 0;

    // Resources
    NkUnorderedMap<uint64, NkDX12Buffer>       mBuffers;
    NkUnorderedMap<uint64, NkDX12Texture>      mTextures;
    NkUnorderedMap<uint64, NkDX12Sampler>      mSamplers;
    NkUnorderedMap<uint64, NkDX12Shader>       mShaders;
    NkUnorderedMap<uint64, NkDX12Pipeline>     mPipelines;
    NkUnorderedMap<uint64, NkDX12RenderPass>   mRenderPasses;
    NkUnorderedMap<uint64, NkDX12Framebuffer>  mFramebuffers;
    NkUnorderedMap<uint64, NkDX12DescSetLayout>mDescLayouts;
    NkUnorderedMap<uint64, NkDX12DescSet>      mDescSets;
    struct DX12Fence { ComPtr<ID3D12Fence> fence; UINT64 value=0; HANDLE event=nullptr; };
    NkUnorderedMap<uint64, DX12Fence>          mFences;

    mutable threading::NkMutex  mMutex;
    NkDeviceInitInfo    mInit   {};
    NkDeviceCaps        mCaps   {};
    bool                mIsValid= false;
    uint32              mWidth=0, mHeight=0;
    HWND                mHwnd   = nullptr;
    bool                mVsync = true;
    bool                mAllowTearing = true;
    bool                mTearingSupported = false;
    bool                mEnableComputeQueue = true;
    uint32              mSwapchainBufferCount = MAX_FRAMES;
    uint32              mRtvHeapCapacity = 256;
    uint32              mDsvHeapCapacity = 64;
    uint32              mSrvHeapCapacity = 1024;
    uint32              mSamplerHeapCapacity = 64;
};

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
