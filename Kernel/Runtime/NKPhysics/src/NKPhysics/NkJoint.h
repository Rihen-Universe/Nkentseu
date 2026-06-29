#pragma once
// =============================================================================
// NkJoint.h — Articulations (contraintes entre 2 corps). [M7] ZÉRO STL.
// Primitives GÉNÉRIQUES (aucune notion de morphologie) : assemblées en n'importe
// quel squelette (humanoïde, animal, créature, mécanique). Résolues dans le même
// solveur séquentiel que les contacts (masse effective + biais + warm-start).
//
//   M7 cut 1 : DISTANCE (longueur cible) + BALL (point-à-point, 3 DOF) = épaule/hanche.
//   À venir  : REVOLUTE (pivot 1 DOF + limites), PRISMATIC, WELD, + moteurs/drives (M8).
// =============================================================================
#include "NKPhysics/NkPhysicsTypes.h"
#include "NKMath/NkQuat.h"

namespace nkentseu {
    namespace physics {

        using NkQuatf = nkentseu::math::NkQuatf;

        using NkJointId = uint32;
        static constexpr NkJointId NK_INVALID_JOINT = 0u;

        enum class NkJointType : uint8 {
            DISTANCE = 0,   // garde 2 ancres à une distance cible (corde, entretoise)
            BALL,           // point-à-point : les 2 ancres coïncident (épaule, hanche)
            REVOLUTE,       // [M7+] pivot 1 DOF (coude, genou, roue) + limites
            PRISMATIC,      // [M7+] glissière 1 DOF (piston, ascenseur) + limites
            WELD            // [M7+] soudure rigide (position + orientation)
        };

        // Articulation entre les corps `a` et `b` (tous deux réels ; pour un point fixe,
        // utiliser un corps STATIC comme ancre). Ancres en repère LOCAL (relatif au COM).
        struct NkJoint {
            NkJointType type = NkJointType::BALL;
            NkBodyId    a = NK_INVALID_BODY, b = NK_INVALID_BODY;
            NkVec3f     localAnchorA{};      // ancre dans le repère local de A
            NkVec3f     localAnchorB{};      // ancre dans le repère local de B
            NkVec3f     localAxisA{ 0,0,1 }; // REVOLUTE : axe de charnière (repère local A)
            NkVec3f     localAxisB{ 0,0,1 }; // REVOLUTE : axe de charnière (repère local B)
            float32     restLength = 0.f;    // DISTANCE : longueur cible
            NkQuatf     refRotation{};       // WELD : orientation relative cible (qA⁻¹·qB à la création)
            NkVec3f     impulse{};           // warm-start linéaire (ball/point-à-point : 3 axes ; distance : .x)
            NkVec3f     angImpulse{};        // warm-start angulaire (weld : 3 axes monde)
            bool        enabled = true;
        };

    } // namespace physics
} // namespace nkentseu
