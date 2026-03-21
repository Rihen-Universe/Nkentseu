#pragma once
// =============================================================================
// NkSoftwareTypes.h — v4 final
// Corrections v4 :
//   - NkSWGeomContext : callbacks par valeur (NkFunction) pas par pointeur
//   - colorTarget alias toujours sync avec colorTargets[0]
//   - NkSWShader::SetDefaultPassthrough() corrigé
// =============================================================================
#include "NKRenderer/RHI/Core/NkIDevice.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkFunctions.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKThreading/NkMutex.h"
#include "NKCore/NkAtomic.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
using threading::NkMutex;
using threading::NkScopedLock;
using namespace math;

// =============================================================================
// NkSWVertex
// =============================================================================
struct NkSWVertex {
    NkVec4f position  = {0,0,0,1};
    NkVec3f normal    = {0,0,1};
    NkVec2f uv        = {0,0};
    NkVec4f color     = {1,1,1,1};
    float   attrs[16] = {};
    uint32  attrCount = 0;

    void SetAttrVec3(uint32 base, float x, float y, float z) {
        if (base+2 < 16) { attrs[base]=x; attrs[base+1]=y; attrs[base+2]=z;
                            attrCount = NkMax(attrCount, base+3); }
    }
    void SetAttrVec4(uint32 base, float x, float y, float z, float w) {
        if (base+3 < 16) { attrs[base]=x; attrs[base+1]=y; attrs[base+2]=z; attrs[base+3]=w;
                            attrCount = NkMax(attrCount, base+4); }
    }
    NkVec3f GetAttrVec3(uint32 base) const {
        return (base+2 < 16) ? NkVec3f{attrs[base],attrs[base+1],attrs[base+2]} : NkVec3f{};
    }
    NkVec4f GetAttrVec4(uint32 base) const {
        return (base+3 < 16) ? NkVec4f{attrs[base],attrs[base+1],attrs[base+2],attrs[base+3]} : NkVec4f{};
    }
};

// =============================================================================
// Signatures des shaders CPU
// =============================================================================
using NkSWVertexFn   = NkFunction<NkSWVertex(const void*, uint32, const void*)>;
using NkSWFragmentFn = NkFunction<NkVec4f(const NkSWVertex&, const void*, const void*)>;
using NkSWComputeFn  = NkFunction<void(uint32, uint32, uint32, const void*)>;

// GS CPU : reçoit une primitive et peut en émettre de nouvelles
// Forward déclaré — NkSWGeomContext est défini dans NkSoftwareRasterizer.h
struct NkSWGeomContext;
using NkSWGeomFn = NkFunction<void(const NkSWVertex*, uint32, NkSWGeomContext&)>;

// Domain shader CPU pour la tessellation
using NkSWTessFn = NkFunction<NkSWVertex(const NkSWVertex*, uint32, const float*, const void*)>;

// =============================================================================
// NkSWMSAASample + offsets
// =============================================================================
struct NkSWMSAASample {
    NkVec4f color   = {0,0,0,0};
    float   depth   = 1.f;
    bool    covered = false;
};

inline const NkVec2f* NkSWGetMSAAOffsets(uint32 n) {
    static const NkVec2f k1x[1] = {{0.f,0.f}};
    static const NkVec2f k2x[2] = {{0.25f,0.25f},{-0.25f,-0.25f}};
    static const NkVec2f k4x[4] = {{-0.125f,-0.375f},{0.375f,-0.125f},
                                     {-0.375f,0.125f},{0.125f,0.375f}};
    static const NkVec2f k8x[8] = {{0.0625f,-0.1875f},{-0.0625f,0.1875f},
                                     {0.3125f,0.0625f},{-0.1875f,-0.3125f},
                                     {-0.3125f,0.3125f},{-0.4375f,-0.0625f},
                                     {0.1875f,0.4375f},{0.4375f,-0.4375f}};
    switch (n) { case 2: return k2x; case 4: return k4x; case 8: return k8x; default: return k1x; }
}

// =============================================================================
// Objets GPU SW
// =============================================================================
struct NkSWBuffer {
    NkVector<uint8> data;
    NkBufferDesc    desc;
    bool            mapped = false;
};

struct NkSWTexture {
    NkVector<NkVector<uint8>> mips;
    NkTextureDesc             desc;
    bool                      isRenderTarget = false;

    uint32  Width (uint32 mip=0) const { uint32 v=desc.width >>mip; return v?v:1u; }
    uint32  Height(uint32 mip=0) const { uint32 v=desc.height>>mip; return v?v:1u; }
    uint32  Bpp()                const;

    NkVec4f Read       (uint32 x, uint32 y, uint32 mip=0) const;
    void    Write      (uint32 x, uint32 y, const NkVec4f& c, uint32 mip=0);
    NkVec4f Sample     (float u, float v, uint32 mip=0) const;
    NkVec4f SampleShadow(float u, float v, float compareZ) const;
    void    Clear      (const NkVec4f& c, uint32 mip=0);
    void    ClearDepth (float d=1.f);

    const uint8* Data(uint32 mip=0) const { return mips.Size()>mip ? mips[mip].Data() : nullptr; }
    uint8*       Data(uint32 mip=0)       { return mips.Size()>mip ? mips[mip].Data() : nullptr; }
    uint32       RowPitch(uint32 mip=0) const { return Width(mip)*Bpp(); }
};

struct NkSWSampler { NkSamplerDesc desc; };

struct NkSWShader {
    NkSWVertexFn   vertFn;
    NkSWFragmentFn fragFn;
    NkSWComputeFn  computeFn;
    bool           isCompute = false;
    // GS + Tess CPU
    NkSWGeomFn          geomFn;
    NkPrimitiveTopology geomOutputTopology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    NkSWTessFn  tessFn;
    uint32      patchSize  = 0;
    float       tessOuter  = 1.f;
    float       tessInner  = 1.f;

    void SetDefaultPassthrough() {
        fragFn = [](const NkSWVertex& v, const void*, const void*) -> NkVec4f {
            return v.color;
        };
    }
};

// =============================================================================
// MRT
// =============================================================================
static constexpr uint32 kNkSWMaxColorTargets = 4;

struct NkSWPipeline {
    uint64              shaderId     = 0;
    bool                isCompute    = false;
    bool                depthTest    = true;
    bool                depthWrite   = true;
    bool                blendEnable  = false;
    NkCompareOp         depthOp      = NkCompareOp::NK_LESS;
    NkCullMode          cullMode     = NkCullMode::NK_BACK;
    NkFrontFace         frontFace    = NkFrontFace::NK_CCW;
    NkBlendFactor       srcColor     = NkBlendFactor::NK_SRC_ALPHA;
    NkBlendFactor       dstColor     = NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
    NkBlendFactor       srcAlpha     = NkBlendFactor::NK_ONE;
    NkBlendFactor       dstAlpha     = NkBlendFactor::NK_ZERO;
    NkPrimitiveTopology topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    uint32              vertexStride = 0;
    uint32              sampleCount  = 1;
    NkVector<uint64>    descLayoutIds;
};

struct NkSWRenderPass  { NkRenderPassDesc desc; };

struct NkSWFramebuffer {
    NkVector<uint64> colorIds;
    uint64           depthId      = 0;
    uint32           w=0, h=0;
    uint64           renderPassId = 0;
    uint32           sampleCount  = 1;
    NkVector<uint64> msaaColorIds;
    uint64           msaaDepthId  = 0;
};

struct NkSWDescSetLayout { NkDescriptorSetLayoutDesc desc; };

struct NkSWDescSet {
    struct Binding {
        uint32           slot  = 0;
        NkDescriptorType type  = {};
        uint64           bufId = 0, texId = 0, sampId = 0;
    };
    NkVector<Binding> bindings;
    uint64            layoutId = 0;
    const Binding* FindBinding(uint32 slot) const {
        for (auto& b : bindings) if (b.slot == slot) return &b;
        return nullptr;
    }
};

struct NkSWFence     { bool signaled=false; };
struct NkSWSemaphore { bool signaled=false; };

// =============================================================================
// NkSWDrawState
// =============================================================================
struct NkSWDrawState {
    // MRT — colorTarget est TOUJOURS == colorTargets[0] (maintenu par ResolveDrawState)
    NkSWTexture*  colorTargets[kNkSWMaxColorTargets] = {};
    uint32        colorTargetCount = 0;
    NkSWTexture*  colorTarget = nullptr;   // alias garanti == colorTargets[0]
    NkSWTexture*  depthTarget = nullptr;
    uint32        targetW = 0, targetH = 0;

    // MSAA
    uint32        msaaSamples = 1;
    NkSWTexture*  msaaTargets[kNkSWMaxColorTargets] = {};
    NkSWTexture*  msaaDepth   = nullptr;

    NkSWPipeline* pipeline  = nullptr;
    NkSWShader*   shader    = nullptr;
    const void*   uniformData = nullptr;

    static constexpr uint32 kMaxBindings = 16;
    NkSWTexture*  textures[kMaxBindings] = {};
    NkSWSampler*  samplers[kMaxBindings] = {};

    const uint8*  vertexData   = nullptr;
    uint32        vertexStride = 0;

    // Viewport
    float vpX=0, vpY=0, vpW=0, vpH=0, vpMinZ=0.f, vpMaxZ=1.f;
    bool  hasViewport = false;

    // Scissor
    int32 scissorX=0, scissorY=0, scissorW=0, scissorH=0;
    bool  hasScissor = false;

    // Push constants
    uint8  pushConstants[256] = {};
    uint32 pushConstantsSize  = 0;

    uint32 instanceIndex = 0;
    bool   wireframe = false;

    // Helper : s'assurer que colorTarget == colorTargets[0]
    void SyncColorTargetAlias() {
        colorTarget = (colorTargetCount > 0) ? colorTargets[0] : nullptr;
    }
};

// =============================================================================
// Utilitaires pixel
// =============================================================================
inline uint32 NkSWBpp(NkGpuFormat fmt) {
    switch (fmt) {
        case NkGpuFormat::NK_RGBA8_UNORM: case NkGpuFormat::NK_RGBA8_SRGB:
        case NkGpuFormat::NK_BGRA8_UNORM: case NkGpuFormat::NK_D32_FLOAT:
        case NkGpuFormat::NK_R32_FLOAT:   case NkGpuFormat::NK_RG16_FLOAT: return 4;
        case NkGpuFormat::NK_RGB8_UNORM:  case NkGpuFormat::NK_RGB32_FLOAT: return 3;
        case NkGpuFormat::NK_RGBA16_FLOAT: case NkGpuFormat::NK_RGBA32_FLOAT: return 16;
        case NkGpuFormat::NK_RG8_UNORM:   case NkGpuFormat::NK_D16_UNORM: return 2;
        case NkGpuFormat::NK_R8_UNORM: return 1;
        default: return 4;
    }
}

inline NkVec4f NkSWReadPixel(const uint8* p, NkGpuFormat fmt) {
    switch (fmt) {
        case NkGpuFormat::NK_RGBA8_UNORM: case NkGpuFormat::NK_RGBA8_SRGB:
            return {p[0]/255.f, p[1]/255.f, p[2]/255.f, p[3]/255.f};
        case NkGpuFormat::NK_BGRA8_UNORM:
            return {p[2]/255.f, p[1]/255.f, p[0]/255.f, p[3]/255.f};
        case NkGpuFormat::NK_D32_FLOAT: case NkGpuFormat::NK_R32_FLOAT:
            { float v; memcpy(&v, p, 4); return {v,v,v,1.f}; }
        case NkGpuFormat::NK_RGB8_UNORM:
            return {p[0]/255.f, p[1]/255.f, p[2]/255.f, 1.f};
        case NkGpuFormat::NK_RG8_UNORM:  return {p[0]/255.f, p[1]/255.f, 0.f, 1.f};
        case NkGpuFormat::NK_R8_UNORM:   return {p[0]/255.f, 0.f, 0.f, 1.f};
        default: return {p[0]/255.f, p[1]/255.f, p[2]/255.f, p[3]/255.f};
    }
}

inline void NkSWWritePixel(uint8* p, NkGpuFormat fmt, const NkVec4f& c) {
    float r=NkClamp(c.x,0.f,1.f), g=NkClamp(c.y,0.f,1.f);
    float b=NkClamp(c.z,0.f,1.f), a=NkClamp(c.w,0.f,1.f);
    switch (fmt) {
        case NkGpuFormat::NK_RGBA8_UNORM: case NkGpuFormat::NK_RGBA8_SRGB:
            p[0]=(uint8)(r*255.f); p[1]=(uint8)(g*255.f);
            p[2]=(uint8)(b*255.f); p[3]=(uint8)(a*255.f); break;
        case NkGpuFormat::NK_BGRA8_UNORM:
            p[0]=(uint8)(b*255.f); p[1]=(uint8)(g*255.f);
            p[2]=(uint8)(r*255.f); p[3]=(uint8)(a*255.f); break;
        case NkGpuFormat::NK_D32_FLOAT: case NkGpuFormat::NK_R32_FLOAT:
            memcpy(p, &c.x, 4); break;
        case NkGpuFormat::NK_RGB8_UNORM:
            p[0]=(uint8)(r*255.f); p[1]=(uint8)(g*255.f); p[2]=(uint8)(b*255.f); break;
        default:
            p[0]=(uint8)(r*255.f); p[1]=(uint8)(g*255.f);
            p[2]=(uint8)(b*255.f); p[3]=(uint8)(a*255.f); break;
    }
}

} // namespace nkentseu