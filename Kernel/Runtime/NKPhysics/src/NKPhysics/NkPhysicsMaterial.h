#pragma once
// =============================================================================
// NkPhysicsMaterial.h — Matériau physique (friction/restitution). [SCAFFOLD]
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"

namespace nkentseu {
    namespace physics {

        // Propriétés de surface d'un corps. La densité sert à calculer masse+inertie
        // depuis la forme (volume/aire). friction = coefficient de Coulomb.
        struct NkPhysicsMaterial {
            float32  density        = 1.0f;
            float32  staticFriction = 0.6f;
            float32  dynamicFriction= 0.4f;
            float32  restitution    = 0.0f;  // 0 = pas de rebond, 1 = rebond parfait
            NkCombine frictionMode  = NkCombine::MULTIPLY; // sqrt(a*b) usuel -> MULTIPLY puis sqrt
            NkCombine restitutionMode = NkCombine::MAX;
        };

        NK_FORCE_INLINE float32 NkCombineValue(float32 a, float32 b, NkCombine mode) noexcept {
            switch (mode) {
                case NkCombine::MIN:      return a < b ? a : b;
                case NkCombine::MAX:      return a > b ? a : b;
                case NkCombine::MULTIPLY: return a * b;
                case NkCombine::AVERAGE:
                default:                  return 0.5f * (a + b);
            }
        }

        // Coefficients effectifs d'une paire de matériaux en contact.
        NK_FORCE_INLINE float32 NkMixFriction(const NkPhysicsMaterial& a, const NkPhysicsMaterial& b) noexcept {
            return NkCombineValue(a.dynamicFriction, b.dynamicFriction, a.frictionMode);
        }
        NK_FORCE_INLINE float32 NkMixRestitution(const NkPhysicsMaterial& a, const NkPhysicsMaterial& b) noexcept {
            return NkCombineValue(a.restitution, b.restitution, a.restitutionMode);
        }

    } // namespace physics
} // namespace nkentseu
