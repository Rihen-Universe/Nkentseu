#pragma once
// =============================================================================
// NkRHI.h — Rendering Hardware Interface
// Couche d'abstraction entre le moteur de rendu et les APIs graphiques.
//
// Principe : le code moteur ne voit QUE des handles opaques (NkBufferHandle,
// NkTextureHandle…) et des commandes abstraites (DrawIndexed, SetPipeline…).
// Le backend traduit ces commandes en appels Vulkan / OpenGL / Metal / DX.
//
// Inspiré de : Unreal RHI, Filament Backend, bgfx, NVRHI.
// =============================================================================

#include "NKWindow/Core/NkTypes.h"
#include <cstdint>
#include <memory>
#include <functional>

namespace nkentseu { namespace rhi {

// ===========================================================================
// Handles opaques — simples entiers 32 bits
// ===========================================================================

#define NK_DEFINE_HANDLE(T) \
    struct T { NkU32 id = 0; \
               bool IsValid() const { return id != 0; } \
               bool operator==(const T& o) const { return id == o.id; } \
               bool operator!=(const T& o) const { return id != o.id; } \
               static T Null() { return {0}; } }

NK_DEFINE_HANDLE(NkBufferHandle);
NK_DEFINE_HANDLE(NkTextureHandle);
NK_DEFINE_HANDLE(NkSamplerHandle);
NK_DEFINE_HANDLE(NkShaderHandle);
NK_DEFINE_HANDLE(NkPipelineHandle);
NK_DEFINE_HANDLE(NkRenderPassHandle);
NK_DEFINE_HANDLE(NkFramebufferHandle);
NK_DEFINE_HANDLE(NkDescriptorSetHandle);
NK_DEFINE_HANDLE(NkDescriptorLayoutHandle);
NK_DEFINE_HANDLE(NkCommandBufferHandle);

#undef NK_DEFINE_HANDLE

// ===========================================================================
// Enums — indépendants de toute API
// ===========================================================================

enum class NkBufferUsage : NkU32 {
    Vertex          = 1 << 0,
    Index           = 1 << 1,
    Uniform         = 1 << 2,
    Storage         = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
    Indirect        = 1 << 6,
};
inline NkBufferUsage operator|(NkBufferUsage a, NkBufferUsage b) {
    return static_cast<NkBufferUsage>(static_cast<NkU32>(a) | static_cast<NkU32>(b));
}
inline bool operator&(NkBufferUsage a, NkBufferUsage b) {
    return (static_cast<NkU32>(a) & static_cast<NkU32>(b)) != 0;
}

enum class NkMemoryType : NkU32 {
    GpuOnly     = 0,  ///< DEVICE_LOCAL — buffers vertex/index définitifs
    CpuToGpu    = 1,  ///< HOST_VISIBLE | HOST_COHERENT — staging, UBO
    GpuToCpu    = 2,  ///< Pour readback
};

enum class NkTextureFormat : NkU32 {
    Undefined       = 0,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,
    R8_UNORM,
    RG8_UNORM,
    RGBA16F,
    RGBA32F,
    R32F,
    RG32F,
    Depth16,
    Depth24Stencil8,
    Depth32F,
    BC1_RGBA_UNORM,
    BC3_RGBA_UNORM,
    BC7_RGBA_UNORM,
};

enum class NkTextureUsage : NkU32 {
    Sampled         = 1 << 0,
    RenderTarget    = 1 << 1,
    DepthStencil    = 1 << 2,
    Storage         = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
};
inline NkTextureUsage operator|(NkTextureUsage a, NkTextureUsage b) {
    return static_cast<NkTextureUsage>(static_cast<NkU32>(a) | static_cast<NkU32>(b));
}

enum class NkFilter : NkU32 { Nearest, Linear };
enum class NkWrapMode : NkU32 { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };
enum class NkMipmapMode : NkU32 { Nearest, Linear };

enum class NkShaderStage : NkU32 {
    Vertex      = 1 << 0,
    Fragment    = 1 << 1,
    Compute     = 1 << 2,
    Geometry    = 1 << 3,
    TessControl = 1 << 4,
    TessEval    = 1 << 5,
    All         = 0xFFFFFFFF,
};
inline NkShaderStage operator|(NkShaderStage a, NkShaderStage b) {
    return static_cast<NkShaderStage>(static_cast<NkU32>(a) | static_cast<NkU32>(b));
}

enum class NkVertexFormat : NkU32 {
    Float1, Float2, Float3, Float4,
    Int1,   Int2,   Int3,   Int4,
    UByte4_UNORM,
};

enum class NkIndexType : NkU32 { Uint16, Uint32 };

enum class NkPrimitiveTopology : NkU32 {
    TriangleList, TriangleStrip, LineList, LineStrip, PointList,
};

enum class NkCullMode : NkU32 { None, Front, Back };
enum class NkFrontFace : NkU32 { CCW, CW };
enum class NkPolygonMode : NkU32 { Fill, Wireframe, Point };
enum class NkCompareOp : NkU32 { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

enum class NkBlendFactor : NkU32 {
    Zero, One,
    SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
    SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha,
};
enum class NkBlendOp : NkU32 { Add, Subtract, ReverseSubtract, Min, Max };

enum class NkLoadOp  : NkU32 { Load, Clear, DontCare };
enum class NkStoreOp : NkU32 { Store, DontCare };

enum class NkDescriptorType : NkU32 {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage,
    InputAttachment,
    PushConstant,       // pas un descriptor Vulkan mais pratique à modéliser ici
};

enum class NkPipelineType : NkU32 { Graphics, Compute };

// ===========================================================================
// Descripteurs de création
// ===========================================================================

struct NkBufferDesc {
    NkU64         size      = 0;
    NkBufferUsage usage     = NkBufferUsage::Vertex;
    NkMemoryType  memory    = NkMemoryType::GpuOnly;
    const void*   initData  = nullptr; ///< Si non-null : uploadé immédiatement
    const char*   debugName = nullptr;
};

struct NkTextureDesc {
    NkU32           width       = 1;
    NkU32           height      = 1;
    NkU32           depth       = 1;
    NkU32           mipLevels   = 1;
    NkU32           arrayLayers = 1;
    NkTextureFormat format      = NkTextureFormat::RGBA8_SRGB;
    NkTextureUsage  usage       = NkTextureUsage::Sampled | NkTextureUsage::TransferDst;
    NkU32           samples     = 1;
    const char*     debugName   = nullptr;
};

struct NkSamplerDesc {
    NkFilter      minFilter    = NkFilter::Linear;
    NkFilter      magFilter    = NkFilter::Linear;
    NkMipmapMode  mipmapMode   = NkMipmapMode::Linear;
    NkWrapMode    wrapU        = NkWrapMode::Repeat;
    NkWrapMode    wrapV        = NkWrapMode::Repeat;
    NkWrapMode    wrapW        = NkWrapMode::Repeat;
    float         mipLodBias   = 0.f;
    float         minLod       = 0.f;
    float         maxLod       = 1000.f;
    bool          anisotropy   = true;
    float         maxAnisotropy= 16.f;
    bool          compareEnable= false;
    NkCompareOp   compareOp    = NkCompareOp::Less;
};

struct NkShaderDesc {
    NkShaderStage stage    = NkShaderStage::Vertex;
    const void*   code     = nullptr; ///< SPIR-V pour Vulkan, GLSL pour OpenGL
    NkU32         codeSize = 0;       ///< En bytes
    const char*   entry    = "main";
    const char*   debugName= nullptr;
};

struct NkVertexAttribute {
    NkU32         location = 0;
    NkU32         binding  = 0;
    NkU32         offset   = 0;
    NkVertexFormat format  = NkVertexFormat::Float3;
};

struct NkVertexBinding {
    NkU32 binding   = 0;
    NkU32 stride    = 0;
    bool  instanced = false;
};

struct NkBlendState {
    bool          enabled       = false;
    NkBlendFactor srcColor      = NkBlendFactor::SrcAlpha;
    NkBlendFactor dstColor      = NkBlendFactor::OneMinusSrcAlpha;
    NkBlendOp     colorOp       = NkBlendOp::Add;
    NkBlendFactor srcAlpha      = NkBlendFactor::One;
    NkBlendFactor dstAlpha      = NkBlendFactor::Zero;
    NkBlendOp     alphaOp       = NkBlendOp::Add;
    NkU32         writeMask     = 0xF; // RGBA
};

struct NkRasterState {
    NkCullMode       cullMode      = NkCullMode::Back;
    NkFrontFace      frontFace     = NkFrontFace::CCW;
    NkPolygonMode    polygonMode   = NkPolygonMode::Fill;
    bool             depthBias     = false;
    float            depthBiasConst= 0.f;
    float            depthBiasSlope= 0.f;
    bool             depthClamp    = false;
};

struct NkDepthStencilState {
    bool        depthTest   = true;
    bool        depthWrite  = true;
    NkCompareOp depthOp     = NkCompareOp::Less;
    bool        stencilTest = false;
};

struct NkDescriptorBinding {
    NkU32           binding    = 0;
    NkDescriptorType type      = NkDescriptorType::UniformBuffer;
    NkU32           count      = 1;
    NkShaderStage   stages     = NkShaderStage::All;
};

struct NkDescriptorLayoutDesc {
    static constexpr NkU32 kMaxBindings = 16;
    NkDescriptorBinding bindings[kMaxBindings];
    NkU32               bindingCount = 0;
    const char*         debugName    = nullptr;
};

struct NkAttachmentDesc {
    NkTextureFormat format      = NkTextureFormat::RGBA8_SRGB;
    NkLoadOp        loadOp      = NkLoadOp::Clear;
    NkStoreOp       storeOp     = NkStoreOp::Store;
    NkLoadOp        stencilLoad = NkLoadOp::DontCare;
    NkStoreOp       stencilStore= NkStoreOp::DontCare;
    NkU32           samples     = 1;
    bool            isDepth     = false;
};

struct NkRenderPassDesc {
    static constexpr NkU32 kMaxAttachments = 8;
    NkAttachmentDesc colorAttachments[kMaxAttachments - 1];
    NkU32            colorCount    = 0;
    NkAttachmentDesc depthAttachment;
    bool             hasDepth      = false;
    const char*      debugName     = nullptr;
};

struct NkPipelineDesc {
    NkPipelineType      type         = NkPipelineType::Graphics;
    NkRenderPassHandle  renderPass;
    NkU32               subpass      = 0;

    // Shaders
    static constexpr NkU32 kMaxShaders = 6;
    NkShaderHandle      shaders[kMaxShaders];
    NkU32               shaderCount  = 0;

    // Vertex input
    static constexpr NkU32 kMaxAttribs  = 16;
    static constexpr NkU32 kMaxBindings = 4;
    NkVertexAttribute   attributes[kMaxAttribs];
    NkU32               attributeCount = 0;
    NkVertexBinding     bindings[kMaxBindings];
    NkU32               bindingCount   = 0;

    // États fixes
    NkPrimitiveTopology topology       = NkPrimitiveTopology::TriangleList;
    NkRasterState       raster;
    NkDepthStencilState depthStencil;

    // Blend (un par color attachment)
    NkBlendState        blends[kMaxAttachments - 1];

    // Descriptor layouts
    static constexpr NkU32 kMaxSets = 4;
    NkDescriptorLayoutHandle layouts[kMaxSets];
    NkU32               layoutCount    = 0;

    // Push constants
    NkU32               pushConstantSize  = 0;
    NkShaderStage       pushConstantStages= NkShaderStage::Vertex | NkShaderStage::Fragment;

    // Compute only
    NkShaderHandle      computeShader;

    const char*         debugName = nullptr;
};

struct NkFramebufferDesc {
    NkRenderPassHandle  renderPass;
    static constexpr NkU32 kMaxAttachments = 8;
    NkTextureHandle     attachments[kMaxAttachments];
    NkU32               attachmentCount = 0;
    NkU32               width  = 0;
    NkU32               height = 0;
    const char*         debugName = nullptr;
};

// ===========================================================================
// Commandes de rendu (enregistrées dans un command buffer)
// ===========================================================================

struct NkViewport {
    float x = 0.f, y = 0.f, width = 0.f, height = 0.f;
    float minDepth = 0.f, maxDepth = 1.f;
};

struct NkScissor {
    NkI32 x = 0, y = 0;
    NkU32 width = 0, height = 0;
};

struct NkClearValue {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
    float depth   = 1.f;
    NkU32 stencil = 0;
};

struct NkDrawCall {
    NkU32 vertexCount   = 0;
    NkU32 instanceCount = 1;
    NkU32 firstVertex   = 0;
    NkU32 firstInstance = 0;
};

struct NkDrawIndexedCall {
    NkU32 indexCount    = 0;
    NkU32 instanceCount = 1;
    NkU32 firstIndex    = 0;
    NkI32 vertexOffset  = 0;
    NkU32 firstInstance = 0;
};

// ===========================================================================
// Descriptor update
// ===========================================================================

struct NkDescriptorWrite {
    NkU32             binding     = 0;
    NkU32             arrayIndex  = 0;
    NkDescriptorType  type        = NkDescriptorType::UniformBuffer;
    // Union selon type
    NkBufferHandle    buffer;
    NkU64             bufferOffset= 0;
    NkU64             bufferRange = 0;
    NkTextureHandle   texture;
    NkSamplerHandle   sampler;
};

}} // namespace nkentseu::rhi