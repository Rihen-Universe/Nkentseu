// =============================================================================
// NkMaterialSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkMaterialSystem.h"

namespace nkentseu {
namespace renderer {
    static const char* GetShaderFolder(NkMaterialType t) {
    switch(t) {
        case NkMaterialType::NK_PBR_METALLIC:  return "PBR";
        case NkMaterialType::NK_TOON:          return "Toon";
        case NkMaterialType::NK_TOON_INK:      return "ToonInk";
        case NkMaterialType::NK_ANIME:         return "Anime";
        case NkMaterialType::NK_SKIN:          return "Skin";
        case NkMaterialType::NK_HAIR:          return "Hair";
        case NkMaterialType::NK_GLASS:         return "Glass";
        case NkMaterialType::NK_CLOTH:         return "Cloth";
        case NkMaterialType::NK_CAR_PAINT:     return "CarPaint";
        case NkMaterialType::NK_FOLIAGE:       return "Foliage";
        case NkMaterialType::NK_WATER:         return "Water";
        case NkMaterialType::NK_TERRAIN:       return "Terrain";
        case NkMaterialType::NK_EMISSIVE:      return "Emissive";
        case NkMaterialType::NK_VOLUME:        return "Volume";
        case NkMaterialType::NK_UNLIT:         return "Unlit";
        default:                               return "PBR";
    }
}

    NkMaterialSystem::~NkMaterialSystem() { Shutdown(); }

    bool NkMaterialSystem::Init(NkIDevice* device, NkTextureLibrary* texLib) {
        mDevice = device; mTexLib = texLib;
        RegisterBuiltins();
        return true;
    }

    void NkMaterialSystem::Shutdown() {
        for (auto* inst : mInstances) delete inst;
        mInstances.Clear();
        mTemplates.Clear();
    }

    // ── Enregistrement ───────────────────────────────────────────────────────
    NkMatHandle NkMaterialSystem::RegisterTemplate(const NkMaterialTemplateDesc& desc) {
        TemplateEntry e;
        e.desc     = desc;
        e.compiled = false;
        NkMatHandle h{mNextId++};
        mTemplates.Insert(h.id, e);
        return h;
    }

    NkMatHandle NkMaterialSystem::FindTemplate(const NkString& name) const {
        for (auto& [id, e] : mTemplates) {
            if (e.desc.name == name) return NkMatHandle{id};
        }
        return NkMatHandle::Null();
    }

    // ── Built-ins ─────────────────────────────────────────────────────────────
    void NkMaterialSystem::RegisterBuiltins() {
        auto reg = [this](NkMaterialType t, const char* name,
                           NkRenderQueue q = NkRenderQueue::NK_OPAQUE) {
            NkMaterialTemplateDesc d;
            d.type=t; d.name=name; d.queue=q;
            return RegisterTemplate(d);
        };
        mTmplPBR     = reg(NkMaterialType::NK_PBR_METALLIC, "Default_PBR");
        mTmplToon    = reg(NkMaterialType::NK_TOON,         "Default_Toon");
        mTmplUnlit   = reg(NkMaterialType::NK_UNLIT,        "Default_Unlit");
        mTmplWire    = reg(NkMaterialType::NK_WIREFRAME_MAT,"Default_Wireframe");
        mTmplSkin    = reg(NkMaterialType::NK_SKIN,         "Default_Skin");
        mTmplHair    = reg(NkMaterialType::NK_HAIR,         "Default_Hair",
                            NkRenderQueue::NK_ALPHA_TEST);
        mTmplAnime   = reg(NkMaterialType::NK_ANIME,        "Default_Anime");
        mTmplArchviz = reg(NkMaterialType::NK_ARCHIVIZ,     "Default_Archviz");
    }

    // ── Instance ─────────────────────────────────────────────────────────────
    NkMaterialInstance* NkMaterialSystem::CreateInstance(NkMatHandle tmpl) {
        auto* inst    = new NkMaterialInstance();
        inst->mTemplate = tmpl;
        inst->mDirty    = true;
        // Init PBR defaults
        inst->mPBR.albedo    = {1,1,1,1};
        inst->mPBR.metallic  = 0.f;
        inst->mPBR.roughness = 0.5f;
        inst->mPBR.ao        = 1.f;
        mInstances.PushBack(inst);
        return inst;
    }

    void NkMaterialSystem::DestroyInstance(NkMaterialInstance*& inst) {
        if (!inst) return;
        for (uint32 i=0;i<(uint32)mInstances.Size();i++){
            if (mInstances[i]==inst){
                delete inst; mInstances.RemoveAt(i); break;
            }
        }
        inst=nullptr;
    }

    // ── Bind ─────────────────────────────────────────────────────────────────
    bool NkMaterialSystem::BindInstance(NkICommandBuffer* cmd,
                                          NkMaterialInstance* inst,
                                          NkTextureLibrary* texLib) {
        if (!inst) return false;
        auto* tmplEntry = mTemplates.Find(inst->mTemplate.id);
        if (!tmplEntry) return false;

        // Compiler pipeline si nécessaire
        if (!tmplEntry->compiled) {
            tmplEntry->pipeline = CompilePipeline(*tmplEntry);
            tmplEntry->compiled = true;
        }
        cmd->BindPipeline(tmplEntry->pipeline);

        if (inst->mDirty) {
            // Upload PBR/Toon uniforms
            cmd->UpdateUniformBuffer(1, &inst->mPBR, sizeof(NkPBRParams));
            // Bind textures (fallback sur built-ins si non setté)
            auto GetTex = [&](const NkString& name) -> NkTexHandle {
                for (auto& p : inst->mParams)
                    if (p.kind==NkMaterialInstance::Param::Kind::TEX && p.name==name)
                        return p.tex;
                return texLib->GetWhite1x1();
            };
            cmd->BindTexture(3,  texLib->GetRHIHandle(GetTex("albedo")));
            cmd->BindTexture(4,  texLib->GetRHIHandle(GetTex("normal")));
            cmd->BindTexture(5,  texLib->GetRHIHandle(GetTex("orm")));
            cmd->BindTexture(6,  texLib->GetRHIHandle(GetTex("emissive")));
            inst->MarkClean();
        }
        return true;
    }

    NkPipelineHandle NkMaterialSystem::CompilePipeline(const TemplateEntry& t) {
        // TODO: appel au ShaderCompiler + NkIDevice::CreatePipeline
        // Placeholder — retourne handle vide que le backend gère
        NkPipelineDesc pd;
        pd.name = t.desc.name;
        pd.type = t.desc.type == NkMaterialType::NK_CUSTOM ? NkPipelineType::NK_CUSTOM_SHADER : NkPipelineType::NK_BUILTIN;
        pd.materialType = (uint16)t.desc.type;
        pd.blendMode    = (uint8)t.desc.blendMode;
        pd.cullMode     = (uint8)t.desc.cullMode;
        pd.fillMode     = (uint8)t.desc.fillMode;
        pd.depthWrite   = t.desc.depthWrite;
        pd.depthTest    = t.desc.depthTest;
        return mDevice->CreatePipeline(pd);
    }

    void NkMaterialSystem::FlushCompilations() {
        for (auto& [id, e] : mTemplates) {
            if (!e.compiled) {
                e.pipeline = CompilePipeline(e);
                e.compiled = true;
            }
        }
    }

    // ── NkMaterialInstance setters ────────────────────────────────────────────
    NkMaterialInstance* NkMaterialInstance::SetAlbedo(NkVec3f c, float32 a) {
        mPBR.albedo={c.x,c.y,c.z,a}; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetMetallic(float32 v) {
        mPBR.metallic=v; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetRoughness(float32 v) {
        mPBR.roughness=v; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetEmissive(NkVec3f c, float32 str) {
        mPBR.emissive={c.x,c.y,c.z,1}; mPBR.emissiveStrength=str; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetSubsurface(float32 v, NkVec3f c) {
        mPBR.subsurface=v; mPBR.subsurfaceColor={c.x,c.y,c.z,1}; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetClearcoat(float32 v, float32 r) {
        mPBR.clearcoat=v; mPBR.clearcoatRough=r; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetToonThreshold(float32 v) {
        mToon.shadowThreshold=v; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetToonShadowColor(NkVec3f c) {
        mToon.shadowColor={c.x,c.y,c.z,1}; mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetOutline(float32 w, NkVec3f c) {
        mToon.outlineWidth=w; mToon.outlineColor={c.x,c.y,c.z,1}; mDirty=true; return this;
    }

    NkMaterialInstance* NkMaterialInstance::SetTexture(const NkString& n, NkTexHandle t) {
        for (auto& p:mParams) if(p.name==n&&p.kind==Param::Kind::TEX){p.tex=t;mDirty=true;return this;}
        Param p; p.name=n; p.kind=Param::Kind::TEX; p.tex=t;
        mParams.PushBack(p); mDirty=true; return this;
    }
    NkMaterialInstance* NkMaterialInstance::SetAlbedoMap  (NkTexHandle t){return SetTexture("albedo",t);}
    NkMaterialInstance* NkMaterialInstance::SetNormalMap  (NkTexHandle t,float32 s){mPBR.normalStrength=s;return SetTexture("normal",t);}
    NkMaterialInstance* NkMaterialInstance::SetORMMap     (NkTexHandle t){return SetTexture("orm",t);}
    NkMaterialInstance* NkMaterialInstance::SetEmissiveMap(NkTexHandle t){return SetTexture("emissive",t);}
    NkMaterialInstance* NkMaterialInstance::SetAOMap      (NkTexHandle t){return SetTexture("ao",t);}

    NkMaterialInstance* NkMaterialInstance::SetFloat(const NkString& n,float32 v){
        Param p;p.name=n;p.kind=Param::Kind::F;p.f=v;mParams.PushBack(p);mDirty=true;return this;}
    NkMaterialInstance* NkMaterialInstance::SetVec3(const NkString& n,NkVec3f v){
        Param p;p.name=n;p.kind=Param::Kind::V3;p.v3=v;mParams.PushBack(p);mDirty=true;return this;}
    NkMaterialInstance* NkMaterialInstance::SetVec4(const NkString& n,NkVec4f v){
        Param p;p.name=n;p.kind=Param::Kind::V4;p.v4=v;mParams.PushBack(p);mDirty=true;return this;}
    NkMaterialInstance* NkMaterialInstance::SetVec2(const NkString& n,NkVec2f v){
        Param p;p.name=n;p.kind=Param::Kind::V2;p.v2=v;mParams.PushBack(p);mDirty=true;return this;}
    NkMaterialInstance* NkMaterialInstance::SetInt(const NkString& n,int32 v){
        Param p;p.name=n;p.kind=Param::Kind::I;p.i=v;mParams.PushBack(p);mDirty=true;return this;}
    NkMaterialInstance* NkMaterialInstance::SetBool(const NkString& n,bool v){
        Param p;p.name=n;p.kind=Param::Kind::B;p.b=v;mParams.PushBack(p);mDirty=true;return this;}
    NkMaterialInstance* NkMaterialInstance::SetColor(const NkString& n,NkVec4f c){return SetVec4(n,c);}

    NkRenderQueue NkMaterialInstance::GetQueue() const {
        // TODO: lookup du template
        return NkRenderQueue::NK_OPAQUE;
    }

} // namespace renderer
} // namespace nkentseu
