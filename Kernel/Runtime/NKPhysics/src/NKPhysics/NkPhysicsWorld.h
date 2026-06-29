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

                // Réglages.
                void SetGravity(const NkVec3f& g) noexcept { mConfig.gravity = g; }
                const NkPhysicsConfig& Config() const noexcept { return mConfig; }
                NkVector<NkRigidBody>&       Bodies() noexcept { return mBodies; }
                const NkVector<NkRigidBody>& Bodies() const noexcept { return mBodies; }

                // Requêtes (déléguées à NKCollision, résultat -> NkBodyId). [spec M10]
                // bool Raycast(const collision::NkRay3D& r, NkBodyId& hitBody, ...);
                // uint32 OverlapShape(const collision::NkShape& s, NkVector<NkBodyId>& out, ...);

            private:
                NkPhysicsConfig        mConfig;
                collision::NkWorld     mCollision;   // détection (DBVH, manifolds)
                NkVector<NkRigidBody>  mBodies;
                NkContactSolver        mSolver;
                NkBodyId               mNextId = 1u;
        };

    } // namespace physics
} // namespace nkentseu
