// =============================================================================
// NkIntegrator.cpp — Intégration semi-implicite (Euler symplectique). [M0]
// =============================================================================
#include "NKPhysics/NkIntegrator.h"

namespace nkentseu {
    namespace physics {

        void NkIntegrateVelocity(NkRigidBody& b, const NkVec3f& gravity, float32 dt) noexcept {
            if (b.invMass <= 0.f) return;                       // static / kinematic : pas de force
            NkVec3f accel = b.force * b.invMass;
            if ((b.flags & NK_BODY_NO_GRAVITY) == 0) accel = accel + gravity * b.gravityScale;
            b.linearVelocity  = b.linearVelocity + accel * dt;
            // amortissement exponentiel implicite : v *= 1/(1 + c*dt)
            b.linearVelocity  = b.linearVelocity  * (1.f / (1.f + b.linearDamping  * dt));
            b.angularVelocity = b.angularVelocity * (1.f / (1.f + b.angularDamping * dt));
            b.force  = { 0.f, 0.f, 0.f };
            b.torque = { 0.f, 0.f, 0.f };
        }

        void NkIntegrateOrientation(NkRigidBody& b, float32 dt) noexcept {
            // orientation : q += 0.5 * (w⊗q) * dt, puis renormalisation.
            const NkVec3f w = b.angularVelocity;
            if (w.Dot(w) > 1e-12f) {
                const NkQuatf wq(w.x, w.y, w.z, 0.f);           // (x,y,z,w)
                b.orientation = (b.orientation + (wq * b.orientation) * (0.5f * dt)).Normalized();
            }
        }

        void NkIntegratePosition(NkRigidBody& b, float32 dt) noexcept {
            b.position = b.position + b.linearVelocity * dt;
            NkIntegrateOrientation(b, dt);
        }

    } // namespace physics
} // namespace nkentseu
