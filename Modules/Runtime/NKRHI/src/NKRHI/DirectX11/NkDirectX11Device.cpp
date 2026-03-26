// =============================================================================
// NkRHI_Device_DX11.cpp — Backend DirectX 11.1
// =============================================================================
#ifdef NK_RHI_DX11_ENABLED
#include "NkDirectX11Device.h"
#include "NkDirectX11CommandBuffer.h"
#include "NKRHI/Core/NkGpuPolicy.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cwchar>
#include <algorithm>

#define NK_DX11_LOG(...)  logger_src.Infof("[NkRHI_DX11] " __VA_ARGS__)
#define NK_DX11_ERR(...)  logger_src.Infof("[NkRHI_DX11][ERR] " __VA_ARGS__)
#define NK_DX11_CHECK(hr, msg) do { if(FAILED(hr)) { NK_DX11_ERR(msg " (hr=0x%X)\n",(unsigned)(hr)); } } while(0)
#define NK_DX11_SAFE(p) if(p){(p)->Release();(p)=nullptr;}

namespace nkentseu {
    namespace {

        static int ScoreDxgiAdapter(const DXGI_ADAPTER_DESC1& desc, NkGpuPreference preference) {
            int score = static_cast<int>(desc.DedicatedVideoMemory >> 20);
            const bool isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            if (isSoftware) score -= 10000;

            const wchar_t* name = desc.Description;
            const bool likelyIntegrated =
                (wcsstr(name, L"Intel") != nullptr) ||
                (wcsstr(name, L"UHD") != nullptr) ||
                (wcsstr(name, L"Iris") != nullptr);

            switch (preference) {
                case NkGpuPreference::NK_HIGH_PERFORMANCE:
                    score += likelyIntegrated ? 200 : 1500;
                    break;
                case NkGpuPreference::NK_LOW_POWER:
                    score += likelyIntegrated ? 1500 : 200;
                    break;
                case NkGpuPreference::NK_DEFAULT:
                default:
                    score += likelyIntegrated ? 700 : 1000;
                    break;
            }
            return score;
        }

        static DXGI_FORMAT ToDx11DepthTextureFormat(const NkTextureDesc& desc, bool forTextureResource) {
            if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
                return forTextureResource ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_D32_FLOAT;
            }
            return forTextureResource ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_D24_UNORM_S8_UINT;
        }

        static DXGI_FORMAT ToDx11DepthSrvFormat(const NkTextureDesc& desc) {
            if (desc.format == NkGPUFormat::NK_D32_FLOAT) {
                return DXGI_FORMAT_R32_FLOAT;
            }
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }

    } // namespace

    NkDirectX11Device::~NkDirectX11Device() { if (mIsValid) Shutdown(); }

    // =============================================================================
    bool NkDirectX11Device::Initialize(const NkDeviceInitInfo& init) {
        mInit = init;
        NkGpuPolicy::ApplyPreContext(mInit.context);

        const NkDirectX11Desc& dxCfg = mInit.context.dx11;
        const NkGpuSelectionDesc& gpuCfg = mInit.context.gpu;
        mVsync = dxCfg.vsync;
        mAllowTearing = dxCfg.allowTearing && !mVsync;
        mSwapchainBufferCount = math::NkMax(2u, dxCfg.swapchainBuffers);
        mMsaaSamples = math::NkMax(1u, dxCfg.msaaSamples);
        mMsaaQuality = dxCfg.msaaQuality;
        mMinFeatureLevel = dxCfg.minFeatureLevel == 0
            ? D3D_FEATURE_LEVEL_11_0
            : static_cast<D3D_FEATURE_LEVEL>(dxCfg.minFeatureLevel);
        if (mMinFeatureLevel != D3D_FEATURE_LEVEL_11_1 &&
            mMinFeatureLevel != D3D_FEATURE_LEVEL_11_0) {
            mMinFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        }

        mHwnd = nullptr;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
        mHwnd = init.surface.hwnd;
    #endif
        if (!mHwnd) {
            NK_DX11_ERR("HWND manquant dans NkDeviceInitInfo.surface\n");
            return false;
        }

        mWidth  = NkDeviceInitWidth(init);
        mHeight = NkDeviceInitHeight(init);
        if (mWidth == 0)  mWidth = 1280;
        if (mHeight == 0) mHeight = 720;

        // Le device RHI cree son contexte DX11 depuis la surface.
        UINT flags = dxCfg.debugDevice ? D3D11_CREATE_DEVICE_DEBUG : 0;

        D3D_FEATURE_LEVEL requestedLevels[2] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };
        UINT requestedLevelCount = 2;
        if (mMinFeatureLevel == D3D_FEATURE_LEVEL_11_1) {
            requestedLevels[0] = D3D_FEATURE_LEVEL_11_1;
            requestedLevelCount = 1;
        }
        D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_11_0;

        IDXGIAdapter1* selectedAdapter = nullptr;
        IDXGIFactory1* enumFactory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&enumFactory)) && enumFactory) {
            struct Candidate {
                IDXGIAdapter1* adapter = nullptr;
                UINT index = 0;
                DXGI_ADAPTER_DESC1 desc{};
                int score = 0;
            };
            NkVector<Candidate> candidates;
            const NkGpuVendor requestedVendor = gpuCfg.vendorPreference;
            const bool strictVendor = requestedVendor != NkGpuVendor::NK_ANY;
            const uint32 preferredAdapterIndex =
                (dxCfg.preferredAdapter != UINT32_MAX)
                    ? dxCfg.preferredAdapter
                    : ((gpuCfg.adapterIndex >= 0) ? static_cast<uint32>(gpuCfg.adapterIndex) : UINT32_MAX);

            for (UINT i = 0;; ++i) {
                IDXGIAdapter1* cand = nullptr;
                if (enumFactory->EnumAdapters1(i, &cand) == DXGI_ERROR_NOT_FOUND) {
                    break;
                }

                DXGI_ADAPTER_DESC1 desc{};
                cand->GetDesc1(&desc);

                if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && !gpuCfg.allowSoftwareAdapter) {
                    cand->Release();
                    continue;
                }
                if (strictVendor &&
                    !NkGpuPolicy::MatchesVendorPciId(static_cast<uint32>(desc.VendorId), requestedVendor)) {
                    cand->Release();
                    continue;
                }

                Candidate c{};
                c.adapter = cand;
                c.index = i;
                c.desc = desc;
                c.score = ScoreDxgiAdapter(desc, gpuCfg.preference);
                candidates.PushBack(c);
            }

            if (!candidates.Empty()) {
                if (preferredAdapterIndex != UINT32_MAX) {
                    for (uint32 i = 0; i < candidates.Size(); ++i) {
                        if (candidates[i].index == preferredAdapterIndex) {
                            selectedAdapter = candidates[i].adapter;
                            break;
                        }
                    }
                }

                if (!selectedAdapter) {
                    int bestScore = -1000000;
                    for (uint32 i = 0; i < candidates.Size(); ++i) {
                        if (candidates[i].score > bestScore) {
                            bestScore = candidates[i].score;
                            selectedAdapter = candidates[i].adapter;
                        }
                    }
                }

                for (uint32 i = 0; i < candidates.Size(); ++i) {
                    if (candidates[i].adapter != selectedAdapter) {
                        candidates[i].adapter->Release();
                    }
                }
            }
        }
        NK_DX11_SAFE(enumFactory);

        ID3D11Device* baseDevice = nullptr;
        ID3D11DeviceContext* baseContext = nullptr;

        HRESULT hr = D3D11CreateDevice(
            selectedAdapter,
            selectedAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            requestedLevels,
            requestedLevelCount,
            D3D11_SDK_VERSION,
            &baseDevice,
            &createdLevel,
            &baseContext);

        if (FAILED(hr) && !selectedAdapter) {
            hr = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_WARP,
                nullptr,
                flags,
                requestedLevels,
                requestedLevelCount,
                D3D11_SDK_VERSION,
                &baseDevice,
                &createdLevel,
                &baseContext);
        }
        NK_DX11_SAFE(selectedAdapter);
        (void)createdLevel;
        if (FAILED(hr) || !baseDevice || !baseContext) {
            NK_DX11_ERR("D3D11CreateDevice failed (hr=0x%X)\n", (unsigned)hr);
            NK_DX11_SAFE(baseContext);
            NK_DX11_SAFE(baseDevice);
            return false;
        }

        hr = baseDevice->QueryInterface(__uuidof(ID3D11Device1), (void**)&mDevice);
        if (FAILED(hr) || !mDevice) {
            NK_DX11_ERR("QueryInterface(ID3D11Device1) failed (hr=0x%X)\n", (unsigned)hr);
            NK_DX11_SAFE(baseContext);
            NK_DX11_SAFE(baseDevice);
            return false;
        }
        hr = baseContext->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&mContext);
        NK_DX11_SAFE(baseContext);
        NK_DX11_SAFE(baseDevice);
        if (FAILED(hr) || !mContext) {
            NK_DX11_ERR("QueryInterface(ID3D11DeviceContext1) failed (hr=0x%X)\n", (unsigned)hr);
            NK_DX11_SAFE(mDevice);
            return false;
        }

        // Créer la factory DXGI
        CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&mFactory);
        CreateSwapchain(mWidth, mHeight);
        QueryCaps();

        mIsValid = true;
        NK_DX11_LOG("Initialisé (%u×%u)\n", mWidth, mHeight);
        return true;
    }

    void NkDirectX11Device::CreateSwapchain(uint32 w, uint32 h) {
        DXGI_SWAP_CHAIN_DESC1 scd{};
        scd.Width=w; scd.Height=h;
        scd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
        scd.SampleDesc={1,0};
        scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount=mSwapchainBufferCount;
        scd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.Flags=mAllowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

        IDXGIDevice2* dxgiDev=nullptr;
        mDevice->QueryInterface(__uuidof(IDXGIDevice2),(void**)&dxgiDev);
        IDXGIAdapter* adapter=nullptr;
        if (dxgiDev) dxgiDev->GetAdapter(&adapter);
        IDXGIFactory2* factory=nullptr;
        if (adapter) adapter->GetParent(__uuidof(IDXGIFactory2),(void**)&factory);
        if (factory) {
            HRESULT hr = factory->CreateSwapChainForHwnd(mDevice,mHwnd,&scd,nullptr,nullptr,&mSwapchain);
            NK_DX11_CHECK(hr, "CreateSwapChainForHwnd");
        }
        NK_DX11_SAFE(factory); NK_DX11_SAFE(adapter); NK_DX11_SAFE(dxgiDev);
        if (!mSwapchain) {
            NK_DX11_ERR("Swapchain creation failed\n");
            return;
        }

        // Back buffer texture
        ID3D11Texture2D* bb=nullptr;
        HRESULT hr = mSwapchain->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&bb);
        NK_DX11_CHECK(hr, "IDXGISwapChain::GetBuffer");
        if (!bb) return;
        uint64 colorId=NextId();
        NkDX11Texture swt; swt.tex=bb; swt.isSwapchain=true;
        swt.desc=NkTextureDesc::RenderTarget(w,h,NkGPUFormat::NK_BGRA8_UNORM);
        mDevice->CreateRenderTargetView(bb,nullptr,&swt.rtv);
        mTextures[colorId]=swt;
        NkTextureHandle colorH; colorH.id=colorId;

        // Depth
        NkTextureDesc dd=NkTextureDesc::DepthStencil(w,h);
        auto depthH=CreateTexture(dd);

        // Render pass
        NkRenderPassDesc rpd;
        rpd.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_BGRA8_UNORM))
        .SetDepth(NkAttachmentDesc::Depth());
        mSwapchainRP=CreateRenderPass(rpd);

        NkFramebufferDesc fbd;
        fbd.renderPass=mSwapchainRP;
        fbd.colorAttachments.PushBack(colorH);
        fbd.depthAttachment=depthH;
        fbd.width=w; fbd.height=h;
        mSwapchainFB=CreateFramebuffer(fbd);
    }

    void NkDirectX11Device::DestroySwapchain() {
        mContext->OMSetRenderTargets(0,nullptr,nullptr);
        DestroyFramebuffer(mSwapchainFB);
        DestroyRenderPass(mSwapchainRP);
        NK_DX11_SAFE(mSwapchain);
    }

    void NkDirectX11Device::ResizeSwapchain(uint32 w, uint32 h) {
        WaitIdle();
        DestroySwapchain();
        CreateSwapchain(w,h);
    }

    void NkDirectX11Device::Shutdown() {
        WaitIdle();
        DestroySwapchain();
        for (auto& [id,b] : mBuffers)   NK_DX11_SAFE(b.buf);
        for (auto& [id,t] : mTextures)  if (!t.isSwapchain) {
            NK_DX11_SAFE(t.tex); NK_DX11_SAFE(t.srv); NK_DX11_SAFE(t.rtv);
            NK_DX11_SAFE(t.dsv); NK_DX11_SAFE(t.uav);
        }
        for (auto& [id,s] : mSamplers)  NK_DX11_SAFE(s.ss);
        for (auto& [id,sh]:mShaders) {
            NK_DX11_SAFE(sh.vs); NK_DX11_SAFE(sh.ps); NK_DX11_SAFE(sh.cs);
            NK_DX11_SAFE(sh.gs); NK_DX11_SAFE(sh.vsBlob);
        }
        for (auto& [id,p] : mPipelines) {
            NK_DX11_SAFE(p.vs); NK_DX11_SAFE(p.ps); NK_DX11_SAFE(p.cs);
            NK_DX11_SAFE(p.il); NK_DX11_SAFE(p.rs); NK_DX11_SAFE(p.dss); NK_DX11_SAFE(p.bs);
        }
        NK_DX11_SAFE(mFactory);
        NK_DX11_SAFE(mContext);
        NK_DX11_SAFE(mDevice);
        mIsValid=false;
        NK_DX11_LOG("Shutdown\n");
    }

    // =============================================================================
    // Buffers
    // =============================================================================
    NkBufferHandle NkDirectX11Device::CreateBuffer(const NkBufferDesc& desc) {
        threading::NkScopedLock lock(mMutex);
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth=(UINT)desc.sizeBytes;
        switch(desc.usage) {
            case NkResourceUsage::NK_UPLOAD:   bd.Usage=D3D11_USAGE_DYNAMIC; bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE; break;
            case NkResourceUsage::NK_READBACK: bd.Usage=D3D11_USAGE_STAGING; bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ; break;
            case NkResourceUsage::NK_IMMUTABLE:bd.Usage=D3D11_USAGE_IMMUTABLE; break;
            default:                        bd.Usage=D3D11_USAGE_DEFAULT; break;
        }
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_VERTEX_BUFFER))   bd.BindFlags|=D3D11_BIND_VERTEX_BUFFER;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_INDEX_BUFFER))    bd.BindFlags|=D3D11_BIND_INDEX_BUFFER;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_UNIFORM_BUFFER))  bd.BindFlags|=D3D11_BIND_CONSTANT_BUFFER;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_STORAGE_BUFFER))  bd.BindFlags|=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_SHADER_RESOURCE)) bd.BindFlags|=D3D11_BIND_SHADER_RESOURCE;

        // Type flags selon le type de buffer
        if (desc.type==NkBufferType::NK_VERTEX)  bd.BindFlags|=D3D11_BIND_VERTEX_BUFFER;
        if (desc.type==NkBufferType::NK_INDEX)   bd.BindFlags|=D3D11_BIND_INDEX_BUFFER;
        if (desc.type==NkBufferType::NK_UNIFORM) { bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; }
        if (desc.type==NkBufferType::NK_STORAGE) {
            bd.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
            bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            bd.StructureByteStride=4; // float
        }

        D3D11_SUBRESOURCE_DATA* pData=nullptr, data{};
        if (desc.initialData) { data.pSysMem=desc.initialData; pData=&data; }

        ID3D11Buffer* buf=nullptr;
        HRESULT hr=mDevice->CreateBuffer(&bd,pData,&buf);
        NK_DX11_CHECK(hr,"CreateBuffer");

        uint64 hid=NextId(); mBuffers[hid]={buf,desc};
        NkBufferHandle h; h.id=hid; return h;
    }

    void NkDirectX11Device::DestroyBuffer(NkBufferHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mBuffers.Find(h.id); if(!it) return;
        NK_DX11_SAFE(it->buf); mBuffers.Erase(h.id); h.id=0;
    }

    bool NkDirectX11Device::WriteBuffer(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
        auto it=mBuffers.Find(buf.id); if(!it) return false;
        D3D11_MAPPED_SUBRESOURCE ms{};
        if (it->desc.usage==NkResourceUsage::NK_UPLOAD) {
            mContext->Map(it->buf,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
            memcpy((uint8*)ms.pData+off,data,(size_t)sz);
            mContext->Unmap(it->buf,0);
        } else {
            D3D11_BOX box{(UINT)off,0,0,(UINT)(off+sz),1,1};
            mContext->UpdateSubresource(it->buf,0,&box,data,0,0);
        }
        return true;
    }

    bool NkDirectX11Device::WriteBufferAsync(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
        return WriteBuffer(buf,data,sz,off);
    }

    bool NkDirectX11Device::ReadBuffer(NkBufferHandle buf,void* out,uint64 sz,uint64 off) {
        auto it=mBuffers.Find(buf.id); if(!it) return false;
        // Créer un staging buffer temporaire
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth=(UINT)sz; bd.Usage=D3D11_USAGE_STAGING;
        bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Buffer* staging=nullptr;
        mDevice->CreateBuffer(&bd,nullptr,&staging);
        D3D11_BOX box{(UINT)off,0,0,(UINT)(off+sz),1,1};
        mContext->CopySubresourceRegion(staging,0,0,0,0,it->buf,0,&box);
        D3D11_MAPPED_SUBRESOURCE ms{};
        mContext->Map(staging,0,D3D11_MAP_READ,0,&ms);
        memcpy(out,ms.pData,(size_t)sz);
        mContext->Unmap(staging,0);
        staging->Release();
        return true;
    }

    NkMappedMemory NkDirectX11Device::MapBuffer(NkBufferHandle buf,uint64 off,uint64 sz) {
        auto it=mBuffers.Find(buf.id); if(!it) return {};
        D3D11_MAPPED_SUBRESOURCE ms{};
        mContext->Map(it->buf,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
        uint64 mapSz=sz>0?sz:it->desc.sizeBytes-off;
        return {(uint8*)ms.pData+off,mapSz};
    }
    void NkDirectX11Device::UnmapBuffer(NkBufferHandle buf) {
        auto it=mBuffers.Find(buf.id); if(!it) return;
        mContext->Unmap(it->buf,0);
    }

    // =============================================================================
    // Textures
    // =============================================================================
    NkTextureHandle NkDirectX11Device::CreateTexture(const NkTextureDesc& desc) {
        threading::NkScopedLock lock(mMutex);
        const bool isDepth = NkFormatIsDepth(desc.format);
        const bool wantsSrv = NkHasFlag(desc.bindFlags, NkBindFlags::NK_SHADER_RESOURCE);

        DXGI_FORMAT resourceFormat = ToDXGIFormat(desc.format);
        DXGI_FORMAT dsvFormat = ToDx11DepthTextureFormat(desc, false);
        DXGI_FORMAT srvFormat = ToDx11DepthSrvFormat(desc);
        if (isDepth) {
            resourceFormat = ToDx11DepthTextureFormat(desc, wantsSrv);
        }

        D3D11_TEXTURE2D_DESC td{};
        td.Width=desc.width;
        td.Height=desc.height;
        td.MipLevels=desc.mipLevels==0?1:desc.mipLevels;
        td.ArraySize=math::NkMax(1u, desc.arrayLayers);
        td.Format=resourceFormat;
        td.SampleDesc.Count=(UINT)math::NkMax(1u, (uint32)desc.samples);
        td.SampleDesc.Quality=0;
        td.Usage=D3D11_USAGE_DEFAULT;
        if (wantsSrv) td.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
        if (!isDepth && NkHasFlag(desc.bindFlags,NkBindFlags::NK_RENDER_TARGET)) td.BindFlags|=D3D11_BIND_RENDER_TARGET;
        if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_DEPTH_STENCIL)) td.BindFlags|=D3D11_BIND_DEPTH_STENCIL;
        if (!isDepth && NkHasFlag(desc.bindFlags,NkBindFlags::NK_UNORDERED_ACCESS)) td.BindFlags|=D3D11_BIND_UNORDERED_ACCESS;

        if (td.SampleDesc.Count > 1) {
            UINT qualityLevels = 0;
            mDevice->CheckMultisampleQualityLevels(td.Format, td.SampleDesc.Count, &qualityLevels);
            if (qualityLevels == 0) {
                td.SampleDesc.Count = 1;
                td.SampleDesc.Quality = 0;
            } else {
                td.SampleDesc.Quality = (UINT)math::NkMin<uint32>(mMsaaQuality, qualityLevels - 1);
            }
        }

        if (td.MipLevels!=1 && (td.BindFlags&D3D11_BIND_RENDER_TARGET))
            td.MiscFlags|=D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if (desc.type==NkTextureType::NK_CUBE||desc.type==NkTextureType::NK_CUBE_ARRAY)
            td.MiscFlags|=D3D11_RESOURCE_MISC_TEXTURECUBE;

        D3D11_SUBRESOURCE_DATA initData{};
        D3D11_SUBRESOURCE_DATA* pInit=nullptr;
        if (desc.initialData) {
            uint32 bpp=NkFormatBytesPerPixel(desc.format);
            initData.pSysMem=desc.initialData;
            initData.SysMemPitch=desc.rowPitch>0?desc.rowPitch:desc.width*bpp;
            initData.SysMemSlicePitch=initData.SysMemPitch*desc.height;
            pInit=&initData;
        }

        ID3D11Texture2D* tex=nullptr;
        HRESULT hr=mDevice->CreateTexture2D(&td,pInit,&tex);
        NK_DX11_CHECK(hr,"CreateTexture2D");
        if (FAILED(hr) || !tex) {
            return {};
        }

        NkDX11Texture t; t.tex=tex; t.desc=desc;

        if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
            if (NkFormatIsDepth(desc.format)) {
                sd.Format=srvFormat;
            } else {
                sd.Format=ToDXGIFormat(desc.format);
            }
            if (desc.type==NkTextureType::NK_CUBE) {
                sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURECUBE;
                sd.TextureCube.MipLevels=td.MipLevels;
            } else {
                sd.ViewDimension=desc.samples>NkSampleCount::NK_S1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
                sd.Texture2D.MipLevels=td.MipLevels;
            }
            hr = mDevice->CreateShaderResourceView(tex,&sd,&t.srv);
            NK_DX11_CHECK(hr, "CreateShaderResourceView");
        }
        if (td.BindFlags & D3D11_BIND_RENDER_TARGET)
        {
            hr = mDevice->CreateRenderTargetView(tex,nullptr,&t.rtv);
            NK_DX11_CHECK(hr, "CreateRenderTargetView");
        }
        if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
            D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
            dd.Format=dsvFormat;
            dd.ViewDimension=desc.samples>NkSampleCount::NK_S1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
            hr = mDevice->CreateDepthStencilView(tex,&dd,&t.dsv);
            NK_DX11_CHECK(hr, "CreateDepthStencilView");
        }
        if (td.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
        {
            hr = mDevice->CreateUnorderedAccessView(tex,nullptr,&t.uav);
            NK_DX11_CHECK(hr, "CreateUnorderedAccessView");
        }

        uint64 hid=NextId(); mTextures[hid]=t;
        NkTextureHandle h; h.id=hid; return h;
    }

    void NkDirectX11Device::DestroyTexture(NkTextureHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mTextures.Find(h.id); if(!it) return;
        if (!it->isSwapchain) {
            NK_DX11_SAFE(it->tex); NK_DX11_SAFE(it->srv);
            NK_DX11_SAFE(it->rtv); NK_DX11_SAFE(it->dsv);
            NK_DX11_SAFE(it->uav);
        }
        mTextures.Erase(h.id); h.id=0;
    }

    bool NkDirectX11Device::WriteTexture(NkTextureHandle t,const void* p,uint32 rp) {
        auto it=mTextures.Find(t.id); if(!it) return false;
        auto& desc=it->desc;
        uint32 pitch=rp>0?rp:desc.width*NkFormatBytesPerPixel(desc.format);
        mContext->UpdateSubresource(it->tex,0,nullptr,p,pitch,pitch*desc.height);
        return true;
    }
    bool NkDirectX11Device::WriteTextureRegion(NkTextureHandle t,const void* p,
        uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
        uint32 mip,uint32 layer,uint32 rp) {
        auto it=mTextures.Find(t.id); if(!it) return false;
        auto& desc=it->desc;
        uint32 pitch=rp>0?rp:w*NkFormatBytesPerPixel(desc.format);
        D3D11_BOX box{x,y,z,x+w,y+h,z+d2};
        uint32 sub=D3D11CalcSubresource(mip,layer,desc.mipLevels?desc.mipLevels:1);
        mContext->UpdateSubresource(it->tex,sub,&box,p,pitch,pitch*h);
        return true;
    }
    bool NkDirectX11Device::GenerateMipmaps(NkTextureHandle t, NkFilter) {
        auto it=mTextures.Find(t.id); if(!it) return false;
        if (it->srv) mContext->GenerateMips(it->srv);
        return true;
    }

    // =============================================================================
    // Samplers
    // =============================================================================
    NkSamplerHandle NkDirectX11Device::CreateSampler(const NkSamplerDesc& d) {
        threading::NkScopedLock lock(mMutex);
        D3D11_SAMPLER_DESC sd{};
        sd.Filter=ToDX11Filter(d.magFilter,d.minFilter,d.mipFilter,d.compareEnable);
        sd.AddressU=ToDX11Address(d.addressU);
        sd.AddressV=ToDX11Address(d.addressV);
        sd.AddressW=ToDX11Address(d.addressW);
        sd.MipLODBias=d.mipLodBias;
        sd.MaxAnisotropy=(UINT)d.maxAnisotropy;
        sd.ComparisonFunc=d.compareEnable?ToDX11Compare(d.compareOp):D3D11_COMPARISON_NEVER;
        sd.MinLOD=d.minLod; sd.MaxLOD=d.maxLod;
        ID3D11SamplerState* ss=nullptr;
        mDevice->CreateSamplerState(&sd,&ss);
        uint64 hid=NextId(); mSamplers[hid]={ss};
        NkSamplerHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::DestroySampler(NkSamplerHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mSamplers.Find(h.id); if(!it) return;
        NK_DX11_SAFE(it->ss); mSamplers.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Shaders (compilation HLSL runtime avec D3DCompile)
    // =============================================================================
    NkShaderHandle NkDirectX11Device::CreateShader(const NkShaderDesc& desc) {
        threading::NkScopedLock lock(mMutex);
        NkDX11Shader sh;
        for (uint32 i=0;i<desc.stages.Size();i++) {
            auto& s=desc.stages[i];
            const char* src = s.hlslSource;
            if (!src) { NK_DX11_ERR("DX11 shader: manque le source HLSL (stage %d)\n",i); continue; }

            const char* target=nullptr; const char* entry=s.entryPoint?s.entryPoint:"main";
            switch(s.stage) {
                case NkShaderStage::NK_VERTEX:   target="vs_5_0"; break;
                case NkShaderStage::NK_FRAGMENT: target="ps_5_0"; break;
                case NkShaderStage::NK_COMPUTE:  target="cs_5_0"; break;
                case NkShaderStage::NK_GEOMETRY: target="gs_5_0"; break;
                default: continue;
            }

            ID3DBlob* code=nullptr; ID3DBlob* err=nullptr;
            UINT flags=D3DCOMPILE_ENABLE_STRICTNESS;
            #ifdef _DEBUG
            flags|=D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
            #endif
            HRESULT hr=D3DCompile(src,(SIZE_T)strlen(src),nullptr,nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,entry,target,flags,0,&code,&err);
            if (FAILED(hr)) {
                if (err) { NK_DX11_ERR("Shader error: %s\n",(char*)err->GetBufferPointer()); err->Release(); }
                continue;
            }

            switch(s.stage) {
                case NkShaderStage::NK_VERTEX:
                    mDevice->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.vs);
                    sh.vsBlob=code; code=nullptr; // garder le blob pour InputLayout
                    break;
                case NkShaderStage::NK_FRAGMENT:
                    mDevice->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.ps);
                    break;
                case NkShaderStage::NK_COMPUTE:
                    mDevice->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.cs);
                    break;
                case NkShaderStage::NK_GEOMETRY:
                    mDevice->CreateGeometryShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.gs);
                    break;
                default: break;
            }
            if (err)  err->Release();
            if (code) code->Release();
        }
        uint64 hid=NextId(); mShaders[hid]=sh;
        NkShaderHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::DestroyShader(NkShaderHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mShaders.Find(h.id); if(!it) return;
        NK_DX11_SAFE(it->vs); NK_DX11_SAFE(it->ps);
        NK_DX11_SAFE(it->cs); NK_DX11_SAFE(it->gs);
        NK_DX11_SAFE(it->vsBlob);
        mShaders.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Pipelines
    // =============================================================================
    NkPipelineHandle NkDirectX11Device::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
        threading::NkScopedLock lock(mMutex);
        auto sit=mShaders.Find(d.shader.id); if(!sit) return {};
        auto& sh=*sit;

        NkDX11Pipeline p;
        p.vs=sh.vs; p.ps=sh.ps;
        p.topology=ToDX11Topology(d.topology);
        p.isCompute=false;
        for (uint32 i = 0; i < d.vertexLayout.bindings.Size(); ++i) {
            const auto& b = d.vertexLayout.bindings[i];
            if (b.binding < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) {
                p.vertexStrides[b.binding] = b.stride;
            }
        }

        // Input Layout
        if (sh.vsBlob && d.vertexLayout.attributes.Size()>0) {
            NkVector<D3D11_INPUT_ELEMENT_DESC> elems(d.vertexLayout.attributes.Size());
            static const DXGI_FORMAT fmtTable[]={
                DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
                DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
                DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_SNORM,
                DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM,
                DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
                DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32B32A32_SINT,
            };
            for (uint32 i=0;i<d.vertexLayout.attributes.Size();i++) {
                auto& a=d.vertexLayout.attributes[i];
                elems[i].SemanticName=a.semanticName?a.semanticName:"TEXCOORD";
                elems[i].SemanticIndex=a.semanticIdx;
                elems[i].Format=fmtTable[math::NkMin((uint32)a.format,15u)];
                elems[i].InputSlot=a.binding;
                elems[i].AlignedByteOffset=a.offset;
                // Trouver le binding pour savoir si c'est instancé
                bool instanced=false;
                for (uint32 j=0;j<d.vertexLayout.bindings.Size();j++)
                    if (d.vertexLayout.bindings[j].binding==a.binding) { instanced=d.vertexLayout.bindings[j].perInstance; break; }
                elems[i].InputSlotClass=instanced?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
                elems[i].InstanceDataStepRate=instanced?1:0;
            }
            mDevice->CreateInputLayout(elems.Data(),(UINT)elems.size(),
                sh.vsBlob->GetBufferPointer(),sh.vsBlob->GetBufferSize(),&p.il);
        }

        // Rasterizer state
        D3D11_RASTERIZER_DESC1 rd{};
        rd.FillMode=d.rasterizer.fillMode==NkFillMode::NK_SOLID?D3D11_FILL_SOLID:D3D11_FILL_WIREFRAME;
        rd.CullMode=d.rasterizer.cullMode==NkCullMode::NK_NONE?D3D11_CULL_NONE:d.rasterizer.cullMode==NkCullMode::NK_FRONT?D3D11_CULL_FRONT:D3D11_CULL_BACK;
        rd.FrontCounterClockwise=d.rasterizer.frontFace==NkFrontFace::NK_CCW;
        rd.DepthClipEnable=d.rasterizer.depthClip;
        rd.ScissorEnable=d.rasterizer.scissorTest;
        rd.MultisampleEnable=d.rasterizer.multisampleEnable;
        rd.DepthBias=(INT)d.rasterizer.depthBiasConst;
        rd.SlopeScaledDepthBias=d.rasterizer.depthBiasSlope;
        rd.DepthBiasClamp=d.rasterizer.depthBiasClamp;
        mDevice->CreateRasterizerState1(&rd,&p.rs);

        // Depth stencil
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable=d.depthStencil.depthTestEnable;
        dsd.DepthWriteMask=d.depthStencil.depthWriteEnable?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc=ToDX11Compare(d.depthStencil.depthCompareOp);
        dsd.StencilEnable=d.depthStencil.stencilEnable;
        auto conv=[&](const NkStencilOpState& s)->D3D11_DEPTH_STENCILOP_DESC {
            return {ToDX11StencilOp(s.failOp),ToDX11StencilOp(s.depthFailOp),ToDX11StencilOp(s.passOp),ToDX11Compare(s.compareOp)};
        };
        dsd.FrontFace=conv(d.depthStencil.front); dsd.BackFace=conv(d.depthStencil.back);
        mDevice->CreateDepthStencilState(&dsd,&p.dss);

        // Blend
        D3D11_BLEND_DESC1 bsd{}; bsd.AlphaToCoverageEnable=d.blend.alphaToCoverage;
        bsd.IndependentBlendEnable=d.blend.attachments.Size()>1;
        for (uint32 i=0;i<d.blend.attachments.Size();i++) {
            auto& a=d.blend.attachments[i];
            bsd.RenderTarget[i].BlendEnable=a.blendEnable;
            bsd.RenderTarget[i].SrcBlend=ToDX11Blend(a.srcColor);
            bsd.RenderTarget[i].DestBlend=ToDX11Blend(a.dstColor);
            bsd.RenderTarget[i].BlendOp=ToDX11BlendOp(a.colorOp);
            bsd.RenderTarget[i].SrcBlendAlpha=ToDX11Blend(a.srcAlpha);
            bsd.RenderTarget[i].DestBlendAlpha=ToDX11Blend(a.dstAlpha);
            bsd.RenderTarget[i].BlendOpAlpha=ToDX11BlendOp(a.alphaOp);
            bsd.RenderTarget[i].RenderTargetWriteMask=a.colorWriteMask&0xF;
        }
        mDevice->CreateBlendState1(&bsd,&p.bs);

        uint64 hid=NextId(); mPipelines[hid]=p;
        NkPipelineHandle h; h.id=hid; return h;
    }

    NkPipelineHandle NkDirectX11Device::CreateComputePipeline(const NkComputePipelineDesc& d) {
        threading::NkScopedLock lock(mMutex);
        auto sit=mShaders.Find(d.shader.id); if(!sit) return {};
        NkDX11Pipeline p; p.cs=sit->cs; p.isCompute=true;
        uint64 hid=NextId(); mPipelines[hid]=p;
        NkPipelineHandle h; h.id=hid; return h;
    }

    void NkDirectX11Device::DestroyPipeline(NkPipelineHandle& h) {
        threading::NkScopedLock lock(mMutex);
        auto it=mPipelines.Find(h.id); if(!it) return;
        NK_DX11_SAFE(it->il); NK_DX11_SAFE(it->rs);
        NK_DX11_SAFE(it->dss); NK_DX11_SAFE(it->bs);
        mPipelines.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Render Passes & Framebuffers
    // =============================================================================
    NkRenderPassHandle  NkDirectX11Device::CreateRenderPass(const NkRenderPassDesc& d) {
        threading::NkScopedLock lock(mMutex);
        uint64 hid=NextId(); mRenderPasses[hid]={d};
        NkRenderPassHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::DestroyRenderPass(NkRenderPassHandle& h) {
        threading::NkScopedLock lock(mMutex); mRenderPasses.Erase(h.id); h.id=0;
    }

    NkFramebufferHandle NkDirectX11Device::CreateFramebuffer(const NkFramebufferDesc& d) {
        threading::NkScopedLock lock(mMutex);
        NkDX11Framebuffer fb; fb.w=d.width; fb.h=d.height;
        for (uint32 i=0;i<d.colorAttachments.Size();i++) {
            auto it=mTextures.Find(d.colorAttachments[i].id);
            if(it) fb.rtvs[fb.rtvCount++]=it->rtv;
        }
        if (d.depthAttachment.IsValid()) {
            auto it=mTextures.Find(d.depthAttachment.id);
            if(it) fb.dsv=it->dsv;
        }
        uint64 hid=NextId(); mFramebuffers[hid]=fb;
        NkFramebufferHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::DestroyFramebuffer(NkFramebufferHandle& h) {
        threading::NkScopedLock lock(mMutex); mFramebuffers.Erase(h.id); h.id=0;
    }

    // =============================================================================
    // Descriptor Sets
    // =============================================================================
    NkDescSetHandle NkDirectX11Device::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
        threading::NkScopedLock lock(mMutex);
        uint64 hid=NextId(); mDescLayouts[hid]={d};
        NkDescSetHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
        threading::NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id=0;
    }
    NkDescSetHandle NkDirectX11Device::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
        threading::NkScopedLock lock(mMutex);
        NkDX11DescSet ds; ds.layoutId=layoutHandle.id;
        uint64 hid=NextId(); mDescSets[hid]=ds;
        NkDescSetHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::FreeDescriptorSet(NkDescSetHandle& h) {
        threading::NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id=0;
    }

    void NkDirectX11Device::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
        threading::NkScopedLock lock(mMutex);
        for (uint32 i=0;i<n;i++) {
            auto& w=writes[i];
            auto sit=mDescSets.Find(w.set.id); if(!sit) continue;
            auto& ds=*sit;
            // Trouver ou créer le slot
            NkDX11DescSet::Slot* slot=nullptr;
            for (uint32 j=0;j<ds.count;j++) if(ds.slots[j].slot==w.binding) { slot=&ds.slots[j]; break; }
            if (!slot) { slot=&ds.slots[ds.count++]; slot->slot=w.binding; }
            slot->type=w.type;
            if (w.buffer.IsValid()) {
                auto bit=mBuffers.Find(w.buffer.id);
                if(bit) { slot->kind=NkDX11DescSet::Slot::Buffer; slot->buf=bit->buf; }
            }
            if (w.texture.IsValid()) {
                auto tit=mTextures.Find(w.texture.id);
                if(tit) {
                    slot->srv=tit->srv;
                    slot->uav=tit->uav;
                    slot->kind = slot->ss ? NkDX11DescSet::Slot::TextureAndSampler
                                          : NkDX11DescSet::Slot::Texture;
                }
            }
            if (w.sampler.IsValid()) {
                auto sit2=mSamplers.Find(w.sampler.id);
                if(sit2) {
                    slot->ss=sit2->ss;
                    slot->kind = slot->srv ? NkDX11DescSet::Slot::TextureAndSampler
                                           : NkDX11DescSet::Slot::Sampler;
                }
            }
        }
    }

    void NkDirectX11Device::ApplyDescSet(uint64 id) {
        auto it=mDescSets.Find(id); if(!it) return;
        for (uint32 i=0;i<it->count;i++) {
            auto& s=it->slots[i];
            switch(s.kind) {
                case NkDX11DescSet::Slot::Buffer:
                    if (s.type==NkDescriptorType::NK_UNIFORM_BUFFER) {
                        mContext->VSSetConstantBuffers(s.slot,1,&s.buf);
                        mContext->PSSetConstantBuffers(s.slot,1,&s.buf);
                        mContext->CSSetConstantBuffers(s.slot,1,&s.buf);
                    } else {
                        mContext->CSSetUnorderedAccessViews(s.slot,1,&s.uav,nullptr);
                    }
                    break;
                case NkDX11DescSet::Slot::Texture:
                    mContext->VSSetShaderResources(s.slot,1,&s.srv);
                    mContext->PSSetShaderResources(s.slot,1,&s.srv);
                    mContext->CSSetShaderResources(s.slot,1,&s.srv);
                    break;
                case NkDX11DescSet::Slot::Sampler:
                    mContext->VSSetSamplers(s.slot,1,&s.ss);
                    mContext->PSSetSamplers(s.slot,1,&s.ss);
                    mContext->CSSetSamplers(s.slot,1,&s.ss);
                    break;
                case NkDX11DescSet::Slot::TextureAndSampler:
                    mContext->VSSetShaderResources(s.slot,1,&s.srv);
                    mContext->PSSetShaderResources(s.slot,1,&s.srv);
                    mContext->CSSetShaderResources(s.slot,1,&s.srv);
                    mContext->VSSetSamplers(s.slot,1,&s.ss);
                    mContext->PSSetSamplers(s.slot,1,&s.ss);
                    mContext->CSSetSamplers(s.slot,1,&s.ss);
                    break;
                default: break;
            }
        }
    }

    void NkDirectX11Device::ApplyPipeline(uint64 id) {
        auto it=mPipelines.Find(id); if(!it) return;
        auto& p=*it;
        if (p.isCompute) {
            mContext->CSSetShader(p.cs,nullptr,0);
            return;
        }
        mContext->VSSetShader(p.vs,nullptr,0);
        mContext->PSSetShader(p.ps,nullptr,0);
        if (p.il)  mContext->IASetInputLayout(p.il);
        if (p.rs)  mContext->RSSetState(p.rs);
        if (p.dss) mContext->OMSetDepthStencilState(p.dss,0);
        if (p.bs) {
            float factors[4]={0,0,0,0};
            mContext->OMSetBlendState(p.bs,factors,0xFFFFFFFF);
        }
        mContext->IASetPrimitiveTopology(p.topology);
    }

    // =============================================================================
    // Command Buffer
    // =============================================================================
    NkICommandBuffer* NkDirectX11Device::CreateCommandBuffer(NkCommandBufferType t) {
        return new NkDirectX11CommandBuffer(this, t);
    }
    void NkDirectX11Device::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb=nullptr; }

    // =============================================================================
    // Submit
    // =============================================================================
    void NkDirectX11Device::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
        for (uint32 i=0;i<n;i++) {
            auto* dx=dynamic_cast<NkDirectX11CommandBuffer*>(cbs[i]);
            if (dx) dx->Execute(this);
        }
        if (fence.IsValid()) {
            auto it=mFences.Find(fence.id);
            if(it) it->signaled=true;
        }
    }
    void NkDirectX11Device::SubmitAndPresent(NkICommandBuffer* cb) {
        Submit(&cb,1,{});
        if (!mSwapchain) return;
        const UINT syncInterval = mVsync ? 1u : 0u;
        const UINT flags = (!mVsync && mAllowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
        mSwapchain->Present(syncInterval, flags);
    }

    // Fence (émulation CPU pour DX11)
    NkFenceHandle NkDirectX11Device::CreateFence(bool signaled) {
        uint64 hid=NextId(); mFences[hid]={0,signaled};
        NkFenceHandle h; h.id=hid; return h;
    }
    void NkDirectX11Device::DestroyFence(NkFenceHandle& h) { mFences.Erase(h.id); h.id=0; }
    bool NkDirectX11Device::WaitFence(NkFenceHandle f,uint64) {
        auto it=mFences.Find(f.id); if(!it) return false;
        return it->signaled;
    }
    bool NkDirectX11Device::IsFenceSignaled(NkFenceHandle f) {
        auto it=mFences.Find(f.id); return it && it->signaled;
    }
    void NkDirectX11Device::ResetFence(NkFenceHandle f) {
        auto it=mFences.Find(f.id); if(it) it->signaled=false;
    }
    void NkDirectX11Device::WaitIdle() { mContext->Flush(); }

    // Frame
    bool NkDirectX11Device::BeginFrame(NkFrameContext& frame) {
        frame.frameIndex=mFrameIndex; frame.frameNumber=mFrameNumber;
        return true;
    }
    void NkDirectX11Device::EndFrame(NkFrameContext&) {
        mFrameIndex=(mFrameIndex+1)%MAX_FRAMES; ++mFrameNumber;
    }
    void NkDirectX11Device::OnResize(uint32 w, uint32 h) {
        if (w==0||h==0) return;
        mWidth=w; mHeight=h; ResizeSwapchain(w,h);
    }

    // =============================================================================
    // Caps
    // =============================================================================
    void NkDirectX11Device::QueryCaps() {
        D3D11_FEATURE_DATA_D3D11_OPTIONS2 opts{};
        mDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,&opts,sizeof(opts));
        mCaps.tessellationShaders=true; mCaps.geometryShaders=true;
        mCaps.computeShaders=NkDeviceInitComputeEnabledForApi(mInit, NkGraphicsApi::NK_API_DIRECTX11);
        mCaps.drawIndirect=true;
        mCaps.multiViewport=true; mCaps.independentBlend=true;
        mCaps.logicOp=true; mCaps.textureCompressionBC=true;
        mCaps.maxTextureDim2D=16384; mCaps.maxTextureDim3D=2048;
        mCaps.maxColorAttachments=8; mCaps.maxVertexAttributes=16;
        mCaps.maxPushConstantBytes=128; mCaps.minUniformBufferAlign=256;
        mCaps.msaa2x=mCaps.msaa4x=mCaps.msaa8x=true;
    }

    // =============================================================================
    // Accès internes
    // =============================================================================
    ID3D11Buffer* NkDirectX11Device::GetDXBuffer(uint64 id) const { auto it=mBuffers.Find(id); return it ?it->buf:nullptr; }
    ID3D11ShaderResourceView* NkDirectX11Device::GetSRV(uint64 id) const { auto it=mTextures.Find(id); return it ?it->srv:nullptr; }
    ID3D11UnorderedAccessView* NkDirectX11Device::GetUAV(uint64 id) const { auto it=mTextures.Find(id); return it ?it->uav:nullptr; }
    const NkDX11Pipeline* NkDirectX11Device::GetPipeline(uint64 id) const { auto it=mPipelines.Find(id); return it; }
    const NkDX11DescSet*  NkDirectX11Device::GetDescSet(uint64 id) const { auto it=mDescSets.Find(id); return it; }
    const NkDX11Framebuffer* NkDirectX11Device::GetFBO(uint64 id) const { auto it=mFramebuffers.Find(id); return it; }

    // =============================================================================
    // Conversions
    // =============================================================================
    DXGI_FORMAT NkDirectX11Device::ToDXGIFormat(NkGPUFormat f, bool) {
        switch(f) {
            case NkGPUFormat::NK_R8_UNORM:      return DXGI_FORMAT_R8_UNORM;
            case NkGPUFormat::NK_RG8_UNORM:     return DXGI_FORMAT_R8G8_UNORM;
            case NkGPUFormat::NK_RGBA8_UNORM:   return DXGI_FORMAT_R8G8B8A8_UNORM;
            case NkGPUFormat::NK_RGBA8_SRGB:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case NkGPUFormat::NK_BGRA8_UNORM:   return DXGI_FORMAT_B8G8R8A8_UNORM;
            case NkGPUFormat::NK_BGRA8_SRGB:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
            case NkGPUFormat::NK_R16_FLOAT:     return DXGI_FORMAT_R16_FLOAT;
            case NkGPUFormat::NK_RG16_FLOAT:    return DXGI_FORMAT_R16G16_FLOAT;
            case NkGPUFormat::NK_RGBA16_FLOAT:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case NkGPUFormat::NK_R32_FLOAT:     return DXGI_FORMAT_R32_FLOAT;
            case NkGPUFormat::NK_RG32_FLOAT:    return DXGI_FORMAT_R32G32_FLOAT;
            case NkGPUFormat::NK_RGB32_FLOAT:   return DXGI_FORMAT_R32G32B32_FLOAT;
            case NkGPUFormat::NK_RGBA32_FLOAT:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case NkGPUFormat::NK_R32_UINT:      return DXGI_FORMAT_R32_UINT;
            case NkGPUFormat::NK_RG32_UINT:     return DXGI_FORMAT_R32G32_UINT;
            case NkGPUFormat::NK_D16_UNORM:     return DXGI_FORMAT_D16_UNORM;
            case NkGPUFormat::NK_D32_FLOAT:     return DXGI_FORMAT_D32_FLOAT;
            case NkGPUFormat::NK_D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
            case NkGPUFormat::NK_BC1_RGB_UNORM: return DXGI_FORMAT_BC1_UNORM;
            case NkGPUFormat::NK_BC3_UNORM:     return DXGI_FORMAT_BC3_UNORM;
            case NkGPUFormat::NK_BC5_UNORM:     return DXGI_FORMAT_BC5_UNORM;
            case NkGPUFormat::NK_BC7_UNORM:     return DXGI_FORMAT_BC7_UNORM;
            case NkGPUFormat::NK_R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;
            default:                      return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }
    D3D11_FILTER NkDirectX11Device::ToDX11Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp) {
        bool aniso = false; // détection simplifiée
        if (cmp) return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        if (mag==NkFilter::NK_LINEAR&&min==NkFilter::NK_LINEAR&&mip==NkMipFilter::NK_LINEAR) return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        if (mag==NkFilter::NK_NEAREST&&min==NkFilter::NK_NEAREST&&mip==NkMipFilter::NK_NEAREST) return D3D11_FILTER_MIN_MAG_MIP_POINT;
        return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
    D3D11_TEXTURE_ADDRESS_MODE NkDirectX11Device::ToDX11Address(NkAddressMode a) {
        switch(a) {
            case NkAddressMode::NK_REPEAT:         return D3D11_TEXTURE_ADDRESS_WRAP;
            case NkAddressMode::NK_MIRRORED_REPEAT: return D3D11_TEXTURE_ADDRESS_MIRROR;
            case NkAddressMode::NK_CLAMP_TO_EDGE:    return D3D11_TEXTURE_ADDRESS_CLAMP;
            case NkAddressMode::NK_CLAMP_TO_BORDER:  return D3D11_TEXTURE_ADDRESS_BORDER;
            default:                            return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }
    D3D11_COMPARISON_FUNC NkDirectX11Device::ToDX11Compare(NkCompareOp op) {
        switch(op) {
            case NkCompareOp::NK_NEVER:        return D3D11_COMPARISON_NEVER;
            case NkCompareOp::NK_LESS:         return D3D11_COMPARISON_LESS;
            case NkCompareOp::NK_EQUAL:        return D3D11_COMPARISON_EQUAL;
            case NkCompareOp::NK_LESS_EQUAL:    return D3D11_COMPARISON_LESS_EQUAL;
            case NkCompareOp::NK_GREATER:      return D3D11_COMPARISON_GREATER;
            case NkCompareOp::NK_NOT_EQUAL:     return D3D11_COMPARISON_NOT_EQUAL;
            case NkCompareOp::NK_GREATER_EQUAL: return D3D11_COMPARISON_GREATER_EQUAL;
            default:                        return D3D11_COMPARISON_ALWAYS;
        }
    }
    D3D11_BLEND NkDirectX11Device::ToDX11Blend(NkBlendFactor f) {
        switch(f) {
            case NkBlendFactor::NK_ZERO:              return D3D11_BLEND_ZERO;
            case NkBlendFactor::NK_ONE:               return D3D11_BLEND_ONE;
            case NkBlendFactor::NK_SRC_COLOR:          return D3D11_BLEND_SRC_COLOR;
            case NkBlendFactor::NK_ONE_MINUS_SRC_COLOR:  return D3D11_BLEND_INV_SRC_COLOR;
            case NkBlendFactor::NK_SRC_ALPHA:          return D3D11_BLEND_SRC_ALPHA;
            case NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA:  return D3D11_BLEND_INV_SRC_ALPHA;
            case NkBlendFactor::NK_DST_ALPHA:          return D3D11_BLEND_DEST_ALPHA;
            case NkBlendFactor::NK_ONE_MINUS_DST_ALPHA:  return D3D11_BLEND_INV_DEST_ALPHA;
            case NkBlendFactor::NK_SRC_ALPHA_SATURATE:  return D3D11_BLEND_SRC_ALPHA_SAT;
            default:                               return D3D11_BLEND_ONE;
        }
    }
    D3D11_BLEND_OP NkDirectX11Device::ToDX11BlendOp(NkBlendOp op) {
        switch(op) {
            case NkBlendOp::NK_ADD:    return D3D11_BLEND_OP_ADD;
            case NkBlendOp::NK_SUB:    return D3D11_BLEND_OP_SUBTRACT;
            case NkBlendOp::NK_REV_SUB: return D3D11_BLEND_OP_REV_SUBTRACT;
            case NkBlendOp::NK_MIN:    return D3D11_BLEND_OP_MIN;
            case NkBlendOp::NK_MAX:    return D3D11_BLEND_OP_MAX;
            default:                return D3D11_BLEND_OP_ADD;
        }
    }
    D3D11_STENCIL_OP NkDirectX11Device::ToDX11StencilOp(NkStencilOp op) {
        switch(op) {
            case NkStencilOp::NK_KEEP:      return D3D11_STENCIL_OP_KEEP;
            case NkStencilOp::NK_ZERO:      return D3D11_STENCIL_OP_ZERO;
            case NkStencilOp::NK_REPLACE:   return D3D11_STENCIL_OP_REPLACE;
            case NkStencilOp::NK_INCR_CLAMP: return D3D11_STENCIL_OP_INCR_SAT;
            case NkStencilOp::NK_DECR_CLAMP: return D3D11_STENCIL_OP_DECR_SAT;
            case NkStencilOp::NK_INVERT:    return D3D11_STENCIL_OP_INVERT;
            case NkStencilOp::NK_INCR_WRAP:  return D3D11_STENCIL_OP_INCR;
            case NkStencilOp::NK_DECR_WRAP:  return D3D11_STENCIL_OP_DECR;
            default:                     return D3D11_STENCIL_OP_KEEP;
        }
    }
    D3D11_PRIMITIVE_TOPOLOGY NkDirectX11Device::ToDX11Topology(NkPrimitiveTopology t) {
        switch(t) {
            case NkPrimitiveTopology::NK_TRIANGLE_LIST:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case NkPrimitiveTopology::NK_TRIANGLE_STRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            case NkPrimitiveTopology::NK_LINE_LIST:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            case NkPrimitiveTopology::NK_LINE_STRIP:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case NkPrimitiveTopology::NK_POINT_LIST:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            default:                                 return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED
