#pragma once
// =============================================================================
// NkTexture.h  —  Ressource texture RefCounted
// =============================================================================
#include "NKRenderer/Core/NkRef.h"
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace render {

class NkRenderDevice; // forward

// =============================================================================
// Descripteur de sampler
// =============================================================================
struct NkSamplerDesc {
    NkFilterMode filterMag    = NkFilterMode::NK_LINEAR;
    NkFilterMode filterMin    = NkFilterMode::NK_LINEAR;
    NkMipFilter  filterMip    = NkMipFilter::NK_LINEAR;
    NkWrapMode   wrapU        = NkWrapMode::NK_REPEAT;
    NkWrapMode   wrapV        = NkWrapMode::NK_REPEAT;
    NkWrapMode   wrapW        = NkWrapMode::NK_REPEAT;
    float32      maxAnisotropy= 1.f;
    float32      minLod       = -1000.f;
    float32      maxLod       =  1000.f;
    float32      mipBias      = 0.f;
    NkColorF     borderColor  = NkColorF::Transparent();
    bool         compareMode  = false;  // true = shadow map PCF
    NkDepthOp    compareOp    = NkDepthOp::NK_LESS_EQUAL;

    static NkSamplerDesc Linear()         { return {}; }
    static NkSamplerDesc Nearest()        { NkSamplerDesc d; d.filterMag=d.filterMin=NkFilterMode::NK_NEAREST; d.filterMip=NkMipFilter::NK_NEAREST; return d; }
    static NkSamplerDesc Anisotropic(float32 n=16.f) { NkSamplerDesc d; d.filterMag=d.filterMin=NkFilterMode::NK_ANISOTROPIC; d.maxAnisotropy=n; return d; }
    static NkSamplerDesc Clamp()          { NkSamplerDesc d; d.wrapU=d.wrapV=d.wrapW=NkWrapMode::NK_CLAMP; return d; }
    static NkSamplerDesc Shadow()         { NkSamplerDesc d; d.wrapU=d.wrapV=d.wrapW=NkWrapMode::NK_CLAMP; d.compareMode=true; return d; }
    static NkSamplerDesc ClampLinear()    { NkSamplerDesc d; d.wrapU=d.wrapV=d.wrapW=NkWrapMode::NK_CLAMP; d.filterMip=NkMipFilter::NK_NONE; d.minLod=d.maxLod=0.f; return d; }
};

// =============================================================================
// Source des données
// =============================================================================
enum class NkTextureSrc : uint32 {
    NK_EMPTY,         // Pas de données (render target, à allouer)
    NK_RAW_PIXELS,    // Pointeur mémoire + dimensions
    NK_FILE_PATH,     // Chemin fichier (NkImage LoadSTB)
    NK_RENDER_TARGET, // Color render target
    NK_DEPTH_STENCIL, // Depth/stencil target
};

// =============================================================================
// Descripteur complet
// =============================================================================
struct NkTextureDesc {
    // Dimensions
    uint32        width    = 1;
    uint32        height   = 1;
    uint32        depth    = 1;
    uint32        layers   = 1;
    uint32        mipLevels= 1;   // 0 = auto toute la chaîne
    // Format & type
    NkPixelFormat format   = NkPixelFormat::NK_RGBA8_SRGB;
    NkTextureKind kind     = NkTextureKind::NK_2D;
    NkMSAA        msaa     = NkMSAA::NK_1X;
    // Source
    NkTextureSrc  source   = NkTextureSrc::NK_EMPTY;
    const void*   pixels   = nullptr;
    uint32        rowPitch = 0;   // 0 = tight-packed
    NkString      filePath;
    bool          srgb         = true;
    bool          generateMips = false;
    // Sampler intégré
    NkSamplerDesc sampler;
    // Debug
    const char*   debugName = nullptr;

    // ── Factory helpers ──────────────────────────────────────────────────────
    static NkTextureDesc Tex2D(uint32 w, uint32 h,
                                NkPixelFormat fmt=NkPixelFormat::NK_RGBA8_SRGB,
                                uint32 mips=1) {
        NkTextureDesc d; d.width=w; d.height=h; d.format=fmt; d.mipLevels=mips;
        d.kind=NkTextureKind::NK_2D; return d;
    }
    static NkTextureDesc FromFile(const char* path,bool srgb=true,bool mips=true) {
        NkTextureDesc d; d.source=NkTextureSrc::NK_FILE_PATH; d.filePath=path;
        d.srgb=srgb; d.generateMips=mips; d.mipLevels=0; return d;
    }
    static NkTextureDesc FromMemory(const void* px,uint32 w,uint32 h,
                                     NkPixelFormat fmt=NkPixelFormat::NK_RGBA8_UNORM,
                                     uint32 pitch=0) {
        NkTextureDesc d; d.source=NkTextureSrc::NK_RAW_PIXELS; d.pixels=px;
        d.width=w; d.height=h; d.format=fmt; d.rowPitch=pitch; return d;
    }
    static NkTextureDesc RenderTarget(uint32 w,uint32 h,
                                       NkPixelFormat fmt=NkPixelFormat::NK_RGBA16F,
                                       NkMSAA msaa=NkMSAA::NK_1X) {
        NkTextureDesc d; d.kind=NkTextureKind::NK_RENDER_TARGET;
        d.source=NkTextureSrc::NK_RENDER_TARGET;
        d.width=w; d.height=h; d.format=fmt; d.msaa=msaa; d.mipLevels=1; return d;
    }
    static NkTextureDesc DepthStencil(uint32 w,uint32 h,
                                       NkPixelFormat fmt=NkPixelFormat::NK_D32F,
                                       NkMSAA msaa=NkMSAA::NK_1X) {
        NkTextureDesc d; d.kind=NkTextureKind::NK_DEPTH_STENCIL;
        d.source=NkTextureSrc::NK_DEPTH_STENCIL;
        d.width=w; d.height=h; d.format=fmt; d.msaa=msaa; d.mipLevels=1;
        d.sampler=NkSamplerDesc::Shadow(); return d;
    }
    static NkTextureDesc Cubemap(uint32 sz,NkPixelFormat fmt=NkPixelFormat::NK_RGBA16F,uint32 mips=0) {
        NkTextureDesc d; d.kind=NkTextureKind::NK_CUBE; d.width=d.height=sz;
        d.layers=6; d.format=fmt; d.mipLevels=mips; return d;
    }
    // Fallbacks 1×1
    static NkTextureDesc White1x1() {
        static const uint8 px[4]={255,255,255,255};
        auto d=FromMemory(px,1,1,NkPixelFormat::NK_RGBA8); d.srgb=false; d.debugName="White1x1"; return d;
    }
    static NkTextureDesc Black1x1() {
        static const uint8 px[4]={0,0,0,255};
        auto d=FromMemory(px,1,1,NkPixelFormat::NK_RGBA8); d.srgb=false; d.debugName="Black1x1"; return d;
    }
    static NkTextureDesc FlatNormal1x1() {
        static const uint8 px[4]={128,128,255,255};
        auto d=FromMemory(px,1,1,NkPixelFormat::NK_RGBA8); d.srgb=false; d.debugName="FlatNormal"; return d;
    }
    static NkTextureDesc ORM_Default1x1() {
        // Occlusion=1, Roughness=0.5, Metallic=0
        static const uint8 px[4]={255,128,0,255};
        auto d=FromMemory(px,1,1,NkPixelFormat::NK_RGBA8); d.srgb=false; d.debugName="ORM_Default"; return d;
    }
};

// =============================================================================
// NkTexture — ressource GPU RefCounted
// =============================================================================
class NkTexture : public NkRefCounted {
public:
    // Créé UNIQUEMENT via NkRenderDevice::CreateTexture(...)
    // Ne pas appeler le constructeur directement.

    const NkTextureDesc& Desc()     const { return mDesc; }
    uint32               Width()    const { return mDesc.width; }
    uint32               Height()   const { return mDesc.height; }
    uint32               Depth()    const { return mDesc.depth; }
    uint32               Layers()   const { return mDesc.layers; }
    uint32               MipLevels()const { return mDesc.mipLevels; }
    NkPixelFormat        Format()   const { return mDesc.format; }
    NkTextureKind        Kind()     const { return mDesc.kind; }
    NkMSAA               MSAA()     const { return mDesc.msaa; }
    const char*          Name()     const { return mDesc.debugName ? mDesc.debugName : ""; }
    bool                 IsRenderTarget()  const { return mDesc.kind == NkTextureKind::NK_RENDER_TARGET; }
    bool                 IsDepthStencil()  const { return mDesc.kind == NkTextureKind::NK_DEPTH_STENCIL; }
    uint64               GPUBytes()        const { return mGPUBytes; }

    // Accès interne RHI (opaque — uniquement pour les renderers internes)
    uint64 RHITextureId() const { return mRHITextureId; }
    uint64 RHISamplerId() const { return mRHISamplerId; }

protected:
    friend class NkRenderDevice;
    explicit NkTexture(const NkTextureDesc& desc)
        : mDesc(desc), mRHITextureId(0), mRHISamplerId(0), mGPUBytes(0) {}
    virtual void Destroy() override;

    NkTextureDesc mDesc;
    uint64        mRHITextureId = 0;
    uint64        mRHISamplerId = 0;
    uint64        mGPUBytes     = 0;
    NkRenderDevice* mDevice     = nullptr;
};

using NkTexturePtr = NkRef<NkTexture>;

} // namespace render
} // namespace nkentseu