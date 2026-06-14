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
            // Binding 8 pour l'UBO materiau : binding=4 collide avec texNormal
            // (COMBINED_IMAGE_SAMPLER au binding=4) dans le tableau descriptor GL —
            // le second Add(4,SAMPLER) ecrasait le premier Add(4,UBO).
            // Bindings libres : 0=Camera, 1=Object, 2=Lights, 3=Shadow UBO ;
            //                   3=albedo, 4=normal, 5=ORM, 6=emissive texture.
            // → binding=8 est libre dans les deux namespaces (UBO et texture).
            NkDescriptorSetLayoutDesc instLayout;
            instLayout.Add(8, NkDescriptorType::NK_UNIFORM_BUFFER,
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
            // Destruction differee des pipelines orphelins par UpdateRenderPass.
            for (auto& p : mPendingDestroy) {
                if (p.IsValid()) mDevice->DestroyPipeline(p);
            }
            mPendingDestroy.Clear();
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
                              // NK_NONE par défaut : la passe miroir applique mirror_matrix
                              // (det=-1) qui inverse le winding des triangles. Avec BACK cull,
                              // les sphères miroir disparaissent. NoCull permet la double-passe
                              // sans état de cull dynamique. Coût négligeable sur scènes
                              // raisonnables, à raffiner avec un toggle CB plus tard si besoin.
                              NkCullMode cull  = NkCullMode::NK_NONE,
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

            mTmplPBR      = reg(NkMaterialType::NK_PBR_METALLIC, "Default_PBR",      "PBR");
            mTmplToon     = reg(NkMaterialType::NK_TOON,         "Default_Toon",     "Toon");
            mTmplUnlit    = reg(NkMaterialType::NK_UNLIT,        "Default_Unlit",    "Unlit");
            mTmplWire     = reg(NkMaterialType::NK_WIREFRAME_MAT,"Default_Wireframe","PBR",
                                NkRenderQueue::NK_OPAQUE,
                                NkCullMode::NK_NONE, NkFillMode::NK_WIREFRAME);
            mTmplSkin     = reg(NkMaterialType::NK_SKIN,         "Default_Skin",     "Skin");
            mTmplHair     = reg(NkMaterialType::NK_HAIR,         "Default_Hair",     "Hair",
                                NkRenderQueue::NK_ALPHA_TEST);
            mTmplAnime    = reg(NkMaterialType::NK_ANIME,        "Default_Anime",    "Anime");
            mTmplArchviz  = reg(NkMaterialType::NK_ARCHIVIZ,     "Default_Archviz",  "PBR");
            mTmplReflFloor= reg(NkMaterialType::NK_REFL_FLOOR,   "Default_ReflFloor","ReflFloor");
            // M.1 v0 : Layered material (2 layers PBR + masque vertex-color).
            // Shader Resources/.../Shaders/Layered/VK/layered.{vert,frag}.vk.glsl.
            // L'instance Layered ecrit un NkLayeredParams (208B) dans son UBO
            // au lieu du NkPBRParams (96B) standard — cf. BindInstance branch.
            mTmplLayered  = reg(NkMaterialType::NK_LAYERED,      "Default_Layered",  "Layered");
            // M.1 v1 : 8 layers PBR simplifies (albedo+metallic+roughness) avec
            // masks parametriques. Shader Resources/.../Shaders/LayeredV1/VK/.
            // L'instance ecrit NkLayeredV1Params (~336B) au lieu du PBR (96B).
            mTmplLayeredV1= reg(NkMaterialType::NK_LAYERED_V1,   "Default_LayeredV1","LayeredV1");
        }

        // ── Contexte partagé (NkRender3D → NkMaterialSystem) ──────────────────────
        void NkMaterialSystem::SetSharedContext(NkDescSetHandle globalLayout,
                                                NkDescSetHandle objectLayout,
                                                const NkVertexLayout& vertexLayout,
                                                NkRenderPassHandle rp) {
            mSharedGlobalLayout = globalLayout;
            mSharedObjectLayout = objectLayout;
            (void)vertexLayout; // layout reconstruit dans CompilePipeline, evite copy NkVector
            mCurrentRP          = rp;
            // Invalide les pipelines existants pour les recompiler avec le bon layout.
            for (auto& pair : mTemplates) {
                TemplateEntry& e = pair.Second;
                if (e.compiled && e.pipeline.IsValid()) {
                    mDevice->DestroyPipeline(e.pipeline);
                    e.pipeline = {};
                }
                e.compiled = false;
            }
        }

        void NkMaterialSystem::UpdateRenderPass(NkRenderPassHandle rp) {
            // Stocke uniquement le RP pour la PROCHAINE compilation lazy. Ne pas
            // invalider les pipelines deja compiles : Vulkan accepte un pipeline
            // pour tout RP *compatible* (memes formats d'attachment). Le projet
            // garantit que tous les RPs utilises partagent HDR R16G16B16A16
            // + D32_FLOAT (Geometry pass HDR et tout RT planar reflection HDR).
            // Cela elimine 2 recompiles par frame quand Demo4 alterne passe
            // miroir (rt_rp) et passe principale (Geometry_rp) -> regression
            // FPS observee precedemment.
            mCurrentRP = rp;
        }

        // ── Accès instance / pipeline ─────────────────────────────────────────────
        NkMaterialInstance* NkMaterialSystem::GetInstance(NkMatInstHandle h) const {
            auto* p = mInstanceMap.Find(h.id);
            return p ? *p : nullptr;
        }

        NkPipelineHandle NkMaterialSystem::GetPipeline(NkMatHandle tmpl,
                                                        NkRenderPassHandle currentRP) {
            if (currentRP.IsValid() && currentRP != mCurrentRP)
                UpdateRenderPass(currentRP);
            auto* e = mTemplates.Find(tmpl.id);
            if (!e) return {};
            if (!e->compiled) {
                e->pipeline = CompilePipeline(*e);
                e->compiled = true;
            }
            return e->pipeline;
        }

        // ── Instance ─────────────────────────────────────────────────────────────
        NkMaterialInstance* NkMaterialSystem::CreateInstance(NkMatHandle tmpl) {
            auto* inst    = new NkMaterialInstance();
            inst->mHandle   = NkMatInstHandle{mNextInstId++};
            inst->mTemplate = tmpl;
            inst->mDirty    = true;
            inst->mPBR.albedo    = {1,1,1,1};
            inst->mPBR.metallic  = 0.f;
            inst->mPBR.roughness = 0.5f;
            inst->mPBR.ao        = 1.f;

            // Per-instance GPU UBO. Taille selon type :
            //   NK_LAYERED    -> sizeof(NkLayeredParams)   (~208 B, 2 PBR + mask)
            //   NK_LAYERED_V1 -> sizeof(NkLayeredV1Params) (~336 B, 8 PBRLayer + masks)
            //   autres        -> sizeof(NkPBRParams)       (96 B, suffit Toon/Anime aussi)
            const NkMaterialType matType = GetTemplateType(tmpl);
            uint64 uboBytes = sizeof(NkPBRParams);
            if      (matType == NkMaterialType::NK_LAYERED)    uboBytes = sizeof(NkLayeredParams);
            else if (matType == NkMaterialType::NK_LAYERED_V1) uboBytes = sizeof(NkLayeredV1Params);
            inst->mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(uboBytes));

            // Allocate descriptor set
            if (mInstDescLayout.IsValid())
                inst->mDescSet = mDevice->AllocateDescriptorSet(mInstDescLayout);

            mInstances.PushBack(inst);
            mInstanceMap.Insert(inst->mHandle.id, inst);
            return inst;
        }

        void NkMaterialSystem::DestroyInstance(NkMaterialInstance*& inst) {
            if (!inst) return;
            // M.4 : detache du parent (retire de mChildren) et orpheline les enfants
            // en promouvant leurs valeurs locales (deja copiees -> rien a faire).
            if (inst->mParent) {
                auto& siblings = inst->mParent->mChildren;
                for (uint32 i = 0; i < (uint32)siblings.Size(); ++i) {
                    if (siblings[i] == inst) { siblings.RemoveAt(i); break; }
                }
                inst->mParent = nullptr;
            }
            for (auto* child : inst->mChildren) {
                if (child) child->mParent = nullptr;
            }
            inst->mChildren.Clear();

            mInstanceMap.Remove(inst->mHandle.id);
            for (uint32 i=0;i<(uint32)mInstances.Size();i++){
                if (mInstances[i]==inst){
                    if (inst->mUBO.IsValid())     mDevice->DestroyBuffer(inst->mUBO);
                    if (inst->mDescSet.IsValid()) mDevice->FreeDescriptorSet(inst->mDescSet);
                    delete inst; mInstances.RemoveAt(i); break;
                }
            }
            inst=nullptr;
        }

        // ── M.4 Hierarchical Instances ───────────────────────────────────────────
        NkMaterialInstance* NkMaterialSystem::CreateChildInstance(NkMaterialInstance* parent) {
            if (!parent) return nullptr;
            auto* child = CreateInstance(parent->mTemplate);
            if (!child) return nullptr;

            // Copie initiale des params depuis le parent (full snapshot)
            child->mPBR     = parent->mPBR;
            child->mToon    = parent->mToon;
            child->mLayered = parent->mLayered;
            child->mParams  = parent->mParams;
            child->mDirty   = true;

            // Tous les masks restent a 0 : l'enfant herite de tout
            child->mPBROverrideMask    = 0;
            child->mToonOverrideMask   = 0;
            child->mLayeredOverridden  = false;
            child->mOverrideParamNames.Clear();

            // Etablit le lien parent->enfant
            child->mParent = parent;
            parent->mChildren.PushBack(child);
            return child;
        }

        // ── M.4 : Propagation des changements aux enfants ────────────────────────
        // Copie un champ PBR depuis ce parent vers tous les enfants non-overrides
        // pour ce bit (recursivement). Idempotent ; safe a appeler quand pas d'enfants.
        void NkMaterialInstance::PropagatePBRField(uint32 bit) {
            for (auto* child : mChildren) {
                if (!child) continue;
                if (child->mPBROverrideMask & bit) continue;   // override -> skip

                // Copie le champ corresp depuis ce parent
                switch (bit) {
                    case NK_PBR_O_ALBEDO:
                        child->mPBR.albedo = mPBR.albedo;
                        break;
                    case NK_PBR_O_EMISSIVE:
                        child->mPBR.emissive         = mPBR.emissive;
                        child->mPBR.emissiveStrength = mPBR.emissiveStrength;
                        break;
                    case NK_PBR_O_METALLIC:
                        child->mPBR.metallic = mPBR.metallic;
                        break;
                    case NK_PBR_O_ROUGHNESS:
                        child->mPBR.roughness = mPBR.roughness;
                        break;
                    case NK_PBR_O_AO:
                        child->mPBR.ao = mPBR.ao;
                        break;
                    case NK_PBR_O_NORMAL_STR:
                        child->mPBR.normalStrength = mPBR.normalStrength;
                        break;
                    case NK_PBR_O_CLEARCOAT:
                        child->mPBR.clearcoat      = mPBR.clearcoat;
                        child->mPBR.clearcoatRough = mPBR.clearcoatRough;
                        break;
                    case NK_PBR_O_SUBSURFACE:
                        child->mPBR.subsurface      = mPBR.subsurface;
                        child->mPBR.subsurfaceColor = mPBR.subsurfaceColor;
                        break;
                    case NK_PBR_O_ANISOTROPY:
                        child->mPBR.anisotropy = mPBR.anisotropy;
                        break;
                    case NK_PBR_O_SHEEN:
                        child->mPBR.sheen = mPBR.sheen;
                        break;
                    case NK_PBR_O_REFL_FLOOR:
                        child->mPBR.reflFloorFaceMode = mPBR.reflFloorFaceMode;
                        break;
                    default:
                        break;
                }
                child->mDirty = true;
                child->PropagatePBRField(bit);   // recurse aux petits-enfants
            }
        }

        void NkMaterialInstance::PropagateToonField(uint32 bit) {
            for (auto* child : mChildren) {
                if (!child) continue;
                if (child->mToonOverrideMask & bit) continue;

                switch (bit) {
                    case NK_TOON_O_ALBEDO:
                        child->mToon.albedoColor = mToon.albedoColor;
                        break;
                    case NK_TOON_O_SHADOW_TH:
                        child->mToon.shadowThreshold = mToon.shadowThreshold;
                        break;
                    case NK_TOON_O_SHADOW_SMOOTH:
                        child->mToon.shadowSmooth = mToon.shadowSmooth;
                        break;
                    case NK_TOON_O_SHADOW_COLOR:
                        child->mToon.shadowColor = mToon.shadowColor;
                        break;
                    case NK_TOON_O_OUTLINE:
                        child->mToon.outlineWidth = mToon.outlineWidth;
                        child->mToon.outlineColor = mToon.outlineColor;
                        break;
                    case NK_TOON_O_RIM:
                        child->mToon.rimIntensity = mToon.rimIntensity;
                        child->mToon.rimColor     = mToon.rimColor;
                        break;
                    case NK_TOON_O_SPEC:
                        child->mToon.specHardness = mToon.specHardness;
                        break;
                    case NK_TOON_O_MATCAP:
                        child->mToon.matcapStrength = mToon.matcapStrength;
                        break;
                    default:
                        break;
                }
                child->mDirty = true;
                child->PropagateToonField(bit);
            }
        }

        void NkMaterialInstance::PropagateLayered() {
            for (auto* child : mChildren) {
                if (!child) continue;
                if (child->mLayeredOverridden) continue;
                child->mLayered = mLayered;
                child->mDirty = true;
                child->PropagateLayered();
            }
        }

        void NkMaterialInstance::PropagateNamedParam(const NkString& name) {
            for (auto* child : mChildren) {
                if (!child) continue;
                if (child->IsNamedOverridden(name)) continue;
                child->CopyNamedFromParent(name);
                child->mDirty = true;
                child->PropagateNamedParam(name);
            }
        }

        bool NkMaterialInstance::IsNamedOverridden(const NkString& name) const {
            for (auto& n : mOverrideParamNames) {
                if (n == name) return true;
            }
            return false;
        }

        void NkMaterialInstance::AddNamedOverride(const NkString& name) {
            if (!IsNamedOverridden(name)) mOverrideParamNames.PushBack(name);
        }

        // Copie un param nomme depuis mParent vers ce->mParams (insert si absent).
        void NkMaterialInstance::CopyNamedFromParent(const NkString& name) {
            if (!mParent) return;
            // Recherche dans le parent
            for (auto& p : mParent->mParams) {
                if (p.name == name) {
                    // Cherche dans this->mParams, remplace si trouve, sinon push
                    bool found = false;
                    for (auto& q : mParams) {
                        if (q.name == name && q.kind == p.kind) {
                            q = p;   // copy struct (kind+union+tex)
                            found = true;
                            break;
                        }
                    }
                    if (!found) mParams.PushBack(p);
                    return;
                }
            }
        }

        void NkMaterialInstance::MarkPBRChanged(uint32 bit) {
            mDirty = true;
            if (mParent) mPBROverrideMask |= bit;
            PropagatePBRField(bit);
        }

        void NkMaterialInstance::MarkToonChanged(uint32 bit) {
            mDirty = true;
            if (mParent) mToonOverrideMask |= bit;
            PropagateToonField(bit);
        }

        void NkMaterialInstance::MarkLayeredChanged() {
            mDirty = true;
            if (mParent) mLayeredOverridden = true;
            PropagateLayered();
        }

        void NkMaterialInstance::MarkNamedChanged(const NkString& name) {
            mDirty = true;
            if (mParent) AddNamedOverride(name);
            PropagateNamedParam(name);
        }

        // ── M.4 : Reset overrides (re-link au parent pour un champ) ──────────────
        NkMaterialInstance* NkMaterialInstance::ResetPBROverride(NkPBROverrideBit bit) {
            if (!mParent) return this;
            if (mPBROverrideMask & (uint32)bit) {
                mPBROverrideMask &= ~(uint32)bit;
                // Resync ce champ depuis le parent puis propage aux petits-enfants
                mParent->PropagatePBRField((uint32)bit);
            }
            return this;
        }

        NkMaterialInstance* NkMaterialInstance::ResetToonOverride(NkToonOverrideBit bit) {
            if (!mParent) return this;
            if (mToonOverrideMask & (uint32)bit) {
                mToonOverrideMask &= ~(uint32)bit;
                mParent->PropagateToonField((uint32)bit);
            }
            return this;
        }

        NkMaterialInstance* NkMaterialInstance::ResetNamedOverride(const NkString& name) {
            if (!mParent) return this;
            for (uint32 i = 0; i < (uint32)mOverrideParamNames.Size(); ++i) {
                if (mOverrideParamNames[i] == name) {
                    mOverrideParamNames.RemoveAt(i);
                    mParent->PropagateNamedParam(name);
                    return this;
                }
            }
            return this;
        }

        // ── Bind ─────────────────────────────────────────────────────────────────
        bool NkMaterialSystem::BindInstance(NkICommandBuffer* cmd,
                                            NkMaterialInstance* inst) {
            NkTextureLibrary* texLib = mTexLib;
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
                // Determine which params to upload based on material type.
                //   NK_LAYERED     -> NkLayeredParams   (2 PBR + mask).
                //   NK_LAYERED_V1  -> NkLayeredV1Params (8 PBRLayer + masks).
                //   NK_TOON / NK_TOON_INK / NK_ANIME -> NkToonParams.
                //   autres -> NkPBRParams (defaut).
                if (inst->mUBO.IsValid()) {
                    NkMaterialType matType = GetTemplateType(inst->mTemplate);
                    if (matType == NkMaterialType::NK_LAYERED) {
                        mDevice->WriteBuffer(inst->mUBO, &inst->mLayered, sizeof(NkLayeredParams));
                    } else if (matType == NkMaterialType::NK_LAYERED_V1) {
                        mDevice->WriteBuffer(inst->mUBO, &inst->mLayeredV1, sizeof(NkLayeredV1Params));
                    } else if (matType == NkMaterialType::NK_TOON  ||
                               matType == NkMaterialType::NK_TOON_INK ||
                               matType == NkMaterialType::NK_ANIME) {
                        mDevice->WriteBuffer(inst->mUBO, &inst->mToon, sizeof(NkToonParams));
                    } else {
                        mDevice->WriteBuffer(inst->mUBO, &inst->mPBR, sizeof(NkPBRParams));
                    }
                }

                // Helper to get a texture's RHI handle (fallback to white)
                auto GetTex = [&](const NkString& name) -> NkTextureHandle {
                    for (auto& p : inst->mParams)
                        if (p.kind==NkMaterialInstance::Param::Kind::TEX && p.name==name)
                            return texLib->GetRHIHandle(p.tex);
                    return texLib->GetRHIHandle(texLib->GetWhite1x1());
                };

                NkTextureHandle albedoTex   = GetTex("albedo");
                // Binding 4 : matcap si present (Toon/Anime), sinon normal map (PBR).
                // GetTex cherche "matcap" d'abord, retombe sur "normal".
                NkTextureHandle slot4Tex;
                {
                    bool found = false;
                    for (auto& p : inst->mParams)
                        if (p.kind==NkMaterialInstance::Param::Kind::TEX && p.name=="matcap") {
                            slot4Tex = texLib->GetRHIHandle(p.tex); found = true; break;
                        }
                    if (!found) slot4Tex = GetTex("normal");
                }
                NkTextureHandle ormTex      = GetTex("orm");
                NkTextureHandle emissiveTex = GetTex("emissive");

                NkDescriptorWrite writes[5] = {};
                // binding 8: material UBO (binding=4 est pris par texNormal SAMPLER en GL).
                // bufferRange depend du type material (sizeof exact).
                {
                    NkMaterialType matType = GetTemplateType(inst->mTemplate);
                    uint64 uboRange = sizeof(NkPBRParams);
                    if (matType == NkMaterialType::NK_LAYERED)
                        uboRange = sizeof(NkLayeredParams);
                    else if (matType == NkMaterialType::NK_LAYERED_V1)
                        uboRange = sizeof(NkLayeredV1Params);
                    else if (matType == NkMaterialType::NK_TOON ||
                             matType == NkMaterialType::NK_TOON_INK ||
                             matType == NkMaterialType::NK_ANIME)
                        uboRange = sizeof(NkToonParams);
                    writes[0].set=inst->mDescSet; writes[0].binding=8;
                    writes[0].type=NkDescriptorType::NK_UNIFORM_BUFFER;
                    writes[0].buffer=inst->mUBO; writes[0].bufferRange=uboRange;
                }
                // binding 3-6: textures
                auto fillTex = [&](uint32 idx, uint32 binding, NkTextureHandle tex) {
                    writes[idx].set=inst->mDescSet; writes[idx].binding=binding;
                    writes[idx].type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                    writes[idx].texture=tex; writes[idx].sampler=mLinearSampler;
                };
                fillTex(1, 3, albedoTex);
                fillTex(2, 4, slot4Tex);
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
            // D.1 : NoCull par defaut — meme raison que PBR pipeline (NkRender3D::EnsurePBRPipeline) :
            // les meshes primitifs (icosphere, etc.) n'ont pas de winding CCW garanti.
            pd.rasterizer = NkRasterizerDesc::NoCull();
            if (t.desc.cullMode == NkCullMode::NK_BACK)
                pd.rasterizer.cullMode = nkentseu::NkCullMode::NK_BACK;
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

            // Descriptor set layouts (doivent matcher NkRender3D) :
            //   set 0 = global (camera, lights, shadow, IBL) — fourni par SetSharedContext
            //   set 1 = per-object (model matrix, bones)    — fourni par SetSharedContext
            //   set 2 = per-instance (PBR UBO + textures)   — propre a NkMaterialSystem
            if (mSharedGlobalLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mSharedGlobalLayout);
            if (mSharedObjectLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mSharedObjectLayout);
            if (mInstDescLayout.IsValid())     pd.descriptorSetLayouts.PushBack(mInstDescLayout);

            // Vertex layout NkVertex3D — identical au layout de NkRender3D.
            // Reconstruit ici car stocker un NkVertexLayout (qui contient un NkVector)
            // comme membre crashe lors de l'operator= (allocateur null au moment de
            // SetSharedContext). La structure NkVertex3D est {pos12,normal12,tangent12,
            // uv8,uv28,color4} = 56 bytes, offsets 0/12/24/36/44/52.
            pd.vertexLayout
              .AddBinding(0, sizeof(NkVertex3D), false)
              .AddAttribute(0, 0, NkVertexFormat::NK_RGB32_FLOAT, 0,  "POSITION", 0)
              .AddAttribute(1, 0, NkVertexFormat::NK_RGB32_FLOAT, 12, "NORMAL",   0)
              .AddAttribute(2, 0, NkVertexFormat::NK_RGB32_FLOAT, 24, "TANGENT",  0)
              .AddAttribute(3, 0, NkVertexFormat::NK_RG32_FLOAT,  36, "TEXCOORD", 0)
              .AddAttribute(4, 0, NkVertexFormat::NK_RG32_FLOAT,  44, "TEXCOORD", 1)
              .AddAttribute(5, 0, NkVertexFormat::NK_RGBA8_UNORM, 52, "COLOR",    0);

            // Render pass (Vulkan exige la compat RP a la creation du pipeline).
            // Sur OpenGL mCurrentRP est invalide, le backend GL ignore ce champ.
            pd.renderPass = mCurrentRP;

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
        // M.4 : chaque setter marque overridé (si enfant) + propage aux enfants
        // via MarkPBRChanged / MarkToonChanged.
        NkMaterialInstance* NkMaterialInstance::SetAlbedo(NkVec3f c, float32 a) {
            mPBR.albedo={c.x,c.y,c.z,a};
            mToon.albedoColor={c.x,c.y,c.z,a};
            MarkPBRChanged (NK_PBR_O_ALBEDO);
            MarkToonChanged(NK_TOON_O_ALBEDO);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetMetallic(float32 v) {
            mPBR.metallic=v;
            mToon.metallic=v;   // teinte spec Toon/Anime : 0=blanc, 1=albedo
            MarkPBRChanged (NK_PBR_O_METALLIC);
            // Pas de bit Toon dédié pour metallic (partage de la valeur — accept
            // que le metallic Toon suit le PBR via le meme setter, override
            // groupe sur le bit PBR_METALLIC seul).
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetRoughness(float32 v) {
            mPBR.roughness=v; MarkPBRChanged(NK_PBR_O_ROUGHNESS); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetEmissive(NkVec3f c, float32 str) {
            mPBR.emissive={c.x,c.y,c.z,1}; mPBR.emissiveStrength=str;
            MarkPBRChanged(NK_PBR_O_EMISSIVE); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetSubsurface(float32 v, NkVec3f c) {
            mPBR.subsurface=v; mPBR.subsurfaceColor={c.x,c.y,c.z,1};
            MarkPBRChanged(NK_PBR_O_SUBSURFACE); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetClearcoat(float32 v, float32 r) {
            mPBR.clearcoat=v; mPBR.clearcoatRough=r;
            MarkPBRChanged(NK_PBR_O_CLEARCOAT); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetToonThreshold(float32 v) {
            mToon.shadowThreshold=v; MarkToonChanged(NK_TOON_O_SHADOW_TH); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetToonSmooth(float32 v) {
            mToon.shadowSmooth=v; MarkToonChanged(NK_TOON_O_SHADOW_SMOOTH); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetToonShadowColor(NkVec3f c) {
            mToon.shadowColor={c.x,c.y,c.z,1}; MarkToonChanged(NK_TOON_O_SHADOW_COLOR); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetOutline(float32 w, NkVec3f c) {
            mToon.outlineWidth=w; mToon.outlineColor={c.x,c.y,c.z,1};
            MarkToonChanged(NK_TOON_O_OUTLINE); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetRim(float32 intensity, NkVec3f c) {
            mToon.rimIntensity=intensity; mToon.rimColor={c.x,c.y,c.z,1};
            MarkToonChanged(NK_TOON_O_RIM); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetSpecHardness(float32 v) {
            mToon.specHardness=v; MarkToonChanged(NK_TOON_O_SPEC); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetMatcapMap(NkTexHandle t) {
            return SetTexture("matcap", t);
        }
        NkMaterialInstance* NkMaterialInstance::SetMatcapStrength(float32 v) {
            mToon.matcapStrength=v; MarkToonChanged(NK_TOON_O_MATCAP); return this;
        }

        NkMaterialInstance* NkMaterialInstance::SetReflFloorFaceMode(int32 mode) {
            mPBR.reflFloorFaceMode = (float32)mode;
            MarkPBRChanged(NK_PBR_O_REFL_FLOOR);
            return this;
        }

        // ── M.1 v0 : Layered material setters ───────────────────────────────────
        // Layered est traite en bloc (un seul bit mLayeredOverridden).
        NkMaterialInstance* NkMaterialInstance::SetLayerBase(const NkPBRParams& p) {
            mLayered.base = p; MarkLayeredChanged(); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetLayerTop(const NkPBRParams& p) {
            mLayered.top = p; MarkLayeredChanged(); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetLayerMaskSource(int32 src) {
            mLayered.maskSource = src; MarkLayeredChanged(); return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetLayered(const NkLayeredParams& l) {
            mLayered = l; MarkLayeredChanged(); return this;
        }

        // ── M.1 v1 : N=8 layers setters ─────────────────────────────────────────
        NkMaterialInstance* NkMaterialInstance::SetLayeredV1(const NkLayeredV1Params& l) {
            mLayeredV1 = l;
            mDirty = true;
            // Pas de propagation M.4 fine sur LayeredV1 v0 — copie en bloc.
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetLayerV1(int32 idx, const NkPBRLayer& layer) {
            if (idx < 0 || idx >= 8) return this;
            mLayeredV1.layers[idx] = layer;
            mDirty = true;
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetLayerV1Mask(int32 idx,
                                                                 NkLayerMaskSource src,
                                                                 float32 k) {
            if (idx < 0 || idx >= 8) return this;
            if (idx < 4) mLayeredV1.maskSources0[idx]     = (int32)src;
            else         mLayeredV1.maskSources1[idx - 4] = (int32)src;
            if (idx < 4) mLayeredV1.maskConstants0[idx]     = k;
            else         mLayeredV1.maskConstants1[idx - 4] = k;
            mDirty = true;
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetLayerV1Count(int32 n) {
            mLayeredV1.numLayers = (n < 1) ? 1 : (n > 8 ? 8 : n);
            mDirty = true;
            return this;
        }

        NkMaterialInstance* NkMaterialInstance::SetTexture(const NkString& n, NkTexHandle t) {
            for (auto& p:mParams)
                if(p.name==n && p.kind==Param::Kind::TEX) {
                    p.tex=t;
                    MarkNamedChanged(n);
                    return this;
                }
            Param p;
            p.name=n;
            p.kind=Param::Kind::TEX;
            p.tex=t;
            mParams.PushBack(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetAlbedoMap  (NkTexHandle t){return SetTexture("albedo",t);}
        NkMaterialInstance* NkMaterialInstance::SetNormalMap  (NkTexHandle t,float32 s){
            mPBR.normalStrength=s;
            MarkPBRChanged(NK_PBR_O_NORMAL_STR);
            return SetTexture("normal",t);
        }
        NkMaterialInstance* NkMaterialInstance::SetORMMap     (NkTexHandle t){return SetTexture("orm",t);}
        NkMaterialInstance* NkMaterialInstance::SetEmissiveMap(NkTexHandle t){return SetTexture("emissive",t);}
        NkMaterialInstance* NkMaterialInstance::SetAOMap      (NkTexHandle t){return SetTexture("ao",t);}

        // Helper interne membre : dedup + assign d'un Param non-tex par
        // (name, kind). Retourne true si le Param existait deja.
        bool NkMaterialInstance::DedupNamedParam(const Param& src) {
            for (auto& p : mParams) {
                if (p.name == src.name && p.kind == src.kind) {
                    p = src;
                    return true;
                }
            }
            mParams.PushBack(src);
            return false;
        }

        NkMaterialInstance* NkMaterialInstance::SetFloat(const NkString& n,float32 v) {
            Param p; p.name=n; p.kind=Param::Kind::F; p.f=v;
            DedupNamedParam(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec3(const NkString& n,NkVec3f v){
            Param p; p.name=n; p.kind=Param::Kind::V3; p.v3=v;
            DedupNamedParam(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec4(const NkString& n,NkVec4f v){
            Param p; p.name=n; p.kind=Param::Kind::V4; p.v4=v;
            DedupNamedParam(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetVec2(const NkString& n,NkVec2f v){
            Param p; p.name=n; p.kind=Param::Kind::V2; p.v2=v;
            DedupNamedParam(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetInt(const NkString& n,int32 v){
            Param p; p.name=n; p.kind=Param::Kind::I; p.i=v;
            DedupNamedParam(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetBool(const NkString& n,bool v){
            Param p; p.name=n; p.kind=Param::Kind::B; p.b=v;
            DedupNamedParam(p);
            MarkNamedChanged(n);
            return this;
        }
        NkMaterialInstance* NkMaterialInstance::SetColor(const NkString& n,NkVec4f c){return SetVec4(n,c);}

        NkRenderQueue NkMaterialInstance::GetQueue() const {
            return NkRenderQueue::NK_OPAQUE;
        }

    } // namespace renderer
} // namespace nkentseu
