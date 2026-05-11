#pragma once
// =============================================================================
// NkMaterial.h  — NKRenderer v4.0  (Materials/)
//
// API Unreal-style : un seul objet manipulable qui encapsule template + instance.
//
// Usage :
//   auto* mat = NkMaterial::Create(renderer->GetMaterials(), "PBR");
//   mat->SetAlbedo({1, 0, 0})->SetRoughness(0.3f)->SetMetallic(0.8f);
//   mat->SetTexture("albedo_map", myTex);
//   render3D->Submit(mesh, mat, transform);
//
//   NkMaterial::Destroy(mat);
//
// Correspondance Unreal :
//   NkMaterial                 ≈  UMaterialInstanceDynamic
//   NkMaterial::Create(sys, t) ≈  UMaterialInstanceDynamic::Create(parent, outer)
//   SetColor/SetFloat/SetTexture ≈ SetVectorParameterValue / SetScalarParameterValue
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRHI/Core/NkIDevice.h"

namespace nkentseu {
    namespace renderer {

        class NkMaterialSystem;
        class NkMaterialInstance;
        enum class NkMaterialType : uint16;

        // =========================================================================
        // NkMaterial
        // =========================================================================
        class NkMaterial {
            public:
                // ── Création / Destruction ───────────────────────────────────────
                // type  : NK_PBR_METALLIC, NK_TOON, NK_UNLIT, NK_SKIN, …
                static NkMaterial* Create(NkMaterialSystem* sys, NkMaterialType type);
                // name  : nom du template builtin ("Default_PBR", "Default_Toon", …)
                static NkMaterial* Create(NkMaterialSystem* sys, const char* templateName);
                static void        Destroy(NkMaterial*& mat);

                ~NkMaterial();

                // ── Paramètres nommés (correspondent aux @param du shader) ───────
                // Ces méthodes correspondent à SetScalarParameterValue (UE) /
                // SetFloat (Unity) — identifiées par le nom de la variable shader.
                NkMaterial* SetFloat  (const char* name, float32    v);
                NkMaterial* SetVec2   (const char* name, NkVec2f    v);
                NkMaterial* SetVec3   (const char* name, NkVec3f    v);
                NkMaterial* SetVec4   (const char* name, NkVec4f    v);
                NkMaterial* SetColor  (const char* name, NkVec4f    c);   // alias SetVec4
                NkMaterial* SetInt    (const char* name, int32      v);
                NkMaterial* SetBool   (const char* name, bool       v);
                NkMaterial* SetTexture(const char* name, NkTexHandle t);

                // ── Raccourcis PBR (SetScalarParameterValue UE-style) ────────────
                NkMaterial* SetAlbedo      (NkVec3f c, float32 a = 1.f);
                NkMaterial* SetAlbedoMap   (NkTexHandle t);
                NkMaterial* SetNormalMap   (NkTexHandle t, float32 strength = 1.f);
                NkMaterial* SetORMMap      (NkTexHandle t);     // AO+Roughness+Metallic
                NkMaterial* SetMetallic    (float32 v);
                NkMaterial* SetRoughness   (float32 v);
                NkMaterial* SetEmissive    (NkVec3f c, float32 strength = 1.f);
                NkMaterial* SetEmissiveMap (NkTexHandle t);
                NkMaterial* SetAOMap       (NkTexHandle t);
                NkMaterial* SetSubsurface  (float32 v, NkVec3f color = {1.f,0.5f,0.3f});
                NkMaterial* SetClearcoat   (float32 v, float32 roughness = 0.f);

                // ── Raccourcis Toon / NPR ────────────────────────────────────────
                NkMaterial* SetToonThreshold (float32 v);
                NkMaterial* SetToonSmooth    (float32 v);
                NkMaterial* SetToonShadow    (NkVec3f color);
                NkMaterial* SetOutline       (float32 width, NkVec3f color = {0,0,0});
                NkMaterial* SetRim           (float32 intensity, NkVec3f color = {1,1,1});
                NkMaterial* SetSpecHardness  (float32 v);

                // ── État ──────────────────────────────────────────────────────────
                bool          IsValid()   const;
                NkRenderQueue GetQueue()  const;
                const char*   GetName()   const;
                NkMaterialType GetType()  const;

                // ── Usage interne (NkRender3D/NkRender2D) ────────────────────────
                bool Bind(NkICommandBuffer* cmd, NkTextureLibrary* texLib);

            private:
                NkMaterial() = default;
                NkMaterial(const NkMaterial&) = delete;
                NkMaterial& operator=(const NkMaterial&) = delete;

                NkMaterialSystem*   mSystem   = nullptr;
                NkMaterialInstance* mInstance = nullptr;
        };

    } // namespace renderer
} // namespace nkentseu
