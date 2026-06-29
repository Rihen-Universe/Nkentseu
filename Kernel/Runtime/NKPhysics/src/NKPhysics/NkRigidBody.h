#pragma once
// =============================================================================
// NkRigidBody.h — Corps rigide : état dynamique (données). [SCAFFOLD]
// La détection (forme/AABB) est déléguée à un body NKCollision référencé par id.
// =============================================================================
#include "NKPhysics/NkPhysicsMaterial.h"
#include "NKMath/NkQuat.h"
#include "NKCollision/NkColShapes.h"   // forme de repos (locale) pour la synchro

namespace nkentseu {
    namespace physics {

        using NkQuatf = nkentseu::math::NkQuatf;

        // Descripteur de création (passé à NkPhysicsWorld::CreateBody).
        struct NkBodyDef {
            NkBodyType        type = NkBodyType::DYNAMIC;
            NkVec3f           position{};
            NkQuatf           orientation{};
            NkVec3f           linearVelocity{};
            NkVec3f           angularVelocity{};
            NkPhysicsMaterial material{};
            uint32            flags = NK_BODY_NONE;
            float32           linearDamping  = 0.0f;
            float32           angularDamping  = 0.05f;
            float32           gravityScale    = 1.0f;
            uint32            layer = 0x1u;          // transmis à NKCollision
            uint32            mask  = 0xFFFFFFFFu;
            void*             user  = nullptr;
        };

        // État runtime d'un corps rigide. masse/inertie INVERSES = 0 pour static/kinematic.
        struct NkRigidBody {
            NkBodyType type = NkBodyType::DYNAMIC;
            // pose
            NkVec3f position{};
            NkQuatf orientation{};
            // cinématique
            NkVec3f linearVelocity{};
            NkVec3f angularVelocity{};
            // dynamique (inverses pour traiter ∞ proprement)
            float32 invMass = 1.0f;
            NkVec3f invInertiaDiag{ 1.f, 1.f, 1.f };   // inertie inverse (repère local, diag)
            // forces accumulées sur la frame
            NkVec3f force{};
            NkVec3f torque{};
            // propriétés
            NkPhysicsMaterial material{};
            float32 linearDamping  = 0.0f;
            float32 angularDamping  = 0.05f;
            float32 gravityScale    = 1.0f;
            uint32  flags = NK_BODY_NONE;
            uint32  layer = 0x1u;          // bitmask d'appartenance (filtre des requêtes COM/moment)
            float32 sleepTimer = 0.f;
            // liens
            NkBodyId id = NK_INVALID_BODY;
            uint32   collisionId = 0;   // id du body NKCollision associé
            collision::NkShape restShape;   // forme en repère LOCAL (transformée par pose -> shape monde)
            void*    user = nullptr;

            NK_FORCE_INLINE bool IsDynamic()  const noexcept { return type == NkBodyType::DYNAMIC; }
            NK_FORCE_INLINE bool IsAwake()     const noexcept { return (flags & NK_BODY_SLEEPING) == 0; }

            // Applique une force au centre de masse (intégrée au prochain Step).
            NK_FORCE_INLINE void ApplyForce(const NkVec3f& f) noexcept { force = force + f; }
            // Impulse instantané (modifie directement la vitesse).
            NK_FORCE_INLINE void ApplyImpulse(const NkVec3f& imp) noexcept { linearVelocity = linearVelocity + imp * invMass; }
        };

    } // namespace physics
} // namespace nkentseu
