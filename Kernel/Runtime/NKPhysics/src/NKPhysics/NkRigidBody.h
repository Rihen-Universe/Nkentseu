#pragma once
// =============================================================================
// NkRigidBody.h — Corps rigide : état dynamique (données). [SCAFFOLD]
// La détection (forme/AABB) est déléguée à un body NKCollision référencé par id.
// =============================================================================
#include "NKPhysics/NkPhysicsMaterial.h"
#include "NKMath/NkQuat.h"
#include "NKCollision/NkColShapes.h" // forme de repos (locale) pour la synchro

namespace nkentseu {
	namespace physics {

		using NkQuatf = nkentseu::math::NkQuatf;

		// Descripteur de création (passé à NkPhysicsWorld::CreateBody).
		struct NkBodyDef {
				NkBodyType type = NkBodyType::DYNAMIC;
				NkVec3f position{};
				NkQuatf orientation{};
				NkVec3f linearVelocity{};
				NkVec3f angularVelocity{};
				NkPhysicsMaterial material{};
				uint32 flags = NK_BODY_NONE;
				float32 linearDamping = 0.0f;
				float32 angularDamping = 0.05f;
				float32 gravityScale = 1.0f;
				uint32 layer = 0x1u; // transmis à NKCollision
				uint32 mask = 0xFFFFFFFFu;
				void *user = nullptr;
		};

		// État runtime d'un corps rigide. masse/inertie INVERSES = 0 pour static/kinematic.
		struct NkRigidBody {
				NkBodyType type = NkBodyType::DYNAMIC;
				// pose
				NkVec3f position{};
				NkQuatf orientation{};
				// cinématique
				NkVec3f linearVelocity{};
				NkVec3f angularVelocity{};
				// dynamique (inverses pour traiter ∞ proprement)
				float32 invMass = 1.0f;
				NkVec3f invInertiaDiag{1.f, 1.f, 1.f}; // inertie inverse (repère local, diag)
				// forces accumulées sur la frame
				NkVec3f force{};
				NkVec3f torque{};
				// propriétés
				NkPhysicsMaterial material{};
				float32 linearDamping = 0.0f;
				float32 angularDamping = 0.05f;
				float32 gravityScale = 1.0f;
				uint32 flags = NK_BODY_NONE;
				uint32 layer = 0x1u; // bitmask d'appartenance (filtre des requêtes COM/moment)
				float32 sleepTimer = 0.f;
				// liens
				NkBodyId id = NK_INVALID_BODY;
				uint32 collisionId = 0;		  // id du body NKCollision associé
				collision::NkShape restShape; // forme en repère LOCAL (transformée par pose -> shape monde)
				void *user = nullptr;

				NK_FORCE_INLINE bool IsDynamic() const noexcept {
					return type == NkBodyType::DYNAMIC;
				}

				NK_FORCE_INLINE bool IsAwake() const noexcept {
					return (flags & NK_BODY_SLEEPING) == 0;
				}

				// Applique une force au centre de masse (intégrée au prochain Step).
				NK_FORCE_INLINE void ApplyForce(const NkVec3f &f) noexcept {
					force = force + f;
				}

				// Impulse instantané (modifie directement la vitesse).
				NK_FORCE_INLINE void ApplyImpulse(const NkVec3f &imp) noexcept {
					linearVelocity = linearVelocity + imp * invMass;
				}

				// Force appliquee EN UN POINT du monde : la part au centre de masse
				// ET le couple qu'elle produit. AJOUTE LE 2026-09-03 : avant, seule
				// ApplyForce (centre de masse) existait -- une roue, un propulseur,
				// une voile ne pouvaient ni tanguer ni faire tourner leur corps.
				NK_FORCE_INLINE void ApplyForceAtPoint(const NkVec3f &f, const NkVec3f &pWorld) noexcept {
					force = force + f;
					torque = torque + (pWorld - position).Cross(f);
				}
		};

		// ── Inertie en repere MONDE, appliquee a un vecteur ───────────────────
		// Remontees ici le 2026-09-03 depuis NkPhysicsWorld.cpp, ou elles etaient
		// `static` -- donc invisibles de l integrateur, qui en avait besoin pour
		// integrer `torque`. Le manque etait dans le socle, on le comble dans le
		// socle : une seule formule, partagee par le solveur, les joints, l integrateur
		// et bientot les roues. Pas de neuvieme copie.
		//   invI_world * v = R * (invInertiaDiag ⊙ (Rᵀ v))   avec R = orientation.
		NK_FORCE_INLINE NkVec3f NkInvInertiaApply(const NkRigidBody &b, const NkVec3f &v) noexcept {
			const NkVec3f loc = b.orientation.Conjugate() * v;
			const NkVec3f sc{loc.x * b.invInertiaDiag.x, loc.y * b.invInertiaDiag.y, loc.z * b.invInertiaDiag.z};
			return b.orientation * sc;
		}
		//   I_world * w  (inertie DIRECTE, pour le moment cinetique) -- 0 si infinie.
		NK_FORCE_INLINE NkVec3f NkInertiaApply(const NkRigidBody &b, const NkVec3f &w) noexcept {
			const NkVec3f loc = b.orientation.Conjugate() * w;
			const NkVec3f I{b.invInertiaDiag.x > 0.f ? 1.f / b.invInertiaDiag.x : 0.f,
							b.invInertiaDiag.y > 0.f ? 1.f / b.invInertiaDiag.y : 0.f,
							b.invInertiaDiag.z > 0.f ? 1.f / b.invInertiaDiag.z : 0.f};
			return b.orientation * NkVec3f{loc.x * I.x, loc.y * I.y, loc.z * I.z};
		}

	} // namespace physics
} // namespace nkentseu
