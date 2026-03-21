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
    struct T { uint32 id = 0; \
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

enum class NkBufferUsage : uint32 {
    Vertex          = 1 << 0,
    Index           = 1 << 1,
    Uniform         = 1 << 2,
    Storage         = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
    Indirect        = 1 << 6,
};
inline NkBufferUsage operator|(NkBufferUsage a, NkBufferUsage b) {
    return static_cast<NkBufferUsage>(static_cast<uint32>(a) | static_cast<uint32>(b));
}
inline bool operator&(NkBufferUsage a, NkBufferUsage b) {
    return (static_cast<uint32>(a) & static_cast<uint32>(b)) != 0;
}

enum class NkMemoryType : uint32 {
    GpuOnly     = 0,  ///< DEVICE_LOCAL — buffers vertex/index définitifs
    CpuToGpu    = 1,  ///< HOST_VISIBLE | HOST_COHERENT — staging, UBO
    GpuToCpu    = 2,  ///< Pour readback
};

enum class NkTextureFormat : uint32 {
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

enum class NkTextureUsage : uint32 {
    Sampled         = 1 << 0,
    RenderTarget    = 1 << 1,
    DepthStencil    = 1 << 2,
    Storage         = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
};
inline NkTextureUsage operator|(NkTextureUsage a, NkTextureUsage b) {
    return static_cast<NkTextureUsage>(static_cast<uint32>(a) | static_cast<uint32>(b));
}

enum class NkFilter : uint32 { Nearest, Linear };
enum class NkWrapMode : uint32 { Repeat, MirroredRepeat, ClampToEdge, ClampToBorder };
enum class NkMipmapMode : uint32 { Nearest, Linear };

enum class NkShaderStage : uint32 {
    Vertex      = 1 << 0,
    Fragment    = 1 << 1,
    Compute     = 1 << 2,
    Geometry    = 1 << 3,
    TessControl = 1 << 4,
    TessEval    = 1 << 5,
    All         = 0xFFFFFFFF,
};
inline NkShaderStage operator|(NkShaderStage a, NkShaderStage b) {
    return static_cast<NkShaderStage>(static_cast<uint32>(a) | static_cast<uint32>(b));
}

enum class NkVertexFormat : uint32 {
    Float1, Float2, Float3, Float4,
    Int1,   Int2,   Int3,   Int4,
    UByte4_UNORM,
};

enum class NkIndexType : uint32 { Uint16, Uint32 };

enum class NkPrimitiveTopology : uint32 {
    TriangleList, TriangleStrip, LineList, LineStrip, PointList,
};

enum class NkCullMode : uint32 { None, Front, Back };
enum class NkFrontFace : uint32 { CCW, CW };
enum class NkPolygonMode : uint32 { Fill, Wireframe, Point };
enum class NkCompareOp : uint32 { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

enum class NkBlendFactor : uint32 {
    Zero, One,
    SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
    SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha,
};
enum class NkBlendOp : uint32 { Add, Subtract, ReverseSubtract, Min, Max };

enum class NkLoadOp  : uint32 { Load, Clear, DontCare };
enum class NkStoreOp : uint32 { Store, DontCare };

enum class NkDescriptorType : uint32 {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage,
    InputAttachment,
    PushConstant,       // pas un descriptor Vulkan mais pratique à modéliser ici
};

enum class NkPipelineType : uint32 { Graphics, Compute };

// ===========================================================================
// Descripteurs de création
// ===========================================================================

struct NkBufferDesc {
    uint64         size      = 0;
    NkBufferUsage usage     = NkBufferUsage::Vertex;
    NkMemoryType  memory    = NkMemoryType::GpuOnly;
    const void*   initData  = nullptr; ///< Si non-null : uploadé immédiatement
    const char*   debugName = nullptr;
};

struct NkTextureDesc {
    uint32           width       = 1;
    uint32           height      = 1;
    uint32           depth       = 1;
    uint32           mipLevels   = 1;
    uint32           arrayLayers = 1;
    NkTextureFormat format      = NkTextureFormat::RGBA8_SRGB;
    NkTextureUsage  usage       = NkTextureUsage::Sampled | NkTextureUsage::TransferDst;
    uint32           samples     = 1;
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
    uint32         codeSize = 0;       ///< En bytes
    const char*   entry    = "main";
    const char*   debugName= nullptr;
};

struct NkVertexAttribute {
    uint32         location = 0;
    uint32         binding  = 0;
    uint32         offset   = 0;
    NkVertexFormat format  = NkVertexFormat::Float3;
};

struct NkVertexBinding {
    uint32 binding   = 0;
    uint32 stride    = 0;
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
    uint32         writeMask     = 0xF; // RGBA
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
    uint32           binding    = 0;
    NkDescriptorType type      = NkDescriptorType::UniformBuffer;
    uint32           count      = 1;
    NkShaderStage   stages     = NkShaderStage::All;
};

struct NkDescriptorLayoutDesc {
    static constexpr uint32 kMaxBindings = 16;
    NkDescriptorBinding bindings[kMaxBindings];
    uint32               bindingCount = 0;
    const char*         debugName    = nullptr;
};

struct NkAttachmentDesc {
    NkTextureFormat format      = NkTextureFormat::RGBA8_SRGB;
    NkLoadOp        loadOp      = NkLoadOp::Clear;
    NkStoreOp       storeOp     = NkStoreOp::Store;
    NkLoadOp        stencilLoad = NkLoadOp::DontCare;
    NkStoreOp       stencilStore= NkStoreOp::DontCare;
    uint32           samples     = 1;
    bool            isDepth     = false;
};

struct NkRenderPassDesc {
    static constexpr uint32 kMaxAttachments = 8;
    NkAttachmentDesc colorAttachments[kMaxAttachments - 1];
    uint32            colorCount    = 0;
    NkAttachmentDesc depthAttachment;
    bool             hasDepth      = false;
    const char*      debugName     = nullptr;
};

struct NkPipelineDesc {
    NkPipelineType      type         = NkPipelineType::Graphics;
    NkRenderPassHandle  renderPass;
    uint32               subpass      = 0;

    // Shaders
    static constexpr uint32 kMaxShaders = 6;
    NkShaderHandle      shaders[kMaxShaders];
    uint32               shaderCount  = 0;

    // Vertex input
    static constexpr uint32 kMaxAttribs  = 16;
    static constexpr uint32 kMaxBindings = 4;
    NkVertexAttribute   attributes[kMaxAttribs];
    uint32               attributeCount = 0;
    NkVertexBinding     bindings[kMaxBindings];
    uint32               bindingCount   = 0;

    // États fixes
    NkPrimitiveTopology topology       = NkPrimitiveTopology::TriangleList;
    NkRasterState       raster;
    NkDepthStencilState depthStencil;

    // Blend (un par color attachment)
    NkBlendState        blends[kMaxAttachments - 1];

    // Descriptor layouts
    static constexpr uint32 kMaxSets = 4;
    NkDescriptorLayoutHandle layouts[kMaxSets];
    uint32               layoutCount    = 0;

    // Push constants
    uint32               pushConstantSize  = 0;
    NkShaderStage       pushConstantStages= NkShaderStage::Vertex | NkShaderStage::Fragment;

    // Compute only
    NkShaderHandle      computeShader;

    const char*         debugName = nullptr;
};

struct NkFramebufferDesc {
    NkRenderPassHandle  renderPass;
    static constexpr uint32 kMaxAttachments = 8;
    NkTextureHandle     attachments[kMaxAttachments];
    uint32               attachmentCount = 0;
    uint32               width  = 0;
    uint32               height = 0;
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
    int32 x = 0, y = 0;
    uint32 width = 0, height = 0;
};

struct NkClearValue {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
    float depth   = 1.f;
    uint32 stencil = 0;
};

struct NkDrawCall {
    uint32 vertexCount   = 0;
    uint32 instanceCount = 1;
    uint32 firstVertex   = 0;
    uint32 firstInstance = 0;
};

struct NkDrawIndexedCall {
    uint32 indexCount    = 0;
    uint32 instanceCount = 1;
    uint32 firstIndex    = 0;
    int32 vertexOffset  = 0;
    uint32 firstInstance = 0;
};

// ===========================================================================
// Descriptor update
// ===========================================================================

struct NkDescriptorWrite {
    uint32             binding     = 0;
    uint32             arrayIndex  = 0;
    NkDescriptorType  type        = NkDescriptorType::UniformBuffer;
    // Union selon type
    NkBufferHandle    buffer;
    uint64             bufferOffset= 0;
    uint64             bufferRange = 0;
    NkTextureHandle   texture;
    NkSamplerHandle   sampler;
};

}} // namespace nkentseu::rhi