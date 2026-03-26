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
            Albedo        = 3,   // RGBA
            NormalMap     = 4,
            ORM           = 5,   // Occlusion/Roughness/Metallic
            Emissive      = 6,
            HeightMap     = 7,
            // PBR avancé
            AnisotropyDir = 13,
            SheenColor    = 14,
            ClearCoatNormal = 15,
            // SSS
            SubsurfaceScatter = 16,
            // Volume
            VolumeAbsorption  = 17,
            // Custom (jusqu'à binding 31)
            Custom0 = 18,
            Custom1 = 19,
            Custom2 = 20,
        };

        struct NkMatTexSlot {
            NkTexSlot    slot       = NkTexSlot::Albedo;
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
            Surface,      // Objet 3D standard (opaque, masqué, translucide)
            Decal,        // Projection sur surfaces
            PostProcess,  // Fullscreen post-effect
            UI,           // Interface utilisateur 2D
            Volume,       // Rendu volumétrique
            LightFunction,// Masque de lumière
            Particles,    // Système de particules
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
                        NkShadingModel model    = NkShadingModel::DefaultLit,
                        NkBlendMode    blend    = NkBlendMode::Opaque,
                        NkMaterialDomain domain = NkMaterialDomain::Surface);

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
            NkShadingModel    mShadingModel = NkShadingModel::DefaultLit;
            NkBlendMode       mBlendMode   = NkBlendMode::Opaque;
            NkMaterialDomain  mDomain      = NkMaterialDomain::Surface;
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
                case NkTexSlot::Albedo:    return SetAlbedoMap(tex);
                case NkTexSlot::NormalMap: return SetNormalMap(tex);
                case NkTexSlot::ORM:       return SetORMMap(tex);
                case NkTexSlot::Emissive:  return SetEmissiveMap(tex);
                case NkTexSlot::HeightMap: return SetHeightMap(tex);
                default:
                    uint32 idx = (uint32)slot - (uint32)NkTexSlot::Custom0;
                    if (idx < 8) { mCustomTextures[idx] = tex; mDirty = true; }
                    return *this;
            }
        }

    } // namespace renderer
} // namespace nkentseu
