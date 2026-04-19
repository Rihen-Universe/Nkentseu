// =============================================================================
// NkMaterialSystem.cpp
// =============================================================================
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"

namespace nkentseu {
    namespace renderer {

        bool NkMaterialSystem::Init(NkIDevice* device, NkResourceManager* resources) {
            if (!device || !resources) return false;
            mDevice    = device;
            mResources = resources;
            CreateDefaultMaterials();
            return true;
        }

        void NkMaterialSystem::Shutdown() {
            // Détruire toutes les instances
            mInstances.ForEach([](const uint64&, InstanceEntry& e) {
                delete e.wrapper;
                e.wrapper = nullptr;
            });
            mInstances.Clear();
            mTemplates.Clear();
            mNamedMaterials.Clear();
            mDevice    = nullptr;
            mResources = nullptr;
        }

        NkMaterialHandle NkMaterialSystem::CreateMaterial(const NkMaterialDesc& desc) {
            uint64 hid = NextId();
            TemplateEntry e{};
            e.desc     = desc;
            e.name     = desc.name;
            e.compiled = false;  // lazy-compile
            mTemplates[hid] = NkTraits::NkMove(e);
            NkMaterialHandle h; h.id = hid; return h;
        }

        NkMaterialHandle NkMaterialSystem::RegisterMaterial(const char* name,
                                                              const NkMaterialDesc& desc) {
            auto h = CreateMaterial(desc);
            if (h.IsValid() && name) mNamedMaterials[NkString(name)] = h;
            return h;
        }

        NkMaterialHandle NkMaterialSystem::FindMaterial(const char* name) const {
            if (!name) return {};
            auto* found = mNamedMaterials.Find(NkString(name));
            return found ? *found : NkMaterialHandle{};
        }

        NkMaterialInstance* NkMaterialSystem::CreateInstance(NkMaterialHandle tmpl) {
            if (!tmpl.IsValid()) return nullptr;

            uint64 hid = NextId();
            InstanceEntry e{};
            e.templateHandle = tmpl;
            e.dirty          = true;
            e.wrapper        = new NkMaterialInstance();
            e.wrapper->mHandle = NkMaterialInstHandle{hid};
            e.wrapper->mSystem = this;

            auto* tmplEntry = mTemplates.Find(tmpl.id);
            if (tmplEntry) e.wrapper->mDesc = tmplEntry->desc;

            mInstances[hid] = NkTraits::NkMove(e);
            return mInstances[hid].wrapper;
        }

        NkMaterialInstance* NkMaterialSystem::CreateInstance(
                const NkMaterialInstanceDesc& desc) {
            auto* inst = CreateInstance(desc.templateHandle);
            if (!inst) return nullptr;
            // Appliquer les overrides
            for (uint32 i = 0; i < (uint32)desc.overrides.Size(); ++i) {
                const auto& p = desc.overrides[i];
                if (p.type == NkMaterialParam::Type::TEX)
                    inst->SetTexture(p.name.CStr(), p.texture);
                else if (p.type == NkMaterialParam::Type::F1)
                    inst->SetFloat(p.name.CStr(), p.val.f[0]);
                else if (p.type == NkMaterialParam::Type::F4)
                    inst->SetColor(p.name.CStr(), {p.val.f[0],p.val.f[1],p.val.f[2],p.val.f[3]});
            }
            return inst;
        }

        void NkMaterialSystem::DestroyInstance(NkMaterialInstance*& inst) {
            if (!inst) return;
            uint64 id = inst->mHandle.id;
            auto* e = mInstances.Find(id);
            if (e) {
                if (e->rhiDescSetId) {
                    NkDescSetHandle s; s.id = e->rhiDescSetId;
                    mDevice->FreeDescriptorSet(s);
                }
                if (e->rhiUBOId) {
                    NkBufferHandle b; b.id = e->rhiUBOId;
                    mDevice->DestroyBuffer(b);
                }
                delete e->wrapper;
                mInstances.Erase(id);
            }
            inst = nullptr;
        }

        void NkMaterialSystem::FlushPendingUpdates() {
            mInstances.ForEach([this](const uint64& id, InstanceEntry& e) {
                if (e.dirty && e.rhiDescSetId) {
                    RebuildDescriptorSet(id, e);
                    e.dirty = false;
                }
            });
        }

        uint64 NkMaterialSystem::GetPipelineRHIId(NkMaterialHandle h) const {
            auto* e = mTemplates.Find(h.id);
            return (e && e->compiled) ? e->rhiPipelineId : 0;
        }

        uint64 NkMaterialSystem::GetDescSetRHIId(NkMaterialInstHandle h) const {
            auto* e = mInstances.Find(h.id);
            return e ? e->rhiDescSetId : 0;
        }

        bool NkMaterialSystem::IsMaterialDirty(NkMaterialInstHandle h) const {
            auto* e = mInstances.Find(h.id);
            return e ? e->dirty : false;
        }

        void NkMaterialSystem::CreateDefaultMaterials() {
            mDefaultPBR       = RegisterMaterial("Default_PBR",
                                                   NkMaterialDesc::PBR("Default_PBR"));
            mDefaultUnlit     = RegisterMaterial("Default_Unlit",
                                                   NkMaterialDesc::Unlit("Default_Unlit"));
            mDefaultToon      = RegisterMaterial("Default_Toon",
                                                   NkMaterialDesc::Toon("Default_Toon"));
            mDefaultWireframe = RegisterMaterial("Default_Wireframe",
                                                   NkMaterialDesc::Wireframe("Default_Wireframe"));
            mShadowDepth      = RegisterMaterial("ShadowDepth",
                                                   NkMaterialDesc::ShadowDepth("ShadowDepth"));
        }

        void NkMaterialSystem::RebuildDescriptorSet(uint64 id, InstanceEntry& e) {
            if (!mDevice) return;
            auto* tmpl = mTemplates.Find(e.templateHandle.id);
            if (!tmpl) return;

            // Préparer les écritures descriptor
            NkVector<NkDescriptorWrite> writes;

            for (uint32 i = 0; i < (uint32)e.params.Size(); ++i) {
                const auto& p = e.params[i];
                if (p.type != NkMaterialParam::Type::TEX) continue;
                if (!p.texture.IsValid()) continue;

                uint64 texRHI  = mResources->GetTextureRHIId(p.texture);
                uint64 sampRHI = mResources->GetSamplerRHIId(p.texture);

                // Slot convention : albedo=0, normal=1, orm=2, emissive=3, height=4
                int32 slot = -1;
                if (p.name == "albedo" || p.name == "uAlbedoMap")    slot = 0;
                else if (p.name == "normal" || p.name == "uNormalMap")  slot = 1;
                else if (p.name == "orm"    || p.name == "uORMMap")     slot = 2;
                else if (p.name == "emissive"||p.name == "uEmissiveTex")slot = 3;
                else if (p.name == "height"  ||p.name == "uHeightTex")  slot = 4;
                if (slot < 0) continue;

                NkDescriptorWrite w{};
                NkDescSetHandle ds; ds.id = e.rhiDescSetId;
                w.set     = ds;
                w.binding = (uint32)slot;
                w.type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                NkTextureHandle th; th.id = texRHI;  w.texture = th;
                NkSamplerHandle sh; sh.id = sampRHI; w.sampler = sh;
                w.textureLayout = NkResourceState::NK_SHADER_READ;
                writes.PushBack(w);
            }
            if (!writes.IsEmpty())
                mDevice->UpdateDescriptorSets(writes.Data(), (uint32)writes.Size());
        }

        // =============================================================================
        // NkMaterialInstance — API fluide
        // =============================================================================
        NkMaterialInstance* NkMaterialInstance::SetAlbedo(const NkColorF& c) {
            return SetColor("albedo_color", c);
        }
        NkMaterialInstance* NkMaterialInstance::SetAlbedoMap(NkTextureHandle tex) {
            return SetTexture("albedo", tex);
        }
        NkMaterialInstance* NkMaterialInstance::SetNormalMap(NkTextureHandle tex,
                                                               float32 scale) {
            SetTexture("normal", tex);
            return SetFloat("normal_scale", scale);
        }
        NkMaterialInstance* NkMaterialInstance::SetORMMap(NkTextureHandle tex) {
            return SetTexture("orm", tex);
        }
        NkMaterialInstance* NkMaterialInstance::SetRoughness(float32 v) {
            return SetFloat("roughness", v);
        }
        NkMaterialInstance* NkMaterialInstance::SetMetallic(float32 v) {
            return SetFloat("metallic", v);
        }
        NkMaterialInstance* NkMaterialInstance::SetEmissive(const NkColorF& c,
                                                              float32 intensity) {
            SetColor("emissive_color", c);
            return SetFloat("emissive_scale", intensity);
        }
        NkMaterialInstance* NkMaterialInstance::SetEmissiveMap(NkTextureHandle tex) {
            return SetTexture("emissive", tex);
        }
        NkMaterialInstance* NkMaterialInstance::SetTexture(const char* name,
                                                             NkTextureHandle tex) {
            if (!mSystem || !mHandle.IsValid()) return this;
            mSystem->SetMaterialTexture(mHandle, name, tex);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetFloat(const char* name, float32 v) {
            if (!mSystem || !mHandle.IsValid()) return this;
            mSystem->SetMaterialFloat(mHandle, name, v);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec4(const char* name,
                                                          float32 x, float32 y,
                                                          float32 z, float32 w) {
            if (!mSystem || !mHandle.IsValid()) return this;
            mSystem->SetMaterialVec4(mHandle, name, {x,y,z,w});
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetColor(const char* name,
                                                           const NkColorF& c) {
            return SetVec4(name, c.r, c.g, c.b, c.a);
        }
        NkMaterialInstance* NkMaterialInstance::SetOpacity(float32 v) {
            return SetFloat("alpha", v);
        }
        NkMaterialInstance* NkMaterialInstance::SetTwoSided(bool v) {
            if (!mSystem || !mHandle.IsValid()) return this;
            mSystem->SetMaterialBool(mHandle, "two_sided", v);
            return this;
        }

        // Helpers SetMaterial* dans NkMaterialSystem
        bool NkMaterialSystem::SetMaterialTexture(NkMaterialInstHandle inst,
                                                    const char* name,
                                                    NkTextureHandle tex) {
            auto* e = mInstances.Find(inst.id);
            if (!e) return false;
            // Chercher si déjà présent, sinon ajouter
            for (uint32 i = 0; i < (uint32)e->params.Size(); ++i) {
                if (e->params[i].name == name) {
                    e->params[i].texture = tex;
                    e->dirty = true;
                    return true;
                }
            }
            e->params.PushBack(NkMaterialParam::Tex(name, tex));
            e->dirty = true;
            return true;
        }
        bool NkMaterialSystem::SetMaterialFloat(NkMaterialInstHandle inst,
                                                  const char* name, float32 v) {
            auto* e = mInstances.Find(inst.id);
            if (!e) return false;
            for (uint32 i = 0; i < (uint32)e->params.Size(); ++i) {
                if (e->params[i].name == name) { e->params[i].val.f[0]=v; e->dirty=true; return true; }
            }
            e->params.PushBack(NkMaterialParam::Float(name, v));
            e->dirty = true; return true;
        }
        bool NkMaterialSystem::SetMaterialVec4(NkMaterialInstHandle inst,
                                                 const char* name, NkVec4f v) {
            auto* e = mInstances.Find(inst.id);
            if (!e) return false;
            for (uint32 i = 0; i < (uint32)e->params.Size(); ++i) {
                if (e->params[i].name == name) {
                    e->params[i].val.f[0]=v.x; e->params[i].val.f[1]=v.y;
                    e->params[i].val.f[2]=v.z; e->params[i].val.f[3]=v.w;
                    e->dirty=true; return true;
                }
            }
            e->params.PushBack(NkMaterialParam::Float4(name, v.x,v.y,v.z,v.w));
            e->dirty=true; return true;
        }
        bool NkMaterialSystem::SetMaterialBool(NkMaterialInstHandle inst,
                                                 const char* name, bool v) {
            auto* e = mInstances.Find(inst.id);
            if (!e) return false;
            for (uint32 i = 0; i < (uint32)e->params.Size(); ++i) {
                if (e->params[i].name == name) { e->params[i].val.b=v; e->dirty=true; return true; }
            }
            e->params.PushBack(NkMaterialParam::Bool(name, v));
            e->dirty=true; return true;
        }
        bool NkMaterialSystem::SetMaterialVec2(NkMaterialInstHandle inst,
                                                 const char* name, NkVec2f v) {
            return SetMaterialFloat(inst, name, v.x);
        }
        bool NkMaterialSystem::SetMaterialVec3(NkMaterialInstHandle inst,
                                                 const char* name, NkVec3f v) {
            auto* e = mInstances.Find(inst.id);
            if (!e) return false;
            for (uint32 i = 0; i < (uint32)e->params.Size(); ++i) {
                if (e->params[i].name == name) {
                    e->params[i].val.f[0]=v.x; e->params[i].val.f[1]=v.y; e->params[i].val.f[2]=v.z;
                    e->dirty=true; return true;
                }
            }
            e->params.PushBack(NkMaterialParam::Float3(name, v.x, v.y, v.z));
            e->dirty=true; return true;
        }
        bool NkMaterialSystem::SetMaterialInt(NkMaterialInstHandle inst,
                                               const char* name, int32 v) {
            auto* e = mInstances.Find(inst.id);
            if (!e) return false;
            for (uint32 i = 0; i < (uint32)e->params.Size(); ++i) {
                if (e->params[i].name == name) { e->params[i].val.i[0]=v; e->dirty=true; return true; }
            }
            NkMaterialParam p; p.name=name; p.type=NkMaterialParam::Type::I1; p.val.i[0]=v;
            e->params.PushBack(p); e->dirty=true; return true;
        }
        void NkMaterialSystem::FlushMaterialInst(NkMaterialInstHandle inst) {
            auto* e = mInstances.Find(inst.id);
            if (!e || !e->dirty) return;
            RebuildDescriptorSet(inst.id, *e);
            e->dirty = false;
        }

    } // namespace renderer
} // namespace nkentseu
