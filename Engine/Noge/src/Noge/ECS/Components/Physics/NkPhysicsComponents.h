#pragma once
// =============================================================================
// NKECS/Components/Physics/NkPhysicsComponents.h
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"

using namespace nkentseu::math;

namespace nkentseu {
    namespace ecs {

        // =====================================================================
        // NkRigidbodyComponent
        // =====================================================================
        enum class NkRigidbodyType : nk_uint8 {
            Dynamic  = 0,   // affecté par forces et gravité
            Kinematic,      // déplacé par code, pas par forces
            Static          // immobile (bake en BVH)
        };

        struct NkRigidbodyComponent {
            NkRigidbodyType type     = NkRigidbodyType::Dynamic;
            float32  mass            = 1.f;
            float32  drag            = 0.f;
            float32  angularDrag     = 0.05f;
            bool     useGravity      = true;
            bool     freezeRotX     = false;
            bool     freezeRotY     = false;
            bool     freezeRotZ     = false;

            // Calculés par NkPhysicsSystem
            NkVec3f  velocity        = {0,0,0};
            NkVec3f  angularVelocity = {0,0,0};
            NkVec3f  force           = {0,0,0};   // accumulateur de frame
        };
        NK_COMPONENT(NkRigidbodyComponent)

        // =====================================================================
        // NkColliderComponent — forme de collision
        // =====================================================================
        enum class NkColliderShape : nk_uint8 {
            Box = 0, Sphere, Capsule, Cylinder, Mesh
        };

        struct NkColliderComponent {
            NkColliderShape shape    = NkColliderShape::Box;
            NkVec3f  offset          = {0,0,0};    // offset depuis le transform
            NkVec3f  size            = {1,1,1};    // box: extents, sphere: radius en x
            float32  friction        = 0.5f;
            float32  restitution     = 0.0f;
            bool     isTrigger       = false;
            nk_uint8 layer           = 0;
        };
        NK_COMPONENT(NkColliderComponent)

        // =====================================================================
        // NkVelocityComponent — vélocité pure (pour systèmes simples sans rigidbody)
        // =====================================================================
        struct NkVelocityComponent {
            NkVec3f linear  = {0,0,0};
            NkVec3f angular = {0,0,0};
        };
        NK_COMPONENT(NkVelocityComponent)

    } // namespace ecs
} // namespace nkentseu
