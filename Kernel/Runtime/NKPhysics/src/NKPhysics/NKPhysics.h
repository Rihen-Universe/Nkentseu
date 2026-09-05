#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NKPhysics.h — Include unique du module de dynamique du corps rigide. [SCAFFOLD]
//
//   #include "NKPhysics/NKPhysics.h"
//   nkentseu::physics::NkPhysicsWorld world({ /*gravity*/ {0,-9.81f,0} });
//   auto id = world.CreateBody(def, nkentseu::collision::NkShape::Box3D({0,5,0},{.5f,.5f,.5f}));
//   world.Step(1.f/60.f);
//
// Posé sur NKCollision (détection). Bibliothèque de simulation PURE (sans ECS) :
// l'intégration gameplay (composants) se fait dans Noge. Voir ROADMAP.md.
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"
#include "NKPhysics/NkPhysicsMaterial.h"
#include "NKPhysics/NkRigidBody.h"
#include "NKPhysics/NkIntegrator.h"
#include "NKPhysics/NkContactSolver.h"
#include "NKPhysics/NkJoint.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKPhysics/NkRagdoll.h"
#include "NKPhysics/NkCloth.h" // tissu XPBD (2026-09-05)
#include "NKPhysics/NkMannequin.h" // corps skinne : capsules par os, point-dans-maillage (2026-09-05)
#include "NKPhysics/NkBodySDF.h"  // champ de distance signe du corps skinne (2026-09-05, lot 2)
#include "NKPhysics/NkGarment.h"   // vetements proceduraux sur squelette (2026-09-05)
