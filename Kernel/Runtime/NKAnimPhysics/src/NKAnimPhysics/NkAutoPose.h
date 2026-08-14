// =============================================================================
// NKAnimPhysics/NkAutoPose.h
// -----------------------------------------------------------------------------
// M3.5 (NkAnima — physique d'animation façon Cascadeur) : AUTO-POSING. Brique 5/6.
// Génère des poses INTERMÉDIAIRES physiquement plausibles entre deux clés : au lieu
// d'un simple lerp (qui peut faire basculer le personnage), on interpole PUIS on
// passe la pose par le correcteur d'équilibre (M3.4, NkPoseBalancer) → l'entre-deux
// reste en équilibre. Pondération réglable (curseur intention artistique ↔ physique).
// Pure Foundation (NKMath + NKContainers) : AUCUN GPU, testable headless.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKAnimPhysics/NkPoseMass.h"

namespace nkentseu {
	namespace renderer {

		struct NkAutoPose {
			public:
				// Interpole (lerp des positions monde) entre `poseA` et `poseB` à `t` ∈ [0,1], PUIS corrige
				// l'équilibre pour que la pose intermédiaire reste plausible. Écrit le résultat dans `out`
				// (redimensionné à `count`). `plantedMask` (optionnel, sinon nullptr) → correction « pieds
				// plantés » (M3.4 V2) ; sinon décalage corps entier (V1). `balanceStrength` ∈ [0,1] = curseur
				// (0 = lerp brut, 1 = COM ramené sur le support). `supportPts` = appuis au sol.
				static void BlendBalanced(const math::NkVec3f *poseA, const math::NkVec3f *poseB, int32 count,
										  float32 t, const NkPoseMass &mass, const bool *plantedMask,
										  const math::NkVec3f *supportPts, int32 supportCount, float32 balanceStrength,
										  NkVector<math::NkVec3f> &out,
										  const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f, 0.f});

				// Auto-test headless (aucun GPU).
				static bool SelfTest();
		};

	} // namespace renderer
} // namespace nkentseu
