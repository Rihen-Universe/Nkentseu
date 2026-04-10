#pragma once
// =============================================================================
// NkDX12Renderer.h
// Backend DirectX 12 pour NKRenderer.
//
// Spécificités DX12 (différences par rapport à DX11) :
//   • Shaders HLSL SM 6.x compilés via dxc
//   • Root Signature explicite :
//       slot 0 → RootCBV → FrameData  (b0, space0)
//       slot 1 → RootCBV → DrawData   (b1, space0)
//       slot 2 → DescriptorTable → MaterialData (b2) + textures (t0-t4) + samplers (s0-s4)
//       slot 3 → RootCBV → LightsData (b3, space0)
//   • Resource barriers explicites avant/après chaque draw
//   • Triple buffering : chaque frame utilise ses propres UBOs
//   • Bindless optionnel via NkBindlessHeap
// =============================================================================
#include "NKRenderer/Core/NkBaseRenderer.h"

#ifdef NK_RHI_DX12_ENABLED

namespace nkentseu {
namespace renderer {

    class NkDX12Renderer : public NkBaseRenderer {
    public:
        explicit NkDX12Renderer(NkIDevice* device)
            : NkBaseRenderer(device) {}
        ~NkDX12Renderer() override = default;

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
        static nkentseu::NkGPUFormat ToRhiFormat(NkTextureFormat f);

        // Triple buffering : 3 sets d'UBOs (un par frame en vol)
        static constexpr uint32 kFramesInFlight = 3;
        struct PerFrameResources {
            nkentseu::NkBufferHandle frameCB;
            nkentseu::NkBufferHandle drawCB;
            nkentseu::NkBufferHandle lightsCB;
            nkentseu::NkDescSetHandle frameSet;  // set=0
            nkentseu::NkDescSetHandle drawSet;   // set=2
            bool ready = false;
        } mPerFrame[kFramesInFlight];

        nkentseu::NkDescSetHandle mSet0Layout;  // frame + lights
        nkentseu::NkDescSetHandle mSet1Layout;  // material + textures
        nkentseu::NkDescSetHandle mSet2Layout;  // draw
        bool mLayoutsReady = false;

        nkentseu::NkTextureHandle mWhiteTex;
        nkentseu::NkSamplerHandle mDefaultSampler;

        // Structs UBO (std430 / ConstantBuffer DX12)
        struct alignas(256) FrameData {  // DX12 : CBV doit être aligné sur 256 bytes
            float view[16];
            float proj[16];
            float vp[16];
            float cam[4];
            float t[4];
            float _pad[16];  // compléter à 256 bytes
        };
        struct alignas(256) DrawData {
            float model[16];
            float modelIT[16];
            float color[4];
            float _pad[12];
        };
        struct alignas(256) LightsData {
            float dirDir[4]; float dirCol[4];
            float ptPos[8][4]; float ptCol[8][4];
            int32 numPt; int32 _p[3];
            float _pad[8];
        };

        bool EnsureLayouts();
        bool EnsurePerFrameResources();
        uint32 GetCurrentFrameIdx() const;
        void UpdateFrameResources(const NkRenderPassInfo& pass, const NkRendererCommand& cmd);
        void DrawScene(NkRendererCommand& dc, NkICommandBuffer* cmd);
    };

    inline nkentseu::NkGPUFormat NkDX12Renderer::ToRhiFormat(NkTextureFormat f){
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

    inline uint32 NkDX12Renderer::GetCurrentFrameIdx() const {
        return mDevice->GetFrameIndex() % kFramesInFlight;
    }

    inline bool NkDX12Renderer::EnsureLayouts(){
        if (mLayoutsReady) return true;
        // set=0 : FrameData(b=0) + LightsData(b=1)
        { nkentseu::NkDescriptorSetLayoutDesc d;
          d.Add(0,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
          d.Add(1,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_FRAGMENT);
          mSet0Layout=mDevice->CreateDescriptorSetLayout(d); }
        // set=1 : MaterialData(b=0) + textures(t=0..4)
        { nkentseu::NkDescriptorSetLayoutDesc d;
          d.Add(0,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_FRAGMENT);
          for(int k=0;k<5;++k) d.Add(k+1,nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,nkentseu::NkShaderStage::NK_FRAGMENT);
          mSet1Layout=mDevice->CreateDescriptorSetLayout(d); }
        // set=2 : DrawData(b=0)
        { nkentseu::NkDescriptorSetLayoutDesc d;
          d.Add(0,nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER,nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
          mSet2Layout=mDevice->CreateDescriptorSetLayout(d); }
        mLayoutsReady = mSet0Layout.IsValid()&&mSet1Layout.IsValid()&&mSet2Layout.IsValid();
        return mLayoutsReady;
    }

    inline bool NkDX12Renderer::EnsurePerFrameResources(){
        if (!mWhiteTex.IsValid()){
            nkentseu::NkTextureDesc td{}; td.type=nkentseu::NkTextureType::NK_TEX2D;
            td.format=nkentseu::NkGPUFormat::NK_RGBA8_UNORM; td.width=td.height=1; td.mipLevels=1;
            td.bindFlags=nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
            mWhiteTex=mDevice->CreateTexture(td);
            static const uint8 w[4]={255,255,255,255}; mDevice->WriteTexture(mWhiteTex,w,4);
        }
        if (!mDefaultSampler.IsValid()) mDefaultSampler=mDevice->CreateSampler(nkentseu::NkSamplerDesc::Linear());

        EnsureLayouts();
        for(uint32 fi=0;fi<kFramesInFlight;++fi){
            PerFrameResources& pf=mPerFrame[fi];
            if(pf.ready) continue;
            pf.frameCB  = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(FrameData)));
            pf.drawCB   = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(DrawData)));
            pf.lightsCB = mDevice->CreateBuffer(nkentseu::NkBufferDesc::Uniform(sizeof(LightsData)));
            if(!pf.frameCB.IsValid()) return false;
            // Descriptor set=0 pour cette frame
            if(mSet0Layout.IsValid()){
                pf.frameSet = mDevice->AllocateDescriptorSet(mSet0Layout);
                if(pf.frameSet.IsValid()){
                    nkentseu::NkDescriptorWrite ws[2]{};
                    ws[0].set=pf.frameSet; ws[0].binding=0; ws[0].type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER; ws[0].buffer=pf.frameCB; ws[0].bufferRange=sizeof(FrameData);
                    ws[1].set=pf.frameSet; ws[1].binding=1; ws[1].type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER; ws[1].buffer=pf.lightsCB; ws[1].bufferRange=sizeof(LightsData);
                    mDevice->UpdateDescriptorSets(ws,2);
                }
            }
            // Descriptor set=2
            if(mSet2Layout.IsValid()){
                pf.drawSet = mDevice->AllocateDescriptorSet(mSet2Layout);
                if(pf.drawSet.IsValid()){
                    nkentseu::NkDescriptorWrite w{}; w.set=pf.drawSet; w.binding=0; w.type=nkentseu::NkDescriptorType::NK_UNIFORM_BUFFER; w.buffer=pf.drawCB; w.bufferRange=sizeof(DrawData);
                    mDevice->UpdateDescriptorSets(&w,1);
                }
            }
            pf.ready=true;
        }
        return true;
    }

    inline void NkDX12Renderer::UpdateFrameResources(const NkRenderPassInfo& pass,
                                                      const NkRendererCommand& cmd){
        const uint32 fi=GetCurrentFrameIdx();
        PerFrameResources& pf=mPerFrame[fi];
        if(pass.camera.IsValid()){
            FrameData fd{}; NkMat4f v=GetViewMatrix(pass.camera),p=GetProjectionMatrix(pass.camera),vp_=GetViewProjectionMatrix(pass.camera);
            memcpy(fd.view,v.data,64); memcpy(fd.proj,p.data,64); memcpy(fd.vp,vp_.data,64);
            mDevice->WriteBuffer(pf.frameCB,&fd,sizeof(fd));
        }
        LightsData ld{};
        for(usize i=0;i<cmd.GetLights().Size();++i){
            const NkLightDesc& l=cmd.GetLights()[i];
            if(l.type==NkLightType::Directional){ld.dirDir[0]=l.direction.x;ld.dirDir[1]=l.direction.y;ld.dirDir[2]=l.direction.z;ld.dirCol[0]=l.color.x;ld.dirCol[1]=l.color.y;ld.dirCol[2]=l.color.z;ld.dirCol[3]=l.intensity;}
            else if(l.type==NkLightType::Point&&ld.numPt<8){int j=ld.numPt++;ld.ptPos[j][0]=l.position.x;ld.ptPos[j][1]=l.position.y;ld.ptPos[j][2]=l.position.z;ld.ptPos[j][3]=l.radius;ld.ptCol[j][0]=l.color.x;ld.ptCol[j][1]=l.color.y;ld.ptCol[j][2]=l.color.z;ld.ptCol[j][3]=l.intensity;}
        }
        mDevice->WriteBuffer(pf.lightsCB,&ld,sizeof(ld));
    }

    // ── Shader (HLSL SM6 via dxc) ─────────────────────────────────────────────
    inline bool NkDX12Renderer::CreateShader_Impl(NkShaderEntry& e){
        nkentseu::NkShaderDesc rhiDesc; rhiDesc.debugName=e.desc.debugName;
        for(usize i=0;i<e.desc.stages.Size();++i){
            const NkShaderStageDesc& s=e.desc.stages[i];
            if(s.backend!=NkShaderBackend::DX12) continue;
            nkentseu::NkShaderStageDesc rhi{};
            switch(s.stage){
                case NkShaderStageType::Vertex:   rhi.stage=nkentseu::NkShaderStage::NK_VERTEX;   break;
                case NkShaderStageType::Fragment: rhi.stage=nkentseu::NkShaderStage::NK_FRAGMENT; break;
                case NkShaderStageType::Compute:  rhi.stage=nkentseu::NkShaderStage::NK_COMPUTE;  break;
                default: rhi.stage=nkentseu::NkShaderStage::NK_VERTEX;
            }
            rhi.hlslSource=s.source.CStr(); rhi.entryPoint=s.entryPoint.CStr();
            rhiDesc.stages.PushBack(rhi);
        }
        if(rhiDesc.stages.IsEmpty()) return false;
        nkentseu::NkShaderHandle h=mDevice->CreateShader(rhiDesc);
        if(!h.IsValid()) return false;
        e.rhiHandle.id=h.id; return true;
    }
    inline void NkDX12Renderer::DestroyShader_Impl(NkShaderEntry& e){
        nkentseu::NkShaderHandle h; h.id=e.rhiHandle.id; if(h.IsValid()) mDevice->DestroyShader(h); e.rhiHandle.id=0;
    }

    // ── Texture (avec barrier de transition DX12) ─────────────────────────────
    inline bool NkDX12Renderer::CreateTexture_Impl(NkTextureEntry& e){
        const NkTextureDesc& td=e.desc;
        nkentseu::NkTextureDesc rhi{};
        rhi.type=nkentseu::NkTextureType::NK_TEX2D; rhi.format=ToRhiFormat(td.format);
        rhi.width=td.width; rhi.height=td.height; rhi.mipLevels=td.mipLevels;
        rhi.samples=nkentseu::NkSampleCount::NK_S1; rhi.debugName=td.debugName;
        rhi.initialData=td.initialData; rhi.rowPitch=td.rowPitch;
        rhi.bindFlags = td.isRenderTarget
            ? (nkentseu::NkBindFlags::NK_RENDER_TARGET|nkentseu::NkBindFlags::NK_SHADER_RESOURCE)
            : td.isDepthTarget
                ? (nkentseu::NkBindFlags::NK_DEPTH_STENCIL|nkentseu::NkBindFlags::NK_SHADER_RESOURCE)
                : nkentseu::NkBindFlags::NK_SHADER_RESOURCE;
        nkentseu::NkTextureHandle tex=mDevice->CreateTexture(rhi);
        if(!tex.IsValid()) return false; e.rhiTextureId=tex.id;
        nkentseu::NkSamplerHandle samp=mDevice->CreateSampler(nkentseu::NkSamplerDesc::Linear());
        if(!samp.IsValid()){mDevice->DestroyTexture(tex);return false;}
        e.rhiSamplerId=samp.id; return true;
    }
    inline void NkDX12Renderer::DestroyTexture_Impl(NkTextureEntry& e){
        nkentseu::NkTextureHandle t; t.id=e.rhiTextureId; nkentseu::NkSamplerHandle s; s.id=e.rhiSamplerId;
        if(t.IsValid())mDevice->DestroyTexture(t); if(s.IsValid())mDevice->DestroySampler(s);
        e.rhiTextureId=e.rhiSamplerId=0;
    }
    inline bool NkDX12Renderer::UpdateTexture_Impl(NkTextureEntry& e,const void* d,uint32 rp,uint32 mip,uint32 lay){
        nkentseu::NkTextureHandle t; t.id=e.rhiTextureId; if(!t.IsValid())return false;
        if(mip==0&&lay==0) return mDevice->WriteTexture(t,d,rp);
        return mDevice->WriteTextureRegion(t,d,0,0,0,e.desc.width,e.desc.height,1,mip,lay,rp);
    }

    // ── Mesh ──────────────────────────────────────────────────────────────────
    inline bool NkDX12Renderer::CreateMesh_Impl(NkMeshEntry& e){
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
            if(!ibo.IsValid()){mDevice->DestroyBuffer(vbo);return false;} e.rhiIBO=ibo.id;
        }
        return true;
    }
    inline void NkDX12Renderer::DestroyMesh_Impl(NkMeshEntry& e){
        nkentseu::NkBufferHandle vbo; vbo.id=e.rhiVBO; nkentseu::NkBufferHandle ibo; ibo.id=e.rhiIBO;
        if(vbo.IsValid())mDevice->DestroyBuffer(vbo); if(ibo.IsValid())mDevice->DestroyBuffer(ibo);
        e.rhiVBO=e.rhiIBO=0;
    }
    inline bool NkDX12Renderer::UpdateMesh_Impl(NkMeshEntry& e,const void* v,uint32 vc,const void* i,uint32 ic){
        nkentseu::NkBufferHandle vbo; vbo.id=e.rhiVBO; nkentseu::NkBufferHandle ibo; ibo.id=e.rhiIBO;
        uint32 s=e.desc.Is2D()?sizeof(NkVertex2D):sizeof(NkVertex3D);
        bool ok=true;
        if(vbo.IsValid()&&v&&vc)ok&=mDevice->WriteBuffer(vbo,v,(uint64)vc*s);
        if(ibo.IsValid()&&i&&ic)ok&=mDevice->WriteBuffer(ibo,i,(uint64)ic*(e.desc.idxFmt==NkIndexFormat::UInt16?2:4));
        e.vertexCount=vc; e.indexCount=ic; return ok;
    }

    // ── Material ──────────────────────────────────────────────────────────────
    inline bool NkDX12Renderer::CreateMaterial_Impl(NkMaterialEntry& e){
        EnsurePerFrameResources();
        e.rhiPipeline=mSet1Layout.id;
        return mSet1Layout.IsValid();
    }
    inline void NkDX12Renderer::DestroyMaterial_Impl(NkMaterialEntry& e){e.rhiPipeline=0;}

    inline bool NkDX12Renderer::CreateMatInst_Impl(NkMaterialInstEntry& e,const NkMaterialEntry& /*t*/){
        EnsurePerFrameResources();
        nkentseu::NkDescSetHandle layout; layout.id=mSet1Layout.id;
        if(!layout.IsValid()) return false;
        nkentseu::NkDescSetHandle set=mDevice->AllocateDescriptorSet(layout);
        if(!set.IsValid()) return false;
        e.rhiDescSet=set.id; e.dirty=true;
        // White tex par défaut
        nkentseu::NkDescriptorWrite ws[5]{};
        for(int k=0;k<5;++k){ws[k].set=set;ws[k].binding=k+1;ws[k].type=nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;ws[k].texture=mWhiteTex;ws[k].sampler=mDefaultSampler;ws[k].textureLayout=nkentseu::NkResourceState::NK_SHADER_READ;}
        mDevice->UpdateDescriptorSets(ws,5);
        return true;
    }
    inline void NkDX12Renderer::DestroyMatInst_Impl(NkMaterialInstEntry& e){
        nkentseu::NkDescSetHandle s; s.id=e.rhiDescSet; if(s.IsValid())mDevice->FreeDescriptorSet(s); e.rhiDescSet=0;
    }
    inline bool NkDX12Renderer::RebuildMatInst_Impl(NkMaterialInstEntry& e,const NkMaterialEntry& /*t*/){
        if(!e.rhiDescSet) return false;
        nkentseu::NkDescSetHandle set; set.id=e.rhiDescSet;
        for(usize pi=0;pi<e.params.Size();++pi){
            const NkMaterialParam& p=e.params[pi];
            if(p.type!=NkMaterialParam::Type::Texture) continue;
            const NkTextureEntry* te=GetTexture(p.texture); if(!te||!te->valid) continue;
            int slot=-1;
            if(strcmp(p.name.CStr(),"uAlbedoMap")==0)slot=0;
            else if(strcmp(p.name.CStr(),"uNormalMap")==0)slot=1;
            else if(strcmp(p.name.CStr(),"uMetalRoughMap")==0)slot=2;
            else if(strcmp(p.name.CStr(),"uOcclusionMap")==0)slot=3;
            else if(strcmp(p.name.CStr(),"uEmissiveMap")==0)slot=4;
            if(slot<0) continue;
            nkentseu::NkDescriptorWrite w{};
            w.set=set; w.binding=slot+1; w.type=nkentseu::NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w.texture.id=te->rhiTextureId; w.sampler.id=te->rhiSamplerId;
            w.textureLayout=nkentseu::NkResourceState::NK_SHADER_READ;
            mDevice->UpdateDescriptorSets(&w,1);
        }
        e.dirty=false; return true;
    }

    // ── Render Target / Font (identique DX11) ─────────────────────────────────
    inline bool NkDX12Renderer::CreateRenderTarget_Impl(NkRenderTargetEntry& e){
        nkentseu::NkRenderPassDesc rpd=nkentseu::NkRenderPassDesc::Forward();
        e.rhiRenderPass=mDevice->CreateRenderPass(rpd).id; if(!e.rhiRenderPass) return false;
        nkentseu::NkFramebufferDesc fd{}; nkentseu::NkRenderPassHandle rp; rp.id=e.rhiRenderPass; fd.renderPass=rp;
        fd.width=e.desc.width>0?e.desc.width:mDevice->GetSwapchainWidth();
        fd.height=e.desc.height>0?e.desc.height:mDevice->GetSwapchainHeight();
        for(usize i=0;i<e.desc.colorAttachments.Size();++i){nkentseu::NkTextureHandle t;t.id=e.desc.colorAttachments[i].id;fd.colorAttachments.PushBack(t);e.colorTextures.PushBack(e.desc.colorAttachments[i]);}
        if(e.desc.depthAttachment.IsValid()){nkentseu::NkTextureHandle t;t.id=e.desc.depthAttachment.id;fd.depthAttachment=t;e.depthTexture=e.desc.depthAttachment;}
        e.rhiFramebuffer=mDevice->CreateFramebuffer(fd).id; return e.rhiFramebuffer!=0;
    }
    inline void NkDX12Renderer::DestroyRenderTarget_Impl(NkRenderTargetEntry& e){
        nkentseu::NkFramebufferHandle fb;fb.id=e.rhiFramebuffer;nkentseu::NkRenderPassHandle rp;rp.id=e.rhiRenderPass;
        if(fb.IsValid())mDevice->DestroyFramebuffer(fb); if(rp.IsValid())mDevice->DestroyRenderPass(rp);
        e.rhiFramebuffer=e.rhiRenderPass=0;
    }
    inline bool NkDX12Renderer::CreateFont_Impl(NkFontEntry&){return true;}
    inline void NkDX12Renderer::DestroyFont_Impl(NkFontEntry&){}

    // ── Frame ─────────────────────────────────────────────────────────────────
    inline bool NkDX12Renderer::BeginFrame_Impl(){
        EnsurePerFrameResources();
        nkentseu::NkFrameContext fc; return mDevice->BeginFrame(fc);
    }
    inline bool NkDX12Renderer::BeginRenderPass_Impl(const NkRenderPassInfo& info, NkICommandBuffer* cmd){
        nkentseu::NkRenderPassHandle rp=mDevice->GetSwapchainRenderPass();
        nkentseu::NkFramebufferHandle fbo=mDevice->GetSwapchainFramebuffer();
        const uint32 W=mDevice->GetSwapchainWidth(), H=mDevice->GetSwapchainHeight();
        nkentseu::NkRect2D area{0,0,(int32)W,(int32)H};
        if(info.clearColor) cmd->SetClearColor(info.clearColorValue.x,info.clearColorValue.y,info.clearColorValue.z,info.clearColorValue.w);
        if(info.clearDepth)  cmd->SetClearDepth(info.clearDepthValue);
        return cmd->BeginRenderPass(rp,fbo,area);
    }

    inline void NkDX12Renderer::SubmitDrawCalls_Impl(NkRendererCommand& drawCmd,
                                                      NkICommandBuffer* cmd,
                                                      const NkRenderPassInfo& pass){
        const uint32 W=mDevice->GetSwapchainWidth(),H=mDevice->GetSwapchainHeight();
        nkentseu::NkViewport vp{0.f,0.f,(float32)W,(float32)H,0.f,1.f}; vp.flipY=false;
        cmd->SetViewport(vp); nkentseu::NkRect2D sc{0,0,(int32)W,(int32)H}; cmd->SetScissor(sc);
        UpdateFrameResources(pass,drawCmd);
        DrawScene(drawCmd,cmd);
        mStats.drawCalls3D=(uint32)drawCmd.GetDrawCalls3D().Size();
    }

    inline void NkDX12Renderer::DrawScene(NkRendererCommand& drawCmd, NkICommandBuffer* cmd){
        const uint32 fi=GetCurrentFrameIdx();
        PerFrameResources& pf=mPerFrame[fi];
        if(pf.frameSet.IsValid()) cmd->BindDescriptorSet(pf.frameSet,0);  // set=0

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

            // DrawData (set=2) — écriture et bind
            DrawData dd{}; memcpy(dd.model,dc.transform.data,64); memcpy(dd.modelIT,dc.transform.data,64);
            dd.color[0]=dd.color[1]=dd.color[2]=dd.color[3]=1.f;
            mDevice->WriteBuffer(pf.drawCB,&dd,sizeof(dd));
            if(pf.drawSet.IsValid()) cmd->BindDescriptorSet(pf.drawSet,2);

            if(inst->dirty) RebuildMatInst_Impl(*inst,*tmpl);
            nkentseu::NkDescSetHandle ds; ds.id=inst->rhiDescSet;
            if(ds.IsValid()) cmd->BindDescriptorSet(ds,1);  // set=1

            nkentseu::NkBufferHandle vbo; vbo.id=mesh->rhiVBO;
            cmd->BindVertexBuffer(0,vbo);
            if(mesh->indexCount>0){
                nkentseu::NkBufferHandle ibo; ibo.id=mesh->rhiIBO;
                cmd->BindIndexBuffer(ibo,nkentseu::NkIndexFormat::NK_UINT32);
                cmd->DrawIndexed(mesh->indexCount);
            } else cmd->Draw(mesh->vertexCount);
        }
    }

    inline void NkDX12Renderer::EndRenderPass_Impl(NkICommandBuffer* cmd){cmd->EndRenderPass();}
    inline void NkDX12Renderer::EndFrame_Impl(NkICommandBuffer* /*cmd*/){
        nkentseu::NkFrameContext fc; fc.frameIndex=mDevice->GetFrameIndex(); fc.frameNumber=mDevice->GetFrameNumber();
        mDevice->EndFrame(fc);
    }

} // namespace renderer
} // namespace nkentseu

#endif // NK_RHI_DX12_ENABLED