// =============================================================================
// NkDX11Context.cpp â€” Production Ready
// Patches : device lost (DXGI_ERROR_DEVICE_REMOVED), OnResize 0x0,
//           destructor auto-shutdown, compute via ID3D11ComputeShader
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKContext/Core/NkGpuPolicy.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include "NkDXContext.h"
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define NK_DX11_LOG(...) logger.Infof("[NkDX11] " __VA_ARGS__)
#define NK_DX11_ERR(...) logger.Errorf("[NkDX11] " __VA_ARGS__)
#define NK_DX11_CHECK(hr, msg) do { if(FAILED(hr)){ NK_DX11_ERR(msg " hr=0x%08X\n",(unsigned)(hr)); return false; } } while(0)

namespace nkentseu {
namespace {

    static DXGI_GPU_PREFERENCE ToDxgiPreference(NkGpuPreference preference) {
        switch (preference) {
            case NkGpuPreference::NK_LOW_POWER:        return DXGI_GPU_PREFERENCE_MINIMUM_POWER;
            case NkGpuPreference::NK_HIGH_PERFORMANCE: return DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
            case NkGpuPreference::NK_DEFAULT:
            default: return DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
        }
    }

    static bool PickDx11Adapter(
        IDXGIFactory6* factory,
        const NkContextDesc& contextDesc,
        const NkDirectX11Desc& dx11Desc,
        ComPtr<IDXGIAdapter1>& outAdapter,
        DXGI_ADAPTER_DESC1& outDesc,
        uint32& outIndex)
    {
        outAdapter.Reset();
        outDesc = {};
        outIndex = UINT32_MAX;
        if (!factory) return false;

        const uint32 explicitIndex =
            (contextDesc.gpu.adapterIndex >= 0)
                ? static_cast<uint32>(contextDesc.gpu.adapterIndex)
                : dx11Desc.preferredAdapter;

        const bool allowSoftware = contextDesc.gpu.allowSoftwareAdapter;
        const NkGpuVendor vendorPref = contextDesc.gpu.vendorPreference;

        auto tryPick = [&](IDXGIAdapter1* a1, uint32 index) -> bool {
            if (!a1) return false;
            DXGI_ADAPTER_DESC1 d1{};
            if (FAILED(a1->GetDesc1(&d1))) return false;
            if (!allowSoftware && (d1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) return false;
            if (!NkGpuPolicy::MatchesVendorPciId(d1.VendorId, vendorPref)) return false;
            outAdapter = a1;
            outDesc = d1;
            outIndex = index;
            return true;
        };

        if (explicitIndex != UINT32_MAX) {
            ComPtr<IDXGIAdapter1> byIndex;
            if (factory->EnumAdapters1(explicitIndex, &byIndex) == DXGI_ERROR_NOT_FOUND) {
                NK_DX11_LOG("Requested adapter #%u not found; fallback to policy\n", explicitIndex);
            } else if (tryPick(byIndex.Get(), explicitIndex)) {
                return true;
            } else {
                NK_DX11_LOG("Requested adapter #%u rejected by filters; fallback to policy\n", explicitIndex);
            }
        }

        const DXGI_GPU_PREFERENCE pref = ToDxgiPreference(contextDesc.gpu.preference);
        ComPtr<IDXGIAdapter1> byPref;
        for (UINT i = 0;
             factory->EnumAdapterByGpuPreference(i, pref, IID_PPV_ARGS(&byPref)) != DXGI_ERROR_NOT_FOUND;
             ++i)
        {
            if (tryPick(byPref.Get(), i)) return true;
            byPref.Reset();
        }

        ComPtr<IDXGIAdapter1> byEnum;
        for (UINT i = 0; factory->EnumAdapters1(i, &byEnum) != DXGI_ERROR_NOT_FOUND; ++i) {
            if (tryPick(byEnum.Get(), i)) return true;
            byEnum.Reset();
        }

        if (vendorPref != NkGpuVendor::NK_ANY) {
            NK_DX11_LOG("No adapter matched vendor=%s; retrying without vendor filter\n",
                        NkGpuPolicy::VendorName(vendorPref));
            NkContextDesc fallbackDesc = contextDesc;
            fallbackDesc.gpu.vendorPreference = NkGpuVendor::NK_ANY;
            return PickDx11Adapter(factory, fallbackDesc, dx11Desc, outAdapter, outDesc, outIndex);
        }
        return false;
    }

} // namespace

// =============================================================================
NkDX11Context::~NkDX11Context() { if (mIsValid) Shutdown(); }

bool NkDX11Context::Initialize(const NkWindow& window, const NkContextDesc& desc) {
    if (mIsValid) { NK_DX11_ERR("Already initialized\n"); return false; }
    mDesc = desc;
    const NkSurfaceDesc surf = window.GetSurfaceDesc();
    if (!surf.IsValid()) { NK_DX11_ERR("Invalid NkSurfaceDesc\n"); return false; }

    mData.hwnd   = static_cast<HWND>(surf.hwnd);
    mData.width  = surf.width;
    mData.height = surf.height;
    mVSync       = desc.dx11.vsync;

    if (!CreateDeviceAndSwapchain(desc, mData.hwnd)) return false;
    if (!CreateRenderTargets()) return false;

    mIsValid = true;
    NK_DX11_LOG("Ready â€” %s | VRAM %u MB\n", mData.renderer, mData.vramMB);
    return true;
}

// =============================================================================
bool NkDX11Context::CreateDeviceAndSwapchain(const NkContextDesc& d, HWND hwnd) {
    const NkDirectX11Desc& dx11 = d.dx11;

    UINT flags = 0;
    if (dx11.debugDevice) flags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
    };
    D3D_FEATURE_LEVEL chosenLevel;

    ComPtr<ID3D11Device>        dev;
    ComPtr<ID3D11DeviceContext> ctx;

    ComPtr<IDXGIFactory6> pickFactory;
    CreateDXGIFactory2(dx11.debugDevice ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&pickFactory));
    ComPtr<IDXGIAdapter1> selectedAdapter;
    DXGI_ADAPTER_DESC1 selectedDesc{};
    uint32 selectedIndex = UINT32_MAX;
    PickDx11Adapter(pickFactory.Get(), d, dx11, selectedAdapter, selectedDesc, selectedIndex);

    HRESULT hr = D3D11CreateDevice(
        selectedAdapter.Get(),
        selectedAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags, featureLevels, ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, &dev, &chosenLevel, &ctx);

    if (FAILED(hr) && d.gpu.allowSoftwareAdapter) {
        // Fallback WARP (software)
        NK_DX11_LOG("Hardware device failed, trying WARP\n");
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            flags, featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, &dev, &chosenLevel, &ctx);
    }
    NK_DX11_CHECK(hr, "D3D11CreateDevice");

    NK_DX11_CHECK(dev.As(&mData.device),   "QueryInterface Device1");
    NK_DX11_CHECK(ctx.As(&mData.context),  "QueryInterface Context1");

    // VRAM via DXGI adapter
    ComPtr<IDXGIDevice>  dxgiDev;
    ComPtr<IDXGIAdapter> dxgiAdp;
    mData.device.As(&dxgiDev);
    dxgiDev->GetAdapter(&dxgiAdp);
    DXGI_ADAPTER_DESC adpDesc = {};
    dxgiAdp->GetDesc(&adpDesc);
    mData.vramMB = (uint32)(adpDesc.DedicatedVideoMemory / (1024*1024));
    if (selectedAdapter) {
        NK_DX11_LOG("Using adapter #%u vendor=0x%04X device=0x%04X\n",
                    selectedIndex,
                    (unsigned)selectedDesc.VendorId,
                    (unsigned)selectedDesc.DeviceId);
    }

    // Swapchain
    ComPtr<IDXGIFactory2> factory;
    dxgiAdp->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width              = mData.width;
    scDesc.Height             = mData.height;
    scDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc.Count   = dx11.msaaSamples > 1 ? dx11.msaaSamples : 1;
    scDesc.SampleDesc.Quality = dx11.msaaSamples > 1 ? dx11.msaaQuality : 0;
    scDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount        = dx11.swapchainBuffers;
    scDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.Flags              = dx11.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    NK_DX11_CHECK(
        factory->CreateSwapChainForHwnd(
            mData.device.Get(), hwnd, &scDesc, nullptr, nullptr,
            &mData.swapchain),
        "CreateSwapChainForHwnd");

    // DÃ©sactiver Alt+Enter fullscreen automatique
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    NK_DX11_LOG("Device+Swapchain OK (feature level %04X, VRAM %u MB)\n",
                (unsigned)chosenLevel, mData.vramMB);
    return true;
}

bool NkDX11Context::CreateRenderTargets() {
    // RTV
    ComPtr<ID3D11Texture2D> backBuf;
    NK_DX11_CHECK(mData.swapchain->GetBuffer(0, IID_PPV_ARGS(&backBuf)), "GetBuffer");
    NK_DX11_CHECK(mData.device->CreateRenderTargetView(backBuf.Get(), nullptr, &mData.rtv), "CreateRTV");

    // DSV
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width     = mData.width;
    depthDesc.Height    = mData.height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count   = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage     = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    NK_DX11_CHECK(mData.device->CreateTexture2D(&depthDesc, nullptr, &mData.depthTex), "CreateDepthTex");
    NK_DX11_CHECK(mData.device->CreateDepthStencilView(mData.depthTex.Get(), nullptr, &mData.dsv), "CreateDSV");
    return true;
}

void NkDX11Context::DestroyRenderTargets() {
    if (mData.context) mData.context->OMSetRenderTargets(0, nullptr, nullptr);
    mData.rtv.Reset();
    mData.dsv.Reset();
    mData.depthTex.Reset();
}

void NkDX11Context::HandleDeviceLost() {
    NK_DX11_ERR("Device lost â€” attempting recovery\n");
    DestroyRenderTargets();
    mData.swapchain.Reset();
    mData.context.Reset();
    mData.device.Reset();
    // RecrÃ©er â€” en pratique on notifie l'application qui dÃ©cide de rÃ©initialiser
    // Pour l'instant on marque comme invalide
    mIsValid = false;
}

void NkDX11Context::Shutdown() {
    if (!mIsValid) return;
    if (mData.context) mData.context->ClearState();
    DestroyRenderTargets();
    mData.swapchain.Reset();
    mData.context.Reset();
    mData.device.Reset();
    mIsValid = false;
    NK_DX11_LOG("Shutdown OK\n");
}

// =============================================================================
bool NkDX11Context::BeginFrame() {
    if (!mIsValid) return false;
    ID3D11RenderTargetView* rtvs[] = {mData.rtv.Get()};
    mData.context->OMSetRenderTargets(1, rtvs, mData.dsv.Get());
    float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    mData.context->ClearRenderTargetView(mData.rtv.Get(), clearColor);
    mData.context->ClearDepthStencilView(mData.dsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    D3D11_VIEWPORT vp{};
    vp.Width=(float)mData.width; vp.Height=(float)mData.height;
    vp.MinDepth=0.0f; vp.MaxDepth=1.0f;
    mData.context->RSSetViewports(1, &vp);
    return true;
}

void NkDX11Context::EndFrame() { /* flush optionnel */ }

void NkDX11Context::Present() {
    if (!mIsValid) return;
    bool allowTearing = mDesc.dx11.allowTearing;
    UINT flags = (!mVSync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = mData.swapchain->Present(mVSync ? 1 : 0, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        HandleDeviceLost();
    else if (FAILED(hr))
        NK_DX11_ERR("Present failed 0x%08X\n", (unsigned)hr);
}

bool NkDX11Context::OnResize(uint32 w, uint32 h) {
    if (!mIsValid) return false;
    if (w == 0 || h == 0) return true; // MinimisÃ©e â€” skip
    mData.width = w; mData.height = h;
    mData.context->ClearState();
    DestroyRenderTargets();
    HRESULT hr = mData.swapchain->ResizeBuffers(0, w, h,
        DXGI_FORMAT_UNKNOWN,
        mDesc.dx11.allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        { HandleDeviceLost(); return false; }
    NK_DX11_CHECK(hr, "ResizeBuffers");
    return CreateRenderTargets();
}

void NkDX11Context::SetVSync(bool e)  { mVSync = e; }
bool NkDX11Context::GetVSync()  const { return mVSync; }
bool NkDX11Context::IsValid()   const { return mIsValid; }
NkGraphicsApi NkDX11Context::GetApi()  const { return NkGraphicsApi::NK_GFX_API_D3D11; }
NkContextDesc NkDX11Context::GetDesc() const { return mDesc; }
void* NkDX11Context::GetNativeContextData() { return &mData; }
bool NkDX11Context::SupportsCompute() const { return true; } // CS_5_0 dispo sur DX11+

NkContextInfo NkDX11Context::GetInfo() const {
    NkContextInfo i;
    i.api              = NkGraphicsApi::NK_GFX_API_D3D11;
    i.renderer         = mData.renderer;
    i.vendor           = mData.vendor;
    i.version          = "DirectX 11.1";
    i.vramMB           = mData.vramMB;
    i.computeSupported = true;
    i.windowWidth      = mData.width;
    i.windowHeight     = mData.height;
    return i;
}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WINDOWS

