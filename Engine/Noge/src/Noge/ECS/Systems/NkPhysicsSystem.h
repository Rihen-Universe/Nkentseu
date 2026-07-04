#pragma once
// =============================================================================
// Noge/ECS/Systems/NkPhysicsSystem.h — pont ECS -> NKPhysics (rigid-body 3D)
// =============================================================================
// Modèle identique à NkRenderSystem (pont ECS -> NKRenderer) :
//   - POSSÈDE un physics::NkPhysicsWorld (NKCollision + NKPhysics).
//   - Pour chaque entité (NkRigidbody3D + NkCollider3D + NkTransform) sans corps,
//     crée un NkRigidBody (NkBodyDef + NkShape) et stocke l'id (collider.physicsBodyId).
//   - Avance la simulation à pas fixe (groupe FixedUpdate), puis SYNCHRONISE :
//       • dynamique : corps -> NkTransform (+ vélocités vers le composant)
//       • cinématique : NkTransform -> corps (avant le step)
// =============================================================================

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"   // NkRigidbody3D, NkCollider3D, NkBodyType
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKPhysics/NkRigidBody.h"
#include "NKCollision/NkColShapes.h"

namespace nkentseu {

    using namespace math;

    class NkPhysicsSystem final : public ecs::NkSystem {
        public:
            NkPhysicsSystem() noexcept = default;

            [[nodiscard]] ecs::NkSystemDesc Describe() const override {
                return ecs::NkSystemDesc{}
                    .Writes<ecs::NkTransform>()
                    .Writes<ecs::NkRigidbody3D>()
                    .Writes<ecs::NkCollider3D>()
                    .InGroup(ecs::NkSystemGroup::FixedUpdate)
                    .Sequential()
                    .Named("NkPhysicsSystem");
            }

            void Execute(ecs::NkWorld& world, float32 fixedDt) noexcept override;

            // Accès direct au monde physique (raycast, gravité, requêtes...).
            [[nodiscard]] physics::NkPhysicsWorld&       World()       noexcept { return mWorld; }
            [[nodiscard]] const physics::NkPhysicsWorld& World() const noexcept { return mWorld; }

        private:
            // Crée le corps physique d'une entité depuis ses composants.
            physics::NkBodyId CreateBodyFor(const ecs::NkRigidbody3D& rb,
                                            const ecs::NkCollider3D&   col,
                                            const ecs::NkTransform&    tf) noexcept;

            // NkCollider3D -> forme de collision (box/sphère/capsule).
            [[nodiscard]] static collision::NkShape MakeShape(const ecs::NkCollider3D& col) noexcept;

            // Enum Noge -> enum NKPhysics.
            [[nodiscard]] static physics::NkBodyType ConvertBodyType(ecs::NkBodyType t) noexcept {
                switch (t) {
                    case ecs::NkBodyType::Static:    return physics::NkBodyType::STATIC;
                    case ecs::NkBodyType::Kinematic: return physics::NkBodyType::KINEMATIC;
                    default:                         return physics::NkBodyType::DYNAMIC;
                }
            }

            physics::NkPhysicsWorld mWorld;   // monde physique POSSÉDÉ (gravité par défaut)
    };

} // namespace nkentseu
