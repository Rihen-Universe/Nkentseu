// =============================================================================
// NkMaterial.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkMaterial.h"
#include "NkMaterialSystem.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {
    namespace renderer {

        // ── Création ──────────────────────────────────────────────────────────
        NkMaterial* NkMaterial::Create(NkMaterialSystem* sys, NkMaterialType type) {
            if (!sys) return nullptr;
            NkMatHandle tmpl;
            switch (type) {
                case NkMaterialType::NK_PBR_METALLIC:  tmpl = sys->DefaultPBR();       break;
                case NkMaterialType::NK_PBR_SPECULAR:  tmpl = sys->DefaultPBR();       break;
                case NkMaterialType::NK_TOON:          tmpl = sys->DefaultToon();      break;
                case NkMaterialType::NK_UNLIT:         tmpl = sys->DefaultUnlit();     break;
                case NkMaterialType::NK_WIREFRAME_MAT: tmpl = sys->DefaultWireframe(); break;
                case NkMaterialType::NK_SKIN:          tmpl = sys->DefaultSkin();      break;
                case NkMaterialType::NK_HAIR:          tmpl = sys->DefaultHair();      break;
                case NkMaterialType::NK_ANIME:         tmpl = sys->DefaultAnime();     break;
                case NkMaterialType::NK_ARCHIVIZ:      tmpl = sys->DefaultArchviz();   break;
                case NkMaterialType::NK_REFL_FLOOR:   tmpl = sys->DefaultReflFloor(); break;
                case NkMaterialType::NK_LAYERED:      tmpl = sys->DefaultLayered();   break;
                case NkMaterialType::NK_LAYERED_V1:   tmpl = sys->DefaultLayeredV1(); break;
                default:                               tmpl = sys->DefaultPBR();       break;
            }
            if (!tmpl.IsValid()) return nullptr;
            // NkMaterial a un constructeur prive : on alloue via NKMemory puis
            // placement-new ici (acces autorise depuis un membre de la classe).
            auto* mat      = static_cast<NkMaterial*>(memory::NkAlloc((nk_size)sizeof(NkMaterial)));
            if (!mat) return nullptr;
            new (mat) NkMaterial();
            mat->mSystem   = sys;
            mat->mInstance = sys->CreateInstance(tmpl);
            return mat;
        }

        NkMaterial* NkMaterial::Create(NkMaterialSystem* sys, const char* templateName) {
            if (!sys || !templateName) return nullptr;
            NkMatHandle tmpl = sys->FindTemplate(NkString(templateName));
            if (!tmpl.IsValid()) return nullptr;
            auto* mat      = static_cast<NkMaterial*>(memory::NkAlloc((nk_size)sizeof(NkMaterial)));
            if (!mat) return nullptr;
            new (mat) NkMaterial();
            mat->mSystem   = sys;
            mat->mInstance = sys->CreateInstance(tmpl);
            return mat;
        }

        // M.4 : Cree un enfant heritant des params du parent. L'enfant utilise
        // le meme template, ses propres UBO/descSet, et a un lien live vers le
        // parent. Modifier le parent met automatiquement a jour les enfants
        // dont le champ touche n'est PAS override.
        NkMaterial* NkMaterial::CreateChild(NkMaterial* parent) {
            if (!parent || !parent->mSystem || !parent->mInstance) return nullptr;
            auto* mat = static_cast<NkMaterial*>(memory::NkAlloc((nk_size)sizeof(NkMaterial)));
            if (!mat) return nullptr;
            new (mat) NkMaterial();
            mat->mSystem   = parent->mSystem;
            mat->mInstance = parent->mSystem->CreateChildInstance(parent->mInstance);
            if (!mat->mInstance) { mat->~NkMaterial(); memory::NkFree(mat); return nullptr; }
            return mat;
        }

        void NkMaterial::Destroy(NkMaterial*& mat) {
            if (!mat) return;
            if (mat->mSystem && mat->mInstance)
                mat->mSystem->DestroyInstance(mat->mInstance);
            mat->~NkMaterial();
            memory::NkFree(mat);
            mat = nullptr;
        }

        // ── M.4 : Retrait d'overrides (re-link au parent) ────────────────────
        NkMaterial* NkMaterial::ResetParameter(const char* name) {
            if (mInstance && name) mInstance->ResetNamedOverride(NkString(name));
            return this;
        }
        NkMaterial* NkMaterial::ResetPBROverride(NkPBROverrideBit bit) {
            if (mInstance) mInstance->ResetPBROverride(bit);
            return this;
        }
        NkMaterial* NkMaterial::ResetToonOverride(NkToonOverrideBit bit) {
            if (mInstance) mInstance->ResetToonOverride(bit);
            return this;
        }

        NkMaterial::~NkMaterial() {
            // Instance deja liberee par Destroy — ne pas double-free.
            // Si l'utilisateur detruit sans passer par Destroy, on fuit
            // l'instance (pas de crash). Pattern Unreal : toujours utiliser Destroy.
        }

        // ── Paramètres nommés ─────────────────────────────────────────────────
        NkMaterial* NkMaterial::SetFloat(const char* n, float32 v) {
            if (mInstance) mInstance->SetFloat(n, v); return this;
        }
        NkMaterial* NkMaterial::SetVec2(const char* n, NkVec2f v) {
            if (mInstance) mInstance->SetVec2(n, v); return this;
        }
        NkMaterial* NkMaterial::SetVec3(const char* n, NkVec3f v) {
            if (mInstance) mInstance->SetVec3(n, v); return this;
        }
        NkMaterial* NkMaterial::SetVec4(const char* n, NkVec4f v) {
            if (mInstance) mInstance->SetVec4(n, v); return this;
        }
        NkMaterial* NkMaterial::SetColor(const char* n, NkVec4f c) {
            if (mInstance) mInstance->SetColor(n, c); return this;
        }
        NkMaterial* NkMaterial::SetInt(const char* n, int32 v) {
            if (mInstance) mInstance->SetInt(n, v); return this;
        }
        NkMaterial* NkMaterial::SetBool(const char* n, bool v) {
            if (mInstance) mInstance->SetBool(n, v); return this;
        }
        NkMaterial* NkMaterial::SetTexture(const char* n, NkTexHandle t) {
            if (mInstance) mInstance->SetTexture(n, t); return this;
        }

        // ── PBR convenience ───────────────────────────────────────────────────
        NkMaterial* NkMaterial::SetAlbedo(NkVec3f c, float32 a) {
            if (mInstance) mInstance->SetAlbedo(c, a); return this;
        }
        NkMaterial* NkMaterial::SetAlbedoMap(NkTexHandle t) {
            if (mInstance) mInstance->SetAlbedoMap(t); return this;
        }
        NkMaterial* NkMaterial::SetNormalMap(NkTexHandle t, float32 s) {
            if (mInstance) mInstance->SetNormalMap(t, s); return this;
        }
        NkMaterial* NkMaterial::SetORMMap(NkTexHandle t) {
            if (mInstance) mInstance->SetORMMap(t); return this;
        }
        NkMaterial* NkMaterial::SetMetallic(float32 v) {
            if (mInstance) mInstance->SetMetallic(v); return this;
        }
        NkMaterial* NkMaterial::SetRoughness(float32 v) {
            if (mInstance) mInstance->SetRoughness(v); return this;
        }
        NkMaterial* NkMaterial::SetEmissive(NkVec3f c, float32 s) {
            if (mInstance) mInstance->SetEmissive(c, s); return this;
        }
        NkMaterial* NkMaterial::SetEmissiveMap(NkTexHandle t) {
            if (mInstance) mInstance->SetEmissiveMap(t); return this;
        }
        NkMaterial* NkMaterial::SetAOMap(NkTexHandle t) {
            if (mInstance) mInstance->SetAOMap(t); return this;
        }
        NkMaterial* NkMaterial::SetSubsurface(float32 v, NkVec3f c) {
            if (mInstance) mInstance->SetSubsurface(v, c); return this;
        }
        NkMaterial* NkMaterial::SetClearcoat(float32 v, float32 r) {
            if (mInstance) mInstance->SetClearcoat(v, r); return this;
        }

        // ── Toon / NPR convenience ─────────────────────────────────────────────
        NkMaterial* NkMaterial::SetToonThreshold(float32 v) {
            if (mInstance) mInstance->SetToonThreshold(v); return this;
        }
        NkMaterial* NkMaterial::SetToonSmooth(float32 v) {
            if (mInstance) mInstance->SetToonSmooth(v); return this;
        }
        NkMaterial* NkMaterial::SetToonShadow(NkVec3f c) {
            if (mInstance) mInstance->SetToonShadowColor(c); return this;
        }
        NkMaterial* NkMaterial::SetOutline(float32 w, NkVec3f c) {
            if (mInstance) mInstance->SetOutline(w, c); return this;
        }
        NkMaterial* NkMaterial::SetRim(float32 intensity, NkVec3f c) {
            if (mInstance) mInstance->SetRim(intensity, c); return this;
        }
        NkMaterial* NkMaterial::SetSpecHardness(float32 v) {
            if (mInstance) mInstance->SetSpecHardness(v); return this;
        }
        NkMaterial* NkMaterial::SetMatcapMap(NkTexHandle t) {
            if (mInstance) mInstance->SetMatcapMap(t); return this;
        }
        NkMaterial* NkMaterial::SetMatcapStrength(float32 v) {
            if (mInstance) mInstance->SetMatcapStrength(v); return this;
        }

        // ── M.1 v0 : Layered passthrough ──────────────────────────────────────
        NkMaterial* NkMaterial::SetLayerBase(const NkPBRParams& p) {
            if (mInstance) mInstance->SetLayerBase(p); return this;
        }
        NkMaterial* NkMaterial::SetLayerTop(const NkPBRParams& p) {
            if (mInstance) mInstance->SetLayerTop(p); return this;
        }
        NkMaterial* NkMaterial::SetLayerMaskSource(int32 src) {
            if (mInstance) mInstance->SetLayerMaskSource(src); return this;
        }

        // ── Shadow overrides (NkVSM v1) ──────────────────────────────────────
        NkMaterial* NkMaterial::SetReceiveShadow(bool b) {
            if (mInstance) mInstance->mReceiveShadow = b; return this;
        }
        NkMaterial* NkMaterial::SetShadowBiasMul(float32 m) {
            if (mInstance) mInstance->mShadowBiasMul = m; return this;
        }
        NkMaterial* NkMaterial::SetCastShadowAlphaTest(bool b) {
            if (mInstance) mInstance->mCastShadowAlphaTest = b; return this;
        }
        bool NkMaterial::GetReceiveShadow() const {
            return mInstance ? mInstance->mReceiveShadow : true;
        }
        float32 NkMaterial::GetShadowBiasMul() const {
            return mInstance ? mInstance->mShadowBiasMul : 1.f;
        }
        bool NkMaterial::GetCastShadowAlphaTest() const {
            return mInstance ? mInstance->mCastShadowAlphaTest : false;
        }

        // ── Triplanar projection (style UE5 World Aligned Texture) ────────────
        NkMaterial* NkMaterial::SetTriplanarTileSize(float32 tileSizeMeters) {
            if (mInstance) mInstance->mTriplanarTileSize = tileSizeMeters;
            return this;
        }
        float32 NkMaterial::GetTriplanarTileSize() const {
            return mInstance ? mInstance->mTriplanarTileSize : 0.f;
        }

        // ── État ──────────────────────────────────────────────────────────────
        bool NkMaterial::IsValid() const {
            return mSystem && mInstance;
        }

        NkRenderQueue NkMaterial::GetQueue() const {
            if (!mInstance) return NkRenderQueue::NK_OPAQUE;
            return mInstance->GetQueue();
        }

        const char* NkMaterial::GetName() const {
            if (!mInstance || !mSystem) return "";
            auto tmpl = mInstance->GetTemplate();
            if (!tmpl.IsValid()) return "";
            auto* n = mSystem->GetTemplateName(tmpl);
            return n ? n->CStr() : "";
        }

        NkMaterialType NkMaterial::GetType() const {
            if (!mInstance || !mSystem) return NkMaterialType::NK_PBR_METALLIC;
            auto tmpl = mInstance->GetTemplate();
            if (!tmpl.IsValid()) return NkMaterialType::NK_PBR_METALLIC;
            return mSystem->GetTemplateType(tmpl);
        }

        NkMatInstHandle NkMaterial::GetInstHandle() const {
            return mInstance ? mInstance->GetHandle() : NkMatInstHandle::Null();
        }

        bool NkMaterial::Bind(NkICommandBuffer* cmd, NkTextureLibrary* texLib) {
            if (!mSystem || !mInstance) return false;
            (void)texLib;  // mTexLib interne utilise par BindInstance
            return mSystem->BindInstance(cmd, mInstance);
        }

    } // namespace renderer
} // namespace nkentseu
