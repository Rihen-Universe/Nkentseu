#pragma once
// =============================================================================
// NkMaterialSystem.h
// Système de matériaux haut niveau.
//
// NkMaterialLibrary  — registre global des templates (shader + params par défaut)
// NkMaterialBuilder  — DSL fluide pour construire un template
// NkUniform          — wrapper type-safe pour un uniform block
// NkPushConstant     — push constant immédiat (< 128 bytes)
//
// Modèles de matériaux prédéfinis (accès via NkMaterialLibrary::Get()) :
//   "nk/unlit"       — couleur + texture, pas de lumière
//   "nk/phong"       — Phong directionnel
//   "nk/pbr"         — PBR metallic-roughness
//   "nk/toon"        — Toon shading
//   "nk/pbr_specular"— PBR specular-glossiness
//   "nk/2d_sprite"   — sprite 2D avec alpha blend
//   "nk/2d_text"     — rendu texte (atlas de glyphes)
//   "nk/shadow"      — depth-only pass shadow map
//   "nk/debug"       — wireframe debug
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkUniform<T> — wrapper type-safe pour un uniform block GPU
    // =========================================================================
    template<typename T>
    class NkUniform {
    public:
        static_assert(sizeof(T) % 16 == 0,
            "NkUniform<T> : T doit être aligné sur 16 bytes (std140)");

        explicit NkUniform(NkIRenderer* renderer, const char* name=nullptr,
                           uint32 binding=0)
            : mRenderer(renderer)
        {
            NkUniformBlockDesc d{};
            d.name      = name;
            d.sizeBytes = sizeof(T);
            d.binding   = binding;
            mHandle = mRenderer->CreateUniformBlock(d);
        }

        ~NkUniform() {
            if (mHandle.IsValid()) mRenderer->DestroyUniformBlock(mHandle);
        }

        NkUniform(const NkUniform&) = delete;
        NkUniform& operator=(const NkUniform&) = delete;

        // Écrire depuis un struct T
        bool Upload(const T& data) {
            if (!mHandle.IsValid()) return false;
            mData = data;
            return mRenderer->WriteUniformBlock(mHandle, &mData, sizeof(T));
        }

        // Accès direct au buffer local (modifié sans upload immédiat)
        T&       Get()       { return mData; }
        const T& Get() const { return mData; }

        // Forcer l'upload du buffer local
        bool Flush() {
            return mRenderer->WriteUniformBlock(mHandle, &mData, sizeof(T));
        }

        NkUniformBlockHandle GetHandle() const { return mHandle; }
        bool IsValid() const { return mHandle.IsValid(); }

    private:
        NkIRenderer*         mRenderer = nullptr;
        NkUniformBlockHandle mHandle;
        T                    mData{};
    };

    // =========================================================================
    // NkPushConstant — données immédiates < 128 bytes
    // =========================================================================
    struct NkPushConstantData {
        static constexpr uint32 kMaxBytes = 128;
        uint8  data[kMaxBytes] = {};
        uint32 size = 0;
        uint32 stageFlags = 0;  // bitmask de NkShaderStageType

        template<typename T>
        void Set(const T& v, uint32 stages = 0xFF) {
            static_assert(sizeof(T) <= kMaxBytes, "Push constant trop grand (> 128 bytes)");
            memcpy(data, &v, sizeof(T));
            size       = sizeof(T);
            stageFlags = stages;
        }

        template<typename T>
        const T& As() const {
            return *reinterpret_cast<const T*>(data);
        }
    };

    // =========================================================================
    // NkMaterialBuilder — DSL fluide
    // =========================================================================
    class NkMaterialBuilder {
    public:
        explicit NkMaterialBuilder(const char* debugName=nullptr) {
            if (debugName) mDesc.debugName = debugName;
        }

        NkMaterialBuilder& WithShader(NkShaderHandle sh) {
            mDesc.shader = sh; return *this;
        }
        NkMaterialBuilder& WithDomain(NkMaterialDomain d) {
            mDesc.domain = d; return *this;
        }
        NkMaterialBuilder& WithShading(NkShadingModel s) {
            mDesc.shading = s; return *this;
        }
        NkMaterialBuilder& WithBlend(NkBlendMode b) {
            mDesc.blend = b; return *this;
        }
        NkMaterialBuilder& WithCull(NkCullMode c) {
            mDesc.cull = c; return *this;
        }
        NkMaterialBuilder& WithFill(NkFillMode f) {
            mDesc.fill = f; return *this;
        }
        NkMaterialBuilder& DepthTest(bool on)    { mDesc.depthTest    = on; return *this; }
        NkMaterialBuilder& DepthWrite(bool on)   { mDesc.depthWrite   = on; return *this; }
        NkMaterialBuilder& CastShadow(bool on)   { mDesc.castShadow   = on; return *this; }
        NkMaterialBuilder& ReceiveShadow(bool on){ mDesc.receiveShadow= on; return *this; }
        NkMaterialBuilder& DoubleSided()         { mDesc.cull = NkCullMode::None; return *this; }
        NkMaterialBuilder& Wireframe()           { mDesc.fill = NkFillMode::Wireframe; return *this; }

        NkMaterialBuilder& Param(const char* name, float32 v) {
            NkMaterialParam p{};
            p.type = NkMaterialParam::Type::Float;
            p.f = v; p.name = name;
            mDesc.params.PushBack(p); return *this;
        }
        NkMaterialBuilder& Param(const char* name, NkVec4f v) {
            NkMaterialParam p{};
            p.type = NkMaterialParam::Type::Vec4;
            p.v4[0]=v.x; p.v4[1]=v.y; p.v4[2]=v.z; p.v4[3]=v.w;
            p.name = name;
            mDesc.params.PushBack(p); return *this;
        }
        NkMaterialBuilder& Param(const char* name, NkTextureHandle tex) {
            NkMaterialParam p{};
            p.type    = NkMaterialParam::Type::Texture;
            p.texture = tex; p.name = name;
            mDesc.params.PushBack(p); return *this;
        }
        NkMaterialBuilder& Param(const char* name, bool v) {
            NkMaterialParam p{};
            p.type = NkMaterialParam::Type::Bool;
            p.b = v; p.name = name;
            mDesc.params.PushBack(p); return *this;
        }

        NkMaterialHandle Build(NkIRenderer* renderer) const {
            return renderer->CreateMaterialTemplate(mDesc);
        }

        const NkMaterialTemplateDesc& GetDesc() const { return mDesc; }

    private:
        NkMaterialTemplateDesc mDesc;
    };

    // =========================================================================
    // NkMaterialInstanceBuilder — crée une instance depuis un template
    // =========================================================================
    class NkMaterialInstanceBuilder {
    public:
        explicit NkMaterialInstanceBuilder(NkMaterialHandle tmpl,
                                           const char* name=nullptr)
        {
            mDesc.templateHandle = tmpl;
            if (name) mDesc.debugName = name;
        }

        NkMaterialInstanceBuilder& Set(const char* name, float32 v) {
            NkMaterialParam p{};
            p.type = NkMaterialParam::Type::Float; p.f=v; p.name=name;
            mDesc.overrides.PushBack(p); return *this;
        }
        NkMaterialInstanceBuilder& Set(const char* name, NkVec3f v) {
            NkMaterialParam p{};
            p.type=NkMaterialParam::Type::Vec3;
            p.v3[0]=v.x; p.v3[1]=v.y; p.v3[2]=v.z; p.name=name;
            mDesc.overrides.PushBack(p); return *this;
        }
        NkMaterialInstanceBuilder& Set(const char* name, NkVec4f v) {
            NkMaterialParam p{};
            p.type=NkMaterialParam::Type::Vec4;
            p.v4[0]=v.x; p.v4[1]=v.y; p.v4[2]=v.z; p.v4[3]=v.w; p.name=name;
            mDesc.overrides.PushBack(p); return *this;
        }
        NkMaterialInstanceBuilder& Set(const char* name, NkTextureHandle tex) {
            NkMaterialParam p{};
            p.type=NkMaterialParam::Type::Texture; p.texture=tex; p.name=name;
            mDesc.overrides.PushBack(p); return *this;
        }
        NkMaterialInstanceBuilder& Set(const char* name, bool v) {
            NkMaterialParam p{};
            p.type=NkMaterialParam::Type::Bool; p.b=v; p.name=name;
            mDesc.overrides.PushBack(p); return *this;
        }

        NkMaterialInstHandle Build(NkIRenderer* renderer) const {
            return renderer->CreateMaterialInstance(mDesc);
        }

    private:
        NkMaterialInstanceDesc mDesc;
    };

    // =========================================================================
    // NkMaterialLibrary — registre des templates standards
    // =========================================================================
    class NkMaterialLibrary {
    public:
        NkMaterialLibrary() = default;
        ~NkMaterialLibrary() { Shutdown(); }

        // Initialise les templates built-in (nécessite les shaders déjà créés)
        bool Init(NkIRenderer* renderer,
                  NkShaderHandle shPBR,
                  NkShaderHandle shUnlit,
                  NkShaderHandle sh2DSprite,
                  NkShaderHandle shText,
                  NkShaderHandle shShadow,
                  NkShaderHandle shDebug,
                  NkShaderHandle shToon = NkShaderHandle::Null());

        void Shutdown();

        // Récupérer un template par nom
        NkMaterialHandle Get(const char* name) const;

        // Enregistrer un template custom
        void Register(const char* name, NkMaterialHandle handle);

        // Supprimer un template du registre (ne détruit pas la ressource)
        void Unregister(const char* name);

        // Créer une instance directement depuis le nom du template
        NkMaterialInstHandle CreateInstance(NkIRenderer* renderer,
                                            const char* templateName,
                                            const char* instName=nullptr) const;

        bool IsRegistered(const char* name) const;

        // Accès aux templates intégrés
        NkMaterialHandle GetPBR()      const { return mPBR; }
        NkMaterialHandle GetUnlit()    const { return mUnlit; }
        NkMaterialHandle GetSprite2D() const { return m2DSprite; }
        NkMaterialHandle GetText2D()   const { return mText; }
        NkMaterialHandle GetShadow()   const { return mShadow; }
        NkMaterialHandle GetDebug()    const { return mDebug; }
        NkMaterialHandle GetToon()     const { return mToon; }

    private:
        NkIRenderer* mRenderer = nullptr;

        NkMaterialHandle mPBR;
        NkMaterialHandle mUnlit;
        NkMaterialHandle m2DSprite;
        NkMaterialHandle mText;
        NkMaterialHandle mShadow;
        NkMaterialHandle mDebug;
        NkMaterialHandle mToon;

        NkHashMap<NkString, NkMaterialHandle> mRegistry;
    };

    // =========================================================================
    // NkMaterialLibrary — implémentation inline (petite, pas besoin d'un .cpp)
    // =========================================================================
    inline bool NkMaterialLibrary::Init(NkIRenderer* renderer,
                                        NkShaderHandle shPBR,
                                        NkShaderHandle shUnlit,
                                        NkShaderHandle sh2DSprite,
                                        NkShaderHandle shText,
                                        NkShaderHandle shShadow,
                                        NkShaderHandle shDebug,
                                        NkShaderHandle shToon)
    {
        mRenderer = renderer;

        // PBR
        mPBR = NkMaterialBuilder("nk/pbr")
            .WithShader(shPBR)
            .WithShading(NkShadingModel::PBR)
            .WithBlend(NkBlendMode::Opaque)
            .WithCull(NkCullMode::Back)
            .Param("baseColorFactor",  NkVec4f{1,1,1,1})
            .Param("metallicFactor",   0.0f)
            .Param("roughnessFactor",  0.5f)
            .Param("emissiveStrength", 0.0f)
            .Param("occlusionStrength",1.0f)
            .Param("useAlbedoMap",     false)
            .Param("useNormalMap",     false)
            .Param("useMetalRoughMap", false)
            .Param("useOcclusionMap",  false)
            .Param("useEmissiveMap",   false)
            .Param("alphaCutoff",      0.0f)
            .Build(renderer);
        Register("nk/pbr", mPBR);

        // Unlit
        mUnlit = NkMaterialBuilder("nk/unlit")
            .WithShader(shUnlit)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::Opaque)
            .Param("baseColorFactor", NkVec4f{1,1,1,1})
            .Param("useAlbedoMap",    false)
            .Param("alphaCutoff",     0.0f)
            .Build(renderer);
        Register("nk/unlit", mUnlit);

        // Unlit transparent
        NkMaterialHandle unlitTransp = NkMaterialBuilder("nk/unlit_alpha")
            .WithShader(shUnlit)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::AlphaBlend)
            .DepthWrite(false)
            .Param("baseColorFactor", NkVec4f{1,1,1,1})
            .Param("useAlbedoMap",    false)
            .Build(renderer);
        Register("nk/unlit_alpha", unlitTransp);

        // Additive
        NkMaterialHandle additive = NkMaterialBuilder("nk/additive")
            .WithShader(shUnlit)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::Additive)
            .DepthWrite(false)
            .Build(renderer);
        Register("nk/additive", additive);

        // 2D Sprite
        m2DSprite = NkMaterialBuilder("nk/2d_sprite")
            .WithShader(sh2DSprite)
            .WithDomain(NkMaterialDomain::Surface)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::AlphaBlend)
            .WithCull(NkCullMode::None)
            .DepthWrite(false)
            .CastShadow(false)
            .ReceiveShadow(false)
            .Param("tintColor",  NkVec4f{1,1,1,1})
            .Param("useTexture", false)
            .Build(renderer);
        Register("nk/2d_sprite", m2DSprite);

        // Text
        mText = NkMaterialBuilder("nk/2d_text")
            .WithShader(shText)
            .WithDomain(NkMaterialDomain::UI)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::AlphaBlend)
            .WithCull(NkCullMode::None)
            .DepthWrite(false)
            .CastShadow(false)
            .Build(renderer);
        Register("nk/2d_text", mText);

        // Shadow depth-only
        mShadow = NkMaterialBuilder("nk/shadow")
            .WithShader(shShadow)
            .WithDomain(NkMaterialDomain::Surface)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::Opaque)
            .WithCull(NkCullMode::Front)  // front-face culling pour réduire l'acne
            .DepthWrite(true)
            .Build(renderer);
        Register("nk/shadow", mShadow);

        // Debug wireframe
        mDebug = NkMaterialBuilder("nk/debug")
            .WithShader(shDebug)
            .WithDomain(NkMaterialDomain::Surface)
            .WithShading(NkShadingModel::Unlit)
            .WithBlend(NkBlendMode::AlphaBlend)
            .WithFill(NkFillMode::Wireframe)
            .WithCull(NkCullMode::None)
            .DepthTest(false)
            .DepthWrite(false)
            .CastShadow(false)
            .Build(renderer);
        Register("nk/debug", mDebug);

        // Toon (optionnel)
        if (shToon.IsValid()) {
            mToon = NkMaterialBuilder("nk/toon")
                .WithShader(shToon)
                .WithShading(NkShadingModel::Toon)
                .WithBlend(NkBlendMode::Opaque)
                .Param("baseColorFactor", NkVec4f{1,1,1,1})
                .Build(renderer);
            Register("nk/toon", mToon);
        }

        return true;
    }

    inline void NkMaterialLibrary::Shutdown() {
        mRegistry.Clear();
        // Les handles sont détruits par le renderer lors de son Shutdown
        mPBR    = NkMaterialHandle::Null();
        mUnlit  = NkMaterialHandle::Null();
        m2DSprite=NkMaterialHandle::Null();
        mText   = NkMaterialHandle::Null();
        mShadow = NkMaterialHandle::Null();
        mDebug  = NkMaterialHandle::Null();
        mToon   = NkMaterialHandle::Null();
    }

    inline NkMaterialHandle NkMaterialLibrary::Get(const char* name) const {
        const NkMaterialHandle* h = mRegistry.Find(NkString(name));
        return h ? *h : NkMaterialHandle::Null();
    }

    inline void NkMaterialLibrary::Register(const char* name, NkMaterialHandle handle) {
        mRegistry[NkString(name)] = handle;
    }

    inline void NkMaterialLibrary::Unregister(const char* name) {
        mRegistry.Erase(NkString(name));
    }

    inline bool NkMaterialLibrary::IsRegistered(const char* name) const {
        return mRegistry.Find(NkString(name)) != nullptr;
    }

    inline NkMaterialInstHandle NkMaterialLibrary::CreateInstance(
        NkIRenderer* renderer, const char* templateName, const char* instName) const
    {
        NkMaterialHandle tmpl = Get(templateName);
        if (!tmpl.IsValid()) return NkMaterialInstHandle::Null();
        NkMaterialInstanceDesc d{};
        d.templateHandle = tmpl;
        if (instName) d.debugName = instName;
        return renderer->CreateMaterialInstance(d);
    }

} // namespace renderer
} // namespace nkentseu