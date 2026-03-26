#pragma once
// =============================================================================
// NkDescs.h
// Descripteurs de création pour toutes les ressources RHI.
// Règle : chaque ressource = un struct *Desc immuable passé à la création.
// Après création, la ressource est identifiée par un NkRhiHandle<> opaque.
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include <cstring>

namespace nkentseu {

    class NkICommandBuffer; // forward déclaration pour NkSubmitInfo

    // =============================================================================
    // Buffer
    // =============================================================================
    struct NkBufferDesc {
        uint64        sizeBytes    = 0;
        NkBufferType  type         = NkBufferType::NK_VERTEX;
        NkResourceUsage usage      = NkResourceUsage::NK_DEFAULT;
        NkBindFlags   bindFlags    = NkBindFlags::NK_NONE;
        const void*   initialData  = nullptr;  // nullptr = non initialisé
        const char*   debugName    = nullptr;

        // Helpers statiques
        static NkBufferDesc Vertex(uint64 sz, const void* data=nullptr) {
            return {sz, NkBufferType::NK_VERTEX, NkResourceUsage::NK_IMMUTABLE,
                    NkBindFlags::NK_VERTEX_BUFFER, data};
        }

        static NkBufferDesc VertexDynamic(uint64 sz) {
            return {sz, NkBufferType::NK_VERTEX, NkResourceUsage::NK_UPLOAD,
                    NkBindFlags::NK_VERTEX_BUFFER};
        }

        static NkBufferDesc Index(uint64 sz, const void* data=nullptr) {
            return {sz, NkBufferType::NK_INDEX, NkResourceUsage::NK_IMMUTABLE,
                    NkBindFlags::NK_INDEX_BUFFER, data};
        }

        static NkBufferDesc IndexDynamic(uint64 sz) {
            return {sz, NkBufferType::NK_INDEX, NkResourceUsage::NK_UPLOAD,
                    NkBindFlags::NK_INDEX_BUFFER};
        }

        static NkBufferDesc Uniform(uint64 sz) {
            return {sz, NkBufferType::NK_UNIFORM, NkResourceUsage::NK_UPLOAD,
                    NkBindFlags::NK_UNIFORM_BUFFER};
        }

        static NkBufferDesc Storage(uint64 sz, bool cpuRead=false) {
            NkResourceUsage usage = cpuRead ? NkResourceUsage::NK_READBACK : NkResourceUsage::NK_DEFAULT;
            return {sz, NkBufferType::NK_STORAGE, usage,
                    NkBindFlags::NK_STORAGE_BUFFER | NkBindFlags::NK_UNORDERED_ACCESS};
        }

        static NkBufferDesc Staging(uint64 sz) {
            return {sz, NkBufferType::NK_STAGING, NkResourceUsage::NK_UPLOAD, NkBindFlags::NK_NONE};
        }
    };

    // =============================================================================
    // Texture
    // =============================================================================
    struct NkTextureDesc {
        NkTextureType  type        = NkTextureType::NK_TEX2D;
        NkGPUFormat       format      = NkGPUFormat::NK_RGBA8_UNORM;
        uint32         width       = 1;
        uint32         height      = 1;
        uint32         depth       = 1;       // 1 pour Tex1D/Tex2D/Cube
        uint32         arrayLayers = 1;       // 6 pour Cubemap
        uint32         mipLevels   = 1;       // 0 = génère toute la chaîne
        NkSampleCount  samples     = NkSampleCount::NK_S1;
        NkBindFlags    bindFlags   = NkBindFlags::NK_SHADER_RESOURCE;
        NkResourceUsage usage      = NkResourceUsage::NK_DEFAULT;
        const void*    initialData = nullptr; // données du mip 0, layer 0
        uint32         rowPitch    = 0;       // 0 = auto calculé
        const char*    debugName   = nullptr;

        // ── Helpers ──
        static NkTextureDesc Tex2D(uint32 w, uint32 h, NkGPUFormat fmt=NkGPUFormat::NK_RGBA8_SRGB, uint32 mips=1) {
            return {NkTextureType::NK_TEX2D, fmt, w, h, 1, 1, mips};
        }

        static NkTextureDesc RenderTarget(uint32 w, uint32 h,
                                        NkGPUFormat fmt=NkGPUFormat::NK_RGBA8_SRGB,
                                        NkSampleCount msaa=NkSampleCount::NK_S1) {
            NkTextureDesc d;
            d.type=NkTextureType::NK_TEX2D; d.format=fmt;
            d.width=w; d.height=h; d.mipLevels=1; d.samples=msaa;
            d.bindFlags=NkBindFlags::NK_RENDER_TARGET|NkBindFlags::NK_SHADER_RESOURCE;
            return d;
        }

        static NkTextureDesc DepthStencil(uint32 w, uint32 h,
                                        NkGPUFormat fmt=NkGPUFormat::NK_D32_FLOAT,
                                        NkSampleCount msaa=NkSampleCount::NK_S1) {
            NkTextureDesc d;
            d.type=NkTextureType::NK_TEX2D; d.format=fmt;
            d.width=w; d.height=h; d.mipLevels=1; d.samples=msaa;
            d.bindFlags=NkBindFlags::NK_DEPTH_STENCIL|NkBindFlags::NK_SHADER_RESOURCE;
            return d;
        }

        static NkTextureDesc Cubemap(uint32 sz, NkGPUFormat fmt=NkGPUFormat::NK_RGBA16_FLOAT, uint32 mips=0) {
            return {NkTextureType::NK_CUBE, fmt, sz, sz, 1, 6, mips,
                    NkSampleCount::NK_S1, NkBindFlags::NK_SHADER_RESOURCE};
        }

        static NkTextureDesc Tex3D(uint32 w, uint32 h, uint32 d2, NkGPUFormat fmt=NkGPUFormat::NK_R32_FLOAT) {
            return {NkTextureType::NK_TEX3D, fmt, w, h, d2, 1, 1,
                    NkSampleCount::NK_S1,
                    NkBindFlags::NK_SHADER_RESOURCE|NkBindFlags::NK_UNORDERED_ACCESS};
        }
    };

    // =============================================================================
    // Sampler
    // =============================================================================
    struct NkSamplerDesc {
        NkFilter      magFilter    = NkFilter::NK_LINEAR;
        NkFilter      minFilter    = NkFilter::NK_LINEAR;
        NkMipFilter   mipFilter    = NkMipFilter::NK_LINEAR;
        NkAddressMode addressU     = NkAddressMode::NK_REPEAT;
        NkAddressMode addressV     = NkAddressMode::NK_REPEAT;
        NkAddressMode addressW     = NkAddressMode::NK_REPEAT;
        float32         mipLodBias   = 0.f;
        float32         minLod       = -1000.f;
        float32         maxLod       =  1000.f;
        float32         maxAnisotropy= 1.f;   // 1 = off, 16 = max
        bool          compareEnable= false;
        NkCompareOp   compareOp    = NkCompareOp::NK_LESS_EQUAL;
        NkBorderColor borderColor  = NkBorderColor::NK_TRANSPARENT_BLACK;

        // Presets
        static NkSamplerDesc Linear() { return {}; }

        static NkSamplerDesc Nearest() {
            NkSamplerDesc d; d.magFilter=NkFilter::NK_NEAREST;
            d.minFilter=NkFilter::NK_NEAREST; d.mipFilter=NkMipFilter::NK_NEAREST;
            return d;
        }

        static NkSamplerDesc Anisotropic(float32 n=16.f) {
            NkSamplerDesc d; d.maxAnisotropy=n; return d;
        }

        static NkSamplerDesc Shadow() {
            NkSamplerDesc d;
            d.addressU=d.addressV=d.addressW=NkAddressMode::NK_CLAMP_TO_EDGE;
            d.compareEnable=true; d.compareOp=NkCompareOp::NK_LESS_EQUAL;
            d.borderColor=NkBorderColor::NK_OPAQUE_WHITE;
            return d;
        }

        static NkSamplerDesc Clamp() {
            NkSamplerDesc d;
            d.addressU=d.addressV=d.addressW=NkAddressMode::NK_CLAMP_TO_EDGE;
            return d;
        }
    };

    // =============================================================================
    // Vertex layout
    // =============================================================================
    struct NkVertexAttribute {
        uint32         location   = 0;   // slot dans le shader
        uint32         binding    = 0;   // quel vertex buffer stream
        NkVertexFormat format     = NkGPUFormat::NK_RGB32_FLOAT;
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
        NkVector<NkVertexAttribute> attributes;
        NkVector<NkVertexBinding>   bindings;

        // Builder fluide
        NkVertexLayout& AddAttribute(uint32 loc, uint32 bind,
                                    NkVertexFormat fmt, uint32 off,
                                    const char* sem=nullptr, uint32 semIdx=0) {
            attributes.PushBack({loc,bind,fmt,off,sem,semIdx});
            return *this;
        }

        NkVertexLayout& AddBinding(uint32 bind, uint32 stride, bool instanced=false) {
            bindings.PushBack({bind,stride,instanced});
            return *this;
        }
    };

    // =============================================================================
    // Rasterizer state
    // =============================================================================
    struct NkRasterizerDesc {
        NkFillMode fillMode           = NkFillMode::NK_SOLID;
        NkCullMode cullMode           = NkCullMode::NK_BACK;
        NkFrontFace frontFace         = NkFrontFace::NK_CCW;
        bool        depthClip         = true;
        bool        scissorTest       = false;
        bool        multisampleEnable = false;
        bool        antialiasedLine   = false;
        float32       depthBiasConst    = 0.f;
        float32       depthBiasSlope    = 0.f;
        float32       depthBiasClamp    = 0.f;

        static NkRasterizerDesc Default()       { return {}; }
        static NkRasterizerDesc NoCull()        { NkRasterizerDesc d; d.cullMode=NkCullMode::NK_NONE; return d; }
        static NkRasterizerDesc Wireframe()     { NkRasterizerDesc d; d.fillMode=NkFillMode::NK_WIREFRAME; return d; }
        static NkRasterizerDesc ShadowMap() {
            NkRasterizerDesc d;
            d.depthBiasConst=1.25f; d.depthBiasSlope=1.75f;
            d.depthBiasClamp=0.0f; d.cullMode=NkCullMode::NK_FRONT;
            return d;
        }
    };

    // =============================================================================
    // Depth / Stencil state
    // =============================================================================
    struct NkStencilOpState {
        NkStencilOp failOp      = NkStencilOp::NK_KEEP;
        NkStencilOp depthFailOp = NkStencilOp::NK_KEEP;
        NkStencilOp passOp      = NkStencilOp::NK_KEEP;
        NkCompareOp compareOp   = NkCompareOp::NK_ALWAYS;
        uint32      compareMask = 0xFF;
        uint32      writeMask   = 0xFF;
        uint32      reference   = 0;
    };

    struct NkDepthStencilDesc {
        bool            depthTestEnable  = true;
        bool            depthWriteEnable = true;
        NkCompareOp     depthCompareOp   = NkCompareOp::NK_LESS;
        bool            stencilEnable    = false;
        NkStencilOpState front, back;

        static NkDepthStencilDesc Default()       { return {}; }
        static NkDepthStencilDesc NoDepth()       { NkDepthStencilDesc d; d.depthTestEnable=false; d.depthWriteEnable=false; return d; }
        static NkDepthStencilDesc ReadOnly()      { NkDepthStencilDesc d; d.depthWriteEnable=false; return d; }
        static NkDepthStencilDesc DepthEqual()    { NkDepthStencilDesc d; d.depthCompareOp=NkCompareOp::NK_EQUAL; d.depthWriteEnable=false; return d; }
        static NkDepthStencilDesc ReverseZ()      { NkDepthStencilDesc d; d.depthCompareOp=NkCompareOp::NK_GREATER; return d; }
    };

    // =============================================================================
    // Blend state (par attachment)
    // =============================================================================
    struct NkBlendAttachment {
        bool          blendEnable      = false;
        NkBlendFactor srcColor         = NkBlendFactor::NK_ONE;
        NkBlendFactor dstColor         = NkBlendFactor::NK_ZERO;
        NkBlendOp     colorOp          = NkBlendOp::NK_ADD;
        NkBlendFactor srcAlpha         = NkBlendFactor::NK_ONE;
        NkBlendFactor dstAlpha         = NkBlendFactor::NK_ZERO;
        NkBlendOp     alphaOp          = NkBlendOp::NK_ADD;
        uint8         colorWriteMask   = 0xF; // RGBA

        static NkBlendAttachment Opaque()  { return {}; }
        static NkBlendAttachment Alpha() {
            NkBlendAttachment b; b.blendEnable=true;
            b.srcColor=NkBlendFactor::NK_SRC_ALPHA;
            b.dstColor=NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
            b.srcAlpha=NkBlendFactor::NK_ONE;
            b.dstAlpha=NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
            return b;
        }
        static NkBlendAttachment PreMultAlpha() {
            NkBlendAttachment b; b.blendEnable=true;
            b.srcColor=NkBlendFactor::NK_ONE;
            b.dstColor=NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
            b.srcAlpha=NkBlendFactor::NK_ONE;
            b.dstAlpha=NkBlendFactor::NK_ONE_MINUS_SRC_ALPHA;
            return b;
        }
        static NkBlendAttachment Additive() {
            NkBlendAttachment b; b.blendEnable=true;
            b.srcColor=b.srcAlpha=NkBlendFactor::NK_ONE;
            b.dstColor=b.dstAlpha=NkBlendFactor::NK_ONE;
            return b;
        }
    };

    struct NkBlendDesc {
        NkVector<NkBlendAttachment> attachments;
        bool              alphaToCoverage  = false;
        float32           blendConstants[4]= {0,0,0,0};

        NkBlendDesc() { attachments.PushBack(NkBlendAttachment::Opaque()); }

        static NkBlendDesc Opaque()  { return NkBlendDesc{}; }
        static NkBlendDesc Alpha()   { NkBlendDesc d; d.attachments[0]=NkBlendAttachment::Alpha(); return d; }
        static NkBlendDesc Additive(){ NkBlendDesc d; d.attachments[0]=NkBlendAttachment::Additive(); return d; }
    };

    // =============================================================================
    // Shader source / bytecode
    // =============================================================================
    struct NkShaderStageDesc {
        NkShaderStage stage       = NkShaderStage::NK_VERTEX;
        const char*   glslSource  = nullptr;   // GLSL pour OpenGL
        const char*   hlslSource  = nullptr;   // HLSL pour DX11/DX12
        const char*   mslSource   = nullptr;   // MSL pour Metal
        NkVector<uint8> spirvBinary;
        const void*   dxilData    = nullptr;   // DXIL bytecode précompilé
        uint64        dxilSize    = 0;
        const void*   metalIRData = nullptr;   // Metal IR précompilé
        uint64        metalIRSize = 0;
        const char*   entryPoint  = "main";    // "VSMain"/"PSMain" pour HLSL

        // Backend Software — fonctions CPU (opaques, castées dans NkSoftwareDevice)
        const void*   cpuVertFn   = nullptr;   // NkSWVertexShader* (software rasterizer)
        const void*   cpuFragFn   = nullptr;   // NkSWPixelShader*
        const void*   cpuCompFn   = nullptr;   // NkSWComputeShader*
    };

    struct NkShaderDesc {
        NkVector<NkShaderStageDesc> stages;
        const char*                 debugName = nullptr;

        NkShaderDesc& AddStage(const NkShaderStageDesc& s) {
            stages.PushBack(s); return *this;
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
            NkShaderStageDesc s{}; 
            s.stage=stage; 
            s.spirvBinary.Resize((uint32)sz);
            memcpy(s.spirvBinary.Data(), data, (size_t)sz);
            return AddStage(s);
        }
    };

    // =============================================================================
    // Render Pass (attachments color + depth + resolve MSAA)
    // =============================================================================
    struct NkAttachmentDesc {
        NkGPUFormat      format      = NkGPUFormat::NK_RGBA8_SRGB;
        NkSampleCount samples     = NkSampleCount::NK_S1;
        NkLoadOp      loadOp      = NkLoadOp::NK_CLEAR;
        NkStoreOp     storeOp     = NkStoreOp::NK_STORE;
        NkLoadOp      stencilLoad = NkLoadOp::NK_DONT_CARE;
        NkStoreOp     stencilStore= NkStoreOp::NK_DONT_CARE;
        NkClearValue  clearValue;

        static NkAttachmentDesc Color(NkGPUFormat fmt=NkGPUFormat::NK_RGBA8_SRGB,
                                    NkLoadOp load=NkLoadOp::NK_CLEAR) {
            NkAttachmentDesc a; a.format=fmt; a.loadOp=load; return a;
        }
        static NkAttachmentDesc Depth(NkGPUFormat fmt=NkGPUFormat::NK_D32_FLOAT) {
            NkAttachmentDesc a; a.format=fmt;
            a.clearValue.depthStencil={1.f,0}; return a;
        }
        static NkAttachmentDesc ColorLoad(NkGPUFormat fmt=NkGPUFormat::NK_RGBA8_SRGB) {
            return Color(fmt, NkLoadOp::NK_LOAD);
        }
    };

    struct NkRenderPassDesc {
        NkVector<NkAttachmentDesc> colorAttachments;
        bool             hasDepth     = false;
        NkAttachmentDesc depthAttachment;
        bool             hasResolve   = false; // MSAA resolve
        NkAttachmentDesc resolveAttachment;
        const char*      debugName    = nullptr;

        NkRenderPassDesc& AddColor(NkAttachmentDesc a) {
            colorAttachments.PushBack(a); return *this;
        }

        NkRenderPassDesc& SetDepth(NkAttachmentDesc a) {
            hasDepth=true; depthAttachment=a; return *this;
        }

        NkRenderPassDesc& SetResolve(NkAttachmentDesc a) {
            hasResolve=true; resolveAttachment=a; return *this;
        }

        // Presets
        static NkRenderPassDesc Forward(NkGPUFormat color=NkGPUFormat::NK_RGBA8_SRGB,
                                        NkGPUFormat depth=NkGPUFormat::NK_D32_FLOAT,
                                        NkSampleCount msaa=NkSampleCount::NK_S1) {
            NkRenderPassDesc rp;
            NkAttachmentDesc c; c.format=color; c.samples=msaa; rp.AddColor(c);
            NkAttachmentDesc d; d.format=depth; d.samples=msaa; rp.SetDepth(d);
            return rp;
        }

        static NkRenderPassDesc ShadowMap(NkGPUFormat depth=NkGPUFormat::NK_D32_FLOAT) {
            NkRenderPassDesc rp;
            rp.SetDepth(NkAttachmentDesc::Depth(depth));
            return rp;
        }
        
        static NkRenderPassDesc GBuffer() {
            // Albedo, Normal, ORMa (Occlusion/Roughness/Metal/alpha), Emission
            NkRenderPassDesc rp;
            rp.AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_SRGB))    // albedo
            .AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA16_FLOAT))  // normal
            .AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA8_UNORM))   // ORM
            .AddColor(NkAttachmentDesc::Color(NkGPUFormat::NK_RGBA16_FLOAT))  // emission
            .SetDepth(NkAttachmentDesc::Depth());
            return rp;
        }
    };

    // =============================================================================
    // Framebuffer (lie un RenderPass à des textures concrètes)
    // =============================================================================
    struct NkFramebufferDesc {
        NkRenderPassHandle        renderPass;
        NkVector<NkTextureHandle> colorAttachments;
        NkTextureHandle           depthAttachment;
        NkVector<NkTextureHandle> resolveAttachments;
        uint32                    width  = 0;
        uint32                    height = 0;
        uint32                    layers = 1;
        const char*               debugName = nullptr;
    };

    // =============================================================================
    // Graphics Pipeline
    // =============================================================================
    struct NkGraphicsPipelineDesc {
        NkShaderHandle       shader;
        NkVertexLayout       vertexLayout;
        NkPrimitiveTopology  topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
        NkRasterizerDesc     rasterizer;
        NkDepthStencilDesc   depthStencil;
        NkBlendDesc          blend;
        NkSampleCount        samples      = NkSampleCount::NK_S1;
        NkRenderPassHandle   renderPass;
        uint32               subpass      = 0;
        uint32               patchControlPoints = 3;

        // Push constants — déclarés ici pour que le pipeline layout soit correct
        // (obligatoire sur Vulkan/DX12 pour que les push constants soient valides)
        NkVector<NkPushConstantRange> pushConstants;

        // Descriptor set layouts à inclure dans le pipeline layout (obligatoire sur Vulkan)
        NkVector<NkDescSetHandle> descriptorSetLayouts;

        const char*          debugName    = nullptr;

        NkGraphicsPipelineDesc& AddPushConstant(NkShaderStage stages, uint32 offset, uint32 size) {
            pushConstants.PushBack({stages, offset, size});
            return *this;
        }
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
        NK_UNIFORM_BUFFER,
        NK_UNIFORM_BUFFER_DYNAMIC,
        NK_STORAGE_BUFFER,
        NK_STORAGE_BUFFER_DYNAMIC,
        NK_SAMPLED_TEXTURE,     // texture + sampler séparés
        NK_STORAGE_TEXTURE,     // image load/store
        NK_COMBINED_IMAGE_SAMPLER, // texture + sampler combinés (GL style)
        NK_SAMPLER,
        NK_INPUT_ATTACHMENT,    // subpass input (Vulkan)
    };

    struct NkDescriptorBinding {
        uint32             binding    = 0;
        NkDescriptorType   type       = NkDescriptorType::NK_UNIFORM_BUFFER;
        uint32             count      = 1;      // array size
        NkShaderStage      stages     = NkShaderStage::NK_ALL_GRAPHICS;
    };

    struct NkDescriptorSetLayoutDesc {
        NkVector<NkDescriptorBinding> bindings;
        bool                    isBindless  = false;
        const char*             debugName   = nullptr;

        NkDescriptorSetLayoutDesc& Add(uint32 binding, NkDescriptorType type,
                                        NkShaderStage stages=NkShaderStage::NK_ALL_GRAPHICS,
                                        uint32 count=1) {
            bindings.PushBack({binding, type, count, stages});
            return *this;
        }

        // Binding d'un tableau de N ressources — pour bindless descriptor indexing
        NkDescriptorSetLayoutDesc& AddBindless(uint32 binding, NkDescriptorType type,
                                                NkShaderStage stages, uint32 maxCount) {
            isBindless = true;
            return Add(binding, type, stages, maxCount);
        }

        uint32 TotalBindingCount() const { return (uint32)bindings.Size(); }
    };

    // =============================================================================
    // Write descriptor (mise à jour d'un descriptor set)
    // =============================================================================
    struct NkDescriptorWrite {
        NkDescSetHandle    set;
        uint32             binding    = 0;
        uint32             arrayElem  = 0;
        NkDescriptorType   type       = NkDescriptorType::NK_UNIFORM_BUFFER;
        // Buffer
        NkBufferHandle     buffer;
        uint64             bufferOffset = 0;
        uint64             bufferRange  = 0;
        // Texture + Sampler
        NkTextureHandle    texture;
        NkSamplerHandle    sampler;
        // Layout attendu pour la texture
        NkResourceState    textureLayout = NkResourceState::NK_SHADER_READ;
    };

    // =============================================================================
    // Barrier de ressource (transition d'état)
    // =============================================================================
    struct NkBufferBarrier {
        NkBufferHandle     buffer;
        NkResourceState    stateBefore = NkResourceState::NK_COMMON;
        NkResourceState    stateAfter  = NkResourceState::NK_SHADER_READ;
        NkPipelineStage    srcStage    = NkPipelineStage::NK_ALL_COMMANDS;
        NkPipelineStage    dstStage    = NkPipelineStage::NK_ALL_COMMANDS;
    };

    struct NkTextureBarrier {
        NkTextureHandle    texture;
        NkResourceState    stateBefore = NkResourceState::NK_COMMON;
        NkResourceState    stateAfter  = NkResourceState::NK_SHADER_READ;
        NkPipelineStage    srcStage    = NkPipelineStage::NK_ALL_COMMANDS;
        NkPipelineStage    dstStage    = NkPipelineStage::NK_ALL_COMMANDS;
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
    // Swapchain
    // =============================================================================
    struct NkSwapchainDesc {
        uint32          width       = 0;        // 0 = taille de la fenêtre
        uint32          height      = 0;
        NkGPUFormat        colorFormat = NkGPUFormat::NK_BGRA8_UNORM;
        NkGPUFormat        depthFormat = NkGPUFormat::NK_D32_FLOAT;
        NkSampleCount   samples     = NkSampleCount::NK_S1;
        uint32          imageCount  = 3;        // 2=double, 3=triple buffering
        bool            vsync       = true;
        bool            hdr         = false;    // HDR si disponible
        const char*     debugName   = nullptr;
    };

    // =============================================================================
    // Bindless descriptor heap
    // =============================================================================
    struct NkBindlessHeapDesc {
        uint32 maxSampledTextures  = 65536;
        uint32 maxStorageTextures  = 16384;
        uint32 maxUniformBuffers   = 16384;
        uint32 maxStorageBuffers   = 16384;
        uint32 maxSamplers         = 2048;
        const char* debugName      = nullptr;
    };

    // =============================================================================
    // Submit info — soumission avec semaphores (GPU-GPU sync sans CPU)
    // =============================================================================
    struct NkSubmitInfo {
        NkICommandBuffer* const* commandBuffers       = nullptr;
        uint32                   commandBufferCount   = 0;
        const NkSemaphoreHandle* waitSemaphores       = nullptr;
        const NkPipelineStage*   waitStages           = nullptr;  // un par waitSemaphore
        uint32                   waitSemaphoreCount   = 0;
        const NkSemaphoreHandle* signalSemaphores     = nullptr;
        uint32                   signalSemaphoreCount = 0;
        NkFenceHandle            fence;   // signalé quand le GPU a fini
    };

} // namespace nkentseu
