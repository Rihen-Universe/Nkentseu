#pragma once
// =============================================================================
// NkTypes.h
// Types fondamentaux du RHI — formats, états, enums, handles opaques.
// Ce fichier est l'unique source de vérité pour tout le RHI.
// Aucune dépendance vers une API native (pas de VkFormat, DXGI_FORMAT, etc.)
// =============================================================================
#include "NKCore/NkTypes.h"
#include "NKMath/NkRectangle.h"
#include "NKMath/NkColor.h"
#include <cstddef>

namespace nkentseu {

    // =============================================================================
    // Handle opaque — identifiant GPU (64-bit, 0 = invalide)
    // Chaque ressource GPU est identifiée par un NkRhiHandle<Tag> typé.
    // =============================================================================
    template<typename Tag>
    struct NkRhiHandle {
        uint64 id = 0;
        bool IsValid() const { return id != 0; }
        bool operator==(const NkRhiHandle& o) const { return id == o.id; }
        bool operator!=(const NkRhiHandle& o) const { return id != o.id; }
        static NkRhiHandle Null() { return {0}; }
    };

    // Tags pour les handles
    struct NkTagBuffer    {};
    struct NkTagTexture   {};
    struct NkTagSampler   {};
    struct NkTagShader    {};
    struct NkTagPipeline  {};
    struct NkTagRenderPass{};
    struct NkTagFramebuffer{};
    struct NkTagFence     {};
    struct NkTagSemaphore {};
    struct NkTagDescSet   {};
    struct NkTagBindlessHeap{};

    using NkBufferHandle      = NkRhiHandle<NkTagBuffer>;
    using NkTextureHandle     = NkRhiHandle<NkTagTexture>;
    using NkSamplerHandle     = NkRhiHandle<NkTagSampler>;
    using NkShaderHandle      = NkRhiHandle<NkTagShader>;
    using NkPipelineHandle    = NkRhiHandle<NkTagPipeline>;
    using NkRenderPassHandle  = NkRhiHandle<NkTagRenderPass>;
    using NkFramebufferHandle = NkRhiHandle<NkTagFramebuffer>;
    using NkFenceHandle       = NkRhiHandle<NkTagFence>;
    using NkDescSetHandle     = NkRhiHandle<NkTagDescSet>;
    using NkSemaphoreHandle   = NkRhiHandle<NkTagSemaphore>;
    using NkBindlessHeapHandle= NkRhiHandle<NkTagBindlessHeap>;

    // =============================================================================
    // Formats pixel / vertex
    // =============================================================================
    enum class NkGPUFormat : uint32 {
        NK_UNDEFINED = 0,

        // ── Couleur 8 bits par canal ──
        NK_R8_UNORM, NK_R8_SNORM, NK_R8_UINT, NK_R8_SINT,
        NK_RG8_UNORM, NK_RG8_SNORM,
        NK_RGBA8_UNORM, NK_RGB8_UNORM, NK_RGBA8_SNORM, NK_RGBA8_UINT, NK_RGBA8_SINT,
        NK_RGBA8_SRGB,
        NK_BGRA8_UNORM, NK_BGRA8_SRGB,

        // ── Couleur 16 bits ──
        NK_R16_FLOAT, NK_RG16_FLOAT, NK_RGBA16_FLOAT,
        NK_R16_UINT, NK_R16_SINT, NK_RG16_UINT, NK_RGBA16_UINT,

        // ── Couleur 32 bits ──
        NK_R32_FLOAT, NK_RG32_FLOAT, NK_RGB32_FLOAT, NK_RGBA32_FLOAT,
        NK_R32_UINT, NK_RG32_UINT, NK_RGB32_UINT, NK_RGBA32_UINT,
        NK_R32_SINT, NK_RG32_SINT, NK_RGBA32_SINT,

        // ── Formats vertex spéciaux ──
        NK_R8G8B8A8_UNORM_PACKED,   // vertex color compact
        NK_A2B10G10R10_UNORM,       // normal packed
        NK_R11G11B10_FLOAT,         // HDR color compact

        // ── Depth / Stencil ──
        NK_D16_UNORM,
        NK_D24_UNORM_S8_UINT,
        NK_D32_FLOAT,
        NK_D32_FLOAT_S8_UINT,

        // ── Formats compressés (textures) ──
        NK_BC1_RGB_UNORM,  NK_BC1_RGB_SRGB,
        NK_BC3_UNORM,      NK_BC3_SRGB,
        NK_BC5_UNORM,      NK_BC5_SNORM,
        NK_BC7_UNORM,      NK_BC7_SRGB,
        NK_ETC2_RGB_UNORM, NK_ETC2_RGBA_UNORM,  // mobile
        NK_ASTC_4X4_UNORM, NK_ASTC_4X4_SRGB,   // mobile

        NK_COUNT
    };

    // Utilitaires format
    inline bool NkFormatIsDepth(NkGPUFormat f) {
        return f==NkGPUFormat::NK_D16_UNORM || f==NkGPUFormat::NK_D24_UNORM_S8_UINT
            || f==NkGPUFormat::NK_D32_FLOAT || f==NkGPUFormat::NK_D32_FLOAT_S8_UINT;
    }

    inline bool NkFormatHasStencil(NkGPUFormat f) {
        return f==NkGPUFormat::NK_D24_UNORM_S8_UINT || f==NkGPUFormat::NK_D32_FLOAT_S8_UINT;
    }

    inline bool NkFormatIsSrgb(NkGPUFormat f) {
        return f==NkGPUFormat::NK_RGBA8_SRGB || f==NkGPUFormat::NK_BGRA8_SRGB
            || f==NkGPUFormat::NK_BC1_RGB_SRGB || f==NkGPUFormat::NK_BC3_SRGB
            || f==NkGPUFormat::NK_BC7_SRGB    || f==NkGPUFormat::NK_ASTC_4X4_SRGB
            || f==NkGPUFormat::NK_ETC2_RGB_UNORM; // approximation
    }

    inline uint32 NkFormatBytesPerPixel(NkGPUFormat f) {
        switch(f) {
            case NkGPUFormat::NK_R8_UNORM:     return 1;
            case NkGPUFormat::NK_RG8_UNORM:    return 2;
            case NkGPUFormat::NK_RGBA8_UNORM:
            case NkGPUFormat::NK_BGRA8_UNORM:
            case NkGPUFormat::NK_RGBA8_SRGB:
            case NkGPUFormat::NK_R32_FLOAT:
            case NkGPUFormat::NK_D32_FLOAT:    return 4;
            case NkGPUFormat::NK_RG16_FLOAT:   return 4;
            case NkGPUFormat::NK_RGBA16_FLOAT: return 8;
            case NkGPUFormat::NK_RG32_FLOAT:   return 8;
            case NkGPUFormat::NK_RGBA32_FLOAT: return 16;
            case NkGPUFormat::NK_RGB32_FLOAT:  return 12;
            default:                        return 0;
        }
    }

    // =============================================================================
    // Vertex format pour VertexAttribute
    // Défini par l'utilisateur via NkGPUFormat — le RHI ne préjuge pas de la structure
    // des vertices. Utiliser NkGPUFormat pour décrire chaque attribut.
    // =============================================================================
    using NkVertexFormat = NkGPUFormat;

    inline uint32 NkVertexFormatSize(NkVertexFormat f) {
        return NkFormatBytesPerPixel(f);
    }

    // =============================================================================
    // Index format
    // =============================================================================
    enum class NkIndexFormat : uint32 {
        NK_UINT16,
        NK_UINT32
    };

    // =============================================================================
    // Usage des ressources (buffers, textures)
    // =============================================================================
    enum class NkResourceUsage : uint32 {
        NK_DEFAULT   = 0,    // GPU read/write, pas d'accès CPU direct
        NK_UPLOAD    = 1,    // CPU write → GPU read (staging, uniforms dynamiques)
        NK_READBACK  = 2,    // GPU write → CPU read (screenshots, readbacks)
        NK_IMMUTABLE = 3,    // Init une fois, jamais modifié (géométrie statique)
    };

    // =============================================================================
    // Flags de bind (utilisation de la ressource dans le pipeline)
    // =============================================================================
    enum class NkBindFlags : uint32 {
        NK_NONE            = 0,
        NK_VERTEX_BUFFER   = 1 << 0,
        NK_INDEX_BUFFER    = 1 << 1,
        NK_UNIFORM_BUFFER  = 1 << 2,  // Constant Buffer
        NK_STORAGE_BUFFER  = 1 << 3,  // SSBO / UAV Buffer
        NK_SHADER_RESOURCE = 1 << 4,  // Texture SRV / Buffer SRV
        NK_UNORDERED_ACCESS= 1 << 5,  // UAV / Storage Image
        NK_RENDER_TARGET   = 1 << 6,
        NK_DEPTH_STENCIL   = 1 << 7,
        NK_INDIRECT_ARGS   = 1 << 8,  // Draw/Dispatch indirect
        NK_TRANSFER_SRC    = 1 << 9,
        NK_TRANSFER_DST    = 1 << 10,
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
        NK_VERTEX, NK_INDEX, NK_UNIFORM, NK_STORAGE, NK_INDIRECT, NK_STAGING
    };

    // =============================================================================
    // Type de texture
    // =============================================================================
    enum class NkTextureType : uint32 {
        NK_TEX1D, NK_TEX2D, NK_TEX3D, NK_CUBE, NK_TEX2D_ARRAY, NK_CUBE_ARRAY
    };

    // =============================================================================
    // Sample count (MSAA)
    // =============================================================================
    enum class NkSampleCount : uint32 {
        NK_S1=1, NK_S2=2, NK_S4=4, NK_S8=8, NK_S16=16, NK_S32=32, NK_S64=64
    };

    // =============================================================================
    // Shader stage
    // =============================================================================

    enum class NkShaderStage : uint32 {
        NK_VERTEX       = 1<<0,
        NK_FRAGMENT     = 1<<1,  // Pixel
        NK_GEOMETRY     = 1<<2,
        NK_TESS_CTRL    = 1<<3,
        NK_TESS_EVAL    = 1<<4,
        NK_COMPUTE      = 1<<5,
        NK_MESH         = 1<<6,  // Mesh shaders (DX12/Vulkan 1.2+/Metal 3)
        NK_TASK         = 1<<7,  // Amplification / Task
        NK_ALL_GRAPHICS = NK_VERTEX|NK_FRAGMENT|NK_GEOMETRY|NK_TESS_CTRL|NK_TESS_EVAL,
        NK_ALL          = NK_ALL_GRAPHICS|NK_COMPUTE|NK_MESH|NK_TASK
    };

    inline NkShaderStage operator|(NkShaderStage a, NkShaderStage b) {
        return (NkShaderStage)((uint32)a|(uint32)b);
    }

    // =============================================================================
    // Push Constant Range
    // =============================================================================
    struct NkPushConstantRange {
        NkShaderStage stages = NkShaderStage::NK_ALL_GRAPHICS;
        uint32        offset = 0;
        uint32        size   = 0;
    };

    // =============================================================================
    // Topology primitive
    // =============================================================================
    enum class NkPrimitiveTopology : uint32 {
        NK_TRIANGLE_LIST, NK_TRIANGLE_STRIP, NK_TRIANGLE_FAN,
        NK_LINE_LIST, NK_LINE_STRIP,
        NK_POINT_LIST,
        NK_PATCH_LIST
    };

    // =============================================================================
    // Fill / Cull mode
    // =============================================================================
    enum class NkFillMode : uint32 { NK_SOLID, NK_WIREFRAME, NK_POINT};
    enum class NkCullMode : uint32 { NK_NONE, NK_FRONT, NK_BACK };
    enum class NkFrontFace: uint32 { NK_CCW, NK_CW };

    // =============================================================================
    // Depth / Stencil ops
    // =============================================================================
    enum class NkCompareOp : uint32 {
        NK_NEVER, NK_LESS, NK_EQUAL, NK_LESS_EQUAL,
        NK_GREATER, NK_NOT_EQUAL, NK_GREATER_EQUAL, NK_ALWAYS
    };

    enum class NkStencilOp : uint32 {
        NK_KEEP, NK_ZERO, NK_REPLACE, NK_INCR_CLAMP, NK_DECR_CLAMP, NK_INVERT, NK_INCR_WRAP, NK_DECR_WRAP
    };

    // =============================================================================
    // Blend
    // =============================================================================
    enum class NkBlendFactor : uint32 {
        NK_ZERO, NK_ONE,
        NK_SRC_COLOR, NK_ONE_MINUS_SRC_COLOR, NK_DST_COLOR, NK_ONE_MINUS_DST_COLOR,
        NK_SRC_ALPHA, NK_ONE_MINUS_SRC_ALPHA, NK_DST_ALPHA, NK_ONE_MINUS_DST_ALPHA,
        NK_CONSTANT_COLOR, NK_ONE_MINUS_CONSTANT_COLOR,
        NK_SRC_ALPHA_SATURATE
    };

    enum class NkBlendOp : uint32 { NK_ADD, NK_SUB, NK_REV_SUB, NK_MIN, NK_MAX };

    // =============================================================================
    // Filter / Address mode (samplers)
    // =============================================================================
    enum class NkFilter : uint32 {
        NK_NEAREST, NK_LINEAR, NK_CUBIC
    };

    enum class NkMipFilter : uint32 {
        NK_NONE, NK_NEAREST, NK_LINEAR
    };

    enum class NkAddressMode : uint32 {
        NK_REPEAT, NK_MIRRORED_REPEAT, NK_CLAMP_TO_EDGE, NK_CLAMP_TO_BORDER, NK_MIRROR_CLAMP_TO_EDGE
    };

    enum class NkBorderColor : uint32 {
        NK_TRANSPARENT_BLACK, NK_OPAQUE_BLACK, NK_OPAQUE_WHITE
    };

    // =============================================================================
    // Attachment load / store
    // =============================================================================
    enum class NkLoadOp  : uint32 { NK_LOAD, NK_CLEAR, NK_DONT_CARE };
    enum class NkStoreOp : uint32 { NK_STORE, NK_DONT_CARE, NK_RESOLVE };

    // =============================================================================
    // Pipeline stage (pour les barrières)
    // =============================================================================
    enum class NkPipelineStage : uint32 {
        NK_NONE             = 0,
        NK_TOP_OF_PIPE      = 1<<0,
        NK_VERTEX_INPUT     = 1<<1,
        NK_VERTEX_SHADER    = 1<<2,
        NK_FRAGMENT_SHADER  = 1<<3,
        NK_EARLY_FRAGMENT   = 1<<4,
        NK_LATE_FRAGMENT    = 1<<5,
        NK_COLOR_ATTACHMENT = 1<<6,
        NK_COMPUTE_SHADER   = 1<<7,
        NK_TRANSFER         = 1<<8,
        NK_BOTTOM_OF_PIPE   = 1<<9,
        NK_ALL_GRAPHICS     = 1<<10,
        NK_ALL_COMMANDS     = 1<<11
    };

    inline NkPipelineStage operator|(NkPipelineStage a, NkPipelineStage b) {
        return (NkPipelineStage)((uint32)a|(uint32)b);
    }

    // =============================================================================
    // Resource state (état courant d'une ressource)
    // =============================================================================
    enum class NkResourceState : uint32 {
        NK_UNDEFINED,
        NK_COMMON,
        NK_VERTEX_BUFFER,
        NK_INDEX_BUFFER,
        NK_UNIFORM_BUFFER,
        NK_SHADER_READ,
        NK_SHADER_WRITE,
        NK_RENDER_TARGET,
        NK_DEPTH_READ,
        NK_DEPTH_WRITE,
        NK_PRESENT,
        NK_TRANSFER_SRC,
        NK_TRANSFER_DST,
        NK_UNORDERED_ACCESS,
        NK_INDIRECT_ARG
    };

    // =============================================================================
    // Queue type — pour le multi-queue submit
    // =============================================================================
    enum class NkQueueType : uint32 {
        NK_GRAPHICS = 0,   // graphics + compute + copy (queue principale)
        NK_COMPUTE  = 1,   // compute + copy (async compute queue)
        NK_TRANSFER = 2,   // copy uniquement (DMA engine dédié)
        NK_PRESENT  = 3    // présentation (peut être identique à NK_GRAPHICS)
    };

    // =============================================================================
    // Viewport et scissor
    // =============================================================================
    struct NkViewport {
        float32 x=0, y=0;
        float32 width=0, height=0;
        float32 minDepth=0.f, maxDepth=1.f;
    };

    using NkRect2D = math::NkIntRect;

    using NkScissor = NkRect2D;

    // =============================================================================
    // Clear values
    // =============================================================================
    struct NkClearColor {
        float32 r=0, g=0, b=0, a=1;
        NkClearColor() = default;
        NkClearColor(float32 r, float32 g, float32 b, float32 a=1.f) : r(r),g(g),b(b),a(a) {}
        // Conversion implicite depuis NkColor (uint8 → float32 normalisé)
        NkClearColor(const math::NkColor& c)
            : r(c.r/255.f), g(c.g/255.f), b(c.b/255.f), a(c.a/255.f) {}
    };

    struct NkClearDepth  { float32 depth=1.f; uint32 stencil=0; };
    
    union  NkClearValue  {
        NkClearColor  color;
        NkClearDepth  depthStencil;
        NkClearValue() : color{0,0,0,1} {}
    };

    // =============================================================================
    // Résultat d'opération RHI
    // =============================================================================
    enum class NkRHIResult : uint32 {
        NK_OK = 0,
        NK_OUT_OF_MEMORY,
        NK_DEVICE_LOST,
        NK_INVALID_PARAM,
        NK_NOT_SUPPORTED,
        NK_ALREADY_EXISTS,
        NK_TIMEOUT,
        NK_UNKNOWN,
    };

    inline bool NkSucceeded(NkRHIResult r) { return r == NkRHIResult::NK_OK; }

    inline const char* NkRHIResultName(NkRHIResult r) {
        switch(r) {
            case NkRHIResult::NK_OK:           return "Ok";
            case NkRHIResult::NK_OUT_OF_MEMORY:return "OutOfMemory";
            case NkRHIResult::NK_DEVICE_LOST:  return "DeviceLost";
            case NkRHIResult::NK_INVALID_PARAM:return "InvalidParam";
            case NkRHIResult::NK_NOT_SUPPORTED:return "NotSupported";
            default:                           return "Unknown";
        }
    }

} // namespace nkentseu
