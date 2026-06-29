#pragma once
// =============================================================================
// NkIntegrator.h — Intégration semi-implicite (Euler symplectique). [SCAFFOLD]
//   v += (gravity*gravityScale + force*invMass) * dt ; v *= 1/(1+damping*dt)
//   x += v * dt ; orientation += 0.5 * w * orientation * dt (normalisée)
// À implémenter au jalon M0.
// =============================================================================
#include "NKPhysics/NkRigidBody.h"

namespace nkentseu {
    namespace physics {

        // Intègre les forces -> vitesses (gravité + amortissement). [spec]
        void NkIntegrateVelocity(NkRigidBody& b, const NkVec3f& gravity, float32 dt) noexcept;
        // Intègre les vitesses -> pose (position + orientation). [spec]
        void NkIntegratePosition(NkRigidBody& b, float32 dt) noexcept;

    } // namespace physics
} // namespace nkentseu
