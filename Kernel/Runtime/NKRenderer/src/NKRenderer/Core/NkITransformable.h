#pragma once
// =============================================================================
// NkITransformable.h  — NKRenderer v5.0  (Core/)
//
// Interface UE-style pour tout objet qui possede une transform monde
// (position, rotation, scale, parent). Implementee par NkSceneNode et tous
// ses derives (NkStaticMesh, NkSkeletalMesh, NkLight, NkCamera...).
//
// Dual avec NkIDrawable : un objet peut etre transformable sans etre drawable
// (ex : pivot vide), drawable sans etre transformable (ex : skybox, debug HUD),
// ou les deux (la majorite : meshes, lights, particles).
// =============================================================================
#include "NkRendererTypes.h"
#include "NKMath/NkMat.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkQuat.h"

namespace nkentseu {
    namespace renderer {

        // Transform locale stockee comme TRS (translation + quaternion + scale).
        // Plus efficace en memoire que mat4 (32 bytes vs 64), evite les derives
        // numeriques de skew/non-uniform scale, et facilite les animations.
        struct NkTransform {
            NkVec3f translation = {0,0,0};
            NkQuatf rotation    = {0,0,0,1};   // identite
            NkVec3f scale       = {1,1,1};

            // Construit la matrice de transformation 4x4 column-major.
            NkMat4f ToMatrix() const noexcept {
                NkMat4f S = NkMat4f::Scale(scale);
                NkMat4f R = rotation.ToMat4();
                NkMat4f T = NkMat4f::Translate(translation);
                return T * R * S;
            }

            static NkTransform Identity() noexcept { return {}; }
        };

        // =====================================================================
        // NkITransformable
        // =====================================================================
        class NkITransformable {
            public:
                virtual ~NkITransformable() = default;

                // ── Local transform ─────────────────────────────────────────────
                virtual const NkTransform& GetLocalTransform() const noexcept = 0;
                virtual void                SetLocalTransform(const NkTransform& t) noexcept = 0;

                // Helpers : modifient la transform locale composante par composante
                virtual void SetLocalPosition(NkVec3f pos)  noexcept = 0;
                virtual void SetLocalRotation(NkQuatf rot)  noexcept = 0;
                virtual void SetLocalScale   (NkVec3f s)    noexcept = 0;
                virtual NkVec3f GetLocalPosition() const noexcept = 0;
                virtual NkQuatf GetLocalRotation() const noexcept = 0;
                virtual NkVec3f GetLocalScale()    const noexcept = 0;

                // ── World transform (cache, recalcule si dirty) ────────────────
                // Inclut la composition avec le parent si attache.
                virtual const NkMat4f& GetWorldMatrix() const noexcept = 0;
                virtual NkVec3f         GetWorldPosition() const noexcept = 0;

                // ── Hierarchie ─────────────────────────────────────────────────
                virtual NkITransformable* GetParent() const noexcept = 0;
                virtual void               SetParent(NkITransformable* parent) noexcept = 0;

                // Marque la transform world comme stale (a recalculer au prochain
                // GetWorldMatrix). Appele par les setters et propage aux enfants.
                virtual void MarkTransformDirty() noexcept = 0;
        };

    } // namespace renderer
} // namespace nkentseu
