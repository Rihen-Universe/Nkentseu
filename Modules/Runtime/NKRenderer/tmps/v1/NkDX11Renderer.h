#pragma once
// =============================================================================
// NkDX11Renderer.h
// Backend DirectX 11 pour NKRenderer.
//
// Spécificités DX11 :
//   • Shaders HLSL SM 5.0 compilés via fxc (à travers NkIDevice DX11)
//   • Registres : b0=Frame, b1=Draw, b2=Material, b3=Lights
//                 t0-t4=textures, s0-s4=samplers
//   • NDC : Z ∈ [0,1], Y vers le haut, UV Y=0 en haut du texel
//   • Matrices : column-major (même convention que le code C++)
//   • Pas de descriptor sets : les bindings sont implicites (cbuffer register)
// =============================================================================
#include "NKRenderer/Core/NkBaseRenderer.h"

#ifdef NK_RHI_DX11_ENABLED

namespace nkentseu {
namespace renderer {

    class NkDX11Renderer : public NkBaseRenderer {
    public:
        explicit NkDX11Renderer(NkIDevice* device)
            : NkBaseRenderer(device) {}

        ~NkDX11Renderer() override = default;

    protected:
        bool CreateShader_Impl   (NkShaderEntry& e)                              override;
        void DestroyShader_Impl  (NkShaderEntry& e)                              override;
        bool CreateTexture_Impl  (NkTextureEntry& e)                             override;
        void DestroyTexture_Impl (NkTextureEntry& e)                             override;
        bool UpdateTexture_Impl  (NkTextureEntry& e, const void* d,
                                   uint32 rp, uint32 mip, uint32 lay)           override;
        bool CreateMesh_Impl     (NkMeshEntry& e)                                override;
        void DestroyMesh_Impl    (NkMeshEntry& e)                                override;
        bool UpdateMesh_Impl     (NkMeshEntry& e, const void* v, uint32 vc,
                                   const void* i, uint32 ic)                    override;
        bool CreateMaterial_Impl (NkMaterialEntry& e)                            override;
        void DestroyMaterial_Impl(NkMaterialEntry& e)                            override;
        bool CreateMatInst_Impl  (NkMaterialInstEntry& e,
                                   const NkMaterialEntry& t)                    override;
        void DestroyMatInst_Impl (NkMaterialInstEntry& e)                        override;
        bool RebuildMatInst_Impl (NkMaterialInstEntry& e,
                                   const NkMaterialEntry& t)                    override;
        bool CreateRenderTarget_Impl (NkRenderTargetEntry& e)                    override;
        void DestroyRenderTarget_Impl(NkRenderTargetEntry& e)                    override;
        bool CreateFont_Impl     (NkFontEntry& e)                                override;
        void DestroyFont_Impl    (NkFontEntry& e)                                override;
        bool BeginFrame_Impl     ()                                              override;
        bool BeginRenderPass_Impl(const NkRenderPassInfo& i,
                                   NkICommandBuffer* cmd)                        override;
        void SubmitDrawCalls_Impl(NkRendererCommand& dc,
                                   NkICommandBuffer* cmd,
                                   const NkRenderPassInfo& p)                   override;
        void EndRenderPass_Impl  (NkICommandBuffer* cmd)                         override;
        void EndFrame_Impl       (NkICommandBuffer* cmd)                         override;

    private:
        static nkentseu::NkGPUFormat   ToRhiFormat(NkTextureFormat f);
        static nkentseu::NkBlendDesc   ToRhiBlend (NkBlendMode b);
        static nkentseu::NkCullMode    ToRhiCull  (NkCullMode c);
        static nkentseu::NkFillMode    ToRhiFill  (NkFillMode f);

        // UBOs persistants (identique OpenGL mais avec ConstantBuffer sémantique DX11)
        struct FrameCB { float view[16]; float proj[16]; float vp[16]; float cam[4]; float t[4]; };
        struct DrawCB  { float model[16]; float modelIT[16]; float color[4]; };
        struct LightsCB{
            float dirDir[4]; float dirCol[4];
            float ptPos[8][4]; float ptCol[8][4];
            int32 numPt; int32 _pad[3];
        };

        nkentseu::NkBufferHandle mFrameCB;
        nkentseu::NkBufferHandle mDrawCB;
        nkentseu::NkBufferHandle mLightsCB;

        // DX11 utilise un DescriptorSetLayout simplifié (1 binding = 1 resource)
        nkentseu::NkDescSetHandle mFrameLayout;
        nkentseu::NkDescSetHandle mMatLayout;
        bool mLayoutsReady = false;

        nkentseu::NkTextureHandle mWhiteTex;
        nkentseu::NkSamplerHandle mDefaultSampler;

        bool EnsureResources();
        void UpdateLightsCB(const NkRendererCommand& cmd);
        void DrawScene3D(NkRendererCommand& dc, NkICommandBuffer* cmd);
        void DrawScene2D(NkRendererCommand& dc, NkICommandBuffer* cmd);
    };

    // ── Conversions ────────────────────────────────────────────────────────────
    inline nkentseu::NkGPUFormat NkDX11Renderer::ToRhiFormat(NkTextureFormat f) {
        switch(f){
            case NkTextureFormat::RGBA8_SRGB:  return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
            case NkTextureFormat::RGBA8_UNORM: return nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
            case NkTextureFormat::RGBA16F:     return nkentseu::NkGPUFormat::NK_RGBA16_FLOAT;
            case NkTextureFormat::RGBA32F:     return nkentseu::NkGPUFormat::NK_RGBA32_FLOAT;
            case NkTextureFormat::R32F:        return nkentseu::NkGPUFormat::NK_R32_FLOAT;
            case NkTextureFormat::D32F:        return nkentseu::NkGPUFormat::NK_D32_FLOAT;
            case NkTextureFormat::D24S8:       return nkentseu::NkGPUFormat::NK_D24_UNORM_S8_UINT;
            default:                           return nkentseu::NkGPUFormat::NK_RGBA8_SRGB;
        }
    }
    inline nkentseu::NkBlendDesc NkDX11Renderer::ToRhiBlend(NkBlendMode b){
        switch(b){
            case NkBlendMode::AlphaBlend:   return nkentseu::NkBlendDesc::Alpha();
            case NkBlendMode::Additive:     return nkentseu::NkBlendDesc::Additive();
            default:                        return nkentseu::NkBlendDesc::Opaque();
        }
    }
    inline nkentseu::NkCullMode NkDX11Renderer::ToRhiCull(NkCullMode c){
        switch(c){case NkCullMode::Front:return nkentseu::NkCullMode::NK_FRONT;case NkCullMode::None:return nkentseu::NkCullMode::NK_NONE;default:return nkentseu::NkCullMode::NK_BACK;}
    }
    inline nkentseu::NkFillMode NkDX11Renderer::ToRhiFill(NkFillMode f){
        return f==NkFillMode::Wireframe?nkentseu::NkFillMode::NK_WIREFRAME:nkentseu::NkFillMode::NK_SOLID;
    }

    inline bool NkDX11Renderer::EnsureResources() {
        if (!mFrameCB.IsValid())
            mFrameCB   = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(FrameCB)));
        if (!mDrawCB.IsValid())
            mDrawCB    = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(DrawCB)));
        if (!mLightsCB.IsValid())
            mLightsCB  = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(LightsCB)));
        if (!mWhiteTex.IsValid()) {
            nkentseu::NkTextureDesc td{};
            td.type=nkentseu::NkTextureType::NK_TEX2D; td.format=nkentseu::NkGPUFormat::NK_RGBA8_UNORM;
            td.width=td.height=1; td.mipLevels=1; td.bindFlags=nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
            mWhiteTex = mDevice->CreateTexture(td);
            static const uint8 w[4]={255,255,255,255};
            mDevice->WriteTexture(mWhiteTex,w,4);
        }
        if (!mDefaultSampler.IsValid())
            mDefaultSampler = mDevice->CreateSampler(nkentseu::NkSamplerDesc::Linear());

        if (!mLayoutsReady) {
            // Layout frame (b0=FrameCB, b3=LightsCB)
            {
                nkentseu::NkDescriptorSetLayoutDesc d;
                d.Add(0,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
                d.Add(3,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_FRAGMENT);
                mFrameLayout = mDevice->CreateDescriptorSetLayout(d);
            }
            // Layout material (b2=MatCB, t0-t4=textures)
            {
                nkentseu::NkDescriptorSetLayoutDesc d;
                d.Add(2,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_FRAGMENT);
                for(int k=0;k<5;++k) d.Add(k,nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,nkentseu::NkShaderStage::NK_FRAGMENT);
                mMatLayout = mDevice->CreateDescriptorSetLayout(d);
            }
            mLayoutsReady = mFrameLayout.IsValid() && mMatLayout.IsValid();
        }
        return mFrameCB.IsValid();
    }

    // ── Shader (HLSL via NkIDevice DX11) ──────────────────────────────────────
    inline bool NkDX11Renderer::CreateShader_Impl(NkShaderEntry& e) {
        nkentseu::NkShaderDesc rhiDesc; rhiDesc.debugName=e.desc.debugName;
        for (usize i=0;i<e.desc.stages.Size();++i) {
            const NkShaderStageDesc& s=e.desc.stages[i];
            if (s.backend!=NkShaderBackend::DX11) continue;
            nkentseu::NkShaderStageDesc rhi{};
            switch(s.stage){
                case NkShaderStageType::Vertex:   rhi.stage=nkentseu::NkShaderStage::NK_VERTEX;   break;
                case NkShaderStageType::Fragment: rhi.stage=nkentseu::NkShaderStage::NK_FRAGMENT; break;
                default: rhi.stage=nkentseu::NkShaderStage::NK_VERTEX;
            }
            rhi.hlslSource  = s.source.CStr();
            rhi.entryPoint  = s.entryPoint.CStr();
            rhiDesc.stages.PushBack(rhi);
        }
        if (rhiDesc.stages.IsEmpty()) return false;
        nkentseu::NkShaderHandle h=mDevice->CreateShader(rhiDesc);
        if (!h.IsValid()) return false;
        e.rhiHandle.id=h.id; return true;
    }
    inline void NkDX11Renderer::DestroyShader_Impl(NkShaderEntry& e){
        nkentseu::NkShaderHandle h; h.id=e.rhiHandle.id;
        if(h.IsValid()) mDevice->DestroyShader(h); e.rhiHandle.id=0;
    }

    // ── Texture ───────────────────────────────────────────────────────────────
    inline bool NkDX11Renderer::CreateTexture_Impl(NkTextureEntry& e){
        const NkTextureDesc& td=e.desc;
        nkentseu::NkTextureDesc rhi{};
        rhi.type  = nkentseu::NkTextureType::NK_TEX2D;
        rhi.format= ToRhiFormat(td.format);
        rhi.width=td.width; rhi.height=td.height; rhi.depth=td.depth;
        rhi.arrayLayers=td.layers; rhi.mipLevels=td.mipLevels;
        rhi.samples=nkentseu::NkSampleCount::NK_S1;
        rhi.debugName=td.debugName; rhi.initialData=td.initialData; rhi.rowPitch=td.rowPitch;
        rhi.bindFlags = td.isRenderTarget
            ? (nkentseu::NkBindFlags::NK_RENDER_TARGET|nkentseu::NkBindFlags::NK_SHADER_RESOURCE)
            : td.isDepthTarget
                ? (nkentseu::NkBindFlags::NK_DEPTH_STENCIL|nkentseu::NkBindFlags::NK_SHADER_RESOURCE)
                : nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
        nkentseu::NkTextureHandle tex=mDevice->CreateTexture(rhi);
        if (!tex.IsValid()) return false;
        e.rhiTextureId=tex.id;
        nkentseu::NkSamplerDesc sd{};
        sd.magFilter = (td.sampler.filter==NkSamplerFilter::Nearest)?nkentseu::NkFilter::NK_NEAREST:nkentseu::NkFilter::NK_LINEAR;
        sd.minFilter = sd.magFilter;
        sd.mipFilter = (td.sampler.filter==NkSamplerFilter::Nearest)?nkentseu::NkMipFilter::NK_NEAREST:nkentseu::NkMipFilter::NK_LINEAR;
        sd.maxAnisotropy=td.sampler.anisotropy;
        nkentseu::NkSamplerHandle samp=mDevice->CreateSampler(sd);
        if (!samp.IsValid()) { mDevice->DestroyTexture(tex); return false; }
        e.rhiSamplerId=samp.id; return true;
    }
    inline void NkDX11Renderer::DestroyTexture_Impl(NkTextureEntry& e){
        nkentseu::NkTextureHandle t; t.id=e.rhiTextureId;
        nkentseu::NkSamplerHandle s; s.id=e.rhiSamplerId;
        if(t.IsValid()) mDevice->DestroyTexture(t);
        if(s.IsValid()) mDevice->DestroySampler(s);
        e.rhiTextureId=e.rhiSamplerId=0;
    }
    inline bool NkDX11Renderer::UpdateTexture_Impl(NkTextureEntry& e,const void* d,uint32 rp,uint32 mip,uint32 lay){
        nkentseu::NkTextureHandle t; t.id=e.rhiTextureId;
        if(!t.IsValid()) return false;
        if(mip==0&&lay==0) return mDevice->WriteTexture(t,d,rp);
        return mDevice->WriteTextureRegion(t,d,0,0,0,e.desc.width,e.desc.height,1,mip,lay,rp);
    }

    // ── Mesh ──────────────────────────────────────────────────────────────────
    inline bool NkDX11Renderer::CreateMesh_Impl(NkMeshEntry& e){
        const NkMeshDesc& md=e.desc;
        uint32 stride=md.Is2D()?sizeof(NkVertex2D):sizeof(NkVertex3D);
        const void* vdata=md.Is2D()?(const void*)md.vertices2D:(const void*)md.vertices3D;
        const uint64 vsz=(uint64)md.vertexCount*stride;
        const uint64 isz=md.indexCount*(md.idxFmt==NkIndexFormat::UInt16?2:4);
        nkentseu::NkBufferHandle vbo=(md.usage==NkMeshUsage::Static&&vdata)
            ?mDevice->CreateBuffer(nkentseu::NkBufferDesc::Vertex(vsz,vdata))
            :mDevice->CreateBuffer(nkentseu::NkBufferDesc::VertexDynamic(vsz>0?vsz:64));
        if(!vbo.IsValid()) return false; e.rhiVBO=vbo.id;
        if(isz>0){
            const void* idata=md.idxFmt==NkIndexFormat::UInt16?(const void*)md.indices16:(const void*)md.indices32;
            nkentseu::NkBufferHandle ibo=(md.usage==NkMeshUsage::Static&&idata)
                ?mDevice->CreateBuffer(nkentseu::NkBufferDesc::Index(isz,idata))
                :mDevice->CreateBuffer(nkentseu::NkBufferDesc::IndexDynamic(isz));
            if(!ibo.IsValid()){mDevice->DestroyBuffer(vbo);return false;}
            e.rhiIBO=ibo.id;
        }
        return true;
    }
    inline void NkDX11Renderer::DestroyMesh_Impl(NkMeshEntry& e){
        nkentseu::NkBufferHandle vbo; vbo.id=e.rhiVBO; nkentseu::NkBufferHandle ibo; ibo.id=e.rhiIBO;
        if(vbo.IsValid())mDevice->DestroyBuffer(vbo); if(ibo.IsValid())mDevice->DestroyBuffer(ibo);
        e.rhiVBO=e.rhiIBO=0;
    }
    inline bool NkDX11Renderer::UpdateMesh_Impl(NkMeshEntry& e,const void* v,uint32 vc,const void* i,uint32 ic){
        nkentseu::NkBufferHandle vbo; vbo.id=e.rhiVBO; nkentseu::NkBufferHandle ibo; ibo.id=e.rhiIBO;
        uint32 s=e.desc.Is2D()?sizeof(NkVertex2D):sizeof(NkVertex3D);
        bool ok=true;
        if(vbo.IsValid()&&v&&vc) ok&=mDevice->WriteBuffer(vbo,v,(uint64)vc*s);
        if(ibo.IsValid()&&i&&ic) ok&=mDevice->WriteBuffer(ibo,i,(uint64)ic*(e.desc.idxFmt==NkIndexFormat::UInt16?2:4));
        e.vertexCount=vc; e.indexCount=ic; return ok;
    }

    // ── Material ──────────────────────────────────────────────────────────────
    inline bool NkDX11Renderer::CreateMaterial_Impl(NkMaterialEntry& e){
        EnsureResources();
        e.rhiPipeline=mMatLayout.id;
        return mMatLayout.IsValid();
    }
    inline void NkDX11Renderer::DestroyMaterial_Impl(NkMaterialEntry& e){ e.rhiPipeline=0; }

    inline bool NkDX11Renderer::CreateMatInst_Impl(NkMaterialInstEntry& e,const NkMaterialEntry& /*t*/){
        EnsureResources();
        nkentseu::NkDescSetHandle layout; layout.id=mMatLayout.id;
        if(!layout.IsValid()) return false;
        nkentseu::NkDescSetHandle set=mDevice->AllocateDescriptorSet(layout);
        if(!set.IsValid()) return false;
        e.rhiDescSet=set.id; e.dirty=true;
        // Lier white tex par défaut
        nkentseu::NkDescriptorWrite ws[5]{};
        for(int k=0;k<5;++k){
            ws[k].set=set; ws[k].binding=k;
            ws[k].type=nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            ws[k].texture=mWhiteTex; ws[k].sampler=mDefaultSampler;
            ws[k].textureLayout=nkentseu::NkResourceState::NK_SHADER_READ;
        }
        mDevice->UpdateDescriptorSets(ws,5);
        return true;
    }
    inline void NkDX11Renderer::DestroyMatInst_Impl(NkMaterialInstEntry& e){
        nkentseu::NkDescSetHandle s; s.id=e.rhiDescSet;
        if(s.IsValid()) mDevice->FreeDescriptorSet(s); e.rhiDescSet=0;
    }
    inline bool NkDX11Renderer::RebuildMatInst_Impl(NkMaterialInstEntry& e,const NkMaterialEntry& /*t*/){
        if(!e.rhiDescSet) return false;
        nkentseu::NkDescSetHandle set; set.id=e.rhiDescSet;
        // Mettre à jour les textures selon les params
        for(usize pi=0;pi<e.params.Size();++pi){
            const NkMaterialParam& p=e.params[pi];
            if(p.type!=NkMaterialParam::Type::Texture) continue;
            const NkTextureEntry* te=GetTexture(p.texture);
            if(!te||!te->valid) continue;
            int slot=-1;
            if(strcmp(p.name.CStr(),"uAlbedoMap")==0)     slot=0;
            else if(strcmp(p.name.CStr(),"uNormalMap")==0) slot=1;
            else if(strcmp(p.name.CStr(),"uMetalRoughMap")==0) slot=2;
            else if(strcmp(p.name.CStr(),"uOcclusionMap")==0)  slot=3;
            else if(strcmp(p.name.CStr(),"uEmissiveMap")==0)   slot=4;
            if(slot<0) continue;
            nkentseu::NkDescriptorWrite w{};
            w.set=set; w.binding=slot;
            w.type=nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w.texture.id=te->rhiTextureId; w.sampler.id=te->rhiSamplerId;
            w.textureLayout=nkentseu::NkResourceState::NK_SHADER_READ;
            mDevice->UpdateDescriptorSets(&w,1);
        }
        e.dirty=false; return true;
    }

    // ── Render Target ─────────────────────────────────────────────────────────
    inline bool NkDX11Renderer::CreateRenderTarget_Impl(NkRenderTargetEntry& e){
        nkentseu::NkRenderPassDesc rpd=nkentseu::NkRenderPassDesc::Forward();
        e.rhiRenderPass=mDevice->CreateRenderPass(rpd).id;
        if(!e.rhiRenderPass) return false;
        nkentseu::NkFramebufferDesc fd{};
        nkentseu::NkRenderPassHandle rp; rp.id=e.rhiRenderPass; fd.renderPass=rp;
        fd.width=e.desc.width>0?e.desc.width:mDevice->GetSwapchainWidth();
        fd.height=e.desc.height>0?e.desc.height:mDevice->GetSwapchainHeight();
        for(usize i=0;i<e.desc.colorAttachments.Size();++i){
            nkentseu::NkTextureHandle t; t.id=e.desc.colorAttachments[i].id;
            fd.colorAttachments.PushBack(t); e.colorTextures.PushBack(e.desc.colorAttachments[i]);
        }
        if(e.desc.depthAttachment.IsValid()){
            nkentseu::NkTextureHandle t; t.id=e.desc.depthAttachment.id;
            fd.depthAttachment=t; e.depthTexture=e.desc.depthAttachment;
        }
        e.rhiFramebuffer=mDevice->CreateFramebuffer(fd).id;
        return e.rhiFramebuffer!=0;
    }
    inline void NkDX11Renderer::DestroyRenderTarget_Impl(NkRenderTargetEntry& e){
        nkentseu::NkFramebufferHandle fb; fb.id=e.rhiFramebuffer;
        nkentseu::NkRenderPassHandle  rp; rp.id=e.rhiRenderPass;
        if(fb.IsValid())mDevice->DestroyFramebuffer(fb);
        if(rp.IsValid())mDevice->DestroyRenderPass(rp);
        e.rhiFramebuffer=e.rhiRenderPass=0;
    }

    inline bool NkDX11Renderer::CreateFont_Impl(NkFontEntry&){ return true; }
    inline void NkDX11Renderer::DestroyFont_Impl(NkFontEntry&){}

    inline void NkDX11Renderer::UpdateLightsCB(const NkRendererCommand& cmd){
        LightsCB lb{};
        for(usize i=0;i<cmd.GetLights().Size();++i){
            const NkLightDesc& l=cmd.GetLights()[i];
            if(l.type==NkLightType::Directional){
                lb.dirDir[0]=l.direction.x; lb.dirDir[1]=l.direction.y; lb.dirDir[2]=l.direction.z;
                lb.dirCol[0]=l.color.x; lb.dirCol[1]=l.color.y; lb.dirCol[2]=l.color.z; lb.dirCol[3]=l.intensity;
            }else if(l.type==NkLightType::Point&&lb.numPt<8){
                int j=lb.numPt++;
                lb.ptPos[j][0]=l.position.x; lb.ptPos[j][1]=l.position.y; lb.ptPos[j][2]=l.position.z; lb.ptPos[j][3]=l.radius;
                lb.ptCol[j][0]=l.color.x; lb.ptCol[j][1]=l.color.y; lb.ptCol[j][2]=l.color.z; lb.ptCol[j][3]=l.intensity;
            }
        }
        mDevice->WriteBuffer(mLightsCB,&lb,sizeof(lb));
    }

    inline bool NkDX11Renderer::BeginFrame_Impl(){
        EnsureResources();
        nkentseu::NkFrameContext fc;
        return mDevice->BeginFrame(fc);
    }

    inline bool NkDX11Renderer::BeginRenderPass_Impl(const NkRenderPassInfo& info, NkICommandBuffer* cmd){
        nkentseu::NkRenderPassHandle rp=mDevice->GetSwapchainRenderPass();
        nkentseu::NkFramebufferHandle fbo=mDevice->GetSwapchainFramebuffer();
        if(info.target.IsValid()){
            const NkRenderTargetEntry* rt=GetRenderTarget(info.target);
            if(rt&&rt->valid){nkentseu::NkRenderPassHandle cr;cr.id=rt->rhiRenderPass;nkentseu::NkFramebufferHandle cf;cf.id=rt->rhiFramebuffer;rp=cr;fbo=cf;}
        }
        const uint32 W=mDevice->GetSwapchainWidth(),H=mDevice->GetSwapchainHeight();
        nkentseu::NkRect2D area{0,0,(int32)W,(int32)H};
        if(info.clearColor) cmd->SetClearColor(info.clearColorValue.x,info.clearColorValue.y,info.clearColorValue.z,info.clearColorValue.w);
        if(info.clearDepth)  cmd->SetClearDepth(info.clearDepthValue);
        return cmd->BeginRenderPass(rp,fbo,area);
    }

    inline void NkDX11Renderer::SubmitDrawCalls_Impl(NkRendererCommand& drawCmd,
                                                       NkICommandBuffer* cmd,
                                                       const NkRenderPassInfo& pass){
        const uint32 W=mDevice->GetSwapchainWidth(),H=mDevice->GetSwapchainHeight();
        nkentseu::NkViewport vp{0.f,0.f,(float32)W,(float32)H,0.f,1.f};
        vp.flipY=false;  // DX11 : pas de Y-flip
        cmd->SetViewport(vp);
        nkentseu::NkRect2D sc{0,0,(int32)W,(int32)H};
        cmd->SetScissor(sc);

        // FrameCB
        if(pass.camera.IsValid()){
            FrameCB fb{};
            NkMat4f v=GetViewMatrix(pass.camera), p=GetProjectionMatrix(pass.camera), vp_=GetViewProjectionMatrix(pass.camera);
            memcpy(fb.view,v.data,64); memcpy(fb.proj,p.data,64); memcpy(fb.vp,vp_.data,64);
            mDevice->WriteBuffer(mFrameCB,&fb,sizeof(fb));
        }
        UpdateLightsCB(drawCmd);
        DrawScene3D(drawCmd,cmd);
        DrawScene2D(drawCmd,cmd);
        mStats.drawCalls3D=(uint32)drawCmd.GetDrawCalls3D().Size();
    }

    inline void NkDX11Renderer::DrawScene3D(NkRendererCommand& drawCmd,NkICommandBuffer* cmd){
        uint64 lastPipe=0;
        for(usize i=0;i<drawCmd.GetDrawCalls3D().Size();++i){
            const NkDrawCall3D& dc=drawCmd.GetDrawCalls3D()[i];
            if(!dc.mesh.IsValid()||!dc.material.IsValid()) continue;
            const NkMeshEntry* mesh=GetMesh(dc.mesh);
            NkMaterialInstEntry* inst=GetMaterialInst(dc.material);
            if(!mesh||!inst||!mesh->valid||!inst->valid) continue;
            const NkMaterialEntry* tmpl=GetMaterial(inst->templateHandle);
            if(!tmpl||!tmpl->valid) continue;
            nkentseu::NkPipelineHandle pipe; pipe.id=tmpl->rhiPipeline;
            if(pipe.id!=lastPipe&&pipe.IsValid()){cmd->BindGraphicsPipeline(pipe);lastPipe=pipe.id;}
            DrawCB db{};
            memcpy(db.model,dc.transform.data,64); memcpy(db.modelIT,dc.transform.data,64);
            db.color[0]=db.color[1]=db.color[2]=db.color[3]=1.f;
            mDevice->WriteBuffer(mDrawCB,&db,sizeof(db));
            if(inst->dirty) RebuildMatInst_Impl(*inst,*tmpl);
            nkentseu::NkDescSetHandle ds; ds.id=inst->rhiDescSet;
            if(ds.IsValid()) cmd->BindDescriptorSet(ds,0);
            nkentseu::NkBufferHandle vbo; vbo.id=mesh->rhiVBO;
            cmd->BindVertexBuffer(0,vbo);
            if(mesh->indexCount>0){
                nkentseu::NkBufferHandle ibo; ibo.id=mesh->rhiIBO;
                cmd->BindIndexBuffer(ibo,nkentseu::NkIndexFormat::NK_UINT32);
                cmd->DrawIndexed(mesh->indexCount);
            }else cmd->Draw(mesh->vertexCount);
        }
    }
    inline void NkDX11Renderer::DrawScene2D(NkRendererCommand& /*dc*/,NkICommandBuffer* /*cmd*/){}

    inline void NkDX11Renderer::EndRenderPass_Impl(NkICommandBuffer* cmd){cmd->EndRenderPass();}
    inline void NkDX11Renderer::EndFrame_Impl(NkICommandBuffer* /*cmd*/){
        nkentseu::NkFrameContext fc; fc.frameIndex=mDevice->GetFrameIndex(); fc.frameNumber=mDevice->GetFrameNumber();
        mDevice->EndFrame(fc);
    }

} // namespace renderer
} // namespace nkentseu

#endif // NK_RHI_DX11_ENABLED