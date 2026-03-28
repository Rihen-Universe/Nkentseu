#pragma once
// =============================================================================
// NkMaterial.h
// Système de matériaux du NKRenderer — le cœur du moteur de rendu.
//
// Architecture :
//   NkMaterialTemplate  → schéma (slots, paramètres possibles)
//   NkMaterialInstance  → valeurs concrètes (texture + scalaires + overrides)
//   NkMaterialLibrary   → cache, partage d'instances
//
// Modèles disponibles :
//   ── Jeux vidéo (PBR temps réel, proche UE5) ──────────────────────────────
//   • DefaultLit         : PBR Metallic/Roughness standard
//   • Subsurface         : SSS pour peau / végétation
//   • ClearCoat          : Vernis (voitures, bois verni)
//   • TwoSidedFoliage    : Feuilles, herbe
//   • Hair               : Marschner / Kajiya-Kay
//   • Cloth              : Ashikhmin (velours, tissu)
//   • Eye                : Cornée + iris spéculaire
//   • SingleLayerWater   : Eau temps réel
//   • ThinTranslucent    : Verre fin, bulle
//   • Toon               : Cel shading
//   • Unlit              : Émissif / UI / effets spéciaux
//
//   ── Modélisation / Film / Animation (Disney Principled) ──────────────────
//   • Principled         : Disney BSDF complet (≈ Blender Principled BSDF)
//   • PBR_SpecGloss      : PBR Specular/Glossiness (UPBGE / Blender)
//   • GlassBSDF          : Réfraction physique
//   • VolumeBSDF         : Volumes (fumée, feu, SSS complexe)
//   • Phong              : Phong classique (legacy)
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRenderer/Core/NkShaderAsset.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Paramètre scalaire d'un matériau
        // =============================================================================
        struct NkMatScalar {
            NkString name;
            float    value    = 0.f;
            float    minValue = 0.f;
            float    maxValue = 1.f;
            bool     isDirty  = true;
        };

        // Paramètre vecteur (vec4) d'un matériau
        struct NkMatVector {
            NkString  name;
            NkVec4f   value   = {0,0,0,1};
            bool      isColor = false;  // true = afficher comme sélecteur de couleur
            bool      isDirty = true;
        };

        // Slot de texture d'un matériau
        enum class NkTexSlot : uint32 {
            NK_ALBEDO        = 3,   // RGBA
            NK_NORMAL_MAP    = 4,
            NK_ORM           = 5,   // Occlusion/Roughness/Metallic
            NK_EMISSIVE      = 6,
            NK_HEIGHT_MAP    = 7,
            // PBR avancé
            NK_ANISOTROPY_DIR = 13,
            NK_SHEEN_COLOR    = 14,
            NK_CLEAR_COAT_NORMAL = 15,
            // SSS
            NK_SUBSURFACE_SCATTER = 16,
            // Volume
            NK_VOLUME_ABSORPTION  = 17,
            // Custom (jusqu'à binding 31)
            NK_CUSTOM0 = 18,
            NK_CUSTOM1 = 19,
            NK_CUSTOM2 = 20,
        };

        struct NkMatTexSlot {
            NkTexSlot    slot       = NkTexSlot::NK_ALBEDO;
            NkString     name;
            NkTexture2D* texture    = nullptr;  // null = utiliser la valeur scalaire/couleur
            bool         isDirty    = true;
            bool         required   = false;
            bool         srgb       = true;
        };

        // =============================================================================
        // Données GPU du matériau (layout std140 exact, 256 bytes)
        // Doit correspondre exactement à kGLSL_MaterialUBO dans NkShaderAsset.h
        // =============================================================================
        struct alignas(16) NkMaterialGPUData {
            float albedo[4]         = {1,1,1,1};     // albedo + alpha
            float pbrParams[4]      = {0,0.5f,1,1};  // metallic, roughness, ao, emissiveScale
            float emissiveColor[4]  = {0,0,0,0};
            float uvTilingOffset[4] = {1,1,0,0};

            float flags[4]          = {0,0,0,0};  // hasAlbedo, hasNormal, hasORM, hasEmissive
            float flags2[4]         = {0,0,0.01f,0}; // hasHeight, heightScale, alphaCutoff, shadingModel

            float subsurface[4]     = {1,0.3f,0.1f,0.1f}; // rgb=color, a=radius
            float clearCoat[4]      = {0,0.5f,0,0};
            float specular[4]       = {0.5f,0,0,0};
            float sheen[4]          = {0,0,0,0};
            float transmission[4]   = {0,1.45f,0,0}; // weight, IOR, thickness, absorption
        };

        static_assert(sizeof(NkMaterialGPUData) == 176, "NkMaterialGPUData size mismatch");

        // =============================================================================
        // Domaine d'utilisation du matériau
        // =============================================================================
        enum class NkMaterialDomain : uint32 {
            NK_SURFACE,      // Objet 3D standard (opaque, masqué, translucide)
            NK_DECAL,        // Projection sur surfaces
            NK_POST_PROCESS, // Fullscreen post-effect
            NK_UI,           // Interface utilisateur 2D
            NK_VOLUME,       // Rendu volumétrique
            NK_LIGHT_FUNCTION,// Masque de lumière
            NK_PARTICLES,    // Système de particules
        };

        // =============================================================================
        // NkMaterial — matériau complet
        // =============================================================================
        class NkMaterial {
            public:
                NkMaterial()  = default;
                ~NkMaterial() { Destroy(); }
                NkMaterial(const NkMaterial&) = delete;
                NkMaterial& operator=(const NkMaterial&) = delete;

                // ── Création ──────────────────────────────────────────────────────────────
                bool Create(NkIDevice* device,
                            NkShadingModel model    = NkShadingModel::NK_DEFAULT_LIT,
                            NkBlendMode    blend    = NkBlendMode::NK_OPAQUE,
                            NkMaterialDomain domain = NkMaterialDomain::NK_SURFACE);

                // Créer une instance à partir d'un matériau existant (partage le pipeline)
                NkMaterial* CreateInstance() const;

                void Destroy();

                // ── Propriétés de base ────────────────────────────────────────────────────
                NkMaterial& SetName(const char* name)        { mName = name; return *this; }
                NkMaterial& SetShadingModel(NkShadingModel m){ mShadingModel = m; mDirty = true; return *this; }
                NkMaterial& SetBlendMode(NkBlendMode b)      { mBlendMode = b; mDirty = true; return *this; }
                NkMaterial& SetDomain(NkMaterialDomain d)    { mDomain = d; mDirty = true; return *this; }
                NkMaterial& SetTwoSided(bool v)              { mTwoSided = v; mDirty = true; return *this; }

                // ── PBR Metallic/Roughness ────────────────────────────────────────────────
                NkMaterial& SetAlbedo(const NkColor4f& c);
                NkMaterial& SetMetallic(float m);
                NkMaterial& SetRoughness(float r);
                NkMaterial& SetAO(float ao);
                NkMaterial& SetEmissive(const NkColor4f& c, float scale = 1.f);
                NkMaterial& SetUVTiling(float u, float v);
                NkMaterial& SetUVOffset(float u, float v);
                NkMaterial& SetAlphaCutoff(float cutoff);

                // ── Textures ──────────────────────────────────────────────────────────────
                NkMaterial& SetAlbedoMap    (NkTexture2D* tex);
                NkMaterial& SetNormalMap    (NkTexture2D* tex);
                NkMaterial& SetORMMap       (NkTexture2D* tex); // R=AO, G=Roughness, B=Metallic
                NkMaterial& SetEmissiveMap  (NkTexture2D* tex);
                NkMaterial& SetHeightMap    (NkTexture2D* tex, float scale = 0.05f);
                NkMaterial& SetTexture      (NkTexSlot slot, NkTexture2D* tex);

                // Séparés (si pas d'ORM packagé)
                NkMaterial& SetRoughnessMap (NkTexture2D* tex);
                NkMaterial& SetMetallicMap  (NkTexture2D* tex);
                NkMaterial& SetAOMap        (NkTexture2D* tex);

                // ── Modèles spéciaux ──────────────────────────────────────────────────────

                // Subsurface Scattering
                NkMaterial& SetSubsurfaceColor(const NkColor4f& c);
                NkMaterial& SetSubsurfaceRadius(float r);

                // Clear Coat (vernis)
                NkMaterial& SetClearCoat(float intensity, float roughness = 0.1f);

                // Spéculaire
                NkMaterial& SetSpecular(float specular, float specularTint = 0.f);
                NkMaterial& SetAnisotropy(float anisotropy, float rotation = 0.f);

                // Sheen (tissu)
                NkMaterial& SetSheen(const NkColor4f& color, float tint = 0.f);

                // Transmission / Verre
                NkMaterial& SetTransmission(float weight, float ior = 1.45f, float thickness = 0.f);

                // Toon shading
                NkMaterial& SetToonBands(int bands, float smoothness = 0.01f);
                NkMaterial& SetToonRimLight(const NkColor4f& color, float width = 0.1f);
                NkMaterial& SetToonOutline(const NkColor4f& color, float thickness = 0.003f);

                // Eau
                NkMaterial& SetWaterNormalSpeed(float s)   { mWaterSpeed = s; mDirty = true; return *this; }
                NkMaterial& SetWaterFoamThreshold(float t) { mWaterFoam = t;  mDirty = true; return *this; }

                // Disney Principled (film/animation)
                NkMaterial& SetSubsurface(float v);   // Blender Subsurface
                NkMaterial& SetSpecularTint(float v);
                NkMaterial& SetSheenTint(float v);
                NkMaterial& SetClearCoatGloss(float v);
                NkMaterial& SetIOR(float ior);

                // ── Paramètres custom ─────────────────────────────────────────────────────
                NkMaterial& SetScalar(const char* name, float value);
                NkMaterial& SetVector(const char* name, const NkVec4f& value);
                float       GetScalar(const char* name, float def = 0.f) const;
                NkVec4f     GetVector(const char* name) const;

                // ── GPU ───────────────────────────────────────────────────────────────────
                // Uploader les données vers le GPU si dirty
                bool FlushToGPU();

                // Binder le matériau dans un command buffer (UBO + textures)
                void Bind(NkICommandBuffer* cmd, NkIDevice* device);

                // ── Pipeline ──────────────────────────────────────────────────────────────
                // Récupérer (ou compiler) le pipeline pour un render pass donné
                NkPipelineHandle GetPipeline(NkRenderPassHandle rp,
                                            const NkVertexLayout& vtxLayout,
                                            bool shadowPass = false);

                // ── Accesseurs ────────────────────────────────────────────────────────────
                bool              IsValid()           const { return mUBO.IsValid(); }
                NkMaterialID      GetID()             const { return mID; }
                const NkString&   GetName()           const { return mName; }
                NkShadingModel    GetShadingModel()   const { return mShadingModel; }
                NkBlendMode       GetBlendMode()      const { return mBlendMode; }
                NkMaterialDomain  GetDomain()         const { return mDomain; }
                bool              IsTwoSided()        const { return mTwoSided; }
                bool              IsDirty()           const { return mDirty; }
                NkBufferHandle    GetUBOHandle()      const { return mUBO; }
                NkDescSetHandle   GetDescSet()        const { return mDescSet; }
                NkDescSetHandle   GetDescSetLayout()  const { return mDescSetLayout; }
                const NkMaterialGPUData& GetGPUData() const { return mGPUData; }

                // Cloner ce matériau
                NkMaterial* Clone(const char* newName = nullptr) const;

                // ── Sérialisation ─────────────────────────────────────────────────────────
                // Sauvegarder / charger depuis un fichier .nkmat (format texte JSON-like)
                bool SaveToFile(const char* path) const;
                bool LoadFromFile(NkIDevice* device, const char* path);

            private:
                // ── Données GPU ───────────────────────────────────────────────────────────
                NkIDevice*        mDevice       = nullptr;
                NkBufferHandle    mUBO;
                NkDescSetHandle   mDescSet;
                NkDescSetHandle   mDescSetLayout;
                NkMaterialGPUData mGPUData;

                // ── Propriétés matériau ───────────────────────────────────────────────────
                NkString          mName;
                NkShadingModel    mShadingModel = NkShadingModel::NK_DEFAULT_LIT;
                NkBlendMode       mBlendMode   = NkBlendMode::NK_OPAQUE;
                NkMaterialDomain  mDomain      = NkMaterialDomain::NK_SURFACE;
                bool              mTwoSided    = false;
                bool              mDirty       = true;

                // ── Textures ──────────────────────────────────────────────────────────────
                NkTexture2D*  mAlbedoMap      = nullptr;
                NkTexture2D*  mNormalMap      = nullptr;
                NkTexture2D*  mORMMap         = nullptr;
                NkTexture2D*  mEmissiveMap    = nullptr;
                NkTexture2D*  mHeightMap      = nullptr;
                NkTexture2D*  mRoughnessMap   = nullptr; // si ORM non packagé
                NkTexture2D*  mMetallicMap    = nullptr;
                NkTexture2D*  mAOMap          = nullptr;
                NkTexture2D*  mCustomTextures[8] = {};

                // ── Paramètres spéciaux ───────────────────────────────────────────────────
                float   mToonBands      = 4.f;
                float   mToonSmooth     = 0.01f;
                NkColor4f mToonRim      = {0.9f,0.9f,1.f,1.f};
                float   mToonRimWidth   = 0.1f;
                NkColor4f mToonOutlineColor = {0,0,0,1};
                float   mToonOutlineThick = 0.003f;
                float   mWaterSpeed     = 0.05f;
                float   mWaterFoam      = 0.5f;

                // ── Paramètres custom (scalaires + vecteurs) ──────────────────────────────
                NkVector<NkMatScalar> mCustomScalars;
                NkVector<NkMatVector> mCustomVectors;

                // ── Pipelines compilés ────────────────────────────────────────────────────
                struct PipelineEntry {
                    NkRenderPassHandle  rp;
                    NkPipelineHandle    pipe;
                    NkPipelineHandle    shadowPipe;
                    NkVertexLayout      vtxLayout;
                };
                NkVector<PipelineEntry> mPipelines;

                // ── ID unique ─────────────────────────────────────────────────────────────
                NkMaterialID mID;
                static uint64 sIDCounter;

                // ── Helpers ───────────────────────────────────────────────────────────────
                NkShaderVariantKey BuildVariantKey() const;
                NkDescriptorSetLayoutDesc BuildLayoutDesc() const;
                void CreateDescSetLayout();
                void AllocDescSet();
                void UpdateDescSet();
                NkBlendDesc     BuildBlendDesc() const;
                NkRasterizerDesc BuildRasterizerDesc() const;
                NkDepthStencilDesc BuildDepthStencilDesc() const;
        };

        // Bloc legacy (NkPBRMaterial / NkFilmMaterial) désactivé :
        // il repose sur une ancienne API (NkTextureRef, NkMaterialTextureSlot, Type virtuel)
        // qui n'existe plus dans NkMaterial actuel.
        // On garde une seule API matériau cohérente et entièrement implémentée dans NkMaterial.cpp.
#if 0
        // =========================================================================
        // NkPBRUniforms — bloc uniforms GPU
        // =========================================================================
        struct alignas(16) NkPBRUniforms {
            // ── Base Color & PBR ───────────────────────────────────────────────────
            NkColor4f baseColor        = {1,1,1,1};  // 16 bytes — Base Color (linear)
            float32   metallic         = 0.f;         //  4
            float32   roughness        = 0.5f;        //  4
            float32   specular         = 0.5f;        //  4 — F0 scale, 0.5=4% (Unreal default)
            float32   normalStrength   = 1.f;         //  4  → 32 bytes

            // ── Emissive ─────────────────────────────────────────────────────────
            NkColor4f emissionColor    = {0,0,0,0};  // 16
            float32   emissionIntensity= 1.f;         //  4
            float32   aoStrength       = 1.f;         //  4
            float32   alphaCutoff      = 0.333f;      //  4
            uint32    blendMode        = 0;            //  4  → 64 bytes

            // ── Anisotropy (Unreal 4.26+) ─────────────────────────────────────────
            float32   anisotropy       = 0.f;         //  4
            float32   anisotropyRot    = 0.f;         //  4
            float32   heightScale      = 0.05f;       //  4 — parallax strength
            uint32    shadingModel     = 0;            //  4  → 80 bytes

            // ── Texture presence flags (bitfield) ─────────────────────────────────
            uint32    texFlags         = 0;            //  4 — bit i = HasTexture(slot i)
            float32   _pad[3]         = {};            // 12  → 96 bytes (6 × 16)
        };

        // Bit masks pour texFlags
        enum NkPBRTexFlag : uint32 {
            NK_PBRTEX_ALBEDO      = 1 << 0,
            NK_PBRTEX_NORMAL      = 1 << 1,
            NK_PBRTEX_ORM         = 1 << 2,
            NK_PBRTEX_EMISSION    = 1 << 3,
            NK_PBRTEX_SPECULAR    = 1 << 4,
            NK_PBRTEX_OPACITY     = 1 << 5,
            NK_PBRTEX_HEIGHT      = 1 << 6,
            NK_PBRTEX_ANISOTROPY  = 1 << 7,
        };

        // =========================================================================
        // NkPBRMaterial — PBR jeux vidéo, inspiré Unreal Engine
        // =========================================================================
        class NkPBRMaterial : public NkMaterial {
            public:
                NkPBRMaterial()  = default;
                ~NkPBRMaterial() { Destroy(); }

                Type GetType() const noexcept override { return Type::NK_PBR; }

                // ── Shading Model ─────────────────────────────────────────────────────

                void SetShadingModel(NkShadingModel model) noexcept {
                    mUniforms.shadingModel = static_cast<uint32>(model); mDirty=true;
                }
                NkShadingModel GetShadingModel() const noexcept {
                    return static_cast<NkShadingModel>(mUniforms.shadingModel);
                }

                // ── Paramètres PBR (Unreal Engine style) ──────────────────────────────

                void SetBaseColor(NkColor4f c)    noexcept { mUniforms.baseColor   = c;   mDirty=true; }
                void SetMetallic (float32 v)      noexcept { mUniforms.metallic    = v;   mDirty=true; }
                void SetRoughness(float32 v)      noexcept { mUniforms.roughness   = v;   mDirty=true; }
                // Specular : contrôle le F0 des non-métaux. 0.5 = 4% (valeur Unreal par défaut).
                void SetSpecular (float32 v)      noexcept { mUniforms.specular    = v;   mDirty=true; }
                void SetAOStrength(float32 v)     noexcept { mUniforms.aoStrength  = v;   mDirty=true; }
                void SetEmission(NkColor4f c, float32 intensity=1.f) noexcept {
                    mUniforms.emissionColor=c; mUniforms.emissionIntensity=intensity; mDirty=true;
                }
                void SetNormalStrength(float32 v) noexcept { mUniforms.normalStrength=v; mDirty=true; }
                void SetAnisotropy(float32 v, float32 rotation=0.f) noexcept {
                    mUniforms.anisotropy=v; mUniforms.anisotropyRot=rotation; mDirty=true;
                }
                void SetHeightScale(float32 v)    noexcept { mUniforms.heightScale=v; mDirty=true; }

                NkColor4f GetBaseColor()  const noexcept { return mUniforms.baseColor; }
                float32   GetMetallic()   const noexcept { return mUniforms.metallic;  }
                float32   GetRoughness()  const noexcept { return mUniforms.roughness; }
                float32   GetSpecular()   const noexcept { return mUniforms.specular;  }

                // ── Textures (helpers nommés) ──────────────────────────────────────────

                void SetAlbedoMap    (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_ALBEDO,      t); mDirty=true; }
                void SetNormalMap    (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_NORMAL,      t); mDirty=true; }
                void SetORMMap       (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_ORM,         t); mDirty=true; }
                void SetEmissionMap  (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_EMISSION,    t); mDirty=true; }
                void SetSpecularMap  (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_SPECULAR,    t); mDirty=true; }
                void SetOpacityMap   (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_OPACITY,     t); mDirty=true; }
                void SetHeightMap    (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_HEIGHT,      t); mDirty=true; }
                void SetAnisotropyMap(NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_ANISOTROPY,  t); mDirty=true; }

                // ── Presets rapides ────────────────────────────────────────────────────

                void MakeMetal(NkColor4f color, float32 roughness=0.3f) noexcept {
                    SetBaseColor(color); SetMetallic(1.f); SetRoughness(roughness);
                }
                void MakeDielectric(NkColor4f color, float32 roughness=0.5f, float32 specular=0.5f) noexcept {
                    SetBaseColor(color); SetMetallic(0.f); SetRoughness(roughness); SetSpecular(specular);
                }

                // ── GPU ───────────────────────────────────────────────────────────────

                bool Init   (NkIDevice* device) noexcept;
                bool Upload (NkIDevice* device) noexcept override;
                void Destroy() noexcept;

                NkDescSetHandle GetDescSet()       const noexcept { return mDescSet; }
                NkBufferHandle  GetUniformBuffer() const noexcept { return mUBO; }

                // Nombre de bindings textures dans le descriptor set (après UBO = binding 0)
                static constexpr uint32 TEX_BINDING_COUNT = 8; // slots 0-7 (albedo..anisotropy)

            private:
                void SyncTextureFlags() noexcept;

                NkPBRUniforms   mUniforms;
                NkBufferHandle  mUBO;
                NkDescSetHandle mDescSet;
                NkDescSetHandle mDescSetLayout;
                NkIDevice*      mDevice = nullptr;
                bool            mDirty  = true;
        };

        // =========================================================================
        // NkFilmUniforms — bloc uniforms GPU Principled BSDF
        // =========================================================================
        struct alignas(16) NkFilmUniforms {
            NkPBRUniforms base;                        // 96 bytes — base PBR (réutilise)

            // ── Subsurface Scattering ────────────────────────────────────────────
            NkColor4f sssColor       = {1,1,1,1};     // 16  — couleur de diffusion
            float32   sssStrength    = 0.f;            //  4  — intensité globale
            float32   sssRadius      = 1.f;            //  4  — rayon de diffusion (mm)
            float32   sssIOR         = 1.4f;           //  4  — IOR pour SSS
            float32   _pad0          = 0.f;            //  4  → 32 bytes (bloc SSS = 128)

            // ── Clearcoat ─────────────────────────────────────────────────────────
            float32   clearcoat      = 0.f;            //  4  — épaisseur clearcoat [0,1]
            float32   clearcoatRough = 0.f;            //  4  — rugosité clearcoat
            float32   _pad1[2]       = {};             //  8  → 144 bytes

            // ── Sheen (tissu/velours) ──────────────────────────────────────────────
            NkColor4f sheenColor     = {1,1,1,1};     // 16
            float32   sheen          = 0.f;            //  4  — intensité sheen
            float32   sheenRoughness = 0.5f;           //  4  — rugosité sheen
            float32   _pad2[2]       = {};             //  8  → 176 bytes

            // ── Transmission (verre/eau) ───────────────────────────────────────────
            float32   transmission    = 0.f;           //  4  — poids de transmission [0,1]
            float32   ior             = 1.45f;         //  4  — indice de réfraction
            float32   transmissionRoughness = 0.f;     //  4  — rugosité surface interne
            float32   _pad3           = 0.f;           //  4  → 192 bytes

            // ── Emission (override base) ───────────────────────────────────────────
            float32   emissionStrength = 1.f;          //  4  — multiplicateur d'émission
            // ── Anisotropy ────────────────────────────────────────────────────────
            float32   anisotropy      = 0.f;           //  4
            float32   anisotropyRot   = 0.f;           //  4
            // ── Tex flags film ────────────────────────────────────────────────────
            uint32    filmTexFlags    = 0;             //  4  → 208 bytes
            // ── Padding → 224 bytes = 14 × 16 ────────────────────────────────────
            float32   _pad4[4]        = {};            // 16  → 224 bytes
        };

        // Bit masks filmTexFlags (en plus de ceux PBR)
        enum NkFilmTexFlag : uint32 {
            NK_FILMTEX_SUBSURFACE   = 1 << 0,
            NK_FILMTEX_CLEARCOAT    = 1 << 1,
            NK_FILMTEX_CC_NORMAL    = 1 << 2,
            NK_FILMTEX_SHEEN        = 1 << 3,
            NK_FILMTEX_TRANSMISSION = 1 << 4,
        };

        // =========================================================================
        // NkFilmMaterial — Principled BSDF (UPBGE / Blender Cycles quality)
        // =========================================================================
        class NkFilmMaterial : public NkMaterial {
            public:
                NkFilmMaterial()  = default;
                ~NkFilmMaterial() { Destroy(); }

                Type GetType() const noexcept override { return Type::NK_FILM; }

                // ── Paramètres de base (réutilise le bloc PBR) ────────────────────────

                void SetBaseColor(NkColor4f c)   noexcept { mUniforms.base.baseColor  = c;   mDirty=true; }
                void SetMetallic (float32 v)     noexcept { mUniforms.base.metallic   = v;   mDirty=true; }
                void SetRoughness(float32 v)     noexcept { mUniforms.base.roughness  = v;   mDirty=true; }
                void SetSpecular (float32 v)     noexcept { mUniforms.base.specular   = v;   mDirty=true; }
                void SetNormalStrength(float32 v)noexcept { mUniforms.base.normalStrength=v; mDirty=true; }
                void SetEmission(NkColor4f c, float32 strength=1.f) noexcept {
                    mUniforms.base.emissionColor=c;
                    mUniforms.emissionStrength=strength;
                    mDirty=true;
                }

                // ── Subsurface Scattering (SSS) ────────────────────────────────────────
                // S'approche du modèle skin de Blender Principled BSDF.
                void SetSSS(float32 strength, NkColor4f color={1,1,1,1},
                            float32 radius=1.f, float32 ior=1.4f) noexcept {
                    mUniforms.sssStrength=strength; mUniforms.sssColor=color;
                    mUniforms.sssRadius=radius; mUniforms.sssIOR=ior; mDirty=true;
                }

                // ── Clearcoat (vernis / carrosserie) ──────────────────────────────────
                void SetClearcoat(float32 weight, float32 roughness=0.f) noexcept {
                    mUniforms.clearcoat=weight; mUniforms.clearcoatRough=roughness; mDirty=true;
                }

                // ── Sheen (tissu, velours) ─────────────────────────────────────────────
                void SetSheen(float32 weight, NkColor4f color={1,1,1,1}, float32 roughness=0.5f) noexcept {
                    mUniforms.sheen=weight; mUniforms.sheenColor=color;
                    mUniforms.sheenRoughness=roughness; mDirty=true;
                }

                // ── Transmission / Verre ──────────────────────────────────────────────
                void SetTransmission(float32 weight, float32 ior=1.45f, float32 txRoughness=0.f) noexcept {
                    mUniforms.transmission=weight; mUniforms.ior=ior;
                    mUniforms.transmissionRoughness=txRoughness; mDirty=true;
                }

                // ── Anisotropy ────────────────────────────────────────────────────────
                void SetAnisotropy(float32 aniso, float32 rotation=0.f) noexcept {
                    mUniforms.anisotropy=aniso; mUniforms.anisotropyRot=rotation; mDirty=true;
                }

                // ── Textures ──────────────────────────────────────────────────────────

                void SetAlbedoMap        (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_ALBEDO,           t); mDirty=true; }
                void SetNormalMap        (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_NORMAL,           t); mDirty=true; }
                void SetORMMap           (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_ORM,              t); mDirty=true; }
                void SetEmissionMap      (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_EMISSION,         t); mDirty=true; }
                void SetSSSMap           (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_SUBSURFACE,       t); mDirty=true; }
                void SetClearcoatMap     (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_CLEARCOAT,        t); mDirty=true; }
                void SetClearcoatNormalMap(NkTextureRef t)noexcept { SetTexture(NkMaterialTextureSlot::NK_CLEARCOAT_NORMAL, t); mDirty=true; }
                void SetSheenMap         (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_SHEEN,            t); mDirty=true; }
                void SetTransmissionMap  (NkTextureRef t) noexcept { SetTexture(NkMaterialTextureSlot::NK_TRANSMISSION,     t); mDirty=true; }

                // ── Presets film ──────────────────────────────────────────────────────

                void MakeGlass(float32 ior=1.5f, float32 roughness=0.f) noexcept {
                    SetMetallic(0.f); SetRoughness(roughness); SetTransmission(1.f, ior);
                    SetBlendMode(NkBlendMode::NK_TRANSLUCENT);
                }
                void MakeSkin(NkColor4f color={0.9f,0.7f,0.6f,1.f}) noexcept {
                    SetBaseColor(color); SetMetallic(0.f); SetRoughness(0.4f);
                    SetSSS(0.8f, {1.f,0.3f,0.2f,1.f}, 1.5f, 1.4f);
                }
                void MakeCloth(NkColor4f color={0.5f,0.5f,0.5f,1.f}, float32 sheen=0.5f) noexcept {
                    SetBaseColor(color); SetMetallic(0.f); SetRoughness(0.9f);
                    SetSheen(sheen);
                }

                // ── GPU ───────────────────────────────────────────────────────────────

                bool Init   (NkIDevice* device) noexcept;
                bool Upload (NkIDevice* device) noexcept override;
                void Destroy() noexcept;

                NkDescSetHandle GetDescSet()       const noexcept { return mDescSet; }
                NkBufferHandle  GetUniformBuffer() const noexcept { return mUBO; }

                static constexpr uint32 TEX_BINDING_COUNT = 13; // slots 0-12

            private:
                NkFilmUniforms  mUniforms;
                NkBufferHandle  mUBO;
                NkDescSetHandle mDescSet;
                NkDescSetHandle mDescSetLayout;
                NkIDevice*      mDevice = nullptr;
                bool            mDirty  = true;
        };

        // ── Fabriques globales ──────────────────────────────────────────────────────

        inline NkPBRMaterialRef  NkMakePBRMaterial()  { return memory::NkMakeShared<NkPBRMaterial>(); }
        inline NkFilmMaterialRef NkMakeFilmMaterial() { return memory::NkMakeShared<NkFilmMaterial>(); }
#endif

        // =============================================================================
        // NkMaterialLibrary — catalogue centralisé de matériaux
        // =============================================================================
        class NkMaterialLibrary {
        public:
            static NkMaterialLibrary& Get();

            void Init(NkIDevice* device);
            void Shutdown();

            // Enregistrer un matériau
            void Register(NkMaterial* mat);

            // Charger depuis un fichier .nkmat
            NkMaterial* Load(const char* path);

            // Charger depuis un répertoire (parcours récursif)
            void LoadDirectory(const char* dir);

            // Accès par nom
            NkMaterial* Find(const NkString& name) const;
            NkMaterial* Find(NkMaterialID id)      const;

            // Matériaux built-in
            NkMaterial* GetDefaultPBR();
            NkMaterial* GetDefaultUnlit();
            NkMaterial* GetDefaultWireframe();
            NkMaterial* GetDefaultToon();
            NkMaterial* GetDefaultGlass();
            NkMaterial* GetDefaultWater();
            NkMaterial* GetDefaultHair();
            NkMaterial* GetDefaultCloth();
            NkMaterial* GetDefaultSkin();     // SSS pour peau

            // Matériaux de débogage
            NkMaterial* GetDebugNormals();
            NkMaterial* GetDebugUV();
            NkMaterial* GetDebugVertexColor();
            NkMaterial* GetDebugDepth();
            NkMaterial* GetDebugAO();
            NkMaterial* GetDebugRoughness();
            NkMaterial* GetDebugMetallic();

            void Update(); // Flush GPU dirty materials

        private:
            NkMaterialLibrary() = default;
            NkIDevice*   mDevice = nullptr;
            NkUnorderedMap<NkString, NkMaterial*>     mByName;
            NkUnorderedMap<uint64,   NkMaterial*>     mByID;
            NkVector<NkMaterial*>                     mAll;

            NkMaterial* mDefaultPBR      = nullptr;
            NkMaterial* mDefaultUnlit    = nullptr;
            NkMaterial* mDefaultWire     = nullptr;

            void CreateBuiltins();
        };

        // =============================================================================
        // Implémentation inline
        // =============================================================================

        inline NkMaterial& NkMaterial::SetAlbedo(const NkColor4f& c) {
            mGPUData.albedo[0]=c.r; mGPUData.albedo[1]=c.g;
            mGPUData.albedo[2]=c.b; mGPUData.albedo[3]=c.a;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetMetallic(float m) {
            mGPUData.pbrParams[0] = m; mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetRoughness(float r) {
            mGPUData.pbrParams[1] = r; mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetAO(float ao) {
            mGPUData.pbrParams[2] = ao; mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetEmissive(const NkColor4f& c, float scale) {
            mGPUData.emissiveColor[0]=c.r; mGPUData.emissiveColor[1]=c.g;
            mGPUData.emissiveColor[2]=c.b; mGPUData.pbrParams[3] = scale;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetUVTiling(float u, float v) {
            mGPUData.uvTilingOffset[0]=u; mGPUData.uvTilingOffset[1]=v;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetUVOffset(float u, float v) {
            mGPUData.uvTilingOffset[2]=u; mGPUData.uvTilingOffset[3]=v;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetAlphaCutoff(float c) {
            mGPUData.flags2[2] = c; mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetAlbedoMap(NkTexture2D* tex) {
            mAlbedoMap = tex;
            mGPUData.flags[0] = tex ? 1.f : 0.f;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetNormalMap(NkTexture2D* tex) {
            mNormalMap = tex;
            mGPUData.flags[1] = tex ? 1.f : 0.f;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetORMMap(NkTexture2D* tex) {
            mORMMap = tex;
            mGPUData.flags[2] = tex ? 1.f : 0.f;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetEmissiveMap(NkTexture2D* tex) {
            mEmissiveMap = tex;
            mGPUData.flags[3] = tex ? 1.f : 0.f;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetHeightMap(NkTexture2D* tex, float scale) {
            mHeightMap = tex;
            mGPUData.flags2[0] = tex ? 1.f : 0.f;
            mGPUData.flags2[1] = scale;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetSubsurfaceColor(const NkColor4f& c) {
            mGPUData.subsurface[0]=c.r; mGPUData.subsurface[1]=c.g;
            mGPUData.subsurface[2]=c.b;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetSubsurfaceRadius(float r) {
            mGPUData.subsurface[3] = r; mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetClearCoat(float intensity, float roughness) {
            mGPUData.clearCoat[0] = intensity;
            mGPUData.clearCoat[1] = roughness;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetSpecular(float s, float st) {
            mGPUData.specular[0] = s;
            mGPUData.specular[1] = st;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetSheen(const NkColor4f& c, float tint) {
            mGPUData.sheen[0]=c.r; mGPUData.sheen[1]=c.g;
            mGPUData.sheen[2]=c.b; mGPUData.sheen[3]=tint;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetTransmission(float w, float ior, float thick) {
            mGPUData.transmission[0]=w;
            mGPUData.transmission[1]=ior;
            mGPUData.transmission[2]=thick;
            mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetIOR(float ior) {
            mGPUData.transmission[1] = ior; mDirty = true; return *this;
        }

        inline NkMaterial& NkMaterial::SetTexture(NkTexSlot slot, NkTexture2D* tex) {
            switch (slot) {
                case NkTexSlot::NK_ALBEDO:    return SetAlbedoMap(tex);
                case NkTexSlot::NK_NORMAL_MAP: return SetNormalMap(tex);
                case NkTexSlot::NK_ORM:       return SetORMMap(tex);
                case NkTexSlot::NK_EMISSIVE:  return SetEmissiveMap(tex);
                case NkTexSlot::NK_HEIGHT_MAP: return SetHeightMap(tex);
                default:
                    uint32 idx = (uint32)slot - (uint32)NkTexSlot::NK_CUSTOM0;
                    if (idx < 8) { mCustomTextures[idx] = tex; mDirty = true; }
                    return *this;
            }
        }

    } // namespace renderer
} // namespace nkentseu
