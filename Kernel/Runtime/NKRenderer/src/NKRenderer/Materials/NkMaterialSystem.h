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
            // Roadmap : extension a N=8 layers + shader generation dynamique.
            NK_LAYERED=80,
            // Custom
            NK_CUSTOM=100,
        };

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
            float32 _pad[2]           = {};
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

                // M.1 v0 : Layered material setters
                NkMaterialInstance* SetLayerBase     (const NkPBRParams& p);
                NkMaterialInstance* SetLayerTop      (const NkPBRParams& p);
                NkMaterialInstance* SetLayerMaskSource(int32 src); // 0=R 1=G 2=B 3=A
                NkMaterialInstance* SetLayered       (const NkLayeredParams& l);

                NkMatHandle       GetTemplate() const { return mTemplate; }
                NkRenderQueue     GetQueue()    const;
                bool              IsDirty()     const { return mDirty; }
                void              MarkClean()         { mDirty=false; }
                NkDescSetHandle   GetDescSet()  const { return mDescSet; }
                const NkPBRParams&  GetPBR()    const { return mPBR; }
                const NkToonParams& GetToon()   const { return mToon; }
                const NkLayeredParams& GetLayered() const { return mLayered; }

            private:
                friend class NkMaterialSystem;
                NkMatInstHandle mHandle;
                NkMatHandle     mTemplate;
                NkDescSetHandle mDescSet;
                NkBufferHandle  mUBO;
                bool            mDirty = true;
                NkPBRParams     mPBR;
                NkToonParams    mToon;
                NkLayeredParams mLayered;   // M.1 v0 : utilise si type == NK_LAYERED
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
                NkMatHandle mTmplLayered;  // M.1 v0

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
