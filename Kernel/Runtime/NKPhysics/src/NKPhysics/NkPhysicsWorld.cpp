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

        NkRigidBody* NkPhysicsWorld::FindByCollisionId(uint32 cid) noexcept {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) if (mBodies[i].collisionId == cid) return &mBodies[i];
            return nullptr;
        }

        // Inertie inverse en repère MONDE appliquée à un vecteur (torque -> accel ang.) :
        //   invI_world * v = R * (invInertiaDiag ⊙ (Rᵀ v))   avec R = orientation.
        static NkVec3f NkInvInertiaApply(const NkRigidBody& b, const NkVec3f& v) noexcept {
            const NkVec3f loc = b.orientation.Conjugate() * v;
            const NkVec3f sc{ loc.x * b.invInertiaDiag.x, loc.y * b.invInertiaDiag.y, loc.z * b.invInertiaDiag.z };
            return b.orientation * sc;
        }

        // ── M1+M2 : solveur de contacts (normale + frottement + angulaire) ────
        struct NkSolverPoint {
            NkVec3f rA, rB;                  // bras de levier (contact - centre de masse)
            float32 nMass = 0.f, t1Mass = 0.f, t2Mass = 0.f;
            float32 bias = 0.f;              // biais normal (Baumgarte + restitution)
            float32 nImp = 0.f, t1Imp = 0.f, t2Imp = 0.f;
            uint32  id = 0;                  // feature-id NKCollision (matching warm-start)
        };
        struct NkSolverContact {
            NkRigidBody* a; NkRigidBody* b;
            NkVec3f n, t1, t2;               // normale + 2 tangentes
            float32 friction;
            NkSolverPoint p[4]; int32 count;
        };

        // Masse effective d'une contrainte le long de `dir` (linéaire + angulaire).
        static float32 NkEffMass(const NkRigidBody& A, const NkRigidBody& B,
                                 const NkVec3f& rA, const NkVec3f& rB, const NkVec3f& dir) noexcept {
            const NkVec3f rnA = rA.Cross(dir), rnB = rB.Cross(dir);
            const float32 ang = rnA.Dot(NkInvInertiaApply(A, rnA)) + rnB.Dot(NkInvInertiaApply(B, rnB));
            const float32 k = A.invMass + B.invMass + ang;
            return k > 0.f ? 1.f / k : 0.f;
        }

        void NkPhysicsWorld::SolveContacts(float32 dt) {
            (void)dt;                                          // M4 : la vitesse n'utilise plus dt (Baumgarte retiré)
            const auto& pairs = mCollision.Pairs();
            NkVector<NkSolverContact> cs;
            for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
                const collision::NkCollisionPair& p = pairs[i];
                NkRigidBody* A = FindByCollisionId(p.a); NkRigidBody* B = FindByCollisionId(p.b);
                if (!A || !B) continue;
                if ((A->flags & NK_BODY_TRIGGER) || (B->flags & NK_BODY_TRIGGER)) continue;
                if (A->invMass + B->invMass <= 0.f) continue;
                if (!A->IsAwake() && !B->IsAwake()) continue;   // M6 : deux corps endormis -> rien à faire
                NkSolverContact c; c.a = A; c.b = B; c.n = p.manifold.normal; c.count = 0;
                // base tangente orthonormée à n
                c.t1 = (math::NkAbs(c.n.x) > 0.9f) ? c.n.Cross(NkVec3f{0,1,0}) : c.n.Cross(NkVec3f{1,0,0});
                c.t1 = c.t1.Normalized(); c.t2 = c.n.Cross(c.t1);
                c.friction = NkMixFriction(A->material, B->material);
                const float32 e = NkMixRestitution(A->material, B->material);
                for (int32 k = 0; k < p.manifold.count && k < 4; ++k) {
                    const collision::NkContactPoint3D& mp = p.manifold.points[k];
                    NkSolverPoint sp;
                    sp.rA = mp.point - A->position; sp.rB = mp.point - B->position;
                    sp.nMass  = NkEffMass(*A, *B, sp.rA, sp.rB, c.n);
                    sp.t1Mass = NkEffMass(*A, *B, sp.rA, sp.rB, c.t1);
                    sp.t2Mass = NkEffMass(*A, *B, sp.rA, sp.rB, c.t2);
                    // vitesse relative initiale (au point) pour la restitution
                    const NkVec3f vA = A->linearVelocity + A->angularVelocity.Cross(sp.rA);
                    const NkVec3f vB = B->linearVelocity + B->angularVelocity.Cross(sp.rB);
                    const float32 vn0 = (vB - vA).Dot(c.n);
                    // M4 : plus de biais Baumgarte dans la vitesse (géré par split-impulse
                    // positionnel) -> pas d'injection d'énergie. On garde la restitution.
                    const float32 rest = (vn0 < -1.f) ? -e * vn0 : 0.f;
                    sp.bias = rest;
                    sp.id = mp.id;
                    // M3 : récupérer l'impulse accumulée de la frame précédente (même feature).
                    for (uint32 w = 0; w < (uint32)mWarm.Size(); ++w) {
                        const NkWarmEntry& we = mWarm[w];
                        if (we.a == A->id && we.b == B->id && we.id == sp.id) { sp.nImp = we.n; sp.t1Imp = we.t1; sp.t2Imp = we.t2; break; }
                    }
                    c.p[c.count++] = sp;
                }
                if (c.count > 0) cs.PushBack(c);
            }
            // M3 : WARM-START — ré-appliquer les impulses accumulées avant d'itérer.
            for (uint32 i = 0; i < (uint32)cs.Size(); ++i) {
                NkSolverContact& c = cs[i];
                for (int32 k = 0; k < c.count; ++k) {
                    NkSolverPoint& sp = c.p[k];
                    const NkVec3f P = c.n * sp.nImp + c.t1 * sp.t1Imp + c.t2 * sp.t2Imp;
                    c.a->linearVelocity  = c.a->linearVelocity  - P * c.a->invMass;
                    c.a->angularVelocity = c.a->angularVelocity - NkInvInertiaApply(*c.a, sp.rA.Cross(P));
                    c.b->linearVelocity  = c.b->linearVelocity  + P * c.b->invMass;
                    c.b->angularVelocity = c.b->angularVelocity + NkInvInertiaApply(*c.b, sp.rB.Cross(P));
                }
            }
            // Itérations séquentielles : frottement (borné par μ·N) puis normale.
            for (int32 it = 0; it < mConfig.velocityIters; ++it) {
                for (uint32 i = 0; i < (uint32)cs.Size(); ++i) {
                    NkSolverContact& c = cs[i];
                    for (int32 k = 0; k < c.count; ++k) {
                        NkSolverPoint& sp = c.p[k];
                        // -- frottement (2 axes), borné par le cône de Coulomb --
                        const float32 maxF = c.friction * sp.nImp;
                        for (int32 ti = 0; ti < 2; ++ti) {
                            const NkVec3f t = (ti == 0) ? c.t1 : c.t2;
                            float32& acc = (ti == 0) ? sp.t1Imp : sp.t2Imp;
                            const float32 tm = (ti == 0) ? sp.t1Mass : sp.t2Mass;
                            const NkVec3f vA = c.a->linearVelocity + c.a->angularVelocity.Cross(sp.rA);
                            const NkVec3f vB = c.b->linearVelocity + c.b->angularVelocity.Cross(sp.rB);
                            float32 lambda = -tm * (vB - vA).Dot(t);
                            const float32 newImp = math::NkClamp(acc + lambda, -maxF, maxF);
                            lambda = newImp - acc; acc = newImp;
                            const NkVec3f P = t * lambda;
                            c.a->linearVelocity  = c.a->linearVelocity  - P * c.a->invMass;
                            c.a->angularVelocity = c.a->angularVelocity - NkInvInertiaApply(*c.a, sp.rA.Cross(P));
                            c.b->linearVelocity  = c.b->linearVelocity  + P * c.b->invMass;
                            c.b->angularVelocity = c.b->angularVelocity + NkInvInertiaApply(*c.b, sp.rB.Cross(P));
                        }
                        // -- normale (non-pénétration + restitution + Baumgarte) --
                        const NkVec3f vA = c.a->linearVelocity + c.a->angularVelocity.Cross(sp.rA);
                        const NkVec3f vB = c.b->linearVelocity + c.b->angularVelocity.Cross(sp.rB);
                        const float32 vn = (vB - vA).Dot(c.n);
                        float32 lambda = -sp.nMass * (vn - sp.bias);
                        const float32 newImp = math::NkMax(0.f, sp.nImp + lambda);
                        lambda = newImp - sp.nImp; sp.nImp = newImp;
                        const NkVec3f P = c.n * lambda;
                        c.a->linearVelocity  = c.a->linearVelocity  - P * c.a->invMass;
                        c.a->angularVelocity = c.a->angularVelocity - NkInvInertiaApply(*c.a, sp.rA.Cross(P));
                        c.b->linearVelocity  = c.b->linearVelocity  + P * c.b->invMass;
                        c.b->angularVelocity = c.b->angularVelocity + NkInvInertiaApply(*c.b, sp.rB.Cross(P));
                    }
                }
            }
            // M3 : sauvegarder les impulses pour le warm-start de la frame suivante.
            mWarm.Clear();
            for (uint32 i = 0; i < (uint32)cs.Size(); ++i) {
                const NkSolverContact& c = cs[i];
                for (int32 k = 0; k < c.count; ++k) {
                    NkWarmEntry we; we.a = c.a->id; we.b = c.b->id; we.id = c.p[k].id;
                    we.n = c.p[k].nImp; we.t1 = c.p[k].t1Imp; we.t2 = c.p[k].t2Imp;
                    mWarm.PushBack(we);
                }
            }
        }

        // ── M4 : split-impulse — projection positionnelle (sans énergie) ─────
        // Pousse les corps en chevauchement le long de la normale, proportionnellement
        // à leur masse inverse. Agit sur la POSITION (pas la vitesse) -> pas de rebond
        // parasite. Une passe par frame, facteur < 1 -> la pénétration converge vers slop.
        void NkPhysicsWorld::CorrectPositions() {
            const auto& pairs = mCollision.Pairs();
            const float32 factor = 0.8f;                       // part de pénétration corrigée par frame
            for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
                const collision::NkCollisionPair& p = pairs[i];
                NkRigidBody* A = FindByCollisionId(p.a); NkRigidBody* B = FindByCollisionId(p.b);
                if (!A || !B) continue;
                if ((A->flags & NK_BODY_TRIGGER) || (B->flags & NK_BODY_TRIGGER)) continue;
                // M6 : un corps endormi est immovable (invMass effective = 0).
                const float32 imA = A->IsAwake() ? A->invMass : 0.f;
                const float32 imB = B->IsAwake() ? B->invMass : 0.f;
                const float32 kSum = imA + imB;
                if (kSum <= 0.f) continue;
                float32 maxPen = 0.f;
                for (int32 k = 0; k < p.manifold.count; ++k) maxPen = math::NkMax(maxPen, p.manifold.points[k].depth);
                const float32 pen = maxPen - mConfig.slop;
                if (pen <= 0.f) continue;
                const NkVec3f corr = p.manifold.normal * (factor * pen / kSum);
                A->position = A->position - corr * imA;         // normale A->B : A recule, B avance
                B->position = B->position + corr * imB;
            }
        }

        // ── M6 : sommeil ──────────────────────────────────────────────────────
        // Un corps « perturbateur » peut réveiller ses voisins endormis.
        static bool NkIsDisturbing(const NkRigidBody& b, const NkPhysicsConfig& cfg) noexcept {
            if (b.type == NkBodyType::STATIC) return false;
            const float32 lv2 = b.linearVelocity.Dot(b.linearVelocity);
            const float32 av2 = b.angularVelocity.Dot(b.angularVelocity);
            const bool moving = lv2 > cfg.linearSleepTol * cfg.linearSleepTol
                             || av2 > cfg.angularSleepTol * cfg.angularSleepTol;
            return (b.type == NkBodyType::KINEMATIC) ? moving : (b.IsAwake() && moving);
        }
        static void NkWake(NkRigidBody& b) noexcept { b.flags &= ~NK_BODY_SLEEPING; b.sleepTimer = 0.f; }

        void NkPhysicsWorld::WakeContacts() {
            const auto& pairs = mCollision.Pairs();
            for (uint32 i = 0; i < (uint32)pairs.Size(); ++i) {
                NkRigidBody* A = FindByCollisionId(pairs[i].a); NkRigidBody* B = FindByCollisionId(pairs[i].b);
                if (!A || !B) continue;
                if (NkIsDisturbing(*A, mConfig) && B->IsDynamic() && !B->IsAwake()) NkWake(*B);
                if (NkIsDisturbing(*B, mConfig) && A->IsDynamic() && !A->IsAwake()) NkWake(*A);
            }
        }

        void NkPhysicsWorld::UpdateSleep(float32 dt) {
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                NkRigidBody& b = mBodies[i];
                if (!b.IsDynamic() || !b.IsAwake()) continue;
                const float32 lv2 = b.linearVelocity.Dot(b.linearVelocity);
                const float32 av2 = b.angularVelocity.Dot(b.angularVelocity);
                if (lv2 < mConfig.linearSleepTol * mConfig.linearSleepTol
                 && av2 < mConfig.angularSleepTol * mConfig.angularSleepTol) b.sleepTimer += dt;
                else b.sleepTimer = 0.f;
                if (b.sleepTimer >= mConfig.sleepTime) {
                    b.flags |= NK_BODY_SLEEPING;
                    b.linearVelocity = { 0,0,0 }; b.angularVelocity = { 0,0,0 };
                }
            }
        }

        void NkPhysicsWorld::Step(float32 dt) {
            if (dt <= 0.f) return;
            // 1) forces -> vitesses
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                NkRigidBody& b = mBodies[i];
                if (b.type == NkBodyType::DYNAMIC && b.IsAwake()) NkIntegrateVelocity(b, mConfig.gravity, dt);
            }
            // 2) détection (broadphase DBVH + manifolds multi-points)
            mCollision.Step();
            // 2b) réveiller les corps endormis touchés par un perturbateur
            WakeContacts();
            // 3) solveur de contacts (vitesses, sans Baumgarte) + warm-start
            SolveContacts(dt);
            // 4) vitesses -> positions (DYNAMIC + KINEMATIC : le kinematic suit sa vitesse imposée)
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                NkRigidBody& b = mBodies[i];
                if (b.type != NkBodyType::STATIC && b.IsAwake()) NkIntegratePosition(b, dt);
            }
            // 5) correction positionnelle (split-impulse) — n'affecte que les invMass>0
            CorrectPositions();
            // 6) re-synchroniser les shapes de collision sur la pose finale
            for (uint32 i = 0; i < (uint32)mBodies.Size(); ++i) {
                NkRigidBody& b = mBodies[i];
                if (b.type == NkBodyType::STATIC) continue;     // static : jamais re-sync
                if (collision::NkBody* cb = mCollision.GetBody(b.collisionId)) {
                    collision::NkShape s = cb->shape;
                    NkSyncShape(s, b.position - collision::NkShapeCenter3D(s), b.orientation);  // resynchro absolue
                    mCollision.SetShape(b.collisionId, s);
                }
            }
            // 7) mise en sommeil des corps immobiles
            UpdateSleep(dt);
        }

    } // namespace physics
} // namespace nkentseu
