#pragma once
// =============================================================================
// NkMaterial.h  —  Matériau multi-modèle RefCounted
//
// Modèles :
//   NK_UNLIT    — couleur/texture pure
//   NK_PHONG    — éclairage Phong
//   NK_PBR      — PBR Metallic-Roughness
//   NK_TOON     — Cel-shading
//   NK_CUSTOM   — shader fourni par l'utilisateur
//
// Domaines :
//   NK_SURFACE    — opaque/translucide standard
//   NK_DECAL      — décal projeté
//   NK_POSTPROCESS— fullscreen effect
//   NK_UI         — interface 2D
//   NK_PARTICLE   — billboard
// =============================================================================
#include "NKRenderer/Core/NkRef.h"
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkShader.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRenderer/Core/NkUniform.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace render {

class NkRenderDevice;

// =============================================================================
// Enums
// =============================================================================
enum class NkMaterialModel : uint32 {
    NK_UNLIT=0, NK_PHONG, NK_PBR, NK_TOON, NK_CUSTOM,
};

enum class NkMaterialDomain : uint32 {
    NK_SURFACE=0, NK_DECAL, NK_POSTPROCESS, NK_UI, NK_PARTICLE,
};

// =============================================================================
// Paramètres par modèle
// =============================================================================
struct NkUnlitParams {
    NkColorF     baseColor   = NkColorF::White();
    NkTexturePtr albedoTex;
    NkTexturePtr emissiveTex;
    NkColorF     emissiveColor= NkColorF(0,0,0,0);
    float32      alphaClip   = 0.f;
};

struct NkPhongParams {
    NkColorF     ambient   = NkColorF(0.15f,0.15f,0.15f,1.f);
    NkColorF     diffuse   = NkColorF::White();
    NkColorF     specular  = NkColorF(.5f,.5f,.5f,1.f);
    float32      shininess = 32.f;
    NkTexturePtr diffuseTex;
    NkTexturePtr specularTex;
    NkTexturePtr normalTex;
    bool receiveShadow = true;
    bool castShadow    = true;
};

struct NkPBRParams {
    NkColorF     baseColor      = NkColorF::White();
    NkTexturePtr albedoTex;          // sRGB
    NkTexturePtr normalTex;          // Linear, tangent-space
    float32      normalScale    = 1.f;
    NkTexturePtr ormTex;             // Occlusion(R)+Roughness(G)+Metallic(B)
    float32      metallic       = 0.f;
    float32      roughness      = 0.5f;
    float32      occlusion      = 1.f;
    NkColorF     emissiveColor  = NkColorF(0,0,0,0);
    NkTexturePtr emissiveTex;
    float32      emissiveScale  = 1.f;
    bool         clearcoat      = false;
    float32      clearcoatFactor= 0.f;
    float32      clearcoatRoughness = 0.f;
    float32      transmission   = 0.f;
    float32      ior            = 1.5f;
    float32      alphaClip      = 0.f;
    bool         receiveShadow  = true;
    bool         castShadow     = true;
    float32      iblIntensity   = 1.f;

    // Displacement / Parallax
    NkTexturePtr heightTex;
    float32      heightScale    = 0.f;  // 0 = disabled
};

struct NkToonParams {
    NkColorF     baseColor       = NkColorF::White();
    NkTexturePtr albedoTex;
    NkColorF     shadowColor     = NkColorF(.2f,.2f,.4f,1.f);
    NkColorF     outlineColor    = NkColorF::Black();
    float32      outlineWidth    = 0.02f;
    float32      shadowThreshold = 0.5f;
    float32      shadowSmoothness= 0.02f;
    float32      specThreshold   = 0.8f;
    float32      specSmoothness  = 0.02f;
    uint32       shadeSteps      = 3;
    bool         receiveShadow   = true;
    bool         outlineEnabled  = true;
};

// Paramètre générique pour NK_CUSTOM
struct NkMaterialParam {
    NkString     name;
    enum class T : uint8 { F1,F2,F3,F4,I1,I2,I3,I4,MAT4,BOOL,TEX } type = T::F1;
    union { float32 f[16]; int32 i[4]; bool b; } val{};
    NkTexturePtr texture;

    static NkMaterialParam Float(const char* n, float32 v)   { NkMaterialParam p; p.name=n; p.type=T::F1;  p.val.f[0]=v; return p; }
    static NkMaterialParam Float3(const char* n, float32 x,float32 y,float32 z) { NkMaterialParam p; p.name=n; p.type=T::F3; p.val.f[0]=x; p.val.f[1]=y; p.val.f[2]=z; return p; }
    static NkMaterialParam Float4(const char* n, float32 x,float32 y,float32 z,float32 w) { NkMaterialParam p; p.name=n; p.type=T::F4; p.val.f[0]=x; p.val.f[1]=y; p.val.f[2]=z; p.val.f[3]=w; return p; }
    static NkMaterialParam Color(const char* n, const NkColorF& c) { return Float4(n,c.r,c.g,c.b,c.a); }
    static NkMaterialParam Bool(const char* n, bool v) { NkMaterialParam p; p.name=n; p.type=T::BOOL; p.val.b=v; return p; }
    static NkMaterialParam Tex(const char* n, NkTexturePtr t) { NkMaterialParam p; p.name=n; p.type=T::TEX; p.texture=t; return p; }
};

// =============================================================================
// Descripteur de matériau
// =============================================================================
struct NkMaterialDesc {
    NkString         name;
    NkMaterialModel  model   = NkMaterialModel::NK_PBR;
    NkMaterialDomain domain  = NkMaterialDomain::NK_SURFACE;

    // ── Pipeline state ───────────────────────────────────────────────────────
    NkBlendMode blendMode   = NkBlendMode::NK_OPAQUE;
    NkFillMode  fillMode    = NkFillMode::NK_SOLID;
    NkCullMode  cullMode    = NkCullMode::NK_BACK;
    NkFrontFace frontFace   = NkFrontFace::NK_CCW;
    bool        depthTest   = true;
    bool        depthWrite  = true;
    NkDepthOp   depthOp     = NkDepthOp::NK_LESS;
    bool        twoSided    = false;
    float32     depthBiasConst = 0.f;
    float32     depthBiasSlope = 0.f;

    // ── Paramètres selon le modèle ───────────────────────────────────────────
    NkUnlitParams  unlit;
    NkPhongParams  phong;
    NkPBRParams    pbr;
    NkToonParams   toon;

    // ── Custom shader + params ───────────────────────────────────────────────
    NkShaderDesc               customShader;
    NkVector<NkMaterialParam>  customParams;

    // ── Factory helpers ──────────────────────────────────────────────────────
    static NkMaterialDesc Unlit(const char* n="Unlit") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_UNLIT; return d;
    }
    static NkMaterialDesc Phong(const char* n="Phong") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_PHONG; return d;
    }
    static NkMaterialDesc PBR(const char* n="PBR") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_PBR; return d;
    }
    static NkMaterialDesc Toon(const char* n="Toon") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_TOON; return d;
    }
    static NkMaterialDesc Custom(const NkShaderDesc& s, const char* n="Custom") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_CUSTOM; d.customShader=s; return d;
    }
    static NkMaterialDesc Wireframe(const char* n="Wireframe") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_UNLIT;
        d.fillMode=NkFillMode::NK_WIREFRAME; d.cullMode=NkCullMode::NK_NONE;
        return d;
    }
    static NkMaterialDesc ShadowDepth(const char* n="ShadowDepth") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_UNLIT;
        d.cullMode=NkCullMode::NK_FRONT;  // front-face culling pour shadow map
        d.depthBiasConst=1.25f; d.depthBiasSlope=1.75f;
        return d;
    }
    static NkMaterialDesc Transparent(const char* n="Transparent") {
        NkMaterialDesc d; d.name=n; d.model=NkMaterialModel::NK_PBR;
        d.blendMode=NkBlendMode::NK_ALPHA; d.depthWrite=false; return d;
    }

    // Clé de tri pour le batching (opaque avant alpha, puis par shader)
    uint32 SortKey() const {
        uint32 k=0;
        k|=((uint32)domain)  <<24;
        k|=(blendMode!=NkBlendMode::NK_OPAQUE?1u:0u)<<23;
        k|=((uint32)model)   <<16;
        k|=((uint32)blendMode)<<8;
        return k;
    }
};

// =============================================================================
// NkMaterial — ressource RefCounted
// Contient le pipeline compilé + les descriptor sets + les UBOs
// =============================================================================
class NkMaterial : public NkRefCounted {
public:
    const NkMaterialDesc& Desc()      const { return mDesc; }
    const NkString&       Name()      const { return mDesc.name; }
    NkMaterialModel       Model()     const { return mDesc.model; }
    NkMaterialDomain      Domain()    const { return mDesc.domain; }
    bool                  IsOpaque()  const { return mDesc.blendMode==NkBlendMode::NK_OPAQUE; }
    bool                  IsAlpha()   const { return mDesc.blendMode!=NkBlendMode::NK_OPAQUE; }
    uint32                SortKey()   const { return mDesc.SortKey(); }

    // Accès interne RHI
    uint64 RHIPipelineId()    const { return mRHIPipelineId; }
    uint64 RHIDescSetLayoutId()const{ return mRHIDescLayoutId; }
    uint64 RHIDescSetId()     const { return mRHIDescSetId; }
    uint64 RHIMatUBOId()      const { return mRHIMatUBOId; }

    // Paramètre dynamique — change une texture ou une valeur sans recréer le matériau
    void SetTexture(uint32 slot, NkTexturePtr tex);
    void SetFloat(const char* name, float32 value);
    void SetFloat3(const char* name, float32 x, float32 y, float32 z);
    void SetFloat4(const char* name, float32 x, float32 y, float32 z, float32 w);
    void SetColor(const char* name, const NkColorF& c) { SetFloat4(name,c.r,c.g,c.b,c.a); }
    void MarkDirty();   // Force la mise à jour du descriptor set au prochain flush

protected:
    friend class NkRenderDevice;
    explicit NkMaterial(const NkMaterialDesc& d) : mDesc(d) {}
    virtual void Destroy() override;

    NkMaterialDesc  mDesc;
    uint64          mRHIPipelineId    = 0;
    uint64          mRHIDescLayoutId  = 0;
    uint64          mRHIDescSetId     = 0;
    uint64          mRHIMatUBOId      = 0;
    bool            mDirty            = true;
    NkRenderDevice* mDevice           = nullptr;
};

using NkMaterialPtr = NkRef<NkMaterial>;

// =============================================================================
// NkMaterialLibrary — registre nommé de matériaux
// =============================================================================
class NkMaterialLibrary {
public:
    void Register(const char* name, NkMaterialDesc desc);
    const NkMaterialDesc* Find(const char* name) const;
    bool  Remove(const char* name);
    void  Clear();
    uint32 Count() const;

    // Matériaux par défaut enregistrés au init
    void RegisterDefaults();

    struct Entry { NkString name; NkMaterialDesc desc; };
    const NkVector<Entry>& Entries() const { return mEntries; }

private:
    NkVector<Entry> mEntries;
};

} // namespace render
} // namespace nkentseu