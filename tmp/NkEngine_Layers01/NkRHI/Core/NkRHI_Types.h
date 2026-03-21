#pragma once
// =============================================================================
// NkRHI_Types.h
// Types fondamentaux du RHI — formats, états, enums, handles opaques.
// Ce fichier est l'unique source de vérité pour tout le RHI.
// Aucune dépendance vers une API native (pas de VkFormat, DXGI_FORMAT, etc.)
// =============================================================================
#include "../NkFinal_work/Core/NkTypes.h"
#include <cstddef>

namespace nkentseu {

// =============================================================================
// Handle opaque — identifiant GPU (64-bit, 0 = invalide)
// Chaque ressource GPU est identifiée par un NkHandle<Tag> typé.
// =============================================================================
template<typename Tag>
struct NkHandle {
    uint64 id = 0;
    bool IsValid() const { return id != 0; }
    bool operator==(const NkHandle& o) const { return id == o.id; }
    bool operator!=(const NkHandle& o) const { return id != o.id; }
    static NkHandle Null() { return {0}; }
};

// Tags pour les handles
struct TagBuffer    {};
struct TagTexture   {};
struct TagSampler   {};
struct TagShader    {};
struct TagPipeline  {};
struct TagRenderPass{};
struct TagFramebuffer{};
struct TagFence     {};
struct TagSemaphore {};
struct TagDescSet   {};

using NkBufferHandle      = NkHandle<TagBuffer>;
using NkTextureHandle     = NkHandle<TagTexture>;
using NkSamplerHandle     = NkHandle<TagSampler>;
using NkShaderHandle      = NkHandle<TagShader>;
using NkPipelineHandle    = NkHandle<TagPipeline>;
using NkRenderPassHandle  = NkHandle<TagRenderPass>;
using NkFramebufferHandle = NkHandle<TagFramebuffer>;
using NkFenceHandle       = NkHandle<TagFence>;
using NkDescSetHandle     = NkHandle<TagDescSet>;

// =============================================================================
// Formats pixel / vertex
// =============================================================================
enum class NkFormat : uint32 {
    Undefined = 0,

    // ── Couleur 8 bits par canal ──
    R8_Unorm, R8_Snorm, R8_Uint, R8_Sint,
    RG8_Unorm, RG8_Snorm,
    RGBA8_Unorm, RGBA8_Snorm, RGBA8_Uint, RGBA8_Sint,
    RGBA8_Srgb,
    BGRA8_Unorm, BGRA8_Srgb,

    // ── Couleur 16 bits ──
    R16_Float, RG16_Float, RGBA16_Float,
    R16_Uint, R16_Sint, RG16_Uint, RGBA16_Uint,

    // ── Couleur 32 bits ──
    R32_Float, RG32_Float, RGB32_Float, RGBA32_Float,
    R32_Uint, RG32_Uint, RGB32_Uint, RGBA32_Uint,
    R32_Sint, RG32_Sint,

    // ── Formats vertex spéciaux ──
    R8G8B8A8_Unorm_Packed,   // vertex color compact
    A2B10G10R10_Unorm,       // normal packed
    R11G11B10_Float,         // HDR color compact

    // ── Depth / Stencil ──
    D16_Unorm,
    D24_Unorm_S8_Uint,
    D32_Float,
    D32_Float_S8_Uint,

    // ── Formats compressés (textures) ──
    BC1_RGB_Unorm,  BC1_RGB_Srgb,
    BC3_Unorm,      BC3_Srgb,
    BC5_Unorm,      BC5_Snorm,
    BC7_Unorm,      BC7_Srgb,
    ETC2_RGB_Unorm, ETC2_RGBA_Unorm,  // mobile
    ASTC_4x4_Unorm, ASTC_4x4_Srgb,   // mobile

    Count
};

// Utilitaires format
inline bool NkFormatIsDepth(NkFormat f) {
    return f==NkFormat::D16_Unorm || f==NkFormat::D24_Unorm_S8_Uint
        || f==NkFormat::D32_Float || f==NkFormat::D32_Float_S8_Uint;
}
inline bool NkFormatHasStencil(NkFormat f) {
    return f==NkFormat::D24_Unorm_S8_Uint || f==NkFormat::D32_Float_S8_Uint;
}
inline bool NkFormatIsSrgb(NkFormat f) {
    return f==NkFormat::RGBA8_Srgb || f==NkFormat::BGRA8_Srgb
        || f==NkFormat::BC1_RGB_Srgb || f==NkFormat::BC3_Srgb
        || f==NkFormat::BC7_Srgb    || f==NkFormat::ASTC_4x4_Srgb
        || f==NkFormat::ETC2_RGB_Unorm; // approximation
}
inline uint32 NkFormatBytesPerPixel(NkFormat f) {
    switch(f) {
        case NkFormat::R8_Unorm:     return 1;
        case NkFormat::RG8_Unorm:    return 2;
        case NkFormat::RGBA8_Unorm:
        case NkFormat::BGRA8_Unorm:
        case NkFormat::RGBA8_Srgb:
        case NkFormat::R32_Float:
        case NkFormat::D32_Float:    return 4;
        case NkFormat::RG16_Float:   return 4;
        case NkFormat::RGBA16_Float: return 8;
        case NkFormat::RG32_Float:   return 8;
        case NkFormat::RGBA32_Float: return 16;
        case NkFormat::RGB32_Float:  return 12;
        default:                     return 0;
    }
}

// =============================================================================
// Vertex format pour VertexAttribute
// =============================================================================
enum class NkVertexFormat : uint32 {
    Float1, Float2, Float3, Float4,
    Half2,  Half4,
    Byte4_Unorm, Byte4_Snorm,
    UShort2_Unorm, UShort4_Unorm,
    Uint1, Uint2, Uint4,
    Int1,  Int2,  Int4,
};

inline uint32 NkVertexFormatSize(NkVertexFormat f) {
    switch(f) {
        case NkVertexFormat::Float1: return 4;
        case NkVertexFormat::Float2: return 8;
        case NkVertexFormat::Float3: return 12;
        case NkVertexFormat::Float4: return 16;
        case NkVertexFormat::Half2:  return 4;
        case NkVertexFormat::Half4:  return 8;
        case NkVertexFormat::Byte4_Unorm:
        case NkVertexFormat::Byte4_Snorm: return 4;
        case NkVertexFormat::UShort2_Unorm: return 4;
        case NkVertexFormat::UShort4_Unorm: return 8;
        case NkVertexFormat::Uint1:  return 4;
        case NkVertexFormat::Uint2:  return 8;
        case NkVertexFormat::Uint4:  return 16;
        case NkVertexFormat::Int1:   return 4;
        case NkVertexFormat::Int2:   return 8;
        case NkVertexFormat::Int4:   return 16;
        default:                     return 0;
    }
}

// =============================================================================
// Index format
// =============================================================================
enum class NkIndexFormat : uint32 { Uint16, Uint32 };

// =============================================================================
// Usage des ressources (buffers, textures)
// =============================================================================
enum class NkResourceUsage : uint32 {
    Default    = 0,    // GPU read/write, pas d'accès CPU direct
    Upload     = 1,    // CPU write → GPU read (staging, uniforms dynamiques)
    Readback   = 2,    // GPU write → CPU read (screenshots, readbacks)
    Immutable  = 3,    // Init une fois, jamais modifié (géométrie statique)
};

// =============================================================================
// Flags de bind (utilisation de la ressource dans le pipeline)
// =============================================================================
enum class NkBindFlags : uint32 {
    None            = 0,
    VertexBuffer    = 1 << 0,
    IndexBuffer     = 1 << 1,
    UniformBuffer   = 1 << 2,  // Constant Buffer
    StorageBuffer   = 1 << 3,  // SSBO / UAV Buffer
    ShaderResource  = 1 << 4,  // Texture SRV / Buffer SRV
    UnorderedAccess = 1 << 5,  // UAV / Storage Image
    RenderTarget    = 1 << 6,
    DepthStencil    = 1 << 7,
    IndirectArgs    = 1 << 8,  // Draw/Dispatch indirect
    TransferSrc     = 1 << 9,
    TransferDst     = 1 << 10,
};
inline NkBindFlags operator|(NkBindFlags a, NkBindFlags b) {
    return (NkBindFlags)((uint32)a|(uint32)b);
}
inline bool NkHasFlag(NkBindFlags flags, NkBindFlags bit) {
    return ((uint32)flags & (uint32)bit) != 0;
}

// =============================================================================
// Type de buffer
// =============================================================================
enum class NkBufferType : uint32 {
    Vertex, Index, Uniform, Storage, Indirect, Staging
};

// =============================================================================
// Type de texture
// =============================================================================
enum class NkTextureType : uint32 {
    Tex1D, Tex2D, Tex3D, Cube, Tex2DArray, CubeArray
};

// =============================================================================
// Sample count (MSAA)
// =============================================================================
enum class NkSampleCount : uint32 {
    S1=1, S2=2, S4=4, S8=8, S16=16, S32=32, S64=64
};

// =============================================================================
// Shader stage
// =============================================================================
enum class NkShaderStage : uint32 {
    Vertex   = 1<<0,
    Fragment = 1<<1,  // Pixel
    Geometry = 1<<2,
    TessCtrl = 1<<3,
    TessEval = 1<<4,
    Compute  = 1<<5,
    Mesh     = 1<<6,  // Mesh shaders (DX12/Vulkan 1.2+/Metal 3)
    Task     = 1<<7,  // Amplification / Task
    AllGraphics = Vertex|Fragment|Geometry|TessCtrl|TessEval,
    All         = AllGraphics|Compute|Mesh|Task,
};
inline NkShaderStage operator|(NkShaderStage a, NkShaderStage b) {
    return (NkShaderStage)((uint32)a|(uint32)b);
}

// =============================================================================
// Topology primitive
// =============================================================================
enum class NkPrimitiveTopology : uint32 {
    TriangleList, TriangleStrip, TriangleFan,
    LineList, LineStrip,
    PointList,
    PatchList,
};

// =============================================================================
// Fill / Cull mode
// =============================================================================
enum class NkFillMode : uint32 { Solid, Wireframe, Point };
enum class NkCullMode : uint32 { None, Front, Back };
enum class NkFrontFace: uint32 { CCW, CW };

// =============================================================================
// Depth / Stencil ops
// =============================================================================
enum class NkCompareOp : uint32 {
    Never, Less, Equal, LessEqual,
    Greater, NotEqual, GreaterEqual, Always
};
enum class NkStencilOp : uint32 {
    Keep, Zero, Replace, IncrClamp, DecrClamp, Invert, IncrWrap, DecrWrap
};

// =============================================================================
// Blend
// =============================================================================
enum class NkBlendFactor : uint32 {
    Zero, One,
    SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
    SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha,
    ConstantColor, OneMinusConstantColor,
    SrcAlphaSaturate,
};
enum class NkBlendOp : uint32 { Add, Sub, RevSub, Min, Max };

// =============================================================================
// Filter / Address mode (samplers)
// =============================================================================
enum class NkFilter : uint32 {
    Nearest, Linear, Cubic,
};
enum class NkMipFilter : uint32 {
    None, Nearest, Linear
};
enum class NkAddressMode : uint32 {
    Repeat, MirroredRepeat, ClampToEdge, ClampToBorder, MirrorClampToEdge
};
enum class NkBorderColor : uint32 {
    TransparentBlack, OpaqueBlack, OpaqueWhite
};

// =============================================================================
// Attachment load / store
// =============================================================================
enum class NkLoadOp  : uint32 { Load, Clear, DontCare };
enum class NkStoreOp : uint32 { Store, DontCare, Resolve };

// =============================================================================
// Pipeline stage (pour les barrières)
// =============================================================================
enum class NkPipelineStage : uint32 {
    None            = 0,
    TopOfPipe       = 1<<0,
    VertexInput     = 1<<1,
    VertexShader    = 1<<2,
    FragmentShader  = 1<<3,
    EarlyFragment   = 1<<4,
    LateFragment    = 1<<5,
    ColorAttachment = 1<<6,
    ComputeShader   = 1<<7,
    Transfer        = 1<<8,
    BottomOfPipe    = 1<<9,
    AllGraphics     = 1<<10,
    AllCommands     = 1<<11,
};
inline NkPipelineStage operator|(NkPipelineStage a, NkPipelineStage b) {
    return (NkPipelineStage)((uint32)a|(uint32)b);
}

// =============================================================================
// Resource state (état courant d'une ressource)
// =============================================================================
enum class NkResourceState : uint32 {
    Undefined,
    Common,
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    ShaderRead,
    ShaderWrite,
    RenderTarget,
    DepthRead,
    DepthWrite,
    Present,
    TransferSrc,
    TransferDst,
    UnorderedAccess,
    IndirectArg,
};

// =============================================================================
// Viewport et scissor
// =============================================================================
struct NkViewport {
    float x=0, y=0;
    float width=0, height=0;
    float minDepth=0.f, maxDepth=1.f;
};

struct NkRect2D {
    int32  x=0, y=0;
    uint32 width=0, height=0;
};

// =============================================================================
// Clear values
// =============================================================================
struct NkClearColor  { float r=0,g=0,b=0,a=1; };
struct NkClearDepth  { float depth=1.f; uint32 stencil=0; };
union  NkClearValue  {
    NkClearColor  color;
    NkClearDepth  depthStencil;
    NkClearValue() : color{0,0,0,1} {}
};

// =============================================================================
// Résultat d'opération RHI
// =============================================================================
enum class NkRHIResult : uint32 {
    Ok = 0,
    OutOfMemory,
    DeviceLost,
    InvalidParam,
    NotSupported,
    AlreadyExists,
    Timeout,
    Unknown,
};
inline bool NkSucceeded(NkRHIResult r) { return r == NkRHIResult::Ok; }
inline const char* NkRHIResultName(NkRHIResult r) {
    switch(r) {
        case NkRHIResult::Ok:           return "Ok";
        case NkRHIResult::OutOfMemory:  return "OutOfMemory";
        case NkRHIResult::DeviceLost:   return "DeviceLost";
        case NkRHIResult::InvalidParam: return "InvalidParam";
        case NkRHIResult::NotSupported: return "NotSupported";
        default:                        return "Unknown";
    }
}

} // namespace nkentseu
