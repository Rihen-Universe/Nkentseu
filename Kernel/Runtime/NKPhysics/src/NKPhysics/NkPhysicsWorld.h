#pragma once
// =============================================================================
// NkPhysicsWorld.h — Monde de simulation du corps rigide. [SCAFFOLD / SPEC]
//
// Possède EN INTERNE un collision::NkWorld (détection) ; chaque NkRigidBody
// référence un body de collision (collisionId). Boucle Step(dt) :
//   1. intégrer forces -> vitesses          (NkIntegrator, gravité/damping)
//   2. collision.Step()                      (broadphase DBVH + manifolds NKCollision)
//   3. solveur de contacts (vitesse)         (NkContactSolver : impulses + warm-start)
//   4. intégrer vitesses -> positions        (NkIntegrator)
//   5. correction positionnelle              (anti-enfoncement)
//   6. re-synchroniser les shapes collision  (collision.SetShape)
//   7. sommeil / réveil des îlots            (M6)
//
// Les requêtes (raycast/overlap/shapecast/sweep) sont déléguées à NKCollision et
// remontées au niveau NkRigidBody.
// =============================================================================
#include "NKPhysics/NkRigidBody.h"
#include "NKPhysics/NkContactSolver.h"
#include "NKPhysics/NkJoint.h"
#include "NKCollision/NKCollision.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace physics {

        class NkPhysicsWorld {
            public:
                explicit NkPhysicsWorld(const NkPhysicsConfig& cfg = {}) noexcept;

                // Crée un corps (def + forme de collision) ; renvoie son id. [spec M0]
                NkBodyId CreateBody(const NkBodyDef& def, const collision::NkShape& shape);
                void     DestroyBody(NkBodyId id);
                NkRigidBody*       GetBody(NkBodyId id) noexcept;
                const NkRigidBody* GetBody(NkBodyId id) const noexcept;

                // Avance la simulation de `dt` (cf. boucle en tête de fichier). [spec M0..M6]
                void Step(float32 dt);

                // ── Articulations (M7) ───────────────────────────────────────
                // Joint à distance : garde les 2 ancres (monde) à leur distance courante.
                NkJointId CreateDistanceJoint(NkBodyId a, NkBodyId b, const NkVec3f& anchorAWorld, const NkVec3f& anchorBWorld);
                // Joint ball (point-à-point) : les 2 corps partagent le pivot (monde).
                NkJointId CreateBallJoint(NkBodyId a, NkBodyId b, const NkVec3f& pivotWorld);
                // Joint revolute (charnière 1 DOF) : pivot + axe de rotation (monde) — coude, genou, trappe.
                NkJointId CreateRevoluteJoint(NkBodyId a, NkBodyId b, const NkVec3f& pivotWorld, const NkVec3f& axisWorld);
                void      DestroyJoint(NkJointId id);
                NkVector<NkJoint>&       Joints() noexcept { return mJoints; }
                const NkVector<NkJoint>& Joints() const noexcept { return mJoints; }

                // Pilotage (utile pour les corps KINEMATIC : plateformes, ascenseurs…).
                void SetLinearVelocity(NkBodyId id, const NkVec3f& v) noexcept { if (NkRigidBody* b = GetBody(id)) b->linearVelocity = v; }
                void SetAngularVelocity(NkBodyId id, const NkVec3f& w) noexcept { if (NkRigidBody* b = GetBody(id)) b->angularVelocity = w; }

                // Réglages.
                void SetGravity(const NkVec3f& g) noexcept { mConfig.gravity = g; }
                const NkPhysicsConfig& Config() const noexcept { return mConfig; }
                NkVector<NkRigidBody>&       Bodies() noexcept { return mBodies; }
                const NkVector<NkRigidBody>& Bodies() const noexcept { return mBodies; }

                // Requêtes (déléguées à NKCollision, résultat -> NkBodyId). [spec M10]
                // bool Raycast(const collision::NkRay3D& r, NkBodyId& hitBody, ...);
                // uint32 OverlapShape(const collision::NkShape& s, NkVector<NkBodyId>& out, ...);

            private:
                // Impulse accumulée d'un point de contact, conservée pour le warm-start
                // (clé = paire de corps + feature-id NKCollision).
                struct NkWarmEntry { uint32 a = 0, b = 0, id = 0; float32 n = 0.f, t1 = 0.f, t2 = 0.f; };

                NkRigidBody* FindByCollisionId(uint32 cid) noexcept;
                void         SolveContacts(float32 dt);     // M1..M3 : impulses séquentielles + warm-start
                void         CorrectPositions();            // M4 : split-impulse (projection positionnelle)
                void         WakeContacts();                // M6 : réveiller les corps touchés par un perturbateur
                void         UpdateSleep(float32 dt);       // M6 : endormir les corps immobiles
                void         SolveJoints(float32 dt);       // M7 : contraintes d'articulation

                NkPhysicsConfig          mConfig;
                collision::NkWorld       mCollision;   // détection (DBVH, manifolds)
                NkVector<NkRigidBody>    mBodies;
                NkContactSolver          mSolver;
                NkVector<NkWarmEntry>    mWarm;         // cache d'impulses (frame précédente)
                NkVector<NkJoint>        mJoints;       // articulations (M7)
                NkBodyId                 mNextId = 1u;
                NkJointId                mNextJointId = 1u;
        };

    } // namespace physics
} // namespace nkentseu
