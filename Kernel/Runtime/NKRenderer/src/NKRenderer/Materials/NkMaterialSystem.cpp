// =============================================================================
// NkMaterialSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkMaterialSystem.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
    namespace renderer {

        NkMaterialSystem::~NkMaterialSystem() { Shutdown(); }

        bool NkMaterialSystem::Init(NkIDevice* device, NkTextureLibrary* texLib,
                                    NkShaderLibrary* shaderLib, NkGraphicsApi api) {
            mDevice = device; mTexLib = texLib; mShaderLib = shaderLib; mApi = api;

            // Per-instance descriptor layout:
            //   binding 1 = PBR uniform buffer
            //   binding 3 = albedo sampled texture
            //   binding 4 = normal sampled texture
            //   binding 5 = orm sampled texture
            //   binding 6 = emissive sampled texture
            // NK_ALL_GRAPHICS est dans ::nkentseu::NkShaderStage (RHI), pas dans
            // renderer::NkShaderStage. Qualification explicite pour eviter l'ambiguite.
            using RHIStage = ::nkentseu::NkShaderStage;
            NkDescriptorSetLayoutDesc instLayout;
            instLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,
                           RHIStage::NK_ALL_GRAPHICS)
                      .Add(3, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                           RHIStage::NK_ALL_GRAPHICS)
                      .Add(4, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                           RHIStage::NK_ALL_GRAPHICS)
                      .Add(5, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                           RHIStage::NK_ALL_GRAPHICS)
                      .Add(6, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                           RHIStage::NK_ALL_GRAPHICS);
            mInstDescLayout = mDevice->CreateDescriptorSetLayout(instLayout);

            mLinearSampler = mDevice->CreateSampler(NkSamplerDesc::Linear());

            RegisterBuiltins();
            return true;
        }

        void NkMaterialSystem::Shutdown() {
            for (auto* inst : mInstances) {
                if (inst->mUBO.IsValid())     mDevice->DestroyBuffer(inst->mUBO);
                if (inst->mDescSet.IsValid()) mDevice->FreeDescriptorSet(inst->mDescSet);
                delete inst;
            }
            mInstances.Clear();
            for (auto& pair : mTemplates) {
                TemplateEntry& e = pair.Second;
                if (e.pipeline.IsValid()) mDevice->DestroyPipeline(e.pipeline);
            }
            mTemplates.Clear();
            if (mInstDescLayout.IsValid()) mDevice->DestroyDescriptorSetLayout(mInstDescLayout);
            if (mLinearSampler.IsValid())  mDevice->DestroySampler(mLinearSampler);
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
            for (auto& pair : mTemplates) {
                if (pair.Second.desc.name == name) return NkMatHandle{pair.First};
            }
            return NkMatHandle::Null();
        }

        // ── Built-ins ─────────────────────────────────────────────────────────────
        // Chaque template built-in est associe a son shader GLSL VK via
        // NkShaderLibrary::LoadOrCompileVF(shaderDirName, fallbackVS, fallbackFS).
        // La convention de chemin est :
        //   Resources/NKRenderer/Shaders/<shaderDir>/VK/<shaderdir>.vert.vk.glsl
        //   Resources/NKRenderer/Shaders/<shaderDir>/VK/<shaderdir>.frag.vk.glsl
        // Si le fichier est absent, le fallback embedded (source vide ici) laisse
        // le shader invalide — c'est intentionnel : les templates sans shader sur
        // disque ne rendent pas de pipeline jusqu'a ce que le fichier soit cree.
        void NkMaterialSystem::RegisterBuiltins() {
            // Enregistrement PUR (sans compilation shader).
            // Le chargement du shader et la compilation du pipeline sont
            // deferes a CompilePipeline(), appelee au premier BindInstance().
            // Cela evite : collision de noms avec NkRender3D ("PBR" enregistre
            // au step 6 vs step 9), et echec d'init si GlslToGlsl echoue.
            auto reg = [this](NkMaterialType t, const char* name,
                              const char* shaderDir,
                              NkRenderQueue q  = NkRenderQueue::NK_OPAQUE,
                              NkCullMode cull  = NkCullMode::NK_BACK,
                              NkFillMode fill  = NkFillMode::NK_SOLID) -> NkMatHandle {
                NkMaterialTemplateDesc d;
                d.type      = t;
                d.name      = name;
                d.queue     = q;
                d.cullMode  = cull;
                d.fillMode  = fill;
                // shaderDir stocke dans vertSrcGL comme "hint" de chemin.
                // CompilePipeline() le lira pour LoadOrCompileVF("MatSys_<dir>", ...).
                d.vertSrcGL = shaderDir ? shaderDir : "";
                return RegisterTemplate(d);
            };

            mTmplPBR     = reg(NkMaterialType::NK_PBR_METALLIC, "Default_PBR",     "PBR");
            mTmplToon    = reg(NkMaterialType::NK_TOON,         "Default_Toon",    "Toon");
            mTmplUnlit   = reg(NkMaterialType::NK_UNLIT,        "Default_Unlit",   "Unlit");
            mTmplWire    = reg(NkMaterialType::NK_WIREFRAME_MAT,"Default_Wireframe","PBR",
                               NkRenderQueue::NK_OPAQUE,
                               NkCullMode::NK_NONE, NkFillMode::NK_WIREFRAME);
            mTmplSkin    = reg(NkMaterialType::NK_SKIN,         "Default_Skin",    "Skin");
            mTmplHair    = reg(NkMaterialType::NK_HAIR,         "Default_Hair",    "Hair",
                               NkRenderQueue::NK_ALPHA_TEST);
            mTmplAnime   = reg(NkMaterialType::NK_ANIME,        "Default_Anime",   "Anime");
            mTmplArchviz = reg(NkMaterialType::NK_ARCHIVIZ,     "Default_Archviz", "PBR");
        }

        // ── Instance ─────────────────────────────────────────────────────────────
        NkMaterialInstance* NkMaterialSystem::CreateInstance(NkMatHandle tmpl) {
            auto* inst    = new NkMaterialInstance();
            inst->mTemplate = tmpl;
            inst->mDirty    = true;
            inst->mPBR.albedo    = {1,1,1,1};
            inst->mPBR.metallic  = 0.f;
            inst->mPBR.roughness = 0.5f;
            inst->mPBR.ao        = 1.f;

            // Per-instance GPU UBO
            inst->mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(sizeof(NkPBRParams)));

            // Allocate descriptor set
            if (mInstDescLayout.IsValid())
                inst->mDescSet = mDevice->AllocateDescriptorSet(mInstDescLayout);

            mInstances.PushBack(inst);
            return inst;
        }

        void NkMaterialSystem::DestroyInstance(NkMaterialInstance*& inst) {
            if (!inst) return;
            for (uint32 i=0;i<(uint32)mInstances.Size();i++){
                if (mInstances[i]==inst){
                    if (inst->mUBO.IsValid())     mDevice->DestroyBuffer(inst->mUBO);
                    if (inst->mDescSet.IsValid()) mDevice->FreeDescriptorSet(inst->mDescSet);
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

            if (!tmplEntry->compiled) {
                tmplEntry->pipeline = CompilePipeline(*tmplEntry);
                tmplEntry->compiled = true;
            }
            if (tmplEntry->pipeline.IsValid())
                cmd->BindGraphicsPipeline(tmplEntry->pipeline);

            if (inst->mDirty && inst->mDescSet.IsValid()) {
                // Upload PBR params to UBO
                if (inst->mUBO.IsValid())
                    mDevice->WriteBuffer(inst->mUBO, &inst->mPBR, sizeof(NkPBRParams));

                // Helper to get a texture's RHI handle (fallback to white)
                auto GetTex = [&](const NkString& name) -> NkTextureHandle {
                    for (auto& p : inst->mParams)
                        if (p.kind==NkMaterialInstance::Param::Kind::TEX && p.name==name)
                            return texLib->GetRHIHandle(p.tex);
                    return texLib->GetRHIHandle(texLib->GetWhite1x1());
                };

                NkTextureHandle albedoTex   = GetTex("albedo");
                NkTextureHandle normalTex   = GetTex("normal");
                NkTextureHandle ormTex      = GetTex("orm");
                NkTextureHandle emissiveTex = GetTex("emissive");

                NkDescriptorWrite writes[5] = {};
                // binding 1: PBR UBO
                writes[0].set=inst->mDescSet; writes[0].binding=1;
                writes[0].type=NkDescriptorType::NK_UNIFORM_BUFFER;
                writes[0].buffer=inst->mUBO; writes[0].bufferRange=sizeof(NkPBRParams);
                // binding 3-6: textures
                auto fillTex = [&](uint32 idx, uint32 binding, NkTextureHandle tex) {
                    writes[idx].set=inst->mDescSet; writes[idx].binding=binding;
                    writes[idx].type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                    writes[idx].texture=tex; writes[idx].sampler=mLinearSampler;
                };
                fillTex(1, 3, albedoTex);
                fillTex(2, 4, normalTex);
                fillTex(3, 5, ormTex);
                fillTex(4, 6, emissiveTex);
                mDevice->UpdateDescriptorSets(writes, 5);
                inst->MarkClean();
            }

            if (inst->mDescSet.IsValid())
                cmd->BindDescriptorSet(inst->mDescSet, 2);

            return true;
        }

        NkPipelineHandle NkMaterialSystem::CompilePipeline(TemplateEntry& t) {
            // Chargement lazy du shader au premier CompilePipeline().
            // Priority :
            //   1. shaderHandle deja valide (custom material ou appel precedent)
            //   2. shaderDir hint dans vertSrcGL → LoadOrCompileVF depuis disque
            //      (nom prefixe "MatSys_<dir>" pour eviter collision avec NkRender3D)
            //   3. sources inline vertSrcVK/fragSrcVK (cas NK_CUSTOM)
            //   4. skip → template param-only, aucun rendu direct
            ::nkentseu::NkShaderHandle sh = t.shaderHandle;

            if (!sh.IsValid() && mShaderLib) {
                // Cas 2 : hint de chemin stocke dans vertSrcGL par RegisterBuiltins
                const NkString& shaderDirHint = t.desc.vertSrcGL;
                const bool isHint = !shaderDirHint.Empty()
                                 && t.desc.fragSrcGL.Empty(); // hint = vertSrcGL seul
                if (isHint) {
                    NkString matSysName = NkString("MatSys_") + shaderDirHint;
                    auto prog = mShaderLib->LoadOrCompileVF(shaderDirHint, "", "");
                    // On reuse le handle existant si deja compile (mByName["PBR"]).
                    // Pour le pipeline MatSys on prend directement le RHI handle.
                    sh = mShaderLib->GetRHIHandle(prog);
                    t.shaderHandle = sh;
                }

                // Cas 3 : sources inline (NK_CUSTOM ou override explicite)
                if (!sh.IsValid()) {
                    const bool hasVK = !t.desc.vertSrcVK.Empty() && !t.desc.fragSrcVK.Empty();
                    const bool hasGL = !t.desc.fragSrcGL.Empty(); // vrai inline (pas hint)
                    if (hasVK || hasGL) {
                        const NkString& vs = hasVK ? t.desc.vertSrcVK : t.desc.vertSrcGL;
                        const NkString& fs = hasVK ? t.desc.fragSrcVK : t.desc.fragSrcGL;
                        auto prog = mShaderLib->CompileVF(vs, fs, t.desc.name);
                        sh = mShaderLib->GetRHIHandle(prog);
                        t.shaderHandle = sh;
                    }
                }
            }

            if (!sh.IsValid()) {
                logger.Info("[NkMaterialSystem] '{0}': no shader available, "
                            "skip pipeline compile\n", t.desc.name);
                return {};
            }

            NkGraphicsPipelineDesc pd;
            pd.debugName = t.desc.name.CStr();
            pd.shader    = sh;

            // Rasterizer
            pd.rasterizer = NkRasterizerDesc::Default();
            if (t.desc.doubleSided || t.desc.cullMode == NkCullMode::NK_NONE)
                pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_NONE;
            else if (t.desc.cullMode == NkCullMode::NK_FRONT)
                pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_FRONT;
            if (t.desc.fillMode == NkFillMode::NK_WIREFRAME)
                pd.rasterizer.fillMode = nkentseu::NkFillMode::NK_WIREFRAME;

            // Depth
            if (!t.desc.depthTest) {
                pd.depthStencil = NkDepthStencilDesc::NoDepth();
            } else {
                pd.depthStencil = NkDepthStencilDesc::Default();
                pd.depthStencil.depthWriteEnable = t.desc.depthWrite;
            }

            // Blend
            switch (t.desc.blendMode) {
                case NkBlendMode::NK_ALPHA:     pd.blend = NkBlendDesc::Alpha();    break;
                case NkBlendMode::NK_ADDITIVE:  pd.blend = NkBlendDesc::Additive(); break;
                default:                        pd.blend = NkBlendDesc::Opaque();   break;
            }

            // Descriptor set layout set 2 = per-instance (PBR UBO + textures)
            if (mInstDescLayout.IsValid())
                pd.descriptorSetLayouts.PushBack(mInstDescLayout);

            return mDevice->CreateGraphicsPipeline(pd);
        }

        const NkString* NkMaterialSystem::GetTemplateName(NkMatHandle h) const {
            auto* e = mTemplates.Find(h.id);
            return e ? &e->desc.name : nullptr;
        }

        NkMaterialType NkMaterialSystem::GetTemplateType(NkMatHandle h) const {
            auto* e = mTemplates.Find(h.id);
            return e ? e->desc.type : NkMaterialType::NK_PBR_METALLIC;
        }

        void NkMaterialSystem::FlushCompilations() {
            for (auto& pair : mTemplates) {
                TemplateEntry& e = pair.Second;
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
            for (auto& p:mParams)
                if(p.name==n && p.kind==Param::Kind::TEX) {
                    p.tex=t;
                    mDirty=true;
                    return this;
                }
            Param p;
            p.name=n;
            p.kind=Param::Kind::TEX;
            p.tex=t;
            mParams.PushBack(p);
            mDirty=true;
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetAlbedoMap  (NkTexHandle t){return SetTexture("albedo",t);}
        NkMaterialInstance* NkMaterialInstance::SetNormalMap  (NkTexHandle t,float32 s){mPBR.normalStrength=s;return SetTexture("normal",t);}
        NkMaterialInstance* NkMaterialInstance::SetORMMap     (NkTexHandle t){return SetTexture("orm",t);}
        NkMaterialInstance* NkMaterialInstance::SetEmissiveMap(NkTexHandle t){return SetTexture("emissive",t);}
        NkMaterialInstance* NkMaterialInstance::SetAOMap      (NkTexHandle t){return SetTexture("ao",t);}

        NkMaterialInstance* NkMaterialInstance::SetFloat(const NkString& n,float32 v) {
            Param p; p.name=n; p.kind=Param::Kind::F; p.f=v;
            mParams.PushBack(p); mDirty=true; return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec3(const NkString& n,NkVec3f v){
            Param p; p.name=n; p.kind=Param::Kind::V3; p.v3=v;
            mParams.PushBack(p); mDirty=true; return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec4(const NkString& n,NkVec4f v){
            Param p; p.name=n; p.kind=Param::Kind::V4; p.v4=v;
            mParams.PushBack(p); mDirty=true; return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec2(const NkString& n,NkVec2f v){
            Param p; p.name=n; p.kind=Param::Kind::V2; p.v2=v;
            mParams.PushBack(p); mDirty=true; return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetInt(const NkString& n,int32 v){
            Param p; p.name=n; p.kind=Param::Kind::I; p.i=v;
            mParams.PushBack(p); mDirty=true; return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetBool(const NkString& n,bool v){
            Param p; p.name=n; p.kind=Param::Kind::B; p.b=v;
            mParams.PushBack(p); mDirty=true; return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetColor(const NkString& n,NkVec4f c){return SetVec4(n,c);}

        NkRenderQueue NkMaterialInstance::GetQueue() const {
            return NkRenderQueue::NK_OPAQUE;
        }

    } // namespace renderer
} // namespace nkentseu
