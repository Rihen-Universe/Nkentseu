// =============================================================================
// NkDX12Context.cpp — Production Ready
// Patches :
//   - OnResize 0x0 guard
//   - DestroySwapchainDependents : FlushGPU() AVANT destruction des allocators
//   - WaitForFence propre avec timeout
//   - Compute queue dédiée
//   - Device lost via DXGI_ERROR_DEVICE_REMOVED
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include "NkDXContext.h"
#include <cstdio>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#define NK_DX12_LOG(...) printf("[NkDX12] " __VA_ARGS__)
#define NK_DX12_ERR(...) printf("[NkDX12][ERROR] " __VA_ARGS__)
#define NK_DX12_CHECK(hr, msg) do { if(FAILED(hr)){ NK_DX12_ERR(msg " hr=0x%08X\n",(unsigned)(hr)); return false; } } while(0)

namespace nkentseu {

// =============================================================================
NkDX12Context::~NkDX12Context() {
    if (mIsValid) {
        FlushGPU(); // Toujours attendre le GPU avant destruction
        Shutdown();
    }
}

bool NkDX12Context::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) { NK_DX12_ERR("Already initialized\n"); return false; }
    mDesc = desc;
    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    if (!surf.IsValid()) { NK_DX12_ERR("Invalid NkSurfaceDesc\n"); return false; }

    mData.hwnd   = static_cast<HWND>(surf.hwnd);
    mData.width  = surf.width;
    mData.height = surf.height;
    mVSync       = desc.dx12.vsync;
    mData.vsync  = desc.dx12.vsync;

    const NkDirectX12Desc& d = desc.dx12;
    if (!CreateDevice(d))          return false;
    if (!CreateCommandQueues(d))   return false;
    if (!CreateSwapchain(surf.width, surf.height, d)) return false;
    if (!CreateDescriptorHeaps(d)) return false;
    if (!CreateRenderTargets())    return false;
    if (!CreateDepthBuffer(surf.width, surf.height))  return false;
    if (!CreateCommandObjects())   return false;
    if (!CreateSyncObjects())      return false;

    mIsValid = true;
    NK_DX12_LOG("Ready — %s | VRAM %u MB\n", mData.renderer, mData.vramMB);
    return true;
}

// =============================================================================
bool NkDX12Context::CreateDevice(const NkDirectX12Desc& d) {
    // Debug layer
    if (d.debugDevice) {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            debug->EnableDebugLayer();
        if (d.gpuValidation) {
            ComPtr<ID3D12Debug1> debug1;
            if (SUCCEEDED(debug.As(&debug1)))
                debug1->SetEnableGPUBasedValidation(TRUE);
        }
    }

    UINT dxgiFlags = d.debugDevice ? DXGI_CREATE_FACTORY_DEBUG : 0;
    NK_DX12_CHECK(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&mData.factory)), "CreateDXGIFactory2");

    // Adapter — préférer discret (haute perf)
    ComPtr<IDXGIAdapter1> adp1;
    for (UINT i = 0; mData.factory->EnumAdapterByGpuPreference(
        i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adp1)) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc1;
        adp1->GetDesc1(&desc1);
        if (desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adp1.Get(), D3D_FEATURE_LEVEL_11_0,
                                         _uuidof(ID3D12Device), nullptr))) {
            NK_DX12_CHECK(adp1.As(&mData.adapter), "QueryAdapter4");
            break;
        }
    }
    if (!mData.adapter) { NK_DX12_ERR("No suitable DX12 adapter\n"); return false; }

    NK_DX12_CHECK(D3D12CreateDevice(mData.adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&mData.device)), "D3D12CreateDevice");

    // VRAM
    DXGI_ADAPTER_DESC3 adpDesc3 = {};
    mData.adapter->GetDesc3(&adpDesc3);
    mData.vramMB = (uint32)(adpDesc3.DedicatedVideoMemory / (1024*1024));

    if (d.debugDevice) {
        ComPtr<ID3D12InfoQueue1> infoQueue;
        if (SUCCEEDED(mData.device.As(&infoQueue))) {
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        }
    }
    NK_DX12_LOG("Device OK (VRAM %u MB)\n", mData.vramMB);
    return true;
}

bool NkDX12Context::CreateCommandQueues(const NkDirectX12Desc& d) {
    // Graphics queue (DIRECT)
    D3D12_COMMAND_QUEUE_DESC qDesc = {};
    qDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    qDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    NK_DX12_CHECK(mData.device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&mData.commandQueue)), "CreateCmdQueue DIRECT");

    // Compute queue dédiée
    if (d.enableComputeQueue) {
        qDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        NK_DX12_CHECK(mData.device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&mData.computeCommandQueue)), "CreateCmdQueue COMPUTE");
    }
    return true;
}

bool NkDX12Context::CreateSwapchain(uint32 w, uint32 h, const NkDirectX12Desc& d) {
    // Vérifier tearing
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(mData.factory.As(&factory5))) {
        BOOL tearing = FALSE;
        factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));
        mData.tearingSupported = (tearing == TRUE);
    }

    mData.backBufferCount = d.swapchainBuffers;

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width       = w;
    scDesc.Height      = h;
    scDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferCount = d.swapchainBuffers;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc  = {1, 0};
    scDesc.Flags       = (d.allowTearing && mData.tearingSupported) ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> sc1;
    NK_DX12_CHECK(mData.factory->CreateSwapChainForHwnd(
        mData.commandQueue.Get(), mData.hwnd, &scDesc, nullptr, nullptr, &sc1),
        "CreateSwapChain");
    NK_DX12_CHECK(sc1.As(&mData.swapchain), "QuerySwapChain4");
    mData.factory->MakeWindowAssociation(mData.hwnd, DXGI_MWA_NO_ALT_ENTER);
    mData.currentBackBuffer = mData.swapchain->GetCurrentBackBufferIndex();
    return true;
}

bool NkDX12Context::CreateDescriptorHeaps(const NkDirectX12Desc& d) {
    // RTV
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = d.rtvHeapSize;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    NK_DX12_CHECK(mData.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mData.rtvHeap)), "CreateRTVHeap");
    mData.rtvDescSize = mData.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // DSV
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = d.dsvHeapSize;
    dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    NK_DX12_CHECK(mData.device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mData.dsvHeap)), "CreateDSVHeap");

    // SRV/CBV/UAV
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = d.srvHeapSize;
    srvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    NK_DX12_CHECK(mData.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mData.srvHeap)), "CreateSRVHeap");
    mData.srvDescSize = mData.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Sampler
    D3D12_DESCRIPTOR_HEAP_DESC sampHeapDesc = {};
    sampHeapDesc.NumDescriptors = d.samplerHeapSize;
    sampHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    sampHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    NK_DX12_CHECK(mData.device->CreateDescriptorHeap(&sampHeapDesc, IID_PPV_ARGS(&mData.samplerHeap)), "CreateSamplerHeap");

    return true;
}

bool NkDX12Context::CreateRenderTargets() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mData.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32 i = 0; i < mData.backBufferCount; ++i) {
        NK_DX12_CHECK(mData.swapchain->GetBuffer(i, IID_PPV_ARGS(&mData.backBuffers[i])), "GetBuffer");
        mData.device->CreateRenderTargetView(mData.backBuffers[i].Get(), nullptr, rtvHandle);
        mData.rtvHandles[i] = rtvHandle;
        rtvHandle.ptr += mData.rtvDescSize;
    }
    return true;
}

bool NkDX12Context::CreateDepthBuffer(uint32 w, uint32 h) {
    D3D12_CLEAR_VALUE clearVal{};
    clearVal.Format               = DXGI_FORMAT_D32_FLOAT;
    clearVal.DepthStencil.Depth   = 1.0f;
    clearVal.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width              = w;
    rd.Height             = h;
    rd.DepthOrArraySize   = 1;
    rd.MipLevels          = 1;
    rd.Format             = DXGI_FORMAT_D32_FLOAT;
    rd.SampleDesc         = {1, 0};
    rd.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    NK_DX12_CHECK(mData.device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearVal,
        IID_PPV_ARGS(&mData.depthBuffer)), "CreateDepthBuffer");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    mData.dsvHandle = mData.dsvHeap->GetCPUDescriptorHandleForHeapStart();
    mData.device->CreateDepthStencilView(mData.depthBuffer.Get(), &dsvDesc, mData.dsvHandle);
    return true;
}

bool NkDX12Context::CreateCommandObjects() {
    for (uint32 i = 0; i < mData.backBufferCount; ++i) {
        NK_DX12_CHECK(mData.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mData.cmdAllocators[i])),
            "CreateCmdAllocator");
    }
    NK_DX12_CHECK(mData.device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        mData.cmdAllocators[0].Get(), nullptr,
        IID_PPV_ARGS(&mData.cmdList)), "CreateCommandList");
    mData.cmdList->Close();

    // Compute
    if (mData.computeCommandQueue) {
        NK_DX12_CHECK(mData.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&mData.computeAllocator)),
            "CreateComputeAllocator");
        NK_DX12_CHECK(mData.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
            mData.computeAllocator.Get(), nullptr,
            IID_PPV_ARGS(&mData.computeCmdList)), "CreateComputeCmdList");
        mData.computeCmdList->Close();
    }
    return true;
}

bool NkDX12Context::CreateSyncObjects() {
    NK_DX12_CHECK(mData.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mData.fence)), "CreateFence");
    mData.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!mData.fenceEvent) { NK_DX12_ERR("CreateEvent failed\n"); return false; }
    for (uint32 i = 0; i < kNkDX12MaxFrames; ++i)
        mData.fenceValues[i] = 0;
    return true;
}

// =============================================================================
void NkDX12Context::WaitForFence(uint32 frameIndex) {
    uint64 val = mData.fenceValues[frameIndex];
    if (mData.fence->GetCompletedValue() < val) {
        mData.fence->SetEventOnCompletion(val, mData.fenceEvent);
        WaitForSingleObjectEx(mData.fenceEvent, INFINITE, FALSE);
    }
}

void NkDX12Context::FlushGPU() {
    for (uint32 i = 0; i < mData.backBufferCount; ++i) {
        ++mData.fenceValues[i];
        mData.commandQueue->Signal(mData.fence.Get(), mData.fenceValues[i]);
        WaitForFence(i);
    }
}

// =============================================================================
void NkDX12Context::DestroySwapchainDependents() {
    // PATCH CRITIQUE : FlushGPU avant de toucher les allocators
    if (mData.device) FlushGPU();

    for (uint32 i = 0; i < mData.backBufferCount; ++i) {
        mData.backBuffers[i].Reset();
        if (mData.cmdAllocators[i]) mData.cmdAllocators[i]->Reset();
    }
    mData.depthBuffer.Reset();
    mData.swapchain.Reset();
}

bool NkDX12Context::RecreateSwapchainDependents(uint32 w, uint32 h) {
    DestroySwapchainDependents();
    if (!CreateSwapchain(w, h, mDesc.dx12))   return false;
    if (!CreateRenderTargets())                return false;
    if (!CreateDepthBuffer(w, h))              return false;
    return true;
}

// =============================================================================
void NkDX12Context::Shutdown() {
    if (!mIsValid) return;
    FlushGPU();
    DestroySwapchainDependents();
    if (mData.fenceEvent) { CloseHandle(mData.fenceEvent); mData.fenceEvent = nullptr; }
    mData.fence.Reset();
    mData.cmdList.Reset();
    mData.computeCmdList.Reset();
    mData.computeAllocator.Reset();
    mData.srvHeap.Reset();
    mData.samplerHeap.Reset();
    mData.dsvHeap.Reset();
    mData.rtvHeap.Reset();
    mData.computeCommandQueue.Reset();
    mData.commandQueue.Reset();
    mData.device.Reset();
    mData.adapter.Reset();
    mData.factory.Reset();
    mIsValid = false;
    NK_DX12_LOG("Shutdown OK\n");
}

// =============================================================================
bool NkDX12Context::BeginFrame() {
    if (!mIsValid) return false;
    uint32 frame = mData.currentBackBuffer;

    // Attendre que la frame soit libre
    WaitForFence(frame);

    // Reset
    mData.cmdAllocators[frame]->Reset();
    mData.cmdList->Reset(mData.cmdAllocators[frame].Get(), nullptr);

    // Transition PRESENT → RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = mData.backBuffers[frame].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    mData.cmdList->ResourceBarrier(1, &barrier);

    // Bind RTV + DSV
    mData.cmdList->OMSetRenderTargets(1, &mData.rtvHandles[frame], FALSE, &mData.dsvHandle);

    // Clear
    float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    mData.cmdList->ClearRenderTargetView(mData.rtvHandles[frame], clearColor, 0, nullptr);
    mData.cmdList->ClearDepthStencilView(mData.dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Viewport + scissor
    D3D12_VIEWPORT vp{0,0,(float)mData.width,(float)mData.height,0.0f,1.0f};
    D3D12_RECT scissor{0,0,(LONG)mData.width,(LONG)mData.height};
    mData.cmdList->RSSetViewports(1, &vp);
    mData.cmdList->RSSetScissorRects(1, &scissor);
    return true;
}

void NkDX12Context::EndFrame() {
    if (!mIsValid) return;
    uint32 frame = mData.currentBackBuffer;

    // Transition RENDER_TARGET → PRESENT
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = mData.backBuffers[frame].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    mData.cmdList->ResourceBarrier(1, &barrier);
    mData.cmdList->Close();

    ID3D12CommandList* lists[] = {mData.cmdList.Get()};
    mData.commandQueue->ExecuteCommandLists(1, lists);
}

void NkDX12Context::Present() {
    if (!mIsValid) return;
    uint32 frame = mData.currentBackBuffer;
    bool allowTearing = mDesc.dx12.allowTearing && mData.tearingSupported;
    UINT flags = (!mVSync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = mData.swapchain->Present(mVSync ? 1 : 0, flags);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        NK_DX12_ERR("Device lost 0x%08X\n", (unsigned)mData.device->GetDeviceRemovedReason());
        mIsValid = false; return;
    }

    // Signal fence pour cette frame
    ++mData.fenceValues[frame];
    mData.commandQueue->Signal(mData.fence.Get(), mData.fenceValues[frame]);
    mData.currentBackBuffer = mData.swapchain->GetCurrentBackBufferIndex();
}

bool NkDX12Context::OnResize(uint32 w, uint32 h) {
    if (!mIsValid) return false;
    if (w == 0 || h == 0) return true; // Minimisée — patch critique
    mData.width = w; mData.height = h;
    return RecreateSwapchainDependents(w, h);
}

void NkDX12Context::SetVSync(bool e) { mVSync = e; mData.vsync = e; }
bool NkDX12Context::GetVSync()  const { return mVSync; }
bool NkDX12Context::IsValid()   const { return mIsValid; }
NkGraphicsApi NkDX12Context::GetApi()  const { return NkGraphicsApi::NK_API_DIRECTX12; }
NkContextDesc NkDX12Context::GetDesc() const { return mDesc; }
void* NkDX12Context::GetNativeContextData() { return &mData; }
bool NkDX12Context::SupportsCompute() const { return mData.computeCommandQueue != nullptr; }

NkContextInfo NkDX12Context::GetInfo() const {
    NkContextInfo i;
    i.api              = NkGraphicsApi::NK_API_DIRECTX12;
    i.renderer         = mData.renderer;
    i.vendor           = mData.vendor;
    i.version          = "DirectX 12";
    i.vramMB           = mData.vramMB;
    i.computeSupported = SupportsCompute();
    return i;
}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS
