#pragma once
// =============================================================================
// NkOpenGLRenderer.h
// Implémentation OpenGL du NkBaseRenderer.
//
// Traduit les appels NKRenderer → handles RHI NkIDevice OpenGL.
// Gère la compilation GLSL, le binding des descriptors sets,
// et la soumission des draw calls triés.
// =============================================================================
#include "NKRenderer/Core/NkBaseRenderer.h"
#include "NKRenderer/Shaders/OpenGL/NkShaders_OpenGL.h"
#include "NKRenderer/Shaders/Vulkan/NkShaders_Vulkan.h"
#include "NKRenderer/Shaders/DX11/NkShaders_DX11.h"
#include "NKRenderer/Shaders/DX12/NkShaders_DX12.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
namespace renderer {

    class NkOpenGLRenderer : public NkBaseRenderer {
    public:
        explicit NkOpenGLRenderer(NkIDevice* device)
            : NkBaseRenderer(device) {}

        ~NkOpenGLRenderer() override = default;

    protected:
        // ── NkBaseRenderer — impl ─────────────────────────────────────────────
        bool CreateShader_Impl   (NkShaderEntry& e)                                         override;
        void DestroyShader_Impl  (NkShaderEntry& e)                                         override;

        bool CreateTexture_Impl  (NkTextureEntry& e)                                        override;
        void DestroyTexture_Impl (NkTextureEntry& e)                                        override;
        bool UpdateTexture_Impl  (NkTextureEntry& e, const void* d,
                                   uint32 rp, uint32 mip, uint32 layer)                    override;

        bool CreateMesh_Impl     (NkMeshEntry& e)                                           override;
        void DestroyMesh_Impl    (NkMeshEntry& e)                                           override;
        bool UpdateMesh_Impl     (NkMeshEntry& e, const void* v, uint32 vc,
                                   const void* i, uint32 ic)                               override;

        bool CreateMaterial_Impl (NkMaterialEntry& e)                                       override;
        void DestroyMaterial_Impl(NkMaterialEntry& e)                                       override;

        bool CreateMatInst_Impl  (NkMaterialInstEntry& e, const NkMaterialEntry& tmpl)     override;
        void DestroyMatInst_Impl (NkMaterialInstEntry& e)                                   override;
        bool RebuildMatInst_Impl (NkMaterialInstEntry& e, const NkMaterialEntry& tmpl)     override;

        bool CreateRenderTarget_Impl (NkRenderTargetEntry& e)                               override;
        void DestroyRenderTarget_Impl(NkRenderTargetEntry& e)                               override;

        bool CreateFont_Impl     (NkFontEntry& e)                                           override;
        void DestroyFont_Impl    (NkFontEntry& e)                                           override;

        // Frame
        bool BeginFrame_Impl     ()                                                         override;
        bool BeginRenderPass_Impl(const NkRenderPassInfo& info,
                                   NkICommandBuffer* cmd)                                   override;
        void SubmitDrawCalls_Impl(NkRendererCommand& drawCmd,
                                   NkICommandBuffer* rhiCmd,
                                   const NkRenderPassInfo& pass)                            override;
        void EndRenderPass_Impl  (NkICommandBuffer* cmd)                                    override;
        void EndFrame_Impl       (NkICommandBuffer* cmd)                                    override;

    private:
        // ── Helpers internes ─────────────────────────────────────────────────

        // Sélectionne les sources GLSL adaptées (OpenGL vs Vulkan selon mApi)
        bool BuildRhiShaderDesc(const NkShaderEntry& e, nkentseu::NkShaderDesc& out) const;

        // Construit un NkVertexLayout depuis un NkMeshEntry
        nkentseu::NkVertexLayout BuildVertexLayout(const NkMeshEntry& e) const;

        // Convertit NkTextureFormat → NkGPUFormat RHI
        static nkentseu::NkGPUFormat ToRhiFormat(NkTextureFormat fmt);

        // Convertit NkSamplerFilter → NkFilter / NkMipFilter RHI
        static nkentseu::NkFilter    ToRhiFilter(NkSamplerFilter f);
        static nkentseu::NkMipFilter ToRhiMipFilter(NkSamplerFilter f);
        static nkentseu::NkAddressMode ToRhiWrap(NkSamplerWrap w);
        static nkentseu::NkCullMode  ToRhiCull(NkCullMode c);
        static nkentseu::NkFillMode  ToRhiFill(NkFillMode f);
        static nkentseu::NkBlendDesc ToRhiBlend(NkBlendMode b);
        static nkentseu::NkPrimitiveTopology ToRhiTopology(NkPrimitiveMode m);

        // Émet les draw calls 3D
        void Submit3D(NkRendererCommand& drawCmd,
                      NkICommandBuffer* rhiCmd,
                      const NkRenderPassInfo& pass);

        // Émet les draw calls 2D (sprites, primitives, texte)
        void Submit2D(NkRendererCommand& drawCmd,
                      NkICommandBuffer* rhiCmd);

        // Émet les debug draws (wireframe, grid…)
        void SubmitDebug(NkRendererCommand& drawCmd, NkICommandBuffer* rhiCmd);

        // UBO per-frame (caméra, temps)
        struct FrameUBOData {
            float view[16];
            float proj[16];
            float viewProj[16];
            float cameraPos[4];
            float time[4];
        };
        struct DrawUBOData {
            float model[16];
            float modelIT[16];
            float objectColor[4];
        };
        struct LightsUBOData {
            float dirLightDir[4];
            float dirLightColor[4];   // w = intensity
            float pointPos[8][4];     // w = radius
            float pointColor[8][4];   // w = intensity
            int32 numPointLights;
            int32 _pad[3];
        };

        // Buffers UBO persistants par frame
        nkentseu::NkBufferHandle mFrameUBO;
        nkentseu::NkBufferHandle mDrawUBO;
        nkentseu::NkBufferHandle mLightsUBO;

        // Layout descriptor commun (binding 0=FrameUBO, 1=DrawUBO, 2=MaterialUBO, 3=LightsUBO)
        nkentseu::NkDescSetHandle mCommonLayout;

        // Sampler par défaut
        nkentseu::NkSamplerHandle mDefaultSampler;

        // Pipeline state courant (évite les rebinds inutiles)
        uint64 mBoundPipeline = 0;
        uint64 mBoundVBO      = 0;
        uint64 mBoundIBO      = 0;

        bool mFrameStarted = false;
    };

    // =========================================================================
    // NkOpenGLRenderer — implémentation des conversions de types
    // =========================================================================
    inline nkentseu::NkGPUFormat NkOpenGLRenderer::ToRhiFormat(NkTextureFormat fmt) {
        switch (fmt) {
            case NkTextureFormat::RGBA8_SRGB:  return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
            case NkTextureFormat::RGBA8_UNORM: return nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
            case NkTextureFormat::RGBA16F:     return nkentseu::NkGPUFormat::NK_RGBA16_FLOAT;
            case NkTextureFormat::RGBA32F:     return nkentseu::NkGPUFormat::NK_RGBA32_FLOAT;
            case NkTextureFormat::RG16F:       return nkentseu::NkGPUFormat::NK_RG16_FLOAT;
            case NkTextureFormat::R32F:        return nkentseu::NkGPUFormat::NK_R32_FLOAT;
            case NkTextureFormat::BC1_SRGB:    return nkentseu::NkGPUFormat::NK_BC1_RGB_SRGB;
            case NkTextureFormat::BC3_SRGB:    return nkentseu::NkGPUFormat::NK_BC3_SRGB;
            case NkTextureFormat::BC5_UNORM:   return nkentseu::NkGPUFormat::NK_BC5_UNORM;
            case NkTextureFormat::BC7_SRGB:    return nkentseu::NkGPUFormat::NK_BC7_SRGB;
            case NkTextureFormat::D32F:        return nkentseu::NkGPUFormat::NK_D32_FLOAT;
            case NkTextureFormat::D24S8:       return nkentseu::NkGPUFormat::NK_D24_UNORM_S8_UINT;
            default:                           return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
        }
    }

    inline nkentseu::NkFilter NkOpenGLRenderer::ToRhiFilter(NkSamplerFilter f) {
        switch (f) {
            case NkSamplerFilter::Nearest:     return nkentseu::NkFilter::NK_NEAREST;
            case NkSamplerFilter::Anisotropic: return nkentseu::NkFilter::NK_LINEAR;
            default:                           return nkentseu::NkFilter::NK_LINEAR;
        }
    }

    inline nkentseu::NkMipFilter NkOpenGLRenderer::ToRhiMipFilter(NkSamplerFilter f) {
        return (f == NkSamplerFilter::Nearest)
            ? nkentseu::NkMipFilter::NK_NEAREST
            : nkentseu::NkMipFilter::NK_LINEAR;
    }

    inline nkentseu::NkAddressMode NkOpenGLRenderer::ToRhiWrap(NkSamplerWrap w) {
        switch (w) {
            case NkSamplerWrap::ClampToEdge:    return nkentseu::NkAddressMode::NK_CLAMP_TO_EDGE;
            case NkSamplerWrap::ClampToBorder:  return nkentseu::NkAddressMode::NK_CLAMP_TO_BORDER;
            case NkSamplerWrap::MirroredRepeat: return nkentseu::NkAddressMode::NK_MIRRORED_REPEAT;
            default:                            return nkentseu::NkAddressMode::NK_REPEAT;
        }
    }

    inline nkentseu::NkCullMode NkOpenGLRenderer::ToRhiCull(NkCullMode c) {
        switch (c) {
            case NkCullMode::Front: return nkentseu::NkCullMode::NK_FRONT;
            case NkCullMode::None:  return nkentseu::NkCullMode::NK_NONE;
            default:                return nkentseu::NkCullMode::NK_BACK;
        }
    }

    inline nkentseu::NkFillMode NkOpenGLRenderer::ToRhiFill(NkFillMode f) {
        return (f == NkFillMode::Wireframe)
            ? nkentseu::NkFillMode::NK_WIREFRAME
            : nkentseu::NkFillMode::NK_SOLID;
    }

    inline nkentseu::NkBlendDesc NkOpenGLRenderer::ToRhiBlend(NkBlendMode b) {
        switch (b) {
            case NkBlendMode::AlphaBlend:   return nkentseu::NkBlendDesc::Alpha();
            case NkBlendMode::Additive:     return nkentseu::NkBlendDesc::Additive();
            case NkBlendMode::PreMultAlpha: {
                nkentseu::NkBlendDesc d;
                d.attachments[0] = nkentseu::NkBlendAttachment::PreMultAlpha();
                return d;
            }
            default: return nkentseu::NkBlendDesc::Opaque();
        }
    }

    inline nkentseu::NkPrimitiveTopology NkOpenGLRenderer::ToRhiTopology(NkPrimitiveMode m) {
        switch (m) {
            case NkPrimitiveMode::Lines:         return nkentseu::NkPrimitiveTopology::NK_LINE_LIST;
            case NkPrimitiveMode::Points:        return nkentseu::NkPrimitiveTopology::NK_POINT_LIST;
            case NkPrimitiveMode::TriangleStrip: return nkentseu::NkPrimitiveTopology::NK_TRIANGLE_STRIP;
            default:                             return nkentseu::NkPrimitiveTopology::NK_TRIANGLE_LIST;
        }
    }

    inline nkentseu::NkVertexLayout NkOpenGLRenderer::BuildVertexLayout(const NkMeshEntry& e) const {
        nkentseu::NkVertexLayout layout;
        if (e.desc.Is2D()) {
            layout
                .AddAttribute(0, 0, nkentseu::NkGPUFormat::NK_RG32_FLOAT,   0,                "POSITION", 0)
                .AddAttribute(1, 0, nkentseu::NkGPUFormat::NK_RG32_FLOAT,   8,                "TEXCOORD", 0)
                .AddAttribute(2, 0, nkentseu::NkGPUFormat::NK_RGBA32_FLOAT, 16,               "COLOR",    0)
                .AddBinding(0, (uint32)sizeof(NkVertex2D));
        } else {
            layout
                .AddAttribute(0, 0, nkentseu::NkGPUFormat::NK_RGB32_FLOAT,  0,                "POSITION", 0)
                .AddAttribute(1, 0, nkentseu::NkGPUFormat::NK_RGB32_FLOAT,  3*sizeof(float32), "NORMAL",  0)
                .AddAttribute(2, 0, nkentseu::NkGPUFormat::NK_RGB32_FLOAT,  6*sizeof(float32), "TANGENT", 0)
                .AddAttribute(3, 0, nkentseu::NkGPUFormat::NK_RG32_FLOAT,   9*sizeof(float32), "TEXCOORD",0)
                .AddAttribute(4, 0, nkentseu::NkGPUFormat::NK_RG32_FLOAT,  11*sizeof(float32), "TEXCOORD",1)
                .AddAttribute(5, 0, nkentseu::NkGPUFormat::NK_RGBA32_FLOAT,13*sizeof(float32), "COLOR",   0)
                .AddBinding(0, (uint32)sizeof(NkVertex3D));
        }
        return layout;
    }

    // =========================================================================
    // Implémentation des méthodes _Impl (dans NkOpenGLRenderer.cpp)
    // =========================================================================
    inline bool NkOpenGLRenderer::BuildRhiShaderDesc(const NkShaderEntry& e,
                                                      nkentseu::NkShaderDesc& out) const
    {
        out.debugName = e.desc.debugName;
        const NkShaderBackend backend = CurrentBackend();

        for (usize si = 0; si < e.desc.stages.Size(); ++si) {
            const NkShaderStageDesc& stage = e.desc.stages[si];
            if (stage.backend != backend) continue;

            nkentseu::NkShaderStageDesc rhi{};
            // Mapper le stage NKRenderer → stage RHI
            switch (stage.stage) {
                case NkShaderStageType::Vertex:   rhi.stage = nkentseu::NkShaderStage::NK_VERTEX;   break;
                case NkShaderStageType::Fragment: rhi.stage = nkentseu::NkShaderStage::NK_FRAGMENT; break;
                case NkShaderStageType::Geometry: rhi.stage = nkentseu::NkShaderStage::NK_GEOMETRY; break;
                case NkShaderStageType::TessCtrl: rhi.stage = nkentseu::NkShaderStage::NK_TESS_CTRL;break;
                case NkShaderStageType::TessEval: rhi.stage = nkentseu::NkShaderStage::NK_TESS_EVAL;break;
                case NkShaderStageType::Compute:  rhi.stage = nkentseu::NkShaderStage::NK_COMPUTE;  break;
            }

            const char* src = stage.source.CStr();
            rhi.entryPoint = stage.entryPoint.CStr();

            switch (backend) {
                case NkShaderBackend::OpenGL:
                case NkShaderBackend::Vulkan:  rhi.glslSource = src; break;
                case NkShaderBackend::DX11:
                case NkShaderBackend::DX12:    rhi.hlslSource = src; break;
            }
            out.stages.PushBack(rhi);
        }
        return !out.stages.IsEmpty();
    }

    inline bool NkOpenGLRenderer::CreateShader_Impl(NkShaderEntry& e) {
        nkentseu::NkShaderDesc rhiDesc;
        if (!BuildRhiShaderDesc(e, rhiDesc)) {
            logger_src.Infof("[NKRenderer] CreateShader_Impl: aucune variante pour le backend courant\n");
            return false;
        }
        nkentseu::NkShaderHandle h = mDevice->CreateShader(rhiDesc);
        if (!h.IsValid()) return false;
        e.rhiHandle.id = h.id;
        return true;
    }

    inline void NkOpenGLRenderer::DestroyShader_Impl(NkShaderEntry& e) {
        nkentseu::NkShaderHandle h; h.id = e.rhiHandle.id;
        if (h.IsValid()) mDevice->DestroyShader(h);
        e.rhiHandle.id = 0;
    }

    inline bool NkOpenGLRenderer::CreateTexture_Impl(NkTextureEntry& e) {
        const NkTextureDesc& td = e.desc;
        nkentseu::NkTextureDesc rhi{};

        switch (td.type) {
            case NkTextureType::Cube:    rhi.type = nkentseu::NkTextureType::NK_CUBE; break;
            case NkTextureType::Tex3D:   rhi.type = nkentseu::NkTextureType::NK_TEX3D; break;
            case NkTextureType::Array2D: rhi.type = nkentseu::NkTextureType::NK_TEX2D_ARRAY; break;
            default:                     rhi.type = nkentseu::NkTextureType::NK_TEX2D; break;
        }
        rhi.format      = ToRhiFormat(td.format);
        rhi.width       = td.width;
        rhi.height      = td.height;
        rhi.depth       = td.depth;
        rhi.arrayLayers = td.layers;
        rhi.mipLevels   = td.mipLevels;
        rhi.samples     = nkentseu::NkSampleCount::NK_S1;
        rhi.debugName   = td.debugName;
        rhi.initialData = td.initialData;
        rhi.rowPitch    = td.rowPitch;

        if (td.isRenderTarget) {
            rhi.bindFlags = nkentseu::NkBindFlags::NK_RENDER_TARGET
                          | nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
            rhi.usage     = nkentseu::NkResourceUsage::NK_DEFAULT;
        } else if (td.isDepthTarget) {
            rhi.bindFlags = nkentseu::NkBindFlags::NK_DEPTH_STENCIL
                          | nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
        } else {
            rhi.bindFlags = nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
            rhi.usage     = td.initialData ? nkentseu::NkResourceUsage::NK_DEFAULT
                                           : nkentseu::NkResourceUsage::NK_DEFAULT;
        }

        nkentseu::NkTextureHandle tex = mDevice->CreateTexture(rhi);
        if (!tex.IsValid()) return false;
        e.rhiTextureId = tex.id;

        // Sampler
        nkentseu::NkSamplerDesc sd{};
        sd.magFilter    = ToRhiFilter(td.sampler.filter);
        sd.minFilter    = ToRhiFilter(td.sampler.filter);
        sd.mipFilter    = ToRhiMipFilter(td.sampler.filter);
        sd.addressU     = ToRhiWrap(td.sampler.wrapU);
        sd.addressV     = ToRhiWrap(td.sampler.wrapV);
        sd.addressW     = ToRhiWrap(td.sampler.wrapW);
        sd.maxAnisotropy= td.sampler.anisotropy;
        sd.mipLodBias   = td.sampler.mipLodBias;

        nkentseu::NkSamplerHandle samp = mDevice->CreateSampler(sd);
        if (!samp.IsValid()) {
            mDevice->DestroyTexture(tex);
            return false;
        }
        e.rhiSamplerId = samp.id;

        return true;
    }

    inline void NkOpenGLRenderer::DestroyTexture_Impl(NkTextureEntry& e) {
        nkentseu::NkTextureHandle t; t.id = e.rhiTextureId;
        nkentseu::NkSamplerHandle s; s.id = e.rhiSamplerId;
        if (t.IsValid()) mDevice->DestroyTexture(t);
        if (s.IsValid()) mDevice->DestroySampler(s);
        e.rhiTextureId = e.rhiSamplerId = 0;
    }

    inline bool NkOpenGLRenderer::UpdateTexture_Impl(NkTextureEntry& e, const void* data,
                                                      uint32 rowPitch, uint32 mip, uint32 layer) {
        nkentseu::NkTextureHandle t; t.id = e.rhiTextureId;
        if (!t.IsValid()) return false;
        if (mip == 0 && layer == 0)
            return mDevice->WriteTexture(t, data, rowPitch);
        return mDevice->WriteTextureRegion(t, data,
            0, 0, 0, e.desc.width, e.desc.height, 1, mip, layer, rowPitch);
    }

    inline bool NkOpenGLRenderer::CreateMesh_Impl(NkMeshEntry& e) {
        const NkMeshDesc& md = e.desc;

        // Calculer la taille du vertex
        uint32 vertexStride = 0;
        const void* vertexData = nullptr;
        if (md.Is2D()) {
            vertexStride = sizeof(NkVertex2D);
            vertexData   = md.vertices2D;
        } else if (md.IsSkinned()) {
            vertexStride = sizeof(NkVertexSkinned);
            vertexData   = md.verticesSkinned;
        } else {
            vertexStride = sizeof(NkVertex3D);
            vertexData   = md.vertices3D;
        }

        const uint64 vboSize = (uint64)md.vertexCount * vertexStride;
        const uint64 iboSize = md.indexCount > 0
            ? (uint64)md.indexCount * (md.idxFmt == NkIndexFormat::UInt16 ? 2 : 4)
            : 0;

        // VBO
        nkentseu::NkBufferHandle vbo;
        if (md.usage == NkMeshUsage::Static && vertexData) {
            vbo = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Vertex(vboSize, vertexData));
        } else {
            vbo = mDevice->CreateBuffer(nkentseu::NkBufferDesc::VertexDynamic(vboSize > 0 ? vboSize : 64));
        }
        if (!vbo.IsValid()) return false;
        e.rhiVBO = vbo.id;

        // IBO
        if (iboSize > 0) {
            const void* idxData = (md.idxFmt == NkIndexFormat::UInt16)
                ? (const void*)md.indices16 : (const void*)md.indices32;

            nkentseu::NkBufferHandle ibo;
            if (md.usage == NkMeshUsage::Static && idxData) {
                ibo = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Index(iboSize, idxData));
            } else {
                ibo = mDevice->CreateBuffer(nkentseu::NkBufferDesc::IndexDynamic(iboSize));
            }
            if (!ibo.IsValid()) {
                mDevice->DestroyBuffer(vbo);
                return false;
            }
            e.rhiIBO = ibo.id;
        }

        e.vertexCount = md.vertexCount;
        e.indexCount  = md.indexCount;
        return true;
    }

    inline void NkOpenGLRenderer::DestroyMesh_Impl(NkMeshEntry& e) {
        nkentseu::NkBufferHandle vbo; vbo.id = e.rhiVBO;
        nkentseu::NkBufferHandle ibo; ibo.id = e.rhiIBO;
        if (vbo.IsValid()) mDevice->DestroyBuffer(vbo);
        if (ibo.IsValid()) mDevice->DestroyBuffer(ibo);
        e.rhiVBO = e.rhiIBO = 0;
    }

    inline bool NkOpenGLRenderer::UpdateMesh_Impl(NkMeshEntry& e,
                                                   const void* verts, uint32 vc,
                                                   const void* idxs,  uint32 ic)
    {
        nkentseu::NkBufferHandle vbo; vbo.id = e.rhiVBO;
        nkentseu::NkBufferHandle ibo; ibo.id = e.rhiIBO;

        uint32 stride = e.desc.Is2D() ? (uint32)sizeof(NkVertex2D)
                                      : (uint32)sizeof(NkVertex3D);
        bool ok = true;
        if (vbo.IsValid() && verts && vc > 0)
            ok &= mDevice->WriteBuffer(vbo, verts, (uint64)vc * stride);
        if (ibo.IsValid() && idxs && ic > 0)
            ok &= mDevice->WriteBuffer(ibo, idxs,
                (uint64)ic * (e.desc.idxFmt == NkIndexFormat::UInt16 ? 2 : 4));

        e.vertexCount = vc;
        e.indexCount  = ic;
        return ok;
    }

    inline bool NkOpenGLRenderer::CreateMaterial_Impl(NkMaterialEntry& e) {
        // Récupérer le shader RHI
        const NkShaderEntry* se = GetShader(e.shader);
        if (!se || !se->valid) return false;

        nkentseu::NkShaderHandle rhiShader; rhiShader.id = se->rhiHandle.id;

        // Descriptor set layout
        nkentseu::NkDescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Add(0, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,
                       nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        layoutDesc.Add(1, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,
                       nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        layoutDesc.Add(2, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,
                       nkentseu::NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(3, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                       nkentseu::NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(4, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                       nkentseu::NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(5, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                       nkentseu::NkShaderStage::NK_FRAGMENT);

        nkentseu::NkDescSetHandle layout = mDevice->CreateDescriptorSetLayout(layoutDesc);
        if (!layout.IsValid()) return false;

        // Pipeline
        nkentseu::NkGraphicsPipelineDesc pipeDesc{};
        pipeDesc.shader       = rhiShader;
        pipeDesc.vertexLayout = nkentseu::NkVertexLayout{};  // sera rempli à la création de l'instance
        pipeDesc.topology     = nkentseu::NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pipeDesc.rasterizer.cullMode = ToRhiCull(e.desc.cull);
        pipeDesc.rasterizer.fillMode = ToRhiFill(e.desc.fill);
        pipeDesc.depthStencil.depthTestEnable  = e.desc.depthTest;
        pipeDesc.depthStencil.depthWriteEnable = e.desc.depthWrite;
        pipeDesc.blend        = ToRhiBlend(e.desc.blend);
        pipeDesc.renderPass   = mDevice->GetSwapchainRenderPass();
        pipeDesc.debugName    = e.desc.debugName;
        pipeDesc.descriptorSetLayouts.PushBack(layout);

        // On stocke le layout dans rhiPipeline temporairement
        // (le pipeline complet est créé lors de la première instance avec un vtx layout)
        e.rhiPipeline = layout.id;
        return true;
    }

    inline void NkOpenGLRenderer::DestroyMaterial_Impl(NkMaterialEntry& e) {
        nkentseu::NkDescSetHandle layout; layout.id = e.rhiPipeline;
        if (layout.IsValid()) mDevice->DestroyDescriptorSetLayout(layout);
        e.rhiPipeline = 0;
    }

    inline bool NkOpenGLRenderer::CreateMatInst_Impl(NkMaterialInstEntry& e,
                                                      const NkMaterialEntry& tmpl)
    {
        nkentseu::NkDescSetHandle layout; layout.id = tmpl.rhiPipeline;
        if (!layout.IsValid()) return false;
        nkentseu::NkDescSetHandle set = mDevice->AllocateDescriptorSet(layout);
        if (!set.IsValid()) return false;
        e.rhiDescSet = set.id;
        e.dirty      = true;
        return true;
    }

    inline void NkOpenGLRenderer::DestroyMatInst_Impl(NkMaterialInstEntry& e) {
        nkentseu::NkDescSetHandle s; s.id = e.rhiDescSet;
        if (s.IsValid()) mDevice->FreeDescriptorSet(s);
        e.rhiDescSet = 0;
    }

    inline bool NkOpenGLRenderer::RebuildMatInst_Impl(NkMaterialInstEntry& e,
                                                       const NkMaterialEntry& tmpl) {
        (void)tmpl;
        e.dirty = false;
        // La mise à jour des descriptors se fait à la soumission
        return true;
    }

    inline bool NkOpenGLRenderer::CreateRenderTarget_Impl(NkRenderTargetEntry& e) {
        const NkRenderTargetDesc& d = e.desc;

        nkentseu::NkRenderPassDesc rpDesc = nkentseu::NkRenderPassDesc::Forward();
        e.rhiRenderPass = mDevice->CreateRenderPass(rpDesc).id;
        if (!e.rhiRenderPass) return false;

        nkentseu::NkFramebufferDesc fbDesc{};
        nkentseu::NkRenderPassHandle rp; rp.id = e.rhiRenderPass;
        fbDesc.renderPass = rp;
        fbDesc.width  = d.width  > 0 ? d.width  : mDevice->GetSwapchainWidth();
        fbDesc.height = d.height > 0 ? d.height : mDevice->GetSwapchainHeight();

        for (usize i = 0; i < d.colorAttachments.Size(); ++i) {
            nkentseu::NkTextureHandle t; t.id = d.colorAttachments[i].id;
            fbDesc.colorAttachments.PushBack(t);
            e.colorTextures.PushBack(d.colorAttachments[i]);
        }
        if (d.depthAttachment.IsValid()) {
            nkentseu::NkTextureHandle t; t.id = d.depthAttachment.id;
            fbDesc.depthAttachment = t;
            e.depthTexture = d.depthAttachment;
        }

        e.rhiFramebuffer = mDevice->CreateFramebuffer(fbDesc).id;
        return e.rhiFramebuffer != 0;
    }

    inline void NkOpenGLRenderer::DestroyRenderTarget_Impl(NkRenderTargetEntry& e) {
        nkentseu::NkFramebufferHandle fb; fb.id = e.rhiFramebuffer;
        nkentseu::NkRenderPassHandle  rp; rp.id = e.rhiRenderPass;
        if (fb.IsValid()) mDevice->DestroyFramebuffer(fb);
        if (rp.IsValid()) mDevice->DestroyRenderPass(rp);
        e.rhiFramebuffer = e.rhiRenderPass = 0;
    }

    inline bool NkOpenGLRenderer::CreateFont_Impl(NkFontEntry& /*e*/) {
        // La gestion de police passe par NKFont en dehors du RHI
        // L'atlas est une texture normale créée séparément via CreateTexture_Impl
        return true;
    }

    inline void NkOpenGLRenderer::DestroyFont_Impl(NkFontEntry& /*e*/) {}

    inline bool NkOpenGLRenderer::BeginFrame_Impl() {
        nkentseu::NkFrameContext fc;
        if (!mDevice->BeginFrame(fc)) return false;

        // Créer les UBO persistants si besoin
        if (!mFrameUBO.IsValid())
            mFrameUBO = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(FrameUBOData)));
        if (!mDrawUBO.IsValid())
            mDrawUBO  = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(DrawUBOData)));
        if (!mLightsUBO.IsValid())
            mLightsUBO= mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(LightsUBOData)));

        mFrameStarted = true;
        return true;
    }

    inline bool NkOpenGLRenderer::BeginRenderPass_Impl(const NkRenderPassInfo& info,
                                                        NkICommandBuffer* cmd)
    {
        nkentseu::NkRenderPassHandle  rp  = mDevice->GetSwapchainRenderPass();
        nkentseu::NkFramebufferHandle fbo = mDevice->GetSwapchainFramebuffer();

        if (info.target.IsValid()) {
            const NkRenderTargetEntry* rt = GetRenderTarget(info.target);
            if (rt && rt->valid) {
                nkentseu::NkRenderPassHandle  customRP;  customRP.id  = rt->rhiRenderPass;
                nkentseu::NkFramebufferHandle customFBO; customFBO.id = rt->rhiFramebuffer;
                rp  = customRP;
                fbo = customFBO;
            }
        }

        const uint32 W = mDevice->GetSwapchainWidth();
        const uint32 H = mDevice->GetSwapchainHeight();
        nkentseu::NkRect2D area{0, 0, (int32)W, (int32)H};

        if (info.clearColor) {
            cmd->SetClearColor(info.clearColorValue.x, info.clearColorValue.y,
                               info.clearColorValue.z, info.clearColorValue.w);
        }
        if (info.clearDepth) {
            cmd->SetClearDepth(info.clearDepthValue);
        }

        return cmd->BeginRenderPass(rp, fbo, area);
    }

    inline void NkOpenGLRenderer::SubmitDrawCalls_Impl(NkRendererCommand& drawCmd,
                                                        NkICommandBuffer* rhiCmd,
                                                        const NkRenderPassInfo& pass)
    {
        const uint32 W = mDevice->GetSwapchainWidth();
        const uint32 H = mDevice->GetSwapchainHeight();

        nkentseu::NkViewport vp{0.f, 0.f, (float32)W, (float32)H, 0.f, 1.f};
        rhiCmd->SetViewport(vp);
        nkentseu::NkRect2D scissor{0, 0, (int32)W, (int32)H};
        rhiCmd->SetScissor(scissor);

        // Mettre à jour le FrameUBO depuis la caméra
        if (pass.camera.IsValid()) {
            FrameUBOData fubo{};
            const NkMat4f view = GetViewMatrix(pass.camera);
            const NkMat4f proj = GetProjectionMatrix(pass.camera);
            const NkMat4f vp_  = GetViewProjectionMatrix(pass.camera);
            memcpy(fubo.view,     view.data,  64);
            memcpy(fubo.proj,     proj.data,  64);
            memcpy(fubo.viewProj, vp_.data,   64);
            mDevice->WriteBuffer(mFrameUBO, &fubo, sizeof(fubo));
        }

        // Mettre à jour LightsUBO
        {
            LightsUBOData lubo{};
            const auto& lights = drawCmd.GetLights();
            for (usize i = 0; i < lights.Size(); ++i) {
                const NkLightDesc& l = lights[i];
                if (l.type == NkLightType::Directional) {
                    lubo.dirLightDir[0] = l.direction.x;
                    lubo.dirLightDir[1] = l.direction.y;
                    lubo.dirLightDir[2] = l.direction.z;
                    lubo.dirLightDir[3] = 0.f;
                    lubo.dirLightColor[0] = l.color.x;
                    lubo.dirLightColor[1] = l.color.y;
                    lubo.dirLightColor[2] = l.color.z;
                    lubo.dirLightColor[3] = l.intensity;
                } else if (l.type == NkLightType::Point && lubo.numPointLights < 8) {
                    int32 j = lubo.numPointLights++;
                    lubo.pointPos[j][0]   = l.position.x;
                    lubo.pointPos[j][1]   = l.position.y;
                    lubo.pointPos[j][2]   = l.position.z;
                    lubo.pointPos[j][3]   = l.radius;
                    lubo.pointColor[j][0] = l.color.x;
                    lubo.pointColor[j][1] = l.color.y;
                    lubo.pointColor[j][2] = l.color.z;
                    lubo.pointColor[j][3] = l.intensity;
                }
            }
            mDevice->WriteBuffer(mLightsUBO, &lubo, sizeof(lubo));
        }

        // Draw calls 3D
        Submit3D(drawCmd, rhiCmd, pass);

        // Draw calls 2D
        Submit2D(drawCmd, rhiCmd);

        // Debug
        SubmitDebug(drawCmd, rhiCmd);

        // Mise à jour stats
        mStats.drawCalls3D = (uint32)drawCmd.GetDrawCalls3D().Size();
        mStats.drawCalls2D = (uint32)(drawCmd.GetDrawCalls2D().Size()
                                    + drawCmd.GetSpriteCalls().Size());
    }

    inline void NkOpenGLRenderer::Submit3D(NkRendererCommand& drawCmd,
                                            NkICommandBuffer* rhiCmd,
                                            const NkRenderPassInfo& /*pass*/)
    {
        const auto& calls = drawCmd.GetDrawCalls3D();
        uint64 lastPipeId = 0;

        for (usize i = 0; i < calls.Size(); ++i) {
            const NkDrawCall3D& dc = calls[i];
            if (!dc.mesh.IsValid() || !dc.material.IsValid()) continue;

            const NkMeshEntry*         mesh = GetMesh(dc.mesh);
            const NkMaterialInstEntry* inst = GetMaterialInst(dc.material);
            if (!mesh || !inst || !mesh->valid || !inst->valid) continue;

            const NkMaterialEntry* tmpl = GetMaterial(inst->templateHandle);
            if (!tmpl || !tmpl->valid) continue;

            // Binder le pipeline si changement
            nkentseu::NkPipelineHandle pipe; pipe.id = tmpl->rhiPipeline;
            if (pipe.id != lastPipeId && pipe.IsValid()) {
                rhiCmd->BindGraphicsPipeline(pipe);
                lastPipeId = pipe.id;
                ++mStats.shaderChanges;
            }

            // Écrire le DrawUBO (model matrix)
            DrawUBOData dubo{};
            memcpy(dubo.model, dc.transform.data, 64);
            // modelIT = transposée inverse (approx : model si pas de non-uniform scale)
            memcpy(dubo.modelIT, dc.transform.data, 64);
            dubo.objectColor[0] = dubo.objectColor[1] = dubo.objectColor[2] = dubo.objectColor[3] = 1.f;
            mDevice->WriteBuffer(mDrawUBO, &dubo, sizeof(dubo));

            // Binder le descriptor set de l'instance
            nkentseu::NkDescSetHandle ds; ds.id = inst->rhiDescSet;
            if (ds.IsValid()) rhiCmd->BindDescriptorSet(ds, 0);

            // Binder le VBO/IBO
            nkentseu::NkBufferHandle vbo; vbo.id = mesh->rhiVBO;
            rhiCmd->BindVertexBuffer(0, vbo);

            if (mesh->indexCount > 0) {
                nkentseu::NkBufferHandle ibo; ibo.id = mesh->rhiIBO;
                nkentseu::NkIndexFormat fmt = (mesh->desc.idxFmt == NkIndexFormat::UInt16)
                    ? nkentseu::NkIndexFormat::NK_UINT16
                    : nkentseu::NkIndexFormat::NK_UINT32;
                rhiCmd->BindIndexBuffer(ibo, fmt);
                rhiCmd->DrawIndexed(mesh->indexCount);
            } else {
                rhiCmd->Draw(mesh->vertexCount);
            }

            mStats.drawCalls3D++;
            mStats.vertices   += mesh->vertexCount;
            mStats.triangles  += mesh->indexCount > 0 ? mesh->indexCount / 3 : mesh->vertexCount / 3;
        }
    }

    inline void NkOpenGLRenderer::Submit2D(NkRendererCommand& drawCmd,
                                            NkICommandBuffer* rhiCmd)
    {
        (void)rhiCmd;
        // Les draw calls 2D sont soumis via NkDrawCall2D.mesh/.material
        const auto& calls = drawCmd.GetDrawCalls2D();
        for (usize i = 0; i < calls.Size(); ++i) {
            const NkDrawCall2D& dc = calls[i];
            if (!dc.mesh.IsValid() || !dc.material.IsValid()) continue;

            const NkMeshEntry* mesh = GetMesh(dc.mesh);
            if (!mesh || !mesh->valid) continue;

            const NkMaterialInstEntry* inst = GetMaterialInst(dc.material);
            if (!inst || !inst->valid) continue;

            const NkMaterialEntry* tmpl = GetMaterial(inst->templateHandle);
            if (!tmpl || !tmpl->valid) continue;

            nkentseu::NkPipelineHandle pipe; pipe.id = tmpl->rhiPipeline;
            if (pipe.IsValid()) rhiCmd->BindGraphicsPipeline(pipe);

            nkentseu::NkDescSetHandle ds; ds.id = inst->rhiDescSet;
            if (ds.IsValid()) rhiCmd->BindDescriptorSet(ds, 0);

            nkentseu::NkBufferHandle vbo; vbo.id = mesh->rhiVBO;
            rhiCmd->BindVertexBuffer(0, vbo);

            if (mesh->indexCount > 0) {
                nkentseu::NkBufferHandle ibo; ibo.id = mesh->rhiIBO;
                rhiCmd->BindIndexBuffer(ibo, nkentseu::NkIndexFormat::NK_UINT32);
                rhiCmd->DrawIndexed(mesh->indexCount);
            } else {
                rhiCmd->Draw(mesh->vertexCount);
            }
        }
    }

    inline void NkOpenGLRenderer::SubmitDebug(NkRendererCommand& drawCmd,
                                               NkICommandBuffer* /*rhiCmd*/)
    {
        // Les debug lines/AABBs/spheres seraient converties en vertex buffer dynamique
        // et soumises avec le shader debug. Squelette pour l'instant.
        (void)drawCmd;
    }

    inline void NkOpenGLRenderer::EndRenderPass_Impl(NkICommandBuffer* cmd) {
        cmd->EndRenderPass();
    }

    inline void NkOpenGLRenderer::EndFrame_Impl(NkICommandBuffer* /*cmd*/) {
        nkentseu::NkFrameContext fc;
        fc.frameIndex  = mDevice->GetFrameIndex();
        fc.frameNumber = mDevice->GetFrameNumber();
        mDevice->EndFrame(fc);
        mFrameStarted  = false;
    }

} // namespace renderer
} // namespace nkentseu