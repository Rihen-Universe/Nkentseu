// =============================================================================
// Noge/ECS/Systems/NkPhysicsSystem.cpp — pont ECS -> NKPhysics (rigid-body 3D)
// =============================================================================
#include "NkPhysicsSystem.h"

namespace nkentseu {

    using namespace ecs;
    using namespace physics;
    using namespace collision;
    using namespace math;

    // -------------------------------------------------------------------------
    // NkCollider3D -> forme de collision (centre = offset local dans le corps).
    // -------------------------------------------------------------------------
    NkShape NkPhysicsSystem::MakeShape(const NkCollider3D& col) noexcept {
        const NkVec3f c = col.center;
        switch (col.shape) {
            case NkCollider3DShape::Sphere:
                return NkShape::Sphere(c, col.sphereRadius);

            case NkCollider3DShape::Capsule: {
                const float32 h = col.capsuleHeight * 0.5f;   // demi-hauteur du segment
                const NkVec3f axis =
                    (col.capsuleDir == NkCapsuleDirection3D::X) ? NkVec3f{ h, 0.f, 0.f } :
                    (col.capsuleDir == NkCapsuleDirection3D::Z) ? NkVec3f{ 0.f, 0.f, h } :
                                                                  NkVec3f{ 0.f, h, 0.f };
                return NkShape::Capsule3D(c - axis, c + axis, col.capsuleRadius);
            }

            case NkCollider3DShape::Box:
            default:
                // boxSize = dimensions PLEINES -> demi-extents pour la forme.
                return NkShape::Box3D(c, col.boxSize * 0.5f);
        }
    }

    // -------------------------------------------------------------------------
    // Crée le corps physique d'une entité depuis ses composants.
    // -------------------------------------------------------------------------
    NkBodyId NkPhysicsSystem::CreateBodyFor(const NkRigidbody3D& rb,
                                            const NkCollider3D&   col,
                                            const NkTransform&    tf) noexcept {
        NkBodyDef def;
        def.type            = ConvertBodyType(rb.bodyType);
        def.position        = tf.localPosition;
        def.orientation     = tf.localRotation;
        def.linearVelocity  = rb.velocity;
        def.angularVelocity = rb.angularVelocity;
        def.gravityScale    = rb.gravityScale;
        def.linearDamping   = rb.drag;
        def.angularDamping  = rb.angularDrag;

        // Matériau : friction/restitution du rigidbody (sinon celles du collider).
        def.material.staticFriction  = rb.friction;
        def.material.dynamicFriction = rb.friction;
        def.material.restitution     = rb.restitution;

        // Filtrage de collision (layer/mask). layer ECS (index) -> bit.
        def.layer = (col.layer < 32u) ? (1u << col.layer) : 0x1u;
        def.mask  = col.layerMask;

        return mWorld.CreateBody(def, MakeShape(col));
    }

    // -------------------------------------------------------------------------
    // Execute — pas fixe : crée les corps, avance la simulation, synchronise.
    // -------------------------------------------------------------------------
    void NkPhysicsSystem::Execute(ecs::NkWorld& world, float32 fixedDt) noexcept {
        // 1. Création des corps manquants + push des cinématiques (transform -> corps).
        world.Query<NkRigidbody3D, NkCollider3D, NkTransform>()
            .ForEach([&](NkEntityId, NkRigidbody3D& rb, NkCollider3D& col, NkTransform& tf) {
                if (!col.enabled) return;

                if (col.physicsBodyId == 0) {
                    col.physicsBodyId = static_cast<uint64>(CreateBodyFor(rb, col, tf));
                    return;
                }

                // Corps cinématique : piloté par le transform -> on pousse sa pose.
                if (rb.bodyType == ecs::NkBodyType::Kinematic) {
                    if (NkRigidBody* body = mWorld.GetBody(static_cast<NkBodyId>(col.physicsBodyId))) {
                        body->position    = tf.localPosition;
                        body->orientation = tf.localRotation;
                    }
                }
            });

        // 2. Avance la simulation (collision + dynamique) d'un pas fixe.
        mWorld.Step(fixedDt);

        // 3. Synchronisation corps -> transform pour les corps DYNAMIQUES.
        world.Query<NkRigidbody3D, NkCollider3D, NkTransform>()
            .ForEach([&](NkEntityId, NkRigidbody3D& rb, const NkCollider3D& col, NkTransform& tf) {
                if (col.physicsBodyId == 0 || rb.bodyType != ecs::NkBodyType::Dynamic) return;

                const NkRigidBody* body = mWorld.GetBody(static_cast<NkBodyId>(col.physicsBodyId));
                if (!body) return;

                tf.localPosition  = body->position;
                tf.localRotation  = body->orientation;
                tf.worldDirty     = true;
                rb.velocity        = body->linearVelocity;
                rb.angularVelocity = body->angularVelocity;
            });
    }

} // namespace nkentseu
