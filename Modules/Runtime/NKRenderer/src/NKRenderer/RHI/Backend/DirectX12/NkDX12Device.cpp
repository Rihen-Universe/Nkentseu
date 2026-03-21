#include "NKLogger/NkLog.h"
#include "NKRenderer/RHI/Core/NkISwapchain.h"
// =============================================================================
// NkRHI_Device_DX12.cpp — Backend DirectX 12
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NKRenderer/RHI/Backend/DirectX12/NkDX12Device.h"
#include "NKRenderer/RHI/Backend/DirectX12/NkDX12CommandBuffer.h"
#include "NKRenderer/RHI/Backend/DirectX12/NkDX12Swapchain.h"
#include "NKRenderer/Context/Graphics/DirectX/NkDirectXContextData.h"
#include "NKCore/NkMacros.h"
#include <cstdio>
#include <cstring>

#define NK_DX12_LOG(...)  logger_src.Infof("[NkRHI_DX12] " __VA_ARGS__)
#define NK_DX12_ERR(...)  logger_src.Infof("[NkRHI_DX12][ERR] " __VA_ARGS__)
#define NK_DX12_CHECK(hr, msg) do { if(FAILED(hr)){NK_DX12_ERR(msg " (hr=0x%X)\n",(unsigned)(hr));} } while(0)

namespace nkentseu {
using threading::NkMutex;
using threading::NkScopedLock;

// =============================================================================
NkDeviceDX12::~NkDeviceDX12() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkDeviceDX12::Initialize(NkIGraphicsContext* ctx) {
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    auto* native = static_cast<NkDX12ContextData*>(ctx->GetNativeContextData());
    if (!native || !native->device || !native->swapchain) {
        NK_DX12_ERR("Donnees natives DX12 indisponibles\n");
        return false;
    }

    mDevice = native->device;
    mHwnd   = native->hwnd;
    mWidth  = native->width;
    mHeight = native->height;
    if (mWidth == 0 || mHeight == 0) {
        const auto info = ctx->GetInfo();
        if (mWidth == 0) mWidth = info.windowWidth;
        if (mHeight == 0) mHeight = info.windowHeight;
    }
    if (mWidth == 0) mWidth = 1;
    if (mHeight == 0) mHeight = 1;

    // Command queue graphique
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qd.Flags= D3D12_COMMAND_QUEUE_FLAG_NONE;
    NK_DX12_CHECK(mDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&mGraphicsQueue)),
                  "CreateCommandQueue (graphics)");

    // Command queue compute
    qd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    mDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&mComputeQueue));

    // DXGI factory
    CreateDXGIFactory2(0, IID_PPV_ARGS(&mFactory));

    // Frame data
    for (uint32 i = 0; i < kMaxFrames; i++) {
        NK_DX12_CHECK(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&mFrameData[i].allocator)), "CreateCommandAllocator");
        NK_DX12_CHECK(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&mFrameData[i].fence)), "CreateFence (frame)");
        mFrameData[i].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    InitDescriptorHeaps();
    CreateSwapchain(mWidth, mHeight);
    QueryCaps();

    mIsValid = true;
    NK_DX12_LOG("Initialisé (%u×%u, %u frames)\n", mWidth, mHeight, kMaxFrames);
    return true;
}

// =============================================================================
void NkDeviceDX12::InitDescriptorHeaps() {
    auto makeHeap = [&](NkDX12DescHeap& heap,
                         D3D12_DESCRIPTOR_HEAP_TYPE type,
                         UINT capacity, bool shaderVisible) {
        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type           = type;
        d.NumDescriptors = capacity;
        d.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                         : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        mDevice->CreateDescriptorHeap(&d, IID_PPV_ARGS(&heap.heap));
        heap.cpuBase      = heap.heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible)
            heap.gpuBase  = heap.heap->GetGPUDescriptorHandleForHeapStart();
        heap.incrementSize= mDevice->GetDescriptorHandleIncrementSize(type);
        heap.capacity     = capacity;
        heap.allocated    = 0;
    };
    makeHeap(mRtvHeap,       D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         256,  false);
    makeHeap(mDsvHeap,       D3D12_DESCRIPTOR_HEAP_TYPE_DSV,         64,   false);
    makeHeap(mCbvSrvUavHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8192, true);
    makeHeap(mSamplerHeap,   D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     256,  true);
}

// =============================================================================
void NkDeviceDX12::CreateSwapchain(uint32 w, uint32 h) {
    // Réutiliser le swapchain déjà créé par NkContext — non-owning
    auto* native = mCtx ? static_cast<NkDX12ContextData*>(mCtx->GetNativeContextData()) : nullptr;
    if (!native || !native->swapchain) {
        NK_DX12_ERR("Pas de IDXGISwapChain4 dans le contexte\n");
        return;
    }
    mSwapchain = native->swapchain.Get(); // non-owning

    uint32 bbCount = native->backBufferCount;
    if (bbCount == 0) bbCount = kMaxFrames;

    // Render pass commun (créé une seule fois en dehors de la boucle)
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_BGRA8_UNORM))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP = CreateRenderPass(rpd);

    for (uint32 i = 0; i < bbCount; i++) {
        // Récupérer le back buffer existant depuis le contexte
        ComPtr<ID3D12Resource> buf;
        mSwapchain->GetBuffer(i, IID_PPV_ARGS(&buf));

        uint64 tid = NextId();
        NkDX12Texture t;
        t.resource    = buf;
        t.isSwapchain = true;
        t.format      = DXGI_FORMAT_B8G8R8A8_UNORM;
        t.desc        = NkTextureDesc::RenderTarget(w, h, NkGpuFormat::NK_BGRA8_UNORM);
        t.state       = D3D12_RESOURCE_STATE_PRESENT;

        // RTV dans notre heap (on possède ce descripteur)
        t.rtvIdx = mRtvHeap.allocated;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap.AllocCPU();
        mDevice->CreateRenderTargetView(buf.Get(), nullptr, rtv);

        mTextures[tid] = t;
        NkTextureHandle colorH; colorH.id = tid;

        // Depth buffer RHI
        NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h);
        auto depthH = CreateTexture(dd);

        NkFramebufferDesc fbd;
        fbd.renderPass = mSwapchainRP;
        fbd.colorAttachments.PushBack(colorH);
        fbd.depthAttachment = depthH;
        fbd.width = w; fbd.height = h;
        mSwapchainFBs.PushBack(CreateFramebuffer(fbd));
    }
    mBackBufferIdx = mSwapchain->GetCurrentBackBufferIndex();
}

void NkDeviceDX12::DestroySwapchain() {
    WaitIdle();
    for (auto& fb : mSwapchainFBs) DestroyFramebuffer(fb);
    mSwapchainFBs.Clear();
    if (mSwapchainRP.IsValid()) DestroyRenderPass(mSwapchainRP);
    mSwapchain = nullptr; // non-owning — appartient au contexte, pas de Reset()
}

void NkDeviceDX12::ResizeSwapchain(uint32 w, uint32 h) {
    WaitIdle();
    DestroySwapchain();
    mWidth = w; mHeight = h;
    CreateSwapchain(w, h);
}

void NkDeviceDX12::Shutdown() {
    WaitIdle();
    DestroySwapchain();
    mBuffers.ForEach([](const uint64&, NkDX12Buffer& b) { if (b.mapped) b.resource->Unmap(0, nullptr); });
    mBuffers.Clear(); mTextures.Clear(); mSamplers.Clear();
    mShaders.Clear(); mPipelines.Clear(); mRenderPasses.Clear();
    mFramebuffers.Clear(); mDescLayouts.Clear(); mDescSets.Clear();
    for (uint32 i = 0; i < kMaxFrames; i++)
        if (mFrameData[i].fenceEvent) CloseHandle(mFrameData[i].fenceEvent);
    mIsValid = false;
    NK_DX12_LOG("Shutdown\n");
}

// =============================================================================
// One-shot execution
// =============================================================================
void NkDeviceDX12::ExecuteOneShot(NkFunction<void(ID3D12GraphicsCommandList*)> fn) {
    ComPtr<ID3D12CommandAllocator> alloc;
    mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    ComPtr<ID3D12GraphicsCommandList> cmd;
    mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), nullptr, IID_PPV_ARGS(&cmd));

    fn(cmd.Get());
    cmd->Close();

    ID3D12CommandList* lists[] = { cmd.Get() };
    mGraphicsQueue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    mGraphicsQueue->Signal(fence.Get(), 1);
    HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    WaitForSingleObject(ev, INFINITE);
    CloseHandle(ev);
}

// =============================================================================
// Resource transitions
// =============================================================================
void NkDeviceDX12::TransitionResource(ID3D12GraphicsCommandList* cmd,
                                        ID3D12Resource* res,
                                        D3D12_RESOURCE_STATES from,
                                        D3D12_RESOURCE_STATES to) {
    if (from == to) return;
    D3D12_RESOURCE_BARRIER b{};
    b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource   = res;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter  = to;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &b);
}

// =============================================================================
// Root Signature par défaut
// =============================================================================
ComPtr<ID3D12RootSignature> NkDeviceDX12::CreateDefaultRootSignature(bool compute) {
    // Root signature minimaliste :
    //   slot 0 : push constants (128 bytes = 32 floats) → root constants
    //   slot 1 : descriptor table CBV/SRV/UAV (t0..t15, b0..b15, u0..u15)
    //   slot 2 : descriptor table samplers (s0..s15)

    D3D12_ROOT_PARAMETER params[3]{};

    // Push constants
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants.ShaderRegister = 0;
    params[0].Constants.RegisterSpace  = 0;
    params[0].Constants.Num32BitValues = 32;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // CBV/SRV/UAV table
    D3D12_DESCRIPTOR_RANGE ranges[3]{};
    ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 16, 1, 0, 0 };
    ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 32, 0, 0, 16 };
    ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 16, 0, 0, 48 };
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 3;
    params[1].DescriptorTable.pDescriptorRanges   = ranges;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Sampler table
    D3D12_DESCRIPTOR_RANGE sampRange{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0, 0 };
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges   = &sampRange;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters     = 3;
    rsd.pParameters       = params;
    rsd.NumStaticSamplers = 0;
    rsd.pStaticSamplers   = nullptr;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err);
    ComPtr<ID3D12RootSignature> rs;
    mDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs));
    return rs;
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDeviceDX12::CreateBuffer(const NkBufferDesc& desc) {
    NkScopedLock lock(mMutex);

    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON;
    bool persistentMap = false;

    switch (desc.usage) {
        case NkResourceUsage::NK_UPLOAD:
            heapType = D3D12_HEAP_TYPE_UPLOAD;
            initState = D3D12_RESOURCE_STATE_GENERIC_READ;
            persistentMap = true;
            break;
        case NkResourceUsage::NK_READBACK:
            heapType = D3D12_HEAP_TYPE_READBACK;
            initState = D3D12_RESOURCE_STATE_COPY_DEST;
            persistentMap = true;
            break;
        default: break;
    }

    // Aligner la taille sur 256 pour les CBV
    uint64 alignedSize = desc.sizeBytes;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNIFORM_BUFFER) ||
        desc.type == NkBufferType::NK_UNIFORM)
        alignedSize = (alignedSize + 255) & ~255ull;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width              = alignedSize;
    rd.Height             = 1;
    rd.DepthOrArraySize   = 1;
    rd.MipLevels          = 1;
    rd.SampleDesc         = { 1, 0 };
    rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = heapType;
    hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ComPtr<ID3D12Resource> res;
    NK_DX12_CHECK(mDevice->CreateCommittedResource(&hp,
        D3D12_HEAP_FLAG_NONE, &rd, initState, nullptr, IID_PPV_ARGS(&res)),
        "CreateCommittedResource (buffer)");

    NkDX12Buffer b;
    b.resource = res;
    b.gpuAddr  = res->GetGPUVirtualAddress();
    b.desc     = desc;
    b.state    = initState;

    if (persistentMap) {
        D3D12_RANGE r{};
        res->Map(0, &r, &b.mapped);
    }

    // Créer les vues selon les flags
    if (desc.type == NkBufferType::NK_UNIFORM ||
        NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNIFORM_BUFFER)) {
        b.cbvIdx = mCbvSrvUavHeap.allocated;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvd{};
        cbvd.BufferLocation = b.gpuAddr;
        cbvd.SizeInBytes    = (UINT)alignedSize;
        mDevice->CreateConstantBufferView(&cbvd, mCbvSrvUavHeap.AllocCPU());
    }

    if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_STORAGE_BUFFER) ||
        desc.type == NkBufferType::NK_STORAGE) {
        b.srvIdx = mCbvSrvUavHeap.allocated;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
        srvd.Format                     = DXGI_FORMAT_UNKNOWN;
        srvd.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvd.Buffer.NumElements         = (UINT)(desc.sizeBytes / sizeof(float));
        srvd.Buffer.StructureByteStride = sizeof(float);
        mDevice->CreateShaderResourceView(res.Get(), &srvd, mCbvSrvUavHeap.AllocCPU());

        b.uavIdx = mCbvSrvUavHeap.allocated;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavd{};
        uavd.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
        uavd.Format                     = DXGI_FORMAT_UNKNOWN;
        uavd.Buffer.NumElements         = (UINT)(desc.sizeBytes / sizeof(float));
        uavd.Buffer.StructureByteStride = sizeof(float);
        mDevice->CreateUnorderedAccessView(res.Get(), nullptr, &uavd, mCbvSrvUavHeap.AllocCPU());
    }

    // Upload données initiales
    if (desc.initialData) {
        if (b.mapped) {
            memory::NkMemCopy(b.mapped, desc.initialData, (size_t)desc.sizeBytes);
        } else {
            // Via upload buffer temporaire
            NkBufferDesc sd = NkBufferDesc::Staging(desc.sizeBytes);
            auto stageH = CreateBuffer(sd);
            auto& stage = mBuffers[stageH.id];
            memory::NkMemCopy(stage.mapped, desc.initialData, (size_t)desc.sizeBytes);
            ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
                TransitionResource(cmd, res.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                cmd->CopyBufferRegion(res.Get(), 0, stage.resource.Get(), 0, desc.sizeBytes);
                TransitionResource(cmd, res.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
            });
            DestroyBuffer(stageH);
            b.state = D3D12_RESOURCE_STATE_COMMON;
        }
    }

    uint64 hid = NextId(); mBuffers[hid] = b;
    NkBufferHandle h; h.id = hid; return h;
}

void NkDeviceDX12::DestroyBuffer(NkBufferHandle& h) {
    NkScopedLock lock(mMutex);
    auto it = mBuffers.Find(h.id); if (it == nullptr) return;
    if (it->mapped) it->resource->Unmap(0, nullptr);
    mBuffers.Erase(h.id); h.id = 0;
}

bool NkDeviceDX12::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.Find(buf.id); if (it == nullptr) return false;
    if (it->mapped) {
        memory::NkMemCopy((uint8*)it->mapped + off, data, (size_t)sz);
        return true;
    }
    // Via upload
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    memory::NkMemCopy(stage.mapped, data, (size_t)sz);
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        auto prevState = it->state;
        TransitionResource(cmd, it->resource.Get(), prevState, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyBufferRegion(it->resource.Get(), off, stage.resource.Get(), 0, sz);
        TransitionResource(cmd, it->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, prevState);
    });
    DestroyBuffer(stageH);
    return true;
}

bool NkDeviceDX12::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.Find(buf.id); if (it == nullptr) return false;
    if (it->mapped) { memory::NkMemCopy((uint8*)it->mapped + off, data, (size_t)sz); return true; }
    return WriteBuffer(buf, data, sz, off);
}

bool NkDeviceDX12::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
    auto it = mBuffers.Find(buf.id); if (it == nullptr) return false;
    if (it->mapped) { memory::NkMemCopy(out, (uint8*)it->mapped + off, (size_t)sz); return true; }
    NkBufferDesc sd = NkBufferDesc::Staging(sz); sd.usage = NkResourceUsage::NK_READBACK;
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, it->resource.Get(), it->state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmd->CopyBufferRegion(stage.resource.Get(), 0, it->resource.Get(), off, sz);
        TransitionResource(cmd, it->resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, it->state);
    });
    memory::NkMemCopy(out, stage.mapped, (size_t)sz);
    DestroyBuffer(stageH);
    return true;
}

NkMappedMemory NkDeviceDX12::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    auto it = mBuffers.Find(buf.id); if (it == nullptr) return {};
    if (it->mapped) {
        uint64 mapSz = sz > 0 ? sz : it->desc.sizeBytes - off;
        return { (uint8*)it->mapped + off, mapSz };
    }
    void* ptr = nullptr;
    D3D12_RANGE r{};
    it->resource->Map(0, &r, &ptr);
    uint64 mapSz = sz > 0 ? sz : it->desc.sizeBytes - off;
    return { (uint8*)ptr + off, mapSz };
}

void NkDeviceDX12::UnmapBuffer(NkBufferHandle buf) {
    auto it = mBuffers.Find(buf.id); if (it == nullptr) return;
    if (!it->mapped) it->resource->Unmap(0, nullptr);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDeviceDX12::CreateTexture(const NkTextureDesc& desc) {
    NkScopedLock lock(mMutex);

    DXGI_FORMAT fmt = ToDXGIFormat(desc.format);
    bool isDepth = NkFormatIsDepth(desc.format);

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width              = desc.width;
    rd.Height             = desc.height;
    rd.DepthOrArraySize   = (UINT16)NK_MAX(desc.arrayLayers, 1u);
    rd.MipLevels          = (UINT16)(desc.mipLevels == 0 ?
        (uint32)(floor(log2(NK_MAX(desc.width, desc.height))) + 1) : desc.mipLevels);
    rd.SampleDesc         = { (UINT)desc.samples, 0 };
    rd.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (isDepth) {
        rd.Format = desc.format == NkGpuFormat::NK_D32_FLOAT ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
        rd.Flags  = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    } else {
        rd.Format = fmt;
        rd.Flags  = D3D12_RESOURCE_FLAG_NONE;
        if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET))    rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNORDERED_ACCESS)) rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_CLEAR_VALUE clearVal{};
    D3D12_CLEAR_VALUE* pClear = nullptr;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET) && !isDepth) {
        clearVal.Format = rd.Format;
        pClear = &clearVal;
    } else if (isDepth) {
        clearVal.Format = rd.Format;
        clearVal.DepthStencil = { 1.f, 0 };
        pClear = &clearVal;
    }

    D3D12_RESOURCE_STATES initState = isDepth
        ? D3D12_RESOURCE_STATE_DEPTH_WRITE
        : (NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET)
           ? D3D12_RESOURCE_STATE_RENDER_TARGET
           : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    hp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    ComPtr<ID3D12Resource> res;
    NK_DX12_CHECK(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
        &rd, initState, pClear, IID_PPV_ARGS(&res)), "CreateCommittedResource (texture)");

    NkDX12Texture t;
    t.resource = res;
    t.desc     = desc;
    t.format   = rd.Format;
    t.state    = initState;

    // Créer les vues
    if (isDepth) {
        t.dsvIdx = mDsvHeap.allocated;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format        = rd.Format;
        dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        mDevice->CreateDepthStencilView(res.Get(), &dsvd, mDsvHeap.AllocCPU());
    } else {
        if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET)) {
            t.rtvIdx = mRtvHeap.allocated;
            mDevice->CreateRenderTargetView(res.Get(), nullptr, mRtvHeap.AllocCPU());
        }
        if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_SHADER_RESOURCE) ||
            (!NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET) && !isDepth)) {
            t.srvIdx = mCbvSrvUavHeap.allocated;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                  = fmt;
            srvd.ViewDimension           = desc.type == NkTextureType::NK_CUBE ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if (desc.type == NkTextureType::NK_CUBE)
                srvd.TextureCube.MipLevels = rd.MipLevels;
            else
                srvd.Texture2D.MipLevels   = rd.MipLevels;
            mDevice->CreateShaderResourceView(res.Get(), &srvd, mCbvSrvUavHeap.AllocCPU());
        }
        if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNORDERED_ACCESS)) {
            t.uavIdx = mCbvSrvUavHeap.allocated;
            mDevice->CreateUnorderedAccessView(res.Get(), nullptr, nullptr, mCbvSrvUavHeap.AllocCPU());
        }
    }

    // Upload données initiales
    if (desc.initialData) {
        uint32 bpp = NkFormatBytesPerPixel(desc.format);
        uint32 rowPitch = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
        uint64 imgSz = (uint64)rowPitch * desc.height;

        NkBufferDesc sd = NkBufferDesc::Staging(imgSz);
        auto stageH = CreateBuffer(sd);
        auto& stage = mBuffers[stageH.id];
        memory::NkMemCopy(stage.mapped, desc.initialData, (size_t)imgSz);

        ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
            TransitionResource(cmd, res.Get(), initState, D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource        = res.Get();
            dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource        = stage.resource.Get();
            src.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Footprint = { rd.Format, desc.width, desc.height, 1, rowPitch };
            cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            TransitionResource(cmd, res.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        });
        t.state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        DestroyBuffer(stageH);
    }

    uint64 hid = NextId(); mTextures[hid] = t;
    NkTextureHandle h; h.id = hid; return h;
}

void NkDeviceDX12::DestroyTexture(NkTextureHandle& h) {
    NkScopedLock lock(mMutex);
    auto it = mTextures.Find(h.id); if (it == nullptr) return;
    mTextures.Erase(h.id); h.id = 0;
}

bool NkDeviceDX12::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    auto it = mTextures.Find(t.id); if (it == nullptr) return false;
    auto& desc = it->desc;
    return WriteTextureRegion(t, p, 0, 0, 0, desc.width, desc.height, 1, 0, 0, rp);
}

bool NkDeviceDX12::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x, uint32 y, uint32 /*z*/, uint32 w, uint32 h, uint32 /*d2*/,
    uint32 mip, uint32 /*layer*/, uint32 rowPitch) {
    auto it = mTextures.Find(t.id); if (it == nullptr) return false;
    uint32 bpp = NkFormatBytesPerPixel(it->desc.format);
    uint32 rp  = rowPitch > 0 ? rowPitch : w * bpp;
    uint64 sz  = (uint64)rp * h;
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    memory::NkMemCopy(stage.mapped, pixels, (size_t)sz);

    auto prevState = it->state;
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, it->resource.Get(), prevState, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = it->resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = mip;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = stage.resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint = { it->format, w, h, 1, rp };
        D3D12_BOX box{ x, y, 0, x + w, y + h, 1 };
        cmd->CopyTextureRegion(&dst, x, y, 0, &src, &box);
        TransitionResource(cmd, it->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, prevState);
    });
    DestroyBuffer(stageH);
    return true;
}

bool NkDeviceDX12::GenerateMipmaps(NkTextureHandle, NkFilter) {
    // DX12 n'a pas de GenerateMips natif — nécessite un compute shader séparé
    // Pour l'instant on retourne true (les mips sont générés à la création si mipLevels>1)
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDeviceDX12::CreateSampler(const NkSamplerDesc& d) {
    NkScopedLock lock(mMutex);
    D3D12_SAMPLER_DESC sd{};
    sd.Filter         = ToDX12Filter(d.magFilter, d.minFilter, d.mipFilter, d.compareEnable);
    sd.AddressU       = ToDX12Address(d.addressU);
    sd.AddressV       = ToDX12Address(d.addressV);
    sd.AddressW       = ToDX12Address(d.addressW);
    sd.MipLODBias     = d.mipLodBias;
    sd.MaxAnisotropy  = (UINT)d.maxAnisotropy;
    sd.ComparisonFunc = d.compareEnable ? ToDX12Compare(d.compareOp) : D3D12_COMPARISON_FUNC_NEVER;
    sd.MinLOD = d.minLod; sd.MaxLOD = d.maxLod;

    UINT idx = mSamplerHeap.allocated;
    mDevice->CreateSampler(&sd, mSamplerHeap.AllocCPU());

    uint64 hid = NextId(); mSamplers[hid] = { idx };
    NkSamplerHandle h; h.id = hid; return h;
}

void NkDeviceDX12::DestroySampler(NkSamplerHandle& h) {
    NkScopedLock lock(mMutex);
    mSamplers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Shaders (DXBC/DXIL via D3DCompile ou pré-compilés)
// =============================================================================
NkShaderHandle NkDeviceDX12::CreateShader(const NkShaderDesc& desc) {
    NkScopedLock lock(mMutex);
    NkDX12Shader sh;

    for (uint32 i = 0; i < (uint32)desc.stages.Size(); i++) {
        auto& s = desc.stages[i];

        // Priorité : bytecode précompilé > HLSL source
        NkVector<uint8>* target = nullptr;
        const char* hlslTarget = nullptr;

        switch (s.stage) {
            case NkShaderStage::NK_VERTEX:   target = &sh.vs.bytecode; hlslTarget = "vs_5_1"; break;
            case NkShaderStage::NK_FRAGMENT: target = &sh.ps.bytecode; hlslTarget = "ps_5_1"; break;
            case NkShaderStage::NK_COMPUTE:  target = &sh.cs.bytecode; hlslTarget = "cs_5_1"; break;
            case NkShaderStage::NK_GEOMETRY: target = &sh.gs.bytecode; hlslTarget = "gs_5_1"; break;
            default: continue;
        }

        if (s.spirvData && s.spirvSize > 0) {
            // DXIL pré-compilé
            target->Resize(s.spirvSize);
            memory::NkMemCopy(target->Data(), s.spirvData, s.spirvSize);
        } else if (s.hlslSource) {
            const char* entry = s.entryPoint ? s.entryPoint : "main";
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            ComPtr<ID3DBlob> code, err;
            HRESULT hr = D3DCompile(s.hlslSource, strlen(s.hlslSource), nullptr,
                nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entry, hlslTarget, flags, 0, &code, &err);
            if (FAILED(hr)) {
                if (err) NK_DX12_ERR("Shader: %s\n", (char*)err->GetBufferPointer());
                continue;
            }
            usize blobSz = code->GetBufferSize();
            target->Resize(blobSz);
            memory::NkMemCopy(target->Data(), code->GetBufferPointer(), blobSz);
            if (s.stage == NkShaderStage::NK_VERTEX) sh.vsBlob = code;
        }
    }

    uint64 hid = NextId(); mShaders[hid] = std::move(sh);
    NkShaderHandle h; h.id = hid; return h;
}

void NkDeviceDX12::DestroyShader(NkShaderHandle& h) {
    NkScopedLock lock(mMutex);
    mShaders.Erase(h.id); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDeviceDX12::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    NkScopedLock lock(mMutex);
    auto sit = mShaders.Find(d.shader.id); if (sit == nullptr) return {};
    auto& sh = (*sit);

    auto rootSig = CreateDefaultRootSignature(false);

    // Input Layout
    NkVector<D3D12_INPUT_ELEMENT_DESC> elems;
    for (uint32 i = 0; i < d.vertexLayout.attributes.Size(); i++) {
        auto& a = d.vertexLayout.attributes[i];
        bool instanced = false;
        for (uint32 j = 0; j < d.vertexLayout.bindings.Size(); j++)
            if (d.vertexLayout.bindings[j].binding == a.binding)
                { instanced = d.vertexLayout.bindings[j].perInstance; break; }
        D3D12_INPUT_ELEMENT_DESC e{};
        e.SemanticName         = a.semanticName ? a.semanticName : "TEXCOORD";
        e.SemanticIndex        = a.semanticIdx;
        e.Format               = ToDXGIVertexFormat(a.format);
        e.InputSlot            = a.binding;
        e.AlignedByteOffset    = a.offset;
        e.InputSlotClass       = instanced ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                                           : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        e.InstanceDataStepRate = instanced ? 1 : 0;
        elems.PushBack(e);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psd{};
    psd.pRootSignature = rootSig.Get();
    psd.VS = sh.vs.bc();
    psd.PS = sh.ps.bc();
    if (!sh.gs.bytecode.IsEmpty()) psd.GS = sh.gs.bc();
    if (!sh.hs.bytecode.IsEmpty()) psd.HS = sh.hs.bc();
    if (!sh.ds.bytecode.IsEmpty()) psd.DS = sh.ds.bc();

    psd.InputLayout = { elems.Data(), (UINT)elems.Size() };
    psd.PrimitiveTopologyType = ToDX12TopologyType(d.topology);

    // Rasterizer
    psd.RasterizerState.FillMode = d.rasterizer.fillMode == NkFillMode::NK_SOLID
        ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    psd.RasterizerState.CullMode = d.rasterizer.cullMode == NkCullMode::NK_NONE
        ? D3D12_CULL_MODE_NONE : d.rasterizer.cullMode == NkCullMode::NK_FRONT
        ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
    psd.RasterizerState.FrontCounterClockwise = d.rasterizer.frontFace == NkFrontFace::NK_CCW;
    psd.RasterizerState.DepthClipEnable       = d.rasterizer.depthClip;
    psd.RasterizerState.DepthBias             = (INT)d.rasterizer.depthBiasConst;
    psd.RasterizerState.SlopeScaledDepthBias  = d.rasterizer.depthBiasSlope;
    psd.RasterizerState.MultisampleEnable     = d.rasterizer.multisampleEnable;

    // Depth-stencil
    psd.DepthStencilState.DepthEnable    = d.depthStencil.depthTestEnable;
    psd.DepthStencilState.DepthWriteMask = d.depthStencil.depthWriteEnable
        ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psd.DepthStencilState.DepthFunc      = ToDX12Compare(d.depthStencil.depthCompareOp);
    psd.DepthStencilState.StencilEnable  = d.depthStencil.stencilEnable;
    auto convSt = [&](const NkStencilOpState& s) -> D3D12_DEPTH_STENCILOP_DESC {
        return { ToDX12StencilOp(s.failOp), ToDX12StencilOp(s.depthFailOp),
                 ToDX12StencilOp(s.passOp),  ToDX12Compare(s.compareOp) };
    };
    psd.DepthStencilState.FrontFace = convSt(d.depthStencil.front);
    psd.DepthStencilState.BackFace  = convSt(d.depthStencil.back);

    // Blend
    psd.BlendState.AlphaToCoverageEnable  = d.blend.alphaToCoverage;
    psd.BlendState.IndependentBlendEnable = d.blend.attachments.Size() > 1;
    for (uint32 i = 0; i < d.blend.attachments.Size() && i < 8; i++) {
        auto& a = d.blend.attachments[i];
        psd.BlendState.RenderTarget[i].BlendEnable    = a.blendEnable;
        psd.BlendState.RenderTarget[i].SrcBlend       = ToDX12Blend(a.srcColor);
        psd.BlendState.RenderTarget[i].DestBlend      = ToDX12Blend(a.dstColor);
        psd.BlendState.RenderTarget[i].BlendOp        = ToDX12BlendOp(a.colorOp);
        psd.BlendState.RenderTarget[i].SrcBlendAlpha  = ToDX12Blend(a.srcAlpha);
        psd.BlendState.RenderTarget[i].DestBlendAlpha = ToDX12Blend(a.dstAlpha);
        psd.BlendState.RenderTarget[i].BlendOpAlpha   = ToDX12BlendOp(a.alphaOp);
        psd.BlendState.RenderTarget[i].RenderTargetWriteMask = a.colorWriteMask & 0xF;
    }

    psd.SampleMask = UINT_MAX;
    psd.SampleDesc = { (UINT)d.samples, 0 };

    // Render target formats
    auto rpit = mRenderPasses.Find(d.renderPass.id);
    if (rpit != nullptr) {
        psd.NumRenderTargets = rpit->desc.colorAttachments.Size();
        for (uint32 i = 0; i < rpit->desc.colorAttachments.Size(); i++)
            psd.RTVFormats[i] = ToDXGIFormat(rpit->desc.colorAttachments[i].format);
        if (rpit->desc.hasDepth)
            psd.DSVFormat = NkFormatIsDepth(rpit->desc.depthAttachment.format)
                ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
    } else {
        psd.NumRenderTargets = 1;
        psd.RTVFormats[0]    = DXGI_FORMAT_B8G8R8A8_UNORM;
        psd.DSVFormat        = DXGI_FORMAT_D32_FLOAT;
    }

    ComPtr<ID3D12PipelineState> pso;
    NK_DX12_CHECK(mDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&pso)),
                  "CreateGraphicsPipelineState");

    NkDX12Pipeline p; p.pso = pso; p.rootSig = rootSig;
    p.isCompute = false;
    p.topology  = ToDX12Topology(d.topology);

    uint64 hid = NextId(); mPipelines[hid] = std::move(p);
    NkPipelineHandle h; h.id = hid; return h;
}

NkPipelineHandle NkDeviceDX12::CreateComputePipeline(const NkComputePipelineDesc& d) {
    NkScopedLock lock(mMutex);
    auto sit = mShaders.Find(d.shader.id); if (sit == nullptr) return {};
    auto& sh = (*sit);

    auto rootSig = CreateDefaultRootSignature(true);

    D3D12_COMPUTE_PIPELINE_STATE_DESC cpd{};
    cpd.pRootSignature = rootSig.Get();
    cpd.CS             = sh.cs.bc();

    ComPtr<ID3D12PipelineState> pso;
    NK_DX12_CHECK(mDevice->CreateComputePipelineState(&cpd, IID_PPV_ARGS(&pso)),
                  "CreateComputePipelineState");

    NkDX12Pipeline p; p.pso = pso; p.rootSig = rootSig; p.isCompute = true;
    uint64 hid = NextId(); mPipelines[hid] = std::move(p);
    NkPipelineHandle h; h.id = hid; return h;
}

void NkDeviceDX12::DestroyPipeline(NkPipelineHandle& h) {
    NkScopedLock lock(mMutex);
    mPipelines.Erase(h.id); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers (metadata only en DX12)
// =============================================================================
NkRenderPassHandle NkDeviceDX12::CreateRenderPass(const NkRenderPassDesc& d) {
    NkScopedLock lock(mMutex);
    uint64 hid = NextId(); mRenderPasses[hid] = { d };
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkDeviceDX12::DestroyRenderPass(NkRenderPassHandle& h) {
    NkScopedLock lock(mMutex); mRenderPasses.Erase(h.id); h.id = 0;
}

NkFramebufferHandle NkDeviceDX12::CreateFramebuffer(const NkFramebufferDesc& d) {
    NkScopedLock lock(mMutex);
    NkDX12Framebuffer fb; fb.w = d.width; fb.h = d.height;
    for (uint32 i = 0; i < d.colorAttachments.Size(); i++) {
        auto it = mTextures.Find(d.colorAttachments[i].id);
        if (it != nullptr) fb.rtvIdxs[fb.rtvCount++] = it->rtvIdx;
    }
    if (d.depthAttachment.IsValid()) {
        auto it = mTextures.Find(d.depthAttachment.id);
        if (it != nullptr) fb.dsvIdx = it->dsvIdx;
    }
    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
void NkDeviceDX12::DestroyFramebuffer(NkFramebufferHandle& h) {
    NkScopedLock lock(mMutex); mFramebuffers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDeviceDX12::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    NkScopedLock lock(mMutex);
    uint64 hid = NextId(); mDescLayouts[hid] = { d };
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDeviceDX12::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id = 0;
}
NkDescSetHandle NkDeviceDX12::AllocateDescriptorSet(NkDescSetHandle layout) {
    NkScopedLock lock(mMutex);
    NkDX12DescSet ds; ds.layoutId = layout.id;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDeviceDX12::FreeDescriptorSet(NkDescSetHandle& h) {
    NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id = 0;
}

void NkDeviceDX12::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    NkScopedLock lock(mMutex);
    for (uint32 i = 0; i < n; i++) {
        auto& w = writes[i];
        auto sit = mDescSets.Find(w.set.id); if (sit == nullptr) continue;

        NkDX12DescSet::Binding b;
        b.slot   = w.binding;
        b.type   = w.type;
        b.bufId  = w.buffer.id;
        b.texId  = w.texture.id;
        b.sampId = w.sampler.id;

        // Remplacer si le slot existe déjà
        bool found = false;
        for (auto& existing : sit->bindings)
            if (existing.slot == w.binding) { existing = b; found = true; break; }
        if (!found) sit->bindings.PushBack(b);
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkDeviceDX12::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_DX12(this, t);
}
void NkDeviceDX12::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb = nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDeviceDX12::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    NkVector<ID3D12CommandList*> lists;
    for (uint32 i = 0; i < n; i++) {
        auto* dx = dynamic_cast<NkCommandBuffer_DX12*>(cbs[i]);
        if (dx && dx->GetCmdList()) lists.PushBack(dx->GetCmdList());
    }
    if (!lists.IsEmpty())
        mGraphicsQueue->ExecuteCommandLists((UINT)lists.Size(), lists.Data());
    if (fence.IsValid()) {
        auto it = mFences.Find(fence.id);
        if (it != nullptr) {
            it->value++;
            mGraphicsQueue->Signal(it->fence.Get(), it->value);
        }
    }
}

void NkDeviceDX12::SubmitAndPresent(NkICommandBuffer* cb) {
    if (!mSwapchain) return;
    auto& fd = mFrameData[mFrameIndex];

    NkICommandBuffer* cbs[] = { cb };
    Submit(cbs, 1, {});

    fd.Signal(mGraphicsQueue.Get(), ++mFenceValue);
    mSwapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    mBackBufferIdx = mSwapchain->GetCurrentBackBufferIndex();
}

// =============================================================================
// Fence
// =============================================================================
NkFenceHandle NkDeviceDX12::CreateFence(bool signaled) {
    DX12Fence f;
    mDevice->CreateFence(signaled ? 1 : 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f.fence));
    f.value = signaled ? 1 : 0;
    f.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    uint64 hid = NextId(); mFences[hid] = std::move(f);
    NkFenceHandle h; h.id = hid; return h;
}
void NkDeviceDX12::DestroyFence(NkFenceHandle& h) {
    auto it = mFences.Find(h.id); if (it == nullptr) return;
    CloseHandle(it->event);
    mFences.Erase(h.id); h.id = 0;
}
bool NkDeviceDX12::WaitFence(NkFenceHandle f, uint64 timeoutNs) {
    auto it = mFences.Find(f.id); if (it == nullptr) return false;
    if (it->fence->GetCompletedValue() < it->value) {
        it->fence->SetEventOnCompletion(it->value, it->event);
        DWORD ms = timeoutNs == UINT64_MAX ? INFINITE : (DWORD)(timeoutNs / 1000000);
        WaitForSingleObject(it->event, ms);
    }
    return it->fence->GetCompletedValue() >= it->value;
}
bool NkDeviceDX12::IsFenceSignaled(NkFenceHandle f) {
    auto it = mFences.Find(f.id); if (it == nullptr) return false;
    return it->fence->GetCompletedValue() >= it->value;
}
void NkDeviceDX12::ResetFence(NkFenceHandle f) {
    auto it = mFences.Find(f.id); if (it == nullptr) return;
    it->value = 0;
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&it->fence));
}
void NkDeviceDX12::WaitIdle() {
    ComPtr<ID3D12Fence> fence;
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    mGraphicsQueue->Signal(fence.Get(), 1);
    HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    WaitForSingleObject(ev, INFINITE);
    CloseHandle(ev);
}

// =============================================================================
// Frame
// =============================================================================
void NkDeviceDX12::BeginFrame(NkFrameContext& frame) {
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
    if (!mSwapchain) {
        NK_DX12_ERR("BeginFrame: swapchain null — frame ignoré\n");
        return;
    }
    mFrameData[mFrameIndex].WaitAndReset(mGraphicsQueue.Get());
    mBackBufferIdx    = mSwapchain->GetCurrentBackBufferIndex();
}
void NkDeviceDX12::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex + 1) % kMaxFrames;
    ++mFrameNumber;
}
void NkDeviceDX12::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return;
    mWidth = w; mHeight = h;
    ResizeSwapchain(w, h);
}

// =============================================================================
// Caps
// =============================================================================
void NkDeviceDX12::QueryCaps() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS opts{};
    mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts));

    mCaps.tessellationShaders = true;
    mCaps.geometryShaders     = true;
    mCaps.computeShaders      = true;
    mCaps.drawIndirect        = true;
    mCaps.multiViewport       = true;
    mCaps.independentBlend    = true;
    mCaps.textureCompressionBC= true;
    mCaps.maxTextureDim2D     = 16384;
    mCaps.maxTextureDim3D     = 2048;
    mCaps.maxColorAttachments = 8;
    mCaps.maxVertexAttributes = 32;
    mCaps.maxPushConstantBytes= 128;
    mCaps.minUniformBufferAlign = 256;
    mCaps.maxComputeGroupSizeX  = 1024;
    mCaps.maxComputeGroupSizeY  = 1024;
    mCaps.maxComputeGroupSizeZ  = 64;
    mCaps.msaa2x = mCaps.msaa4x = mCaps.msaa8x = true;
    mCaps.timestampQueries      = true;
    mCaps.meshShaders = (opts.ResourceHeapTier >= D3D12_RESOURCE_HEAP_TIER_2);
}

// =============================================================================
// Accesseurs internes
// =============================================================================
ID3D12Resource* NkDeviceDX12::GetDX12Buffer(uint64 id) const {
    auto it = mBuffers.Find(id); return it != nullptr ? it->resource.Get() : nullptr;
}
ID3D12Resource* NkDeviceDX12::GetDX12Texture(uint64 id) const {
    auto it = mTextures.Find(id); return it != nullptr ? it->resource.Get() : nullptr;
}
D3D12_GPU_VIRTUAL_ADDRESS NkDeviceDX12::GetBufferGPUAddr(uint64 id) const {
    auto it = mBuffers.Find(id); return it != nullptr ? it->gpuAddr : 0;
}
const NkDX12Pipeline* NkDeviceDX12::GetPipeline(uint64 id) const {
    auto it = mPipelines.Find(id); return it != nullptr ? &(*it) : nullptr;
}
const NkDX12DescSet* NkDeviceDX12::GetDescSet(uint64 id) const {
    auto it = mDescSets.Find(id); return it != nullptr ? &(*it) : nullptr;
}
const NkDX12Framebuffer* NkDeviceDX12::GetFBO(uint64 id) const {
    auto it = mFramebuffers.Find(id); return it != nullptr ? &(*it) : nullptr;
}
D3D12_CPU_DESCRIPTOR_HANDLE NkDeviceDX12::GetRTV(UINT idx) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = mRtvHeap.cpuBase;
    h.ptr += (SIZE_T)idx * mRtvHeap.incrementSize;
    return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE NkDeviceDX12::GetDSV(UINT idx) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = mDsvHeap.cpuBase;
    h.ptr += (SIZE_T)idx * mDsvHeap.incrementSize;
    return h;
}

// =============================================================================
// Conversions
// =============================================================================
DXGI_FORMAT NkDeviceDX12::ToDXGIFormat(NkGpuFormat f) {
    switch (f) {
        case NkGpuFormat::NK_R8_UNORM:         return DXGI_FORMAT_R8_UNORM;
        case NkGpuFormat::NK_RG8_UNORM:        return DXGI_FORMAT_R8G8_UNORM;
        case NkGpuFormat::NK_RGBA8_UNORM:      return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkGpuFormat::NK_RGBA8_SRGB:       return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case NkGpuFormat::NK_BGRA8_UNORM:      return DXGI_FORMAT_B8G8R8A8_UNORM;
        case NkGpuFormat::NK_BGRA8_SRGB:       return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case NkGpuFormat::NK_R16_FLOAT:        return DXGI_FORMAT_R16_FLOAT;
        case NkGpuFormat::NK_RG16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;
        case NkGpuFormat::NK_RGBA16_FLOAT:     return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkGpuFormat::NK_R32_FLOAT:        return DXGI_FORMAT_R32_FLOAT;
        case NkGpuFormat::NK_RG32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
        case NkGpuFormat::NK_RGB32_FLOAT:      return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkGpuFormat::NK_RGBA32_FLOAT:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkGpuFormat::NK_R32_UINT:         return DXGI_FORMAT_R32_UINT;
        case NkGpuFormat::NK_RG32_UINT:        return DXGI_FORMAT_R32G32_UINT;
        case NkGpuFormat::NK_D16_UNORM:        return DXGI_FORMAT_D16_UNORM;
        case NkGpuFormat::NK_D32_FLOAT:        return DXGI_FORMAT_D32_FLOAT;
        case NkGpuFormat::NK_D24_UNORM_S8_UINT:return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case NkGpuFormat::NK_BC1_RGB_UNORM:    return DXGI_FORMAT_BC1_UNORM;
        case NkGpuFormat::NK_BC1_RGB_SRGB:     return DXGI_FORMAT_BC1_UNORM_SRGB;
        case NkGpuFormat::NK_BC3_UNORM:        return DXGI_FORMAT_BC3_UNORM;
        case NkGpuFormat::NK_BC5_UNORM:        return DXGI_FORMAT_BC5_UNORM;
        case NkGpuFormat::NK_BC7_UNORM:        return DXGI_FORMAT_BC7_UNORM;
        case NkGpuFormat::NK_BC7_SRGB:         return DXGI_FORMAT_BC7_UNORM_SRGB;
        case NkGpuFormat::NK_R11G11B10_FLOAT:  return DXGI_FORMAT_R11G11B10_FLOAT;
        case NkGpuFormat::NK_A2B10G10R10_UNORM:return DXGI_FORMAT_R10G10B10A2_UNORM;
        default:                         return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}
DXGI_FORMAT NkDeviceDX12::ToDXGIVertexFormat(NkVertexFormat f) {
    switch (f) {
        case NkGpuFormat::NK_R32_FLOAT:         return DXGI_FORMAT_R32_FLOAT;
        case NkGpuFormat::NK_RG32_FLOAT:        return DXGI_FORMAT_R32G32_FLOAT;
        case NkGpuFormat::NK_RGB32_FLOAT:       return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkGpuFormat::NK_RGBA32_FLOAT:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkGpuFormat::NK_RG16_FLOAT:        return DXGI_FORMAT_R16G16_FLOAT;
        case NkGpuFormat::NK_RGBA16_FLOAT:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkGpuFormat::NK_RGBA8_UNORM:       return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkGpuFormat::NK_RGBA8_SNORM:       return DXGI_FORMAT_R8G8B8A8_SNORM;
        case NkGpuFormat::NK_R32_UINT:          return DXGI_FORMAT_R32_UINT;
        case NkGpuFormat::NK_RG32_UINT:         return DXGI_FORMAT_R32G32_UINT;
        case NkGpuFormat::NK_RGBA32_UINT:       return DXGI_FORMAT_R32G32B32A32_UINT;
        case NkGpuFormat::NK_R32_SINT:          return DXGI_FORMAT_R32_SINT;
        case NkGpuFormat::NK_RG32_SINT:        return DXGI_FORMAT_R32G32_SINT;
        default:                     return DXGI_FORMAT_R32G32B32_FLOAT;
    }
}
D3D12_COMPARISON_FUNC NkDeviceDX12::ToDX12Compare(NkCompareOp op) {
    switch (op) {
        case NkCompareOp::NK_NEVER:        return D3D12_COMPARISON_FUNC_NEVER;
        case NkCompareOp::NK_LESS:         return D3D12_COMPARISON_FUNC_LESS;
        case NkCompareOp::NK_EQUAL:        return D3D12_COMPARISON_FUNC_EQUAL;
        case NkCompareOp::NK_LESS_EQUAL:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case NkCompareOp::NK_GREATER:      return D3D12_COMPARISON_FUNC_GREATER;
        case NkCompareOp::NK_NOT_EQUAL:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case NkCompareOp::NK_GREATER_EQUAL: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        default:                        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}
D3D12_BLEND NkDeviceDX12::ToDX12Blend(NkBlendFactor f) {
    switch (f) {
        case NkBlendFactor::NK_ZERO:              return D3D12_BLEND_ZERO;
        case NkBlendFactor::NK_ONE:               return D3D12_BLEND_ONE;
        case NkBlendFactor::NK_SRC_COLOR:          return D3D12_BLEND_SRC_COLOR;
        case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:  return D3D12_BLEND_INV_SRC_COLOR;
        case NkBlendFactor::NK_SRC_ALPHA:          return D3D12_BLEND_SRC_ALPHA;
        case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:  return D3D12_BLEND_INV_SRC_ALPHA;
        case NkBlendFactor::NK_DST_ALPHA:          return D3D12_BLEND_DEST_ALPHA;
        case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:  return D3D12_BLEND_INV_DEST_ALPHA;
        case NkBlendFactor::NK_SRC_ALPHA_SATURATE:  return D3D12_BLEND_SRC_ALPHA_SAT;
        default:                               return D3D12_BLEND_ONE;
    }
}
D3D12_BLEND_OP NkDeviceDX12::ToDX12BlendOp(NkBlendOp op) {
    switch (op) {
        case NkBlendOp::NK_ADD:    return D3D12_BLEND_OP_ADD;
        case NkBlendOp::NK_SUB:    return D3D12_BLEND_OP_SUBTRACT;
        case NkBlendOp::NK_REV_SUB: return D3D12_BLEND_OP_REV_SUBTRACT;
        case NkBlendOp::NK_MIN:    return D3D12_BLEND_OP_MIN;
        case NkBlendOp::NK_MAX:    return D3D12_BLEND_OP_MAX;
        default:                return D3D12_BLEND_OP_ADD;
    }
}
D3D12_STENCIL_OP NkDeviceDX12::ToDX12StencilOp(NkStencilOp op) {
    switch (op) {
        case NkStencilOp::NK_KEEP:      return D3D12_STENCIL_OP_KEEP;
        case NkStencilOp::NK_ZERO:      return D3D12_STENCIL_OP_ZERO;
        case NkStencilOp::NK_REPLACE:   return D3D12_STENCIL_OP_REPLACE;
        case NkStencilOp::NK_INCR_CLAMP: return D3D12_STENCIL_OP_INCR_SAT;
        case NkStencilOp::NK_DECR_CLAMP: return D3D12_STENCIL_OP_DECR_SAT;
        case NkStencilOp::NK_INVERT:    return D3D12_STENCIL_OP_INVERT;
        case NkStencilOp::NK_INCR_WRAP:  return D3D12_STENCIL_OP_INCR;
        case NkStencilOp::NK_DECR_WRAP:  return D3D12_STENCIL_OP_DECR;
        default:                     return D3D12_STENCIL_OP_KEEP;
    }
}
D3D12_PRIMITIVE_TOPOLOGY NkDeviceDX12::ToDX12Topology(NkPrimitiveTopology t) {
    switch (t) {
        case NkPrimitiveTopology::NK_TRIANGLE_LIST:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case NkPrimitiveTopology::NK_TRIANGLE_STRIP: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case NkPrimitiveTopology::NK_LINE_LIST:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case NkPrimitiveTopology::NK_LINE_STRIP:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case NkPrimitiveTopology::NK_POINT_LIST:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case NkPrimitiveTopology::NK_PATCH_LIST:     return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
        default:                                 return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}
D3D12_PRIMITIVE_TOPOLOGY_TYPE NkDeviceDX12::ToDX12TopologyType(NkPrimitiveTopology t) {
    switch (t) {
        case NkPrimitiveTopology::NK_POINT_LIST:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case NkPrimitiveTopology::NK_LINE_LIST:
        case NkPrimitiveTopology::NK_LINE_STRIP:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case NkPrimitiveTopology::NK_PATCH_LIST:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        default:                                 return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}
D3D12_FILTER NkDeviceDX12::ToDX12Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp) {
    if (cmp) return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    bool linMag = (mag == NkFilter::NK_LINEAR);
    bool linMin = (min == NkFilter::NK_LINEAR);
    bool linMip = (mip == NkMipFilter::NK_LINEAR);
    if (linMag && linMin && linMip) return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    if (!linMag && !linMin && !linMip) return D3D12_FILTER_MIN_MAG_MIP_POINT;
    return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}
D3D12_TEXTURE_ADDRESS_MODE NkDeviceDX12::ToDX12Address(NkAddressMode a) {
    switch (a) {
        case NkAddressMode::NK_REPEAT:         return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case NkAddressMode::NK_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case NkAddressMode::NK_CLAMP_TO_EDGE:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case NkAddressMode::NK_CLAMP_TO_BORDER:  return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:                            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}
D3D12_RESOURCE_STATES NkDeviceDX12::ToDX12State(NkResourceState s) {
    switch (s) {
        case NkResourceState::NK_COMMON:          return D3D12_RESOURCE_STATE_COMMON;
        case NkResourceState::NK_VERTEX_BUFFER:    return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case NkResourceState::NK_INDEX_BUFFER:     return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case NkResourceState::NK_UNIFORM_BUFFER:   return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case NkResourceState::NK_UNORDERED_ACCESS: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case NkResourceState::NK_SHADER_READ:      return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case NkResourceState::NK_RENDER_TARGET:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case NkResourceState::NK_DEPTH_WRITE:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case NkResourceState::NK_DEPTH_READ:       return D3D12_RESOURCE_STATE_DEPTH_READ;
        case NkResourceState::NK_TRANSFER_SRC:     return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case NkResourceState::NK_TRANSFER_DST:     return D3D12_RESOURCE_STATE_COPY_DEST;
        case NkResourceState::NK_INDIRECT_ARG:     return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        case NkResourceState::NK_PRESENT:         return D3D12_RESOURCE_STATE_PRESENT;
        default:                               return D3D12_RESOURCE_STATE_COMMON;
    }
}

// =============================================================================
// Swapchain (NkISwapchain lifecycle) — NkDX12Swapchain
// =============================================================================
NkISwapchain* NkDeviceDX12::CreateSwapchain(NkIGraphicsContext* ctx,
                                              const NkSwapchainDesc& desc)
{
    auto* sc = new NkDX12Swapchain(this);
    if (!sc->Initialize(ctx, desc)) { delete sc; return nullptr; }
    return sc;
}
void NkDeviceDX12::DestroySwapchain(NkISwapchain*& sc) {
    if (!sc) return;
    sc->Shutdown(); delete sc; sc = nullptr;
}

// =============================================================================
// Semaphores DX12 — ID3D12Fence (sync GPU-GPU via valeur monotone)
// En D3D12, les semaphores GPU-GPU s'implémentent avec ID3D12Fence :
//   - Signal(fence, value) depuis la queue producteur
//   - Wait(fence, value)   depuis la queue consommateur
// On réutilise la map mFences existante (DX12Fence).
// =============================================================================
NkSemaphoreHandle NkDeviceDX12::CreateGpuSemaphore() {
    ComPtr<ID3D12Fence> fence;
    HRESULT hr = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&fence));
    if (FAILED(hr)) {
        logger_src.Infof("[NkDeviceDX12] CreateGpuSemaphore: CreateFence échoué (hr=0x%X)\n",
                         (unsigned)hr);
        return NkSemaphoreHandle::Null();
    }

    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
        logger_src.Infof("[NkDeviceDX12] CreateGpuSemaphore: CreateEvent échoué\n");
        return NkSemaphoreHandle::Null();
    }

    NkScopedLock lock(mMutex);
    uint64 hid = NextId();
    DX12Fence f; f.fence = std::move(fence); f.value = 0; f.event = event;
    mFences[hid] = std::move(f);
    NkSemaphoreHandle h; h.id = hid; return h;
}
void NkDeviceDX12::DestroySemaphore(NkSemaphoreHandle& h) {
    if (!h.IsValid()) return;
    NkScopedLock lock(mMutex);
    auto it = mFences.Find(h.id);
    if (it != nullptr) {
        if (it->event) CloseHandle(it->event);
        mFences.Erase(h.id);
    }
    h.id = 0;
}

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED



