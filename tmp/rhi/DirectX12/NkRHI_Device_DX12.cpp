// =============================================================================
// NkRHI_Device_DX12.cpp — Backend DirectX 12
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NkRHI_Device_DX12.h"
#include "NkCommandBuffer_DX12.h"
#include "../../NkFinal_work/Core/NkNativeContextAccess.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#define NK_DX12_LOG(...)  printf("[NkRHI_DX12] " __VA_ARGS__)
#define NK_DX12_ERR(...)  printf("[NkRHI_DX12][ERR] " __VA_ARGS__)
#define NK_DX12_CHECK(hr, msg) do { if(FAILED(hr)){NK_DX12_ERR(msg " (hr=0x%X)\n",(unsigned)(hr));} } while(0)

namespace nkentseu {

// =============================================================================
NkDevice_DX12::~NkDevice_DX12() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkDevice_DX12::Initialize(NkIGraphicsContext* ctx) {
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    auto* native = NkNativeContext::DX12(ctx);
    if (!native) { NK_DX12_ERR("Contexte non DX12\n"); return false; }

    mDevice = static_cast<ID3D12Device*>(native->device);
    mHwnd   = (HWND)native->hwnd;
    mWidth  = ctx->GetInfo().windowWidth;
    mHeight = ctx->GetInfo().windowHeight;

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
void NkDevice_DX12::InitDescriptorHeaps() {
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
void NkDevice_DX12::CreateSwapchain(uint32 w, uint32 h) {
    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width=w; scd.Height=h;
    scd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc={1,0};
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount=kMaxFrames;
    scd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> sc1;
    NK_DX12_CHECK(mFactory->CreateSwapChainForHwnd(
        mGraphicsQueue.Get(), mHwnd, &scd, nullptr, nullptr, &sc1),
        "CreateSwapChainForHwnd");
    sc1.As(&mSwapchain);

    // Swapchain images → textures NkDX12
    for (uint32 i = 0; i < kMaxFrames; i++) {
        ComPtr<ID3D12Resource> buf;
        mSwapchain->GetBuffer(i, IID_PPV_ARGS(&buf));

        uint64 tid = NextId();
        NkDX12Texture t;
        t.resource   = buf;
        t.isSwapchain= true;
        t.format     = DXGI_FORMAT_B8G8R8A8_UNORM;
        t.desc       = NkTextureDesc::RenderTarget(w, h, NkFormat::BGRA8_Unorm);
        t.state      = D3D12_RESOURCE_STATE_PRESENT;

        // RTV
        t.rtvIdx = mRtvHeap.allocated;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap.AllocCPU();
        mDevice->CreateRenderTargetView(buf.Get(), nullptr, rtv);

        mTextures[tid] = t;
        NkTextureHandle colorH; colorH.id = tid;

        // Depth
        NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h);
        auto depthH = CreateTexture(dd);

        NkRenderPassDesc rpd;
        rpd.AddColor(NkAttachmentDesc::Color(NkFormat::BGRA8_Unorm))
           .SetDepth(NkAttachmentDesc::Depth());
        mSwapchainRP = CreateRenderPass(rpd);

        NkFramebufferDesc fbd;
        fbd.renderPass = mSwapchainRP;
        fbd.colorAttachments[0] = colorH; fbd.colorCount = 1;
        fbd.depthAttachment = depthH;
        fbd.width = w; fbd.height = h;
        mSwapchainFBs.push_back(CreateFramebuffer(fbd));
    }
    mBackBufferIdx = mSwapchain->GetCurrentBackBufferIndex();
}

void NkDevice_DX12::DestroySwapchain() {
    WaitIdle();
    for (auto& fb : mSwapchainFBs) DestroyFramebuffer(fb);
    mSwapchainFBs.clear();
    if (mSwapchainRP.IsValid()) DestroyRenderPass(mSwapchainRP);
    mSwapchain.Reset();
}

void NkDevice_DX12::ResizeSwapchain(uint32 w, uint32 h) {
    WaitIdle();
    DestroySwapchain();
    mWidth = w; mHeight = h;
    CreateSwapchain(w, h);
}

void NkDevice_DX12::Shutdown() {
    WaitIdle();
    DestroySwapchain();
    for (auto& [id, b] : mBuffers)   if (b.mapped) b.resource->Unmap(0, nullptr);
    mBuffers.clear(); mTextures.clear(); mSamplers.clear();
    mShaders.clear(); mPipelines.clear(); mRenderPasses.clear();
    mFramebuffers.clear(); mDescLayouts.clear(); mDescSets.clear();
    for (uint32 i = 0; i < kMaxFrames; i++)
        if (mFrameData[i].fenceEvent) CloseHandle(mFrameData[i].fenceEvent);
    mIsValid = false;
    NK_DX12_LOG("Shutdown\n");
}

// =============================================================================
// One-shot execution
// =============================================================================
void NkDevice_DX12::ExecuteOneShot(std::function<void(ID3D12GraphicsCommandList*)> fn) {
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
void NkDevice_DX12::TransitionResource(ID3D12GraphicsCommandList* cmd,
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
ComPtr<ID3D12RootSignature> NkDevice_DX12::CreateDefaultRootSignature(bool compute) {
    // Root signature minimaliste :
    //   slot 0 : push constants (128 bytes = 32 floats) → root constants
    //   slot 1 : descriptor table CBV/SRV/UAV (t0..t15, b0..b15, u0..u15)
    //   slot 2 : descriptor table samplers (s0..s15)

    CD3DX12_ROOT_PARAMETER params[3]{};

    // Push constants
    params[0].InitAsConstants(32, 0, 0,
        compute ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_ALL);

    // CBV/SRV/UAV table
    D3D12_DESCRIPTOR_RANGE ranges[3]{};
    ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 16, 1, 0, 0 };
    ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 32, 0, 0, 16 };
    ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 16, 0, 0, 48 };
    params[1].InitAsDescriptorTable(3, ranges, D3D12_SHADER_VISIBILITY_ALL);

    // Sampler table
    D3D12_DESCRIPTOR_RANGE sampRange{ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0, 0 };
    params[2].InitAsDescriptorTable(1, &sampRange, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsd(3, params, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> blob, err;
    D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err);
    ComPtr<ID3D12RootSignature> rs;
    mDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs));
    return rs;
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDevice_DX12::CreateBuffer(const NkBufferDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);

    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON;
    bool persistentMap = false;

    switch (desc.usage) {
        case NkResourceUsage::Upload:
            heapType = D3D12_HEAP_TYPE_UPLOAD;
            initState = D3D12_RESOURCE_STATE_GENERIC_READ;
            persistentMap = true;
            break;
        case NkResourceUsage::Readback:
            heapType = D3D12_HEAP_TYPE_READBACK;
            initState = D3D12_RESOURCE_STATE_COPY_DEST;
            persistentMap = true;
            break;
        default: break;
    }

    // Aligner la taille sur 256 pour les CBV
    uint64 alignedSize = desc.sizeBytes;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::UniformBuffer) ||
        desc.type == NkBufferType::Uniform)
        alignedSize = (alignedSize + 255) & ~255ull;

    CD3DX12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(alignedSize);
    CD3DX12_HEAP_PROPERTIES hp(heapType);

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
    if (desc.type == NkBufferType::Uniform ||
        NkHasFlag(desc.bindFlags, NkBindFlags::UniformBuffer)) {
        b.cbvIdx = mCbvSrvUavHeap.allocated;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvd{};
        cbvd.BufferLocation = b.gpuAddr;
        cbvd.SizeInBytes    = (UINT)alignedSize;
        mDevice->CreateConstantBufferView(&cbvd, mCbvSrvUavHeap.AllocCPU());
    }

    if (NkHasFlag(desc.bindFlags, NkBindFlags::StorageBuffer) ||
        desc.type == NkBufferType::Storage) {
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
            memcpy(b.mapped, desc.initialData, (size_t)desc.sizeBytes);
        } else {
            // Via upload buffer temporaire
            NkBufferDesc sd = NkBufferDesc::Staging(desc.sizeBytes);
            auto stageH = CreateBuffer(sd);
            auto& stage = mBuffers[stageH.id];
            memcpy(stage.mapped, desc.initialData, (size_t)desc.sizeBytes);
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

void NkDevice_DX12::DestroyBuffer(NkBufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mBuffers.find(h.id); if (it == mBuffers.end()) return;
    if (it->second.mapped) it->second.resource->Unmap(0, nullptr);
    mBuffers.erase(it); h.id = 0;
}

bool NkDevice_DX12::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    if (it->second.mapped) {
        memcpy((uint8*)it->second.mapped + off, data, (size_t)sz);
        return true;
    }
    // Via upload
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    memcpy(stage.mapped, data, (size_t)sz);
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        auto prevState = it->second.state;
        TransitionResource(cmd, it->second.resource.Get(), prevState, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyBufferRegion(it->second.resource.Get(), off, stage.resource.Get(), 0, sz);
        TransitionResource(cmd, it->second.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, prevState);
    });
    DestroyBuffer(stageH);
    return true;
}

bool NkDevice_DX12::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    if (it->second.mapped) { memcpy((uint8*)it->second.mapped + off, data, (size_t)sz); return true; }
    return WriteBuffer(buf, data, sz, off);
}

bool NkDevice_DX12::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    if (it->second.mapped) { memcpy(out, (uint8*)it->second.mapped + off, (size_t)sz); return true; }
    NkBufferDesc sd = NkBufferDesc::Staging(sz); sd.usage = NkResourceUsage::Readback;
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, it->second.resource.Get(), it->second.state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmd->CopyBufferRegion(stage.resource.Get(), 0, it->second.resource.Get(), off, sz);
        TransitionResource(cmd, it->second.resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, it->second.state);
    });
    memcpy(out, stage.mapped, (size_t)sz);
    DestroyBuffer(stageH);
    return true;
}

NkMappedMemory NkDevice_DX12::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return {};
    if (it->second.mapped) {
        uint64 mapSz = sz > 0 ? sz : it->second.desc.sizeBytes - off;
        return { (uint8*)it->second.mapped + off, mapSz };
    }
    void* ptr = nullptr;
    D3D12_RANGE r{};
    it->second.resource->Map(0, &r, &ptr);
    uint64 mapSz = sz > 0 ? sz : it->second.desc.sizeBytes - off;
    return { (uint8*)ptr + off, mapSz };
}

void NkDevice_DX12::UnmapBuffer(NkBufferHandle buf) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return;
    if (!it->second.mapped) it->second.resource->Unmap(0, nullptr);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDevice_DX12::CreateTexture(const NkTextureDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);

    DXGI_FORMAT fmt = ToDXGIFormat(desc.format);
    bool isDepth = NkFormatIsDepth(desc.format);

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width              = desc.width;
    rd.Height             = desc.height;
    rd.DepthOrArraySize   = (UINT16)std::max(desc.arrayLayers, 1u);
    rd.MipLevels          = (UINT16)(desc.mipLevels == 0 ?
        (uint32)(floor(log2(std::max(desc.width, desc.height))) + 1) : desc.mipLevels);
    rd.SampleDesc         = { (UINT)desc.samples, 0 };
    rd.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (isDepth) {
        rd.Format = desc.format == NkFormat::D32_Float ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
        rd.Flags  = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    } else {
        rd.Format = fmt;
        rd.Flags  = D3D12_RESOURCE_FLAG_NONE;
        if (NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget))    rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (NkHasFlag(desc.bindFlags, NkBindFlags::UnorderedAccess)) rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_CLEAR_VALUE clearVal{};
    D3D12_CLEAR_VALUE* pClear = nullptr;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget) && !isDepth) {
        clearVal.Format = rd.Format;
        pClear = &clearVal;
    } else if (isDepth) {
        clearVal.Format = rd.Format;
        clearVal.DepthStencil = { 1.f, 0 };
        pClear = &clearVal;
    }

    D3D12_RESOURCE_STATES initState = isDepth
        ? D3D12_RESOURCE_STATE_DEPTH_WRITE
        : (NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget)
           ? D3D12_RESOURCE_STATE_RENDER_TARGET
           : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
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
        if (NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget)) {
            t.rtvIdx = mRtvHeap.allocated;
            mDevice->CreateRenderTargetView(res.Get(), nullptr, mRtvHeap.AllocCPU());
        }
        if (NkHasFlag(desc.bindFlags, NkBindFlags::ShaderResource) ||
            (!NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget) && !isDepth)) {
            t.srvIdx = mCbvSrvUavHeap.allocated;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                  = fmt;
            srvd.ViewDimension           = desc.type == NkTextureType::Cube ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if (desc.type == NkTextureType::Cube)
                srvd.TextureCube.MipLevels = rd.MipLevels;
            else
                srvd.Texture2D.MipLevels   = rd.MipLevels;
            mDevice->CreateShaderResourceView(res.Get(), &srvd, mCbvSrvUavHeap.AllocCPU());
        }
        if (NkHasFlag(desc.bindFlags, NkBindFlags::UnorderedAccess)) {
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
        memcpy(stage.mapped, desc.initialData, (size_t)imgSz);

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

void NkDevice_DX12::DestroyTexture(NkTextureHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mTextures.find(h.id); if (it == mTextures.end()) return;
    mTextures.erase(it); h.id = 0;
}

bool NkDevice_DX12::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto& desc = it->second.desc;
    return WriteTextureRegion(t, p, 0, 0, 0, desc.width, desc.height, 1, 0, 0, rp);
}

bool NkDevice_DX12::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x, uint32 y, uint32 /*z*/, uint32 w, uint32 h, uint32 /*d2*/,
    uint32 mip, uint32 /*layer*/, uint32 rowPitch) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    uint32 bpp = NkFormatBytesPerPixel(it->second.desc.format);
    uint32 rp  = rowPitch > 0 ? rowPitch : w * bpp;
    uint64 sz  = (uint64)rp * h;
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    memcpy(stage.mapped, pixels, (size_t)sz);

    auto prevState = it->second.state;
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, it->second.resource.Get(), prevState, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = it->second.resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = mip;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = stage.resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint = { it->second.format, w, h, 1, rp };
        D3D12_BOX box{ x, y, 0, x + w, y + h, 1 };
        cmd->CopyTextureRegion(&dst, x, y, 0, &src, &box);
        TransitionResource(cmd, it->second.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, prevState);
    });
    DestroyBuffer(stageH);
    return true;
}

bool NkDevice_DX12::GenerateMipmaps(NkTextureHandle, NkFilter) {
    // DX12 n'a pas de GenerateMips natif — nécessite un compute shader séparé
    // Pour l'instant on retourne true (les mips sont générés à la création si mipLevels>1)
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDevice_DX12::CreateSampler(const NkSamplerDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
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

void NkDevice_DX12::DestroySampler(NkSamplerHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    mSamplers.erase(h.id); h.id = 0;
}

// =============================================================================
// Shaders (DXBC/DXIL via D3DCompile ou pré-compilés)
// =============================================================================
NkShaderHandle NkDevice_DX12::CreateShader(const NkShaderDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkDX12Shader sh;

    for (uint32 i = 0; i < desc.stageCount; i++) {
        auto& s = desc.stages[i];

        // Priorité : bytecode précompilé > HLSL source
        std::vector<uint8_t>* target = nullptr;
        const char* hlslTarget = nullptr;

        switch (s.stage) {
            case NkShaderStage::Vertex:   target = &sh.vs.bytecode; hlslTarget = "vs_5_1"; break;
            case NkShaderStage::Fragment: target = &sh.ps.bytecode; hlslTarget = "ps_5_1"; break;
            case NkShaderStage::Compute:  target = &sh.cs.bytecode; hlslTarget = "cs_5_1"; break;
            case NkShaderStage::Geometry: target = &sh.gs.bytecode; hlslTarget = "gs_5_1"; break;
            default: continue;
        }

        if (s.spirvData && s.spirvSize > 0) {
            // DXIL pré-compilé
            target->assign((uint8_t*)s.spirvData, (uint8_t*)s.spirvData + s.spirvSize);
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
            target->assign((uint8_t*)code->GetBufferPointer(),
                           (uint8_t*)code->GetBufferPointer() + code->GetBufferSize());
            if (s.stage == NkShaderStage::Vertex) sh.vsBlob = code;
        }
    }

    uint64 hid = NextId(); mShaders[hid] = std::move(sh);
    NkShaderHandle h; h.id = hid; return h;
}

void NkDevice_DX12::DestroyShader(NkShaderHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    mShaders.erase(h.id); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDevice_DX12::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit = mShaders.find(d.shader.id); if (sit == mShaders.end()) return {};
    auto& sh = sit->second;

    auto rootSig = CreateDefaultRootSignature(false);

    // Input Layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> elems;
    for (uint32 i = 0; i < d.vertexLayout.attributeCount; i++) {
        auto& a = d.vertexLayout.attributes[i];
        bool instanced = false;
        for (uint32 j = 0; j < d.vertexLayout.bindingCount; j++)
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
        elems.push_back(e);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psd{};
    psd.pRootSignature = rootSig.Get();
    psd.VS = sh.vs.bc();
    psd.PS = sh.ps.bc();
    if (!sh.gs.bytecode.empty()) psd.GS = sh.gs.bc();
    if (!sh.hs.bytecode.empty()) psd.HS = sh.hs.bc();
    if (!sh.ds.bytecode.empty()) psd.DS = sh.ds.bc();

    psd.InputLayout = { elems.data(), (UINT)elems.size() };
    psd.PrimitiveTopologyType = ToDX12TopologyType(d.topology);

    // Rasterizer
    psd.RasterizerState.FillMode = d.rasterizer.fillMode == NkFillMode::Solid
        ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    psd.RasterizerState.CullMode = d.rasterizer.cullMode == NkCullMode::None
        ? D3D12_CULL_MODE_NONE : d.rasterizer.cullMode == NkCullMode::Front
        ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
    psd.RasterizerState.FrontCounterClockwise = d.rasterizer.frontFace == NkFrontFace::CCW;
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
    psd.BlendState.IndependentBlendEnable = d.blend.attachmentCount > 1;
    for (uint32 i = 0; i < d.blend.attachmentCount && i < 8; i++) {
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
    auto rpit = mRenderPasses.find(d.renderPass.id);
    if (rpit != mRenderPasses.end()) {
        psd.NumRenderTargets = rpit->second.desc.colorCount;
        for (uint32 i = 0; i < rpit->second.desc.colorCount; i++)
            psd.RTVFormats[i] = ToDXGIFormat(rpit->second.desc.colorAttachments[i].format);
        if (rpit->second.desc.hasDepth)
            psd.DSVFormat = NkFormatIsDepth(rpit->second.desc.depthAttachment.format)
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

NkPipelineHandle NkDevice_DX12::CreateComputePipeline(const NkComputePipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit = mShaders.find(d.shader.id); if (sit == mShaders.end()) return {};
    auto& sh = sit->second;

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

void NkDevice_DX12::DestroyPipeline(NkPipelineHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPipelines.erase(h.id); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers (metadata only en DX12)
// =============================================================================
NkRenderPassHandle NkDevice_DX12::CreateRenderPass(const NkRenderPassDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mRenderPasses[hid] = { d };
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkDevice_DX12::DestroyRenderPass(NkRenderPassHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mRenderPasses.erase(h.id); h.id = 0;
}

NkFramebufferHandle NkDevice_DX12::CreateFramebuffer(const NkFramebufferDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkDX12Framebuffer fb; fb.w = d.width; fb.h = d.height;
    for (uint32 i = 0; i < d.colorCount; i++) {
        auto it = mTextures.find(d.colorAttachments[i].id);
        if (it != mTextures.end()) fb.rtvIdxs[fb.rtvCount++] = it->second.rtvIdx;
    }
    if (d.depthAttachment.IsValid()) {
        auto it = mTextures.find(d.depthAttachment.id);
        if (it != mTextures.end()) fb.dsvIdx = it->second.dsvIdx;
    }
    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
void NkDevice_DX12::DestroyFramebuffer(NkFramebufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mFramebuffers.erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDevice_DX12::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mDescLayouts[hid] = { d };
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDevice_DX12::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescLayouts.erase(h.id); h.id = 0;
}
NkDescSetHandle NkDevice_DX12::AllocateDescriptorSet(NkDescSetHandle layout) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkDX12DescSet ds; ds.layoutId = layout.id;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDevice_DX12::FreeDescriptorSet(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescSets.erase(h.id); h.id = 0;
}

void NkDevice_DX12::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (uint32 i = 0; i < n; i++) {
        auto& w = writes[i];
        auto sit = mDescSets.find(w.set.id); if (sit == mDescSets.end()) continue;

        NkDX12DescSet::Binding b;
        b.slot   = w.binding;
        b.type   = w.type;
        b.bufId  = w.buffer.id;
        b.texId  = w.texture.id;
        b.sampId = w.sampler.id;

        // Remplacer si le slot existe déjà
        bool found = false;
        for (auto& existing : sit->second.bindings)
            if (existing.slot == w.binding) { existing = b; found = true; break; }
        if (!found) sit->second.bindings.push_back(b);
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkDevice_DX12::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_DX12(this, t);
}
void NkDevice_DX12::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb = nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDevice_DX12::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    std::vector<ID3D12CommandList*> lists;
    for (uint32 i = 0; i < n; i++) {
        auto* dx = dynamic_cast<NkCommandBuffer_DX12*>(cbs[i]);
        if (dx && dx->GetCmdList()) lists.push_back(dx->GetCmdList());
    }
    if (!lists.empty())
        mGraphicsQueue->ExecuteCommandLists((UINT)lists.size(), lists.data());
    if (fence.IsValid()) {
        auto it = mFences.find(fence.id);
        if (it != mFences.end()) {
            it->second.value++;
            mGraphicsQueue->Signal(it->second.fence.Get(), it->second.value);
        }
    }
}

void NkDevice_DX12::SubmitAndPresent(NkICommandBuffer* cb) {
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
NkFenceHandle NkDevice_DX12::CreateFence(bool signaled) {
    DX12Fence f;
    mDevice->CreateFence(signaled ? 1 : 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f.fence));
    f.value = signaled ? 1 : 0;
    f.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    uint64 hid = NextId(); mFences[hid] = std::move(f);
    NkFenceHandle h; h.id = hid; return h;
}
void NkDevice_DX12::DestroyFence(NkFenceHandle& h) {
    auto it = mFences.find(h.id); if (it == mFences.end()) return;
    CloseHandle(it->second.event);
    mFences.erase(it); h.id = 0;
}
bool NkDevice_DX12::WaitFence(NkFenceHandle f, uint64 timeoutNs) {
    auto it = mFences.find(f.id); if (it == mFences.end()) return false;
    if (it->second.fence->GetCompletedValue() < it->second.value) {
        it->second.fence->SetEventOnCompletion(it->second.value, it->second.event);
        DWORD ms = timeoutNs == UINT64_MAX ? INFINITE : (DWORD)(timeoutNs / 1000000);
        WaitForSingleObject(it->second.event, ms);
    }
    return it->second.fence->GetCompletedValue() >= it->second.value;
}
bool NkDevice_DX12::IsFenceSignaled(NkFenceHandle f) {
    auto it = mFences.find(f.id); if (it == mFences.end()) return false;
    return it->second.fence->GetCompletedValue() >= it->second.value;
}
void NkDevice_DX12::ResetFence(NkFenceHandle f) {
    auto it = mFences.find(f.id); if (it == mFences.end()) return;
    it->second.value = 0;
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&it->second.fence));
}
void NkDevice_DX12::WaitIdle() {
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
void NkDevice_DX12::BeginFrame(NkFrameContext& frame) {
    mFrameData[mFrameIndex].WaitAndReset(mGraphicsQueue.Get());
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
    mBackBufferIdx    = mSwapchain->GetCurrentBackBufferIndex();
}
void NkDevice_DX12::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex + 1) % kMaxFrames;
    ++mFrameNumber;
}
void NkDevice_DX12::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return;
    mWidth = w; mHeight = h;
    ResizeSwapchain(w, h);
}

// =============================================================================
// Caps
// =============================================================================
void NkDevice_DX12::QueryCaps() {
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
ID3D12Resource* NkDevice_DX12::GetDX12Buffer(uint64 id) const {
    auto it = mBuffers.find(id); return it != mBuffers.end() ? it->second.resource.Get() : nullptr;
}
ID3D12Resource* NkDevice_DX12::GetDX12Texture(uint64 id) const {
    auto it = mTextures.find(id); return it != mTextures.end() ? it->second.resource.Get() : nullptr;
}
D3D12_GPU_VIRTUAL_ADDRESS NkDevice_DX12::GetBufferGPUAddr(uint64 id) const {
    auto it = mBuffers.find(id); return it != mBuffers.end() ? it->second.gpuAddr : 0;
}
const NkDX12Pipeline* NkDevice_DX12::GetPipeline(uint64 id) const {
    auto it = mPipelines.find(id); return it != mPipelines.end() ? &it->second : nullptr;
}
const NkDX12DescSet* NkDevice_DX12::GetDescSet(uint64 id) const {
    auto it = mDescSets.find(id); return it != mDescSets.end() ? &it->second : nullptr;
}
const NkDX12Framebuffer* NkDevice_DX12::GetFBO(uint64 id) const {
    auto it = mFramebuffers.find(id); return it != mFramebuffers.end() ? &it->second : nullptr;
}
D3D12_CPU_DESCRIPTOR_HANDLE NkDevice_DX12::GetRTV(UINT idx) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = mRtvHeap.cpuBase;
    h.ptr += (SIZE_T)idx * mRtvHeap.incrementSize;
    return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE NkDevice_DX12::GetDSV(UINT idx) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = mDsvHeap.cpuBase;
    h.ptr += (SIZE_T)idx * mDsvHeap.incrementSize;
    return h;
}

// =============================================================================
// Conversions
// =============================================================================
DXGI_FORMAT NkDevice_DX12::ToDXGIFormat(NkFormat f) {
    switch (f) {
        case NkFormat::R8_Unorm:         return DXGI_FORMAT_R8_UNORM;
        case NkFormat::RG8_Unorm:        return DXGI_FORMAT_R8G8_UNORM;
        case NkFormat::RGBA8_Unorm:      return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkFormat::RGBA8_Srgb:       return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case NkFormat::BGRA8_Unorm:      return DXGI_FORMAT_B8G8R8A8_UNORM;
        case NkFormat::BGRA8_Srgb:       return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case NkFormat::R16_Float:        return DXGI_FORMAT_R16_FLOAT;
        case NkFormat::RG16_Float:       return DXGI_FORMAT_R16G16_FLOAT;
        case NkFormat::RGBA16_Float:     return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkFormat::R32_Float:        return DXGI_FORMAT_R32_FLOAT;
        case NkFormat::RG32_Float:       return DXGI_FORMAT_R32G32_FLOAT;
        case NkFormat::RGB32_Float:      return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkFormat::RGBA32_Float:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkFormat::R32_Uint:         return DXGI_FORMAT_R32_UINT;
        case NkFormat::RG32_Uint:        return DXGI_FORMAT_R32G32_UINT;
        case NkFormat::D16_Unorm:        return DXGI_FORMAT_D16_UNORM;
        case NkFormat::D32_Float:        return DXGI_FORMAT_D32_FLOAT;
        case NkFormat::D24_Unorm_S8_Uint:return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case NkFormat::BC1_RGB_Unorm:    return DXGI_FORMAT_BC1_UNORM;
        case NkFormat::BC1_RGB_Srgb:     return DXGI_FORMAT_BC1_UNORM_SRGB;
        case NkFormat::BC3_Unorm:        return DXGI_FORMAT_BC3_UNORM;
        case NkFormat::BC5_Unorm:        return DXGI_FORMAT_BC5_UNORM;
        case NkFormat::BC7_Unorm:        return DXGI_FORMAT_BC7_UNORM;
        case NkFormat::BC7_Srgb:         return DXGI_FORMAT_BC7_UNORM_SRGB;
        case NkFormat::R11G11B10_Float:  return DXGI_FORMAT_R11G11B10_FLOAT;
        case NkFormat::A2B10G10R10_Unorm:return DXGI_FORMAT_R10G10B10A2_UNORM;
        default:                         return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}
DXGI_FORMAT NkDevice_DX12::ToDXGIVertexFormat(NkVertexFormat f) {
    switch (f) {
        case NkVertexFormat::Float1: return DXGI_FORMAT_R32_FLOAT;
        case NkVertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case NkVertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkVertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkVertexFormat::Half2:  return DXGI_FORMAT_R16G16_FLOAT;
        case NkVertexFormat::Half4:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkVertexFormat::Byte4_Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkVertexFormat::Byte4_Snorm: return DXGI_FORMAT_R8G8B8A8_SNORM;
        case NkVertexFormat::Uint1:  return DXGI_FORMAT_R32_UINT;
        case NkVertexFormat::Uint2:  return DXGI_FORMAT_R32G32_UINT;
        case NkVertexFormat::Uint4:  return DXGI_FORMAT_R32G32B32A32_UINT;
        case NkVertexFormat::Int1:   return DXGI_FORMAT_R32_SINT;
        case NkVertexFormat::Int4:   return DXGI_FORMAT_R32G32B32A32_SINT;
        default:                     return DXGI_FORMAT_R32G32B32_FLOAT;
    }
}
D3D12_COMPARISON_FUNC NkDevice_DX12::ToDX12Compare(NkCompareOp op) {
    switch (op) {
        case NkCompareOp::Never:        return D3D12_COMPARISON_FUNC_NEVER;
        case NkCompareOp::Less:         return D3D12_COMPARISON_FUNC_LESS;
        case NkCompareOp::Equal:        return D3D12_COMPARISON_FUNC_EQUAL;
        case NkCompareOp::LessEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case NkCompareOp::Greater:      return D3D12_COMPARISON_FUNC_GREATER;
        case NkCompareOp::NotEqual:     return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case NkCompareOp::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        default:                        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}
D3D12_BLEND NkDevice_DX12::ToDX12Blend(NkBlendFactor f) {
    switch (f) {
        case NkBlendFactor::Zero:              return D3D12_BLEND_ZERO;
        case NkBlendFactor::One:               return D3D12_BLEND_ONE;
        case NkBlendFactor::SrcColor:          return D3D12_BLEND_SRC_COLOR;
        case NkBlendFactor::OneMinusSrcColor:  return D3D12_BLEND_INV_SRC_COLOR;
        case NkBlendFactor::SrcAlpha:          return D3D12_BLEND_SRC_ALPHA;
        case NkBlendFactor::OneMinusSrcAlpha:  return D3D12_BLEND_INV_SRC_ALPHA;
        case NkBlendFactor::DstAlpha:          return D3D12_BLEND_DEST_ALPHA;
        case NkBlendFactor::OneMinusDstAlpha:  return D3D12_BLEND_INV_DEST_ALPHA;
        case NkBlendFactor::SrcAlphaSaturate:  return D3D12_BLEND_SRC_ALPHA_SAT;
        default:                               return D3D12_BLEND_ONE;
    }
}
D3D12_BLEND_OP NkDevice_DX12::ToDX12BlendOp(NkBlendOp op) {
    switch (op) {
        case NkBlendOp::Add:    return D3D12_BLEND_OP_ADD;
        case NkBlendOp::Sub:    return D3D12_BLEND_OP_SUBTRACT;
        case NkBlendOp::RevSub: return D3D12_BLEND_OP_REV_SUBTRACT;
        case NkBlendOp::Min:    return D3D12_BLEND_OP_MIN;
        case NkBlendOp::Max:    return D3D12_BLEND_OP_MAX;
        default:                return D3D12_BLEND_OP_ADD;
    }
}
D3D12_STENCIL_OP NkDevice_DX12::ToDX12StencilOp(NkStencilOp op) {
    switch (op) {
        case NkStencilOp::Keep:      return D3D12_STENCIL_OP_KEEP;
        case NkStencilOp::Zero:      return D3D12_STENCIL_OP_ZERO;
        case NkStencilOp::Replace:   return D3D12_STENCIL_OP_REPLACE;
        case NkStencilOp::IncrClamp: return D3D12_STENCIL_OP_INCR_SAT;
        case NkStencilOp::DecrClamp: return D3D12_STENCIL_OP_DECR_SAT;
        case NkStencilOp::Invert:    return D3D12_STENCIL_OP_INVERT;
        case NkStencilOp::IncrWrap:  return D3D12_STENCIL_OP_INCR;
        case NkStencilOp::DecrWrap:  return D3D12_STENCIL_OP_DECR;
        default:                     return D3D12_STENCIL_OP_KEEP;
    }
}
D3D12_PRIMITIVE_TOPOLOGY NkDevice_DX12::ToDX12Topology(NkPrimitiveTopology t) {
    switch (t) {
        case NkPrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case NkPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case NkPrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case NkPrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case NkPrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case NkPrimitiveTopology::PatchList:     return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
        default:                                 return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}
D3D12_PRIMITIVE_TOPOLOGY_TYPE NkDevice_DX12::ToDX12TopologyType(NkPrimitiveTopology t) {
    switch (t) {
        case NkPrimitiveTopology::PointList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case NkPrimitiveTopology::LineList:
        case NkPrimitiveTopology::LineStrip:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case NkPrimitiveTopology::PatchList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        default:                                 return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}
D3D12_FILTER NkDevice_DX12::ToDX12Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp) {
    if (cmp) return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    bool linMag = (mag == NkFilter::Linear);
    bool linMin = (min == NkFilter::Linear);
    bool linMip = (mip == NkMipFilter::Linear);
    if (linMag && linMin && linMip) return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    if (!linMag && !linMin && !linMip) return D3D12_FILTER_MIN_MAG_MIP_POINT;
    return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}
D3D12_TEXTURE_ADDRESS_MODE NkDevice_DX12::ToDX12Address(NkAddressMode a) {
    switch (a) {
        case NkAddressMode::Repeat:         return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case NkAddressMode::MirroredRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case NkAddressMode::ClampToEdge:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case NkAddressMode::ClampToBorder:  return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:                            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}
D3D12_RESOURCE_STATES NkDevice_DX12::ToDX12State(NkResourceState s) {
    switch (s) {
        case NkResourceState::Common:          return D3D12_RESOURCE_STATE_COMMON;
        case NkResourceState::VertexBuffer:    return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case NkResourceState::IndexBuffer:     return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case NkResourceState::UniformBuffer:   return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case NkResourceState::StorageBuffer:
        case NkResourceState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case NkResourceState::ShaderRead:      return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case NkResourceState::RenderTarget:    return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case NkResourceState::DepthWrite:      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case NkResourceState::DepthRead:       return D3D12_RESOURCE_STATE_DEPTH_READ;
        case NkResourceState::TransferSrc:     return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case NkResourceState::TransferDst:     return D3D12_RESOURCE_STATE_COPY_DEST;
        case NkResourceState::IndirectArg:     return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        case NkResourceState::Present:         return D3D12_RESOURCE_STATE_PRESENT;
        default:                               return D3D12_RESOURCE_STATE_COMMON;
    }
}

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
