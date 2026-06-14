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
#include "NKRenderer/Materials/NkMaterialSystem.h"   // NkPBRParams, NkMaterialType, ...
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
                // M.4 Hierarchical Instances : cree un enfant heritant des params
                // du parent. Tout setter sur l'enfant marque ce champ overridé ;
                // tout setter sur le parent propage aux enfants non-overrides.
                // Le parent doit survivre tant qu'il a des enfants.
                static NkMaterial* CreateChild(NkMaterial* parent);
                static void        Destroy(NkMaterial*& mat);

                ~NkMaterial();

                // ── M.4 : Retrait d'overrides (re-link au parent) ────────────────
                // Apres ResetParameter, le param suit a nouveau le parent. No-op
                // si pas de parent ou si pas un override actif.
                NkMaterial* ResetParameter(const char* name);   // param nomme
                NkMaterial* ResetPBROverride (NkPBROverrideBit  bit);
                NkMaterial* ResetToonOverride(NkToonOverrideBit bit);

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

                // Wrappers inline zero-cost. SetScalarParameterValue prend un float,
                // SetVectorParameterValue un NkVec4f (RGBA), SetTextureParameterValue
                // une NkTexHandle. Convention UE5 : "Parameter" reference une variable
                // declaree dans le shader via @param/@color.
                NkMaterial* SetScalarParameterValue (const char* name, float32     v) {
                    return SetFloat(name, v);
                }
                NkMaterial* SetVectorParameterValue (const char* name, NkVec4f     c) {
                    return SetVec4(name, c);
                }
                NkMaterial* SetTextureParameterValue(const char* name, NkTexHandle t) {
                    return SetTexture(name, t);
                }
                NkMaterial* SetStaticSwitchParameterValue(const char* name, bool b) {
                    return SetBool(name, b);
                }

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
                NkMaterial* SetMatcapMap     (NkTexHandle t);
                NkMaterial* SetMatcapStrength(float32 v);

                // ── M.1 v0 : Layered material (2 layers PBR + masque vColor) ───────
                NkMaterial* SetLayerBase     (const NkPBRParams& p);
                NkMaterial* SetLayerTop      (const NkPBRParams& p);
                NkMaterial* SetLayerMaskSource(int32 src); // 0=R 1=G 2=B 3=A

                // ── Shadow overrides per-material (NkVSM v1) ──────────────────────
                // Permet a un materiau de specifier comment il interagit avec
                // les shadow maps, en surcharge des defaults globaux.
                NkMaterial* SetReceiveShadow      (bool b);     // false = skip shadow sample
                NkMaterial* SetShadowBiasMul      (float32 m);  // multiplicateur shadowBias
                NkMaterial* SetCastShadowAlphaTest(bool b);     // V1 reserve : alpha-tested
                bool   GetReceiveShadow()       const;
                float32 GetShadowBiasMul()      const;
                bool   GetCastShadowAlphaTest() const;

                // ── Triplanar projection (style UE5 World Aligned Texture) ─────────
                // Quand actif, la texture est projetee selon les 3 plans monde
                // (XY/YZ/XZ) et blendee par |normal|. Donne des tiles VRAIMENT
                // carrees independamment du scale/rotation du mesh, et sans
                // seams sur les coins du cube. tileSizeMeters = taille en metres
                // reels d'une tile (ex: 1.0 = 1m, 0.5 = 50cm). 0 = disabled,
                // fallback UV classique du mesh.
                // Affecte : albedo + ORM (AO/Roughness/Metallic) + normal map
                // (via UDN blend a la UE5). Pas l'emissive (souvent decoratif).
                // Cost : 3x sample par texture concernee (acceptable pour PBR).
                NkMaterial* SetTriplanarTileSize  (float32 tileSizeMeters);
                float32     GetTriplanarTileSize() const;

                // ── État ──────────────────────────────────────────────────────────
                bool          IsValid()      const;
                NkRenderQueue GetQueue()     const;
                const char*   GetName()      const;
                NkMaterialType GetType()     const;

                // Handle de l'instance interne — utilisé pour NkDrawCall3D::material.
                NkMatInstHandle GetInstHandle() const;

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
