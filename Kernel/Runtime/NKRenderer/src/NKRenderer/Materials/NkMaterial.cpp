// =============================================================================
// NkMaterial.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkMaterial.h"
#include "NkMaterialSystem.h"

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
                default:                               tmpl = sys->DefaultPBR();       break;
            }
            if (!tmpl.IsValid()) return nullptr;
            auto* mat      = new NkMaterial();
            mat->mSystem   = sys;
            mat->mInstance = sys->CreateInstance(tmpl);
            return mat;
        }

        NkMaterial* NkMaterial::Create(NkMaterialSystem* sys, const char* templateName) {
            if (!sys || !templateName) return nullptr;
            NkMatHandle tmpl = sys->FindTemplate(NkString(templateName));
            if (!tmpl.IsValid()) return nullptr;
            auto* mat      = new NkMaterial();
            mat->mSystem   = sys;
            mat->mInstance = sys->CreateInstance(tmpl);
            return mat;
        }

        void NkMaterial::Destroy(NkMaterial*& mat) {
            if (!mat) return;
            if (mat->mSystem && mat->mInstance)
                mat->mSystem->DestroyInstance(mat->mInstance);
            delete mat;
            mat = nullptr;
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
