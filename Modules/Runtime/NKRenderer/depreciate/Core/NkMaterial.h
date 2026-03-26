#pragma once
// =============================================================================
// NkMaterial.h — Système de matériaux à deux niveaux :
//
//   NkPBRMaterial   — PBR jeux vidéo, inspiré d'Unreal Engine.
//                     Shading Models, Specular/F0, Anisotropy, Opacity, etc.
//
//   NkFilmMaterial  — Principled BSDF (UPBGE/Blender-style).
//                     SSS, Clearcoat, Sheen, Transmission, IOR, Hair, Eye...
//                     Vise la qualité d'un rendu Cycles en temps réel.
//
// Architecture :
//   NkMaterial (base) ← NkPBRMaterial   (NkShadingModel)
//                     ← NkFilmMaterial  (Principled BSDF)
// =============================================================================
#include "NKRenderer/Core/NkTexture.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // =========================================================================
    // NkShadingModel — Shading Models (inspiré Unreal Engine)
    // =========================================================================
    enum class NkShadingModel : uint8 {
        NK_DEFAULT_LIT      = 0,  // PBR standard — rendu le plus courant
        NK_UNLIT            = 1,  // Pas de lighting — émissif pur, UI, FX
        NK_SUBSURFACE       = 2,  // Peau, cire, marbre — approximation SSS rapide
        NK_PREINTEGRATED_SKIN=3,  // Peau humaine (pré-intégrée, moins coûteux)
        NK_CLEAR_COAT       = 4,  // Vernis sur carrosserie, parquet
        NK_SUBSURFACE_PROFILE=5,  // SSS par profil (qualité peau Unreal)
        NK_HAIR             = 6,  // Cheveux / fourrure (Marschner model)
        NK_CLOTH            = 7,  // Tissu (velours, coton — sheen model)
        NK_EYE              = 8,  // Globe oculaire (cornée + iris séparés)
        NK_THIN_TRANSLUCENT = 9,  // Verre fin, feuilles, papier
        NK_COUNT
    };

    // =========================================================================
    // NkBlendMode — mode de mélange (inspiré Unreal Engine)
    // =========================================================================
    enum class NkBlendMode : uint8 {
        NK_OPAQUE           = 0,  // Pas d'alpha, le plus performant
        NK_MASKED           = 1,  // Cutout — pas de tri nécessaire
        NK_TRANSLUCENT      = 2,  // Alpha blend — tri requis (painter's algo)
        NK_ADDITIVE         = 3,  // Additif — FX, lueurs, feu
        NK_MODULATE         = 4,  // Multiplication des couleurs
        NK_ALPHA_COMPOSITE  = 5,  // Pré-multiplié alpha
    };

    // =========================================================================
    // Slots de texture (bindings descriptor set)
    // =========================================================================
    enum class NkMaterialTextureSlot : uint32 {
        NK_ALBEDO           = 0,  // Base Color / Albedo (sRGB)
        NK_NORMAL           = 1,  // Normal map tangent-space (Linear)
        NK_ORM              = 2,  // Occlusion(R) Roughness(G) Metallic(B) (Linear)
        NK_EMISSION         = 3,  // Emissive (HDR possible) (Linear/sRGB)
        NK_SPECULAR         = 4,  // Specular map — F0 pour non-métaux (Linear)
        NK_OPACITY          = 5,  // Masque d'opacité / Alpha map (Linear)
        NK_HEIGHT           = 6,  // Heightmap / Parallax (Linear)
        NK_ANISOTROPY       = 7,  // Direction/intensité d'anisotropie (Linear)
        NK_SUBSURFACE       = 8,  // Carte SSS (couleur/intensité) (sRGB)
        NK_CLEARCOAT        = 9,  // Intensité clearcoat + rugosité (Linear)
        NK_CLEARCOAT_NORMAL = 10, // Normal map de la couche clearcoat (Linear)
        NK_SHEEN            = 11, // Intensité/couleur sheen (Linear)
        NK_TRANSMISSION     = 12, // Carte de transmission (Linear)
        NK_COUNT            = 13
    };

    // =========================================================================
    // NkMaterial — classe de base
    // =========================================================================
    class NKRENDERER_API NkMaterial {
    public:
        enum class Type : uint8 { NK_PBR, NK_FILM };

        virtual ~NkMaterial() = default;
        virtual Type GetType() const noexcept = 0;

        void SetTexture(NkMaterialTextureSlot slot, NkTextureRef tex) noexcept;
        NkTextureRef GetTexture(NkMaterialTextureSlot slot) const noexcept;
        bool HasTexture(NkMaterialTextureSlot slot) const noexcept;

        void SetBlendMode(NkBlendMode mode) noexcept   { mBlendMode  = mode;  }
        void SetAlphaCutoff(float32 v)      noexcept   { mAlphaCutoff= v;     }
        NkBlendMode GetBlendMode()  const noexcept     { return mBlendMode;   }
        float32     GetAlphaCutoff()const noexcept     { return mAlphaCutoff; }
        bool        IsTransparent() const noexcept     { return mBlendMode != NkBlendMode::NK_OPAQUE
                                                                  && mBlendMode != NkBlendMode::NK_MASKED; }

        void SetDoubleSided(bool v) noexcept  { mDoubleSided = v; }
        bool IsDoubleSided()  const noexcept  { return mDoubleSided; }

        void        SetName(const char* name) noexcept { mName = name; }
        const char* GetName() const noexcept           { return mName; }

        virtual bool Upload(NkIDevice* device) noexcept { (void)device; return true; }

    protected:
        NkTextureRef mTextures[static_cast<uint32>(NkMaterialTextureSlot::NK_COUNT)];
        NkBlendMode  mBlendMode   = NkBlendMode::NK_OPAQUE;
        float32      mAlphaCutoff = 0.333f;  // Unreal Engine default
        bool         mDoubleSided = false;
        const char*  mName        = nullptr;
    };

    // =========================================================================
    // NkPBRUniforms — bloc uniforms GPU (std140, Unreal-like)
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
    class NKRENDERER_API NkPBRMaterial : public NkMaterial {
    public:
        NkPBRMaterial()  = default;
        ~NkPBRMaterial() override { Destroy(); }

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
    class NKRENDERER_API NkFilmMaterial : public NkMaterial {
    public:
        NkFilmMaterial()  = default;
        ~NkFilmMaterial() override { Destroy(); }

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

} // namespace nkentseu
