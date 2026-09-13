// =============================================================================
// NkIntegrator.cpp — Intégration semi-implicite (Euler symplectique). [M0]
// =============================================================================
#include "NKPhysics/NkIntegrator.h"

namespace nkentseu {
	namespace physics {

		void NkIntegrateVelocity(NkRigidBody &b, const NkVec3f &gravity, float32 dt) noexcept {
			if (b.invMass <= 0.f)
				return; // static / kinematic : pas de force
			NkVec3f accel = b.force * b.invMass;
			if ((b.flags & NK_BODY_NO_GRAVITY) == 0)
				accel = accel + gravity * b.gravityScale;
			b.linearVelocity = b.linearVelocity + accel * dt;
			// COUPLE -> vitesse angulaire.  INTEGRE DEPUIS LE 2026-09-03.
			// Avant : `torque` etait accumule, remis a zero ligne plus bas, et
			// JAMAIS lu -- un champ inerte, invisible parce que le solveur de
			// contacts fait son angulaire en impulsions directes sans passer par
			// lui. Les deux chemins ecrivent angularVelocity depuis des SOURCES
			// DISTINCTES (impulsions de contact / couple accumule) : ils
			// s'ajoutent, ils ne se doublent pas -- verifie : aucun code du depot
			// n'ecrivait `torque` au moment de cette integration, elle est donc
			// neutre pour tout l'existant. Client qui l'a exigee : la roue de
			// vehicule, qui applique sa force EN UN POINT (ApplyForceAtPoint).
			b.angularVelocity = b.angularVelocity + NkInvInertiaApply(b, b.torque) * dt;
			// amortissement exponentiel implicite : v *= 1/(1 + c*dt)
			b.linearVelocity = b.linearVelocity * (1.f / (1.f + b.linearDamping * dt));
			b.angularVelocity = b.angularVelocity * (1.f / (1.f + b.angularDamping * dt));
			b.force = {0.f, 0.f, 0.f};
			b.torque = {0.f, 0.f, 0.f};
		}

		void NkIntegrateOrientation(NkRigidBody &b, float32 dt) noexcept {
			// orientation : q += 0.5 * (w⊗q) * dt, puis renormalisation.
			const NkVec3f w = b.angularVelocity;
			if (w.Dot(w) > 1e-12f) {
				const NkQuatf wq(w.x, w.y, w.z, 0.f); // (x,y,z,w)
				b.orientation = (b.orientation + (wq * b.orientation) * (0.5f * dt)).Normalized();
			}
		}

		void NkIntegratePosition(NkRigidBody &b, float32 dt) noexcept {
			b.position = b.position + b.linearVelocity * dt;
			NkIntegrateOrientation(b, dt);
		}

	} // namespace physics
} // namespace nkentseu
