#pragma once
// =============================================================================
// NkRHI_Descs.h
// Descripteurs de création pour toutes les ressources RHI.
// Règle : chaque ressource = un struct *Desc immuable passé à la création.
// Après création, la ressource est identifiée par un NkHandle<> opaque.
// =============================================================================
#include "NkRHI_Types.h"
#include <cstring>
#include <vector>

namespace nkentseu {

// =============================================================================
// Buffer
// =============================================================================
struct NkBufferDesc {
    uint64        sizeBytes    = 0;
    NkBufferType  type         = NkBufferType::Vertex;
    NkResourceUsage usage      = NkResourceUsage::Default;
    NkBindFlags   bindFlags    = NkBindFlags::None;
    const void*   initialData  = nullptr;  // nullptr = non initialisé
    const char*   debugName    = nullptr;

    // Helpers statiques
    static NkBufferDesc Vertex(uint64 sz, const void* data=nullptr) {
        return {sz, NkBufferType::Vertex, NkResourceUsage::Immutable,
                NkBindFlags::VertexBuffer, data};
    }
    static NkBufferDesc VertexDynamic(uint64 sz) {
        return {sz, NkBufferType::Vertex, NkResourceUsage::Upload,
                NkBindFlags::VertexBuffer};
    }
    static NkBufferDesc Index(uint64 sz, const void* data=nullptr) {
        return {sz, NkBufferType::Index, NkResourceUsage::Immutable,
                NkBindFlags::IndexBuffer, data};
    }
    static NkBufferDesc Uniform(uint64 sz) {
        return {sz, NkBufferType::Uniform, NkResourceUsage::Upload,
                NkBindFlags::UniformBuffer};
    }
    static NkBufferDesc Storage(uint64 sz, bool cpuRead=false) {
        NkResourceUsage usage = cpuRead ? NkResourceUsage::Readback : NkResourceUsage::Default;
        return {sz, NkBufferType::Storage, usage,
                NkBindFlags::StorageBuffer | NkBindFlags::UnorderedAccess};
    }
    static NkBufferDesc Staging(uint64 sz) {
        return {sz, NkBufferType::Staging, NkResourceUsage::Upload, NkBindFlags::None};
    }
};

// =============================================================================
// Texture
// =============================================================================
struct NkTextureDesc {
    NkTextureType  type        = NkTextureType::Tex2D;
    NkFormat       format      = NkFormat::RGBA8_Unorm;
    uint32         width       = 1;
    uint32         height      = 1;
    uint32         depth       = 1;       // 1 pour Tex1D/Tex2D/Cube
    uint32         arrayLayers = 1;       // 6 pour Cubemap
    uint32         mipLevels   = 1;       // 0 = génère toute la chaîne
    NkSampleCount  samples     = NkSampleCount::S1;
    NkBindFlags    bindFlags   = NkBindFlags::ShaderResource;
    NkResourceUsage usage      = NkResourceUsage::Default;
    const void*    initialData = nullptr; // données du mip 0, layer 0
    uint32         rowPitch    = 0;       // 0 = auto calculé
    const char*    debugName   = nullptr;

    // ── Helpers ──
    static NkTextureDesc Tex2D(uint32 w, uint32 h, NkFormat fmt=NkFormat::RGBA8_Srgb, uint32 mips=1) {
        return {NkTextureType::Tex2D, fmt, w, h, 1, 1, mips};
    }
    static NkTextureDesc RenderTarget(uint32 w, uint32 h,
                                       NkFormat fmt=NkFormat::RGBA8_Srgb,
                                       NkSampleCount msaa=NkSampleCount::S1) {
        NkTextureDesc d;
        d.type=NkTextureType::Tex2D; d.format=fmt;
        d.width=w; d.height=h; d.mipLevels=1; d.samples=msaa;
        d.bindFlags=NkBindFlags::RenderTarget|NkBindFlags::ShaderResource;
        return d;
    }
    static NkTextureDesc DepthStencil(uint32 w, uint32 h,
                                       NkFormat fmt=NkFormat::D32_Float,
                                       NkSampleCount msaa=NkSampleCount::S1) {
        NkTextureDesc d;
        d.type=NkTextureType::Tex2D; d.format=fmt;
        d.width=w; d.height=h; d.mipLevels=1; d.samples=msaa;
        d.bindFlags=NkBindFlags::DepthStencil|NkBindFlags::ShaderResource;
        return d;
    }
    static NkTextureDesc Cubemap(uint32 sz, NkFormat fmt=NkFormat::RGBA16_Float, uint32 mips=0) {
        return {NkTextureType::Cube, fmt, sz, sz, 1, 6, mips,
                NkSampleCount::S1, NkBindFlags::ShaderResource};
    }
    static NkTextureDesc Tex3D(uint32 w, uint32 h, uint32 d2, NkFormat fmt=NkFormat::R32_Float) {
        return {NkTextureType::Tex3D, fmt, w, h, d2, 1, 1,
                NkSampleCount::S1,
                NkBindFlags::ShaderResource|NkBindFlags::UnorderedAccess};
    }
};

// =============================================================================
// Sampler
// =============================================================================
struct NkSamplerDesc {
    NkFilter      magFilter    = NkFilter::Linear;
    NkFilter      minFilter    = NkFilter::Linear;
    NkMipFilter   mipFilter    = NkMipFilter::Linear;
    NkAddressMode addressU     = NkAddressMode::Repeat;
    NkAddressMode addressV     = NkAddressMode::Repeat;
    NkAddressMode addressW     = NkAddressMode::Repeat;
    float         mipLodBias   = 0.f;
    float         minLod       = -1000.f;
    float         maxLod       =  1000.f;
    float         maxAnisotropy= 1.f;   // 1 = off, 16 = max
    bool          compareEnable= false;
    NkCompareOp   compareOp    = NkCompareOp::LessEqual;
    NkBorderColor borderColor  = NkBorderColor::TransparentBlack;

    // Presets
    static NkSamplerDesc Linear() { return {}; }
    static NkSamplerDesc Nearest() {
        NkSamplerDesc d; d.magFilter=NkFilter::Nearest;
        d.minFilter=NkFilter::Nearest; d.mipFilter=NkMipFilter::Nearest;
        return d;
    }
    static NkSamplerDesc Anisotropic(float n=16.f) {
        NkSamplerDesc d; d.maxAnisotropy=n; return d;
    }
    static NkSamplerDesc Shadow() {
        NkSamplerDesc d;
        d.addressU=d.addressV=d.addressW=NkAddressMode::ClampToEdge;
        d.compareEnable=true; d.compareOp=NkCompareOp::LessEqual;
        d.borderColor=NkBorderColor::OpaqueWhite;
        return d;
    }
    static NkSamplerDesc Clamp() {
        NkSamplerDesc d;
        d.addressU=d.addressV=d.addressW=NkAddressMode::ClampToEdge;
        return d;
    }
};

// =============================================================================
// Vertex layout
// =============================================================================
struct NkVertexAttribute {
    uint32         location   = 0;   // slot dans le shader
    uint32         binding    = 0;   // quel vertex buffer stream
    NkVertexFormat format     = NkVertexFormat::Float3;
    uint32         offset     = 0;   // offset dans le stride
    const char*    semanticName = nullptr; // pour DX11 (POSITION, NORMAL, etc.)
    uint32         semanticIdx  = 0;
};

struct NkVertexBinding {
    uint32 binding   = 0;
    uint32 stride    = 0;
    bool   perInstance = false;  // false=per-vertex, true=per-instance
};

struct NkVertexLayout {
    static constexpr uint32 kMaxAttributes = 16;
    static constexpr uint32 kMaxBindings   = 8;

    NkVertexAttribute attributes[kMaxAttributes];
    NkVertexBinding   bindings[kMaxBindings];
    uint32            attributeCount = 0;
    uint32            bindingCount   = 0;

    // Builder fluide
    NkVertexLayout& AddAttribute(uint32 loc, uint32 bind,
                                  NkVertexFormat fmt, uint32 off,
                                  const char* sem=nullptr, uint32 semIdx=0) {
        attributes[attributeCount++] = {loc,bind,fmt,off,sem,semIdx};
        return *this;
    }
    NkVertexLayout& AddBinding(uint32 bind, uint32 stride, bool instanced=false) {
        bindings[bindingCount++] = {bind,stride,instanced};
        return *this;
    }

    // Layouts communs
    static NkVertexLayout PosOnly() {
        NkVertexLayout l;
        l.AddBinding(0,12).AddAttribute(0,0,NkVertexFormat::Float3,0,"POSITION");
        return l;
    }
    static NkVertexLayout PosNormUV() {
        NkVertexLayout l;
        l.AddBinding(0,32)
         .AddAttribute(0,0,NkVertexFormat::Float3, 0,"POSITION")
         .AddAttribute(1,0,NkVertexFormat::Float3,12,"NORMAL")
         .AddAttribute(2,0,NkVertexFormat::Float2,24,"TEXCOORD");
        return l;
    }
    static NkVertexLayout PosNormTanUV() {
        NkVertexLayout l;
        l.AddBinding(0,48)
         .AddAttribute(0,0,NkVertexFormat::Float3, 0,"POSITION")
         .AddAttribute(1,0,NkVertexFormat::Float3,12,"NORMAL")
         .AddAttribute(2,0,NkVertexFormat::Float4,24,"TANGENT")
         .AddAttribute(3,0,NkVertexFormat::Float2,40,"TEXCOORD");
        return l;
    }
    static NkVertexLayout PosColUV() {
        NkVertexLayout l;
        l.AddBinding(0,24)
         .AddAttribute(0,0,NkVertexFormat::Float3, 0,"POSITION")
         .AddAttribute(1,0,NkVertexFormat::Byte4_Unorm,12,"COLOR")
         .AddAttribute(2,0,NkVertexFormat::Float2,16,"TEXCOORD");
        return l;
    }
};

// =============================================================================
// Rasterizer state
// =============================================================================
struct NkRasterizerDesc {
    NkFillMode fillMode           = NkFillMode::Solid;
    NkCullMode cullMode           = NkCullMode::Back;
    NkFrontFace frontFace         = NkFrontFace::CCW;
    bool        depthClip         = true;
    bool        scissorTest       = false;
    bool        multisampleEnable = false;
    bool        antialiasedLine   = false;
    float       depthBiasConst    = 0.f;
    float       depthBiasSlope    = 0.f;
    float       depthBiasClamp    = 0.f;

    static NkRasterizerDesc Default()       { return {}; }
    static NkRasterizerDesc NoCull()        { NkRasterizerDesc d; d.cullMode=NkCullMode::None; return d; }
    static NkRasterizerDesc Wireframe()     { NkRasterizerDesc d; d.fillMode=NkFillMode::Wireframe; return d; }
    static NkRasterizerDesc ShadowMap() {
        NkRasterizerDesc d;
        d.depthBiasConst=1.25f; d.depthBiasSlope=1.75f;
        d.depthBiasClamp=0.0f; d.cullMode=NkCullMode::Front;
        return d;
    }
};

// =============================================================================
// Depth / Stencil state
// =============================================================================
struct NkStencilOpState {
    NkStencilOp failOp      = NkStencilOp::Keep;
    NkStencilOp depthFailOp = NkStencilOp::Keep;
    NkStencilOp passOp      = NkStencilOp::Keep;
    NkCompareOp compareOp   = NkCompareOp::Always;
    uint32      compareMask = 0xFF;
    uint32      writeMask   = 0xFF;
    uint32      reference   = 0;
};

struct NkDepthStencilDesc {
    bool            depthTestEnable  = true;
    bool            depthWriteEnable = true;
    NkCompareOp     depthCompareOp   = NkCompareOp::Less;
    bool            stencilEnable    = false;
    NkStencilOpState front, back;

    static NkDepthStencilDesc Default()       { return {}; }
    static NkDepthStencilDesc NoDepth()       { NkDepthStencilDesc d; d.depthTestEnable=false; d.depthWriteEnable=false; return d; }
    static NkDepthStencilDesc ReadOnly()      { NkDepthStencilDesc d; d.depthWriteEnable=false; return d; }
    static NkDepthStencilDesc DepthEqual()    { NkDepthStencilDesc d; d.depthCompareOp=NkCompareOp::Equal; d.depthWriteEnable=false; return d; }
    static NkDepthStencilDesc ReverseZ()      { NkDepthStencilDesc d; d.depthCompareOp=NkCompareOp::Greater; return d; }
};

// =============================================================================
// Blend state (par attachment)
// =============================================================================
struct NkBlendAttachment {
    bool          blendEnable      = false;
    NkBlendFactor srcColor         = NkBlendFactor::One;
    NkBlendFactor dstColor         = NkBlendFactor::Zero;
    NkBlendOp     colorOp          = NkBlendOp::Add;
    NkBlendFactor srcAlpha         = NkBlendFactor::One;
    NkBlendFactor dstAlpha         = NkBlendFactor::Zero;
    NkBlendOp     alphaOp          = NkBlendOp::Add;
    uint8         colorWriteMask   = 0xF; // RGBA

    static NkBlendAttachment Opaque()  { return {}; }
    static NkBlendAttachment Alpha() {
        NkBlendAttachment b; b.blendEnable=true;
        b.srcColor=NkBlendFactor::SrcAlpha;
        b.dstColor=NkBlendFactor::OneMinusSrcAlpha;
        b.srcAlpha=NkBlendFactor::One;
        b.dstAlpha=NkBlendFactor::OneMinusSrcAlpha;
        return b;
    }
    static NkBlendAttachment PreMultAlpha() {
        NkBlendAttachment b; b.blendEnable=true;
        b.srcColor=NkBlendFactor::One;
        b.dstColor=NkBlendFactor::OneMinusSrcAlpha;
        b.srcAlpha=NkBlendFactor::One;
        b.dstAlpha=NkBlendFactor::OneMinusSrcAlpha;
        return b;
    }
    static NkBlendAttachment Additive() {
        NkBlendAttachment b; b.blendEnable=true;
        b.srcColor=b.srcAlpha=NkBlendFactor::One;
        b.dstColor=b.dstAlpha=NkBlendFactor::One;
        return b;
    }
};

struct NkBlendDesc {
    static constexpr uint32 kMaxAttachments = 8;
    NkBlendAttachment attachments[kMaxAttachments];
    uint32            attachmentCount  = 1;
    bool              alphaToCoverage  = false;
    float             blendConstants[4]= {0,0,0,0};

    static NkBlendDesc Opaque()  { return {}; }
    static NkBlendDesc Alpha()   { NkBlendDesc d; d.attachments[0]=NkBlendAttachment::Alpha(); return d; }
    static NkBlendDesc Additive(){ NkBlendDesc d; d.attachments[0]=NkBlendAttachment::Additive(); return d; }
};

// =============================================================================
// Shader source / bytecode
// =============================================================================
struct NkShaderStageDesc {
    NkShaderStage stage       = NkShaderStage::Vertex;
    const char*   glslSource  = nullptr;   // GLSL pour OpenGL
    const char*   hlslSource  = nullptr;   // HLSL pour DX11/DX12
    const char*   mslSource   = nullptr;   // MSL pour Metal
    const void*   spirvData   = nullptr;   // SPIR-V pour Vulkan
    uint64        spirvSize   = 0;
    const void*   dxilData    = nullptr;   // DXIL bytecode précompilé
    uint64        dxilSize    = 0;
    const void*   metalIRData = nullptr;   // Metal IR précompilé
    uint64        metalIRSize = 0;
    const char*   entryPoint  = "main";    // "VSMain"/"PSMain" pour HLSL
};

struct NkShaderDesc {
    static constexpr uint32 kMaxStages = 8;
    NkShaderStageDesc stages[kMaxStages];
    uint32            stageCount = 0;
    const char*       debugName  = nullptr;

    NkShaderDesc& AddStage(const NkShaderStageDesc& s) {
        stages[stageCount++] = s; return *this;
    }
    NkShaderDesc& AddGLSL(NkShaderStage stage, const char* src, const char* entry="main") {
        NkShaderStageDesc s{}; s.stage=stage; s.glslSource=src; s.entryPoint=entry;
        return AddStage(s);
    }
    NkShaderDesc& AddHLSL(NkShaderStage stage, const char* src, const char* entry="main") {
        NkShaderStageDesc s{}; s.stage=stage; s.hlslSource=src; s.entryPoint=entry;
        return AddStage(s);
    }
    NkShaderDesc& AddMSL(NkShaderStage stage, const char* src, const char* entry="main") {
        NkShaderStageDesc s{}; s.stage=stage; s.mslSource=src; s.entryPoint=entry;
        return AddStage(s);
    }
    NkShaderDesc& AddSPIRV(NkShaderStage stage, const void* data, uint64 sz) {
        NkShaderStageDesc s{}; s.stage=stage; s.spirvData=data; s.spirvSize=sz;
        return AddStage(s);
    }
};

// =============================================================================
// Render Pass (attachments color + depth + resolve MSAA)
// =============================================================================
struct NkAttachmentDesc {
    NkFormat      format      = NkFormat::RGBA8_Srgb;
    NkSampleCount samples     = NkSampleCount::S1;
    NkLoadOp      loadOp      = NkLoadOp::Clear;
    NkStoreOp     storeOp     = NkStoreOp::Store;
    NkLoadOp      stencilLoad = NkLoadOp::DontCare;
    NkStoreOp     stencilStore= NkStoreOp::DontCare;
    NkClearValue  clearValue;

    static NkAttachmentDesc Color(NkFormat fmt=NkFormat::RGBA8_Srgb,
                                   NkLoadOp load=NkLoadOp::Clear) {
        NkAttachmentDesc a; a.format=fmt; a.loadOp=load; return a;
    }
    static NkAttachmentDesc Depth(NkFormat fmt=NkFormat::D32_Float) {
        NkAttachmentDesc a; a.format=fmt;
        a.clearValue.depthStencil={1.f,0}; return a;
    }
    static NkAttachmentDesc ColorLoad(NkFormat fmt=NkFormat::RGBA8_Srgb) {
        return Color(fmt, NkLoadOp::Load);
    }
};

struct NkRenderPassDesc {
    static constexpr uint32 kMaxColorAttachments = 8;
    NkAttachmentDesc colorAttachments[kMaxColorAttachments];
    uint32           colorCount   = 0;
    bool             hasDepth     = false;
    NkAttachmentDesc depthAttachment;
    bool             hasResolve   = false; // MSAA resolve
    NkAttachmentDesc resolveAttachment;
    const char*      debugName    = nullptr;

    NkRenderPassDesc& AddColor(NkAttachmentDesc a) {
        colorAttachments[colorCount++]=a; return *this;
    }
    NkRenderPassDesc& SetDepth(NkAttachmentDesc a) {
        hasDepth=true; depthAttachment=a; return *this;
    }
    NkRenderPassDesc& SetResolve(NkAttachmentDesc a) {
        hasResolve=true; resolveAttachment=a; return *this;
    }

    // Presets
    static NkRenderPassDesc Forward(NkFormat color=NkFormat::RGBA8_Srgb,
                                     NkFormat depth=NkFormat::D32_Float,
                                     NkSampleCount msaa=NkSampleCount::S1) {
        NkRenderPassDesc rp;
        NkAttachmentDesc c; c.format=color; c.samples=msaa; rp.AddColor(c);
        NkAttachmentDesc d; d.format=depth; d.samples=msaa; rp.SetDepth(d);
        return rp;
    }
    static NkRenderPassDesc ShadowMap(NkFormat depth=NkFormat::D32_Float) {
        NkRenderPassDesc rp;
        rp.SetDepth(NkAttachmentDesc::Depth(depth));
        return rp;
    }
    static NkRenderPassDesc GBuffer() {
        // Albedo, Normal, ORMa (Occlusion/Roughness/Metal/alpha), Emission
        NkRenderPassDesc rp;
        rp.AddColor(NkAttachmentDesc::Color(NkFormat::RGBA8_Srgb))   // albedo
           .AddColor(NkAttachmentDesc::Color(NkFormat::RGBA16_Float)) // normal
           .AddColor(NkAttachmentDesc::Color(NkFormat::RGBA8_Unorm))  // ORM
           .AddColor(NkAttachmentDesc::Color(NkFormat::RGBA16_Float)) // emission
           .SetDepth(NkAttachmentDesc::Depth());
        return rp;
    }
};

// =============================================================================
// Framebuffer (lie un RenderPass à des textures concrètes)
// =============================================================================
struct NkFramebufferDesc {
    NkRenderPassHandle  renderPass;
    static constexpr uint32 kMax = 8;
    NkTextureHandle     colorAttachments[kMax];
    uint32              colorCount  = 0;
    NkTextureHandle     depthAttachment;
    NkTextureHandle     resolveAttachments[kMax];
    uint32              width  = 0;
    uint32              height = 0;
    uint32              layers = 1;
    const char*         debugName = nullptr;
};

// =============================================================================
// Graphics Pipeline
// =============================================================================
struct NkGraphicsPipelineDesc {
    NkShaderHandle       shader;
    NkVertexLayout       vertexLayout;
    NkPrimitiveTopology  topology     = NkPrimitiveTopology::TriangleList;
    NkRasterizerDesc     rasterizer;
    NkDepthStencilDesc   depthStencil;
    NkBlendDesc          blend;
    NkSampleCount        samples      = NkSampleCount::S1;
    NkRenderPassHandle   renderPass;
    uint32               subpass      = 0;
    uint32               patchControlPoints = 3; // pour TessCtrl
    const char*          debugName    = nullptr;
};

// =============================================================================
// Compute Pipeline
// =============================================================================
struct NkComputePipelineDesc {
    NkShaderHandle  shader;
    const char*     debugName = nullptr;
};

// =============================================================================
// Descriptor binding (uniform buffer, texture, sampler, storage)
// =============================================================================
enum class NkDescriptorType : uint32 {
    UniformBuffer,
    UniformBufferDynamic,
    StorageBuffer,
    StorageBufferDynamic,
    SampledTexture,     // texture + sampler séparés
    StorageTexture,     // image load/store
    CombinedImageSampler, // texture + sampler combinés (GL style)
    Sampler,
    InputAttachment,    // subpass input (Vulkan)
};

struct NkDescriptorBinding {
    uint32             binding    = 0;
    NkDescriptorType   type       = NkDescriptorType::UniformBuffer;
    uint32             count      = 1;      // array size
    NkShaderStage      stages     = NkShaderStage::AllGraphics;
};

struct NkDescriptorSetLayoutDesc {
    static constexpr uint32 kMax = 32;
    NkDescriptorBinding bindings[kMax];
    uint32              bindingCount = 0;
    const char*         debugName   = nullptr;

    NkDescriptorSetLayoutDesc& Add(uint32 binding, NkDescriptorType type,
                                    NkShaderStage stages=NkShaderStage::AllGraphics,
                                    uint32 count=1) {
        bindings[bindingCount++]={binding,type,count,stages};
        return *this;
    }
};

// =============================================================================
// Write descriptor (mise à jour d'un descriptor set)
// =============================================================================
struct NkDescriptorWrite {
    NkDescSetHandle    set;
    uint32             binding    = 0;
    uint32             arrayElem  = 0;
    NkDescriptorType   type       = NkDescriptorType::UniformBuffer;
    // Buffer
    NkBufferHandle     buffer;
    uint64             bufferOffset = 0;
    uint64             bufferRange  = 0;
    // Texture + Sampler
    NkTextureHandle    texture;
    NkSamplerHandle    sampler;
    // Layout attendu pour la texture
    NkResourceState    textureLayout = NkResourceState::ShaderRead;
};

// =============================================================================
// Barrier de ressource (transition d'état)
// =============================================================================
struct NkBufferBarrier {
    NkBufferHandle     buffer;
    NkResourceState    stateBefore = NkResourceState::Common;
    NkResourceState    stateAfter  = NkResourceState::ShaderRead;
    NkPipelineStage    srcStage    = NkPipelineStage::AllCommands;
    NkPipelineStage    dstStage    = NkPipelineStage::AllCommands;
};

struct NkTextureBarrier {
    NkTextureHandle    texture;
    NkResourceState    stateBefore = NkResourceState::Common;
    NkResourceState    stateAfter  = NkResourceState::ShaderRead;
    NkPipelineStage    srcStage    = NkPipelineStage::AllCommands;
    NkPipelineStage    dstStage    = NkPipelineStage::AllCommands;
    uint32             baseMip     = 0;
    uint32             mipCount    = UINT32_MAX;
    uint32             baseLayer   = 0;
    uint32             layerCount  = UINT32_MAX;
};

// =============================================================================
// Copy region (buffer ↔ buffer, buffer ↔ texture, texture ↔ texture)
// =============================================================================
struct NkBufferCopyRegion {
    uint64 srcOffset=0, dstOffset=0, size=0;
};

struct NkBufferTextureCopyRegion {
    uint64 bufferOffset=0;
    uint32 bufferRowPitch=0;   // 0 = tight packed
    uint32 mipLevel=0, arrayLayer=0;
    uint32 x=0,y=0,z=0;
    uint32 width,height,depth=1;
};

struct NkTextureCopyRegion {
    uint32 srcMip=0, srcLayer=0;
    uint32 dstMip=0, dstLayer=0;
    uint32 srcX=0,srcY=0,srcZ=0;
    uint32 dstX=0,dstY=0,dstZ=0;
    uint32 width,height,depth=1;
};

// =============================================================================
// Draw / Dispatch indirect args (layout mémoire GPU)
// =============================================================================
struct NkDrawIndirectArgs {
    uint32 vertexCount=0, instanceCount=1;
    uint32 firstVertex=0, firstInstance=0;
};
struct NkDrawIndexedIndirectArgs {
    uint32 indexCount=0, instanceCount=1;
    uint32 firstIndex=0;
    int32  vertexOffset=0;
    uint32 firstInstance=0;
};
struct NkDispatchIndirectArgs {
    uint32 groupsX=1, groupsY=1, groupsZ=1;
};

// =============================================================================
// Push constants
// =============================================================================
struct NkPushConstantRange {
    NkShaderStage stages = NkShaderStage::AllGraphics;
    uint32        offset = 0;
    uint32        size   = 0;   // max 128 bytes (garanti par toutes les APIs)
};

} // namespace nkentseu
