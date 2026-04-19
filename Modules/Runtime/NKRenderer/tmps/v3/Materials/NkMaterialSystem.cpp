// =============================================================================
// NkMaterialSystem.cpp
// =============================================================================
#include "NkMaterialSystem.h"
#include "../Shader/NkShaderCompiler.h"
#include "NKLogger/NkLog.h"
#include <cstring>

// Inclure les sources GLSL/HLSL/MSL inline (depuis les fichiers .h de bibliothèque)
// Les shaders sont définis dans Materials/Shaders/ et chargés ici

namespace nkentseu {

    // =========================================================================
    // Sources GLSL inline minimales pour chaque type de matériau
    // En production : charger depuis fichiers ou générer via NkShaderCompiler
    // =========================================================================

    // Shared vertex shader (standard layout)
    static const char* kStdVert_GL = R"GLSL(
#version 460 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aTangent;
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;
layout(binding=0,std140) uniform NkFrame {
    mat4 nk_View,nk_Projection,nk_ViewProjection,nk_InvVP,nk_PrevVP;
    vec4 nk_CamPos,nk_CamDir,nk_Viewport,nk_Time;
    vec4 nk_SunDir,nk_SunColor;
    mat4 nk_Shadow[4]; vec4 nk_CascadeSplits;
    float nk_EnvInt,nk_EnvRot; uint nk_LightCnt,nk_Frame;
};
layout(binding=1,std140) uniform NkObject { mat4 model; vec4 customColor; uint matId; uint p[3]; };
out vec3 vWorldPos; out vec3 vNormal; out vec4 vTangent; out vec2 vUV0,vUV1; out vec3 vViewDir; out vec4 vShadow[4];
void main(){
    vec4 wp=model*vec4(aPosition,1.0); vWorldPos=wp.xyz;
    vUV0=aUV0; vUV1=aUV1; vViewDir=normalize(nk_CamPos.xyz-wp.xyz);
    mat3 NM=transpose(inverse(mat3(model)));
    vNormal=normalize(NM*aNormal); vTangent=vec4(normalize(NM*aTangent.xyz),aTangent.w);
    for(int i=0;i<4;i++) vShadow[i]=nk_Shadow[i]*wp;
    gl_Position=nk_ViewProjection*wp;
}
)GLSL";

    static const char* kStdVert_VK = R"GLSL(
#version 460
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aTangent;
layout(location=3) in vec2 aUV0;
layout(location=4) in vec2 aUV1;
layout(set=0,binding=0,std140) uniform NkFrame {
    mat4 nk_View,nk_Projection,nk_ViewProjection,nk_InvVP,nk_PrevVP;
    vec4 nk_CamPos,nk_CamDir,nk_Viewport,nk_Time;
    vec4 nk_SunDir,nk_SunColor;
    mat4 nk_Shadow[4]; vec4 nk_CascadeSplits;
    float nk_EnvInt,nk_EnvRot; uint nk_LightCnt,nk_Frame;
};
layout(push_constant) uniform PC { mat4 model; vec4 customColor; uint matId; uint p[3]; };
layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec4 vTangent;
layout(location=3) out vec2 vUV0;
layout(location=4) out vec2 vUV1;
layout(location=5) out vec3 vViewDir;
layout(location=6) out vec4 vShadow[4];
void main(){
    vec4 wp=model*vec4(aPosition,1.0); vWorldPos=wp.xyz;
    vUV0=aUV0; vUV1=aUV1; vViewDir=normalize(nk_CamPos.xyz-wp.xyz);
    mat3 NM=transpose(inverse(mat3(model)));
    vNormal=normalize(NM*aNormal); vTangent=vec4(normalize(NM*aTangent.xyz),aTangent.w);
    for(int i=0;i<4;i++) vShadow[i]=nk_Shadow[i]*wp;
    gl_Position=nk_ViewProjection*wp;
}
)GLSL";

    // =========================================================================
    // NkMaterialSystem
    // =========================================================================
    NkMaterialSystem::NkMaterialSystem(NkIDevice* device, NkTextureLibrary* texLib)
        : mDevice(device), mTexLib(texLib), mMSAA(NkSampleCount::NK_S1) {}

    NkMaterialSystem::~NkMaterialSystem() { Shutdown(); }

    bool NkMaterialSystem::Initialize(NkRenderPassHandle fwd,
                                       NkRenderPassHandle def,
                                       NkSampleCount msaa) {
        mForwardPass  = fwd;
        mDeferredPass = def;
        mMSAA         = msaa;
        logger.Infof("[NkMaterialSystem] Initialized\n");
        return true;
    }

    void NkMaterialSystem::Shutdown() {
        ReleaseAll();
        for(auto& kv:mPipelineCache) if(kv.Second.IsValid()) mDevice->DestroyPipeline(kv.Second);
        for(auto& kv:mShaderCache)   if(kv.Second.IsValid()) mDevice->DestroyShader(kv.Second);
        for(auto& kv:mLayoutCache)   if(kv.Second.IsValid()) mDevice->DestroyDescriptorSetLayout(kv.Second);
        mPipelineCache.Clear(); mShaderCache.Clear(); mLayoutCache.Clear();
    }

    NkMatId NkMaterialSystem::AllocId() { return mNextId++; }

    // =========================================================================
    // Création
    // =========================================================================
    NkMatId NkMaterialSystem::Create(NkMaterialType type, const NkString& name) {
        NkMaterial mat;
        mat.id    = AllocId();
        mat.type  = type;
        mat.name  = name.Empty() ? NkString("mat_") + NkString::FromInt((int32)mat.id) : name;
        mat.dirty = true;
        mMaterials[mat.id] = mat;
        return mat.id;
    }

    NkMatId NkMaterialSystem::Clone(NkMatId src, const NkString& newName) {
        auto it = mMaterials.Find(src);
        if (!it) return NK_INVALID_MAT;
        NkMaterial mat = it->Second;
        mat.id    = AllocId();
        mat.name  = newName.Empty() ? mat.name + "_copy" : newName;
        mat.dirty = true;
        mat.uniformBuffer = {};
        mat.descriptorSet = {};
        mat.pipeline      = {};
        mMaterials[mat.id] = mat;
        return mat.id;
    }

    // =========================================================================
    // Préréglages
    // =========================================================================
    NkMatId NkMaterialSystem::CreateDefault() {
        NkMatId id = Create(NkMaterialType::NK_PBR_METALLIC,"default");
        auto* m = Get(id);
        m->pbr.albedo   = {0.8f,0.8f,0.8f,1.f};
        m->pbr.metallic  = 0.f; m->pbr.roughness = 0.5f;
        m->textures.albedo  = mTexLib->GetWhite();
        m->textures.normal  = mTexLib->GetNormal();
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateUnlit(math::NkVec4 color) {
        NkMatId id = Create(NkMaterialType::NK_UNLIT,"unlit");
        auto* m = Get(id);
        m->pbr.albedo = color;
        m->textures.albedo = mTexLib->GetWhite();
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreatePBR(NkTexId albedo, NkTexId normalMap,
                                         float32 metallic, float32 roughness) {
        NkMatId id = Create(NkMaterialType::NK_PBR_METALLIC,"pbr");
        auto* m = Get(id);
        m->textures.albedo = albedo ? albedo : mTexLib->GetWhite();
        m->textures.normal = normalMap ? normalMap : mTexLib->GetNormal();
        m->pbr.metallic = metallic; m->pbr.roughness = roughness;
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateToon(math::NkVec4 light, math::NkVec4 shadow, uint32 steps) {
        NkMatId id = Create(NkMaterialType::NK_TOON,"toon");
        auto* m = Get(id);
        m->toon.colorLight  = light;
        m->toon.colorShadow = shadow;
        m->toon.steps       = (float32)steps;
        m->textures.albedo  = mTexLib->GetWhite();
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateToonInk(math::NkVec4 base, float32 outlineW) {
        NkMatId id = Create(NkMaterialType::NK_TOON_INK,"toon_ink");
        auto* m = Get(id);
        m->toon.colorLight  = base;
        m->toon.outlineWidth= outlineW;
        m->textures.albedo  = mTexLib->GetWhite();
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateAnime(NkTexId albedo, math::NkVec4 shadowColor,
                                           float32 rimStrength) {
        NkMatId id = Create(NkMaterialType::NK_ANIME,"anime");
        auto* m = Get(id);
        m->textures.albedo  = albedo ? albedo : mTexLib->GetWhite();
        m->toon.colorShadow = shadowColor;
        m->toon.rimStrength = rimStrength;
        m->toon.steps = 2.f;
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateArchviz(NkTexId albedo, NkTexId normal, NkTexId ao) {
        NkMatId id = Create(NkMaterialType::NK_ARCHVIZ,"archviz");
        auto* m = Get(id);
        m->textures.albedo  = albedo ? albedo : mTexLib->GetWhite();
        m->textures.normal  = normal ? normal : mTexLib->GetNormal();
        m->textures.ao      = ao ? ao : mTexLib->GetWhite();
        m->pbr.roughness    = 0.7f;
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateSkin(NkTexId albedo, NkTexId sssMap, float32 sssR) {
        NkMatId id = Create(NkMaterialType::NK_SKIN,"skin");
        auto* m = Get(id);
        m->textures.albedo  = albedo ? albedo : mTexLib->GetWhite();
        m->textures.sssMap  = sssMap;
        m->pbr.subsurface   = 0.5f;
        m->pbr.sssRadius    = sssR;
        m->pbr.roughness    = 0.6f;
        m->pbr.subsurfaceColor = {0.9f,0.5f,0.4f};
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateGlass(float32 ior, float32 roughness) {
        NkMatId id = Create(NkMaterialType::NK_GLASS,"glass");
        auto* m = Get(id);
        m->pbr.ior          = ior;
        m->pbr.roughness    = roughness;
        m->pbr.transmission = 1.f;
        m->pbr.metallic     = 0.f;
        m->alphaBlend       = true;
        m->doubleSided      = true;
        m->textures.albedo  = mTexLib->GetWhite();
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateWater(float32 waveHeight) {
        NkMatId id = Create(NkMaterialType::NK_WATER,"water");
        auto* m = Get(id);
        m->water.waveHeight = waveHeight;
        m->alphaBlend       = true;
        m->doubleSided      = false;
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateTerrain(NkTexId splatmap,NkTexId l0,NkTexId l1,
                                              NkTexId l2,NkTexId l3) {
        NkMatId id = Create(NkMaterialType::NK_TERRAIN,"terrain");
        auto* m = Get(id);
        m->textures.splatmap= splatmap;
        m->textures.layer1  = l0; m->textures.layer2=l1;
        m->textures.layer3  = l2; m->textures.layer4=l3;
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateEmissive(math::NkVec3 color, float32 strength) {
        NkMatId id = Create(NkMaterialType::NK_EMISSIVE,"emissive");
        auto* m = Get(id);
        m->pbr.emissive = {color.x,color.y,color.z,1.f};
        m->pbr.emissiveStrength = strength;
        m->textures.albedo = mTexLib->GetBlack();
        MarkDirty(id);
        return id;
    }

    NkMatId NkMaterialSystem::CreateCarPaint(math::NkVec3 color, bool metallic) {
        NkMatId id = Create(NkMaterialType::NK_CAR_PAINT,"carpaint");
        auto* m = Get(id);
        m->pbr.albedo    = {color.x,color.y,color.z,1.f};
        m->pbr.metallic  = metallic ? 0.8f : 0.f;
        m->pbr.roughness = 0.2f;
        m->pbr.clearcoat = 1.f;
        m->pbr.clearcoatRough = 0.1f;
        m->textures.albedo = mTexLib->GetWhite();
        MarkDirty(id);
        return id;
    }

    // =========================================================================
    // Accès
    // =========================================================================
    NkMaterial*       NkMaterialSystem::Get(NkMatId id) {
        auto it=mMaterials.Find(id); return it?&it->Second:nullptr;
    }
    const NkMaterial* NkMaterialSystem::Get(NkMatId id) const {
        auto it=mMaterials.Find(id); return it?&it->Second:nullptr;
    }
    bool NkMaterialSystem::IsValid(NkMatId id) const { return mMaterials.Find(id)!=nullptr; }
    void NkMaterialSystem::MarkDirty(NkMatId id) {
        auto it=mMaterials.Find(id); if(it) it->Second.dirty=true;
    }

    // =========================================================================
    // GPU upload
    // =========================================================================
    void NkMaterialSystem::UploadMaterialUniforms(NkMaterial& mat) {
        // Déterminer la taille des données selon le type
        uint64 dataSize = sizeof(NkPBRParams);
        const void* data = &mat.pbr;
        NkVector<uint8> toonBlob;
        if(mat.type==NkMaterialType::NK_TOON||mat.type==NkMaterialType::NK_TOON_INK||
           mat.type==NkMaterialType::NK_ANIME){
            dataSize=sizeof(NkToonParams); data=&mat.toon;
        } else if(mat.type==NkMaterialType::NK_TERRAIN){
            dataSize=sizeof(NkTerrainParams); data=&mat.terrain;
        } else if(mat.type==NkMaterialType::NK_WATER){
            dataSize=sizeof(NkWaterParams); data=&mat.water;
        }

        // Créer ou mettre à jour le UBO
        if(!mat.uniformBuffer.IsValid()){
            NkBufferDesc bd=NkBufferDesc::Uniform(dataSize);
            mat.uniformBuffer=mDevice->CreateBuffer(bd);
        }
        if(mat.uniformBuffer.IsValid())
            mDevice->WriteBuffer(mat.uniformBuffer,data,dataSize);
    }

    void NkMaterialSystem::CreateDescriptorSet(NkMaterial& mat) {
        // Layout selon le type de matériau
        NkDescriptorSetLayoutDesc layoutDesc;
        layoutDesc.Add(0,NkDescriptorType::NK_UNIFORM_BUFFER,NkShaderStage::NK_ALL_GRAPHICS);
        // Textures
        layoutDesc.Add(1,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(2,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(3,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(4,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
        layoutDesc.Add(5,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);

        uint32 key=(uint32)mat.type;
        NkDescSetHandle layout;
        auto lit=mLayoutCache.Find(key);
        if(lit){ layout=lit->Second; }
        else { layout=mDevice->CreateDescriptorSetLayout(layoutDesc); mLayoutCache[key]=layout; }

        mat.descriptorSet=mDevice->AllocateDescriptorSet(layout);
        if(!mat.descriptorSet.IsValid()) return;

        // Ecrire les descriptors
        NkDescriptorWrite writes[6]={};
        writes[0]={mat.descriptorSet,0,0,NkDescriptorType::NK_UNIFORM_BUFFER};
        writes[0].buffer=mat.uniformBuffer; writes[0].bufferRange=sizeof(NkPBRParams);

        auto BindTex=[&](uint32 idx,NkTexId texId){
            NkTexId tid=texId?texId:mTexLib->GetWhite();
            writes[idx]={mat.descriptorSet,idx,0,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER};
            writes[idx].texture=mTexLib->GetHandle(tid);
            writes[idx].sampler=mTexLib->GetSampler(tid);
            writes[idx].textureLayout=NkResourceState::NK_SHADER_READ;
        };
        BindTex(1,mat.textures.albedo);
        BindTex(2,mat.textures.normal);
        BindTex(3,mat.textures.metalRough);
        BindTex(4,mat.textures.ao);
        BindTex(5,mat.textures.emissive);
        mDevice->UpdateDescriptorSets(writes,6);
    }

    bool NkMaterialSystem::CreatePipelineForMaterial(NkMaterial& mat) {
        if(!mForwardPass.IsValid()) return false;
        NkShaderHandle sh=GetShaderForType(mat.type);
        if(!sh.IsValid()){
            logger.Warnf("[NkMaterialSystem] No shader for type %d\n",(int)mat.type);
            return false;
        }

        NkVertexLayout layout;
        layout.AddAttribute(0,0,NkGPUFormat::NK_RGB32_FLOAT,0,"POSITION",0);
        layout.AddAttribute(1,0,NkGPUFormat::NK_RGB32_FLOAT,12,"NORMAL",0);
        layout.AddAttribute(2,0,NkGPUFormat::NK_RGBA32_FLOAT,24,"TANGENT",0);
        layout.AddAttribute(3,0,NkGPUFormat::NK_RG32_FLOAT,40,"TEXCOORD",0);
        layout.AddAttribute(4,0,NkGPUFormat::NK_RG32_FLOAT,48,"TEXCOORD",1);
        layout.AddBinding(0,56,false);

        NkDescSetHandle matLayout;
        auto lit=mLayoutCache.Find((uint32)mat.type);
        if(lit) matLayout=lit->Second;

        NkGraphicsPipelineDesc pd;
        pd.shader=sh; pd.vertexLayout=layout;
        pd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        pd.rasterizer=mat.doubleSided?NkRasterizerDesc::NoCull():NkRasterizerDesc::Default();
        pd.depthStencil=NkDepthStencilDesc::Default();
        pd.blend=mat.alphaBlend?NkBlendDesc::Alpha():NkBlendDesc::Opaque();
        pd.samples=mMSAA;
        pd.renderPass=mForwardPass;
        pd.debugName=mat.name.CStr();
        if(matLayout.IsValid()) pd.descriptorSetLayouts.PushBack(matLayout);
        pd.pushConstants.PushBack({NkShaderStage::NK_ALL_GRAPHICS,0,sizeof(NkObjectPushConstants)});

        mat.pipeline=mDevice->CreateGraphicsPipeline(pd);
        return mat.pipeline.IsValid();
    }

    void NkMaterialSystem::FlushDirty() {
        for(auto& kv:mMaterials){
            NkMaterial& mat=kv.Second;
            if(!mat.dirty) continue;
            UploadMaterialUniforms(mat);
            if(!mat.descriptorSet.IsValid()) CreateDescriptorSet(mat);
            if(!mat.pipeline.IsValid())      CreatePipelineForMaterial(mat);
            mat.dirty=false;
        }
    }

    void NkMaterialSystem::Bind(NkICommandBuffer* cmd,NkMatId id){
        auto it=mMaterials.Find(id); if(!it) return;
        NkMaterial& m=it->Second;
        if(m.dirty) FlushDirty();
        if(m.pipeline.IsValid())      cmd->BindGraphicsPipeline(m.pipeline);
        if(m.descriptorSet.IsValid()) cmd->BindDescriptorSet(m.descriptorSet,1);
    }

    NkShaderHandle NkMaterialSystem::GetShaderForType(NkMaterialType type) {
        uint32 key=(uint32)type | ((uint32)mDevice->GetApi()<<16);
        auto it=mShaderCache.Find(key);
        if(it) return it->Second;
        // Compiler le shader pour ce type + ce backend
        CompileShaderForType(type);
        it=mShaderCache.Find(key);
        return it?it->Second:NkShaderHandle{};
    }

    bool NkMaterialSystem::CompileShaderForType(NkMaterialType type) {
        NkShaderDesc desc;
        desc.debugName="NkMat_Shader";
        NkGraphicsApi api=mDevice->GetApi();
        bool isVulkan=(api==NkGraphicsApi::NK_API_VULKAN);

        // Vertex shader (identique pour tous les types PBR-like)
        if(isVulkan) desc.AddGLSL(NkShaderStage::NK_VERTEX,kStdVert_VK);
        else         desc.AddGLSL(NkShaderStage::NK_VERTEX,kStdVert_GL);

        // Fragment shader minimal selon le type
        const char* fragSrc=nullptr;
        switch(type){
            case NkMaterialType::NK_UNLIT:
                fragSrc=isVulkan?R"GLSL(
#version 460
layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=3) in vec2 vUV0;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0,std140) uniform MatUBO{vec4 albedo;vec4 em;float metal,rough,ao,emStr;float p[8];};
layout(set=1,binding=1) uniform sampler2D tAlbedo;
layout(push_constant) uniform PC{mat4 m;vec4 cc;uint mid;uint p2[3];};
void main(){fragColor=texture(tAlbedo,vUV0)*albedo*cc;}
)GLSL":R"GLSL(
#version 460 core
in vec3 vWorldPos; in vec3 vNormal; in vec2 vUV0;
layout(location=0) out vec4 fragColor;
layout(binding=2,std140) uniform MatUBO{vec4 albedo;vec4 em;float metal,rough,ao,emStr;float p[8];};
layout(binding=3) uniform sampler2D tAlbedo;
layout(binding=1,std140) uniform ObjUBO{mat4 model;vec4 cc;uint mid;uint p2[3];};
void main(){fragColor=texture(tAlbedo,vUV0)*albedo*cc;}
)GLSL";
                break;
            default:
                // Pour les types complexes (PBR, Toon, etc.), utiliser les fichiers shader
                // compilés. En attendant, fallback unlit.
                fragSrc=isVulkan?R"GLSL(
#version 460
layout(location=0) in vec3 vWorldPos;layout(location=1) in vec3 vNormal;layout(location=3) in vec2 vUV0;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=0,std140) uniform Frame{mat4 V,P,VP,iVP,pVP;vec4 cp,cd,vs,t,sd,sc;mat4 sm[4];vec4 cs;float ei,er;uint lc,fi;};
layout(set=1,binding=0,std140) uniform Mat{vec4 albedo;vec4 em;float metal,rough,ao,emStr;float p[8];};
layout(set=1,binding=1) uniform sampler2D tAlbedo;
layout(push_constant) uniform PC{mat4 m;vec4 cc;uint mid;uint p2[3];};
void main(){
    vec3 N=normalize(vNormal),L=normalize(-sd.xyz);
    float d=max(dot(N,L),0.0)*0.8+0.2;
    fragColor=vec4(texture(tAlbedo,vUV0).rgb*albedo.rgb*cc.rgb*d,albedo.a*cc.a);
}
)GLSL":R"GLSL(
#version 460 core
in vec3 vWorldPos;in vec3 vNormal;in vec2 vUV0;
layout(location=0) out vec4 fragColor;
layout(binding=0,std140) uniform Frame{mat4 V,P,VP,iVP,pVP;vec4 cp,cd,vs,t,sd,sc;mat4 sm[4];vec4 cs;float ei,er;uint lc,fi;};
layout(binding=2,std140) uniform Mat{vec4 albedo;vec4 em;float metal,rough,ao,emStr;float p[8];};
layout(binding=3) uniform sampler2D tAlbedo;
layout(binding=1,std140) uniform Obj{mat4 model;vec4 cc;uint mid;uint p2[3];};
void main(){
    vec3 N=normalize(vNormal),L=normalize(-sd.xyz);
    float d=max(dot(N,L),0.0)*0.8+0.2;
    fragColor=vec4(texture(tAlbedo,vUV0).rgb*albedo.rgb*cc.rgb*d,albedo.a*cc.a);
}
)GLSL";
                break;
        }
        if(fragSrc) desc.AddGLSL(NkShaderStage::NK_FRAGMENT,fragSrc);

        NkShaderHandle sh=mDevice->CreateShader(desc);
        if(!sh.IsValid()){
            logger.Errorf("[NkMaterialSystem] Shader compile failed for type %d\n",(int)type);
            return false;
        }
        uint32 key=(uint32)type|((uint32)mDevice->GetApi()<<16);
        mShaderCache[key]=sh;
        return true;
    }

    uint32 NkMaterialSystem::HashPipelineKey(const NkMaterial& m) const {
        return (uint32)m.type|(m.doubleSided?1<<8:0)|(m.alphaBlend?1<<9:0)|
               (m.alphaMask?1<<10:0)|((uint32)mDevice->GetApi()<<16);
    }

    bool NkMaterialSystem::SaveToJSON(NkMatId,const NkString&) const {return false;}
    NkMatId NkMaterialSystem::LoadFromJSON(const NkString&) {return NK_INVALID_MAT;}

    void NkMaterialSystem::Release(NkMatId id){
        auto it=mMaterials.Find(id);
        if(!it) return;
        NkMaterial& m=it->Second;
        if(m.uniformBuffer.IsValid()) mDevice->DestroyBuffer(m.uniformBuffer);
        if(m.descriptorSet.IsValid()) mDevice->FreeDescriptorSet(m.descriptorSet);
        mMaterials.Erase(id);
    }

    void NkMaterialSystem::ReleaseAll(){
        for(auto& kv:mMaterials){
            if(kv.Second.uniformBuffer.IsValid()) mDevice->DestroyBuffer(kv.Second.uniformBuffer);
            if(kv.Second.descriptorSet.IsValid()) mDevice->FreeDescriptorSet(kv.Second.descriptorSet);
        }
        mMaterials.Clear();
    }

} // namespace nkentseu
