#pragma once
// =============================================================================
// NkContactSolver.h — Solveur de contacts par impulses séquentielles. [SCAFFOLD]
// Consomme les NkManifold3D de NKCollision (multi-points) ; warm-starting via
// NkContactPoint::id (matché contre GetPreviousManifold). À implémenter (M1..M4).
// =============================================================================
#include "NKPhysics/NkRigidBody.h"
#include "NKCollision/NkColTypes.h"

namespace nkentseu {
    namespace physics {

        // Un point de contact prêt pour le solveur (masse effective + accumulateurs).
        struct NkContactConstraintPoint {
            NkVec3f rA{}, rB{};               // bras de levier (contact - centre de masse)
            float32 normalMass  = 0.f;        // masse effective le long de la normale
            float32 tangentMass[2] = { 0.f, 0.f };
            float32 bias        = 0.f;        // biais de restitution / Baumgarte
            float32 normalImpulse  = 0.f;     // accumulé (warm-start)
            float32 tangentImpulse[2] = { 0.f, 0.f };
            uint32  id = 0;                   // feature-id NKCollision (matching)
        };

        // Contrainte de contact entre deux corps (1 à 4 points).
        struct NkContactConstraint {
            NkBodyId a = 0, b = 0;
            NkVec3f  normal{};                // A -> B
            NkVec3f  tangent[2];
            float32  friction = 0.f, restitution = 0.f;
            NkContactConstraintPoint points[4];
            int32    count = 0;
        };

        // Solveur séquentiel (Box2D-like) — INTERFACE (impl jalons M1..M4) :
        //   Prepare()      : construit les contraintes depuis les manifolds + warm-start
        //   WarmStart()    : ré-applique les impulses accumulées de la frame précédente
        //   SolveVelocity(): N itérations (normale puis frottement, cône de Coulomb)
        //   SolvePosition(): correction positionnelle (Baumgarte / split-impulse)
        class NkContactSolver {
            public:
                // (signatures indicatives — à figer à l'implémentation)
                void Prepare(/* world, pairs, dt, config */) noexcept;
                void WarmStart() noexcept;
                bool SolveVelocity() noexcept;   // renvoie true si convergé
                bool SolvePosition() noexcept;
        };

    } // namespace physics
} // namespace nkentseu
