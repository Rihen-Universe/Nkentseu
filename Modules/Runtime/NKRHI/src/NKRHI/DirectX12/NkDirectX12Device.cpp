// =============================================================================
// NkRHI_Device_DX12.cpp — Backend DirectX 12
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NkDirectX12Device.h"
#include "NkDirectX12CommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <algorithm>
#include <cmath>

#define NK_DX12_LOG(...)  logger_src.Infof("[NkRHI_DX12] " __VA_ARGS__)
#define NK_DX12_ERR(...)  logger_src.Infof("[NkRHI_DX12][ERR] " __VA_ARGS__)
#define NK_DX12_CHECK(hr, msg) do { if(FAILED(hr)){NK_DX12_ERR(msg " (hr=0x%X)\n",(unsigned)(hr));} } while(0)

namespace nkentseu {

// =============================================================================
NkDirectX12Device::~NkDirectX12Device() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkDirectX12Device::Initialize(const NkDeviceInitInfo& init) {
    mInit = init;
    NkGpuPolicy::ApplyPreContext(mInit.context);

    const NkDirectX12Desc& dxCfg = mInit.context.dx12;
    const NkGpuSelectionDesc& gpuCfg = mInit.context.gpu;
    mVsync = dxCfg.vsync;
    mAllowTearing = dxCfg.allowTearing;
    mEnableComputeQueue = dxCfg.enableComputeQueue;
    mSwapchainBufferCount = math::NkMax(2u, math::NkMin<uint32>(dxCfg.swapchainBuffers, MAX_FRAMES));
    mRtvHeapCapacity = math::NkMax(8u, dxCfg.rtvHeapSize);
    mDsvHeapCapacity = math::NkMax(8u, dxCfg.dsvHeapSize);
    mSrvHeapCapacity = math::NkMax(64u, dxCfg.srvHeapSize);
    mSamplerHeapCapacity = math::NkMax(8u, dxCfg.samplerHeapSize);

    mHwnd = nullptr;
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    mHwnd = init.surface.hwnd;
#endif
    if (!mHwnd) {
        NK_DX12_ERR("HWND manquant dans NkDeviceInitInfo.surface\n");
        return false;
    }

    mWidth  = NkDeviceInitWidth(init);
    mHeight = NkDeviceInitHeight(init);
    if (mWidth == 0)  mWidth = 1280;
    if (mHeight == 0) mHeight = 720;

    UINT factoryFlags = 0;
    if (dxCfg.debugDevice) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            if (dxCfg.gpuValidation) {
                ComPtr<ID3D12Debug1> debug1;
                if (SUCCEEDED(debug.As(&debug1))) {
                    debug1->SetEnableGPUBasedValidation(TRUE);
                }
            }
        }
    }
    NK_DX12_CHECK(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&mFactory)),
                  "CreateDXGIFactory2");

    ComPtr<IDXGIAdapter1> adapter;
    int bestScore = -1000000;
    const uint32 preferredAdapterIndex =
        (dxCfg.preferredAdapter != UINT32_MAX)
            ? dxCfg.preferredAdapter
            : ((gpuCfg.adapterIndex >= 0) ? static_cast<uint32>(gpuCfg.adapterIndex) : UINT32_MAX);
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIAdapter1> cand;
        if (mFactory->EnumAdapters1(i, &cand) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        cand->GetDesc1(&desc);
        const bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        if (isSoftware && !gpuCfg.allowSoftwareAdapter) {
            continue;
        }
        if (gpuCfg.vendorPreference != NkGpuVendor::NK_ANY &&
            !NkGpuPolicy::MatchesVendorPciId(static_cast<uint32>(desc.VendorId), gpuCfg.vendorPreference)) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(cand.Get(), D3D_FEATURE_LEVEL_11_0,
                                        __uuidof(ID3D12Device), nullptr))) {
            if (preferredAdapterIndex != UINT32_MAX && i == preferredAdapterIndex) {
                adapter = cand;
                break;
            }
            int score = static_cast<int>(desc.DedicatedVideoMemory >> 20);
            if (gpuCfg.preference == NkGpuPreference::NK_HIGH_PERFORMANCE) {
                score += isSoftware ? -10000 : 1000;
            } else if (gpuCfg.preference == NkGpuPreference::NK_LOW_POWER) {
                score += isSoftware ? -10000 : 200;
            } else {
                score += isSoftware ? -10000 : 700;
            }
            if (score > bestScore) {
                bestScore = score;
                adapter = cand;
            }
        }
    }

    if (!adapter) {
        mFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
    }
    if (!adapter) {
        NK_DX12_ERR("Aucun adapter DX12 disponible\n");
        return false;
    }

    HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&mDevice));
    if (FAILED(hr) || !mDevice) {
        NK_DX12_ERR("D3D12CreateDevice failed (hr=0x%X)\n", (unsigned)hr);
        return false;
    }

    // Command queue graphique
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qd.Flags= D3D12_COMMAND_QUEUE_FLAG_NONE;
    NK_DX12_CHECK(mDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&mGraphicsQueue)),
                  "CreateCommandQueue (graphics)");

    // Command queue compute
    qd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    if (mEnableComputeQueue) {
        mDevice->CreateCommandQueue(&qd, IID_PPV_ARGS(&mComputeQueue));
    }

    BOOL tearingSupported = FALSE;
    if (mFactory) {
        mFactory->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &tearingSupported,
            sizeof(tearingSupported));
    }
    mTearingSupported = tearingSupported == TRUE;

    // Frame data
    for (uint32 i = 0; i < MAX_FRAMES; i++) {
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
    NK_DX12_LOG("Initialisé (%u×%u, %u frames)\n", mWidth, mHeight, MAX_FRAMES);
    return true;
}

// =============================================================================
void NkDirectX12Device::InitDescriptorHeaps() {
    auto makeHeap = [&](NkDX12DescHeap& heap,
                         D3D12_DESCRIPTOR_HEAP_TYPE type,
                         UINT capacity, bool shaderVisible) {
        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type           = type;
        d.NumDescriptors = capacity;
        d.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                         : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = mDevice->CreateDescriptorHeap(&d, IID_PPV_ARGS(&heap.heap));
        if (FAILED(hr) || !heap.heap) {
            heap.capacity = 0;
            heap.allocated = 0;
            heap.incrementSize = 0;
            heap.cpuBase = {};
            heap.gpuBase = {};
            NK_DX12_ERR("CreateDescriptorHeap failed (type=%u, hr=0x%X)\n",
                        (unsigned)type, (unsigned)hr);
            return;
        }
        heap.cpuBase      = heap.heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible)
            heap.gpuBase  = heap.heap->GetGPUDescriptorHandleForHeapStart();
        heap.incrementSize= mDevice->GetDescriptorHandleIncrementSize(type);
        heap.capacity     = capacity;
        heap.allocated    = 0;
    };
    makeHeap(mRtvHeap,       D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         mRtvHeapCapacity,  false);
    makeHeap(mDsvHeap,       D3D12_DESCRIPTOR_HEAP_TYPE_DSV,         mDsvHeapCapacity,  false);
    makeHeap(mCbvSrvUavHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mSrvHeapCapacity,  true);
    makeHeap(mSamplerHeap,   D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     mSamplerHeapCapacity, true);
}

// =============================================================================
void NkDirectX12Device::CreateSwapchain(uint32 w, uint32 h) {
    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width=w; scd.Height=h;
    scd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc={1,0};
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount=mSwapchainBufferCount;
    scd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags=(mAllowTearing && mTearingSupported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> sc1;
    NK_DX12_CHECK(mFactory->CreateSwapChainForHwnd(
        mGraphicsQueue.Get(), mHwnd, &scd, nullptr, nullptr, &sc1),
        "CreateSwapChainForHwnd");
    sc1.As(&mSwapchain);

    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_BGRA8_UNORM))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP = CreateRenderPass(rpd);

    NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h);
    NkTextureHandle depthH = CreateTexture(dd);
    mDepthTexId = depthH.id;

    // Swapchain images → textures NkDX12
    for (uint32 i = 0; i < mSwapchainBufferCount; i++) {
        ComPtr<ID3D12Resource> buf;
        mSwapchain->GetBuffer(i, IID_PPV_ARGS(&buf));

        uint64 tid = NextId();
        NkDX12Texture t;
        t.resource   = buf;
        t.isSwapchain= true;
        t.format     = DXGI_FORMAT_B8G8R8A8_UNORM;
        t.desc       = NkTextureDesc::RenderTarget(w, h, NkGPUFormat::NK_BGRA8_UNORM);
        t.state      = D3D12_RESOURCE_STATE_PRESENT;

        // RTV
        t.rtvIdx = mRtvHeap.allocated;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = mRtvHeap.AllocCPU();
        mDevice->CreateRenderTargetView(buf.Get(), nullptr, rtv);

        mTextures[tid] = t;
        NkTextureHandle colorH; colorH.id = tid;

        NkFramebufferDesc fbd;
        fbd.renderPass = mSwapchainRP;
        fbd.colorAttachments.PushBack(colorH);
        fbd.depthAttachment = depthH;
        fbd.width = w; fbd.height = h;
        mSwapchainFBs.PushBack(CreateFramebuffer(fbd));
    }
    mBackBufferIdx = mSwapchain->GetCurrentBackBufferIndex();
}

void NkDirectX12Device::DestroySwapchain() {
    WaitIdle();
    for (const auto& fb : mSwapchainFBs) {
        auto it = mFramebuffers.Find(fb.id);
        if(!it) continue;
        for (uint32 i = 0; i < it->rtvCount; i++) {
            if (it->colorTexIds[i] != 0) {
                NkTextureHandle th; th.id = it->colorTexIds[i];
                DestroyTexture(th);
            }
        }
    }

    for (auto& fb : mSwapchainFBs) DestroyFramebuffer(fb);
    mSwapchainFBs.Clear();

    if (mDepthTexId != 0) {
        NkTextureHandle depthH; depthH.id = mDepthTexId;
        DestroyTexture(depthH);
        mDepthTexId = 0;
    }

    if (mSwapchainRP.IsValid()) DestroyRenderPass(mSwapchainRP);
    mSwapchain.Reset();
    mBackBufferIdx = 0;
}

void NkDirectX12Device::ResizeSwapchain(uint32 w, uint32 h) {
    WaitIdle();
    DestroySwapchain();
    mWidth = w; mHeight = h;
    CreateSwapchain(w, h);
}

void NkDirectX12Device::Shutdown() {
    WaitIdle();
    DestroySwapchain();
    for (auto& [id, b] : mBuffers)   if (b.mapped) b.resource->Unmap(0, nullptr);
    mBuffers.Clear(); mTextures.Clear(); mSamplers.Clear();
    mShaders.Clear(); mPipelines.Clear(); mRenderPasses.Clear();
    mFramebuffers.Clear(); mDescLayouts.Clear(); mDescSets.Clear();
    for (uint32 i = 0; i < MAX_FRAMES; i++)
        if (mFrameData[i].fenceEvent) CloseHandle(mFrameData[i].fenceEvent);
    mIsValid = false;
    NK_DX12_LOG("Shutdown\n");
}

// =============================================================================
// One-shot execution
// =============================================================================
void NkDirectX12Device::ExecuteOneShot(NkFunction<void(ID3D12GraphicsCommandList*)> fn) {
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
void NkDirectX12Device::TransitionResource(ID3D12GraphicsCommandList* cmd,
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
ComPtr<ID3D12RootSignature> NkDirectX12Device::CreateDefaultRootSignature(bool compute) {
    // Root signature:
    //   0 -> Root constants (b15) pour PushConstants
    //   1 -> Root CBV (b0)
    //   2 -> SRV table (t0..t15)
    //   3 -> UAV table (u0..u15)
    //   4 -> Sampler table (s0..s15)
    (void)compute;

    D3D12_ROOT_PARAMETER params[5]{};

    params[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
    params[0].Constants.ShaderRegister  = 15;
    params[0].Constants.RegisterSpace   = 0;
    params[0].Constants.Num32BitValues  = 32;

    params[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
    params[1].Descriptor.ShaderRegister = 0;
    params[1].Descriptor.RegisterSpace  = 0;

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors                    = 16;
    // Aligne le registre SRV avec les shaders de la démo (uShadowMap : register(t1)).
    srvRange.BaseShaderRegister                = 1;
    srvRange.RegisterSpace                     = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    params[2].ParameterType                    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].ShaderVisibility                 = D3D12_SHADER_VISIBILITY_ALL;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges   = &srvRange;

    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors                    = 16;
    uavRange.BaseShaderRegister                = 0;
    uavRange.RegisterSpace                     = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    params[3].ParameterType                    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[3].ShaderVisibility                 = D3D12_SHADER_VISIBILITY_ALL;
    params[3].DescriptorTable.NumDescriptorRanges = 1;
    params[3].DescriptorTable.pDescriptorRanges   = &uavRange;

    D3D12_DESCRIPTOR_RANGE sampRange{};
    sampRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    sampRange.NumDescriptors                    = 16;
    // Aligne le registre sampler avec les shaders de la démo (uShadowSampler : register(s1)).
    sampRange.BaseShaderRegister                = 1;
    sampRange.RegisterSpace                     = 0;
    sampRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    params[4].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[4].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_ALL;
    params[4].DescriptorTable.NumDescriptorRanges = 1;
    params[4].DescriptorTable.pDescriptorRanges   = &sampRange;

    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters     = 5;
    rsd.pParameters       = params;
    rsd.NumStaticSamplers = 0;
    rsd.pStaticSamplers   = nullptr;
    rsd.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1_0, &blob, &err);
    if (FAILED(hr)) {
        NK_DX12_ERR("D3D12SerializeRootSignature failed (hr=0x%X)\n", (unsigned)hr);
        if (err && err->GetBufferPointer()) {
            NK_DX12_ERR("%s\n", (const char*)err->GetBufferPointer());
        }
        return {};
    }

    ComPtr<ID3D12RootSignature> rs;
    hr = mDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                      IID_PPV_ARGS(&rs));
    if (FAILED(hr)) {
        NK_DX12_ERR("CreateRootSignature failed (hr=0x%X)\n", (unsigned)hr);
        return {};
    }
    return rs;
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDirectX12Device::CreateBuffer(const NkBufferDesc& desc) {
    bool uploadAfterCreate = false;
    const void* uploadData = desc.initialData;
    NkBufferHandle h;

    {
    threading::NkScopedLock lock(mMutex);

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
    rd.Alignment          = 0;
    rd.Width              = alignedSize;
    rd.Height             = 1;
    rd.DepthOrArraySize   = 1;
    rd.MipLevels          = 1;
    rd.Format             = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count   = 1;
    rd.SampleDesc.Quality = 0;
    rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags              = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type                 = heapType;
    hp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    hp.CreationNodeMask     = 1;
    hp.VisibleNodeMask      = 1;

    ComPtr<ID3D12Resource> res;
    HRESULT hr = mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
        &rd, initState, nullptr, IID_PPV_ARGS(&res));
    if (FAILED(hr) || !res) {
        NK_DX12_ERR("CreateCommittedResource (buffer) failed (hr=0x%X)\n", (unsigned)hr);
        return {};
    }

    NkDX12Buffer b;
    b.resource = res;
    b.gpuAddr  = res->GetGPUVirtualAddress();
    b.desc     = desc;
    b.state    = initState;

    if (persistentMap) {
        D3D12_RANGE r{};
        hr = res->Map(0, &r, &b.mapped);
        if (FAILED(hr)) {
            NK_DX12_ERR("Map(buffer) failed (hr=0x%X)\n", (unsigned)hr);
            return {};
        }
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
    if (uploadData) {
        if (b.mapped) {
            memcpy(b.mapped, uploadData, (size_t)desc.sizeBytes);
        } else {
            // Important: ne pas créer un staging buffer sous ce lock (deadlock récursif).
            uploadAfterCreate = true;
        }
    }

    uint64 hid = NextId();
    mBuffers[hid] = b;
    h.id = hid;
    }

    if (uploadAfterCreate) {
        WriteBuffer(h, uploadData, desc.sizeBytes, 0);
    }
    return h;
}

void NkDirectX12Device::DestroyBuffer(NkBufferHandle& h) {
    threading::NkScopedLock lock(mMutex);
    auto it = mBuffers.Find(h.id); if(!it) return;
    if (it->mapped) it->resource->Unmap(0, nullptr);
    mBuffers.Erase(h.id); h.id = 0;
}

bool NkDirectX12Device::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.Find(buf.id); if(!it) return false;
    if (it->mapped) {
        memcpy((uint8*)it->mapped + off, data, (size_t)sz);
        return true;
    }
    // Via upload
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    memcpy(stage.mapped, data, (size_t)sz);
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        auto prevState = it->state;
        TransitionResource(cmd, it->resource.Get(), prevState, D3D12_RESOURCE_STATE_COPY_DEST);
        cmd->CopyBufferRegion(it->resource.Get(), off, stage.resource.Get(), 0, sz);
        TransitionResource(cmd, it->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, prevState);
    });
    DestroyBuffer(stageH);
    return true;
}

bool NkDirectX12Device::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.Find(buf.id); if(!it) return false;
    if (it->mapped) { memcpy((uint8*)it->mapped + off, data, (size_t)sz); return true; }
    return WriteBuffer(buf, data, sz, off);
}

bool NkDirectX12Device::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
    auto it = mBuffers.Find(buf.id); if(!it) return false;
    if (it->mapped) { memcpy(out, (uint8*)it->mapped + off, (size_t)sz); return true; }
    NkBufferDesc sd = NkBufferDesc::Staging(sz); sd.usage = NkResourceUsage::NK_READBACK;
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, it->resource.Get(), it->state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        cmd->CopyBufferRegion(stage.resource.Get(), 0, it->resource.Get(), off, sz);
        TransitionResource(cmd, it->resource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, it->state);
    });
    memcpy(out, stage.mapped, (size_t)sz);
    DestroyBuffer(stageH);
    return true;
}

NkMappedMemory NkDirectX12Device::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    auto it = mBuffers.Find(buf.id); if(!it) return {};
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

void NkDirectX12Device::UnmapBuffer(NkBufferHandle buf) {
    auto it = mBuffers.Find(buf.id); if(!it) return;
    if (!it->mapped) it->resource->Unmap(0, nullptr);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDirectX12Device::CreateTexture(const NkTextureDesc& desc) {
    threading::NkScopedLock lock(mMutex);

    DXGI_FORMAT fmt = ToDXGIFormat(desc.format);
    bool isDepth = NkFormatIsDepth(desc.format);
    const bool depthSrvRequested = isDepth && NkHasFlag(desc.bindFlags, NkBindFlags::NK_SHADER_RESOURCE);
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthSrvFormat = DXGI_FORMAT_UNKNOWN;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width              = desc.width;
    rd.Height             = desc.height;
    rd.DepthOrArraySize   = (UINT16)math::NkMax(desc.arrayLayers, 1u);
    rd.MipLevels          = (UINT16)(desc.mipLevels == 0 ?
        (uint32)(floor(log2(math::NkMax(desc.width, desc.height))) + 1) : desc.mipLevels);
    rd.SampleDesc         = { (UINT)desc.samples, 0 };
    rd.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (isDepth) {
        if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
            rd.Format = depthSrvRequested ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
            dsvFormat = DXGI_FORMAT_D32_FLOAT;
            depthSrvFormat = DXGI_FORMAT_R32_FLOAT;
        } else {
            rd.Format = depthSrvRequested ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            depthSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }
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
        clearVal.Format = dsvFormat;
        clearVal.DepthStencil = { 1.f, 0 };
        pClear = &clearVal;
    }

    D3D12_RESOURCE_STATES initState = isDepth
        ? D3D12_RESOURCE_STATE_DEPTH_WRITE
        : (NkHasFlag(desc.bindFlags, NkBindFlags::NK_RENDER_TARGET)
           ? D3D12_RESOURCE_STATE_RENDER_TARGET
           : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
    hp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    hp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    hp.CreationNodeMask     = 1;
    hp.VisibleNodeMask      = 1;
    ComPtr<ID3D12Resource> res;
    HRESULT hr = mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
        &rd, initState, pClear, IID_PPV_ARGS(&res));
    if (FAILED(hr) || !res) {
        NK_DX12_ERR("CreateCommittedResource (texture) failed (hr=0x%X)\n", (unsigned)hr);
        return {};
    }

    NkDX12Texture t;
    t.resource = res;
    t.desc     = desc;
    t.format   = rd.Format;
    t.state    = initState;

    // Créer les vues
    if (isDepth) {
        t.dsvIdx = mDsvHeap.allocated;
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format        = dsvFormat;
        dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        mDevice->CreateDepthStencilView(res.Get(), &dsvd, mDsvHeap.AllocCPU());

        if (depthSrvRequested) {
            t.srvIdx = mCbvSrvUavHeap.allocated;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                  = depthSrvFormat;
            srvd.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvd.Texture2D.MipLevels     = rd.MipLevels;
            mDevice->CreateShaderResourceView(res.Get(), &srvd, mCbvSrvUavHeap.AllocCPU());
        }
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

void NkDirectX12Device::DestroyTexture(NkTextureHandle& h) {
    threading::NkScopedLock lock(mMutex);
    auto it = mTextures.Find(h.id); if(!it) return;
    mTextures.Erase(h.id); h.id = 0;
}

bool NkDirectX12Device::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    auto it = mTextures.Find(t.id); if(!it) return false;
    auto& desc = it->desc;
    return WriteTextureRegion(t, p, 0, 0, 0, desc.width, desc.height, 1, 0, 0, rp);
}

bool NkDirectX12Device::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x, uint32 y, uint32 /*z*/, uint32 w, uint32 h, uint32 /*d2*/,
    uint32 mip, uint32 /*layer*/, uint32 rowPitch) {
    auto it = mTextures.Find(t.id); if(!it) return false;
    uint32 bpp = NkFormatBytesPerPixel(it->desc.format);
    uint32 rp  = rowPitch > 0 ? rowPitch : w * bpp;
    uint64 sz  = (uint64)rp * h;
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    memcpy(stage.mapped, pixels, (size_t)sz);

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

bool NkDirectX12Device::GenerateMipmaps(NkTextureHandle, NkFilter) {
    // DX12 n'a pas de GenerateMips natif — nécessite un compute shader séparé
    // Pour l'instant on retourne true (les mips sont générés à la création si mipLevels>1)
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDirectX12Device::CreateSampler(const NkSamplerDesc& d) {
    threading::NkScopedLock lock(mMutex);
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

void NkDirectX12Device::DestroySampler(NkSamplerHandle& h) {
    threading::NkScopedLock lock(mMutex);
    mSamplers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Shaders (DXBC/DXIL via D3DCompile ou pré-compilés)
// =============================================================================
NkShaderHandle NkDirectX12Device::CreateShader(const NkShaderDesc& desc) {
    threading::NkScopedLock lock(mMutex);
    NkDX12Shader sh;
    bool compiledAtLeastOneStage = false;

    for (uint32 i = 0; i < desc.stages.Size(); i++) {
        auto& s = desc.stages[i];

        // Priorité : bytecode précompilé > HLSL source
        NkVector<uint8>* target = nullptr;
        const char* hlslTarget = nullptr;
        bool stageCompiled = false;

        switch (s.stage) {
            case NkShaderStage::NK_VERTEX:   target = &sh.vs.bytecode; hlslTarget = "vs_5_1"; break;
            case NkShaderStage::NK_FRAGMENT: target = &sh.ps.bytecode; hlslTarget = "ps_5_1"; break;
            case NkShaderStage::NK_COMPUTE:  target = &sh.cs.bytecode; hlslTarget = "cs_5_1"; break;
            case NkShaderStage::NK_GEOMETRY: target = &sh.gs.bytecode; hlslTarget = "gs_5_1"; break;
            default: continue;
        }

        if (s.spirvBinary.Data() && s.spirvBinary.Size() > 0) {
            // DXIL pré-compilé
            target->Assign(static_cast<const uint8*>(s.spirvBinary.Data()), static_cast<usize>(s.spirvBinary.Size()));
            stageCompiled = true;
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
                NK_DX12_ERR("Shader compile failed for stage %u (hr=0x%X)\n",
                            (unsigned)s.stage, (unsigned)hr);
                return {};
            }
            target->Assign(static_cast<const uint8*>(code->GetBufferPointer()),
                           static_cast<usize>(code->GetBufferSize()));
            if (s.stage == NkShaderStage::NK_VERTEX) sh.vsBlob = code;
            stageCompiled = true;
        }

        if (!stageCompiled || target->empty()) {
            NK_DX12_ERR("Shader stage missing/empty for stage %u\n", (unsigned)s.stage);
            return {};
        }
        compiledAtLeastOneStage = true;
    }

    if (!compiledAtLeastOneStage) {
        NK_DX12_ERR("Shader desc has no compilable stage\n");
        return {};
    }

    uint64 hid = NextId(); mShaders[hid] = std::move(sh);
    NkShaderHandle h; h.id = hid; return h;
}

void NkDirectX12Device::DestroyShader(NkShaderHandle& h) {
    threading::NkScopedLock lock(mMutex);
    mShaders.Erase(h.id); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDirectX12Device::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    threading::NkScopedLock lock(mMutex);
    auto sit = mShaders.Find(d.shader.id); if(!sit) return {};
    auto& sh = *sit;

    auto rootSig = CreateDefaultRootSignature(false);
    if (!rootSig) {
        NK_DX12_ERR("CreateGraphicsPipeline: root signature invalid\n");
        return {};
    }

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
    if (sh.vs.bytecode.empty()) {
        NK_DX12_ERR("CreateGraphicsPipeline: missing vertex shader bytecode\n");
        return {};
    }
    psd.VS = sh.vs.bc();
    if (!sh.gs.bytecode.empty()) psd.GS = sh.gs.bc();
    if (!sh.hs.bytecode.empty()) psd.HS = sh.hs.bc();
    if (!sh.ds.bytecode.empty()) psd.DS = sh.ds.bc();

    psd.InputLayout = { elems.Data(), (UINT)elems.size() };
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
    // SampleDesc.Count doit être >= 1 (NkSampleCount zero-init = 0, pas NK_S1=1)
    psd.SampleDesc = { (UINT)d.samples >= 1 ? (UINT)d.samples : 1u, 0 };

    // Render target formats
    auto rpit = mRenderPasses.Find(d.renderPass.id);
    if(rpit) {
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

    if (psd.NumRenderTargets > 0) {
        if (sh.ps.bytecode.empty()) {
            NK_DX12_ERR("CreateGraphicsPipeline: missing pixel shader with color outputs\n");
            return {};
        }
        psd.PS = sh.ps.bc();
    }

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = mDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&pso));
    if (FAILED(hr) || !pso) {
        NK_DX12_ERR("CreateGraphicsPipelineState failed (hr=0x%X)\n", (unsigned)hr);
        return {};
    }

    NkDX12Pipeline p; p.pso = pso; p.rootSig = rootSig;
    p.isCompute = false;
    p.topology  = ToDX12Topology(d.topology);
    for (uint32 i = 0; i < d.vertexLayout.bindings.Size(); ++i) {
        const NkVertexBinding& b = d.vertexLayout.bindings[i];
        if (b.binding < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            p.vertexStrides[b.binding] = b.stride;
    }

    uint64 hid = NextId(); mPipelines[hid] = std::move(p);
    NkPipelineHandle h; h.id = hid; return h;
}

NkPipelineHandle NkDirectX12Device::CreateComputePipeline(const NkComputePipelineDesc& d) {
    threading::NkScopedLock lock(mMutex);
    auto sit = mShaders.Find(d.shader.id); if(!sit) return {};
    auto& sh = *sit;
    if (sh.cs.bytecode.empty()) {
        NK_DX12_ERR("CreateComputePipeline: missing compute shader bytecode\n");
        return {};
    }

    auto rootSig = CreateDefaultRootSignature(true);
    if (!rootSig) {
        NK_DX12_ERR("CreateComputePipeline: root signature invalid\n");
        return {};
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC cpd{};
    cpd.pRootSignature = rootSig.Get();
    cpd.CS             = sh.cs.bc();

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = mDevice->CreateComputePipelineState(&cpd, IID_PPV_ARGS(&pso));
    if (FAILED(hr) || !pso) {
        NK_DX12_ERR("CreateComputePipelineState failed (hr=0x%X)\n", (unsigned)hr);
        return {};
    }

    NkDX12Pipeline p; p.pso = pso; p.rootSig = rootSig; p.isCompute = true;
    uint64 hid = NextId(); mPipelines[hid] = std::move(p);
    NkPipelineHandle h; h.id = hid; return h;
}

void NkDirectX12Device::DestroyPipeline(NkPipelineHandle& h) {
    threading::NkScopedLock lock(mMutex);
    mPipelines.Erase(h.id); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers (metadata only en DX12)
// =============================================================================
NkRenderPassHandle NkDirectX12Device::CreateRenderPass(const NkRenderPassDesc& d) {
    threading::NkScopedLock lock(mMutex);
    uint64 hid = NextId(); mRenderPasses[hid] = { d };
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkDirectX12Device::DestroyRenderPass(NkRenderPassHandle& h) {
    threading::NkScopedLock lock(mMutex); mRenderPasses.Erase(h.id); h.id = 0;
}

NkFramebufferHandle NkDirectX12Device::CreateFramebuffer(const NkFramebufferDesc& d) {
    threading::NkScopedLock lock(mMutex);
    NkDX12Framebuffer fb; fb.w = d.width; fb.h = d.height;
    for (uint32 i = 0; i < d.colorAttachments.Size(); i++) {
        auto it = mTextures.Find(d.colorAttachments[i].id);
        if(it) {
            fb.rtvIdxs[fb.rtvCount] = it->rtvIdx;
            fb.colorTexIds[fb.rtvCount] = d.colorAttachments[i].id;
            fb.rtvCount++;
        }
    }
    if (d.depthAttachment.IsValid()) {
        auto it = mTextures.Find(d.depthAttachment.id);
        if(it) {
            fb.dsvIdx = it->dsvIdx;
            fb.depthTexId = d.depthAttachment.id;
        }
    }
    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
void NkDirectX12Device::DestroyFramebuffer(NkFramebufferHandle& h) {
    threading::NkScopedLock lock(mMutex); mFramebuffers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDirectX12Device::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    threading::NkScopedLock lock(mMutex);
    uint64 hid = NextId(); mDescLayouts[hid] = { d };
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDirectX12Device::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    threading::NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id = 0;
}
NkDescSetHandle NkDirectX12Device::AllocateDescriptorSet(NkDescSetHandle layout) {
    threading::NkScopedLock lock(mMutex);
    NkDX12DescSet ds; ds.layoutId = layout.id;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDirectX12Device::FreeDescriptorSet(NkDescSetHandle& h) {
    threading::NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id = 0;
}

void NkDirectX12Device::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    threading::NkScopedLock lock(mMutex);
    for (uint32 i = 0; i < n; i++) {
        auto& w = writes[i];
        auto sit = mDescSets.Find(w.set.id); if(!sit) continue;

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
NkICommandBuffer* NkDirectX12Device::CreateCommandBuffer(NkCommandBufferType t) {
    NkDirectX12CommandBuffer* cb = new NkDirectX12CommandBuffer(this, t);
    if (!cb->IsValid()) {
        delete cb;
        return nullptr;
    }
    return cb;
}
void NkDirectX12Device::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb = nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDirectX12Device::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    NkVector<ID3D12CommandList*> lists;
    for (uint32 i = 0; i < n; i++) {
        auto* dx = dynamic_cast<NkDirectX12CommandBuffer*>(cbs[i]);
        if (dx && dx->GetCmdList()) lists.PushBack(dx->GetCmdList());
    }
    if (!lists.empty())
        mGraphicsQueue->ExecuteCommandLists((UINT)lists.size(), lists.Data());
    if (fence.IsValid()) {
        auto it = mFences.Find(fence.id);
        if(it) {
            it->value++;
            mGraphicsQueue->Signal(it->fence.Get(), it->value);
        }
    }
}

void NkDirectX12Device::SubmitAndPresent(NkICommandBuffer* cb) {
    auto& fd = mFrameData[mFrameIndex];

    NkICommandBuffer* cbs[] = { cb };
    Submit(cbs, 1, {});

    fd.Signal(mGraphicsQueue.Get(), ++mFenceValue);
    const UINT syncInterval = mVsync ? 1u : 0u;
    const UINT flags = (!mVsync && mAllowTearing && mTearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
    mSwapchain->Present(syncInterval, flags);
    mBackBufferIdx = mSwapchain->GetCurrentBackBufferIndex();

    // Keep command-buffer allocator reuse safe in the current architecture.
    if (fd.fence->GetCompletedValue() < fd.fenceValue) {
        fd.fence->SetEventOnCompletion(fd.fenceValue, fd.fenceEvent);
        WaitForSingleObject(fd.fenceEvent, INFINITE);
    }
}

// =============================================================================
// Fence
// =============================================================================
NkFenceHandle NkDirectX12Device::CreateFence(bool signaled) {
    DX12Fence f;
    mDevice->CreateFence(signaled ? 1 : 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&f.fence));
    f.value = signaled ? 1 : 0;
    f.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    uint64 hid = NextId(); mFences[hid] = std::move(f);
    NkFenceHandle h; h.id = hid; return h;
}
void NkDirectX12Device::DestroyFence(NkFenceHandle& h) {
    auto it = mFences.Find(h.id); if(!it) return;
    CloseHandle(it->event);
    mFences.Erase(h.id); h.id = 0;
}
bool NkDirectX12Device::WaitFence(NkFenceHandle f, uint64 timeoutNs) {
    auto it = mFences.Find(f.id); if(!it) return false;
    if (it->fence->GetCompletedValue() < it->value) {
        it->fence->SetEventOnCompletion(it->value, it->event);
        DWORD ms = timeoutNs == UINT64_MAX ? INFINITE : (DWORD)(timeoutNs / 1000000);
        WaitForSingleObject(it->event, ms);
    }
    return it->fence->GetCompletedValue() >= it->value;
}
bool NkDirectX12Device::IsFenceSignaled(NkFenceHandle f) {
    auto it = mFences.Find(f.id); if(!it) return false;
    return it->fence->GetCompletedValue() >= it->value;
}
void NkDirectX12Device::ResetFence(NkFenceHandle f) {
    auto it = mFences.Find(f.id); if(!it) return;
    it->value = 0;
    mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&it->fence));
}
void NkDirectX12Device::WaitIdle() {
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
bool NkDirectX12Device::BeginFrame(NkFrameContext& frame) {
    if (!mSwapchain) return false;
    mFrameData[mFrameIndex].WaitAndReset(mGraphicsQueue.Get());
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
    mBackBufferIdx    = mSwapchain->GetCurrentBackBufferIndex();
    return true;
}
void NkDirectX12Device::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES;
    ++mFrameNumber;
}
void NkDirectX12Device::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return;
    mWidth = w; mHeight = h;
    ResizeSwapchain(w, h);
}

// =============================================================================
// Caps
// =============================================================================
void NkDirectX12Device::QueryCaps() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS opts{};
    mDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts));

    mCaps.tessellationShaders = true;
    mCaps.geometryShaders     = true;
    mCaps.computeShaders      = NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_GFX_API_D3D12);
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
ID3D12Resource* NkDirectX12Device::GetDX12Buffer(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->resource.Get() : nullptr;
}
ID3D12Resource* NkDirectX12Device::GetDX12Texture(uint64 id) const {
    auto it = mTextures.Find(id); return it ? it->resource.Get() : nullptr;
}
D3D12_GPU_VIRTUAL_ADDRESS NkDirectX12Device::GetBufferGPUAddr(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->gpuAddr : 0;
}
UINT NkDirectX12Device::GetBufferCbvIndex(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->cbvIdx : UINT_MAX;
}
UINT NkDirectX12Device::GetBufferSrvIndex(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->srvIdx : UINT_MAX;
}
UINT NkDirectX12Device::GetBufferUavIndex(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->uavIdx : UINT_MAX;
}
UINT NkDirectX12Device::GetTextureSrvIndex(uint64 id) const {
    auto it = mTextures.Find(id); return it ? it->srvIdx : UINT_MAX;
}
UINT NkDirectX12Device::GetTextureUavIndex(uint64 id) const {
    auto it = mTextures.Find(id); return it ? it->uavIdx : UINT_MAX;
}
UINT NkDirectX12Device::GetSamplerHeapIndex(uint64 id) const {
    auto it = mSamplers.Find(id); return it ? it->heapIdx : UINT_MAX;
}
bool NkDirectX12Device::IsSwapchainTexture(uint64 id) const {
    auto it = mTextures.Find(id); return it ? it->isSwapchain : false;
}
void NkDirectX12Device::TransitionTextureState(ID3D12GraphicsCommandList* cmd,
                                               uint64 textureId,
                                               D3D12_RESOURCE_STATES to) {
    auto it = mTextures.Find(textureId);
    if (!it || !cmd) return;
    TransitionResource(cmd, it->resource.Get(), it->state, to);
    it->state = to;
}
const NkDX12Pipeline* NkDirectX12Device::GetPipeline(uint64 id) const {
    auto it = mPipelines.Find(id); return it;
}
const NkDX12DescSet* NkDirectX12Device::GetDescSet(uint64 id) const {
    auto it = mDescSets.Find(id); return it;
}
const NkDX12Framebuffer* NkDirectX12Device::GetFBO(uint64 id) const {
    auto it = mFramebuffers.Find(id); return it;
}
D3D12_CPU_DESCRIPTOR_HANDLE NkDirectX12Device::GetRTV(UINT idx) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = mRtvHeap.cpuBase;
    h.ptr += (SIZE_T)idx * mRtvHeap.incrementSize;
    return h;
}
D3D12_CPU_DESCRIPTOR_HANDLE NkDirectX12Device::GetDSV(UINT idx) const {
    D3D12_CPU_DESCRIPTOR_HANDLE h = mDsvHeap.cpuBase;
    h.ptr += (SIZE_T)idx * mDsvHeap.incrementSize;
    return h;
}

// =============================================================================
// Conversions
// =============================================================================
DXGI_FORMAT NkDirectX12Device::ToDXGIFormat(NkGPUFormat f) {
    switch (f) {
        case NkGPUFormat::NK_R8_UNORM:         return DXGI_FORMAT_R8_UNORM;
        case NkGPUFormat::NK_RG8_UNORM:        return DXGI_FORMAT_R8G8_UNORM;
        case NkGPUFormat::NK_RGBA8_UNORM:      return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkGPUFormat::NK_RGBA8_SRGB:       return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case NkGPUFormat::NK_BGRA8_UNORM:      return DXGI_FORMAT_B8G8R8A8_UNORM;
        case NkGPUFormat::NK_BGRA8_SRGB:       return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case NkGPUFormat::NK_R16_FLOAT:        return DXGI_FORMAT_R16_FLOAT;
        case NkGPUFormat::NK_RG16_FLOAT:       return DXGI_FORMAT_R16G16_FLOAT;
        case NkGPUFormat::NK_RGBA16_FLOAT:     return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkGPUFormat::NK_R32_FLOAT:        return DXGI_FORMAT_R32_FLOAT;
        case NkGPUFormat::NK_RG32_FLOAT:       return DXGI_FORMAT_R32G32_FLOAT;
        case NkGPUFormat::NK_RGB32_FLOAT:      return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkGPUFormat::NK_RGBA32_FLOAT:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkGPUFormat::NK_R32_UINT:         return DXGI_FORMAT_R32_UINT;
        case NkGPUFormat::NK_RG32_UINT:        return DXGI_FORMAT_R32G32_UINT;
        case NkGPUFormat::NK_D16_UNORM:        return DXGI_FORMAT_D16_UNORM;
        case NkGPUFormat::NK_D32_FLOAT:        return DXGI_FORMAT_D32_FLOAT;
        case NkGPUFormat::NK_D24_UNORM_S8_UINT:return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case NkGPUFormat::NK_BC1_RGB_UNORM:    return DXGI_FORMAT_BC1_UNORM;
        case NkGPUFormat::NK_BC1_RGB_SRGB:     return DXGI_FORMAT_BC1_UNORM_SRGB;
        case NkGPUFormat::NK_BC3_UNORM:        return DXGI_FORMAT_BC3_UNORM;
        case NkGPUFormat::NK_BC5_UNORM:        return DXGI_FORMAT_BC5_UNORM;
        case NkGPUFormat::NK_BC7_UNORM:        return DXGI_FORMAT_BC7_UNORM;
        case NkGPUFormat::NK_BC7_SRGB:         return DXGI_FORMAT_BC7_UNORM_SRGB;
        case NkGPUFormat::NK_R11G11B10_FLOAT:  return DXGI_FORMAT_R11G11B10_FLOAT;
        case NkGPUFormat::NK_A2B10G10R10_UNORM:return DXGI_FORMAT_R10G10B10A2_UNORM;
        default:                         return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}
DXGI_FORMAT NkDirectX12Device::ToDXGIVertexFormat(NkVertexFormat f) {
    switch (f) {
        case NkVertexFormat::NK_R8_UNORM:               return DXGI_FORMAT_R8_UNORM;
        case NkVertexFormat::NK_RG8_UNORM:              return DXGI_FORMAT_R8G8_UNORM;
        case NkVertexFormat::NK_RGBA8_UNORM:
        case NkVertexFormat::NK_R8G8B8A8_UNORM_PACKED:  return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkVertexFormat::NK_RGBA8_SNORM:            return DXGI_FORMAT_R8G8B8A8_SNORM;
        case NkVertexFormat::NK_R16_FLOAT:              return DXGI_FORMAT_R16_FLOAT;
        case NkVertexFormat::NK_RG16_FLOAT:             return DXGI_FORMAT_R16G16_FLOAT;
        case NkVertexFormat::NK_RGBA16_FLOAT:           return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkVertexFormat::NK_R16_UINT:               return DXGI_FORMAT_R16_UINT;
        case NkVertexFormat::NK_RG16_UINT:              return DXGI_FORMAT_R16G16_UINT;
        case NkVertexFormat::NK_RGBA16_UINT:            return DXGI_FORMAT_R16G16B16A16_UINT;
        case NkVertexFormat::NK_R32_FLOAT:              return DXGI_FORMAT_R32_FLOAT;
        case NkVertexFormat::NK_RG32_FLOAT:             return DXGI_FORMAT_R32G32_FLOAT;
        case NkVertexFormat::NK_RGB32_FLOAT:            return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkVertexFormat::NK_RGBA32_FLOAT:           return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkVertexFormat::NK_R32_UINT:               return DXGI_FORMAT_R32_UINT;
        case NkVertexFormat::NK_RG32_UINT:              return DXGI_FORMAT_R32G32_UINT;
        case NkVertexFormat::NK_RGBA32_UINT:            return DXGI_FORMAT_R32G32B32A32_UINT;
        case NkVertexFormat::NK_R32_SINT:               return DXGI_FORMAT_R32_SINT;
        case NkVertexFormat::NK_RG32_SINT:              return DXGI_FORMAT_R32G32_SINT;
        case NkVertexFormat::NK_RGBA32_SINT:            return DXGI_FORMAT_R32G32B32A32_SINT;
        case NkVertexFormat::NK_A2B10G10R10_UNORM:      return DXGI_FORMAT_R10G10B10A2_UNORM;
        default:                                        return DXGI_FORMAT_R32G32B32_FLOAT;
    }
}
D3D12_COMPARISON_FUNC NkDirectX12Device::ToDX12Compare(NkCompareOp op) {
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
D3D12_BLEND NkDirectX12Device::ToDX12Blend(NkBlendFactor f) {
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
D3D12_BLEND_OP NkDirectX12Device::ToDX12BlendOp(NkBlendOp op) {
    switch (op) {
        case NkBlendOp::NK_ADD:    return D3D12_BLEND_OP_ADD;
        case NkBlendOp::NK_SUB:    return D3D12_BLEND_OP_SUBTRACT;
        case NkBlendOp::NK_REV_SUB: return D3D12_BLEND_OP_REV_SUBTRACT;
        case NkBlendOp::NK_MIN:    return D3D12_BLEND_OP_MIN;
        case NkBlendOp::NK_MAX:    return D3D12_BLEND_OP_MAX;
        default:                return D3D12_BLEND_OP_ADD;
    }
}
D3D12_STENCIL_OP NkDirectX12Device::ToDX12StencilOp(NkStencilOp op) {
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
D3D12_PRIMITIVE_TOPOLOGY NkDirectX12Device::ToDX12Topology(NkPrimitiveTopology t) {
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
D3D12_PRIMITIVE_TOPOLOGY_TYPE NkDirectX12Device::ToDX12TopologyType(NkPrimitiveTopology t) {
    switch (t) {
        case NkPrimitiveTopology::NK_POINT_LIST:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case NkPrimitiveTopology::NK_LINE_LIST:
        case NkPrimitiveTopology::NK_LINE_STRIP:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case NkPrimitiveTopology::NK_PATCH_LIST:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        default:                                 return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}
D3D12_FILTER NkDirectX12Device::ToDX12Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp) {
    if (cmp) return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    bool linMag = (mag == NkFilter::NK_LINEAR);
    bool linMin = (min == NkFilter::NK_LINEAR);
    bool linMip = (mip == NkMipFilter::NK_LINEAR);
    if (linMag && linMin && linMip) return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    if (!linMag && !linMin && !linMip) return D3D12_FILTER_MIN_MAG_MIP_POINT;
    return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}
D3D12_TEXTURE_ADDRESS_MODE NkDirectX12Device::ToDX12Address(NkAddressMode a) {
    switch (a) {
        case NkAddressMode::NK_REPEAT:         return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case NkAddressMode::NK_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case NkAddressMode::NK_CLAMP_TO_EDGE:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case NkAddressMode::NK_CLAMP_TO_BORDER:  return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:                            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}
D3D12_RESOURCE_STATES NkDirectX12Device::ToDX12State(NkResourceState s) {
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

} // namespace nkentseu
#endif // NK_RHI_DX12_ENABLED
