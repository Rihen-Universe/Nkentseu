// =============================================================================
// NkRHI_Device_DX12.cpp — Backend DirectX 12
// =============================================================================
#ifdef NK_RHI_DX12_ENABLED
#include "NkDirectX12Device.h"
#include "NkDirectX12CommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <d3d12sdklayers.h>
// DXC (vrai compilateur DX12 : HLSL SM6 -> DXIL). Disponible si le SDK Vulkan/
// Windows fournit dxc/dxcapi.h. Chargé dynamiquement (pas de lien dxcompiler.lib).
#if defined(__has_include)
#  if __has_include(<dxc/dxcapi.h>)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wignored-attributes"
#    pragma clang diagnostic ignored "-Wunknown-attributes"
#    include <dxc/dxcapi.h>
#    pragma clang diagnostic pop
#    define NK_HAS_DXC 1
#  endif
#endif

#define NK_DX12_LOG(...)  logger_src.Infof("[NkRHI_DX12] " __VA_ARGS__)
#define NK_DX12_ERR(...)  logger_src.Infof("[NkRHI_DX12][ERR] " __VA_ARGS__)
#define NK_DX12_CHECK(hr, msg) do { if(FAILED(hr)){NK_DX12_ERR(msg " (hr=0x%X)\n",(unsigned)(hr));} } while(0)

namespace nkentseu {

namespace {
    // ── DX12 InfoQueue routage NkLog ─────────────────────────────────────────
    static void LogDX12Message(D3D12_MESSAGE_CATEGORY cat,
                                D3D12_MESSAGE_SEVERITY sev,
                                D3D12_MESSAGE_ID id,
                                const char* msg) {
        if (!msg) return;
        switch (sev) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            case D3D12_MESSAGE_SEVERITY_ERROR:
                logger.Errorf("[NkRHI_DX12][%d/%d] %s", (int)cat, (int)id, msg);
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                logger.Warnf("[NkRHI_DX12][%d/%d] %s", (int)cat, (int)id, msg);
                break;
            case D3D12_MESSAGE_SEVERITY_INFO:
                logger.Debugf("[NkRHI_DX12][%d/%d] %s", (int)cat, (int)id, msg);
                break;
            default:
                logger.Tracef("[NkRHI_DX12][%d/%d] %s", (int)cat, (int)id, msg);
                break;
        }
    }

    static void __stdcall DX12MessageCallback(D3D12_MESSAGE_CATEGORY cat,
                                                D3D12_MESSAGE_SEVERITY sev,
                                                D3D12_MESSAGE_ID id,
                                                LPCSTR pDescription,
                                                void* /*pContext*/) {
        LogDX12Message(cat, sev, id, pDescription);
    }

    // Fallback polling pour ID3D12InfoQueue (sans InfoQueue1). Appele dans EndFrame.
    static void DrainDX12InfoQueue(ID3D12InfoQueue* q) {
        if (!q) return;
        const UINT64 count = q->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 i = 0; i < count; ++i) {
            SIZE_T size = 0;
            q->GetMessage(i, nullptr, &size);
            if (size == 0) continue;
            NkVector<uint8> buf;
            buf.Resize(static_cast<uint32>(size));
            D3D12_MESSAGE* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.Data());
            if (FAILED(q->GetMessage(i, msg, &size))) continue;
            LogDX12Message(msg->Category, msg->Severity, msg->ID, msg->pDescription);
        }
        q->ClearStoredMessages();
    }
} // namespace

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
#ifdef _DEBUG
    // Build Debug : debug layer DX12 actif par defaut (no-op si la feature Windows
    // « Graphics Tools » est absente — D3D12GetDebugInterface echoue gracieusement).
    const bool wantDebug = true;
#else
    const bool wantDebug = dxCfg.debugDevice || getenv("NK_DX12_DEBUG") != nullptr;
#endif
    if (wantDebug) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            // GPU-Based Validation : config OU override par env NK_DX12_GBV (diag hazards de
            // barrier en LIVE — détecte les lectures de ressource dans un mauvais état que la
            // sérialisation de RenderDoc masque). Désactivé par défaut (coût GPU important).
            const bool wantGBV = dxCfg.gpuValidation || (getenv("NK_DX12_GBV") != nullptr);
            if (wantGBV) {
                ComPtr<ID3D12Debug1> debug1;
                if (SUCCEEDED(debug.As(&debug1))) {
                    debug1->SetEnableGPUBasedValidation(TRUE);
                    NK_DX12_LOG("GPU-Based Validation ACTIVE (NK_DX12_GBV)\n");
                }
            }
        }
    }
    // DRED (Device Removed Extended Data) : capture les auto-breadcrumbs GPU + page faults.
    // Sur un device removed (hang), on saura QUELLE opération GPU a hangé (diag resize DX12).
    {
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dred;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dred)))) {
            dred->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dred->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            NK_DX12_LOG("DRED activé (auto-breadcrumbs + page-fault)\n");
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

    // InfoQueue : route validation messages vers NkLog (debug device seulement).
    // Tente d'abord InfoQueue1 (Win10 2004+ / Agility SDK) pour callback live ;
    // sinon fallback sur InfoQueue + polling cote EndFrame.
    if (wantDebug) {
        if (SUCCEEDED(mDevice.As(&mInfoQueue1)) && mInfoQueue1) {
            if (SUCCEEDED(mInfoQueue1->RegisterMessageCallback(
                    DX12MessageCallback,
                    D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                    nullptr,
                    &mInfoQueueCookie))) {
                NK_DX12_LOG("InfoQueue1 callback enregistre (validation -> NkLog)\n");
            } else {
                mInfoQueue1.Reset();
            }
        }
        if (!mInfoQueue1 && SUCCEEDED(mDevice.As(&mInfoQueue)) && mInfoQueue) {
            NK_DX12_LOG("InfoQueue polling actif (fallback, validation -> NkLog)\n");
        }
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
    // STAGING (CPU, non shader-visible) : descripteurs persistants, source des copies.
    makeHeap(mCbvSrvUavHeap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mSrvHeapCapacity,     false);
    makeHeap(mSamplerHeap,   D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     mSamplerHeapCapacity, false);
    // RINGS shader-visible : destination par draw (ring linéaire, reset/frame). Le heap
    // sampler shader-visible est plafonné à 2048 par D3D12.
    // Capacité CBV/SRV/UAV ring portée 32768→65536 (2026-06-23) : le bloc par-draw passe de
    // 56 à 72 descripteurs (NUM_CBV 16→32). 65536/72 ≈ 910 draws/frame de marge (avant :
    // 32768/72 ≈ 455). Bien sous la limite matérielle (1M descripteurs CBV/SRV/UAV Tier 1).
    makeHeap(mCbvSrvUavRing, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536, true);
    makeHeap(mSamplerRing,   D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     2048,  true);
    mCbvSrvUavRingHead = 0;
    mSamplerRingHead   = 0;
    InitNullDescriptors(); // descripteurs défaut pour remplir les slots non bindés
}

// =============================================================================
// Ring de descripteurs shader-visible (alloc contiguë par draw)
// =============================================================================
void NkDirectX12Device::ResetDescriptorRingForFrame(uint32 /*frameIdx*/) {
    // Présent sérialisé (cf. SubmitAndPresent) → GPU idle entre frames → reset à 0 sûr.
    mCbvSrvUavRingHead = 0;
    mSamplerRingHead   = 0;
}
UINT NkDirectX12Device::AllocCbvSrvUavRing(UINT count) {
    if (mCbvSrvUavRing.capacity == 0) return UINT_MAX;
    if (mCbvSrvUavRingHead + count > mCbvSrvUavRing.capacity) return UINT_MAX; // débordement → skip
    UINT base = mCbvSrvUavRingHead;
    mCbvSrvUavRingHead += count;
    return base;
}
UINT NkDirectX12Device::AllocSamplerRing(UINT count) {
    if (mSamplerRing.capacity == 0) return UINT_MAX;
    if (mSamplerRingHead + count > mSamplerRing.capacity) return UINT_MAX;
    UINT base = mSamplerRingHead;
    mSamplerRingHead += count;
    return base;
}
void NkDirectX12Device::CopyCbvSrvUavToRing(UINT destRingIdx, UINT srcStagingIdx) {
    if (srcStagingIdx == UINT_MAX || destRingIdx == UINT_MAX) return;
    if (!mDevice || mCbvSrvUavRing.capacity == 0) return;
    mDevice->CopyDescriptorsSimple(1, mCbvSrvUavRing.CPUFrom(destRingIdx),
                                   mCbvSrvUavHeap.CPUFrom(srcStagingIdx),
                                   D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
void NkDirectX12Device::CopySamplerToRing(UINT destRingIdx, UINT srcStagingIdx) {
    if (srcStagingIdx == UINT_MAX || destRingIdx == UINT_MAX) return;
    if (!mDevice || mSamplerRing.capacity == 0) return;
    mDevice->CopyDescriptorsSimple(1, mSamplerRing.CPUFrom(destRingIdx),
                                   mSamplerHeap.CPUFrom(srcStagingIdx),
                                   D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}
void NkDirectX12Device::InitNullDescriptors() {
    if (!mDevice) return;
    // SRV NULL (lecture = 0, jamais de hang). Format/dimension obligatoires.
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Texture2D.MipLevels = 1;
        mNullSrvIdx = mCbvSrvUavHeap.allocated;
        mDevice->CreateShaderResourceView(nullptr, &d, mCbvSrvUavHeap.AllocCPU());
    }
    // UAV NULL.
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        mNullUavIdx = mCbvSrvUavHeap.allocated;
        mDevice->CreateUnorderedAccessView(nullptr, nullptr, &d, mCbvSrvUavHeap.AllocCPU());
    }
    // CBV défaut : petit buffer dummy 256 o.
    {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = 256; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc = {1, 0};
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (SUCCEEDED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mDummyCbvBuffer)))) {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cd{};
            cd.BufferLocation = mDummyCbvBuffer->GetGPUVirtualAddress();
            cd.SizeInBytes = 256;
            mDefaultCbvIdx = mCbvSrvUavHeap.allocated;
            mDevice->CreateConstantBufferView(&cd, mCbvSrvUavHeap.AllocCPU());
        }
    }
    // Sampler défaut (linéaire, wrap).
    {
        D3D12_SAMPLER_DESC d{};
        d.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        d.AddressU = d.AddressV = d.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        d.MaxLOD = D3D12_FLOAT32_MAX;
        mDefaultSamplerIdx = mSamplerHeap.allocated;
        mDevice->CreateSampler(&d, mSamplerHeap.AllocCPU());
    }
}
void NkDirectX12Device::FillRingBlockDefaults(UINT csuBase, UINT sampBase) {
    using namespace NkDX12RootLayout;
    if (csuBase != UINT_MAX) {
        for (UINT i = 0; i < NUM_CBV; ++i) CopyCbvSrvUavToRing(csuBase + OFF_CBV + i, mDefaultCbvIdx);
        for (UINT i = 0; i < NUM_SRV; ++i) CopyCbvSrvUavToRing(csuBase + OFF_SRV + i, mNullSrvIdx);
        for (UINT i = 0; i < NUM_UAV; ++i) CopyCbvSrvUavToRing(csuBase + OFF_UAV + i, mNullUavIdx);
    }
    if (sampBase != UINT_MAX)
        for (UINT i = 0; i < NUM_SAMP; ++i) CopySamplerToRing(sampBase + i, mDefaultSamplerIdx);
}
void NkDirectX12Device::LogDREDOutput() {
    if (!mDevice || mDredLogged) return;
    mDredLogged = true;
    ComPtr<ID3D12DeviceRemovedExtendedData> dred;
    if (FAILED(mDevice->QueryInterface(IID_PPV_ARGS(&dred)))) {
        NK_DX12_ERR("DRED: interface indisponible (pas de breadcrumbs)\n");
        return;
    }
    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT bc{};
    if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&bc))) {
        const D3D12_AUTO_BREADCRUMB_NODE* node = bc.pHeadAutoBreadcrumbNode;
        int n = 0;
        while (node && n < 8) {
            UINT last = node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0;
            const char* cl = node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "?";
            NK_DX12_ERR("DRED node[%d] cmdlist='%s' completed=%u/%u\n",
                        n, cl, last, node->BreadcrumbCount);
            if (node->pCommandHistory && last < node->BreadcrumbCount)
                NK_DX12_ERR("DRED   -> op HUNG enum=%u (cf D3D12_AUTO_BREADCRUMB_OP)\n",
                            (unsigned)node->pCommandHistory[last]);
            node = node->pNext; n++;
        }
        if (n == 0) NK_DX12_ERR("DRED: aucun breadcrumb node\n");
    }
    D3D12_DRED_PAGE_FAULT_OUTPUT pf{};
    if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pf))) {
        NK_DX12_ERR("DRED page-fault VA=0x%llX\n", (unsigned long long)pf.PageFaultVA);
        const D3D12_DRED_ALLOCATION_NODE* a = pf.pHeadRecentFreedAllocationNode;
        int n = 0;
        while (a && n < 8) {
            NK_DX12_ERR("DRED   freed-recent[%d] '%s' type=%u\n", n,
                        a->ObjectNameA ? a->ObjectNameA : "?", (unsigned)a->AllocationType);
            a = a->pNext; n++;
        }
    }
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
    // Si la création échoue (ex. device removed pendant un resize), sc1 est null :
    // NE PAS déréférencer (sc1.As planterait en SIGSEGV). On abandonne proprement.
    if (!sc1) { mSwapchain.Reset(); return; }
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
    HRESULT rr0 = mDevice ? mDevice->GetDeviceRemovedReason() : 0;
    NK_DX12_LOG("ResizeSwapchain ENTRY %u×%u (cur %u×%u) deviceRemovedReason=0x%X\n",
                w, h, mWidth, mHeight, (unsigned)rr0);
    if (w == 0 || h == 0) return;
    if (w == mWidth && h == mHeight && mSwapchain) return; // taille inchangée → no-op
    // Device déjà perdu ? abandon propre (évite la cascade de CreateXxx en échec).
    if (mDevice && FAILED(mDevice->GetDeviceRemovedReason())) {
        NK_DX12_ERR("ResizeSwapchain: device removed, skip\n");
        return;
    }
    // Destroy+recreate complet. C'est SÛR pour UN resize (validé au démarrage). Le crash
    // venait des appels EN RAFALE pendant un drag (un WM_SIZE par message) → désormais
    // débouncé côté boucle de jeu (un seul resize après PollEvents). Cf. main.cpp.
    WaitIdle();
    DestroySwapchain();
    mWidth = w; mHeight = h;
    CreateSwapchain(w, h);
}

void NkDirectX12Device::Shutdown() {
    WaitIdle();
    // Drain final + desinscription du callback InfoQueue1 avant que le device parte.
    if (mInfoQueue1 && mInfoQueueCookie != 0) {
        mInfoQueue1->UnregisterMessageCallback(mInfoQueueCookie);
        mInfoQueueCookie = 0;
    }
    if (!mInfoQueue1 && mInfoQueue) {
        DrainDX12InfoQueue(mInfoQueue.Get());
    }
    mInfoQueue1.Reset();
    mInfoQueue.Reset();
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
    // Root signature 1.1 (TABLES LARGES) :
    //   param 0 -> Root constants (b0, space1) — PushConstants (16 DWORD = 64 o)
    //   param 1 -> table CBV b0-31 + SRV t0-31 + UAV u0-7 (space0)
    //   param 2 -> table SAMPLER s0-31 (space0)
    // Flag DESCRIPTORS_VOLATILE : seuls les descripteurs accédés par le shader doivent
    // être valides au draw → on ne copie que les ressources réellement bindées.
    (void)compute;
    using namespace NkDX12RootLayout;

    D3D12_DESCRIPTOR_RANGE1 csuRanges[3]{};
    auto fillRange = [](D3D12_DESCRIPTOR_RANGE1& r, D3D12_DESCRIPTOR_RANGE_TYPE type,
                        UINT num, UINT offset) {
        r.RangeType                         = type;
        r.NumDescriptors                    = num;
        r.BaseShaderRegister                = 0;
        r.RegisterSpace                     = 0;
        r.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
                                            | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        r.OffsetInDescriptorsFromTableStart = offset;
    };
    fillRange(csuRanges[0], D3D12_DESCRIPTOR_RANGE_TYPE_CBV, NUM_CBV, OFF_CBV);
    fillRange(csuRanges[1], D3D12_DESCRIPTOR_RANGE_TYPE_SRV, NUM_SRV, OFF_SRV);
    fillRange(csuRanges[2], D3D12_DESCRIPTOR_RANGE_TYPE_UAV, NUM_UAV, OFF_UAV);

    D3D12_DESCRIPTOR_RANGE1 sampRange{};
    sampRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    sampRange.NumDescriptors                    = NUM_SAMP;
    sampRange.BaseShaderRegister                = 0;
    sampRange.RegisterSpace                     = 0;
    sampRange.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
    sampRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER1 params[NUM_PARAMS]{};
    params[ROOT_CONSTANTS].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[ROOT_CONSTANTS].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;
    params[ROOT_CONSTANTS].Constants.ShaderRegister = 0;
    params[ROOT_CONSTANTS].Constants.RegisterSpace  = 1; // space1 dédié → pas de collision cbuffer
    params[ROOT_CONSTANTS].Constants.Num32BitValues = 16;

    params[TABLE_CBV_SRV_UAV].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[TABLE_CBV_SRV_UAV].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    params[TABLE_CBV_SRV_UAV].DescriptorTable.NumDescriptorRanges = 3;
    params[TABLE_CBV_SRV_UAV].DescriptorTable.pDescriptorRanges   = csuRanges;

    params[TABLE_SAMPLER].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[TABLE_SAMPLER].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;
    params[TABLE_SAMPLER].DescriptorTable.NumDescriptorRanges = 1;
    params[TABLE_SAMPLER].DescriptorTable.pDescriptorRanges   = &sampRange;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC vrsd{};
    vrsd.Version                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    vrsd.Desc_1_1.NumParameters     = NUM_PARAMS;
    vrsd.Desc_1_1.pParameters       = params;
    vrsd.Desc_1_1.NumStaticSamplers = 0;
    vrsd.Desc_1_1.pStaticSamplers   = nullptr;
    vrsd.Desc_1_1.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3D12SerializeVersionedRootSignature(&vrsd, &blob, &err);
    if (FAILED(hr)) {
        NK_DX12_ERR("D3D12SerializeVersionedRootSignature failed (hr=0x%X)\n", (unsigned)hr);
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);

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
    // Un storage buffer reçoit un UAV plus bas (CreateUnorderedAccessView). DX12
    // EXIGE que la ressource ait été créée avec ALLOW_UNORDERED_ACCESS, sinon la
    // création de l'UAV retire le device (DXGI_ERROR_INVALID_CALL). Le flag n'est
    // valide que sur un heap DEFAULT (interdit sur UPLOAD/READBACK).
    if ((NkHasFlag(desc.bindFlags, NkBindFlags::NK_STORAGE_BUFFER) ||
         desc.type == NkBufferType::NK_STORAGE) &&
        heapType == D3D12_HEAP_TYPE_DEFAULT)
        rd.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);

    DXGI_FORMAT fmt = ToDXGIFormat(desc.format);
    bool isDepth = NkFormatIsDepth(desc.format);
    const bool depthSrvRequested = isDepth && NkHasFlag(desc.bindFlags, NkBindFlags::NK_SHADER_RESOURCE);
    // Textures 3D volumétriques (VoxelAO, LUT couleur 3D…). DX12 EXIGE
    // D3D12_RESOURCE_DIMENSION_TEXTURE3D + des vues SRV/UAV TEXTURE3D ; sans cela
    // un UAV ne peut pas être créé sur une ressource sans ALLOW_UNORDERED_ACCESS
    // -> "Removing Device" (cause du crash DX12 demo5 VoxelAO).
    const bool is3D = (desc.type == NkTextureType::NK_TEX3D) || (desc.depth > 1);
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthSrvFormat = DXGI_FORMAT_UNKNOWN;

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = is3D ? D3D12_RESOURCE_DIMENSION_TEXTURE3D
                                 : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width              = desc.width;
    rd.Height             = desc.height;
    rd.DepthOrArraySize   = is3D ? (UINT16)math::NkMax(desc.depth, 1u)
                                 : (UINT16)math::NkMax(desc.arrayLayers, 1u);
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
            srvd.ViewDimension           = is3D ? D3D12_SRV_DIMENSION_TEXTURE3D
                                         : (desc.type == NkTextureType::NK_CUBE ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D);
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            if (is3D)
                srvd.Texture3D.MipLevels   = rd.MipLevels;
            else if (desc.type == NkTextureType::NK_CUBE)
                srvd.TextureCube.MipLevels = rd.MipLevels;
            else
                srvd.Texture2D.MipLevels   = rd.MipLevels;
            mDevice->CreateShaderResourceView(res.Get(), &srvd, mCbvSrvUavHeap.AllocCPU());
        }
        if (NkHasFlag(desc.bindFlags, NkBindFlags::NK_UNORDERED_ACCESS)) {
            t.uavIdx = mCbvSrvUavHeap.allocated;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavd{};
            D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavd = nullptr;
            if (is3D) {
                uavd.Format               = fmt;
                uavd.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE3D;
                uavd.Texture3D.MipSlice   = 0;
                uavd.Texture3D.FirstWSlice = 0;
                uavd.Texture3D.WSize      = (UINT)math::NkMax(desc.depth, 1u);
                pUavd = &uavd;
            }
            mDevice->CreateUnorderedAccessView(res.Get(), nullptr, pUavd, mCbvSrvUavHeap.AllocCPU());
        }
    }

    // Upload données initiales
    if (desc.initialData) {
        uint32 bpp = NkFormatBytesPerPixel(desc.format);
        uint32 rowPitch = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
        // DX12 exige que le RowPitch du placed-footprint soit aligné sur 256 octets
        // (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) ; sinon CopyTextureRegion provoque une
        // faute GPU -> "Removing Device". On copie donc ligne par ligne dans le
        // staging avec un pitch aligné, et on déclare ce pitch dans le footprint.
        const uint32 kPitchAlign = 256u; // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
        uint32 alignedRowPitch = (rowPitch + (kPitchAlign - 1)) & ~(kPitchAlign - 1);
        uint64 imgSz = (uint64)alignedRowPitch * desc.height;

        NkBufferDesc sd = NkBufferDesc::Staging(imgSz);
        auto stageH = CreateBuffer(sd);
        auto& stage = mBuffers[stageH.id];
        for (uint32 y = 0; y < desc.height; ++y) {
            memcpy((uint8*)stage.mapped + (uint64)y * alignedRowPitch,
                   (const uint8*)desc.initialData + (uint64)y * rowPitch,
                   (size_t)rowPitch);
        }

        ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
            TransitionResource(cmd, res.Get(), initState, D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION dst{};
            dst.pResource        = res.Get();
            dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;
            D3D12_TEXTURE_COPY_LOCATION src{};
            src.pResource        = stage.resource.Get();
            src.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Offset    = 0;
            src.PlacedFootprint.Footprint = { rd.Format, desc.width, desc.height, 1, alignedRowPitch };
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
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
    uint32 mip, uint32 layer, uint32 rowPitch) {
    auto it = mTextures.Find(t.id); if(!it) return false;
    uint32 bpp = NkFormatBytesPerPixel(it->desc.format);
    uint32 rp  = rowPitch > 0 ? rowPitch : w * bpp;
    // RowPitch du placed-footprint aligné sur 256 (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT),
    // sinon CopyTextureRegion -> faute GPU -> "Removing Device".
    const uint32 kPitchAlign = 256u;
    uint32 alignedRowPitch = (rp + (kPitchAlign - 1)) & ~(kPitchAlign - 1);
    uint64 sz  = (uint64)alignedRowPitch * h;
    NkBufferDesc sd = NkBufferDesc::Staging(sz);
    auto stageH = CreateBuffer(sd);
    auto& stage = mBuffers[stageH.id];
    for (uint32 row = 0; row < h; ++row) {
        memcpy((uint8*)stage.mapped + (uint64)row * alignedRowPitch,
               (const uint8*)pixels + (uint64)row * rp,
               (size_t)rp);
    }

    auto prevState = it->state;
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, it->resource.Get(), prevState, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = it->resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        // Subresource = mip + arrayLayer * mipLevels (identique a D3D11CalcSubresource).
        // Bug IBL/cubemap DX12 : on ignorait `layer` -> les 6 faces d'un cubemap (et les
        // tranches d'un array) ecrivaient toutes dans la sous-ressource du mip 0 face 0,
        // laissant les faces 1..5 noires -> skybox/IBL noir, metal PBR reflechit du noir.
        {
            // Nombre de mips REEL de la ressource (gere le cas desc.mipLevels==0 = chaine
            // complete, resolu en MipLevels concret a la creation).
            uint32 mips = (uint32)it->resource->GetDesc().MipLevels;
            if (mips == 0) mips = 1;
            dst.SubresourceIndex = mip + layer * mips;
        }
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = stage.resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset    = 0;
        src.PlacedFootprint.Footprint = { it->format, w, h, 1, alignedRowPitch };
        // Le staging ne contient que la région w×h (0-based) ; le décalage destination
        // (x,y) est passé à CopyTextureRegion.
        D3D12_BOX box{ 0, 0, 0, w, h, 1 };
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    D3D12_SAMPLER_DESC sd{};
    // Anisotrope si maxAnisotropy>1 : anti-aliasing a angle rasant (avec mipmaps).
    if (!d.compareEnable && d.maxAnisotropy > 1.f)
        sd.Filter     = D3D12_FILTER_ANISOTROPIC;
    else
        sd.Filter     = ToDX12Filter(d.magFilter, d.minFilter, d.mipFilter, d.compareEnable);
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    mSamplers.Erase(h.id); h.id = 0;
}

// =============================================================================
#ifdef NK_HAS_DXC
// =============================================================================
// Compilation HLSL -> DXIL via DXC (vrai compilateur Shader Model 6 de DX12).
// dxcompiler.dll est chargé dynamiquement (1re fois) -> pas de dépendance lien.
// CLSIDs hardcodés (stables) pour ne pas avoir besoin de dxcompiler.lib.
// Retourne false si dxc indisponible (le device retombe alors sur fxc).
// =============================================================================
namespace {
    // CLSIDs + IIDs DXC hardcodés : clang-mingw ignore __declspec(uuid()) donc
    // __uuidof/IID_PPV_ARGS ne marchent pas pour ces interfaces -> on passe les
    // IID explicitement (valeurs canoniques tirées de dxcapi.h).
    static const CLSID kCLSID_DxcCompiler =
        {0x73e22d93,0xe6ce,0x47f3,{0xb5,0xbf,0xf0,0x66,0x4f,0x39,0xc1,0xb0}};
    static const IID kIID_IDxcCompiler3 =
        {0x228B4687,0x5A6A,0x4730,{0x90,0x0C,0x97,0x02,0xB2,0x20,0x3F,0x54}};
    static const IID kIID_IDxcResult =
        {0x58346CDA,0xDDE7,0x4497,{0x94,0x61,0x6F,0x87,0xAF,0x5E,0x06,0x59}};
    static const IID kIID_IDxcBlob =
        {0x8BA5FB08,0x5195,0x40e2,{0xAC,0x58,0x0D,0x98,0x9C,0x3A,0x01,0x02}};
    static const IID kIID_IDxcBlobUtf8 =
        {0x3DA636C9,0xBA71,0x4024,{0xA3,0x01,0x30,0xCB,0xF1,0x25,0x30,0x5B}};
    typedef HRESULT (__stdcall *NkDxcCreateInstanceFn)(REFCLSID, REFIID, LPVOID*);

    template <typename T>
    static void** NkPpv(ComPtr<T>& p) { return reinterpret_cast<void**>(p.GetAddressOf()); }

    static bool NkDxcCompileHLSL(const char* src, const wchar_t* profile,
                                 const char* entryUtf8,
                                 NkVector<uint8>& out, NkString& errOut) {
        // Switch diagnostic : NK_DISABLE_DXC=1 force le fallback fxc.
        if (getenv("NK_DISABLE_DXC")) { errOut = "dxc desactive (NK_DISABLE_DXC)"; return false; }
        static NkDxcCreateInstanceFn createInstance = nullptr;
        static bool tried = false;
        if (!tried) {
            tried = true;
            HMODULE dll = LoadLibraryA("dxcompiler.dll");
            if (dll) createInstance =
                (NkDxcCreateInstanceFn)GetProcAddress(dll, "DxcCreateInstance");
        }
        if (!createInstance) { errOut = "dxcompiler.dll indisponible"; return false; }

        ComPtr<IDxcCompiler3> compiler;
        if (FAILED(createInstance(kCLSID_DxcCompiler, kIID_IDxcCompiler3, NkPpv(compiler)))
            || !compiler) {
            errOut = "DxcCreateInstance(compiler) a echoue"; return false;
        }

        DxcBuffer buf{};
        buf.Ptr = src; buf.Size = strlen(src); buf.Encoding = DXC_CP_UTF8;

        // Entree : convertir l'UTF-8 (s.entryPoint, ex "VSMain") en wchar pour dxc.
        // Bug historique : "-E main" code en dur -> "missing entry point definition"
        // quand le generateur NkSL nomme l'entree VSMain/PSMain.
        wchar_t entryW[128];
        {
            const char* e = (entryUtf8 && *entryUtf8) ? entryUtf8 : "main";
            usize n = 0;
            for (; e[n] && n < 127; ++n) entryW[n] = (wchar_t)(unsigned char)e[n];
            entryW[n] = L'\0';
        }
        const wchar_t* args[] = { L"-T", profile, L"-E", entryW };
        ComPtr<IDxcResult> result;
        HRESULT hr = compiler->Compile(&buf, args, (UINT32)(sizeof(args)/sizeof(args[0])),
                                       nullptr, kIID_IDxcResult, NkPpv(result));
        if (FAILED(hr) || !result) { errOut = "IDxcCompiler3::Compile a echoue"; return false; }

        HRESULT status = E_FAIL; result->GetStatus(&status);
        if (FAILED(status)) {
            ComPtr<IDxcBlobUtf8> errs;
            result->GetOutput(DXC_OUT_ERRORS, kIID_IDxcBlobUtf8, NkPpv(errs), nullptr);
            if (errs && errs->GetStringLength())
                errOut = NkString((const char*)errs->GetStringPointer());
            else errOut = "dxc: erreur de compilation";
            return false;
        }
        ComPtr<IDxcBlob> obj;
        result->GetOutput(DXC_OUT_OBJECT, kIID_IDxcBlob, NkPpv(obj), nullptr);
        if (!obj || obj->GetBufferSize() == 0) { errOut = "dxc: DXIL vide"; return false; }
        out.Assign((const uint8*)obj->GetBufferPointer(), (usize)obj->GetBufferSize());
        return true;
    }
} // namespace
#endif // NK_HAS_DXC

// Shaders (DXBC/DXIL via DXC en priorité, sinon D3DCompile, ou pré-compilés)
// =============================================================================
NkShaderHandle NkDirectX12Device::CreateShader(const NkShaderDesc& desc) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
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
            // Diag : NK_DX12_DUMPHLSL=1 logge le HLSL que le device compile
            // (utile pour comparer NkSL vs SPIRV-Cross sur un PSO qui echoue).
            if (getenv("NK_DX12_DUMPHLSL"))
                NK_DX12_LOG("[DUMPHLSL stage %u]\n%s\n", (unsigned)s.stage, s.hlslSource);
#ifdef NK_HAS_DXC
            // 1) Vrai compilateur DX12 : dxc -> DXIL (Shader Model 6.0).
            const wchar_t* dxcProfile =
                  s.stage == NkShaderStage::NK_VERTEX   ? L"vs_6_0"
                : s.stage == NkShaderStage::NK_FRAGMENT ? L"ps_6_0"
                : s.stage == NkShaderStage::NK_COMPUTE  ? L"cs_6_0"
                : s.stage == NkShaderStage::NK_GEOMETRY ? L"gs_6_0" : nullptr;
            NkString dxcErr;
            bool dxcOk = dxcProfile &&
                         NkDxcCompileHLSL(s.hlslSource, dxcProfile, entry, *target, dxcErr);
            if (dxcOk) {
                stageCompiled = true;
            } else {
                if (dxcProfile && !dxcErr.Empty())
                    NK_DX12_ERR("dxc stage %u indispo/echec -> fallback fxc: %s\n",
                                (unsigned)s.stage, dxcErr.CStr());
#endif
                // 2) Fallback : fxc (D3DCompile) -> DXBC SM5.1.
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
#ifdef NK_HAS_DXC
            }
#endif
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    mShaders.Erase(h.id); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
// ─────────────────────────────────────────────────────────────────────────────
// Fix #613 : helpers de formats RTV/DSV pour les variantes PSO.
// ─────────────────────────────────────────────────────────────────────────────
void NkDirectX12Device::RenderPassFormats(NkRenderPassHandle rp, UINT& numRT,
                                          DXGI_FORMAT rtvFormats[8],
                                          DXGI_FORMAT& dsvFormat) const {
    // (appelant détient déjà mMutex)
    auto rpit = mRenderPasses.Find(rp.id);
    if (rpit) {
        numRT = (UINT)rpit->desc.colorAttachments.Size();
        if (numRT > 8) numRT = 8;
        for (UINT i = 0; i < numRT; i++)
            rtvFormats[i] = ToDXGIFormat(rpit->desc.colorAttachments[i].format);
        for (UINT i = numRT; i < 8; i++) rtvFormats[i] = DXGI_FORMAT_UNKNOWN;
        dsvFormat = DXGI_FORMAT_UNKNOWN;
        if (rpit->desc.hasDepth)
            dsvFormat = NkFormatIsDepth(rpit->desc.depthAttachment.format)
                ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
    } else {
        // Fallback swapchain (cf. GetSwapchainFormat : B8G8R8A8_UNORM + D32_FLOAT).
        numRT = 1;
        rtvFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        for (UINT i = 1; i < 8; i++) rtvFormats[i] = DXGI_FORMAT_UNKNOWN;
        dsvFormat = DXGI_FORMAT_D32_FLOAT;
    }
}

uint64 NkDirectX12Device::FmtSignature(UINT numRT, const DXGI_FORMAT* rtvFormats,
                                       DXGI_FORMAT dsvFormat) {
    // FNV-1a 64 bits sur (numRT, RTVFormats[0..numRT-1], dsvFormat).
    uint64 h = 1469598103934665603ull;
    auto mix = [&](uint32 v) {
        for (int b = 0; b < 4; b++) { h ^= (uint8)(v >> (b*8)); h *= 1099511628211ull; }
    };
    mix(numRT);
    for (UINT i = 0; i < numRT && i < 8; i++) mix((uint32)rtvFormats[i]);
    mix((uint32)dsvFormat);
    return h;
}

// Construit un PSO graphics avec un jeu EXPLICITE de formats RTV/DSV (le reste du
// state vient de d). rootSig est partagée par toutes les variantes du pipeline.
ComPtr<ID3D12PipelineState> NkDirectX12Device::BuildGraphicsPSO(
        const NkGraphicsPipelineDesc& d, ID3D12RootSignature* rootSig,
        UINT numRT, const DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat) {
    // (appelant détient déjà mMutex)
    auto sit = mShaders.Find(d.shader.id);
    if (!sit) return nullptr;
    auto& sh = *sit;
    if (sh.vs.bytecode.empty()) return nullptr;

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
    psd.pRootSignature = rootSig;
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
    psd.SampleDesc = { (UINT)d.samples >= 1 ? (UINT)d.samples : 1u, 0 };

    // ── Formats RTV/DSV EXPLICITES (fix #613) ──────────────────────────────────
    psd.NumRenderTargets = numRT;
    for (UINT i = 0; i < numRT && i < 8; i++) psd.RTVFormats[i] = rtvFormats[i];
    psd.DSVFormat = dsvFormat;

    if (numRT > 0) {
        if (sh.ps.bytecode.empty()) {
            NK_DX12_ERR("BuildGraphicsPSO: missing pixel shader with color outputs\n");
            return nullptr;
        }
        psd.PS = sh.ps.bc();
    }

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = mDevice->CreateGraphicsPipelineState(&psd, IID_PPV_ARGS(&pso));
    if (FAILED(hr) || !pso) {
        NK_DX12_ERR("CreateGraphicsPipelineState failed (hr=0x%X)\n", (unsigned)hr);
        NK_DX12_ERR("  PSO desc: numInputElems=%u NumRenderTargets=%u RTV0=%d DSVFormat=%d "
                    "DepthEnable=%d topoType=%d sampleCount=%u VS=%zu PS=%zu rootSig=%p\n",
                    (unsigned)elems.size(), (unsigned)psd.NumRenderTargets,
                    (int)psd.RTVFormats[0], (int)psd.DSVFormat,
                    (int)psd.DepthStencilState.DepthEnable,
                    (int)psd.PrimitiveTopologyType, (unsigned)psd.SampleDesc.Count,
                    (size_t)sh.vs.bytecode.size(), (size_t)sh.ps.bytecode.size(),
                    (void*)rootSig);
        for (uint32 ie = 0; ie < (uint32)elems.size(); ie++)
            NK_DX12_ERR("    in[%u] sem=%s idx=%u fmt=%d slot=%u off=%u\n", ie,
                        elems[ie].SemanticName ? elems[ie].SemanticName : "(null)",
                        (unsigned)elems[ie].SemanticIndex, (int)elems[ie].Format,
                        (unsigned)elems[ie].InputSlot, (unsigned)elems[ie].AlignedByteOffset);
        if (mInfoQueue) DrainDX12InfoQueue(mInfoQueue.Get());
        return nullptr;
    }
    return pso;
}

// Résout le PSO de pipelineId compatible avec le RP actif (formats RTV/DSV). Réutilise
// le PSO de base si signature identique ; sinon construit/cache une variante.
ID3D12PipelineState* NkDirectX12Device::ResolvePipelineForRenderPass(uint64 pipelineId,
                                                                     NkRenderPassHandle rp) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    auto* pipe = mPipelines.Find(pipelineId);
    if (!pipe || pipe->isCompute) return nullptr;

    UINT numRT; DXGI_FORMAT rtv[8]; DXGI_FORMAT dsv;
    RenderPassFormats(rp, numRT, rtv, dsv);
    uint64 sig = FmtSignature(numRT, rtv, dsv);

    if (sig == pipe->baseFmtSig) return pipe->pso.Get();   // PSO de base déjà compatible

    auto* v = pipe->variants.Find(sig);
    if (v && *v) return v->Get();

    // Nouvelle variante : reconstruire avec les formats du RP courant.
    ComPtr<ID3D12PipelineState> pso =
        BuildGraphicsPSO(pipe->desc, pipe->rootSig.Get(), numRT, rtv, dsv);
    if (!pso) {
        // Echec build : retomber sur le PSO de base (au pire #613, mais pas de crash).
        return pipe->pso.Get();
    }
    pipe->variants[sig] = pso;
    return pso.Get();
}

NkPipelineHandle NkDirectX12Device::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    auto sit = mShaders.Find(d.shader.id); if(!sit) return {};
    auto& sh = *sit;
    if (sh.vs.bytecode.empty()) {
        NK_DX12_ERR("CreateGraphicsPipeline: missing vertex shader bytecode\n");
        return {};
    }

    auto rootSig = CreateDefaultRootSignature(false);
    if (!rootSig) {
        NK_DX12_ERR("CreateGraphicsPipeline: root signature invalid\n");
        return {};
    }

    // Formats RTV/DSV du PSO de BASE = ceux de d.renderPass (ou fallback swapchain).
    // Le bon format pour chaque passe est ensuite garanti au 1er bind via
    // ResolvePipelineForRenderPass (fix #613) qui construit une variante au besoin.
    UINT numRT; DXGI_FORMAT rtv[8]; DXGI_FORMAT dsv;
    RenderPassFormats(d.renderPass, numRT, rtv, dsv);

    ComPtr<ID3D12PipelineState> pso =
        BuildGraphicsPSO(d, rootSig.Get(), numRT, rtv, dsv);
    if (!pso) return {};   // BuildGraphicsPSO a déjà loggé le détail.

    NkDX12Pipeline p; p.pso = pso; p.rootSig = rootSig;
    p.isCompute = false;
    p.topology  = ToDX12Topology(d.topology);
    p.desc      = d;                                   // pour reconstruire les variantes
    p.baseFmtSig= FmtSignature(numRT, rtv, dsv);       // signature du PSO de base
    for (uint32 i = 0; i < d.vertexLayout.bindings.Size(); ++i) {
        const NkVertexBinding& b = d.vertexLayout.bindings[i];
        if (b.binding < D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            p.vertexStrides[b.binding] = b.stride;
    }

    uint64 hid = NextId(); mPipelines[hid] = std::move(p);
    NkPipelineHandle h; h.id = hid; return h;
}

NkPipelineHandle NkDirectX12Device::CreateComputePipeline(const NkComputePipelineDesc& d) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
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
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    mPipelines.Erase(h.id); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers (metadata only en DX12)
// =============================================================================
NkRenderPassHandle NkDirectX12Device::CreateRenderPass(const NkRenderPassDesc& d) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    uint64 hid = NextId(); mRenderPasses[hid] = { d };
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkDirectX12Device::DestroyRenderPass(NkRenderPassHandle& h) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex); mRenderPasses.Erase(h.id); h.id = 0;
}

NkFramebufferHandle NkDirectX12Device::CreateFramebuffer(const NkFramebufferDesc& d) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    NkDX12Framebuffer fb; fb.w = d.width; fb.h = d.height;

    // Cause A (ecran noir DX12) : on synthetise un NkRenderPass dont les formats
    // d'attachments = formats REELS des textures du FB. Ce RP est expose via
    // GetFramebufferRenderPass() ; le renderer s'en sert pour creer ses PSO offscreen
    // (Geometry R16F, post-process R8/R16...). Sans ca, pd.renderPass reste {} cote
    // renderer => CreateGraphicsPipeline tombe sur le fallback swapchain B8G8R8A8_UNORM
    // => RENDER_TARGET_FORMAT_MISMATCH_PIPELINE_STATE (#613) au draw, rien ne s'affiche.
    // (Le FB peut aussi etre cree avec un d.renderPass explicite : on le respecte alors.)
    NkRenderPassDesc rpd;

    for (uint32 i = 0; i < d.colorAttachments.Size(); i++) {
        auto it = mTextures.Find(d.colorAttachments[i].id);
        if(it) {
            fb.rtvIdxs[fb.rtvCount] = it->rtvIdx;
            fb.colorTexIds[fb.rtvCount] = d.colorAttachments[i].id;
            fb.rtvCount++;
            NkAttachmentDesc ad;
            ad.format  = it->desc.format;          // format reel de la texture RT
            ad.loadOp  = NkLoadOp::NK_LOAD;        // sans incidence sur la compat PSO
            ad.storeOp = NkStoreOp::NK_STORE;
            rpd.AddColor(ad);
        }
    }
    if (d.depthAttachment.IsValid()) {
        auto it = mTextures.Find(d.depthAttachment.id);
        if(it) {
            fb.dsvIdx = it->dsvIdx;
            fb.depthTexId = d.depthAttachment.id;
            NkAttachmentDesc dad;
            dad.format  = it->desc.format;
            dad.loadOp  = NkLoadOp::NK_LOAD;
            dad.storeOp = NkStoreOp::NK_STORE;
            rpd.SetDepth(dad);
        }
    }

    // Respecte un renderPass fourni explicitement ; sinon synthetise depuis les formats.
    if (d.renderPass.IsValid() && mRenderPasses.Find(d.renderPass.id)) {
        fb.renderPassHandle = d.renderPass;
    } else {
        uint64 rpid = NextId();
        mRenderPasses[rpid] = { rpd };
        fb.renderPassHandle.id = rpid;
    }

    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
NkRenderPassHandle NkDirectX12Device::GetFramebufferRenderPass(NkFramebufferHandle fb) const {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    const auto* it = mFramebuffers.Find(fb.id);
    return it ? it->renderPassHandle : NkRenderPassHandle{};
}
void NkDirectX12Device::DestroyFramebuffer(NkFramebufferHandle& h) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    // Libere le RP synthetise par CreateFramebuffer (sauf si c'est le RP swapchain
    // partage ou un RP fourni explicitement et detenu ailleurs).
    if (auto* fbo = mFramebuffers.Find(h.id)) {
        if (fbo->renderPassHandle.IsValid() &&
            fbo->renderPassHandle.id != mSwapchainRP.id)
            mRenderPasses.Erase(fbo->renderPassHandle.id);
    }
    mFramebuffers.Erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDirectX12Device::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    uint64 hid = NextId(); mDescLayouts[hid] = { d };
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDirectX12Device::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex); mDescLayouts.Erase(h.id); h.id = 0;
}
NkDescSetHandle NkDirectX12Device::AllocateDescriptorSet(NkDescSetHandle layout) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
    NkDX12DescSet ds; ds.layoutId = layout.id;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDirectX12Device::FreeDescriptorSet(NkDescSetHandle& h) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex); mDescSets.Erase(h.id); h.id = 0;
}

void NkDirectX12Device::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    threading::NkScopedLock<threading::NkRecursiveMutex> lock(mMutex);
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
    NkDirectX12CommandBuffer* cb = nkentseu::memory::NkGetDefaultAllocator().New<NkDirectX12CommandBuffer>(this, t);
    if (!cb->IsValid()) {
        nkentseu::memory::NkGetDefaultAllocator().Delete(cb);
        return nullptr;
    }
    return cb;
}
void NkDirectX12Device::DestroyCommandBuffer(NkICommandBuffer*& cb) { nkentseu::memory::NkGetDefaultAllocator().Delete(cb); cb = nullptr; }

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

// Décodeur half-float (IEEE 754 binary16) → float. Utilisé par le diag de grille HDR.
static float NkHalfToFloat(uint16 h) {
    uint32 sign = (uint32)(h & 0x8000) << 16;
    uint32 exp  = (h >> 10) & 0x1F;
    uint32 mant = h & 0x3FF;
    uint32 f;
    if (exp == 0) {
        if (mant == 0) { f = sign; }
        else {
            exp = 127 - 15 + 1;
            while ((mant & 0x400) == 0) { mant <<= 1; exp--; }
            mant &= 0x3FF;
            f = sign | (exp << 23) | (mant << 13);
        }
    } else if (exp == 0x1F) {
        f = sign | 0x7F800000 | (mant << 13);
    } else {
        f = sign | ((exp - 15 + 127) << 23) | (mant << 13);
    }
    float out; memcpy(&out, &f, 4); return out;
}

// DIAGNOSTIC (gated NK_DX12_READBACK>=2) : copie tout le HDR RT plein écran dans un staging
// READBACK, puis échantillonne une grille 16x16 et compte les points qui s'écartent
// nettement du fond ambiant → prouve si la géométrie 3D se rend (clusters) ou est
// dégénérée (tout == fond). Staging = ID3D12Resource COM (pas de heap NKMemory).
void NkDirectX12Device::ReadbackHDRGridDiag(ID3D12Resource* res, D3D12_RESOURCE_STATES prev,
                                            uint32 tw, uint32 th) {
    if (!res) return;
    const uint32 bpp = 8; // RGBA16F
    const uint32 kAlign = 256u;
    uint32 rowPitch = (tw * bpp + (kAlign - 1)) & ~(kAlign - 1);
    uint64 total = (uint64)rowPitch * th;

    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = total; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc = {1, 0};
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> st;
    if (FAILED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&st))) || !st) return;

    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, res, prev, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource = res;
        s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource = st.Get();
        d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        d.PlacedFootprint.Offset = 0;
        d.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.PlacedFootprint.Footprint.Width = tw;
        d.PlacedFootprint.Footprint.Height = th;
        d.PlacedFootprint.Footprint.Depth = 1;
        d.PlacedFootprint.Footprint.RowPitch = rowPitch;
        cmd->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);
        TransitionResource(cmd, res, D3D12_RESOURCE_STATE_COPY_SOURCE, prev);
    });

    void* m = nullptr; D3D12_RANGE rr{0, (SIZE_T)total};
    if (FAILED(st->Map(0, &rr, &m)) || !m) return;
    const uint8* base = (const uint8*)m;

    // Fond ambiant approx (luminance) : on échantillonne le coin (0,0) comme référence.
    const uint16* p00 = (const uint16*)(base);
    float bg_r = NkHalfToFloat(p00[0]), bg_g = NkHalfToFloat(p00[1]), bg_b = NkHalfToFloat(p00[2]);
    float bgLum = 0.299f*bg_r + 0.587f*bg_g + 0.114f*bg_b;

    const int G = 16;
    int nonBg = 0, bright = 0;
    float maxLum = -1.f, minLum = 1e9f; int maxX = 0, maxY = 0;
    NkString row;
    for (int gy = 0; gy < G; ++gy) {
        for (int gx = 0; gx < G; ++gx) {
            uint32 x = (uint32)((gx + 0.5f) / G * tw);
            uint32 y = (uint32)((gy + 0.5f) / G * th);
            if (x >= tw) x = tw - 1; if (y >= th) y = th - 1;
            const uint16* px = (const uint16*)(base + (uint64)y * rowPitch + (uint64)x * bpp);
            float r = NkHalfToFloat(px[0]), g = NkHalfToFloat(px[1]), b = NkHalfToFloat(px[2]);
            float lum = 0.299f*r + 0.587f*g + 0.114f*b;
            if (lum > maxLum) { maxLum = lum; maxX = (int)x; maxY = (int)y; }
            if (lum < minLum) minLum = lum;
            // « non-fond » = écart relatif > 25% de la luminance du fond + marge absolue.
            bool nb = (lum > bgLum * 1.25f + 0.02f) || (lum < bgLum * 0.75f - 0.001f);
            if (nb) nonBg++;
            if (lum > bgLum * 2.0f + 0.1f) bright++;
            row += (lum > bgLum * 2.0f + 0.1f) ? '#' : (nb ? '+' : '.');
        }
        NK_DX12_LOG("READBACK HDRGRID %s\n", row.CStr());
        row.Clear();
    }
    NK_DX12_LOG("READBACK HDRGRID SUMMARY bgLum=%.4f nonBg=%d/256 bright=%d minLum=%.4f maxLum=%.4f at(%d,%d)\n",
                bgLum, nonBg, bright, minLum, maxLum, maxX, maxY);
    st->Unmap(0, nullptr);
}

// DIAGNOSTIC (gated NK_DX12_READBACK>=2) : grille de profondeurs sur une texture DEPTH
// (atlas d'ombre D32_FLOAT). Compte les texels avec une profondeur < 1.0 (= géométrie
// rendue). Staging = ID3D12Resource COM. La depth se copie en R32_FLOAT.
void NkDirectX12Device::ReadbackDepthGridDiag(ID3D12Resource* res, D3D12_RESOURCE_STATES prev,
                                              uint32 tw, uint32 th, uint64 tid) {
    if (!res) return;
    const uint32 bpp = 4; // R32_FLOAT
    const uint32 kAlign = 256u;
    uint32 rowPitch = (tw * bpp + (kAlign - 1)) & ~(kAlign - 1);
    uint64 total = (uint64)rowPitch * th;

    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = total; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc = {1, 0};
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> st;
    if (FAILED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&st))) || !st) return;

    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, res, prev, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource = res;
        s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource = st.Get();
        d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        d.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
        d.PlacedFootprint.Footprint.Width = tw;
        d.PlacedFootprint.Footprint.Height = th;
        d.PlacedFootprint.Footprint.Depth = 1;
        d.PlacedFootprint.Footprint.RowPitch = rowPitch;
        cmd->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);
        TransitionResource(cmd, res, D3D12_RESOURCE_STATE_COPY_SOURCE, prev);
    });

    void* m = nullptr; D3D12_RANGE rr{0, (SIZE_T)total};
    if (FAILED(st->Map(0, &rr, &m)) || !m) return;
    const uint8* base = (const uint8*)m;
    const int G = 16;
    int written = 0; float dmin = 1e9f, dmax = -1e9f;
    for (int gy = 0; gy < G; ++gy) {
        for (int gx = 0; gx < G; ++gx) {
            uint32 x = (uint32)((gx + 0.5f) / G * tw);
            uint32 y = (uint32)((gy + 0.5f) / G * th);
            if (x >= tw) x = tw - 1; if (y >= th) y = th - 1;
            float dv; memcpy(&dv, base + (uint64)y * rowPitch + (uint64)x * bpp, 4);
            if (dv < 0.9999f) written++;
            if (dv < dmin) dmin = dv;
            if (dv > dmax) dmax = dv;
        }
    }
    NK_DX12_LOG("READBACK SHADOWATLAS id=%llu %ux%u written(<1.0)=%d/256 dmin=%.4f dmax=%.4f\n",
                (unsigned long long)tid, tw, th, written, dmin, dmax);
    st->Unmap(0, nullptr);
}

// DIAGNOSTIC readback : lire le pixel central du backbuffer courant et le loguer.
// Gate stricte sur la variable d'env NK_DX12_READBACK. Le staging est un ID3D12Resource
// COM (Release auto via ComPtr) — PAS du heap NKMemory, donc aucune règle Alloc/Free
// NKMemory n'est violée ici.
void NkDirectX12Device::ReadbackBackbufferCenterDiag() {
    static int sEnabled = -1;
    static bool sDumpedRTs = false;
    if (sEnabled == -1) {
        const char* v = getenv("NK_DX12_READBACK");
        sEnabled = (v && v[0] && v[0] != '0') ? (v[0] - '0' >= 2 ? 2 : 1) : 0;
        if (sEnabled) NK_DX12_LOG("READBACK diag ACTIF niveau=%d (NK_DX12_READBACK)\n", sEnabled);
    }
    if (!sEnabled || !mDevice) return;

    // Niveau 2 : une seule fois (3e frame, RG stabilisé), dumpe le centre de TOUS les RT
    // offscreen (non-swapchain) ayant un RTV → révèle quelle passe produit/perd l'image.
    if (sEnabled >= 2 && !sDumpedRTs && mFrameNumber >= 3) {
        sDumpedRTs = true;

        // CUBEMAP diag : pour chaque cube (arrayLayers>=6), lit 5 texels (centre + 4 coins)
        // des faces 0/2/4. Permet de distinguer : faces identiques (upload dupliqué),
        // faces différentes mais plates (source plate), ou variation interne (contenu OK
        // → bug = sampling). Lit le format RGBA32F (16 o/texel) ou RGBA8 (4 o).
        for (auto& [ctid, ct] : mTextures) {
            if (ct.isSwapchain || !ct.resource) continue;
            uint32 layers = (ct.desc.type == NkTextureType::NK_CUBE)
                          ? 6u : (uint32)ct.desc.arrayLayers;
            if (layers < 6) continue; // pas un cube
            uint32 cw = (uint32)ct.desc.width, chh = (uint32)ct.desc.height;
            if (cw < 4 || chh < 4) continue;
            DXGI_FORMAT cf = ct.format;
            uint32 cbpp = (cf == DXGI_FORMAT_R32G32B32A32_FLOAT) ? 16u
                        : (cf == DXGI_FORMAT_R16G16B16A16_FLOAT) ? 8u : 4u;
            uint32 mipsReal = (uint32)ct.resource->GetDesc().MipLevels; if (mipsReal == 0) mipsReal = 1;
            for (uint32 face : {0u, 2u, 4u}) {
                // points : centre + 4 positions internes pour mesurer la variation.
                struct PT { uint32 x, y; };
                PT pts[3] = { {cw/2, chh/2}, {cw/4, chh/4}, {(3*cw)/4, (3*chh)/4} };
                float vals[3][4] = {};
                for (int pi = 0; pi < 3; ++pi) {
                    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
                    D3D12_RESOURCE_DESC rd{};
                    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                    rd.Width = 256; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
                    rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc = {1, 0};
                    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                    ComPtr<ID3D12Resource> cst;
                    if (FAILED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&cst))) || !cst) continue;
                    ID3D12Resource* cres = ct.resource.Get();
                    D3D12_RESOURCE_STATES cprev = ct.state;
                    UINT subres = 0 + face * mipsReal; // mip0 de la face
                    uint32 cpx = pts[pi].x, cpy = pts[pi].y;
                    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
                        TransitionResource(cmd, cres, cprev, D3D12_RESOURCE_STATE_COPY_SOURCE);
                        D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource = cres;
                        s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = subres;
                        D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource = cst.Get();
                        d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                        d.PlacedFootprint.Footprint.Format = cf;
                        d.PlacedFootprint.Footprint.Width = 1; d.PlacedFootprint.Footprint.Height = 1;
                        d.PlacedFootprint.Footprint.Depth = 1; d.PlacedFootprint.Footprint.RowPitch = 256;
                        D3D12_BOX b{ cpx, cpy, 0, cpx + 1, cpy + 1, 1 };
                        cmd->CopyTextureRegion(&d, 0, 0, 0, &s, &b);
                        TransitionResource(cmd, cres, D3D12_RESOURCE_STATE_COPY_SOURCE, cprev);
                    });
                    void* mm = nullptr; D3D12_RANGE rrr{0, 16};
                    if (SUCCEEDED(cst->Map(0, &rrr, &mm)) && mm) {
                        if (cbpp == 16) { memcpy(vals[pi], mm, 16); }
                        else { const uint8* bb = (const uint8*)mm; for (int k=0;k<4;k++) vals[pi][k] = bb[k]/255.f; }
                        cst->Unmap(0, nullptr);
                    }
                }
                NK_DX12_LOG("READBACK CUBE id=%llu %ux%u fmt=%d face=%u center=[%.3f %.3f %.3f] q1=[%.3f %.3f %.3f] q3=[%.3f %.3f %.3f]\n",
                            (unsigned long long)ctid, cw, chh, (int)cf, face,
                            vals[0][0],vals[0][1],vals[0][2], vals[1][0],vals[1][1],vals[1][2], vals[2][0],vals[2][1],vals[2][2]);
            }
        }

        for (auto& [tid, t] : mTextures) {
            if (t.isSwapchain || t.rtvIdx == UINT_MAX || !t.resource) continue;
            if (t.desc.depth > 1) continue; // skip 3D
            UINT tw = (UINT)t.desc.width, th = (UINT)t.desc.height;
            if (tw == 0 || th == 0) continue;
            UINT px = tw / 2, py = th / 2;
            D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC rd{};
            rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            rd.Width = 256; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
            rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc = {1, 0};
            rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ComPtr<ID3D12Resource> st;
            if (FAILED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&st))) || !st) continue;
            ID3D12Resource* res = t.resource.Get();
            D3D12_RESOURCE_STATES prev = t.state;
            DXGI_FORMAT rfmt = t.format;
            ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
                TransitionResource(cmd, res, prev, D3D12_RESOURCE_STATE_COPY_SOURCE);
                D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource = res;
                s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = 0;
                D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource = st.Get();
                d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                d.PlacedFootprint.Footprint.Format = rfmt;
                d.PlacedFootprint.Footprint.Width = 1; d.PlacedFootprint.Footprint.Height = 1;
                d.PlacedFootprint.Footprint.Depth = 1; d.PlacedFootprint.Footprint.RowPitch = 256;
                D3D12_BOX b{ px, py, 0, px + 1, py + 1, 1 };
                cmd->CopyTextureRegion(&d, 0, 0, 0, &s, &b);
                TransitionResource(cmd, res, D3D12_RESOURCE_STATE_COPY_SOURCE, prev);
            });
            void* m = nullptr; D3D12_RANGE rr{0, 16};
            if (SUCCEEDED(st->Map(0, &rr, &m)) && m) {
                const uint8* b = (const uint8*)m;
                NK_DX12_LOG("READBACK RT id=%llu %ux%u fmt=%d center bytes=%02X%02X%02X%02X %02X%02X%02X%02X\n",
                            (unsigned long long)tid, tw, th, (int)rfmt,
                            b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7]);
                st->Unmap(0, nullptr);
            }

            // GRILLE sur le HDR RT plein écran (fmt 10 = R16G16B16A16_FLOAT à pleine résol.)
            // → distingue « géométrie dégénérée » (tout == fond ambiant) de « objets rendus »
            // (clusters brillants). Half-float décodé en float ; comptage des points dont la
            // luminance s'écarte nettement du fond ambiant (~0.10,0.10,0.14).
            if (rfmt == DXGI_FORMAT_R16G16B16A16_FLOAT && tw >= mWidth && th >= mHeight) {
                ReadbackHDRGridDiag(res, prev, tw, th);
            }
        }

        // Atlas d'ombre = texture DEPTH (dsvIdx, pas de rtvIdx → ignorée par la boucle RT).
        // On échantillonne une grille de profondeurs : si tout == 1.0 → rien rendu dans
        // l'atlas (toute la scène « non occluse » → SampleCmp devrait rendre 1=éclairé, pas
        // 0). Si des valeurs < 1.0 existent → l'atlas contient des profondeurs valides.
        for (auto& [tid, t] : mTextures) {
            if (t.isSwapchain || t.dsvIdx == UINT_MAX || !t.resource) continue;
            UINT tw = (UINT)t.desc.width, th = (UINT)t.desc.height;
            if (tw < 256 || th < 256) continue; // cible l'atlas d'ombre (grand)
            ReadbackDepthGridDiag(t.resource.Get(), t.state, tw, th, tid);
        }
    }

    // Backbuffer courant : texture color[0] du framebuffer swapchain actif.
    if (mBackBufferIdx >= mSwapchainFBs.Size()) return;
    auto fbIt = mFramebuffers.Find(mSwapchainFBs[mBackBufferIdx].id);
    if (!fbIt || fbIt->rtvCount == 0) return;
    auto texIt = mTextures.Find(fbIt->colorTexIds[0]);
    if (!texIt || !texIt->resource) return;
    ID3D12Resource* bb = texIt->resource.Get();

    // GRILLE BACKBUFFER (1x/s) : copie tout le backbuffer BGRA8 et échantillonne une grille
    // 16x16 → prouve si la SCÈNE (sphères colorées) est dans le backbuffer présenté LIVE,
    // ou seulement du gris uniforme. Distingue « fond au centre » d'un « backbuffer plat ».
    {
        static uint64 sLastGridFrame = (uint64)-1;
        if (mFrameNumber != sLastGridFrame && (mFrameNumber % 30 == 0)) {
            sLastGridFrame = mFrameNumber;
            const uint32 bbpp = 4; const uint32 kA = 256u;
            uint32 rp = (mWidth * bbpp + (kA - 1)) & ~(kA - 1);
            uint64 tot = (uint64)rp * mHeight;
            D3D12_HEAP_PROPERTIES bhp{}; bhp.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC brd{};
            brd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            brd.Width = tot; brd.Height = 1; brd.DepthOrArraySize = 1; brd.MipLevels = 1;
            brd.Format = DXGI_FORMAT_UNKNOWN; brd.SampleDesc = {1, 0};
            brd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ComPtr<ID3D12Resource> bst;
            if (SUCCEEDED(mDevice->CreateCommittedResource(&bhp, D3D12_HEAP_FLAG_NONE, &brd,
                    D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&bst))) && bst) {
                DXGI_FORMAT bfmt = texIt->format;
                ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
                    TransitionResource(cmd, bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
                    D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource = bb;
                    s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = 0;
                    D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource = bst.Get();
                    d.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    d.PlacedFootprint.Footprint.Format = bfmt;
                    d.PlacedFootprint.Footprint.Width = mWidth; d.PlacedFootprint.Footprint.Height = mHeight;
                    d.PlacedFootprint.Footprint.Depth = 1; d.PlacedFootprint.Footprint.RowPitch = rp;
                    cmd->CopyTextureRegion(&d, 0, 0, 0, &s, nullptr);
                    TransitionResource(cmd, bb, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
                });
                void* m = nullptr; D3D12_RANGE rr{0, (SIZE_T)tot};
                if (SUCCEEDED(bst->Map(0, &rr, &m)) && m) {
                    const uint8* base = (const uint8*)m;
                    const int G = 16; int colored = 0, distinct = 0;
                    NkString row;
                    // ref = coin haut-gauche (souvent fond)
                    uint8 ref0 = base[0], ref1 = base[1], ref2 = base[2];
                    for (int gy = 0; gy < G; ++gy) {
                        for (int gx = 0; gx < G; ++gx) {
                            uint32 x = (uint32)((gx + 0.5f) / G * mWidth);
                            uint32 y = (uint32)((gy + 0.5f) / G * mHeight);
                            if (x >= mWidth) x = mWidth - 1; if (y >= mHeight) y = mHeight - 1;
                            const uint8* px = base + (uint64)y * rp + (uint64)x * bbpp;
                            // BGRA : couleur « saturée » si max-min des canaux RGB > 30 (objet coloré).
                            int B = px[0], Gc = px[1], R = px[2];
                            int mx = R > Gc ? (R > B ? R : B) : (Gc > B ? Gc : B);
                            int mn = R < Gc ? (R < B ? R : B) : (Gc < B ? Gc : B);
                            bool col = (mx - mn) > 30;
                            int dr = R - ref2, dg = Gc - ref1, db = B - ref0;
                            bool diff = (dr*dr + dg*dg + db*db) > 900;
                            if (col) colored++;
                            if (diff) distinct++;
                            row += col ? '#' : (diff ? '+' : '.');
                        }
                        row.Clear();
                    }
                    // Corréler avec la luminance HDR du même frame : trouver le HDR RT plein
                    // écran (R16F) et lire son max sur une grille → savoir si l'intermittence
                    // est dans le HDR (lighting) ou seulement au tonemap/composite.
                    float hdrMax = -1.f;
                    for (auto& [htid, ht] : mTextures) {
                        if (ht.isSwapchain || ht.rtvIdx == UINT_MAX || !ht.resource) continue;
                        if (ht.format != DXGI_FORMAT_R16G16B16A16_FLOAT) continue;
                        if (ht.desc.width < mWidth || ht.desc.height < mHeight) continue;
                        // lire 16 texels en diagonale via une copie pleine ligne — simplifié :
                        // copier 1 ligne centrale et prendre le max.
                        uint32 hw = (uint32)ht.desc.width, hh = (uint32)ht.desc.height;
                        uint32 hbpp = 8; uint32 hrp = (hw*hbpp + 255) & ~255u;
                        D3D12_HEAP_PROPERTIES hhp{}; hhp.Type = D3D12_HEAP_TYPE_READBACK;
                        D3D12_RESOURCE_DESC hrd{};
                        hrd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                        hrd.Width = hrp; hrd.Height = 1; hrd.DepthOrArraySize = 1; hrd.MipLevels = 1;
                        hrd.Format = DXGI_FORMAT_UNKNOWN; hrd.SampleDesc = {1,0};
                        hrd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                        ComPtr<ID3D12Resource> hst;
                        if (FAILED(mDevice->CreateCommittedResource(&hhp, D3D12_HEAP_FLAG_NONE, &hrd,
                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&hst))) || !hst) break;
                        ID3D12Resource* hres = ht.resource.Get(); D3D12_RESOURCE_STATES hprev = ht.state;
                        uint32 midY = hh/2;
                        ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd){
                            TransitionResource(cmd, hres, hprev, D3D12_RESOURCE_STATE_COPY_SOURCE);
                            D3D12_TEXTURE_COPY_LOCATION s{}; s.pResource=hres; s.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex=0;
                            D3D12_TEXTURE_COPY_LOCATION d{}; d.pResource=hst.Get(); d.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                            d.PlacedFootprint.Footprint.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;
                            d.PlacedFootprint.Footprint.Width=hw; d.PlacedFootprint.Footprint.Height=1; d.PlacedFootprint.Footprint.Depth=1; d.PlacedFootprint.Footprint.RowPitch=hrp;
                            D3D12_BOX b{0, midY, 0, hw, midY+1, 1};
                            cmd->CopyTextureRegion(&d,0,0,0,&s,&b);
                            TransitionResource(cmd, hres, D3D12_RESOURCE_STATE_COPY_SOURCE, hprev);
                        });
                        void* hm=nullptr; D3D12_RANGE hrr{0,(SIZE_T)hrp};
                        if (SUCCEEDED(hst->Map(0,&hrr,&hm)) && hm) {
                            const uint16* line = (const uint16*)hm;
                            for (uint32 x=0; x<hw; x+=16) {
                                float r=NkHalfToFloat(line[x*4+0]), g=NkHalfToFloat(line[x*4+1]), b=NkHalfToFloat(line[x*4+2]);
                                float l=0.299f*r+0.587f*g+0.114f*b; if (l>hdrMax) hdrMax=l;
                            }
                            hst->Unmap(0,nullptr);
                        }
                        break; // un seul HDR RT
                    }
                    NK_DX12_LOG("BBGRID SUMMARY frame=%llu bbIdx=%u frameIdx=%u colored=%d/256 distinct=%d/256 hdrMaxMidLine=%.4f\n",
                                (unsigned long long)mFrameNumber, mBackBufferIdx, mFrameIndex, colored, distinct, hdrMax);

                    // DUMP BMP PLEINE RÉSOLUTION du backbuffer présenté LIVE (gated). Dumpe
                    // UNE frame « plate » (colored<10) ET une fois UNE frame « colorée »
                    // (colored>100) → comparer si le texte HUD apparaît sur les frames colorées.
                    static bool sDumpedFlat = false, sDumpedColored = false;
                    bool doFlat    = (!sDumpedFlat    && colored < 10  && mFrameNumber > 100);
                    bool doColored = (!sDumpedColored && colored > 100 && mFrameNumber > 100);
                    if (doFlat || doColored) {
                        if (doFlat) sDumpedFlat = true; else sDumpedColored = true;
                        char path[256];
                        snprintf(path, sizeof(path), "logs/bb_live_%s_f%llu.bmp",
                                 doColored ? "COLORED" : "flat", (unsigned long long)mFrameNumber);
                        FILE* f = fopen(path, "wb");
                        if (f) {
                            const uint32 W = mWidth, H = mHeight;
                            const uint32 imgSize = W * H * 4;
                            const uint32 fileSize = 54 + imgSize;
                            uint8 hdr[54] = {0};
                            hdr[0]='B'; hdr[1]='M';
                            hdr[2]= fileSize&0xFF; hdr[3]=(fileSize>>8)&0xFF; hdr[4]=(fileSize>>16)&0xFF; hdr[5]=(fileSize>>24)&0xFF;
                            hdr[10]=54;                 // pixel data offset
                            hdr[14]=40;                 // DIB header size
                            hdr[18]= W&0xFF; hdr[19]=(W>>8)&0xFF; hdr[20]=(W>>16)&0xFF; hdr[21]=(W>>24)&0xFF;
                            // hauteur NÉGATIVE → top-down (sinon BMP est bottom-up et l'image serait inversée)
                            int32 negH = -(int32)H;
                            hdr[22]= negH&0xFF; hdr[23]=(negH>>8)&0xFF; hdr[24]=(negH>>16)&0xFF; hdr[25]=(negH>>24)&0xFF;
                            hdr[26]=1;                  // planes
                            hdr[28]=32;                 // bpp
                            hdr[34]= imgSize&0xFF; hdr[35]=(imgSize>>8)&0xFF; hdr[36]=(imgSize>>16)&0xFF; hdr[37]=(imgSize>>24)&0xFF;
                            fwrite(hdr, 1, 54, f);
                            // Copier ligne par ligne (le staging a un rowPitch aligné 256, le BMP non).
                            for (uint32 y = 0; y < H; ++y)
                                fwrite(base + (uint64)y * rp, 1, (size_t)W * 4, f);
                            fclose(f);
                            NK_DX12_LOG("DUMP backbuffer -> %s (%ux%u)\n", path, W, H);
                        }
                    }
                    bst->Unmap(0, nullptr);
                }
            }
        }
    }

    const UINT cx = mWidth  / 2;
    const UINT cy = mHeight / 2;

    // Staging READBACK : 1 ligne de 256 octets (footprint aligné DX12 minimal).
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = 256; rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN; rd.SampleDesc = {1, 0};
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> staging;
    if (FAILED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&staging))) || !staging) {
        NK_DX12_ERR("READBACK: staging alloc failed\n");
        return;
    }

    // Le backbuffer est en PRESENT à cet instant (EndRenderPass l'y remet). On le passe
    // en COPY_SOURCE, on copie la box 1x1 du centre, puis on le remet en PRESENT.
    ExecuteOneShot([&](ID3D12GraphicsCommandList* cmd) {
        TransitionResource(cmd, bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = bb;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = staging.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint.Offset = 0;
        dst.PlacedFootprint.Footprint.Format   = texIt->format;
        dst.PlacedFootprint.Footprint.Width     = 1;
        dst.PlacedFootprint.Footprint.Height    = 1;
        dst.PlacedFootprint.Footprint.Depth     = 1;
        dst.PlacedFootprint.Footprint.RowPitch  = 256;

        D3D12_BOX box{ cx, cy, 0, cx + 1, cy + 1, 1 };
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

        TransitionResource(cmd, bb, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
    });

    void* mapped = nullptr;
    D3D12_RANGE rr{0, 4};
    if (SUCCEEDED(staging->Map(0, &rr, &mapped)) && mapped) {
        const uint8* px = (const uint8*)mapped;
        // Backbuffer = B8G8R8A8_UNORM → octets B,G,R,A. On logue en RGBA pour lisibilité.
        NK_DX12_LOG("READBACK center(%u,%u) RGBA=(%u,%u,%u,%u) [fmt=%d]\n",
                    cx, cy, (unsigned)px[2], (unsigned)px[1], (unsigned)px[0], (unsigned)px[3],
                    (int)texIt->format);
        staging->Unmap(0, nullptr);
    } else {
        NK_DX12_ERR("READBACK: map failed\n");
    }
}

void NkDirectX12Device::SubmitAndPresent(NkICommandBuffer* cb) {
    auto& fd = mFrameData[mFrameIndex];

    NkICommandBuffer* cbs[] = { cb };
    Submit(cbs, 1, {});

    // DIAGNOSTIC (gated NK_DX12_READBACK) : capturer le pixel central avant Present.
    ReadbackBackbufferCenterDiag();

    fd.Signal(mGraphicsQueue.Get(), ++mFenceValue);
    const UINT syncInterval = mVsync ? 1u : 0u;
    const UINT flags = (!mVsync && mAllowTearing && mTearingSupported) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
    HRESULT phr = mSwapchain->Present(syncInterval, flags);
    if (FAILED(phr)) {
        HRESULT rr = mDevice ? mDevice->GetDeviceRemovedReason() : 0;
        NK_DX12_ERR("Present FAILED hr=0x%X deviceRemovedReason=0x%X\n", (unsigned)phr, (unsigned)rr);
        LogDREDOutput(); // dump la dernière opération GPU avant le hang (1 seule fois)
    }
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
    // Null-safe : si le device a été retiré (DEVICE_REMOVED), CreateFence échoue et
    // renvoie un fence null — déréférencer fence->SetEventOnCompletion crasherait.
    // Ce chemin est notamment emprunté par le cleanup après un échec d'init.
    if (!mDevice || !mGraphicsQueue) return;
    ComPtr<ID3D12Fence> fence;
    HRESULT hr = mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr) || !fence) {
        NK_DX12_ERR("WaitIdle: CreateFence failed (hr=0x%X, device removed ?)\n", (unsigned)hr);
        return;
    }
    if (FAILED(mGraphicsQueue->Signal(fence.Get(), 1))) return;
    HANDLE ev = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!ev) return;
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
    ResetDescriptorRingForFrame(mFrameIndex); // ring de descripteurs : nouvelle frame
    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
    mBackBufferIdx    = mSwapchain->GetCurrentBackBufferIndex();
    return true;
}
void NkDirectX12Device::EndFrame(NkFrameContext&) {
    // InfoQueue1 livre les messages via callback ; polling necessaire seulement
    // si on est tombe sur le fallback InfoQueue.
    if (!mInfoQueue1 && mInfoQueue) {
        DrainDX12InfoQueue(mInfoQueue.Get());
    }
    mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES;
    ++mFrameNumber;
}
void NkDirectX12Device::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return;
    // NE PAS poser mWidth/mHeight ici : ResizeSwapchain les compare à w/h pour son no-op
    // (même taille). Si on les posait avant, la garde verrait w==mWidth et SKIPPERAIT
    // toujours le resize → swapchain figé à l'ancienne taille dans une fenêtre redimensionnée
    // → mismatch → DXGI retire le device (faux DEVICE_HUNG). ResizeSwapchain pose mWidth après.
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
    mCaps.computeShaders      = NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_GFX_API_DX12);
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
DXGI_FORMAT NkDirectX12Device::GetTextureDXGIFormat(uint64 id) const {
    auto it = mTextures.Find(id);
    return it ? ToDXGIFormat(it->desc.format) : DXGI_FORMAT_R8G8B8A8_UNORM;
}
D3D12_GPU_VIRTUAL_ADDRESS NkDirectX12Device::GetBufferGPUAddr(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->gpuAddr : 0;
}
UINT NkDirectX12Device::GetBufferCbvIndex(uint64 id) const {
    auto it = mBuffers.Find(id); return it ? it->cbvIdx : UINT_MAX;
}
bool NkDirectX12Device::PeekBufferFloats(uint64 bufId, float* out, uint32 n, uint32 floatOffset) const {
    auto it = mBuffers.Find(bufId);
    if (!it || !it->mapped) return false;
    uint64 byteOff = (uint64)floatOffset * sizeof(float);
    if (byteOff + (uint64)n * sizeof(float) > it->desc.sizeBytes) return false;
    memcpy(out, (const uint8*)it->mapped + byteOff, (size_t)n * sizeof(float));
    return true;
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
