#pragma once
// =============================================================================
// NkMaterialSystem.h
// Système de matériaux haut-niveau au-dessus de NKRHI.
//
// Modèles supportés :
//   NK_UNLIT         — couleur/texture pure, pas d'éclairage
//   NK_PHONG         — éclairage Phong classique
//   NK_PBR           — PBR Metallic-Roughness (standard)
//   NK_PBR_SPECULAR  — PBR Specular-Glossiness (Archviz)
//   NK_TOON          — Cel-shading configurable
//   NK_SKIN          — Subsurface scattering peau
//   NK_HAIR          — Rendu cheveux anisotrope
//   NK_GLASS         — Transmission + réfraction
//   NK_WATER         — Eau animée + réflexion/réfraction
//   NK_TERRAIN       — Multi-couche avec displacement
//   NK_PARTICLE      — Particules billboard
//   NK_CUSTOM        — Shader fourni par l'utilisateur
//
// Architecture :
//   NkMaterialSystem
//   ├── NkMaterialTemplate  — pipeline compilé + layout
//   └── NkMaterialInstance  — descriptor set + paramètres
//
// Exemple :
//   auto* mat = system.CreatePBR("BrickWall");
//   mat->SetAlbedoMap(albedoTex)
//      ->SetNormalMap(normalTex)
//      ->SetRoughness(0.8f)
//      ->SetMetallic(0.0f);
//   mat->SetupForDevice(device);
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkResourceManager.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Modèle de matériau
        // =============================================================================
        enum class NkMaterialModel : uint32 {
            NK_UNLIT = 0,
            NK_PHONG,
            NK_PBR,
            NK_PBR_SPECULAR,
            NK_TOON,
            NK_SKIN,
            NK_HAIR,
            NK_GLASS,
            NK_WATER,
            NK_TERRAIN,
            NK_PARTICLE,
            NK_CUSTOM,
        };

        // =============================================================================
        // Domaine d'application
        // =============================================================================
        enum class NkMaterialDomain : uint32 {
            NK_SURFACE = 0,
            NK_DECAL,
            NK_POSTPROCESS,
            NK_UI,
            NK_PARTICLE,
        };

        // =============================================================================
        // Paramètre de matériau générique (pour NK_CUSTOM et les overrides)
        // =============================================================================
        struct NkMaterialParam {
            NkString name;
            enum class Type : uint8 {
                F1, F2, F3, F4, I1, I2, I3, I4, MAT4, BOOL, TEX
            } type = Type::F1;
            union { float32 f[16]; int32 i[4]; bool b; } val{};
            NkTextureHandle texture;

            static NkMaterialParam Float (const char* n, float32 v)
                { NkMaterialParam p; p.name=n; p.type=Type::F1; p.val.f[0]=v; return p; }
            static NkMaterialParam Float3(const char* n, float32 x, float32 y, float32 z)
                { NkMaterialParam p; p.name=n; p.type=Type::F3; p.val.f[0]=x; p.val.f[1]=y; p.val.f[2]=z; return p; }
            static NkMaterialParam Float4(const char* n, float32 x, float32 y, float32 z, float32 w)
                { NkMaterialParam p; p.name=n; p.type=Type::F4; p.val.f[0]=x; p.val.f[1]=y; p.val.f[2]=z; p.val.f[3]=w; return p; }
            static NkMaterialParam Color (const char* n, const NkColorF& c)
                { return Float4(n, c.r, c.g, c.b, c.a); }
            static NkMaterialParam Bool  (const char* n, bool v)
                { NkMaterialParam p; p.name=n; p.type=Type::BOOL; p.val.b=v; return p; }
            static NkMaterialParam Tex   (const char* n, NkTextureHandle t)
                { NkMaterialParam p; p.name=n; p.type=Type::TEX; p.texture=t; return p; }
        };

        // =============================================================================
        // Paramètres PBR
        // =============================================================================
        struct NkPBRParams {
            NkColorF     baseColor        = NkColorF::White();
            NkTextureHandle albedoTex;
            NkTextureHandle normalTex;
            float32      normalScale      = 1.f;
            NkTextureHandle ormTex;              // Occlusion(R)+Roughness(G)+Metal(B)
            float32      metallic         = 0.f;
            float32      roughness        = 0.5f;
            float32      occlusion        = 1.f;
            NkColorF     emissiveColor    = NkColorF(0,0,0,0);
            NkTextureHandle emissiveTex;
            float32      emissiveScale    = 1.f;
            float32      alphaClip        = 0.f;
            bool         receiveShadow    = true;
            bool         castShadow       = true;
            float32      iblIntensity     = 1.f;
            bool         clearcoat        = false;
            float32      clearcoatFactor  = 0.f;
            float32      transmission     = 0.f;
            float32      ior              = 1.5f;
            NkTextureHandle heightTex;           // Parallax displacement
            float32      heightScale      = 0.f;
        };

        // =============================================================================
        // Paramètres Toon
        // =============================================================================
        struct NkToonParams {
            NkColorF     baseColor        = NkColorF::White();
            NkTextureHandle albedoTex;
            NkColorF     shadowColor      = NkColorF(.2f,.2f,.4f,1.f);
            NkColorF     outlineColor     = NkColorF::Black();
            float32      outlineWidth     = 0.02f;
            float32      shadowThreshold  = 0.5f;
            float32      shadowSmoothness = 0.02f;
            float32      specThreshold    = 0.8f;
            float32      specSmoothness   = 0.02f;
            uint32       shadeSteps       = 3;
            bool         receiveShadow    = true;
            bool         outlineEnabled   = true;
        };

        // =============================================================================
        // Paramètres Unlit
        // =============================================================================
        struct NkUnlitParams {
            NkColorF     baseColor        = NkColorF::White();
            NkTextureHandle albedoTex;
            NkTextureHandle emissiveTex;
            NkColorF     emissiveColor    = NkColorF(0,0,0,0);
            float32      alphaClip        = 0.f;
        };

        // =============================================================================
        // Descripteur d'un matériau
        // =============================================================================
        struct NkMaterialDesc {
            NkString           name;
            NkMaterialModel    model     = NkMaterialModel::NK_PBR;
            NkMaterialDomain   domain    = NkMaterialDomain::NK_SURFACE;

            // Pipeline state
            NkBlendMode        blendMode    = NkBlendMode::NK_OPAQUE;
            NkFillMode         fillMode     = NkFillMode::NK_SOLID;
            NkCullMode         cullMode     = NkCullMode::NK_BACK;
            NkFrontFace        frontFace    = NkFrontFace::NK_CCW;
            bool               depthTest    = true;
            bool               depthWrite   = true;
            NkDepthOp          depthOp      = NkDepthOp::NK_LESS;
            bool               twoSided     = false;
            float32            depthBiasConst = 0.f;
            float32            depthBiasSlope = 0.f;

            // Paramètres modèles
            NkUnlitParams   unlit;
            NkPBRParams     pbr;
            NkToonParams    toon;

            // Custom shader + params
            NkVector<NkMaterialParam> customParams;

            // Clé de tri pour le batching
            uint32 SortKey() const {
                uint32 k = 0;
                k |= ((uint32)domain)   << 24;
                k |= (blendMode != NkBlendMode::NK_OPAQUE ? 1u : 0u) << 23;
                k |= ((uint32)model)    << 16;
                k |= ((uint32)blendMode)<< 8;
                return k;
            }

            // ── Factory helpers ──────────────────────────────────────────────────────
            static NkMaterialDesc Unlit(const char* n = "Unlit") {
                NkMaterialDesc d; d.name = n; d.model = NkMaterialModel::NK_UNLIT;
                return d;
            }
            static NkMaterialDesc PBR(const char* n = "PBR") {
                NkMaterialDesc d; d.name = n; d.model = NkMaterialModel::NK_PBR;
                return d;
            }
            static NkMaterialDesc Toon(const char* n = "Toon") {
                NkMaterialDesc d; d.name = n; d.model = NkMaterialModel::NK_TOON;
                return d;
            }
            static NkMaterialDesc Wireframe(const char* n = "Wireframe") {
                NkMaterialDesc d; d.name = n; d.model = NkMaterialModel::NK_UNLIT;
                d.fillMode = NkFillMode::NK_WIREFRAME;
                d.cullMode = NkCullMode::NK_NONE;
                return d;
            }
            static NkMaterialDesc ShadowDepth(const char* n = "ShadowDepth") {
                NkMaterialDesc d; d.name = n; d.model = NkMaterialModel::NK_UNLIT;
                d.cullMode = NkCullMode::NK_FRONT;   // front-face culling shadow
                d.depthBiasConst = 1.25f;
                d.depthBiasSlope = 1.75f;
                return d;
            }
            static NkMaterialDesc Transparent(const char* n = "Transparent") {
                NkMaterialDesc d; d.name = n; d.model = NkMaterialModel::NK_PBR;
                d.blendMode = NkBlendMode::NK_ALPHA;
                d.depthWrite = false;
                return d;
            }
        };

        // =============================================================================
        // Descripteur d'instance de matériau
        // =============================================================================
        struct NkMaterialInstanceDesc {
            NkMaterialHandle           templateHandle;
            NkVector<NkMaterialParam>  overrides;
            const char*                debugName = nullptr;
        };

        // =============================================================================
        // NkMaterialInstance — wrapper haut-niveau pour modifier un matériau
        // Expose une API fluide pour modifier les paramètres.
        // =============================================================================
        class NkMaterialSystem;

        class NkMaterialInstance {
           public:
                NkMaterialInstHandle Handle() const { return mHandle; }
                bool IsValid()                const { return mHandle.IsValid(); }

                // API fluide pour configurer les paramètres
                NkMaterialInstance* SetAlbedo    (const NkColorF& c);
                NkMaterialInstance* SetAlbedoMap (NkTextureHandle tex);
                NkMaterialInstance* SetNormalMap (NkTextureHandle tex,
                                                    float32 scale = 1.f);
                NkMaterialInstance* SetORMMap    (NkTextureHandle tex);
                NkMaterialInstance* SetRoughness (float32 v);
                NkMaterialInstance* SetMetallic  (float32 v);
                NkMaterialInstance* SetEmissive  (const NkColorF& c,
                                                    float32 intensity = 1.f);
                NkMaterialInstance* SetEmissiveMap(NkTextureHandle tex);
                NkMaterialInstance* SetTexture   (const char* name,
                                                    NkTextureHandle tex);
                NkMaterialInstance* SetFloat     (const char* name, float32 v);
                NkMaterialInstance* SetColor     (const char* name,
                                                    const NkColorF& c);
                NkMaterialInstance* SetTwoSided  (bool v);
                NkMaterialInstance* SetOpacity   (float32 v);

           private:
                friend class NkMaterialSystem;

                NkMaterialInstHandle  mHandle;
                NkMaterialSystem*     mSystem = nullptr;
                NkMaterialDesc        mDesc;
        };

        // =============================================================================
        // NkMaterialSystem — gestionnaire principal des matériaux
        // =============================================================================
        class NkMaterialSystem {
           public:
                NkMaterialSystem()  = default;
                ~NkMaterialSystem() { Shutdown(); }

                bool Init    (NkIDevice* device, NkResourceManager* resources);
                void Shutdown();
                bool IsValid () const { return mDevice != nullptr; }

                // ── Création de matériaux templates ───────────────────────────────────────
                NkMaterialHandle CreateMaterial(const NkMaterialDesc& desc);

                // Templates enregistrés par nom (accessible globalement)
                NkMaterialHandle RegisterMaterial(const char* name,
                                                   const NkMaterialDesc& desc);
                NkMaterialHandle FindMaterial(const char* name) const;

                // ── Création d'instances ───────────────────────────────────────────────────
                NkMaterialInstance* CreateInstance(NkMaterialHandle tmpl);
                NkMaterialInstance* CreateInstance(const NkMaterialInstanceDesc& desc);
                void                DestroyInstance(NkMaterialInstance*& inst);

                // ── Matériaux par défaut (créés à l'init) ─────────────────────────────────
                NkMaterialHandle GetDefaultPBR()       const { return mDefaultPBR; }
                NkMaterialHandle GetDefaultUnlit()     const { return mDefaultUnlit; }
                NkMaterialHandle GetDefaultToon()      const { return mDefaultToon; }
                NkMaterialHandle GetDefaultWireframe() const { return mDefaultWireframe; }
                NkMaterialHandle GetShadowDepth()      const { return mShadowDepth; }

                // ── Flush (met à jour les descriptor sets marqués dirty) ──────────────────
                void FlushPendingUpdates();

                // ── Accès interne (pour les renderers) ───────────────────────────────────
                uint64 GetPipelineRHIId(NkMaterialHandle h) const;
                uint64 GetDescSetRHIId (NkMaterialInstHandle h) const;
                bool   IsMaterialDirty (NkMaterialInstHandle h) const;

           private:
                void CreateDefaultMaterials();
                bool CompileMaterialPipeline(uint64 entryId);

                struct TemplateEntry {
                    NkMaterialDesc  desc;
                    uint64          rhiPipelineId   = 0;
                    uint64          rhiDescLayoutId = 0;
                    NkString        name;
                    bool            compiled        = false;
                };

                struct InstanceEntry {
                    NkMaterialHandle              templateHandle;
                    NkVector<NkMaterialParam>     params;
                    uint64                        rhiDescSetId  = 0;
                    uint64                        rhiUBOId      = 0;
                    bool                          dirty         = true;
                    NkMaterialInstance*           wrapper       = nullptr;
                };

                NkIDevice*           mDevice    = nullptr;
                NkResourceManager*   mResources = nullptr;

                NkUnorderedMap<uint64, TemplateEntry>  mTemplates;
                NkUnorderedMap<uint64, InstanceEntry>  mInstances;
                NkUnorderedMap<NkString, NkMaterialHandle> mNamedMaterials;

                NkMaterialHandle mDefaultPBR;
                NkMaterialHandle mDefaultUnlit;
                NkMaterialHandle mDefaultToon;
                NkMaterialHandle mDefaultWireframe;
                NkMaterialHandle mShadowDepth;

                NkAtomic<uint64> mNextId{0};
                uint64 NextId() { return ++mNextId; }
        };

    } // namespace renderer
} // namespace nkentseu
