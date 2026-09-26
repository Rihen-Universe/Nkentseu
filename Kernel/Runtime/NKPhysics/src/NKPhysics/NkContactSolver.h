#pragma once
// =============================================================================
// NkContactSolver.h — Solveur de contacts par impulses séquentielles. [SCAFFOLD]
// Consomme les NkManifold3D de NKCollision (multi-points) ; warm-starting via
// NkContactPoint::id (matché contre GetPreviousManifold). À implémenter (M1..M4).
//
// ⚠️ LES CONTACTS SONT BEL ET BIEN RÉSOLUS — MAIS PAS ICI.
//    Cette classe est une COQUILLE : ses quatre méthodes n'ont pas de corps et
//    le membre `NkPhysicsWorld::mSolver` n'est appelé nulle part. Le solveur
//    qui tourne est `NkPhysicsWorld::SolveContacts` (NkPhysicsWorld.cpp:418) :
//    impulses séquentielles, frottement de Coulomb, warm-start par `mWarm`.
//    Mesuré le 2026-09-26 : une bille lâchée de 6 m s'arrête à 0,9999 m pour
//    un attendu de 1,0 m (`NkDemoContact --mesure`).
//
//    Sans cette phrase, qui ouvre ce fichier pour savoir si les contacts sont
//    résolus conclut que non. Le code ne se lit pas dans l'ordre où il tourne.
//
//    CONDITION DE RETRAIT de cet avertissement : le jour où le solveur
//    migrera ici (M1..M4) et où `mSolver` sera réellement appelé par
//    `Substep`, cette phrase doit partir avec la coquille.
// =============================================================================
#include "NKPhysics/NkRigidBody.h"
#include "NKCollision/NkColTypes.h"

namespace nkentseu {
	namespace physics {

		// Un point de contact prêt pour le solveur (masse effective + accumulateurs).
		struct NkContactConstraintPoint {
				NkVec3f rA{}, rB{};		  // bras de levier (contact - centre de masse)
				float32 normalMass = 0.f; // masse effective le long de la normale
				float32 tangentMass[2] = {0.f, 0.f};
				float32 bias = 0.f;			 // biais de restitution / Baumgarte
				float32 normalImpulse = 0.f; // accumulé (warm-start)
				float32 tangentImpulse[2] = {0.f, 0.f};
				uint32 id = 0; // feature-id NKCollision (matching)
		};

		// Contrainte de contact entre deux corps (1 à 4 points).
		struct NkContactConstraint {
				NkBodyId a = 0, b = 0;
				NkVec3f normal{}; // A -> B
				NkVec3f tangent[2];
				float32 friction = 0.f, restitution = 0.f;
				NkContactConstraintPoint points[4];
				int32 count = 0;
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
				bool SolveVelocity() noexcept; // renvoie true si convergé
				bool SolvePosition() noexcept;
		};

	} // namespace physics
} // namespace nkentseu
