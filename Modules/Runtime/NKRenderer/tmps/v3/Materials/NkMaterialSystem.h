#pragma once
// =============================================================================
// NkMaterialSystem.h
// Système de matériaux complet : PBR métallique, NPR, Toon, Archiviz, etc.
//
// Hiérarchie :
//   NkMaterialTemplate  — définit la structure (slots, uniforms, pipeline)
//   NkMaterial          — instance d'un template avec ses paramètres propres
//   NkMaterialSystem    — cache, compilation, binding
//
// Modèles de rendu disponibles :
//   ── Réaliste ──────────────────────────────────────────────────────────────
//   NK_MAT_PBR_METALLIC      — PBR métallique/rugosité (Unreal/GLTF standard)
//   NK_MAT_PBR_SPECULAR      — PBR spéculaire/glossiness (Substance legacy)
//   NK_MAT_ARCHIVIZ          — PBR + subsurface + baked AO (rendu architecture)
//   NK_MAT_SKIN              — SSS + specular double couche (peau humaine)
//   NK_MAT_HAIR              — Kajiya-Kay + Marschner
//   NK_MAT_GLASS             — transmission + réfraction + caustics
//   NK_MAT_CLOTH             — Ashikhmin/Prewitt pour velours/soie
//   NK_MAT_CAR_PAINT         — flake + clearcoat + metallic
//   ── Non-réaliste / Stylisé ───────────────────────────────────────────────
//   NK_MAT_TOON              — cel-shading étapes discrètes
//   NK_MAT_TOON_INK          — toon + outlines (manga/anime)
//   NK_MAT_WATERCOLOR        — aquarelle procédurale
//   NK_MAT_SKETCH            — crayon/esquisse
//   NK_MAT_PIXEL_ART         — pixel art (snap + dithering)
//   NK_MAT_FLAT              — couleur plate, ombre plate (flat design)
//   ── Simulation / Scientifique ────────────────────────────────────────────
//   NK_MAT_VOLUME            — rendu volumétrique (fog, smoke, flame)
//   NK_MAT_TERRAIN           — terrain multi-couche avec splatmap
//   NK_MAT_WATER             — eau (FFT/Gerstner, réflexions, réfraction)
//   NK_MAT_EMISSIVE          — émission pure (LEDs, feu, néons)
//   ── Debug ────────────────────────────────────────────────────────────────
//   NK_MAT_DEBUG_NORMALS     — visualisation des normales
//   NK_MAT_UNLIT             — couleur plate sans lumière
//   NK_MAT_WIREFRAME         — maillage filaire
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkColor.h"

namespace nkentseu {

    // =============================================================================
    // Type de modèle de matériau
    // =============================================================================
    enum class NkMaterialType : uint16 {
        // ── Réaliste ──────────────────────────────────────────────────────────────
        NK_PBR_METALLIC      = 0,
        NK_PBR_SPECULAR      = 1,
        NK_ARCHIVIZ          = 2,
        NK_SKIN              = 3,
        NK_HAIR              = 4,
        NK_GLASS             = 5,
        NK_CLOTH             = 6,
        NK_CAR_PAINT         = 7,
        NK_FOLIAGE           = 8,   // two-sided + subsurface
        // ── Stylisé ───────────────────────────────────────────────────────────────
        NK_TOON              = 20,
        NK_TOON_INK          = 21,
        NK_WATERCOLOR        = 22,
        NK_SKETCH            = 23,
        NK_PIXEL_ART         = 24,
        NK_FLAT              = 25,
        NK_ANIME             = 26,  // ombres dures + rim light + fresnel stylisé
        NK_UPBGE_EEVEE       = 27,  // imite le look EEVEE/UPBGE
        // ── Spécial ───────────────────────────────────────────────────────────────
        NK_TERRAIN           = 40,
        NK_WATER             = 41,
        NK_VOLUME            = 42,
        NK_EMISSIVE          = 43,
        NK_PARTICLE          = 44,
        // ── Debug ─────────────────────────────────────────────────────────────────
        NK_UNLIT             = 60,
        NK_DEBUG_NORMALS     = 61,
        NK_DEBUG_UV          = 62,
        NK_WIREFRAME         = 63,
        NK_COUNT
    };

    // =============================================================================
    // Identifiant de matériau
    // =============================================================================
    using NkMatId = uint64;
    static constexpr NkMatId NK_INVALID_MAT = 0;

    // =============================================================================
    // Paramètres PBR métallique (layout std140 — aligné GPU)
    // =============================================================================
    struct alignas(16) NkPBRParams {
        math::NkVec4  albedo          = {1,1,1,1}; // base color + alpha
        math::NkVec4  emissive        = {0,0,0,0}; // couleur + intensité
        float32       metallic        = 0.f;
        float32       roughness       = 0.5f;
        float32       ao              = 1.f;
        float32       emissiveStrength= 0.f;
        // Clearcoat
        float32       clearcoat       = 0.f;
        float32       clearcoatRough  = 0.f;
        // Sheen (tissu)
        float32       sheen           = 0.f;
        float32       sheenRoughness  = 0.5f;
        // Subsurface / SSS
        float32       subsurface      = 0.f;
        math::NkVec3  subsurfaceColor = {1,0.3f,0.2f};
        float32       sssRadius       = 1.f;
        // Transmission
        float32       transmission    = 0.f;
        float32       ior             = 1.5f;
        // Tiling UV
        math::NkVec2  uvScale         = {1,1};
        math::NkVec2  uvOffset        = {0,0};
        // Normal strength
        float32       normalStrength  = 1.f;
        float32       _pad0[3];
    };

    // =============================================================================
    // Paramètres Toon
    // =============================================================================
    struct alignas(16) NkToonParams {
        math::NkVec4  colorLight  = {1,1,1,1};
        math::NkVec4  colorShadow = {0.2f,0.2f,0.3f,1};
        float32       steps       = 3.f;       // nombre de bandes d'ombre
        float32       stepSharpness = 0.8f;    // dureté des transitions
        float32       rimStrength = 0.4f;
        float32       rimThreshold= 0.1f;
        math::NkVec4  rimColor    = {1,1,1,1};
        // Ink / outline
        float32       outlineWidth= 0.003f;
        math::NkVec4  outlineColor= {0,0,0,1};
        float32       outlineDepthBias = 0.f;
        float32       _pad0[3];
    };

    // =============================================================================
    // Paramètres Terrain
    // =============================================================================
    struct alignas(16) NkTerrainParams {
        math::NkVec4  layer0Color = {0.6f,0.5f,0.3f,1};  // sable/sol
        math::NkVec4  layer1Color = {0.3f,0.55f,0.2f,1}; // herbe
        math::NkVec4  layer2Color = {0.5f,0.45f,0.4f,1}; // roche
        math::NkVec4  layer3Color = {0.95f,0.95f,0.98f,1};// neige
        float32       layer0Height= 0.1f;
        float32       layer1Height= 0.3f;
        float32       layer2Height= 0.7f;
        float32       layer3Height= 0.9f;
        float32       blendSharpness = 4.f;
        float32       snowSlopeFalloff = 0.8f;
        float32       uvScale     = 20.f;
        float32       _pad0;
    };

    // =============================================================================
    // Paramètres Water
    // =============================================================================
    struct alignas(16) NkWaterParams {
        math::NkVec4  shallowColor= {0.1f,0.5f,0.6f,0.7f};
        math::NkVec4  deepColor   = {0.05f,0.2f,0.4f,1.f};
        float32       waveSpeed   = 0.5f;
        float32       waveScale   = 2.f;
        float32       waveHeight  = 0.1f;
        float32       refractionStrength = 0.02f;
        float32       foamThreshold= 0.1f;
        float32       foamSpeed   = 1.f;
        float32       specularPower= 300.f;
        float32       _pad0;
    };

    // =============================================================================
    // Slots de textures pour un matériau
    // =============================================================================
    struct NkMaterialTextures {
        NkTexId albedo       = NK_INVALID_TEX;  // ou diffuse
        NkTexId normal       = NK_INVALID_TEX;
        NkTexId metalRough   = NK_INVALID_TEX;  // R=metallic G=roughness (PBR Met)
        NkTexId specularGloss= NK_INVALID_TEX;  // R=specular A=glossiness (PBR Spec)
        NkTexId ao           = NK_INVALID_TEX;
        NkTexId emissive     = NK_INVALID_TEX;
        NkTexId sssMap       = NK_INVALID_TEX;  // SSS thickness map
        NkTexId detailNormal = NK_INVALID_TEX;  // détail normal (archiviz)
        NkTexId splatmap     = NK_INVALID_TEX;  // terrain layer blending
        NkTexId layer1       = NK_INVALID_TEX;  // terrain layers
        NkTexId layer2       = NK_INVALID_TEX;
        NkTexId layer3       = NK_INVALID_TEX;
        NkTexId layer4       = NK_INVALID_TEX;
        NkTexId opacity      = NK_INVALID_TEX;  // masque d'opacité
        NkTexId heightMap    = NK_INVALID_TEX;  // parallax / displacement
        NkTexId envProbe     = NK_INVALID_TEX;  // reflection cubemap local
    };

    // =============================================================================
    // Instance de matériau
    // =============================================================================
    struct NkMaterial {
        NkMatId           id       = NK_INVALID_MAT;
        NkMaterialType    type     = NkMaterialType::NK_PBR_METALLIC;
        NkString          name;
        NkMaterialTextures textures;
        NkPBRParams       pbr;
        NkToonParams      toon;
        NkTerrainParams   terrain;
        NkWaterParams     water;

        // Render state overrides
        bool   doubleSided     = false;
        bool   alphaBlend      = false;
        bool   alphaMask       = false;
        float32 alphaCutoff    = 0.5f;
        bool   castShadows     = true;
        bool   receiveShadows  = true;
        bool   unlit           = false;
        float32 lineWidth      = 1.f;  // wireframe

        // GPU handles (gérés par NkMaterialSystem)
        NkBufferHandle  uniformBuffer;
        NkDescSetHandle descriptorSet;
        NkPipelineHandle pipeline;
        bool            dirty = true;

        bool IsValid() const { return id != NK_INVALID_MAT; }
    };

    // =============================================================================
    // NkMaterialSystem
    // =============================================================================
    class NkMaterialSystem {
        public:
            explicit NkMaterialSystem(NkIDevice* device, NkTextureLibrary* texLib);
            ~NkMaterialSystem();

            bool Initialize(NkRenderPassHandle forwardPass,
                            NkRenderPassHandle deferredPass,
                            NkSampleCount msaa = NkSampleCount::NK_S1);
            void Shutdown();

            // ── Création ──────────────────────────────────────────────────────────────
            NkMatId Create(NkMaterialType type, const NkString& name = "");
            NkMatId Clone(NkMatId src, const NkString& newName = "");

            // ── Préréglages rapides ───────────────────────────────────────────────────
            NkMatId CreateDefault();           // PBR gris neutre
            NkMatId CreateUnlit(math::NkVec4 color = {1,1,1,1});
            NkMatId CreatePBR(NkTexId albedo, NkTexId normalMap = NK_INVALID_TEX,
                            float32 metallic = 0.f, float32 roughness = 0.5f);
            NkMatId CreateToon(math::NkVec4 lightColor = {1,1,1,1},
                                math::NkVec4 shadowColor = {0.2f,0.2f,0.3f,1},
                                uint32 steps = 3);
            NkMatId CreateToonInk(math::NkVec4 base, float32 outlineWidth = 0.003f);
            NkMatId CreateGlass(float32 ior = 1.5f, float32 roughness = 0.05f);
            NkMatId CreateWater(float32 waveHeight = 0.1f);
            NkMatId CreateTerrain(NkTexId splatmap,
                                NkTexId layer0, NkTexId layer1,
                                NkTexId layer2 = NK_INVALID_TEX,
                                NkTexId layer3 = NK_INVALID_TEX);
            NkMatId CreateEmissive(math::NkVec3 color, float32 strength = 5.f);
            NkMatId CreateAnime(NkTexId albedo,
                                math::NkVec4 shadowColor = {0.75f,0.75f,0.85f,1},
                                float32 rimStrength = 0.3f);
            NkMatId CreateArchviz(NkTexId albedo,
                                NkTexId normal = NK_INVALID_TEX,
                                NkTexId ao     = NK_INVALID_TEX);
            NkMatId CreateSkin(NkTexId albedo,
                                NkTexId sssMap = NK_INVALID_TEX,
                                float32 sssRadius = 1.f);

            // ── Accès et modification ─────────────────────────────────────────────────
            NkMaterial*       Get(NkMatId id);
            const NkMaterial* Get(NkMatId id) const;
            bool              IsValid(NkMatId id) const;
            void              MarkDirty(NkMatId id);

            // ── Flush (met à jour les GPU buffers pour les matériaux dirty) ───────────
            void FlushDirty();

            // ── Binding ───────────────────────────────────────────────────────────────
            // Lie le pipeline + le descriptor set du matériau
            void Bind(NkICommandBuffer* cmd, NkMatId id);

            // Destructions
            void Release(NkMatId id);
            void ReleaseAll();

            // ── Sérialisation ─────────────────────────────────────────────────────────
            bool SaveToJSON(NkMatId id, const NkString& path) const;
            NkMatId LoadFromJSON(const NkString& path);

        private:
            NkIDevice*        mDevice;
            NkTextureLibrary* mTexLib;
            NkRenderPassHandle mForwardPass, mDeferredPass;
            NkSampleCount     mMSAA;
            uint64            mNextId = 1;

            NkUnorderedMap<NkMatId,   NkMaterial>     mMaterials;
            NkUnorderedMap<NkString,  NkMatId>         mNameIndex;
            NkUnorderedMap<uint32,    NkPipelineHandle> mPipelineCache;  // hash → pipeline
            NkUnorderedMap<uint32,    NkDescSetHandle>  mDescLayoutCache;

            // Layouts de descriptor sets partagés par type
            NkDescSetHandle mPBRLayout;
            NkDescSetHandle mToonLayout;
            NkDescSetHandle mTerrainLayout;
            NkDescSetHandle mWaterLayout;
            NkDescSetHandle mUnlitLayout;

            bool CompileShaderForType(NkMaterialType type);
            bool CreatePipelineForMaterial(NkMaterial& mat);
            void UploadMaterialUniforms(NkMaterial& mat);
            void CreateDescriptorSet(NkMaterial& mat);
            uint32 HashPipelineKey(const NkMaterial& mat) const;

            NkShaderHandle GetShaderForType(NkMaterialType type);
            NkUnorderedMap<uint32, NkShaderHandle> mShaderCache;
    };

} // namespace nkentseu

#pragma once
// =============================================================================
// NkMaterialSystem.h
// Système de matériaux : PBR, Toon, Terrain, Water, Skin, Anime, Archviz...
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "../Core/NkTextureLibrary.h"
#include "NKMath/NkVec.h"

namespace nkentseu {

    // =========================================================================
    // Types de matériaux
    // =========================================================================
    enum class NkMaterialType : uint16 {
        NK_PBR_METALLIC=0, NK_PBR_SPECULAR=1, NK_ARCHVIZ=2,
        NK_SKIN=3, NK_HAIR=4, NK_GLASS=5, NK_CLOTH=6, NK_CAR_PAINT=7,
        NK_FOLIAGE=8,
        NK_TOON=20, NK_TOON_INK=21, NK_WATERCOLOR=22, NK_SKETCH=23,
        NK_PIXEL_ART=24, NK_FLAT=25, NK_ANIME=26, NK_UPBGE_EEVEE=27,
        NK_TERRAIN=40, NK_WATER=41, NK_VOLUME=42, NK_EMISSIVE=43, NK_PARTICLE=44,
        NK_UNLIT=60, NK_DEBUG_NORMALS=61, NK_DEBUG_UV=62, NK_WIREFRAME=63,
        NK_COUNT
    };

    using NkMatId = uint64;
    static constexpr NkMatId NK_INVALID_MAT = 0;

    // =========================================================================
    // Paramètres GPU (std140)
    // =========================================================================
    struct alignas(16) NkPBRParams {
        math::NkVec4 albedo           = {1,1,1,1};
        math::NkVec4 emissive         = {0,0,0,0};
        float32      metallic         = 0.f;
        float32      roughness        = 0.5f;
        float32      ao               = 1.f;
        float32      emissiveStrength = 0.f;
        float32      clearcoat        = 0.f;
        float32      clearcoatRough   = 0.f;
        float32      sheen            = 0.f;
        float32      sheenRoughness   = 0.5f;
        float32      subsurface       = 0.f;
        math::NkVec3 subsurfaceColor  = {1,0.3f,0.2f};
        float32      sssRadius        = 1.f;
        float32      transmission     = 0.f;
        float32      ior              = 1.5f;
        float32      uvScaleX         = 1.f;
        float32      uvScaleY         = 1.f;
        float32      uvOffsetX        = 0.f;
        float32      uvOffsetY        = 0.f;
        float32      normalStrength   = 1.f;
        float32      _pad[3];
    };

    struct alignas(16) NkToonParams {
        math::NkVec4 colorLight   = {1,1,1,1};
        math::NkVec4 colorShadow  = {0.2f,0.2f,0.3f,1};
        float32      steps        = 3.f;
        float32      stepSharpness= 0.8f;
        float32      rimStrength  = 0.4f;
        float32      rimThreshold = 0.1f;
        math::NkVec4 rimColor     = {1,1,1,1};
        float32      outlineWidth = 0.003f;
        math::NkVec4 outlineColor = {0,0,0,1};
        float32      outlineDepthBias=0.f;
        float32      _pad[3];
    };

    struct alignas(16) NkTerrainParams {
        math::NkVec4 layer0Color={0.6f,0.5f,0.3f,1};
        math::NkVec4 layer1Color={0.3f,0.55f,0.2f,1};
        math::NkVec4 layer2Color={0.5f,0.45f,0.4f,1};
        math::NkVec4 layer3Color={0.95f,0.95f,0.98f,1};
        float32 layer0H=0.1f,layer1H=0.3f,layer2H=0.7f,layer3H=0.9f;
        float32 blendSharpness=4.f,snowSlopeFalloff=0.8f,uvScale=20.f,_pad;
    };

    struct alignas(16) NkWaterParams {
        math::NkVec4 shallowColor={0.1f,0.5f,0.6f,0.7f};
        math::NkVec4 deepColor={0.05f,0.2f,0.4f,1.f};
        float32 waveSpeed=0.5f,waveScale=2.f,waveHeight=0.1f,refractionStr=0.02f;
        float32 foamThreshold=0.1f,foamSpeed=1.f,specularPower=300.f,_pad;
    };

    // =========================================================================
    // Slots texture
    // =========================================================================
    struct NkMaterialTextures {
        NkTexId albedo=0,normal=0,metalRough=0,specularGloss=0,ao=0;
        NkTexId emissive=0,sssMap=0,detailNormal=0,opacity=0;
        NkTexId splatmap=0,layer1=0,layer2=0,layer3=0,layer4=0;
        NkTexId heightMap=0,envProbe=0,lightmap=0;
    };

    // =========================================================================
    // Instance de matériau
    // =========================================================================
    struct NkMaterial {
        NkMatId              id         = NK_INVALID_MAT;
        NkMaterialType       type       = NkMaterialType::NK_PBR_METALLIC;
        NkString             name;
        NkMaterialTextures   textures;
        NkPBRParams          pbr;
        NkToonParams         toon;
        NkTerrainParams      terrain;
        NkWaterParams        water;
        bool                 doubleSided  = false;
        bool                 alphaBlend   = false;
        bool                 alphaMask    = false;
        float32              alphaCutoff  = 0.5f;
        bool                 castShadows  = true;
        bool                 receiveShadows=true;
        float32              lineWidth    = 1.f;
        NkBufferHandle       uniformBuffer;
        NkDescSetHandle      descriptorSet;
        NkPipelineHandle     pipeline;
        bool                 dirty        = true;
        bool                 IsValid()    const { return id!=NK_INVALID_MAT; }
    };

    // =========================================================================
    // NkMaterialSystem
    // =========================================================================
    class NkMaterialSystem {
    public:
        explicit NkMaterialSystem(NkIDevice* device, NkTextureLibrary* texLib);
        ~NkMaterialSystem();

        bool Initialize(NkRenderPassHandle forwardPass,
                         NkRenderPassHandle deferredPass,
                         NkSampleCount msaa=NkSampleCount::NK_S1);
        void Shutdown();

        // ── Création ──────────────────────────────────────────────────────────
        NkMatId Create(NkMaterialType type, const NkString& name="");
        NkMatId Clone (NkMatId src, const NkString& newName="");

        // ── Préréglages ───────────────────────────────────────────────────────
        NkMatId CreateDefault();
        NkMatId CreateUnlit(math::NkVec4 color={1,1,1,1});
        NkMatId CreatePBR(NkTexId albedo,NkTexId normalMap=0,
                           float32 metallic=0.f,float32 roughness=0.5f);
        NkMatId CreateToon(math::NkVec4 light={1,1,1,1},
                            math::NkVec4 shadow={0.2f,0.2f,0.3f,1},uint32 steps=3);
        NkMatId CreateToonInk(math::NkVec4 base,float32 outlineW=0.003f);
        NkMatId CreateAnime(NkTexId albedo,math::NkVec4 shadowColor={0.75f,0.75f,0.85f,1},
                             float32 rimStrength=0.3f);
        NkMatId CreateArchviz(NkTexId albedo,NkTexId normal=0,NkTexId ao=0);
        NkMatId CreateSkin(NkTexId albedo,NkTexId sssMap=0,float32 sssRadius=1.f);
        NkMatId CreateGlass(float32 ior=1.5f,float32 roughness=0.05f);
        NkMatId CreateWater(float32 waveHeight=0.1f);
        NkMatId CreateTerrain(NkTexId splatmap,NkTexId l0,NkTexId l1,NkTexId l2=0,NkTexId l3=0);
        NkMatId CreateEmissive(math::NkVec3 color,float32 strength=5.f);
        NkMatId CreateCarPaint(math::NkVec3 color,bool metallic=false);

        // ── Accès ─────────────────────────────────────────────────────────────
        NkMaterial*       Get(NkMatId id);
        const NkMaterial* Get(NkMatId id) const;
        bool              IsValid(NkMatId id) const;
        void              MarkDirty(NkMatId id);

        // ── GPU ───────────────────────────────────────────────────────────────
        void FlushDirty();
        void Bind(NkICommandBuffer* cmd, NkMatId id);

        // ── Sérialisation ─────────────────────────────────────────────────────
        bool    SaveToJSON(NkMatId id, const NkString& path) const;
        NkMatId LoadFromJSON(const NkString& path);

        // ── Cycle de vie ──────────────────────────────────────────────────────
        void Release(NkMatId id);
        void ReleaseAll();

    private:
        NkIDevice*         mDevice;
        NkTextureLibrary*  mTexLib;
        NkRenderPassHandle mForwardPass, mDeferredPass;
        NkSampleCount      mMSAA;
        NkMatId            mNextId = 1;

        NkUnorderedMap<NkMatId, NkMaterial>     mMaterials;
        NkUnorderedMap<uint32, NkPipelineHandle> mPipelineCache;
        NkUnorderedMap<uint32, NkShaderHandle>   mShaderCache;
        NkUnorderedMap<uint32, NkDescSetHandle>  mLayoutCache;

        NkDescSetHandle mPBRLayout, mToonLayout, mTerrainLayout,
                        mWaterLayout, mUnlitLayout;

        NkMatId AllocId();
        bool    CompileShaderForType(NkMaterialType type);
        bool    CreatePipelineForMaterial(NkMaterial& mat);
        void    UploadMaterialUniforms(NkMaterial& mat);
        void    CreateDescriptorSet(NkMaterial& mat);
        uint32  HashPipelineKey(const NkMaterial& mat) const;
        NkShaderHandle GetShaderForType(NkMaterialType type);

        // Shader sources multi-backend (indexé par type + backend)
        NkUnorderedMap<uint32, NkShaderHandle> mBackendShaderCache;
    };

} // namespace nkentseu
