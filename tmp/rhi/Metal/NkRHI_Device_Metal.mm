// =============================================================================
// NkRHI_Device_Metal.mm — Backend Apple Metal
// Compilé en Objective-C++ (.mm)
// =============================================================================
#ifdef NK_RHI_METAL_ENABLED
#import "NkRHI_Device_Metal.h"
#import "NkCommandBuffer_Metal.h"
#import "../../NkFinal_work/Core/NkNativeContextAccess.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <simd/simd.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

#define NK_MTL_LOG(...)  printf("[NkRHI_Metal] " __VA_ARGS__)
#define NK_MTL_ERR(...)  printf("[NkRHI_Metal][ERR] " __VA_ARGS__)

namespace nkentseu {

// =============================================================================
static MTLPixelFormat ToMTLFormat(NkFormat f) {
    switch (f) {
        case NkFormat::R8_Unorm:         return MTLPixelFormatR8Unorm;
        case NkFormat::RG8_Unorm:        return MTLPixelFormatRG8Unorm;
        case NkFormat::RGBA8_Unorm:      return MTLPixelFormatRGBA8Unorm;
        case NkFormat::RGBA8_Srgb:       return MTLPixelFormatRGBA8Unorm_sRGB;
        case NkFormat::BGRA8_Unorm:      return MTLPixelFormatBGRA8Unorm;
        case NkFormat::BGRA8_Srgb:       return MTLPixelFormatBGRA8Unorm_sRGB;
        case NkFormat::R16_Float:        return MTLPixelFormatR16Float;
        case NkFormat::RG16_Float:       return MTLPixelFormatRG16Float;
        case NkFormat::RGBA16_Float:     return MTLPixelFormatRGBA16Float;
        case NkFormat::R32_Float:        return MTLPixelFormatR32Float;
        case NkFormat::RG32_Float:       return MTLPixelFormatRG32Float;
        case NkFormat::RGBA32_Float:     return MTLPixelFormatRGBA32Float;
        case NkFormat::R32_Uint:         return MTLPixelFormatR32Uint;
        case NkFormat::D16_Unorm:        return MTLPixelFormatDepth16Unorm;
        case NkFormat::D32_Float:        return MTLPixelFormatDepth32Float;
        case NkFormat::D24_Unorm_S8_Uint:return MTLPixelFormatDepth24Unorm_Stencil8;
        case NkFormat::D32_Float_S8_Uint:return MTLPixelFormatDepth32Float_Stencil8;
        case NkFormat::BC1_RGB_Unorm:    return MTLPixelFormatBC1_RGBA;
        case NkFormat::BC3_Unorm:        return MTLPixelFormatBC3_RGBA;
        case NkFormat::BC5_Unorm:        return MTLPixelFormatBC5_RGUnorm;
        case NkFormat::BC7_Unorm:        return MTLPixelFormatBC7_RGBAUnorm;
        case NkFormat::R11G11B10_Float:  return MTLPixelFormatRG11B10Float;
        case NkFormat::A2B10G10R10_Unorm:return MTLPixelFormatBGR10A2Unorm;
        default:                         return MTLPixelFormatRGBA8Unorm;
    }
}
static MTLSamplerAddressMode ToMTLAddress(NkAddressMode a) {
    switch (a) {
        case NkAddressMode::Repeat:         return MTLSamplerAddressModeRepeat;
        case NkAddressMode::MirroredRepeat: return MTLSamplerAddressModeMirrorRepeat;
        case NkAddressMode::ClampToEdge:    return MTLSamplerAddressModeClampToEdge;
        case NkAddressMode::ClampToBorder:  return MTLSamplerAddressModeClampToBorderColor;
        default:                            return MTLSamplerAddressModeRepeat;
    }
}
static MTLSamplerMinMagFilter ToMTLFilter(NkFilter f) {
    return f == NkFilter::Nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}
static MTLSamplerMipFilter ToMTLMipFilter(NkMipFilter f) {
    switch (f) {
        case NkMipFilter::None:    return MTLSamplerMipFilterNotMipmapped;
        case NkMipFilter::Nearest: return MTLSamplerMipFilterNearest;
        default:                   return MTLSamplerMipFilterLinear;
    }
}
static MTLCompareFunction ToMTLCompare(NkCompareOp op) {
    switch (op) {
        case NkCompareOp::Never:        return MTLCompareFunctionNever;
        case NkCompareOp::Less:         return MTLCompareFunctionLess;
        case NkCompareOp::Equal:        return MTLCompareFunctionEqual;
        case NkCompareOp::LessEqual:    return MTLCompareFunctionLessEqual;
        case NkCompareOp::Greater:      return MTLCompareFunctionGreater;
        case NkCompareOp::NotEqual:     return MTLCompareFunctionNotEqual;
        case NkCompareOp::GreaterEqual: return MTLCompareFunctionGreaterEqual;
        default:                        return MTLCompareFunctionAlways;
    }
}
static MTLBlendFactor ToMTLBlend(NkBlendFactor f) {
    switch (f) {
        case NkBlendFactor::Zero:              return MTLBlendFactorZero;
        case NkBlendFactor::One:               return MTLBlendFactorOne;
        case NkBlendFactor::SrcColor:          return MTLBlendFactorSourceColor;
        case NkBlendFactor::OneMinusSrcColor:  return MTLBlendFactorOneMinusSourceColor;
        case NkBlendFactor::SrcAlpha:          return MTLBlendFactorSourceAlpha;
        case NkBlendFactor::OneMinusSrcAlpha:  return MTLBlendFactorOneMinusSourceAlpha;
        case NkBlendFactor::DstAlpha:          return MTLBlendFactorDestinationAlpha;
        case NkBlendFactor::OneMinusDstAlpha:  return MTLBlendFactorOneMinusDestinationAlpha;
        case NkBlendFactor::SrcAlphaSaturate:  return MTLBlendFactorSourceAlphaSaturated;
        default:                               return MTLBlendFactorOne;
    }
}
static MTLBlendOperation ToMTLBlendOp(NkBlendOp op) {
    switch (op) {
        case NkBlendOp::Add:    return MTLBlendOperationAdd;
        case NkBlendOp::Sub:    return MTLBlendOperationSubtract;
        case NkBlendOp::RevSub: return MTLBlendOperationReverseSubtract;
        case NkBlendOp::Min:    return MTLBlendOperationMin;
        case NkBlendOp::Max:    return MTLBlendOperationMax;
        default:                return MTLBlendOperationAdd;
    }
}
static MTLStencilOperation ToMTLStencilOp(NkStencilOp op) {
    switch (op) {
        case NkStencilOp::Keep:      return MTLStencilOperationKeep;
        case NkStencilOp::Zero:      return MTLStencilOperationZero;
        case NkStencilOp::Replace:   return MTLStencilOperationReplace;
        case NkStencilOp::IncrClamp: return MTLStencilOperationIncrementClamp;
        case NkStencilOp::DecrClamp: return MTLStencilOperationDecrementClamp;
        case NkStencilOp::Invert:    return MTLStencilOperationInvert;
        case NkStencilOp::IncrWrap:  return MTLStencilOperationIncrementWrap;
        case NkStencilOp::DecrWrap:  return MTLStencilOperationDecrementWrap;
        default:                     return MTLStencilOperationKeep;
    }
}
static MTLPrimitiveType ToMTLTopology(NkPrimitiveTopology t) {
    switch (t) {
        case NkPrimitiveTopology::TriangleList:  return MTLPrimitiveTypeTriangle;
        case NkPrimitiveTopology::TriangleStrip: return MTLPrimitiveTypeTriangleStrip;
        case NkPrimitiveTopology::LineList:      return MTLPrimitiveTypeLine;
        case NkPrimitiveTopology::LineStrip:     return MTLPrimitiveTypeLineStrip;
        case NkPrimitiveTopology::PointList:     return MTLPrimitiveTypePoint;
        default:                                 return MTLPrimitiveTypeTriangle;
    }
}
static MTLVertexFormat ToMTLVertexFormat(NkVertexFormat f) {
    switch (f) {
        case NkVertexFormat::Float1: return MTLVertexFormatFloat;
        case NkVertexFormat::Float2: return MTLVertexFormatFloat2;
        case NkVertexFormat::Float3: return MTLVertexFormatFloat3;
        case NkVertexFormat::Float4: return MTLVertexFormatFloat4;
        case NkVertexFormat::Half2:  return MTLVertexFormatHalf2;
        case NkVertexFormat::Half4:  return MTLVertexFormatHalf4;
        case NkVertexFormat::Byte4_Unorm: return MTLVertexFormatUChar4Normalized;
        case NkVertexFormat::Byte4_Snorm: return MTLVertexFormatChar4Normalized;
        case NkVertexFormat::Uint1:  return MTLVertexFormatUInt;
        case NkVertexFormat::Uint4:  return MTLVertexFormatUInt4;
        default:                     return MTLVertexFormatFloat3;
    }
}

// =============================================================================
NkDevice_Metal::~NkDevice_Metal() { if (mIsValid) Shutdown(); }

bool NkDevice_Metal::Initialize(NkIGraphicsContext* ctx) {
    if (!ctx || !ctx->IsValid()) return false;
    mCtx   = ctx;
    mWidth = ctx->GetInfo().windowWidth;
    mHeight= ctx->GetInfo().windowHeight;

    auto* native = NkNativeContext::Metal(ctx);
    if (!native) { NK_MTL_ERR("Contexte non Metal\n"); return false; }

    mDevice = (__bridge id<MTLDevice>)native->device;
    mLayer  = (__bridge CAMetalLayer*)native->layer;

    if (!mDevice) { NK_MTL_ERR("MTLDevice null\n"); return false; }

    mQueue = [mDevice newCommandQueue];
    mLayer.device          = mDevice;
    mLayer.pixelFormat     = MTLPixelFormatBGRA8Unorm_sRGB;
    mLayer.drawableSize    = CGSizeMake(mWidth, mHeight);

    CreateSwapchainObjects();
    QueryCaps();

    mIsValid = true;
    NK_MTL_LOG("Initialisé (%u×%u) sur %s\n", mWidth, mHeight,
               [mDevice.name UTF8String]);
    return true;
}

void NkDevice_Metal::CreateSwapchainObjects() {
    // Depth texture
    NkTextureDesc dd = NkTextureDesc::DepthStencil(mWidth, mHeight);
    mDepthTex = CreateTexture(dd);

    // Render pass metadata
    NkRenderPassDesc rpd;
    rpd.AddColor(NkAttachmentDesc::Color(NkFormat::BGRA8_Srgb))
       .SetDepth(NkAttachmentDesc::Depth());
    mSwapchainRP = CreateRenderPass(rpd);

    // Framebuffer swapchain (la color attachment sera remplacée à chaque frame)
    NkFramebufferDesc fbd;
    fbd.renderPass = mSwapchainRP;
    fbd.colorCount = 0; // sera remplie dans BeginFrame
    fbd.depthAttachment = mDepthTex;
    fbd.width = mWidth; fbd.height = mHeight;
    mSwapchainFB = CreateFramebuffer(fbd);
}

void NkDevice_Metal::Shutdown() {
    WaitIdle();
    for (auto& [id, b] : mBuffers)  [(__bridge id<MTLBuffer>)b.buf  setPurgeableState:MTLPurgeableStateEmpty];
    for (auto& [id, t] : mTextures) if (!t.isSwapchain) {} // ARC libère
    mBuffers.clear(); mTextures.clear(); mSamplers.clear();
    mShaders.clear(); mPipelines.clear(); mRenderPasses.clear();
    mFramebuffers.clear(); mDescLayouts.clear(); mDescSets.clear();
    mIsValid = false;
    NK_MTL_LOG("Shutdown\n");
}

// =============================================================================
// Buffers
// =============================================================================
NkBufferHandle NkDevice_Metal::CreateBuffer(const NkBufferDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    MTLResourceOptions opts = MTLResourceStorageModeShared; // CPU+GPU visible
    switch (desc.usage) {
        case NkResourceUsage::GpuOnly: opts = MTLResourceStorageModePrivate; break;
        default: break;
    }

    id<MTLBuffer> buf = [mDevice newBufferWithLength:(NSUInteger)desc.sizeBytes options:opts];
    if (!buf) return {};

    if (desc.initialData && opts != MTLResourceStorageModePrivate)
        memcpy(buf.contents, desc.initialData, (size_t)desc.sizeBytes);

    if (desc.debugName)
        buf.label = [NSString stringWithUTF8String:desc.debugName];

    uint64 hid = NextId();
    mBuffers[hid] = { (__bridge_retained void*)buf, desc };
    NkBufferHandle h; h.id = hid; return h;
}

void NkDevice_Metal::DestroyBuffer(NkBufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mBuffers.find(h.id); if (it == mBuffers.end()) return;
    if (it->second.buf) CFRelease(it->second.buf);
    mBuffers.erase(it); h.id = 0;
}

bool NkDevice_Metal::WriteBuffer(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    auto* b = (__bridge id<MTLBuffer>)it->second.buf;
    if (b.storageMode == MTLStorageModeShared) {
        memcpy((uint8*)b.contents + off, data, (size_t)sz);
        return true;
    }
    // GPU-only : upload via staging + blit
    id<MTLBuffer> stage = [mDevice newBufferWithLength:sz options:MTLResourceStorageModeShared];
    memcpy(stage.contents, data, (size_t)sz);
    id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromBuffer:stage sourceOffset:0 toBuffer:b destinationOffset:off size:sz];
    [blit endEncoding];
    [cmd commit]; [cmd waitUntilCompleted];
    return true;
}
bool NkDevice_Metal::WriteBufferAsync(NkBufferHandle buf, const void* data, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    auto* b = (__bridge id<MTLBuffer>)it->second.buf;
    if (b.storageMode == MTLStorageModeShared) { memcpy((uint8*)b.contents+off,data,(size_t)sz); return true; }
    return WriteBuffer(buf,data,sz,off);
}
bool NkDevice_Metal::ReadBuffer(NkBufferHandle buf, void* out, uint64 sz, uint64 off) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return false;
    auto* b = (__bridge id<MTLBuffer>)it->second.buf;
    if (b.storageMode == MTLStorageModeShared) { memcpy(out,(uint8*)b.contents+off,(size_t)sz); return true; }
    id<MTLBuffer> stage = [mDevice newBufferWithLength:sz options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cmd=[mQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit=[cmd blitCommandEncoder];
    [blit copyFromBuffer:b sourceOffset:off toBuffer:stage destinationOffset:0 size:sz];
    [blit endEncoding]; [cmd commit]; [cmd waitUntilCompleted];
    memcpy(out,stage.contents,(size_t)sz);
    return true;
}
NkMappedMemory NkDevice_Metal::MapBuffer(NkBufferHandle buf, uint64 off, uint64 sz) {
    auto it = mBuffers.find(buf.id); if (it == mBuffers.end()) return {};
    auto* b = (__bridge id<MTLBuffer>)it->second.buf;
    if (b.storageMode != MTLStorageModeShared) return {};
    uint64 mapSz = sz > 0 ? sz : it->second.desc.sizeBytes - off;
    return { (uint8*)b.contents + off, mapSz };
}
void NkDevice_Metal::UnmapBuffer(NkBufferHandle) {} // No-op pour Metal shared

// =============================================================================
// Textures
// =============================================================================
NkTextureHandle NkDevice_Metal::CreateTexture(const NkTextureDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    MTLTextureDescriptor* td = [[MTLTextureDescriptor alloc] init];
    td.pixelFormat  = ToMTLFormat(desc.format);
    td.width        = desc.width;
    td.height       = desc.height;
    td.depth        = desc.depth > 0 ? desc.depth : 1;
    td.mipmapLevelCount = desc.mipLevels == 0
        ? (NSUInteger)(floor(log2(std::max(desc.width, desc.height))) + 1)
        : desc.mipLevels;
    td.arrayLength  = desc.arrayLayers;
    td.sampleCount  = (NSUInteger)desc.samples;
    td.storageMode  = MTLStorageModePrivate;

    td.usage = MTLTextureUsageUnknown;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::ShaderResource))  td.usage |= MTLTextureUsageShaderRead;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::RenderTarget))    td.usage |= MTLTextureUsageRenderTarget;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::DepthStencil))    td.usage |= MTLTextureUsageRenderTarget;
    if (NkHasFlag(desc.bindFlags, NkBindFlags::UnorderedAccess)) td.usage |= MTLTextureUsageShaderWrite;
    if (td.usage == MTLTextureUsageUnknown)
        td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

    switch (desc.type) {
        case NkTextureType::Cube:      td.textureType = MTLTextureTypeCube; break;
        case NkTextureType::Tex3D:     td.textureType = MTLTextureType3D; break;
        case NkTextureType::Tex2DArray:td.textureType = MTLTextureType2DArray; break;
        default:                       td.textureType = MTLTextureType2D; break;
    }

    id<MTLTexture> tex = [mDevice newTextureWithDescriptor:td];
    if (!tex) return {};

    if (desc.initialData) {
        uint32 bpp = NkFormatBytesPerPixel(desc.format);
        uint32 rp  = desc.rowPitch > 0 ? desc.rowPitch : desc.width * bpp;
        MTLRegion region = MTLRegionMake2D(0, 0, desc.width, desc.height);
        // Pour un stockage private, on doit passer par un buffer staging + blit
        id<MTLBuffer> stage = [mDevice newBufferWithBytes:desc.initialData
                                                   length:rp * desc.height
                                                  options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
        [blit copyFromBuffer:stage sourceOffset:0
             sourceBytesPerRow:rp
           sourceBytesPerImage:rp * desc.height
                    sourceSize:MTLSizeMake(desc.width, desc.height, 1)
                     toTexture:tex
              destinationSlice:0
              destinationLevel:0
             destinationOrigin:MTLOriginMake(0, 0, 0)];
        if (desc.mipLevels != 1)
            [blit generateMipmapsForTexture:tex];
        [blit endEncoding];
        [cmd commit]; [cmd waitUntilCompleted];
    }

    if (desc.debugName)
        tex.label = [NSString stringWithUTF8String:desc.debugName];

    uint64 hid = NextId();
    mTextures[hid] = { (__bridge_retained void*)tex, desc, false };
    NkTextureHandle h; h.id = hid; return h;
}

void NkDevice_Metal::DestroyTexture(NkTextureHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mTextures.find(h.id); if (it == mTextures.end()) return;
    if (!it->second.isSwapchain && it->second.tex) CFRelease(it->second.tex);
    mTextures.erase(it); h.id = 0;
}

bool NkDevice_Metal::WriteTexture(NkTextureHandle t, const void* p, uint32 rp) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto& desc = it->second.desc;
    return WriteTextureRegion(t, p, 0, 0, 0, desc.width, desc.height, 1, 0, 0, rp);
}
bool NkDevice_Metal::WriteTextureRegion(NkTextureHandle t, const void* pixels,
    uint32 x, uint32 y, uint32 /*z*/, uint32 w, uint32 h, uint32 /*d2*/,
    uint32 mip, uint32 slice, uint32 rowPitch) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto* tex = (__bridge id<MTLTexture>)it->second.tex;
    uint32 bpp = NkFormatBytesPerPixel(it->second.desc.format);
    uint32 rp  = rowPitch > 0 ? rowPitch : w * bpp;
    id<MTLBuffer> stage = [mDevice newBufferWithBytes:pixels length:rp*h options:MTLResourceStorageModeShared];
    id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit copyFromBuffer:stage sourceOffset:0 sourceBytesPerRow:rp
        sourceBytesPerImage:rp*h sourceSize:MTLSizeMake(w,h,1)
        toTexture:tex destinationSlice:slice destinationLevel:mip
        destinationOrigin:MTLOriginMake(x,y,0)];
    [blit endEncoding]; [cmd commit]; [cmd waitUntilCompleted];
    return true;
}
bool NkDevice_Metal::GenerateMipmaps(NkTextureHandle t, NkFilter) {
    auto it = mTextures.find(t.id); if (it == mTextures.end()) return false;
    auto* tex = (__bridge id<MTLTexture>)it->second.tex;
    id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
    [blit generateMipmapsForTexture:tex];
    [blit endEncoding]; [cmd commit]; [cmd waitUntilCompleted];
    return true;
}

// =============================================================================
// Samplers
// =============================================================================
NkSamplerHandle NkDevice_Metal::CreateSampler(const NkSamplerDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
    sd.magFilter    = ToMTLFilter(d.magFilter);
    sd.minFilter    = ToMTLFilter(d.minFilter);
    sd.mipFilter    = ToMTLMipFilter(d.mipFilter);
    sd.sAddressMode = ToMTLAddress(d.addressU);
    sd.tAddressMode = ToMTLAddress(d.addressV);
    sd.rAddressMode = ToMTLAddress(d.addressW);
    sd.lodMinClamp  = d.minLod;
    sd.lodMaxClamp  = d.maxLod;
    sd.maxAnisotropy= (NSUInteger)d.maxAnisotropy;
    sd.compareFunction = d.compareEnable ? ToMTLCompare(d.compareOp) : MTLCompareFunctionNever;
    id<MTLSamplerState> ss = [mDevice newSamplerStateWithDescriptor:sd];
    uint64 hid = NextId(); mSamplers[hid] = { (__bridge_retained void*)ss };
    NkSamplerHandle h; h.id = hid; return h;
}
void NkDevice_Metal::DestroySampler(NkSamplerHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mSamplers.find(h.id); if (it == mSamplers.end()) return;
    if (it->second.ss) CFRelease(it->second.ss);
    mSamplers.erase(it); h.id = 0;
}

// =============================================================================
// Shaders (MSL source ou bibliothèque pré-compilée)
// =============================================================================
NkShaderHandle NkDevice_Metal::CreateShader(const NkShaderDesc& desc) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkMetalShader sh;
    for (uint32 i = 0; i < desc.stageCount; i++) {
        auto& s = desc.stages[i];
        id<MTLLibrary> lib = nil;

        if (s.mslSource) {
            NSError* err = nil;
            NSString* src = [NSString stringWithUTF8String:s.mslSource];
            lib = [mDevice newLibraryWithSource:src options:nil error:&err];
            if (err) NK_MTL_ERR("Shader MSL: %s\n", [err.localizedDescription UTF8String]);
        } else if (s.spirvData) {
            // Metal ne lit pas nativement le SPIR-V
            // Il faut SPIRV-Cross pour convertir en MSL — à faire en pré-build
            NK_MTL_ERR("Metal: SPIR-V non supporté directement, utiliser MSL\n");
            continue;
        } else if (s.metalLibPath) {
            NSString* path = [NSString stringWithUTF8String:s.metalLibPath];
            NSError* err = nil;
            lib = [mDevice newLibraryWithURL:[NSURL fileURLWithPath:path] error:&err];
            if (err) NK_MTL_ERR("metallib: %s\n", [err.localizedDescription UTF8String]);
        }

        if (!lib) continue;
        const char* entry = s.entryPoint ? s.entryPoint : "main";
        NSString* fn = [NSString stringWithUTF8String:entry];
        id<MTLFunction> func = [lib newFunctionWithName:fn];
        if (!func) { NK_MTL_ERR("Fonction '%s' introuvable dans le shader\n", entry); continue; }

        void* retained = (__bridge_retained void*)func;
        switch (s.stage) {
            case NkShaderStage::Vertex:   sh.vert = retained; break;
            case NkShaderStage::Fragment: sh.frag = retained; break;
            case NkShaderStage::Compute:  sh.comp = retained; break;
            default: CFRelease(retained); break;
        }
    }
    uint64 hid = NextId(); mShaders[hid] = sh;
    NkShaderHandle h; h.id = hid; return h;
}
void NkDevice_Metal::DestroyShader(NkShaderHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mShaders.find(h.id); if (it == mShaders.end()) return;
    if (it->second.vert) CFRelease(it->second.vert);
    if (it->second.frag) CFRelease(it->second.frag);
    if (it->second.comp) CFRelease(it->second.comp);
    mShaders.erase(it); h.id = 0;
}

// =============================================================================
// Pipelines
// =============================================================================
NkPipelineHandle NkDevice_Metal::CreateGraphicsPipeline(const NkGraphicsPipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit = mShaders.find(d.shader.id); if (sit == mShaders.end()) return {};
    auto& sh = sit->second;

    MTLRenderPipelineDescriptor* pd = [[MTLRenderPipelineDescriptor alloc] init];
    if (sh.vert) pd.vertexFunction   = (__bridge id<MTLFunction>)sh.vert;
    if (sh.frag) pd.fragmentFunction = (__bridge id<MTLFunction>)sh.frag;
    pd.sampleCount = (NSUInteger)d.samples;

    // Vertex descriptor
    if (d.vertexLayout.attributeCount > 0) {
        MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];
        for (uint32 i = 0; i < d.vertexLayout.attributeCount; i++) {
            auto& a = d.vertexLayout.attributes[i];
            vd.attributes[a.location].format      = ToMTLVertexFormat(a.format);
            vd.attributes[a.location].offset      = a.offset;
            vd.attributes[a.location].bufferIndex = a.binding;
        }
        for (uint32 i = 0; i < d.vertexLayout.bindingCount; i++) {
            auto& b = d.vertexLayout.bindings[i];
            vd.layouts[b.binding].stride       = b.stride;
            vd.layouts[b.binding].stepFunction = b.perInstance
                ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
        }
        pd.vertexDescriptor = vd;
    }

    // Render target formats
    auto rpit = mRenderPasses.find(d.renderPass.id);
    if (rpit != mRenderPasses.end()) {
        for (uint32 i = 0; i < rpit->second.desc.colorCount; i++)
            pd.colorAttachments[i].pixelFormat = ToMTLFormat(rpit->second.desc.colorAttachments[i].format);
        if (rpit->second.desc.hasDepth)
            pd.depthAttachmentPixelFormat = ToMTLFormat(rpit->second.desc.depthAttachment.format);
    } else {
        pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        pd.depthAttachmentPixelFormat      = MTLPixelFormatDepth32Float;
    }

    // Blend
    for (uint32 i = 0; i < d.blend.attachmentCount && i < 8; i++) {
        auto& a = d.blend.attachments[i];
        pd.colorAttachments[i].blendingEnabled        = a.blendEnable;
        pd.colorAttachments[i].sourceRGBBlendFactor   = ToMTLBlend(a.srcColor);
        pd.colorAttachments[i].destinationRGBBlendFactor = ToMTLBlend(a.dstColor);
        pd.colorAttachments[i].rgbBlendOperation      = ToMTLBlendOp(a.colorOp);
        pd.colorAttachments[i].sourceAlphaBlendFactor = ToMTLBlend(a.srcAlpha);
        pd.colorAttachments[i].destinationAlphaBlendFactor = ToMTLBlend(a.dstAlpha);
        pd.colorAttachments[i].alphaBlendOperation    = ToMTLBlendOp(a.alphaOp);
        pd.colorAttachments[i].writeMask              = a.colorWriteMask & 0xF;
    }

    NSError* err = nil;
    id<MTLRenderPipelineState> rpso = [mDevice newRenderPipelineStateWithDescriptor:pd error:&err];
    if (err) { NK_MTL_ERR("Pipeline: %s\n", [err.localizedDescription UTF8String]); return {}; }

    // Depth-stencil state
    MTLDepthStencilDescriptor* dsd = [[MTLDepthStencilDescriptor alloc] init];
    dsd.depthCompareFunction= d.depthStencil.depthTestEnable ? ToMTLCompare(d.depthStencil.depthCompareOp) : MTLCompareFunctionAlways;
    dsd.depthWriteEnabled   = d.depthStencil.depthWriteEnable;
    if (d.depthStencil.stencilEnable) {
        MTLStencilDescriptor* sf = [[MTLStencilDescriptor alloc] init];
        sf.stencilFailureOperation   = ToMTLStencilOp(d.depthStencil.front.failOp);
        sf.depthFailureOperation     = ToMTLStencilOp(d.depthStencil.front.depthFailOp);
        sf.depthStencilPassOperation = ToMTLStencilOp(d.depthStencil.front.passOp);
        sf.stencilCompareFunction    = ToMTLCompare(d.depthStencil.front.compareOp);
        dsd.frontFaceStencil = sf;
        MTLStencilDescriptor* sb = [[MTLStencilDescriptor alloc] init];
        sb.stencilFailureOperation   = ToMTLStencilOp(d.depthStencil.back.failOp);
        sb.depthFailureOperation     = ToMTLStencilOp(d.depthStencil.back.depthFailOp);
        sb.depthStencilPassOperation = ToMTLStencilOp(d.depthStencil.back.passOp);
        sb.stencilCompareFunction    = ToMTLCompare(d.depthStencil.back.compareOp);
        dsd.backFaceStencil = sb;
    }
    id<MTLDepthStencilState> dss = [mDevice newDepthStencilStateWithDescriptor:dsd];

    NkMetalPipeline p;
    p.rpso       = (__bridge_retained void*)rpso;
    p.dss        = (__bridge_retained void*)dss;
    p.isCompute  = false;
    p.frontFaceCCW  = d.rasterizer.frontFace == NkFrontFace::CCW;
    p.cullMode      = d.rasterizer.cullMode == NkCullMode::None ? 0 : d.rasterizer.cullMode == NkCullMode::Front ? 1 : 2;
    p.depthBiasConst= d.rasterizer.depthBiasConst;
    p.depthBiasSlope= d.rasterizer.depthBiasSlope;
    p.depthBiasClamp= d.rasterizer.depthBiasClamp;

    uint64 hid = NextId(); mPipelines[hid] = p;
    NkPipelineHandle h; h.id = hid; return h;
}

NkPipelineHandle NkDevice_Metal::CreateComputePipeline(const NkComputePipelineDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto sit = mShaders.find(d.shader.id); if (sit == mShaders.end()) return {};
    id<MTLFunction> comp = (__bridge id<MTLFunction>)sit->second.comp;
    if (!comp) return {};
    NSError* err = nil;
    id<MTLComputePipelineState> cpso = [mDevice newComputePipelineStateWithFunction:comp error:&err];
    if (err) { NK_MTL_ERR("ComputePipeline: %s\n", [err.localizedDescription UTF8String]); return {}; }
    NkMetalPipeline p; p.cpso = (__bridge_retained void*)cpso; p.isCompute = true;
    uint64 hid = NextId(); mPipelines[hid] = p;
    NkPipelineHandle h; h.id = hid; return h;
}

void NkDevice_Metal::DestroyPipeline(NkPipelineHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPipelines.find(h.id); if (it == mPipelines.end()) return;
    if (it->second.rpso) CFRelease(it->second.rpso);
    if (it->second.cpso) CFRelease(it->second.cpso);
    if (it->second.dss)  CFRelease(it->second.dss);
    mPipelines.erase(it); h.id = 0;
}

// =============================================================================
// Render Passes & Framebuffers
// =============================================================================
NkRenderPassHandle NkDevice_Metal::CreateRenderPass(const NkRenderPassDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mRenderPasses[hid] = { d };
    NkRenderPassHandle h; h.id = hid; return h;
}
void NkDevice_Metal::DestroyRenderPass(NkRenderPassHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mRenderPasses.erase(h.id); h.id = 0;
}

NkFramebufferHandle NkDevice_Metal::CreateFramebuffer(const NkFramebufferDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkMetalFramebuffer fb;
    fb.colorCount = d.colorCount;
    for (uint32 i = 0; i < d.colorCount; i++) fb.colorAttachments[i] = d.colorAttachments[i];
    fb.depthAttachment = d.depthAttachment;
    fb.w = d.width; fb.h = d.height;
    uint64 hid = NextId(); mFramebuffers[hid] = fb;
    NkFramebufferHandle h; h.id = hid; return h;
}
void NkDevice_Metal::DestroyFramebuffer(NkFramebufferHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mFramebuffers.erase(h.id); h.id = 0;
}

// =============================================================================
// Descriptor Sets
// =============================================================================
NkDescSetHandle NkDevice_Metal::CreateDescriptorSetLayout(const NkDescriptorSetLayoutDesc& d) {
    std::lock_guard<std::mutex> lock(mMutex);
    uint64 hid = NextId(); mDescLayouts[hid] = { d };
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDevice_Metal::DestroyDescriptorSetLayout(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescLayouts.erase(h.id); h.id = 0;
}
NkDescSetHandle NkDevice_Metal::AllocateDescriptorSet(NkDescSetHandle layout) {
    std::lock_guard<std::mutex> lock(mMutex);
    NkMetalDescSet ds; ds.layoutId = layout.id;
    uint64 hid = NextId(); mDescSets[hid] = ds;
    NkDescSetHandle h; h.id = hid; return h;
}
void NkDevice_Metal::FreeDescriptorSet(NkDescSetHandle& h) {
    std::lock_guard<std::mutex> lock(mMutex); mDescSets.erase(h.id); h.id = 0;
}
void NkDevice_Metal::UpdateDescriptorSets(const NkDescriptorWrite* writes, uint32 n) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (uint32 i = 0; i < n; i++) {
        auto& w = writes[i];
        auto sit = mDescSets.find(w.set.id); if (sit == mDescSets.end()) continue;
        NkMetalDescSet::Binding b{ w.binding, w.type, w.buffer.id, w.texture.id, w.sampler.id };
        bool found = false;
        for (auto& e : sit->second.bindings) if (e.slot == w.binding) { e = b; found = true; break; }
        if (!found) sit->second.bindings.push_back(b);
    }
}

// =============================================================================
// Command Buffers
// =============================================================================
NkICommandBuffer* NkDevice_Metal::CreateCommandBuffer(NkCommandBufferType t) {
    return new NkCommandBuffer_Metal(this, t);
}
void NkDevice_Metal::DestroyCommandBuffer(NkICommandBuffer*& cb) { delete cb; cb = nullptr; }

// =============================================================================
// Submit
// =============================================================================
void NkDevice_Metal::Submit(NkICommandBuffer* const* cbs, uint32 n, NkFenceHandle fence) {
    for (uint32 i = 0; i < n; i++) {
        auto* m = dynamic_cast<NkCommandBuffer_Metal*>(cbs[i]);
        if (m) m->CommitAndWait();
    }
    if (fence.IsValid()) {
        auto it = mFences.find(fence.id); if (it != mFences.end()) it->second.signaled = true;
    }
}

void NkDevice_Metal::SubmitAndPresent(NkICommandBuffer* cb) {
    auto* m = dynamic_cast<NkCommandBuffer_Metal*>(cb);
    if (m) m->CommitAndPresent(mCurrentDrawable);
    mCurrentDrawable = nil;
}

// =============================================================================
// Fence
// =============================================================================
NkFenceHandle NkDevice_Metal::CreateFence(bool signaled) {
    uint64 hid = NextId(); mFences[hid] = { signaled };
    NkFenceHandle h; h.id = hid; return h;
}
void NkDevice_Metal::DestroyFence(NkFenceHandle& h) { mFences.erase(h.id); h.id = 0; }
bool NkDevice_Metal::WaitFence(NkFenceHandle f, uint64) {
    auto it = mFences.find(f.id); return it != mFences.end() && it->second.signaled;
}
bool NkDevice_Metal::IsFenceSignaled(NkFenceHandle f) {
    auto it = mFences.find(f.id); return it != mFences.end() && it->second.signaled;
}
void NkDevice_Metal::ResetFence(NkFenceHandle f) {
    auto it = mFences.find(f.id); if (it != mFences.end()) it->second.signaled = false;
}
void NkDevice_Metal::WaitIdle() {
    id<MTLCommandBuffer> cmd = [mQueue commandBuffer];
    [cmd commit]; [cmd waitUntilCompleted];
}

// =============================================================================
// Frame
// =============================================================================
void NkDevice_Metal::BeginFrame(NkFrameContext& frame) {
    mCurrentDrawable = [mLayer nextDrawable];
    if (!mCurrentDrawable) return;

    // Mettre à jour le framebuffer swapchain avec le drawable courant
    uint64 colorId = NextId();
    NkMetalTexture swt{};
    swt.tex         = (__bridge_retained void*)mCurrentDrawable.texture;
    swt.isSwapchain = true;
    swt.desc        = NkTextureDesc::RenderTarget(mWidth, mHeight, NkFormat::BGRA8_Srgb);
    mTextures[colorId] = swt;

    // Mettre à jour le framebuffer
    auto fbit = mFramebuffers.find(mSwapchainFB.id);
    if (fbit != mFramebuffers.end()) {
        NkTextureHandle ch; ch.id = colorId;
        fbit->second.colorAttachments[0] = ch;
        fbit->second.colorCount = 1;
    }

    frame.frameIndex  = mFrameIndex;
    frame.frameNumber = mFrameNumber;
}

void NkDevice_Metal::EndFrame(NkFrameContext&) {
    mFrameIndex = (mFrameIndex + 1) % kMaxFrames;
    ++mFrameNumber;
}

void NkDevice_Metal::OnResize(uint32 w, uint32 h) {
    if (w == 0 || h == 0) return;
    WaitIdle();
    mWidth = w; mHeight = h;
    mLayer.drawableSize = CGSizeMake(w, h);

    // Recréer depth
    if (mDepthTex.IsValid()) DestroyTexture(mDepthTex);
    NkTextureDesc dd = NkTextureDesc::DepthStencil(w, h);
    mDepthTex = CreateTexture(dd);

    auto fbit = mFramebuffers.find(mSwapchainFB.id);
    if (fbit != mFramebuffers.end()) {
        fbit->second.depthAttachment = mDepthTex;
        fbit->second.w = w; fbit->second.h = h;
    }
}

// =============================================================================
// Caps
// =============================================================================
void NkDevice_Metal::QueryCaps() {
    mCaps.tessellationShaders = true;
    mCaps.computeShaders      = true;
    mCaps.drawIndirect        = true;
    mCaps.multiViewport       = false; // Metal 3+
    mCaps.independentBlend    = true;
    mCaps.textureCompressionBC= [mDevice supportsFamily:MTLGPUFamilyApple1] ? false : true;
    mCaps.textureCompressionASTC = true;
    mCaps.maxTextureDim2D     = 16384;
    mCaps.maxTextureDim3D     = 2048;
    mCaps.maxColorAttachments = 8;
    mCaps.maxVertexAttributes = 31;
    mCaps.maxPushConstantBytes= 4096; // argument buffers ou vertex bytes
    mCaps.minUniformBufferAlign = 256;
    mCaps.timestampQueries    = true;
    mCaps.msaa2x = mCaps.msaa4x = mCaps.msaa8x = true;
    mCaps.meshShaders = [mDevice supportsFamily:MTLGPUFamilyApple6];
}

// =============================================================================
// Accesseurs internes
// =============================================================================
NkMTLBuffer  NkDevice_Metal::GetMTLBuffer(uint64 id) const {
    auto it = mBuffers.find(id); return it != mBuffers.end() ? it->second.buf : nullptr;
}
NkMTLTexture NkDevice_Metal::GetMTLTexture(uint64 id) const {
    auto it = mTextures.find(id); return it != mTextures.end() ? it->second.tex : nullptr;
}
NkMTLSamplerState NkDevice_Metal::GetMTLSampler(uint64 id) const {
    auto it = mSamplers.find(id); return it != mSamplers.end() ? it->second.ss : nullptr;
}
const NkMetalPipeline* NkDevice_Metal::GetPipeline(uint64 id) const {
    auto it = mPipelines.find(id); return it != mPipelines.end() ? &it->second : nullptr;
}
const NkMetalDescSet* NkDevice_Metal::GetDescSet(uint64 id) const {
    auto it = mDescSets.find(id); return it != mDescSets.end() ? &it->second : nullptr;
}
const NkMetalFramebuffer* NkDevice_Metal::GetFBO(uint64 id) const {
    auto it = mFramebuffers.find(id); return it != mFramebuffers.end() ? &it->second : nullptr;
}

} // namespace nkentseu
#endif // NK_RHI_METAL_ENABLED
