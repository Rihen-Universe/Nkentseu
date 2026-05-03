#pragma once
// =============================================================================
// NkMaterialSystem.h  — NKRenderer v4.0  (Materials/)
// Template + instance, catalogue PBR/NPR/Debug/Custom.
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
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
        // Stylisé / NPR
        NK_TOON=20, NK_TOON_INK, NK_ANIME, NK_WATERCOLOR,
        NK_SKETCH, NK_PIXEL_ART, NK_FLAT, NK_UPBGE_EEVEE,
        // Debug
        NK_UNLIT=60, NK_DEBUG_NORMALS, NK_DEBUG_UV,
        NK_WIREFRAME_MAT, NK_DEBUG_DEPTH, NK_DEBUG_AO,
        // Custom
        NK_CUSTOM=100,
    };

    enum class NkRenderQueue : uint8 {
        NK_BACKGROUND=0, NK_OPAQUE=100, NK_ALPHA_TEST=150,
        NK_TRANSPARENT=200, NK_OVERLAY=250,
    };

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

    struct alignas(16) NkToonParams {
        NkVec4f  shadowColor      = {0.2f,0.1f,0.3f,1.f};
        float32  shadowThreshold  = 0.3f;
        float32  shadowSmooth     = 0.05f;
        float32  outlineWidth     = 2.f;
        float32  rimIntensity     = 0.5f;
        NkVec4f  outlineColor     = {0,0,0,1};
        NkVec4f  rimColor         = {1,1,1,1};
        float32  specHardness     = 32.f;
        float32  _pad[3]          = {};
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
        NkMaterialInstance* SetToonShadowColor(NkVec3f c);
        NkMaterialInstance* SetOutline       (float32 w, NkVec3f color);
        NkMaterialInstance* SetFloat  (const NkString& n, float32 v);
        NkMaterialInstance* SetVec2   (const NkString& n, NkVec2f v);
        NkMaterialInstance* SetVec3   (const NkString& n, NkVec3f v);
        NkMaterialInstance* SetVec4   (const NkString& n, NkVec4f v);
        NkMaterialInstance* SetColor  (const NkString& n, NkVec4f c);
        NkMaterialInstance* SetInt    (const NkString& n, int32 v);
        NkMaterialInstance* SetBool   (const NkString& n, bool v);
        NkMaterialInstance* SetTexture(const NkString& n, NkTexHandle t);

        NkMatHandle       GetTemplate() const { return mTemplate; }
        NkRenderQueue     GetQueue()    const;
        bool              IsDirty()     const { return mDirty; }
        void              MarkClean()         { mDirty=false; }
        NkDescSetHandle   GetDescSet()  const { return mDescSet; }
        const NkPBRParams&  GetPBR()    const { return mPBR; }
        const NkToonParams& GetToon()   const { return mToon; }

    private:
        friend class NkMaterialSystem;
        NkMatHandle     mTemplate;
        NkDescSetHandle mDescSet;
        bool            mDirty = true;
        NkPBRParams     mPBR;
        NkToonParams    mToon;
        struct Param {
            NkString name;
            enum class Kind:uint8{F,V2,V3,V4,I,B,TEX} kind;
            union { float32 f; NkVec2f v2; NkVec3f v3; NkVec4f v4; int32 i; bool b; };
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

        bool Init(NkIDevice* device, NkTextureLibrary* texLib);
        void Shutdown();

        NkMatHandle        RegisterTemplate(const NkMaterialTemplateDesc& desc);
        NkMatHandle        FindTemplate    (const NkString& name) const;

        NkMaterialInstance* CreateInstance (NkMatHandle tmpl);
        void                DestroyInstance(NkMaterialInstance*& inst);

        // Bind avant draw (met à jour descset si dirty)
        bool BindInstance(NkICommandBuffer* cmd, NkMaterialInstance* inst,
                           NkTextureLibrary* texLib);

        void FlushCompilations();

        // Templates built-in
        NkMatHandle DefaultPBR()       const { return mTmplPBR; }
        NkMatHandle DefaultToon()      const { return mTmplToon; }
        NkMatHandle DefaultUnlit()     const { return mTmplUnlit; }
        NkMatHandle DefaultWireframe() const { return mTmplWire; }
        NkMatHandle DefaultSkin()      const { return mTmplSkin; }
        NkMatHandle DefaultHair()      const { return mTmplHair; }
        NkMatHandle DefaultAnime()     const { return mTmplAnime; }
        NkMatHandle DefaultArchviz()   const { return mTmplArchviz; }

    private:
        struct TemplateEntry {
            NkMaterialTemplateDesc desc;
            NkPipelineHandle       pipeline;
            bool                   compiled = false;
        };

        NkIDevice*          mDevice  = nullptr;
        NkTextureLibrary*   mTexLib  = nullptr;
        NkHashMap<uint64, TemplateEntry>   mTemplates;
        NkVector<NkMaterialInstance*>      mInstances;
        uint64                             mNextId = 1;

        NkMatHandle mTmplPBR, mTmplToon, mTmplUnlit, mTmplWire;
        NkMatHandle mTmplSkin, mTmplHair, mTmplAnime, mTmplArchviz;

        void RegisterBuiltins();
        NkPipelineHandle CompilePipeline(const TemplateEntry& t);
    };

} // namespace renderer
} // namespace nkentseu
