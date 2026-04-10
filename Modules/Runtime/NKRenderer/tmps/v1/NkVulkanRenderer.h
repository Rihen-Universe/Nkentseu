#pragma once
// =============================================================================
// NkVulkanRenderer.h
// Backend Vulkan pour NKRenderer.
//
// Spécificités Vulkan par rapport à OpenGL :
//   • Descriptor sets organisés par fréquence de mise à jour :
//       set=0 → par frame  (FrameUBO, LightsUBO)
//       set=1 → par passe  (MaterialUBO, textures PBR)
//       set=2 → par draw   (DrawUBO)
//   • Y-flip : déjà intégré dans les shaders Vulkan (clip.y = -clip.y)
//   • Depth NDC : [0,1] — les matrices de projection utilisent depthZeroToOne=true
//   • Pipeline cache : sauvegardé sur disque entre les sessions
//   • Les shaders GLSL-VK sont compilés en SPIRV via NkShaderConverter
// =============================================================================
#include "NKRenderer/Core/NkBaseRenderer.h"
#include "NKRHI/ShaderConvert/NkShaderConvert.h"

#ifdef NK_RHI_VK_ENABLED

namespace nkentseu {
namespace renderer {

    class NkVulkanRenderer : public NkBaseRenderer {
    public:
        explicit NkVulkanRenderer(NkIDevice* device)
            : NkBaseRenderer(device)
        {
            // Charger le cache de shaders compilés
            NkShaderCache::Global().SetCacheDir("Build/ShaderCache/Vulkan");
        }

        ~NkVulkanRenderer() override = default;

    protected:
        // ── NkBaseRenderer impl ───────────────────────────────────────────────
        bool CreateShader_Impl   (NkShaderEntry& e)                             override;
        void DestroyShader_Impl  (NkShaderEntry& e)                             override;

        bool CreateTexture_Impl  (NkTextureEntry& e)                            override;
        void DestroyTexture_Impl (NkTextureEntry& e)                            override;
        bool UpdateTexture_Impl  (NkTextureEntry& e, const void* d,
                                   uint32 rp, uint32 mip, uint32 layer)        override;

        bool CreateMesh_Impl     (NkMeshEntry& e)                               override;
        void DestroyMesh_Impl    (NkMeshEntry& e)                               override;
        bool UpdateMesh_Impl     (NkMeshEntry& e, const void* v, uint32 vc,
                                   const void* i, uint32 ic)                   override;

        bool CreateMaterial_Impl (NkMaterialEntry& e)                           override;
        void DestroyMaterial_Impl(NkMaterialEntry& e)                           override;

        bool CreateMatInst_Impl  (NkMaterialInstEntry& e,
                                   const NkMaterialEntry& tmpl)                override;
        void DestroyMatInst_Impl (NkMaterialInstEntry& e)                       override;
        bool RebuildMatInst_Impl (NkMaterialInstEntry& e,
                                   const NkMaterialEntry& tmpl)                override;

        bool CreateRenderTarget_Impl (NkRenderTargetEntry& e)                   override;
        void DestroyRenderTarget_Impl(NkRenderTargetEntry& e)                   override;

        bool CreateFont_Impl     (NkFontEntry& e)                               override;
        void DestroyFont_Impl    (NkFontEntry& e)                               override;

        bool BeginFrame_Impl     ()                                             override;
        bool BeginRenderPass_Impl(const NkRenderPassInfo& info,
                                   NkICommandBuffer* cmd)                      override;
        void SubmitDrawCalls_Impl(NkRendererCommand& drawCmd,
                                   NkICommandBuffer* rhiCmd,
                                   const NkRenderPassInfo& pass)               override;
        void EndRenderPass_Impl  (NkICommandBuffer* cmd)                        override;
        void EndFrame_Impl       (NkICommandBuffer* cmd)                        override;

    private:
        // ── Compilation GLSL → SPIRV (avec cache .nksc) ───────────────────────
        bool CompileGLSLToSPIRV(const NkShaderStageDesc& stage,
                                 nkentseu::NkShaderStageDesc& outRhi);

        // ── Helpers conversions (identiques à OpenGL mais distincts) ──────────
        static nkentseu::NkGPUFormat    ToRhiFormat (NkTextureFormat f);
        static nkentseu::NkFilter       ToRhiFilter (NkSamplerFilter f);
        static nkentseu::NkMipFilter    ToRhiMipFilter(NkSamplerFilter f);
        static nkentseu::NkAddressMode  ToRhiWrap   (NkSamplerWrap w);
        static nkentseu::NkCullMode     ToRhiCull   (NkCullMode c);
        static nkentseu::NkFillMode     ToRhiFill   (NkFillMode f);
        static nkentseu::NkBlendDesc    ToRhiBlend  (NkBlendMode b);

        // ── Descriptor set layouts (set 0, 1, 2) ─────────────────────────────
        // set=0 : FrameUBO (b=0) + LightsUBO (b=1)
        // set=1 : MaterialUBO (b=0) + textures (b=1..5)
        // set=2 : DrawUBO (b=0)
        nkentseu::NkDescSetHandle mSet0Layout;  // frame
        nkentseu::NkDescSetHandle mSet1Layout;  // material
        nkentseu::NkDescSetHandle mSet2Layout;  // draw

        // Descriptor sets persistants par frame
        nkentseu::NkDescSetHandle mFrameDescSet;
        nkentseu::NkDescSetHandle mDrawDescSet;

        // UBOs persistants
        nkentseu::NkBufferHandle  mFrameUBO;
        nkentseu::NkBufferHandle  mDrawUBO;
        nkentseu::NkBufferHandle  mLightsUBO;

        // White 1×1 texture (fallback pour les slots de texture non liés)
        nkentseu::NkTextureHandle mWhiteTex;
        nkentseu::NkSamplerHandle mDefaultSampler;

        bool mLayoutsCreated = false;

        // ── Données UBO (mêmes structs que OpenGL) ────────────────────────────
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
            float dirLightColor[4];
            float pointPos[8][4];
            float pointColor[8][4];
            int32 numPointLights;
            int32 _pad[3];
        };

        bool EnsureLayouts();
        bool EnsureUBOs();
        void UpdateFrameDescSet();
        void BindMaterialInstance(NkICommandBuffer* cmd,
                                   const NkMaterialInstEntry& inst,
                                   const NkMaterialEntry& tmpl);
    };

    // =========================================================================
    // Conversion de types (Vulkan)
    // =========================================================================
    inline nkentseu::NkGPUFormat NkVulkanRenderer::ToRhiFormat(NkTextureFormat f) {
        switch (f) {
            case NkTextureFormat::RGBA8_SRGB:  return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
            case NkTextureFormat::RGBA8_UNORM: return nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
            case NkTextureFormat::RGBA16F:     return nkentseu::NkGPUFormat::NK_RGBA16_FLOAT;
            case NkTextureFormat::RGBA32F:     return nkentseu::NkGPUFormat::NK_RGBA32_FLOAT;
            case NkTextureFormat::RG16F:       return nkentseu::NkGPUFormat::NK_RG16_FLOAT;
            case NkTextureFormat::R32F:        return nkentseu::NkGPUFormat::NK_R32_FLOAT;
            case NkTextureFormat::D32F:        return nkentseu::NkGPUFormat::NK_D32_FLOAT;
            case NkTextureFormat::D24S8:       return nkentseu::NkGPUFormat::NK_D24_UNORM_S8_UINT;
            case NkTextureFormat::BC1_SRGB:    return nkentseu::NkGPUFormat::NK_BC1_RGB_SRGB;
            case NkTextureFormat::BC3_SRGB:    return nkentseu::NkGPUFormat::NK_BC3_SRGB;
            case NkTextureFormat::BC5_UNORM:   return nkentseu::NkGPUFormat::NK_BC5_UNORM;
            case NkTextureFormat::BC7_SRGB:    return nkentseu::NkGPUFormat::NK_BC7_SRGB;
            default:                           return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
        }
    }
    inline nkentseu::NkFilter      NkVulkanRenderer::ToRhiFilter(NkSamplerFilter f)     { return f==NkSamplerFilter::Nearest ? nkentseu::NkFilter::NK_NEAREST : nkentseu::NkFilter::NK_LINEAR; }
    inline nkentseu::NkMipFilter   NkVulkanRenderer::ToRhiMipFilter(NkSamplerFilter f)  { return f==NkSamplerFilter::Nearest ? nkentseu::NkMipFilter::NK_NEAREST : nkentseu::NkMipFilter::NK_LINEAR; }
    inline nkentseu::NkAddressMode NkVulkanRenderer::ToRhiWrap(NkSamplerWrap w) {
        switch(w){
            case NkSamplerWrap::ClampToEdge:    return nkentseu::NkAddressMode::NK_CLAMP_TO_EDGE;
            case NkSamplerWrap::ClampToBorder:  return nkentseu::NkAddressMode::NK_CLAMP_TO_BORDER;
            case NkSamplerWrap::MirroredRepeat: return nkentseu::NkAddressMode::NK_MIRRORED_REPEAT;
            default:                            return nkentseu::NkAddressMode::NK_REPEAT;
        }
    }
    inline nkentseu::NkCullMode  NkVulkanRenderer::ToRhiCull(NkCullMode c)  { switch(c){case NkCullMode::Front:return nkentseu::NkCullMode::NK_FRONT;case NkCullMode::None:return nkentseu::NkCullMode::NK_NONE;default:return nkentseu::NkCullMode::NK_BACK;} }
    inline nkentseu::NkFillMode  NkVulkanRenderer::ToRhiFill(NkFillMode f)  { return f==NkFillMode::Wireframe?nkentseu::NkFillMode::NK_WIREFRAME:nkentseu::NkFillMode::NK_SOLID; }
    inline nkentseu::NkBlendDesc NkVulkanRenderer::ToRhiBlend(NkBlendMode b){
        switch(b){
            case NkBlendMode::AlphaBlend:   return nkentseu::NkBlendDesc::Alpha();
            case NkBlendMode::Additive:     return nkentseu::NkBlendDesc::Additive();
            case NkBlendMode::PreMultAlpha:{ nkentseu::NkBlendDesc d; d.attachments[0]=nkentseu::NkBlendAttachment::PreMultAlpha(); return d; }
            default:                        return nkentseu::NkBlendDesc::Opaque();
        }
    }

    // =========================================================================
    // Implementations
    // =========================================================================
    inline bool NkVulkanRenderer::EnsureLayouts() {
        if (mLayoutsCreated) return true;

        // set=0 : FrameUBO + LightsUBO
        {
            nkentseu::NkDescriptorSetLayoutDesc d;
            d.Add(0, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER, nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            d.Add(1, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER, nkentseu::NkShaderStage::NK_FRAGMENT);
            mSet0Layout = mDevice->CreateDescriptorSetLayout(d);
        }
        // set=1 : MaterialUBO + 5 textures
        {
            nkentseu::NkDescriptorSetLayoutDesc d;
            d.Add(0, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,          nkentseu::NkShaderStage::NK_FRAGMENT);
            d.Add(1, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  nkentseu::NkShaderStage::NK_FRAGMENT);
            d.Add(2, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  nkentseu::NkShaderStage::NK_FRAGMENT);
            d.Add(3, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  nkentseu::NkShaderStage::NK_FRAGMENT);
            d.Add(4, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  nkentseu::NkShaderStage::NK_FRAGMENT);
            d.Add(5, nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,  nkentseu::NkShaderStage::NK_FRAGMENT);
            mSet1Layout = mDevice->CreateDescriptorSetLayout(d);
        }
        // set=2 : DrawUBO
        {
            nkentseu::NkDescriptorSetLayoutDesc d;
            d.Add(0, nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER, nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
            mSet2Layout = mDevice->CreateDescriptorSetLayout(d);
        }

        mLayoutsCreated = mSet0Layout.IsValid() && mSet1Layout.IsValid() && mSet2Layout.IsValid();
        return mLayoutsCreated;
    }

    inline bool NkVulkanRenderer::EnsureUBOs() {
        if (!mFrameUBO.IsValid())
            mFrameUBO  = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(FrameUBOData)));
        if (!mDrawUBO.IsValid())
            mDrawUBO   = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(DrawUBOData)));
        if (!mLightsUBO.IsValid())
            mLightsUBO = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(LightsUBOData)));
        if (!mWhiteTex.IsValid()) {
            nkentseu::NkTextureDesc td{};
            td.type=nkentseu::NkTextureType::NK_TEX2D;
            td.format=nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
            td.width=td.height=1; td.mipLevels=1;
            td.bindFlags=nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
            mWhiteTex = mDevice->CreateTexture(td);
            static const uint8 white[4]={255,255,255,255};
            mDevice->WriteTexture(mWhiteTex, white, 4);
        }
        if (!mDefaultSampler.IsValid())
            mDefaultSampler = mDevice->CreateSampler(nkentseu::NkSamplerDesc::Linear());

        // Allouer le descriptor set frame (set=0)
        if (!mFrameDescSet.IsValid() && mSet0Layout.IsValid()) {
            mFrameDescSet = mDevice->AllocateDescriptorSet(mSet0Layout);
            if (mFrameDescSet.IsValid()) {
                nkentseu::NkDescriptorWrite w[2]{};
                w[0].set=mFrameDescSet; w[0].binding=0;
                w[0].type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER;
                w[0].buffer=mFrameUBO; w[0].bufferRange=sizeof(FrameUBOData);
                w[1].set=mFrameDescSet; w[1].binding=1;
                w[1].type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER;
                w[1].buffer=mLightsUBO; w[1].bufferRange=sizeof(LightsUBOData);
                mDevice->UpdateDescriptorSets(w, 2);
            }
        }
        // Allouer le descriptor set draw (set=2)
        if (!mDrawDescSet.IsValid() && mSet2Layout.IsValid()) {
            mDrawDescSet = mDevice->AllocateDescriptorSet(mSet2Layout);
            if (mDrawDescSet.IsValid()) {
                nkentseu::NkDescriptorWrite w{};
                w.set=mDrawDescSet; w.binding=0;
                w.type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER;
                w.buffer=mDrawUBO; w.bufferRange=sizeof(DrawUBOData);
                mDevice->UpdateDescriptorSets(&w, 1);
            }
        }
        return mFrameUBO.IsValid() && mDrawUBO.IsValid() && mLightsUBO.IsValid();
    }

    inline bool NkVulkanRenderer::CompileGLSLToSPIRV(const NkShaderStageDesc& stage,
                                                       nkentseu::NkShaderStageDesc& outRhi)
    {
        NkSLStage slStage{};
        switch (stage.stage) {
            case NkShaderStageType::Vertex:   slStage = NkSLStage::NK_VERTEX;   break;
            case NkShaderStageType::Fragment: slStage = NkSLStage::NK_FRAGMENT; break;
            case NkShaderStageType::Compute:  slStage = NkSLStage::NK_COMPUTE;  break;
            default: slStage = NkSLStage::NK_VERTEX;
        }

        const NkString src(stage.source.CStr());
        const uint64 key = NkShaderCache::ComputeKey(src, slStage, "spirv");

        // Tenter le cache disque d'abord
        NkShaderConvertResult res = NkShaderCache::Global().Load(key);
        if (!res.success || res.SpirvWordCount() == 0) {
            if (!NkShaderConverter::CanGlslToSpirv()) {
                // glslang non disponible — on passe le GLSL tel quel (fallback)
                outRhi.glslSource  = stage.source.CStr();
                outRhi.entryPoint  = stage.entryPoint.CStr();
                return true;
            }
            res = NkShaderConverter::GlslToSpirv(src, slStage, stage.debugName.CStr());
            if (res.success) NkShaderCache::Global().Save(key, res);
        }

        if (!res.success) {
            logger_src.Infof("[NKRenderer/Vulkan] SPIRV compile FAILED: %s\n%s\n",
                              stage.debugName.CStr(), res.errors.CStr());
            return false;
        }

        outRhi.spirvBinary.Resize(res.SpirvWordCount() * 4);
        memcpy(outRhi.spirvBinary.Data(),
               res.SpirvWords(),
               res.SpirvWordCount() * 4);
        outRhi.entryPoint = stage.entryPoint.CStr();
        return true;
    }

    inline bool NkVulkanRenderer::CreateShader_Impl(NkShaderEntry& e) {
        nkentseu::NkShaderDesc rhiDesc;
        rhiDesc.debugName = e.desc.debugName;

        for (usize i = 0; i < e.desc.stages.Size(); ++i) {
            const NkShaderStageDesc& stage = e.desc.stages[i];
            if (stage.backend != NkShaderBackend::Vulkan) continue;

            nkentseu::NkShaderStageDesc rhi{};
            switch (stage.stage) {
                case NkShaderStageType::Vertex:   rhi.stage=nkentseu::NkShaderStage::NK_VERTEX;   break;
                case NkShaderStageType::Fragment: rhi.stage=nkentseu::NkShaderStage::NK_FRAGMENT; break;
                case NkShaderStageType::Compute:  rhi.stage=nkentseu::NkShaderStage::NK_COMPUTE;  break;
                default: rhi.stage=nkentseu::NkShaderStage::NK_VERTEX;
            }

            if (!CompileGLSLToSPIRV(stage, rhi)) return false;
            rhiDesc.stages.PushBack(rhi);
        }

        if (rhiDesc.stages.IsEmpty()) return false;
        nkentseu::NkShaderHandle h = mDevice->CreateShader(rhiDesc);
        if (!h.IsValid()) return false;
        e.rhiHandle.id = h.id;
        return true;
    }

    inline void NkVulkanRenderer::DestroyShader_Impl(NkShaderEntry& e) {
        nkentseu::NkShaderHandle h; h.id=e.rhiHandle.id;
        if (h.IsValid()) mDevice->DestroyShader(h);
        e.rhiHandle.id = 0;
    }

    inline bool NkVulkanRenderer::CreateTexture_Impl(NkTextureEntry& e) {
        const NkTextureDesc& td = e.desc;
        nkentseu::NkTextureDesc rhi{};
        rhi.type        = (td.type==NkTextureType::Cube)?nkentseu::NkTextureType::NK_CUBE:nkentseu::NkTextureType::NK_TEX2D;
        rhi.format      = ToRhiFormat(td.format);
        rhi.width       = td.width; rhi.height=td.height; rhi.depth=td.depth;
        rhi.arrayLayers = td.layers; rhi.mipLevels=td.mipLevels;
        rhi.samples     = nkentseu::NkSampleCount::NK_S1;
        rhi.debugName   = td.debugName;
        rhi.initialData = td.initialData; rhi.rowPitch=td.rowPitch;
        rhi.bindFlags = td.isRenderTarget
            ? (nkentseu::NkBindFlags::NK_RENDER_TARGET|nkentseu::NkBindFlags::NK_SHADER_RESOURCE)
            : td.isDepthTarget
                ? (nkentseu::NkBindFlags::NK_DEPTH_STENCIL|nkentseu::NkBindFlags::NK_SHADER_RESOURCE)
                : nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
        nkentseu::NkTextureHandle tex = mDevice->CreateTexture(rhi);
        if (!tex.IsValid()) return false;
        e.rhiTextureId = tex.id;

        nkentseu::NkSamplerDesc sd{};
        sd.magFilter=ToRhiFilter(td.sampler.filter);
        sd.minFilter=ToRhiFilter(td.sampler.filter);
        sd.mipFilter=ToRhiMipFilter(td.sampler.filter);
        sd.addressU=ToRhiWrap(td.sampler.wrapU);
        sd.addressV=ToRhiWrap(td.sampler.wrapV);
        sd.addressW=ToRhiWrap(td.sampler.wrapW);
        sd.maxAnisotropy=td.sampler.anisotropy;
        nkentseu::NkSamplerHandle samp = mDevice->CreateSampler(sd);
        if (!samp.IsValid()) { mDevice->DestroyTexture(tex); return false; }
        e.rhiSamplerId = samp.id;
        return true;
    }

    inline void NkVulkanRenderer::DestroyTexture_Impl(NkTextureEntry& e) {
        nkentseu::NkTextureHandle t; t.id=e.rhiTextureId;
        nkentseu::NkSamplerHandle s; s.id=e.rhiSamplerId;
        if (t.IsValid()) mDevice->DestroyTexture(t);
        if (s.IsValid()) mDevice->DestroySampler(s);
        e.rhiTextureId=e.rhiSamplerId=0;
    }

    inline bool NkVulkanRenderer::UpdateTexture_Impl(NkTextureEntry& e, const void* d,
                                                      uint32 rp, uint32 mip, uint32 layer) {
        nkentseu::NkTextureHandle t; t.id=e.rhiTextureId;
        if (!t.IsValid()) return false;
        if (mip==0&&layer==0) return mDevice->WriteTexture(t,d,rp);
        return mDevice->WriteTextureRegion(t,d,0,0,0,e.desc.width,e.desc.height,1,mip,layer,rp);
    }

    inline bool NkVulkanRenderer::CreateMesh_Impl(NkMeshEntry& e) {
        const NkMeshDesc& md=e.desc;
        uint32 stride = md.Is2D() ? sizeof(NkVertex2D) : sizeof(NkVertex3D);
        const void* vdata = md.Is2D() ? (const void*)md.vertices2D : (const void*)md.vertices3D;
        const uint64 vsz = (uint64)md.vertexCount * stride;
        const uint64 isz = md.indexCount * (md.idxFmt==NkIndexFormat::UInt16?2:4);
        nkentseu::NkBufferHandle vbo = (md.usage==NkMeshUsage::Static && vdata)
            ? mDevice->CreateBuffer(nkentseu::NkBufferDesc::Vertex(vsz,vdata))
            : mDevice->CreateBuffer(nkentseu::NkBufferDesc::VertexDynamic(vsz>0?vsz:64));
        if (!vbo.IsValid()) return false;
        e.rhiVBO = vbo.id;
        if (isz>0) {
            const void* idata = md.idxFmt==NkIndexFormat::UInt16?(const void*)md.indices16:(const void*)md.indices32;
            nkentseu::NkBufferHandle ibo = (md.usage==NkMeshUsage::Static && idata)
                ? mDevice->CreateBuffer(nkentseu::NkBufferDesc::Index(isz,idata))
                : mDevice->CreateBuffer(nkentseu::NkBufferDesc::IndexDynamic(isz));
            if (!ibo.IsValid()) { mDevice->DestroyBuffer(vbo); return false; }
            e.rhiIBO = ibo.id;
        }
        return true;
    }

    inline void NkVulkanRenderer::DestroyMesh_Impl(NkMeshEntry& e) {
        nkentseu::NkBufferHandle vbo; vbo.id=e.rhiVBO;
        nkentseu::NkBufferHandle ibo; ibo.id=e.rhiIBO;
        if (vbo.IsValid()) mDevice->DestroyBuffer(vbo);
        if (ibo.IsValid()) mDevice->DestroyBuffer(ibo);
        e.rhiVBO=e.rhiIBO=0;
    }

    inline bool NkVulkanRenderer::UpdateMesh_Impl(NkMeshEntry& e,
                                                   const void* v, uint32 vc,
                                                   const void* i, uint32 ic) {
        nkentseu::NkBufferHandle vbo; vbo.id=e.rhiVBO;
        nkentseu::NkBufferHandle ibo; ibo.id=e.rhiIBO;
        uint32 stride = e.desc.Is2D()?sizeof(NkVertex2D):sizeof(NkVertex3D);
        bool ok=true;
        if (vbo.IsValid()&&v&&vc) ok&=mDevice->WriteBuffer(vbo,v,(uint64)vc*stride);
        if (ibo.IsValid()&&i&&ic) ok&=mDevice->WriteBuffer(ibo,i,(uint64)ic*(e.desc.idxFmt==NkIndexFormat::UInt16?2:4));
        e.vertexCount=vc; e.indexCount=ic;
        return ok;
    }

    inline bool NkVulkanRenderer::CreateMaterial_Impl(NkMaterialEntry& e) {
        EnsureLayouts();
        // Stocker le layout set=1 dans rhiPipeline pour l'allocation des instances
        e.rhiPipeline = mSet1Layout.id;
        return mSet1Layout.IsValid();
    }

    inline void NkVulkanRenderer::DestroyMaterial_Impl(NkMaterialEntry& e) {
        e.rhiPipeline = 0;  // Le layout partagé est détruit dans Shutdown
    }

    inline bool NkVulkanRenderer::CreateMatInst_Impl(NkMaterialInstEntry& e,
                                                      const NkMaterialEntry& /*tmpl*/) {
        EnsureLayouts(); EnsureUBOs();
        nkentseu::NkDescSetHandle layout; layout.id=mSet1Layout.id;
        if (!layout.IsValid()) return false;
        nkentseu::NkDescSetHandle set = mDevice->AllocateDescriptorSet(layout);
        if (!set.IsValid()) return false;
        e.rhiDescSet=set.id;
        e.dirty=true;
        // Lier la white texture par défaut sur tous les slots
        nkentseu::NkDescriptorWrite writes[5]{};
        for (int k=0;k<5;++k) {
            writes[k].set=set; writes[k].binding=k+1;
            writes[k].type=nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[k].texture=mWhiteTex; writes[k].sampler=mDefaultSampler;
            writes[k].textureLayout=nkentseu::NkResourceState::NK_SHADER_READ;
        }
        mDevice->UpdateDescriptorSets(writes,5);
        return true;
    }

    inline void NkVulkanRenderer::DestroyMatInst_Impl(NkMaterialInstEntry& e) {
        nkentseu::NkDescSetHandle s; s.id=e.rhiDescSet;
        if (s.IsValid()) mDevice->FreeDescriptorSet(s);
        e.rhiDescSet=0;
    }

    inline bool NkVulkanRenderer::RebuildMatInst_Impl(NkMaterialInstEntry& e,
                                                       const NkMaterialEntry& tmpl) {
        // Mettre à jour les textures du descriptor set set=1
        if (!e.rhiDescSet) return false;
        nkentseu::NkDescSetHandle set; set.id=e.rhiDescSet;

        // Construire un buffer MaterialUBO depuis les params
        // (structure correspondant exactement au shader)
        struct MaterialUBOData {
            float baseColorFactor[4] = {1,1,1,1};
            float metallicFactor     = 0.f;
            float roughnessFactor    = 0.5f;
            float occlusionStrength  = 1.f;
            float emissiveStrength   = 0.f;
            float emissiveFactor[3]  = {0,0,0};
            float _pad0              = 0.f;
            int32 useAlbedoMap       = 0;
            int32 useNormalMap       = 0;
            int32 useMetalRoughMap   = 0;
            int32 useOcclusionMap    = 0;
            int32 useEmissiveMap     = 0;
            float alphaCutoff        = 0.f;
            int32 doubleSided        = 0;
            float _pad1              = 0.f;
        } uboData;

        // Textures à lier (slots 1..5 = albedo, normal, metalRough, occlusion, emissive)
        nkentseu::NkTextureHandle texSlots[5];
        nkentseu::NkSamplerHandle sampSlots[5];
        for (int k=0;k<5;++k) { texSlots[k]=mWhiteTex; sampSlots[k]=mDefaultSampler; }

        for (usize pi=0; pi<e.params.Size(); ++pi) {
            const NkMaterialParam& p = e.params[pi];
            const char* n = p.name.CStr();
            if (p.type==NkMaterialParam::Type::Vec4 && strcmp(n,"baseColorFactor")==0) {
                uboData.baseColorFactor[0]=p.v4[0]; uboData.baseColorFactor[1]=p.v4[1];
                uboData.baseColorFactor[2]=p.v4[2]; uboData.baseColorFactor[3]=p.v4[3];
            }
            else if (p.type==NkMaterialParam::Type::Float && strcmp(n,"metallicFactor")==0)   uboData.metallicFactor=p.f;
            else if (p.type==NkMaterialParam::Type::Float && strcmp(n,"roughnessFactor")==0)  uboData.roughnessFactor=p.f;
            else if (p.type==NkMaterialParam::Type::Float && strcmp(n,"occlusionStrength")==0)uboData.occlusionStrength=p.f;
            else if (p.type==NkMaterialParam::Type::Float && strcmp(n,"emissiveStrength")==0) uboData.emissiveStrength=p.f;
            else if (p.type==NkMaterialParam::Type::Float && strcmp(n,"alphaCutoff")==0)      uboData.alphaCutoff=p.f;
            else if (p.type==NkMaterialParam::Type::Bool  && strcmp(n,"useAlbedoMap")==0)     uboData.useAlbedoMap=p.b?1:0;
            else if (p.type==NkMaterialParam::Type::Bool  && strcmp(n,"useNormalMap")==0)     uboData.useNormalMap=p.b?1:0;
            else if (p.type==NkMaterialParam::Type::Bool  && strcmp(n,"useMetalRoughMap")==0) uboData.useMetalRoughMap=p.b?1:0;
            else if (p.type==NkMaterialParam::Type::Bool  && strcmp(n,"useOcclusionMap")==0)  uboData.useOcclusionMap=p.b?1:0;
            else if (p.type==NkMaterialParam::Type::Bool  && strcmp(n,"useEmissiveMap")==0)   uboData.useEmissiveMap=p.b?1:0;
            else if (p.type==NkMaterialParam::Type::Texture && strcmp(n,"uAlbedoMap")==0) {
                const NkTextureEntry* te=GetTexture(p.texture);
                if (te&&te->valid) { texSlots[0].id=te->rhiTextureId; sampSlots[0].id=te->rhiSamplerId; }
            }
            else if (p.type==NkMaterialParam::Type::Texture && strcmp(n,"uNormalMap")==0) {
                const NkTextureEntry* te=GetTexture(p.texture);
                if (te&&te->valid) { texSlots[1].id=te->rhiTextureId; sampSlots[1].id=te->rhiSamplerId; }
            }
        }

        // Créer/update le MaterialUBO pour cette instance
        if (!mDevice) return false;
        // Utiliser un buffer temporaire (ou persistent par instance dans une vraie impl)
        nkentseu::NkBufferHandle matUBO = mDevice->CreateBuffer(
            nkentseu::NkBufferDesc::Uniform(sizeof(uboData)));
        if (!matUBO.IsValid()) return false;
        mDevice->WriteBuffer(matUBO, &uboData, sizeof(uboData));

        // Écrire les descriptors
        nkentseu::NkDescriptorWrite writes[6]{};
        writes[0].set=set; writes[0].binding=0;
        writes[0].type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER;
        writes[0].buffer=matUBO; writes[0].bufferRange=sizeof(uboData);
        for (int k=0;k<5;++k) {
            writes[k+1].set=set; writes[k+1].binding=k+1;
            writes[k+1].type=nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[k+1].texture=texSlots[k]; writes[k+1].sampler=sampSlots[k];
            writes[k+1].textureLayout=nkentseu::NkResourceState::NK_SHADER_READ;
        }
        mDevice->UpdateDescriptorSets(writes,6);
        e.dirty=false;
        return true;
    }

    inline bool NkVulkanRenderer::CreateRenderTarget_Impl(NkRenderTargetEntry& e) {
        const NkRenderTargetDesc& d=e.desc;
        nkentseu::NkRenderPassDesc rpDesc=nkentseu::NkRenderPassDesc::Forward();
        e.rhiRenderPass=mDevice->CreateRenderPass(rpDesc).id;
        if (!e.rhiRenderPass) return false;
        nkentseu::NkFramebufferDesc fbDesc{};
        nkentseu::NkRenderPassHandle rp; rp.id=e.rhiRenderPass;
        fbDesc.renderPass=rp;
        fbDesc.width  = d.width>0?d.width:mDevice->GetSwapchainWidth();
        fbDesc.height = d.height>0?d.height:mDevice->GetSwapchainHeight();
        for (usize i=0;i<d.colorAttachments.Size();++i) {
            nkentseu::NkTextureHandle t; t.id=d.colorAttachments[i].id;
            fbDesc.colorAttachments.PushBack(t);
            e.colorTextures.PushBack(d.colorAttachments[i]);
        }
        if (d.depthAttachment.IsValid()) {
            nkentseu::NkTextureHandle t; t.id=d.depthAttachment.id;
            fbDesc.depthAttachment=t; e.depthTexture=d.depthAttachment;
        }
        e.rhiFramebuffer=mDevice->CreateFramebuffer(fbDesc).id;
        return e.rhiFramebuffer!=0;
    }

    inline void NkVulkanRenderer::DestroyRenderTarget_Impl(NkRenderTargetEntry& e) {
        nkentseu::NkFramebufferHandle fb; fb.id=e.rhiFramebuffer;
        nkentseu::NkRenderPassHandle  rp; rp.id=e.rhiRenderPass;
        if (fb.IsValid()) mDevice->DestroyFramebuffer(fb);
        if (rp.IsValid()) mDevice->DestroyRenderPass(rp);
        e.rhiFramebuffer=e.rhiRenderPass=0;
    }

    inline bool NkVulkanRenderer::CreateFont_Impl(NkFontEntry&)   { return true; }
    inline void NkVulkanRenderer::DestroyFont_Impl(NkFontEntry&)  {}

    inline bool NkVulkanRenderer::BeginFrame_Impl() {
        EnsureLayouts();
        EnsureUBOs();
        nkentseu::NkFrameContext fc;
        return mDevice->BeginFrame(fc);
    }

    inline bool NkVulkanRenderer::BeginRenderPass_Impl(const NkRenderPassInfo& info,
                                                        NkICommandBuffer* cmd) {
        nkentseu::NkRenderPassHandle  rp  = mDevice->GetSwapchainRenderPass();
        nkentseu::NkFramebufferHandle fbo = mDevice->GetSwapchainFramebuffer();
        if (info.target.IsValid()) {
            const NkRenderTargetEntry* rt=GetRenderTarget(info.target);
            if (rt&&rt->valid) {
                nkentseu::NkRenderPassHandle  cr; cr.id=rt->rhiRenderPass;
                nkentseu::NkFramebufferHandle cf; cf.id=rt->rhiFramebuffer;
                rp=cr; fbo=cf;
            }
        }
        const uint32 W=mDevice->GetSwapchainWidth(), H=mDevice->GetSwapchainHeight();
        nkentseu::NkRect2D area{0,0,(int32)W,(int32)H};
        if (info.clearColor) cmd->SetClearColor(info.clearColorValue.x,info.clearColorValue.y,info.clearColorValue.z,info.clearColorValue.w);
        if (info.clearDepth)  cmd->SetClearDepth(info.clearDepthValue);
        return cmd->BeginRenderPass(rp,fbo,area);
    }

    inline void NkVulkanRenderer::SubmitDrawCalls_Impl(NkRendererCommand& drawCmd,
                                                        NkICommandBuffer* rhiCmd,
                                                        const NkRenderPassInfo& pass) {
        const uint32 W=mDevice->GetSwapchainWidth(), H=mDevice->GetSwapchainHeight();
        nkentseu::NkViewport vp{0.f,0.f,(float32)W,(float32)H,0.f,1.f};
        vp.flipY=true;  // Vulkan : Y-flip viewport
        rhiCmd->SetViewport(vp);
        nkentseu::NkRect2D sc{0,0,(int32)W,(int32)H};
        rhiCmd->SetScissor(sc);

        // Mettre à jour FrameUBO
        if (pass.camera.IsValid()) {
            FrameUBOData fubo{};
            NkMat4f view=GetViewMatrix(pass.camera);
            NkMat4f proj=GetProjectionMatrix(pass.camera);
            NkMat4f vp_=GetViewProjectionMatrix(pass.camera);
            memcpy(fubo.view,view.data,64); memcpy(fubo.proj,proj.data,64); memcpy(fubo.viewProj,vp_.data,64);
            mDevice->WriteBuffer(mFrameUBO,&fubo,sizeof(fubo));
        }
        // Mettre à jour LightsUBO
        {
            LightsUBOData lubo{};
            for (usize i=0;i<drawCmd.GetLights().Size();++i) {
                const NkLightDesc& l=drawCmd.GetLights()[i];
                if (l.type==NkLightType::Directional) {
                    lubo.dirLightDir[0]=l.direction.x; lubo.dirLightDir[1]=l.direction.y; lubo.dirLightDir[2]=l.direction.z;
                    lubo.dirLightColor[0]=l.color.x; lubo.dirLightColor[1]=l.color.y; lubo.dirLightColor[2]=l.color.z; lubo.dirLightColor[3]=l.intensity;
                } else if (l.type==NkLightType::Point && lubo.numPointLights<8) {
                    int j=lubo.numPointLights++;
                    lubo.pointPos[j][0]=l.position.x; lubo.pointPos[j][1]=l.position.y; lubo.pointPos[j][2]=l.position.z; lubo.pointPos[j][3]=l.radius;
                    lubo.pointColor[j][0]=l.color.x; lubo.pointColor[j][1]=l.color.y; lubo.pointColor[j][2]=l.color.z; lubo.pointColor[j][3]=l.intensity;
                }
            }
            mDevice->WriteBuffer(mLightsUBO,&lubo,sizeof(lubo));
        }

        // Binder set=0 (frame)
        if (mFrameDescSet.IsValid()) rhiCmd->BindDescriptorSet(mFrameDescSet,0);

        // Draw calls 3D
        const auto& calls3D=drawCmd.GetDrawCalls3D();
        uint64 lastPipe=0;
        for (usize i=0;i<calls3D.Size();++i) {
            const NkDrawCall3D& dc=calls3D[i];
            if (!dc.mesh.IsValid()||!dc.material.IsValid()) continue;
            const NkMeshEntry* mesh=GetMesh(dc.mesh);
            NkMaterialInstEntry* inst=GetMaterialInst(dc.material);
            if (!mesh||!inst||!mesh->valid||!inst->valid) continue;
            const NkMaterialEntry* tmpl=GetMaterial(inst->templateHandle);
            if (!tmpl||!tmpl->valid) continue;

            // Pipeline
            nkentseu::NkPipelineHandle pipe; pipe.id=tmpl->rhiPipeline;
            if (pipe.id!=lastPipe&&pipe.IsValid()) {
                rhiCmd->BindGraphicsPipeline(pipe); lastPipe=pipe.id;
            }

            // DrawUBO
            DrawUBOData dubo{};
            memcpy(dubo.model,dc.transform.data,64); memcpy(dubo.modelIT,dc.transform.data,64);
            dubo.objectColor[0]=dubo.objectColor[1]=dubo.objectColor[2]=dubo.objectColor[3]=1.f;
            mDevice->WriteBuffer(mDrawUBO,&dubo,sizeof(dubo));
            if (mDrawDescSet.IsValid()) rhiCmd->BindDescriptorSet(mDrawDescSet,2);

            // set=1 (material)
            if (inst->dirty) RebuildMatInst_Impl(*inst,*tmpl);
            nkentseu::NkDescSetHandle ds; ds.id=inst->rhiDescSet;
            if (ds.IsValid()) rhiCmd->BindDescriptorSet(ds,1);

            nkentseu::NkBufferHandle vbo; vbo.id=mesh->rhiVBO;
            rhiCmd->BindVertexBuffer(0,vbo);
            if (mesh->indexCount>0) {
                nkentseu::NkBufferHandle ibo; ibo.id=mesh->rhiIBO;
                rhiCmd->BindIndexBuffer(ibo,nkentseu::NkIndexFormat::NK_UINT32);
                rhiCmd->DrawIndexed(mesh->indexCount);
            } else rhiCmd->Draw(mesh->vertexCount);
        }

        mStats.drawCalls3D=(uint32)calls3D.Size();
    }

    inline void NkVulkanRenderer::EndRenderPass_Impl(NkICommandBuffer* cmd) { cmd->EndRenderPass(); }
    inline void NkVulkanRenderer::EndFrame_Impl(NkICommandBuffer* /*cmd*/) {
        nkentseu::NkFrameContext fc;
        fc.frameIndex=mDevice->GetFrameIndex(); fc.frameNumber=mDevice->GetFrameNumber();
        mDevice->EndFrame(fc);
    }

} // namespace renderer
} // namespace nkentseu

#endif // NK_RHI_VK_ENABLED