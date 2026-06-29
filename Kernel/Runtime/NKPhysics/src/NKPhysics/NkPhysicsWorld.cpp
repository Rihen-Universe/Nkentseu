// =============================================================================
// NkPhysicsWorld.cpp — Monde de simulation du corps rigide. [M0]
// M0 : intégration semi-implicite (gravité) + délégation détection à NKCollision +
// synchronisation des shapes. Pas encore de solveur (les corps se traversent) -> M1.
// =============================================================================
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKPhysics/NkIntegrator.h"

namespace nkentseu {
    namespace physics {

        using collision::NkShapeType;

        // ── Masse + inertie (diagonale) depuis la forme et la densité ─────────
        static void NkComputeMassProps(const collision::NkShape& s, float32 density,
                                       NkBodyType type, uint32 flags,
                                       float32& invMass, NkVec3f& invInertiaDiag) noexcept {
            if (type != NkBodyType::DYNAMIC) { invMass = 0.f; invInertiaDiag = { 0,0,0 }; return; }
            float32 mass = 1.f; NkVec3f I{ 1.f, 1.f, 1.f };       // défauts
            switch (s.type) {
                case NkShapeType::NK_SPHERE: case NkShapeType::NK_CIRCLE2D: {
                    const float32 r = s.radius;
                    const float32 vol = (s.type == NkShapeType::NK_SPHERE) ? (4.f / 3.f) * 3.14159265f * r * r * r
                                                                           : 3.14159265f * r * r;
                    mass = density * vol;
                    const float32 i = (s.type == NkShapeType::NK_SPHERE) ? 0.4f * mass * r * r : 0.5f * mass * r * r;
                    I = { i, i, i }; break;
                }
                case NkShapeType::NK_BOX3D: {
                    const NkVec3f h = s.p1; const float32 a = 2.f*h.x, b = 2.f*h.y, c = 2.f*h.z;
                    mass = density * (a * b * c);
                    const float32 k = mass / 12.f;
                    I = { k*(b*b+c*c), k*(a*a+c*c), k*(a*a+b*b) }; break;
                }
                case NkShapeType::NK_BOX2D: {
                    const NkVec3f h = s.p1; const float32 a = 2.f*h.x, b = 2.f*h.y;
                    mass = density * (a * b);
                    const float32 iz = mass * (a*a + b*b) / 12.f;
                    I = { 1.f, 1.f, iz }; break;                  // 2D : seule la rotation Z compte
                }
                default: {                                        // approx : boîte englobante (AABB)
                    collision::NkAABB3D bb = collision::NkComputeAABB3D(s);
                    NkVec3f d = bb.max - bb.min;
                    if (d.x <= 0.f) d.x = 1.f; if (d.y <= 0.f) d.y = 1.f; if (d.z <= 0.f) d.z = 1.f;
                    mass = density * (d.x * d.y * d.z);
                    const float32 k = mass / 12.f;
                    I = { k*(d.y*d.y+d.z*d.z), k*(d.x*d.x+d.z*d.z), k*(d.x*d.x+d.y*d.y) }; break;
                }
            }
            if (mass < 1e-6f) mass = 1e-6f;
            invMass = 1.f / mass;
            const bool fixedRot = (flags & NK_BODY_FIXED_ROT) != 0;
            invInertiaDiag = fixedRot ? NkVec3f{ 0,0,0 }
                                      : NkVec3f{ I.x > 0 ? 1.f/I.x : 0.f, I.y > 0 ? 1.f/I.y : 0.f, I.z > 0 ? 1.f/I.z : 0.f };
        }

        // Translate/oriente la shape de collision pour suivre la pose du corps.
        static void NkSyncShape(collision::NkShape& s, const NkVec3f& delta, const NkQuatf& orient) noexcept {
            s.p0 = s.p0 + delta;
            if (s.type == NkShapeType::NK_CAPSULE3D || s.type == NkShapeType::NK_SEGMENT2D
                || s.type == NkShapeType::NK_CAPSULE2D) s.p1 = s.p1 + delta;
            if (s.type == NkShapeType::NK_BOX3D) s.orientation = orient;
        }

        NkPhysicsWorld::NkPhysicsWorld(const NkPhysicsConfig& cfg) noexcept : mConfig(cfg) {}

        NkBodyId NkPhysicsWorld::CreateBody(const NkBodyDef& def, const collision::NkShape& shape) {
            NkRigidBody b;
            b.id = mNextId++;
            b.type = def.type;
            b.position = def.position; b.orientation = def.orientation;
            b.linearVelocity = def.linearVelocity; b.angularVelocity = def.angularVelocity;
            b.material = def.material; b.flags = def.flags;
            b.linearDamping = def.linearDamping; b.angularDamping = def.angularDamping;
            b.gravityScale = def.gravityScale; b.user = def.user;
            NkComputeMassProps(shape, def.material.density, def.type, def.flags, b.invMass, b.invInertiaDiag);
            b.collisionId = mCollision.AddBody(shape, def.layer, def.mask, def.user);
            if (def.flags & NK_BODY_TRIGGER) mCollision.SetTrigger(b.collisionId, true);
            mBodies.PushBack(b);
            return b.id;
        }

        void NkPhysicsWorld::DestroyBody(NkBodyId id) {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i)
                if (mBodies[i].id == id) { mCollision.RemoveBody(mBodies[i].collisionId); mBodies.RemoveAt(i); return; }
        }

        NkRigidBody* NkPhysicsWorld::GetBody(NkBodyId id) noexcept {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) if (mBodies[i].id == id) return &mBodies[i];
            return nullptr;
        }
        const NkRigidBody* NkPhysicsWorld::GetBody(NkBodyId id) const noexcept {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) if (mBodies[i].id == id) return &mBodies[i];
            return nullptr;
        }

        void NkPhysicsWorld::Step(float32 dt) {
            if (dt <= 0.f) return;
            // 1) forces -> vitesses
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                NkRigidBody& b = mBodies[i];
                if (b.type == NkBodyType::DYNAMIC && b.IsAwake()) NkIntegrateVelocity(b, mConfig.gravity, dt);
            }
            // 2) détection (broadphase DBVH + manifolds) — exploitée par le solveur en M1.
            mCollision.Step();
            // (M1) NkContactSolver : préparer + warm-start + résoudre les vitesses ICI.
            // 3) vitesses -> positions + 4) re-synchroniser les shapes de collision
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                NkRigidBody& b = mBodies[i];
                if (b.type != NkBodyType::DYNAMIC || !b.IsAwake()) continue;
                const NkVec3f oldPos = b.position;
                NkIntegratePosition(b, dt);
                if (collision::NkBody* cb = mCollision.GetBody(b.collisionId)) {
                    collision::NkShape s = cb->shape;
                    NkSyncShape(s, b.position - oldPos, b.orientation);
                    mCollision.SetShape(b.collisionId, s);
                }
            }
            // (M4) correction positionnelle ; (M6) sommeil — à venir.
        }

    } // namespace physics
} // namespace nkentseu
