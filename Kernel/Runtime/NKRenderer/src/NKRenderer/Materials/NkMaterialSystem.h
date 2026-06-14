#pragma once
// =============================================================================
// NkMaterialSystem.h  — NKRenderer v4.0  (Materials/)
// Template + instance, catalogue PBR/NPR/Debug/Custom.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
    namespace renderer {

        // =========================================================================
        // Type de matériau
        // =========================================================================
        enum class NkMaterialType : uint16 {
            // Réaliste
            NK_PBR_METALLIC=0, NK_PBR_SPECULAR, NK_ARCHIVIZ,
            NK_SKIN, NK_HAIR, NK_GLASS, NK_CLOTH, NK_CAR_PAINT,
            NK_FOLIAGE, NK_WATER, NK_TERRAIN, NK_EMISSIVE, NK_VOLUME,
            NK_REFL_FLOOR,     // sol réfléchissant planaire (screen-space RT lookup)
            // Stylisé / NPR
            NK_TOON=20, NK_TOON_INK, NK_ANIME, NK_WATERCOLOR,
            NK_SKETCH, NK_PIXEL_ART, NK_FLAT, NK_UPBGE_EEVEE,
            // Debug
            NK_UNLIT=60, NK_DEBUG_NORMALS, NK_DEBUG_UV,
            NK_WIREFRAME_MAT, NK_DEBUG_DEPTH, NK_DEBUG_AO,
            // M.1 Material Layering — N couches PBR superposees avec masques.
            // v0 = 2 layers (base + top), masque via vertex color.R.
            // v1 = 8 layers (PBR simplifie : albedo+metallic+roughness) avec
            //      sources de masque variees (vColor.rgba, vUV.xy, constant,
            //      layer.albedo.a). Voir NkLayeredV1Params.
            NK_LAYERED=80,
            NK_LAYERED_V1=81,
            // Custom
            NK_CUSTOM=100,
            // Phase E : Materials 2D — meme NkMaterialSystem, vertex layout
            // different (Vert2D : pos+uv+color+flags). Templates utilises par
            // NkRender2D::DrawSpriteMat. Pas de CamUBO/ObjectUBO 3D ; au lieu :
            // push constant ortho matrix + set=2 material UBO + set=0 tex.
            NK_SPRITE_2D=120,
            NK_GLOW_2D=121,
        };

        // Helper : type 2D ou 3D ? Determine quel vertex layout / pipeline
        // layout utiliser dans NkMaterialSystem::CompilePipeline.
        inline bool NkMaterialIsType2D(NkMaterialType t) {
            return (uint16)t >= 120 && (uint16)t < 200;
        }

        // (NkRenderQueue defini dans Core/NkRendererTypes.h)

        enum class NkCullMode : uint8 { NK_BACK=0, NK_FRONT, NK_NONE };
        enum class NkFillMode : uint8 { NK_SOLID=0, NK_WIREFRAME };

        // =========================================================================
        // Paramètres GPU (std140)
        // =========================================================================
        struct alignas(16) NkPBRParams {
            NkVec4f albedo            = {1,1,1,1};
            NkVec4f emissive          = {0,0,0,0};
            float32 metallic          = 0.f;
            float32 roughness         = 0.5f;
            float32 ao                = 1.f;
            float32 emissiveStrength  = 0.f;
            float32 normalStrength    = 1.f;
            float32 clearcoat         = 0.f;
            float32 clearcoatRough    = 0.f;
            float32 subsurface        = 0.f;
            NkVec4f subsurfaceColor   = {1.f,0.5f,0.3f,1.f};
            float32 anisotropy        = 0.f;
            float32 sheen             = 0.f;
            // Champ specifique au sol miroir (NkPlanarReflectionSystem) :
            //   0.0 = FRONT_ONLY (face arriere = discard)
            //   1.0 = BACK_ONLY  (face avant   = discard)
            //   2.0 = BOTH       (les deux faces avec leur RT respectif)
            // Reutilise _pad[0] pour eviter de casser le layout std140 du UBO PBR
            // (96 bytes total). Ignore par les autres shaders PBR/Toon/Anime.
            float32 reflFloorFaceMode = 0.f;
            float32 _pad              = 0.f;
        };

        // M.1 v0 : 2 layers PBR superposes avec masque vertex-color.
        // Layout std140 doit matcher EXACTEMENT le UBO LayeredUBO du shader
        // layered.frag.vk.glsl (set=2 binding=8). Taille = 2 x 96 + 16 = 208 B.
        struct alignas(16) NkLayeredParams {
            NkPBRParams base;       // layer 0 (mask=0)
            NkPBRParams top;        // layer 1 (mask=1)
            // Source du masque dans vColor : 0=R, 1=G, 2=B, 3=A.
            // Roadmap v1 : 4=texture (necessitera un slot tMask supplementaire).
            int32       maskSource    = 0;
            int32       _pad[3]       = {};
        };

        // M.1 v1 : 8 layers PBR-simplifies (albedo+metallic+roughness) avec
        // masks parametriques. Sources de masque possibles (NkLayerMaskSource) :
        //   0=vColor.r 1=.g 2=.b 3=.a 4=vUV.x 5=vUV.y 6=constant 7=layer.albedo.a
        // Layout std140 doit matcher LayeredV1UBO du shader layeredv1.frag.vk.glsl.
        struct alignas(16) NkPBRLayer {
            NkVec4f albedo    = {1.f, 1.f, 1.f, 1.f};   // A : usage masque interne
            float32 metallic  = 0.f;
            float32 roughness = 0.5f;
            float32 _pad0     = 0.f;
            float32 _pad1     = 0.f;
        };  // 32 bytes

        enum NkLayerMaskSource : int32 {
            NK_LAYER_MASK_VCOLOR_R    = 0,
            NK_LAYER_MASK_VCOLOR_G    = 1,
            NK_LAYER_MASK_VCOLOR_B    = 2,
            NK_LAYER_MASK_VCOLOR_A    = 3,
            NK_LAYER_MASK_UV_X        = 4,
            NK_LAYER_MASK_UV_Y        = 5,
            NK_LAYER_MASK_CONSTANT    = 6,
            NK_LAYER_MASK_LAYER_ALPHA = 7,
        };

        // Total : 8*32 + 4*16 + 16 = 336 bytes. Bien sous 16 KiB UBO limit.
        struct alignas(16) NkLayeredV1Params {
            NkPBRLayer layers[8];
            // Packed en ivec4/vec4 pour respecter std140 (arrays scalaires
            // gaspilleraient 12 bytes par element).
            int32   maskSources0[4]  = {0,0,0,0};   // layers 0..3 (0 ignore)
            int32   maskSources1[4]  = {0,0,0,0};   // layers 4..7
            float32 maskConstants0[4]= {0,0,0,0};   // si source==CONSTANT
            float32 maskConstants1[4]= {0,0,0,0};
            int32   numLayers        = 1;
            int32   _pad[3]          = {};
        };

        struct alignas(16) NkToonParams {
            NkVec4f  albedoColor      = {1.f,1.f,1.f,1.f};  // couleur de base (SetAlbedo)
            NkVec4f  shadowColor      = {0.2f,0.1f,0.3f,1.f};
            float32  shadowThreshold  = 0.3f;
            float32  shadowSmooth     = 0.05f;
            float32  outlineWidth     = 2.f;
            float32  rimIntensity     = 0.5f;
            NkVec4f  outlineColor     = {0,0,0,1};
            NkVec4f  rimColor         = {1,1,1,1};
            float32  specHardness     = 32.f;
            float32  metallic         = 0.f;   // teinte spéculaire : 0=blanc, 1=albedo (effet métal cel)
            float32  matcapStrength   = 0.f;   // 0=aucun, 1=full matcap (binding=4)
            float32  _pad[1]          = {};
        };

        // =========================================================================
        // Descripteur template
        // =========================================================================
        struct NkMaterialTemplateDesc {
            NkMaterialType  type        = NkMaterialType::NK_PBR_METALLIC;
            NkRenderQueue   queue       = NkRenderQueue::NK_OPAQUE;
            NkBlendMode     blendMode   = NkBlendMode::NK_OPAQUE;
            NkCullMode      cullMode    = NkCullMode::NK_BACK;
            NkFillMode      fillMode    = NkFillMode::NK_SOLID;
            bool            depthWrite  = true;
            bool            depthTest   = true;
            bool            doubleSided = false;
            NkString        name;
            // Custom shaders (si type == NK_CUSTOM)
            NkString vertSrcGL, fragSrcGL;
            NkString vertSrcVK, fragSrcVK;
            NkString vertSrcDX11, fragSrcDX11;
            NkString vertSrcDX12, fragSrcDX12;
            NkString vertSrcMSL,  fragSrcMSL;
            NkString nkslSource;
        };

        // =========================================================================
        // M.4 Hierarchical Instances — bitfields des champs overridés
        // =========================================================================
        // Quand un enfant override un champ, le bit correspondant est set dans
        // mPBROverrideMask / mToonOverrideMask. Les setters parents propagent
        // alors uniquement aux enfants dont le bit n'est PAS set (live link).
        // Pour les params nommés (SetFloat/SetVec/SetTexture), on utilise un
        // mOverrideParamNames (linear scan, set petit).
        enum NkPBROverrideBit : uint32 {
            NK_PBR_O_ALBEDO       = 1u << 0,
            NK_PBR_O_EMISSIVE     = 1u << 1,
            NK_PBR_O_METALLIC     = 1u << 2,
            NK_PBR_O_ROUGHNESS    = 1u << 3,
            NK_PBR_O_AO           = 1u << 4,
            NK_PBR_O_NORMAL_STR   = 1u << 5,
            NK_PBR_O_CLEARCOAT    = 1u << 6,
            NK_PBR_O_SUBSURFACE   = 1u << 7,
            NK_PBR_O_ANISOTROPY   = 1u << 8,
            NK_PBR_O_SHEEN        = 1u << 9,
            NK_PBR_O_REFL_FLOOR   = 1u << 10,
        };

        enum NkToonOverrideBit : uint32 {
            NK_TOON_O_ALBEDO        = 1u << 0,
            NK_TOON_O_SHADOW_TH     = 1u << 1,
            NK_TOON_O_SHADOW_SMOOTH = 1u << 2,
            NK_TOON_O_SHADOW_COLOR  = 1u << 3,
            NK_TOON_O_OUTLINE       = 1u << 4,
            NK_TOON_O_RIM           = 1u << 5,
            NK_TOON_O_SPEC          = 1u << 6,
            NK_TOON_O_MATCAP        = 1u << 7,
        };

        // =========================================================================
        // NkMaterialInstance
        // =========================================================================
        class NkMaterialSystem;

        class NkMaterialInstance {
            public:
                NkMatInstHandle     GetHandle()  const { return mHandle; }
                NkMaterialInstance* SetAlbedo        (NkVec3f c, float32 a=1.f);
                NkMaterialInstance* SetAlbedoMap     (NkTexHandle t);
                NkMaterialInstance* SetNormalMap     (NkTexHandle t, float32 str=1.f);
                NkMaterialInstance* SetORMMap        (NkTexHandle t);
                NkMaterialInstance* SetMetallic      (float32 v);
                NkMaterialInstance* SetRoughness     (float32 v);
                NkMaterialInstance* SetEmissive      (NkVec3f c, float32 str=1.f);
                NkMaterialInstance* SetEmissiveMap   (NkTexHandle t);
                NkMaterialInstance* SetAOMap         (NkTexHandle t);
                NkMaterialInstance* SetSubsurface    (float32 v, NkVec3f color);
                NkMaterialInstance* SetClearcoat     (float32 v, float32 rough);
                NkMaterialInstance* SetToonThreshold (float32 v);
                NkMaterialInstance* SetToonSmooth    (float32 v);
                NkMaterialInstance* SetToonShadowColor(NkVec3f c);
                NkMaterialInstance* SetOutline       (float32 w, NkVec3f color);
                NkMaterialInstance* SetRim           (float32 intensity, NkVec3f color);
                NkMaterialInstance* SetSpecHardness  (float32 v);
                NkMaterialInstance* SetMatcapMap     (NkTexHandle t);
                NkMaterialInstance* SetMatcapStrength(float32 v);
                NkMaterialInstance* SetFloat  (const NkString& n, float32 v);
                NkMaterialInstance* SetVec2   (const NkString& n, NkVec2f v);
                NkMaterialInstance* SetVec3   (const NkString& n, NkVec3f v);
                NkMaterialInstance* SetVec4   (const NkString& n, NkVec4f v);
                NkMaterialInstance* SetColor  (const NkString& n, NkVec4f c);
                NkMaterialInstance* SetInt    (const NkString& n, int32 v);
                NkMaterialInstance* SetBool   (const NkString& n, bool v);
                NkMaterialInstance* SetTexture(const NkString& n, NkTexHandle t);

                // Sol miroir : mode d'affichage des faces (0=FrontOnly, 1=BackOnly,
                // 2=Both). Lu par le shader ReflFloor via uFloor.reflFloorFaceMode.
                NkMaterialInstance* SetReflFloorFaceMode(int32 mode);

                // M.1 v0 : Layered material setters
                NkMaterialInstance* SetLayerBase     (const NkPBRParams& p);
                NkMaterialInstance* SetLayerTop      (const NkPBRParams& p);
                NkMaterialInstance* SetLayerMaskSource(int32 src); // 0=R 1=G 2=B 3=A
                NkMaterialInstance* SetLayered       (const NkLayeredParams& l);

                // M.1 v1 : Layered N=8 setters
                NkMaterialInstance* SetLayeredV1     (const NkLayeredV1Params& l);
                NkMaterialInstance* SetLayerV1       (int32 idx, const NkPBRLayer& layer);
                NkMaterialInstance* SetLayerV1Mask   (int32 idx, NkLayerMaskSource src, float32 k = 0.f);
                NkMaterialInstance* SetLayerV1Count  (int32 n);

                NkMatHandle       GetTemplate() const { return mTemplate; }
                NkRenderQueue     GetQueue()    const;
                bool              IsDirty()     const { return mDirty; }
                void              MarkClean()         { mDirty=false; }
                NkDescSetHandle   GetDescSet()  const { return mDescSet; }
                const NkPBRParams&  GetPBR()    const { return mPBR; }
                const NkToonParams& GetToon()   const { return mToon; }
                const NkLayeredParams& GetLayered() const { return mLayered; }
                const NkLayeredV1Params& GetLayeredV1() const { return mLayeredV1; }

                // ── M.4 Hierarchical Instances ───────────────────────────────────
                NkMaterialInstance* GetParent() const { return mParent; }
                uint32 GetChildCount() const { return (uint32)mChildren.Size(); }

                // Retire l'override sur un champ PBR/Toon (re-link au parent pour
                // ce champ). No-op si pas de parent ou si bit non set.
                NkMaterialInstance* ResetPBROverride (NkPBROverrideBit bit);
                NkMaterialInstance* ResetToonOverride(NkToonOverrideBit bit);
                // Retire l'override sur un param nomme (Float/Vec/Tex/etc.). Le param
                // est synchronise depuis le parent au prochain BindInstance.
                NkMaterialInstance* ResetNamedOverride(const NkString& name);

            private:
                friend class NkMaterialSystem;
                friend class NkMaterial;       // setters/getters shadow override + autres
                friend class NkRender3D;       // lit mReceiveShadow/mShadowBiasMul

                // M.4 : propage un changement d'un champ PBR aux enfants non-overrides
                // (recursivement). Le bit indique quel champ a change ; les enfants
                // qui n'ont PAS marque ce bit comme override resyncent leur valeur
                // depuis ce parent.
                void PropagatePBRField (uint32 bit);
                void PropagateToonField(uint32 bit);
                // Resync TOUT le mLayered (les Layered sont consideres en bloc).
                void PropagateLayered();
                // Resync un param nomme aux enfants non-overrides (par nom).
                void PropagateNamedParam(const NkString& name);
                // Helper : check si un param nomme est dans la liste d'overrides
                bool IsNamedOverridden(const NkString& name) const;
                void AddNamedOverride (const NkString& name);
                // Helpers unifies appeles par les setters : set local + marque
                // override si on est un enfant + propage aux enfants.
                void MarkPBRChanged   (uint32 bit);
                void MarkToonChanged  (uint32 bit);
                void MarkLayeredChanged();
                void MarkNamedChanged (const NkString& name);
                // Helper interne : copie un param nomme du parent vers ce param
                // (utilise pour syncFromParent).
                void CopyNamedFromParent(const NkString& name);
                // Helper : dedup + assign d'un Param non-tex par (name, kind)
                struct Param;
                bool DedupNamedParam(const Param& src);

                NkMatInstHandle mHandle;
                NkMatHandle     mTemplate;
                NkDescSetHandle mDescSet;
                NkBufferHandle  mUBO;
                bool            mDirty = true;
                NkPBRParams     mPBR;
                NkToonParams    mToon;
                NkLayeredParams mLayered;     // M.1 v0 : utilise si type == NK_LAYERED
                NkLayeredV1Params mLayeredV1; // M.1 v1 : utilise si type == NK_LAYERED_V1
                // NkVSM v1 (2026-05-23) : overrides shadow per-material.
                // Lus par NkRender3D pour propager dans ObjBlock.shadowOverrides.
                // Pas dans le UBO PBR (qui est plein std140 96B) pour eviter
                // un refactor cross-shader. Public via friend class NkMaterial
                // qui expose les setters/getters.
                bool    mReceiveShadow        = true;
                bool    mCastShadowAlphaTest  = false;
                float32 mShadowBiasMul        = 1.0f;

                // 2026-05-24 : Triplanar projection (style UE5 World Aligned
                // Texture). tileSize en metres reels (converti en units via
                // metersPerUnit dans le shader). 0 = disabled, UV classique
                // depuis le mesh est utilise (compat). Lu par NkRender3D pour
                // propager dans ObjBlock.triplanarParams.
                float32 mTriplanarTileSize    = 0.0f;
                struct Param {
                    NkString name;
                    enum class Kind:uint8{F,V2,V3,V4,I,B,TEX} kind = Kind::F;
                    union {
                        float32 f = 0.f;
                        math::NkVec2f v2;
                        math::NkVec3f v3;
                        math::NkVec4f v4;
                        int32 i;
                        bool b;
                    };
                    NkTexHandle tex;
                };
                NkVector<Param> mParams;

                // M.4 Hierarchical Instances
                NkMaterialInstance*              mParent = nullptr;
                NkVector<NkMaterialInstance*>    mChildren;
                uint32                           mPBROverrideMask  = 0;
                uint32                           mToonOverrideMask = 0;
                bool                             mLayeredOverridden = false;
                NkVector<NkString>               mOverrideParamNames;
        };

        // =========================================================================
        // NkMaterialSystem
        // =========================================================================
        class NkMaterialSystem {
            public:
                NkMaterialSystem() = default;
                ~NkMaterialSystem();

                bool Init(NkIDevice* device, NkTextureLibrary* texLib,
                          NkShaderLibrary* shaderLib, NkGraphicsApi api);
                void Shutdown();

                NkMatHandle        RegisterTemplate(const NkMaterialTemplateDesc& desc);
                NkMatHandle        FindTemplate    (const NkString& name) const;

                NkMaterialInstance* CreateInstance (NkMatHandle tmpl);
                // M.4 Hierarchical Instances : cree un enfant heritant du parent.
                // L'enfant utilise le meme template + descSet/UBO independants, et
                // copie les params initiaux du parent. Tout setter sur l'enfant
                // marque l'override sur le champ touche ; tout setter sur le parent
                // propage aux enfants dont le champ n'est PAS overridé.
                NkMaterialInstance* CreateChildInstance(NkMaterialInstance* parent);
                void                DestroyInstance(NkMaterialInstance*& inst);

                // Fournit les layouts partagés de NkRender3D (set 0 + set 1 + vertex layout).
                // Doit être appelé après Init de NkRender3D, avant tout BindInstance.
                // Le renderPass peut être mis à jour plus tard via UpdateRenderPass()
                // (nécessaire sur Vulkan quand le RP change au resize).
                void SetSharedContext(NkDescSetHandle globalLayout,
                                      NkDescSetHandle objectLayout,
                                      const NkVertexLayout& vertexLayout,
                                      NkRenderPassHandle rp = {});
                void UpdateRenderPass(NkRenderPassHandle rp);

                // Bind avant draw (met à jour descset si dirty, utilise mTexLib interne).
                bool BindInstance(NkICommandBuffer* cmd, NkMaterialInstance* inst);

                // Accès instance depuis handle (draw call)
                NkMaterialInstance* GetInstance(NkMatInstHandle h) const;

                // Accès pipeline du template (lazy compile).
                // currentRP : render pass courant (pour Vulkan RP compat).
                NkPipelineHandle    GetPipeline(NkMatHandle tmpl,
                                                NkRenderPassHandle currentRP = {});

                void FlushCompilations();

                // Accès interne utilisé par NkMaterial
                const NkString*    GetTemplateName(NkMatHandle h) const;
                NkMaterialType     GetTemplateType(NkMatHandle h) const;

                // Templates built-in
                NkMatHandle DefaultPBR()       const { return mTmplPBR; }
                NkMatHandle DefaultToon()      const { return mTmplToon; }
                NkMatHandle DefaultUnlit()     const { return mTmplUnlit; }
                NkMatHandle DefaultWireframe() const { return mTmplWire; }
                NkMatHandle DefaultLayeredV1() const { return mTmplLayeredV1; }
                NkMatHandle DefaultSkin()      const { return mTmplSkin; }
                NkMatHandle DefaultHair()      const { return mTmplHair; }
                NkMatHandle DefaultAnime()     const { return mTmplAnime; }
                NkMatHandle DefaultArchviz()   const { return mTmplArchviz; }
                NkMatHandle DefaultReflFloor() const { return mTmplReflFloor; }
                NkMatHandle DefaultLayered()   const { return mTmplLayered; } // M.1

                // Phase G : NkMaterialLibrary (sous-systeme integre).
                // Charge / cache / hot-reload des materiaux .nkasset.
                // Initialise par NkRendererImpl apres NkMaterialSystem::Init.
                class NkMaterialLibrary* GetLibrary() const { return mLibrary; }
                void SetLibrary(class NkMaterialLibrary* lib) { mLibrary = lib; }

                // Layout du descriptor set per-instance materiau (set=2). Expose
                // pour que les pipelines crees hors NkMaterialSystem (PBR canonical
                // dans NkRender3D notamment) puissent le declarer dans leur
                // descriptorSetLayouts[2] — sinon le shader sample des set=2 non
                // declares -> validation errors + comportement non defini.
                NkDescSetHandle GetInstanceLayout() const { return mInstDescLayout; }

            private:
                struct TemplateEntry {
                    NkMaterialTemplateDesc  desc;
                    NkPipelineHandle        pipeline;
                    ::nkentseu::NkShaderHandle shaderHandle; // RHI shader handle
                    bool                    compiled = false;
                };

                NkIDevice*          mDevice     = nullptr;
                NkTextureLibrary*   mTexLib     = nullptr;
                NkShaderLibrary*    mShaderLib  = nullptr;
                NkGraphicsApi       mApi        = NkGraphicsApi::NK_GFX_API_VULKAN;
                NkHashMap<uint64, TemplateEntry>        mTemplates;
                NkVector<NkMaterialInstance*>           mInstances;
                NkHashMap<uint64, NkMaterialInstance*>  mInstanceMap;
                uint64                             mNextId     = 1;
                uint64                             mNextInstId = 1;
                NkDescSetHandle                    mInstDescLayout;
                NkSamplerHandle                    mLinearSampler;
                // Contexte partagé fourni par NkRender3D (layouts set 0/1, RP).
                // Le vertex layout NkVertex3D est reconstruit directement dans
                // CompilePipeline() pour eviter la copie d'un NkVector dont
                // l'allocateur n'est pas initialise dans le membre par defaut.
                NkDescSetHandle    mSharedGlobalLayout;
                NkDescSetHandle    mSharedObjectLayout;
                NkRenderPassHandle mCurrentRP;

                NkMatHandle mTmplPBR, mTmplToon, mTmplUnlit, mTmplWire;
                NkMatHandle mTmplSkin, mTmplHair, mTmplAnime, mTmplArchviz;
                NkMatHandle mTmplReflFloor;
                NkMatHandle mTmplLayered;     // M.1 v0
                NkMatHandle mTmplLayeredV1;   // M.1 v1 (N=8 layers)

                // Pipelines orphelins par UpdateRenderPass : detruits au Shutdown.
                // Cf. UpdateRenderPass — DestroyPipeline immediat invalide les
                // cmd buffers en cours de recording.
                NkVector<NkPipelineHandle> mPendingDestroy;

                // Phase G : sous-systeme NkMaterialLibrary (pointe non-owning).
                // Cree et detruit par NkRendererImpl.
                class NkMaterialLibrary* mLibrary = nullptr;

                void RegisterBuiltins();
                NkPipelineHandle CompilePipeline(TemplateEntry& t);
        };

    } // namespace renderer
} // namespace nkentseu
