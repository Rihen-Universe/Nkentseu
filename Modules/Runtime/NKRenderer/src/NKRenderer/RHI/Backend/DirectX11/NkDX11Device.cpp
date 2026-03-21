#include "NKLogger/NkLog.h"
#include "NKRenderer/RHI/Core/NkISwapchain.h"
#include "NKRenderer/RHI/Backend/DirectX11/NkDX11Swapchain.h"
// =============================================================================
// NkRHI_Device_DX11.cpp — Backend DirectX 11.1
// =============================================================================
#ifdef NK_RHI_DX11_ENABLED
#include "NKRenderer/RHI/Backend/DirectX11/NkDX11Device.h"
#include "NKRenderer/RHI/Backend/DirectX11/NkDX11CommandBuffer.h"
#include "NKRenderer/Context/Graphics/DirectX/NkDirectXContextData.h"
#include <cstdio>
#include <cstring>

#define NK_DX11_LOG(...)  logger_src.Infof("[NkRHI_DX11] " __VA_ARGS__)
#define NK_DX11_ERR(...)  logger_src.Infof("[NkRHI_DX11][ERR] " __VA_ARGS__)
#define NK_DX11_CHECK(hr, msg) do { if(FAILED(hr)) { NK_DX11_ERR(msg " (hr=0x%X)\n",(unsigned)(hr)); } } while(0)
#define NK_DX11_SAFE(p) if(p){(p)->Release();(p)=nullptr;}

namespace nkentseu {
using threading::NkMutex;
using threading::NkScopedLock;

NkDeviceDX11::~NkDeviceDX11() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkDeviceDX11::Initialize(NkIGraphicsContext* ctx) {
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    auto* native = static_cast<NkDX11ContextData*>(ctx->GetNativeContextData());
    if (!native || !native->device || !native->context || !native->swapchain) {
        NK_DX11_ERR("Donnees natives DX11 indisponibles\n");
        return false;
    }

    mDevice  = native->device.Get();
    mContext = native->context.Get();
    mHwnd    = native->hwnd;
    mWidth   = native->width;
    mHeight  = native->height;
    if (mWidth == 0 || mHeight == 0) {
        const auto info = ctx->GetInfo();
        if (mWidth == 0) mWidth = info.windowWidth;
        if (mHeight == 0) mHeight = info.windowHeight;
    }
    if (mWidth == 0) mWidth = 1;
    if (mHeight == 0) mHeight = 1;

    CreateSwapchain(mWidth, mHeight);
    QueryCaps();

    mIsValid = true;
    NK_DX11_LOG("Initialisé (%u×%u)\n", mWidth, mHeight);
    return true;
}

void NkDeviceDX11::CreateSwapchain(uint32 w, uint32 h) {
    // Réutiliser le swapchain déjà créé par le contexte — non-owning
    auto* native = mCtx ? static_cast<NkDX11ContextData*>(mCtx->GetNativeContextData()) : nullptr;
    if (!native || !native->swapchain) {
        NK_DX11_ERR("Pas de IDXGISwapChain1 dans le contexte\n");
        return;
    }
    mSwapchain = native->swapchain.Get(); // non-owning

    // Back buffer texture depuis le swapchain du contexte
    ID3D11Texture2D* bb = nullptr;
    mSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
    if (!bb) { NK_DX11_ERR("GetBuffer(0) échoué\n"); return; }

    uint64 colorId = NextId();
    NkDX11Texture swt; swt.tex = bb; swt.isSwapchain = true;
    swt.desc = NkTextureDesc::RenderTarget(w, h, NkGpuFormat::NK_BGRA8_UNORM);
    mDevice->CreateRenderTargetView(bb, nullptr, &swt.rtv);
    mTextures[colorId] = swt;
    NkTextureHandle colorH; colorH.id = colorId;

    // Depth
    NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h);
    auto depthH = CreateTexture(dd);

    // Render pass
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkGpuFormat::NK_BGRA8_UNORM))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP = CreateRenderPass(rpd);

    NkFramebufferDesc fbd;
    fbd.renderPass = mSwapchainRP;
    fbd.colorAttachments.PushBack(colorH);
    fbd.depthAttachment = depthH;
    fbd.width = w; fbd.height = h;
    mSwapchainFB = CreateFramebuffer(fbd);
}

void NkDeviceDX11::DestroySwapchain() {
    mContext->OMSetRenderTargets(0, nullptr, nullptr);
    DestroyFramebuffer(mSwapchainFB);
    DestroyRenderPass(mSwapchainRP);
    mSwapchain = nullptr; // non-owning — appartient au contexte, pas de Release()
}

void NkDeviceDX11::ResizeSwapchain(uint32 w, uint32 h) {
    WaitIdle();
    DestroySwapchain();
    CreateSwapchain(w,h);
}

void NkDeviceDX11::Shutdown() {
    WaitIdle();
    DestroySwapchain();
    mBuffers.ForEach([](const uint64&, NkDX11Buffer& b)   { NK_DX11_SAFE(b.buf); });
    mTextures.ForEach([](const uint64&, NkDX11Texture& t) {
        if (!t.isSwapchain) {
            NK_DX11_SAFE(t.tex); NK_DX11_SAFE(t.srv); NK_DX11_SAFE(t.rtv);
            NK_DX11_SAFE(t.dsv); NK_DX11_SAFE(t.uav);
        } else {
            // Pour les textures swapchain : libérer le RTV (créé par nous)
            // et la référence sur le back buffer (obtenue via GetBuffer)
            NK_DX11_SAFE(t.rtv);
            NK_DX11_SAFE(t.tex); // Release de la ref GetBuffer()
        }
    });
    mSamplers.ForEach([](const uint64&, NkDX11Sampler& s)  { NK_DX11_SAFE(s.ss); });
    mShaders.ForEach([](const uint64&, NkDX11Shader& sh) {
        NK_DX11_SAFE(sh.vs); NK_DX11_SAFE(sh.ps); NK_DX11_SAFE(sh.cs);
        NK_DX11_SAFE(sh.gs); NK_DX11_SAFE(sh.vsBlob);
    });
    mPipelines.ForEach([](const uint64&, NkDX11Pipeline& p) {
        NK_DX11_SAFE(p.vs); NK_DX11_SAFE(p.ps); NK_DX11_SAFE(p.cs);
        NK_DX11_SAFE(p.il); NK_DX11_SAFE(p.rs); NK_DX11_SAFE(p.dss); NK_DX11_SAFE(p.bs);
    });
    NK_DX11_SAFE(mFactory);
    mIsValid=false;
    NK_DX11_LOG("Shutdown\n");
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDeviceDX11::CreateBuffer(const NkBufferDesc& desc) {
    NkScopedLock lock(mMutex);
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

void NkDeviceDX11::DestroyBuffer(NkBufferHandle& h) {
    NkScopedLock lock(mMutex);
    auto it=mBuffers.Find(h.id); if(it== nullptr) return;
    NK_DX11_SAFE(it->buf); mBuffers.Erase(h.id); h.id=0;
}

bool NkDeviceDX11::WriteBuffer(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
    auto it=mBuffers.Find(buf.id); if(it== nullptr) return false;
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (it->desc.usage==NkResourceUsage::NK_UPLOAD) {
        mContext->Map(it->buf,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
        memory::NkMemCopy((uint8*)ms.pData+off,data,(size_t)sz);
        mContext->Unmap(it->buf,0);
    } else {
        D3D11_BOX box{(UINT)off,0,0,(UINT)(off+sz),1,1};
        mContext->UpdateSubresource(it->buf,0,&box,data,0,0);
    }
    return true;
}

bool NkDeviceDX11::WriteBufferAsync(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
    return WriteBuffer(buf,data,sz,off);
}

bool NkDeviceDX11::ReadBuffer(NkBufferHandle buf,void* out,uint64 sz,uint64 off) {
    auto it=mBuffers.Find(buf.id); if(it== nullptr) return false;
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
    memory::NkMemCopy(out,ms.pData,(size_t)sz);
    mContext->Unmap(staging,0);
    staging->Release();
    return true;
}

NkMappedMemory NkDeviceDX11::MapBuffer(NkBufferHandle buf,uint64 off,uint64 sz) {
    auto it=mBuffers.Find(buf.id); if(it== nullptr) return {};
    D3D11_MAPPED_SUBRESOURCE ms{};
    mContext->Map(it->buf,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
    uint64 mapSz=sz>0?sz:it->desc.sizeBytes-off;
    return {(uint8*)ms.pData+off,mapSz};
}
void NkDeviceDX11::UnmapBuffer(NkBufferHandle buf) {
    auto it=mBuffers.Find(buf.id); if(it== nullptr) return;
    mContext->Unmap(it->buf,0);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDeviceDX11::CreateTexture(const NkTextureDesc& desc) {
    NkScopedLock lock(mMutex);
    D3D11_TEXTURE2D_DESC td{};
    td.Width=desc.width; td.Height=desc.height;
    td.MipLevels=desc.mipLevels==0?0:desc.mipLevels;
    td.ArraySize=desc.arrayLayers;
    td.Format=ToDXGIFormat(desc.format);
    td.SampleDesc={(UINT)desc.samples,0};
    td.Usage=D3D11_USAGE_DEFAULT;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_SHADER_RESOURCE))  td.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_RENDER_TARGET))    td.BindFlags|=D3D11_BIND_RENDER_TARGET;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_DEPTH_STENCIL))    td.BindFlags|=D3D11_BIND_DEPTH_STENCIL;
    // DX11 interdit DXGI_FORMAT_D*_FLOAT avec BIND_SHADER_RESOURCE → utiliser le format typeless
    if ((td.BindFlags & D3D11_BIND_DEPTH_STENCIL) && (td.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
        if      (td.Format == DXGI_FORMAT_D32_FLOAT)        td.Format = DXGI_FORMAT_R32_TYPELESS;
        else if (td.Format == DXGI_FORMAT_D24_UNORM_S8_UINT) td.Format = DXGI_FORMAT_R24G8_TYPELESS;
    }
    if (NkHasFlag(desc.bindFlags,NkBindFlags::NK_UNORDERED_ACCESS)) td.BindFlags|=D3D11_BIND_UNORDERED_ACCESS;
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

    NkDX11Texture t; t.tex=tex; t.desc=desc;

    if (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        if (NkFormatIsDepth(desc.format)) {
            sd.Format=desc.format==NkGpuFormat::NK_D32_FLOAT?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        } else { sd.Format=td.Format; }
        if (desc.type==NkTextureType::NK_CUBE) {
            sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURECUBE;
            sd.TextureCube.MipLevels=td.MipLevels==0?(UINT)-1:td.MipLevels;
        } else {
            sd.ViewDimension=desc.samples>NkSampleCount::NK_S1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MipLevels=td.MipLevels==0?(UINT)-1:td.MipLevels;
        }
        mDevice->CreateShaderResourceView(tex,&sd,&t.srv);
    }
    if (td.BindFlags & D3D11_BIND_RENDER_TARGET)
        mDevice->CreateRenderTargetView(tex,nullptr,&t.rtv);
    if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
        dd.Format=desc.format==NkGpuFormat::NK_D32_FLOAT?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D24_UNORM_S8_UINT;
        dd.ViewDimension=desc.samples>NkSampleCount::NK_S1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
        mDevice->CreateDepthStencilView(tex,&dd,&t.dsv);
    }
    if (td.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
        mDevice->CreateUnorderedAccessView(tex,nullptr,&t.uav);

    uint64 hid=NextId(); mTextures[hid]=t;
    NkTextureHandle h; h.id=hid; return h;
}

void NkDeviceDX11::DestroyTexture(NkTextureHandle& h) {
    NkScopedLock lock(mMutex);
    auto it=mTextures.Find(h.id); if(it== nullptr) return;
    if (!it->isSwapchain) {
        NK_DX11_SAFE(it->tex); NK_DX11_SAFE(it->srv);
        NK_DX11_SAFE(it->rtv); NK_DX11_SAFE(it->dsv);
        NK_DX11_SAFE(it->uav);
    }
    mTextures.Erase(h.id); h.id=0;
}

bool NkDeviceDX11::WriteTexture(NkTextureHandle t,const void* p,uint32 rp) {
    auto it=mTextures.Find(t.id); if(it== nullptr) return false;
    auto& desc=it->desc;
    uint32 pitch=rp>0?rp:desc.width*NkFormatBytesPerPixel(desc.format);
    mContext->UpdateSubresource(it->tex,0,nullptr,p,pitch,pitch*desc.height);
    return true;
}
bool NkDeviceDX11::WriteTextureRegion(NkTextureHandle t,const void* p,
    uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
    uint32 mip,uint32 layer,uint32 rp) {
    auto it=mTextures.Find(t.id); if(it== nullptr) return false;
    auto& desc=it->desc;
    uint32 pitch=rp>0?rp:w*NkFormatBytesPerPixel(desc.format);
    D3D11_BOX box{x,y,z,x+w,y+h,z+d2};
    uint32 sub=D3D11CalcSubresource(mip,layer,desc.mipLevels?desc.mipLevels:1);
    mContext->UpdateSubresource(it->tex,sub,&box,p,pitch,pitch*h);
    return true;
}
bool NkDeviceDX11::GenerateMipmaps(NkTextureHandle t, NkFilter) {
    auto it=mTextures.Find(t.id); if(it== nullptr) return false;
    if (it->srv) mContext->GenerateMips(it->srv);
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDeviceDX11::CreateSampler(const NkSamplerDesc& d) {
    NkScopedLock lock(mMutex);
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
void NkDeviceDX11::DestroySampler(NkSamplerHandle& h) {
    NkScopedLock lock(mMutex);
    auto it=mSamplers.Find(h.id); if(it== nullptr) return;
    NK_DX11_SAFE(it->ss); mSamplers.Erase(h.id); h.id=0;
}

// =============================================================================
// Shaders (compilation HLSL runtime avec D3DCompile)
// =============================================================================
NkShaderHandle NkDeviceDX11::CreateShader(const NkShaderDesc& desc) {
    NkScopedLock lock(mMutex);
    NkDX11Shader sh;
    for (uint32 i=0;i<(uint32)desc.stages.Size();i++) {
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
void NkDeviceDX11::DestroyShader(NkShaderHandle& h) {
    NkScopedLock lock(mMutex);
    auto it=mShaders.Find(h.id); if(it== nullptr) return;
    NK_DX11_SAFE(it->vs); NK_DX11_SAFE(it->ps);
    NK_DX11_SAFE(it->cs); NK_DX11_SAFE(it->gs);
    NK_DX11_SAFE(it->vsBlob);
    mShaders.Erase(h.id); h.id=0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDeviceDX11::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    NkScopedLock lock(mMutex);
    auto sit=mShaders.Find(d.shader.id); if(sit== nullptr) return {};
    auto& sh=(*sit);

    NkDX11Pipeline p;
    p.vs=sh.vs; p.ps=sh.ps;
    p.topology=ToDX11Topology(d.topology);
    p.isCompute=false;

    // Input Layout
    if (sh.vsBlob && d.vertexLayout.attributes.Size()>0) {
        NkVector<D3D11_INPUT_ELEMENT_DESC> elems; elems.Resize(d.vertexLayout.attributes.Size());
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
            elems[i].Format=fmtTable[NK_MIN((uint32)a.format,15u)];
            elems[i].InputSlot=a.binding;
            elems[i].AlignedByteOffset=a.offset;
            // Trouver le binding pour savoir si c'est instancé
            bool instanced=false;
            for (uint32 j=0;j<d.vertexLayout.bindings.Size();j++)
                if (d.vertexLayout.bindings[j].binding==a.binding) { instanced=d.vertexLayout.bindings[j].perInstance; break; }
            elems[i].InputSlotClass=instanced?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
            elems[i].InstanceDataStepRate=instanced?1:0;
        }
        mDevice->CreateInputLayout(elems.Data(),(UINT)elems.Size(),
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

    // Strides par binding (pour IASetVertexBuffers)
    for (uint32 i=0;i<d.vertexLayout.bindings.Size();i++) {
        auto& b=d.vertexLayout.bindings[i];
        if (b.binding < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
            p.strides[b.binding]=b.stride;
    }

    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

NkPipelineHandle NkDeviceDX11::CreateComputePipeline(const NkComputePipelineDesc& d) {
    NkScopedLock lock(mMutex);
    auto sit=mShaders.Find(d.shader.id); if(sit== nullptr) return {};
    NkDX11Pipeline p; p.cs=sit->cs; p.isCompute=true;
    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

void NkDeviceDX11::DestroyPipeline(NkPipelineHandle& h) {
    NkScopedLock lock(mMutex);
    auto it=mPipelines.Find(h.id); if(it== nullptr) return;
    NK_DX11_SAFE(it->il); NK_DX11_SAFE(it->rs);
    NK_DX11_SAFE(it->dss); NK_DX11_SAFE(it->bs);
    mPipelines.Erase(h.id); h.id=0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle  NkDeviceDX11::CreateRenderPass(const NkRenderPassDesc& d) {
    NkScopedLock lock(mMutex);
    uint64 hid=NextId(); mRenderPasses[hid]={d};
    NkRenderPassHandle h; h.id=hid; return h;
}
void NkDeviceDX11::DestroyRenderPass(NkRenderPassHandle& h) {
    NkScopedLock lock(mMutex); mRenderPasses.Erase(h.id); h.id=0;
}

NkFramebufferHandle NkDeviceDX11::CreateFramebuffer(const NkFramebufferDesc& d) {
    NkScopedLock lock(mMutex);
    NkDX11Framebuffer fb; fb.w=d.width; fb.h=d.height;
    for (uint32 i=0;i<d.colorAttachments.Size();i++) {
        auto it=mTextures.Find(d.colorAttachments[i].id);
        if (it!= nullptr) fb.rtvs[fb.rtvCount++]=it->rtv;
    }
    if (d.depthAttachment.IsValid()) {
        auto it=mTextures.Find(d.depthAttachment.id);
        if (it!= nullptr) fb.dsv=it->dsv;
    }
    uint64 hid=NextId(); mFramebuffers[hid]=fb;
    NkFramebufferHandle h; h.id=hid; return h;
}
void NkDeviceDX11::DestroyFramebuffer(NkFramebufferHandle& h) {
    NkScopedLock lock(mMutex); mFramebuffers.Erase(h.id); h.id=0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDeviceDX11::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    NkScopedLock lock(mMutex);
    uint64 hid=NextId(); mDescLayouts[hid]={d};
    NkDescSetHandle h; h.id=hid; return h;
}
void NkDeviceDX11::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    NkScopedLock lock(mMutex); mDescLayouts.Erase(h.id); h.id=0;
}
NkDescSetHandle NkDeviceDX11::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
    NkScopedLock lock(mMutex);
    NkDX11DescSet ds; ds.layoutId=layoutHandle.id;
    uint64 hid=NextId(); mDescSets[hid]=ds;
    NkDescSetHandle h; h.id=hid; return h;
}
void NkDeviceDX11::FreeDescriptorSet(NkDescSetHandle& h) {
    NkScopedLock lock(mMutex); mDescSets.Erase(h.id); h.id=0;
}

void NkDeviceDX11::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    NkScopedLock lock(mMutex);
    for (uint32 i=0;i<n;i++) {
        auto& w=writes[i];
        auto sit=mDescSets.Find(w.set.id); if(sit== nullptr) continue;
        auto& ds=(*sit);
        // Trouver ou créer le slot
        NkDX11DescSet::Slot* slot=nullptr;
        for (uint32 j=0;j<ds.count;j++) if(ds.slots[j].slot==w.binding) { slot=&ds.slots[j]; break; }
        if (!slot) { slot=&ds.slots[ds.count++]; slot->slot=w.binding; }
        slot->type=w.type;
        if (w.buffer.IsValid()) {
            auto bit=mBuffers.Find(w.buffer.id);
            if (bit!= nullptr) { slot->kind=NkDX11DescSet::Slot::Buffer; slot->buf=bit->buf; }
        }
        if (w.texture.IsValid()) {
            auto tit=mTextures.Find(w.texture.id);
            if (tit!= nullptr) {
                slot->kind=NkDX11DescSet::Slot::Texture;
                slot->srv=tit->srv;
                slot->uav=tit->uav;
            }
        }
        if (w.sampler.IsValid()) {
            auto sit2=mSamplers.Find(w.sampler.id);
            if (sit2!= nullptr) { slot->kind=NkDX11DescSet::Slot::Sampler; slot->ss=sit2->ss; }
        }
    }
}

void NkDeviceDX11::ApplyDescSet(uint64 id) {
    auto it=mDescSets.Find(id); if(it== nullptr) return;
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
            default: break;
        }
    }
}

void NkDeviceDX11::ApplyPipeline(uint64 id) {
    auto it=mPipelines.Find(id); if(it== nullptr) return;
    auto& p=(*it);
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
NkICommandBuffer* NkDeviceDX11::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_DX11(this, t);
}
void NkDeviceDX11::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb=nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDeviceDX11::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i=0;i<n;i++) {
        auto* dx=dynamic_cast<NkCommandBuffer_DX11*>(cbs[i]);
        if (dx) dx->Execute(this);
    }
    if (fence.IsValid()) {
        auto it=mFences.Find(fence.id);
        if (it!= nullptr) it->signaled=true;
    }
}
void NkDeviceDX11::SubmitAndPresent(NkICommandBuffer* cb) {
    if (!mSwapchain) { NK_DX11_ERR("SubmitAndPresent: swapchain null\n"); return; }
    Submit(&cb,1,{});
    mSwapchain->Present(0, 0);
}

// Fence (émulation CPU pour DX11)
NkFenceHandle NkDeviceDX11::CreateFence(bool signaled) {
    uint64 hid=NextId(); mFences[hid]={0,signaled};
    NkFenceHandle h; h.id=hid; return h;
}
void NkDeviceDX11::DestroyFence(NkFenceHandle& h) { mFences.Erase(h.id); h.id=0; }
bool NkDeviceDX11::WaitFence(NkFenceHandle f,uint64) {
    auto it=mFences.Find(f.id); if(it== nullptr) return false;
    return it->signaled;
}
bool NkDeviceDX11::IsFenceSignaled(NkFenceHandle f) {
    auto it=mFences.Find(f.id); return it!= nullptr&&it->signaled;
}
void NkDeviceDX11::ResetFence(NkFenceHandle f) {
    auto it=mFences.Find(f.id); if(it!= nullptr) it->signaled=false;
}
void NkDeviceDX11::WaitIdle() { mContext->Flush(); }

// Frame
void NkDeviceDX11::BeginFrame(NkFrameContext& frame) {
    frame.frameIndex=mFrameIndex; frame.frameNumber=mFrameNumber;
}
void NkDeviceDX11::EndFrame(NkFrameContext&) {
    mFrameIndex=(mFrameIndex+1)%kMaxFrames; ++mFrameNumber;
}
void NkDeviceDX11::OnResize(uint32 w, uint32 h) {
    if (w==0||h==0) return;
    mWidth=w; mHeight=h; ResizeSwapchain(w,h);
}

// =============================================================================
// Caps
// =============================================================================
void NkDeviceDX11::QueryCaps() {
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 opts{};
    mDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2,&opts,sizeof(opts));
    mCaps.tessellationShaders=true; mCaps.geometryShaders=true;
    mCaps.computeShaders=true; mCaps.drawIndirect=true;
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
ID3D11Buffer* NkDeviceDX11::GetDXBuffer(uint64 id) const { auto it=mBuffers.Find(id); return it!= nullptr?it->buf:nullptr; }
ID3D11ShaderResourceView* NkDeviceDX11::GetSRV(uint64 id) const { auto it=mTextures.Find(id); return it!= nullptr?it->srv:nullptr; }
ID3D11UnorderedAccessView* NkDeviceDX11::GetUAV(uint64 id) const { auto it=mTextures.Find(id); return it!= nullptr?it->uav:nullptr; }
const NkDX11Pipeline* NkDeviceDX11::GetPipeline(uint64 id) const { auto it=mPipelines.Find(id); return it!= nullptr?&(*it):nullptr; }
const NkDX11DescSet*  NkDeviceDX11::GetDescSet(uint64 id) const { auto it=mDescSets.Find(id); return it!= nullptr?&(*it):nullptr; }
const NkDX11Framebuffer* NkDeviceDX11::GetFBO(uint64 id) const { auto it=mFramebuffers.Find(id); return it!= nullptr?&(*it):nullptr; }

// =============================================================================
// Conversions
// =============================================================================
DXGI_FORMAT NkDeviceDX11::ToDXGIFormat(NkGpuFormat f, bool) {
    switch(f) {
        case NkGpuFormat::NK_R8_UNORM:      return DXGI_FORMAT_R8_UNORM;
        case NkGpuFormat::NK_RG8_UNORM:     return DXGI_FORMAT_R8G8_UNORM;
        case NkGpuFormat::NK_RGBA8_UNORM:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkGpuFormat::NK_RGBA8_SRGB:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case NkGpuFormat::NK_BGRA8_UNORM:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case NkGpuFormat::NK_BGRA8_SRGB:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case NkGpuFormat::NK_R16_FLOAT:     return DXGI_FORMAT_R16_FLOAT;
        case NkGpuFormat::NK_RG16_FLOAT:    return DXGI_FORMAT_R16G16_FLOAT;
        case NkGpuFormat::NK_RGBA16_FLOAT:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkGpuFormat::NK_R32_FLOAT:     return DXGI_FORMAT_R32_FLOAT;
        case NkGpuFormat::NK_RG32_FLOAT:    return DXGI_FORMAT_R32G32_FLOAT;
        case NkGpuFormat::NK_RGB32_FLOAT:   return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkGpuFormat::NK_RGBA32_FLOAT:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkGpuFormat::NK_R32_UINT:      return DXGI_FORMAT_R32_UINT;
        case NkGpuFormat::NK_RG32_UINT:     return DXGI_FORMAT_R32G32_UINT;
        case NkGpuFormat::NK_D16_UNORM:     return DXGI_FORMAT_D16_UNORM;
        case NkGpuFormat::NK_D32_FLOAT:     return DXGI_FORMAT_D32_FLOAT;
        case NkGpuFormat::NK_D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case NkGpuFormat::NK_BC1_RGB_UNORM: return DXGI_FORMAT_BC1_UNORM;
        case NkGpuFormat::NK_BC3_UNORM:     return DXGI_FORMAT_BC3_UNORM;
        case NkGpuFormat::NK_BC5_UNORM:     return DXGI_FORMAT_BC5_UNORM;
        case NkGpuFormat::NK_BC7_UNORM:     return DXGI_FORMAT_BC7_UNORM;
        case NkGpuFormat::NK_R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;
        default:                      return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}
D3D11_FILTER NkDeviceDX11::ToDX11Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp) {
    bool aniso = false; // détection simplifiée
    if (cmp) return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    if (mag==NkFilter::NK_LINEAR&&min==NkFilter::NK_LINEAR&&mip==NkMipFilter::NK_LINEAR) return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if (mag==NkFilter::NK_NEAREST&&min==NkFilter::NK_NEAREST&&mip==NkMipFilter::NK_NEAREST) return D3D11_FILTER_MIN_MAG_MIP_POINT;
    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}
D3D11_TEXTURE_ADDRESS_MODE NkDeviceDX11::ToDX11Address(NkAddressMode a) {
    switch(a) {
        case NkAddressMode::NK_REPEAT:         return D3D11_TEXTURE_ADDRESS_WRAP;
        case NkAddressMode::NK_MIRRORED_REPEAT: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case NkAddressMode::NK_CLAMP_TO_EDGE:    return D3D11_TEXTURE_ADDRESS_CLAMP;
        case NkAddressMode::NK_CLAMP_TO_BORDER:  return D3D11_TEXTURE_ADDRESS_BORDER;
        default:                            return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}
D3D11_COMPARISON_FUNC NkDeviceDX11::ToDX11Compare(NkCompareOp op) {
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
D3D11_BLEND NkDeviceDX11::ToDX11Blend(NkBlendFactor f) {
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
D3D11_BLEND_OP NkDeviceDX11::ToDX11BlendOp(NkBlendOp op) {
    switch(op) {
        case NkBlendOp::NK_ADD:    return D3D11_BLEND_OP_ADD;
        case NkBlendOp::NK_SUB:    return D3D11_BLEND_OP_SUBTRACT;
        case NkBlendOp::NK_REV_SUB: return D3D11_BLEND_OP_REV_SUBTRACT;
        case NkBlendOp::NK_MIN:    return D3D11_BLEND_OP_MIN;
        case NkBlendOp::NK_MAX:    return D3D11_BLEND_OP_MAX;
        default:                return D3D11_BLEND_OP_ADD;
    }
}
D3D11_STENCIL_OP NkDeviceDX11::ToDX11StencilOp(NkStencilOp op) {
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
D3D11_PRIMITIVE_TOPOLOGY NkDeviceDX11::ToDX11Topology(NkPrimitiveTopology t) {
    switch(t) {
        case NkPrimitiveTopology::NK_TRIANGLE_LIST:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case NkPrimitiveTopology::NK_TRIANGLE_STRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case NkPrimitiveTopology::NK_LINE_LIST:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case NkPrimitiveTopology::NK_LINE_STRIP:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case NkPrimitiveTopology::NK_POINT_LIST:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:                                 return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

// =============================================================================
// Swapchain (NkISwapchain lifecycle) — NkDX11Swapchain
// =============================================================================
NkISwapchain* NkDeviceDX11::CreateSwapchain(NkIGraphicsContext* ctx,
                                              const NkSwapchainDesc& desc)
{
    auto* sc = new NkDX11Swapchain(this);
    if (!sc->Initialize(ctx, desc)) { delete sc; return nullptr; }
    return sc;
}
void NkDeviceDX11::DestroySwapchain(NkISwapchain*& sc) {
    if (!sc) return;
    sc->Shutdown(); delete sc; sc = nullptr;
}

// =============================================================================
// Semaphores D3D11
// D3D11 n'a pas de semaphores GPU-GPU natifs (contrairement à Vulkan).
// On émule via ID3D11Query (EVENT query) qui permet une synchronisation CPU-GPU.
// Pour une sync GPU-GPU stricte entre queues, utiliser D3D11_1 avec des fences.
// =============================================================================
NkSemaphoreHandle NkDeviceDX11::CreateGpuSemaphore() {
    // D3D11.1 : ID3D11DeviceContext1 n'a pas de sémaphore GPU-GPU natif.
    // On retourne un handle factice non-null pour la compatibilité API.
    // La synchronisation réelle est assurée par le pipeline GPU sérialisé de D3D11.
    uint64 hid = NextId();
    // Stocker un marqueur dans la map de fences existante (cpu-side token)
    NkDX11FenceObj fence; fence.signaled = true; fence.cpuValue = hid;
    mFences[hid] = fence;
    NkSemaphoreHandle h; h.id = hid; return h;
}
void NkDeviceDX11::DestroySemaphore(NkSemaphoreHandle& h) {
    // Retirer le marqueur de la map fences
    if (h.IsValid()) mFences.Erase(h.id);
    h.id = 0;
}

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED



