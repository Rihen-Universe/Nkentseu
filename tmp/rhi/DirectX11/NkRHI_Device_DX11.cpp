// =============================================================================
// NkRHI_Device_DX11.cpp — Backend DirectX 11.1
// =============================================================================
#ifdef NK_RHI_DX11_ENABLED
#include "NkRHI_Device_DX11.h"
#include "NkCommandBuffer_DX11.h"
#include "../../NkFinal_work/Core/NkNativeContextAccess.h"
#include <cstdio>
#include <cstring>

#define NK_DX11_LOG(...)  printf("[NkRHI_DX11] " __VA_ARGS__)
#define NK_DX11_ERR(...)  printf("[NkRHI_DX11][ERR] " __VA_ARGS__)
#define NK_DX11_CHECK(hr, msg) do { if(FAILED(hr)) { NK_DX11_ERR(msg " (hr=0x%X)\n",(unsigned)(hr)); } } while(0)
#define NK_DX11_SAFE(p) if(p){(p)->Release();(p)=nullptr;}

namespace nkentseu {

NkDevice_DX11::~NkDevice_DX11() { if (mIsValid) Shutdown(); }

// =============================================================================
bool NkDevice_DX11::Initialize(NkIGraphicsContext* ctx) {
    if (!ctx || !ctx->IsValid()) return false;
    mCtx = ctx;

    auto* native = NkNativeContext::DX11(ctx);
    if (!native) { NK_DX11_ERR("Contexte non DX11\n"); return false; }

    mDevice  = static_cast<ID3D11Device1*>(native->device);
    mContext = static_cast<ID3D11DeviceContext1*>(native->context);
    mHwnd    = (HWND)native->hwnd;
    mWidth   = ctx->GetInfo().windowWidth;
    mHeight  = ctx->GetInfo().windowHeight;

    // Créer la factory DXGI
    CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&mFactory);
    CreateSwapchain(mWidth, mHeight);
    QueryCaps();

    mIsValid = true;
    NK_DX11_LOG("Initialisé (%u×%u)\n", mWidth, mHeight);
    return true;
}

void NkDevice_DX11::CreateSwapchain(uint32 w, uint32 h) {
    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width=w; scd.Height=h;
    scd.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc={1,0};
    scd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount=3;
    scd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    IDXGIDevice2* dxgiDev=nullptr;
    mDevice->QueryInterface(__uuidof(IDXGIDevice2),(void**)&dxgiDev);
    IDXGIAdapter* adapter=nullptr;
    dxgiDev->GetAdapter(&adapter);
    IDXGIFactory2* factory=nullptr;
    adapter->GetParent(__uuidof(IDXGIFactory2),(void**)&factory);
    factory->CreateSwapChainForHwnd(mDevice,mHwnd,&scd,nullptr,nullptr,&mSwapchain);
    NK_DX11_SAFE(factory); NK_DX11_SAFE(adapter); NK_DX11_SAFE(dxgiDev);

    // Back buffer texture
    ID3D11Texture2D* bb=nullptr;
    mSwapchain->GetBuffer(0,__uuidof(ID3D11Texture2D),(void**)&bb);
    uint64 colorId=NextId();
    NkDX11Texture swt; swt.tex=bb; swt.isSwapchain=true;
    swt.desc=NkTextureDesc::RenderTarget(w,h,NkFormat::BGRA8_Unorm);
    mDevice->CreateRenderTargetView(bb,nullptr,&swt.rtv);
    mTextures[colorId]=swt;
    NkTextureHandle colorH; colorH.id=colorId;

    // Depth
    NkTextureDesc dd=NkTextureDesc::DepthStencil(w,h);
    auto depthH=CreateTexture(dd);

    // Render pass
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkFormat::BGRA8_Unorm))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP=CreateRenderPass(rpd);

    NkFramebufferDesc fbd;
    fbd.renderPass=mSwapchainRP;
    fbd.colorAttachments[0]=colorH; fbd.colorCount=1;
    fbd.depthAttachment=depthH;
    fbd.width=w; fbd.height=h;
    mSwapchainFB=CreateFramebuffer(fbd);
}

void NkDevice_DX11::DestroySwapchain() {
    mContext->OMSetRenderTargets(0,nullptr,nullptr);
    DestroyFramebuffer(mSwapchainFB);
    DestroyRenderPass(mSwapchainRP);
    NK_DX11_SAFE(mSwapchain);
}

void NkDevice_DX11::ResizeSwapchain(uint32 w, uint32 h) {
    WaitIdle();
    DestroySwapchain();
    CreateSwapchain(w,h);
}

void NkDevice_DX11::Shutdown() {
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
    mIsValid=false;
    NK_DX11_LOG("Shutdown\n");
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDevice_DX11::CreateBuffer(const NkBufferDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth=(UINT)desc.sizeBytes;
    switch(desc.usage) {
        case NkResourceUsage::Upload:   bd.Usage=D3D11_USAGE_DYNAMIC; bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE; break;
        case NkResourceUsage::Readback: bd.Usage=D3D11_USAGE_STAGING; bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ; break;
        case NkResourceUsage::Immutable:bd.Usage=D3D11_USAGE_IMMUTABLE; break;
        default:                        bd.Usage=D3D11_USAGE_DEFAULT; break;
    }
    if (NkHasFlag(desc.bindFlags,NkBindFlags::VertexBuffer))   bd.BindFlags|=D3D11_BIND_VERTEX_BUFFER;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::IndexBuffer))    bd.BindFlags|=D3D11_BIND_INDEX_BUFFER;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::UniformBuffer))  bd.BindFlags|=D3D11_BIND_CONSTANT_BUFFER;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::StorageBuffer))  bd.BindFlags|=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::ShaderResource)) bd.BindFlags|=D3D11_BIND_SHADER_RESOURCE;

    // Type flags selon le type de buffer
    if (desc.type==NkBufferType::Vertex)  bd.BindFlags|=D3D11_BIND_VERTEX_BUFFER;
    if (desc.type==NkBufferType::Index)   bd.BindFlags|=D3D11_BIND_INDEX_BUFFER;
    if (desc.type==NkBufferType::Uniform) { bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER; }
    if (desc.type==NkBufferType::Storage) {
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

void NkDevice_DX11::DestroyBuffer(NkBufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mBuffers.find(h.id); if(it==mBuffers.end()) return;
    NK_DX11_SAFE(it->second.buf); mBuffers.erase(it); h.id=0;
}

bool NkDevice_DX11::WriteBuffer(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
    auto it=mBuffers.find(buf.id); if(it==mBuffers.end()) return false;
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (it->second.desc.usage==NkResourceUsage::Upload) {
        mContext->Map(it->second.buf,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
        memcpy((uint8*)ms.pData+off,data,(size_t)sz);
        mContext->Unmap(it->second.buf,0);
    } else {
        D3D11_BOX box{(UINT)off,0,0,(UINT)(off+sz),1,1};
        mContext->UpdateSubresource(it->second.buf,0,&box,data,0,0);
    }
    return true;
}

bool NkDevice_DX11::WriteBufferAsync(NkBufferHandle buf,const void* data,uint64 sz,uint64 off) {
    return WriteBuffer(buf,data,sz,off);
}

bool NkDevice_DX11::ReadBuffer(NkBufferHandle buf,void* out,uint64 sz,uint64 off) {
    auto it=mBuffers.find(buf.id); if(it==mBuffers.end()) return false;
    // Créer un staging buffer temporaire
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth=(UINT)sz; bd.Usage=D3D11_USAGE_STAGING;
    bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Buffer* staging=nullptr;
    mDevice->CreateBuffer(&bd,nullptr,&staging);
    D3D11_BOX box{(UINT)off,0,0,(UINT)(off+sz),1,1};
    mContext->CopySubresourceRegion(staging,0,0,0,0,it->second.buf,0,&box);
    D3D11_MAPPED_SUBRESOURCE ms{};
    mContext->Map(staging,0,D3D11_MAP_READ,0,&ms);
    memcpy(out,ms.pData,(size_t)sz);
    mContext->Unmap(staging,0);
    staging->Release();
    return true;
}

NkMappedMemory NkDevice_DX11::MapBuffer(NkBufferHandle buf,uint64 off,uint64 sz) {
    auto it=mBuffers.find(buf.id); if(it==mBuffers.end()) return {};
    D3D11_MAPPED_SUBRESOURCE ms{};
    mContext->Map(it->second.buf,0,D3D11_MAP_WRITE_DISCARD,0,&ms);
    uint64 mapSz=sz>0?sz:it->second.desc.sizeBytes-off;
    return {(uint8*)ms.pData+off,mapSz};
}
void NkDevice_DX11::UnmapBuffer(NkBufferHandle buf) {
    auto it=mBuffers.find(buf.id); if(it==mBuffers.end()) return;
    mContext->Unmap(it->second.buf,0);
}

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDevice_DX11::CreateTexture(const NkTextureDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    D3D11_TEXTURE2D_DESC td{};
    td.Width=desc.width; td.Height=desc.height;
    td.MipLevels=desc.mipLevels==0?0:desc.mipLevels;
    td.ArraySize=desc.arrayLayers;
    td.Format=ToDXGIFormat(desc.format);
    td.SampleDesc={(UINT)desc.samples,0};
    td.Usage=D3D11_USAGE_DEFAULT;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::ShaderResource))  td.BindFlags|=D3D11_BIND_SHADER_RESOURCE;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::RenderTarget))    td.BindFlags|=D3D11_BIND_RENDER_TARGET;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::DepthStencil))    td.BindFlags|=D3D11_BIND_DEPTH_STENCIL;
    if (NkHasFlag(desc.bindFlags,NkBindFlags::UnorderedAccess)) td.BindFlags|=D3D11_BIND_UNORDERED_ACCESS;
    if (td.MipLevels!=1 && (td.BindFlags&D3D11_BIND_RENDER_TARGET))
        td.MiscFlags|=D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if (desc.type==NkTextureType::Cube||desc.type==NkTextureType::CubeArray)
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
            sd.Format=desc.format==NkFormat::D32_Float?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        } else { sd.Format=td.Format; }
        if (desc.type==NkTextureType::Cube) {
            sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURECUBE;
            sd.TextureCube.MipLevels=td.MipLevels==0?(UINT)-1:td.MipLevels;
        } else {
            sd.ViewDimension=desc.samples>NkSampleCount::S1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MipLevels=td.MipLevels==0?(UINT)-1:td.MipLevels;
        }
        mDevice->CreateShaderResourceView(tex,&sd,&t.srv);
    }
    if (td.BindFlags & D3D11_BIND_RENDER_TARGET)
        mDevice->CreateRenderTargetView(tex,nullptr,&t.rtv);
    if (td.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
        dd.Format=desc.format==NkFormat::D32_Float?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D24_UNORM_S8_UINT;
        dd.ViewDimension=desc.samples>NkSampleCount::S1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
        mDevice->CreateDepthStencilView(tex,&dd,&t.dsv);
    }
    if (td.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
        mDevice->CreateUnorderedAccessView(tex,nullptr,&t.uav);

    uint64 hid=NextId(); mTextures[hid]=t;
    NkTextureHandle h; h.id=hid; return h;
}

void NkDevice_DX11::DestroyTexture(NkTextureHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mTextures.find(h.id); if(it==mTextures.end()) return;
    if (!it->second.isSwapchain) {
        NK_DX11_SAFE(it->second.tex); NK_DX11_SAFE(it->second.srv);
        NK_DX11_SAFE(it->second.rtv); NK_DX11_SAFE(it->second.dsv);
        NK_DX11_SAFE(it->second.uav);
    }
    mTextures.erase(it); h.id=0;
}

bool NkDevice_DX11::WriteTexture(NkTextureHandle t,const void* p,uint32 rp) {
    auto it=mTextures.find(t.id); if(it==mTextures.end()) return false;
    auto& desc=it->second.desc;
    uint32 pitch=rp>0?rp:desc.width*NkFormatBytesPerPixel(desc.format);
    mContext->UpdateSubresource(it->second.tex,0,nullptr,p,pitch,pitch*desc.height);
    return true;
}
bool NkDevice_DX11::WriteTextureRegion(NkTextureHandle t,const void* p,
    uint32 x,uint32 y,uint32 z,uint32 w,uint32 h,uint32 d2,
    uint32 mip,uint32 layer,uint32 rp) {
    auto it=mTextures.find(t.id); if(it==mTextures.end()) return false;
    auto& desc=it->second.desc;
    uint32 pitch=rp>0?rp:w*NkFormatBytesPerPixel(desc.format);
    D3D11_BOX box{x,y,z,x+w,y+h,z+d2};
    uint32 sub=D3D11CalcSubresource(mip,layer,desc.mipLevels?desc.mipLevels:1);
    mContext->UpdateSubresource(it->second.tex,sub,&box,p,pitch,pitch*h);
    return true;
}
bool NkDevice_DX11::GenerateMipmaps(NkTextureHandle t, NkFilter) {
    auto it=mTextures.find(t.id); if(it==mTextures.end()) return false;
    if (it->second.srv) mContext->GenerateMips(it->second.srv);
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDevice_DX11::CreateSampler(const NkSamplerDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
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
void NkDevice_DX11::DestroySampler(NkSamplerHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mSamplers.find(h.id); if(it==mSamplers.end()) return;
    NK_DX11_SAFE(it->second.ss); mSamplers.erase(it); h.id=0;
}

// =============================================================================
// Shaders (compilation HLSL runtime avec D3DCompile)
// =============================================================================
NkShaderHandle NkDevice_DX11::CreateShader(const NkShaderDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkDX11Shader sh;
    for (uint32 i=0;i<desc.stageCount;i++) {
        auto& s=desc.stages[i];
        const char* src = s.hlslSource;
        if (!src) { NK_DX11_ERR("DX11 shader: manque le source HLSL (stage %d)\n",i); continue; }

        const char* target=nullptr; const char* entry=s.entryPoint?s.entryPoint:"main";
        switch(s.stage) {
            case NkShaderStage::Vertex:   target="vs_5_0"; break;
            case NkShaderStage::Fragment: target="ps_5_0"; break;
            case NkShaderStage::Compute:  target="cs_5_0"; break;
            case NkShaderStage::Geometry: target="gs_5_0"; break;
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
            case NkShaderStage::Vertex:
                mDevice->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.vs);
                sh.vsBlob=code; code=nullptr; // garder le blob pour InputLayout
                break;
            case NkShaderStage::Fragment:
                mDevice->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.ps);
                break;
            case NkShaderStage::Compute:
                mDevice->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&sh.cs);
                break;
            case NkShaderStage::Geometry:
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
void NkDevice_DX11::DestroyShader(NkShaderHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mShaders.find(h.id); if(it==mShaders.end()) return;
    NK_DX11_SAFE(it->second.vs); NK_DX11_SAFE(it->second.ps);
    NK_DX11_SAFE(it->second.cs); NK_DX11_SAFE(it->second.gs);
    NK_DX11_SAFE(it->second.vsBlob);
    mShaders.erase(it); h.id=0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDevice_DX11::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit=mShaders.find(d.shader.id); if(sit==mShaders.end()) return {};
    auto& sh=sit->second;

    NkDX11Pipeline p;
    p.vs=sh.vs; p.ps=sh.ps;
    p.topology=ToDX11Topology(d.topology);
    p.isCompute=false;

    // Input Layout
    if (sh.vsBlob && d.vertexLayout.attributeCount>0) {
        std::vector<D3D11_INPUT_ELEMENT_DESC> elems(d.vertexLayout.attributeCount);
        static const DXGI_FORMAT fmtTable[]={
            DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,
            DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_SNORM,
            DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM,
            DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32B32A32_UINT,
            DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32B32A32_SINT,
        };
        for (uint32 i=0;i<d.vertexLayout.attributeCount;i++) {
            auto& a=d.vertexLayout.attributes[i];
            elems[i].SemanticName=a.semanticName?a.semanticName:"TEXCOORD";
            elems[i].SemanticIndex=a.semanticIdx;
            elems[i].Format=fmtTable[std::min((uint32)a.format,15u)];
            elems[i].InputSlot=a.binding;
            elems[i].AlignedByteOffset=a.offset;
            // Trouver le binding pour savoir si c'est instancé
            bool instanced=false;
            for (uint32 j=0;j<d.vertexLayout.bindingCount;j++)
                if (d.vertexLayout.bindings[j].binding==a.binding) { instanced=d.vertexLayout.bindings[j].perInstance; break; }
            elems[i].InputSlotClass=instanced?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
            elems[i].InstanceDataStepRate=instanced?1:0;
        }
        mDevice->CreateInputLayout(elems.data(),(UINT)elems.size(),
            sh.vsBlob->GetBufferPointer(),sh.vsBlob->GetBufferSize(),&p.il);
    }

    // Rasterizer state
    D3D11_RASTERIZER_DESC1 rd{};
    rd.FillMode=d.rasterizer.fillMode==NkFillMode::Solid?D3D11_FILL_SOLID:D3D11_FILL_WIREFRAME;
    rd.CullMode=d.rasterizer.cullMode==NkCullMode::None?D3D11_CULL_NONE:d.rasterizer.cullMode==NkCullMode::Front?D3D11_CULL_FRONT:D3D11_CULL_BACK;
    rd.FrontCounterClockwise=d.rasterizer.frontFace==NkFrontFace::CCW;
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
    bsd.IndependentBlendEnable=d.blend.attachmentCount>1;
    for (uint32 i=0;i<d.blend.attachmentCount;i++) {
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

NkPipelineHandle NkDevice_DX11::CreateComputePipeline(const NkComputePipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit=mShaders.find(d.shader.id); if(sit==mShaders.end()) return {};
    NkDX11Pipeline p; p.cs=sit->second.cs; p.isCompute=true;
    uint64 hid=NextId(); mPipelines[hid]=p;
    NkPipelineHandle h; h.id=hid; return h;
}

void NkDevice_DX11::DestroyPipeline(NkPipelineHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it=mPipelines.find(h.id); if(it==mPipelines.end()) return;
    NK_DX11_SAFE(it->second.il); NK_DX11_SAFE(it->second.rs);
    NK_DX11_SAFE(it->second.dss); NK_DX11_SAFE(it->second.bs);
    mPipelines.erase(it); h.id=0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle  NkDevice_DX11::CreateRenderPass(const NkRenderPassDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid=NextId(); mRenderPasses[hid]={d};
    NkRenderPassHandle h; h.id=hid; return h;
}
void NkDevice_DX11::DestroyRenderPass(NkRenderPassHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mRenderPasses.erase(h.id); h.id=0;
}

NkFramebufferHandle NkDevice_DX11::CreateFramebuffer(const NkFramebufferDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkDX11Framebuffer fb; fb.w=d.width; fb.h=d.height;
    for (uint32 i=0;i<d.colorCount;i++) {
        auto it=mTextures.find(d.colorAttachments[i].id);
        if (it!=mTextures.end()) fb.rtvs[fb.rtvCount++]=it->second.rtv;
    }
    if (d.depthAttachment.IsValid()) {
        auto it=mTextures.find(d.depthAttachment.id);
        if (it!=mTextures.end()) fb.dsv=it->second.dsv;
    }
    uint64 hid=NextId(); mFramebuffers[hid]=fb;
    NkFramebufferHandle h; h.id=hid; return h;
}
void NkDevice_DX11::DestroyFramebuffer(NkFramebufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mFramebuffers.erase(h.id); h.id=0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDevice_DX11::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid=NextId(); mDescLayouts[hid]={d};
    NkDescSetHandle h; h.id=hid; return h;
}
void NkDevice_DX11::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescLayouts.erase(h.id); h.id=0;
}
NkDescSetHandle NkDevice_DX11::AllocateDescriptorSet(NkDescSetHandle layoutHandle) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkDX11DescSet ds; ds.layoutId=layoutHandle.id;
    uint64 hid=NextId(); mDescSets[hid]=ds;
    NkDescSetHandle h; h.id=hid; return h;
}
void NkDevice_DX11::FreeDescriptorSet(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescSets.erase(h.id); h.id=0;
}

void NkDevice_DX11::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (uint32 i=0;i<n;i++) {
        auto& w=writes[i];
        auto sit=mDescSets.find(w.set.id); if(sit==mDescSets.end()) continue;
        auto& ds=sit->second;
        // Trouver ou créer le slot
        NkDX11DescSet::Slot* slot=nullptr;
        for (uint32 j=0;j<ds.count;j++) if(ds.slots[j].slot==w.binding) { slot=&ds.slots[j]; break; }
        if (!slot) { slot=&ds.slots[ds.count++]; slot->slot=w.binding; }
        slot->type=w.type;
        if (w.buffer.IsValid()) {
            auto bit=mBuffers.find(w.buffer.id);
            if (bit!=mBuffers.end()) { slot->kind=NkDX11DescSet::Slot::Buffer; slot->buf=bit->second.buf; }
        }
        if (w.texture.IsValid()) {
            auto tit=mTextures.find(w.texture.id);
            if (tit!=mTextures.end()) {
                slot->kind=NkDX11DescSet::Slot::Texture;
                slot->srv=tit->second.srv;
                slot->uav=tit->second.uav;
            }
        }
        if (w.sampler.IsValid()) {
            auto sit2=mSamplers.find(w.sampler.id);
            if (sit2!=mSamplers.end()) { slot->kind=NkDX11DescSet::Slot::Sampler; slot->ss=sit2->second.ss; }
        }
    }
}

void NkDevice_DX11::ApplyDescSet(uint64 id) {
    auto it=mDescSets.find(id); if(it==mDescSets.end()) return;
    for (uint32 i=0;i<it->second.count;i++) {
        auto& s=it->second.slots[i];
        switch(s.kind) {
            case NkDX11DescSet::Slot::Buffer:
                if (s.type==NkDescriptorType::UniformBuffer) {
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

void NkDevice_DX11::ApplyPipeline(uint64 id) {
    auto it=mPipelines.find(id); if(it==mPipelines.end()) return;
    auto& p=it->second;
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
NkICommandBuffer* NkDevice_DX11::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_DX11(this, t);
}
void NkDevice_DX11::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb=nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDevice_DX11::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i=0;i<n;i++) {
        auto* dx=dynamic_cast<NkCommandBuffer_DX11*>(cbs[i]);
        if (dx) dx->Execute(this);
    }
    if (fence.IsValid()) {
        auto it=mFences.find(fence.id);
        if (it!=mFences.end()) it->second.signaled=true;
    }
}
void NkDevice_DX11::SubmitAndPresent(NkICommandBuffer* cb) {
    Submit(&cb,1,{});
    UINT flags = 0;
    mSwapchain->Present(0, flags);
}

// Fence (émulation CPU pour DX11)
NkFenceHandle NkDevice_DX11::CreateFence(bool signaled) {
    uint64 hid=NextId(); mFences[hid]={0,signaled};
    NkFenceHandle h; h.id=hid; return h;
}
void NkDevice_DX11::DestroyFence(NkFenceHandle& h) { mFences.erase(h.id); h.id=0; }
bool NkDevice_DX11::WaitFence(NkFenceHandle f,uint64) {
    auto it=mFences.find(f.id); if(it==mFences.end()) return false;
    return it->second.signaled;
}
bool NkDevice_DX11::IsFenceSignaled(NkFenceHandle f) {
    auto it=mFences.find(f.id); return it!=mFences.end()&&it->second.signaled;
}
void NkDevice_DX11::ResetFence(NkFenceHandle f) {
    auto it=mFences.find(f.id); if(it!=mFences.end()) it->second.signaled=false;
}
void NkDevice_DX11::WaitIdle() { mContext->Flush(); }

// Frame
void NkDevice_DX11::BeginFrame(NkFrameContext& frame) {
    frame.frameIndex=mFrameIndex; frame.frameNumber=mFrameNumber;
}
void NkDevice_DX11::EndFrame(NkFrameContext&) {
    mFrameIndex=(mFrameIndex+1)%kMaxFrames; ++mFrameNumber;
}
void NkDevice_DX11::OnResize(uint32 w, uint32 h) {
    if (w==0||h==0) return;
    mWidth=w; mHeight=h; ResizeSwapchain(w,h);
}

// =============================================================================
// Caps
// =============================================================================
void NkDevice_DX11::QueryCaps() {
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
ID3D11Buffer* NkDevice_DX11::GetDXBuffer(uint64 id) const { auto it=mBuffers.find(id); return it!=mBuffers.end()?it->second.buf:nullptr; }
ID3D11ShaderResourceView* NkDevice_DX11::GetSRV(uint64 id) const { auto it=mTextures.find(id); return it!=mTextures.end()?it->second.srv:nullptr; }
ID3D11UnorderedAccessView* NkDevice_DX11::GetUAV(uint64 id) const { auto it=mTextures.find(id); return it!=mTextures.end()?it->second.uav:nullptr; }
const NkDX11Pipeline* NkDevice_DX11::GetPipeline(uint64 id) const { auto it=mPipelines.find(id); return it!=mPipelines.end()?&it->second:nullptr; }
const NkDX11DescSet*  NkDevice_DX11::GetDescSet(uint64 id) const { auto it=mDescSets.find(id); return it!=mDescSets.end()?&it->second:nullptr; }
const NkDX11Framebuffer* NkDevice_DX11::GetFBO(uint64 id) const { auto it=mFramebuffers.find(id); return it!=mFramebuffers.end()?&it->second:nullptr; }

// =============================================================================
// Conversions
// =============================================================================
DXGI_FORMAT NkDevice_DX11::ToDXGIFormat(NkFormat f, bool) {
    switch(f) {
        case NkFormat::R8_Unorm:      return DXGI_FORMAT_R8_UNORM;
        case NkFormat::RG8_Unorm:     return DXGI_FORMAT_R8G8_UNORM;
        case NkFormat::RGBA8_Unorm:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case NkFormat::RGBA8_Srgb:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case NkFormat::BGRA8_Unorm:   return DXGI_FORMAT_B8G8R8A8_UNORM;
        case NkFormat::BGRA8_Srgb:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case NkFormat::R16_Float:     return DXGI_FORMAT_R16_FLOAT;
        case NkFormat::RG16_Float:    return DXGI_FORMAT_R16G16_FLOAT;
        case NkFormat::RGBA16_Float:  return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case NkFormat::R32_Float:     return DXGI_FORMAT_R32_FLOAT;
        case NkFormat::RG32_Float:    return DXGI_FORMAT_R32G32_FLOAT;
        case NkFormat::RGB32_Float:   return DXGI_FORMAT_R32G32B32_FLOAT;
        case NkFormat::RGBA32_Float:  return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case NkFormat::R32_Uint:      return DXGI_FORMAT_R32_UINT;
        case NkFormat::RG32_Uint:     return DXGI_FORMAT_R32G32_UINT;
        case NkFormat::D16_Unorm:     return DXGI_FORMAT_D16_UNORM;
        case NkFormat::D32_Float:     return DXGI_FORMAT_D32_FLOAT;
        case NkFormat::D24_Unorm_S8_Uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case NkFormat::BC1_RGB_Unorm: return DXGI_FORMAT_BC1_UNORM;
        case NkFormat::BC3_Unorm:     return DXGI_FORMAT_BC3_UNORM;
        case NkFormat::BC5_Unorm:     return DXGI_FORMAT_BC5_UNORM;
        case NkFormat::BC7_Unorm:     return DXGI_FORMAT_BC7_UNORM;
        case NkFormat::R11G11B10_Float: return DXGI_FORMAT_R11G11B10_FLOAT;
        default:                      return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}
D3D11_FILTER NkDevice_DX11::ToDX11Filter(NkFilter mag, NkFilter min, NkMipFilter mip, bool cmp) {
    bool aniso = false; // détection simplifiée
    if (cmp) return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    if (mag==NkFilter::Linear&&min==NkFilter::Linear&&mip==NkMipFilter::Linear) return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    if (mag==NkFilter::Nearest&&min==NkFilter::Nearest&&mip==NkMipFilter::Nearest) return D3D11_FILTER_MIN_MAG_MIP_POINT;
    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
}
D3D11_TEXTURE_ADDRESS_MODE NkDevice_DX11::ToDX11Address(NkAddressMode a) {
    switch(a) {
        case NkAddressMode::Repeat:         return D3D11_TEXTURE_ADDRESS_WRAP;
        case NkAddressMode::MirroredRepeat: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case NkAddressMode::ClampToEdge:    return D3D11_TEXTURE_ADDRESS_CLAMP;
        case NkAddressMode::ClampToBorder:  return D3D11_TEXTURE_ADDRESS_BORDER;
        default:                            return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}
D3D11_COMPARISON_FUNC NkDevice_DX11::ToDX11Compare(NkCompareOp op) {
    switch(op) {
        case NkCompareOp::Never:        return D3D11_COMPARISON_NEVER;
        case NkCompareOp::Less:         return D3D11_COMPARISON_LESS;
        case NkCompareOp::Equal:        return D3D11_COMPARISON_EQUAL;
        case NkCompareOp::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case NkCompareOp::Greater:      return D3D11_COMPARISON_GREATER;
        case NkCompareOp::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case NkCompareOp::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        default:                        return D3D11_COMPARISON_ALWAYS;
    }
}
D3D11_BLEND NkDevice_DX11::ToDX11Blend(NkBlendFactor f) {
    switch(f) {
        case NkBlendFactor::Zero:              return D3D11_BLEND_ZERO;
        case NkBlendFactor::One:               return D3D11_BLEND_ONE;
        case NkBlendFactor::SrcColor:          return D3D11_BLEND_SRC_COLOR;
        case NkBlendFactor::OneMinusSrcColor:  return D3D11_BLEND_INV_SRC_COLOR;
        case NkBlendFactor::SrcAlpha:          return D3D11_BLEND_SRC_ALPHA;
        case NkBlendFactor::OneMinusSrcAlpha:  return D3D11_BLEND_INV_SRC_ALPHA;
        case NkBlendFactor::DstAlpha:          return D3D11_BLEND_DEST_ALPHA;
        case NkBlendFactor::OneMinusDstAlpha:  return D3D11_BLEND_INV_DEST_ALPHA;
        case NkBlendFactor::SrcAlphaSaturate:  return D3D11_BLEND_SRC_ALPHA_SAT;
        default:                               return D3D11_BLEND_ONE;
    }
}
D3D11_BLEND_OP NkDevice_DX11::ToDX11BlendOp(NkBlendOp op) {
    switch(op) {
        case NkBlendOp::Add:    return D3D11_BLEND_OP_ADD;
        case NkBlendOp::Sub:    return D3D11_BLEND_OP_SUBTRACT;
        case NkBlendOp::RevSub: return D3D11_BLEND_OP_REV_SUBTRACT;
        case NkBlendOp::Min:    return D3D11_BLEND_OP_MIN;
        case NkBlendOp::Max:    return D3D11_BLEND_OP_MAX;
        default:                return D3D11_BLEND_OP_ADD;
    }
}
D3D11_STENCIL_OP NkDevice_DX11::ToDX11StencilOp(NkStencilOp op) {
    switch(op) {
        case NkStencilOp::Keep:      return D3D11_STENCIL_OP_KEEP;
        case NkStencilOp::Zero:      return D3D11_STENCIL_OP_ZERO;
        case NkStencilOp::Replace:   return D3D11_STENCIL_OP_REPLACE;
        case NkStencilOp::IncrClamp: return D3D11_STENCIL_OP_INCR_SAT;
        case NkStencilOp::DecrClamp: return D3D11_STENCIL_OP_DECR_SAT;
        case NkStencilOp::Invert:    return D3D11_STENCIL_OP_INVERT;
        case NkStencilOp::IncrWrap:  return D3D11_STENCIL_OP_INCR;
        case NkStencilOp::DecrWrap:  return D3D11_STENCIL_OP_DECR;
        default:                     return D3D11_STENCIL_OP_KEEP;
    }
}
D3D11_PRIMITIVE_TOPOLOGY NkDevice_DX11::ToDX11Topology(NkPrimitiveTopology t) {
    switch(t) {
        case NkPrimitiveTopology::TriangleList:  return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case NkPrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case NkPrimitiveTopology::LineList:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case NkPrimitiveTopology::LineStrip:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case NkPrimitiveTopology::PointList:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:                                 return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

} // namespace nkentseu
#endif // NK_RHI_DX11_ENABLED
