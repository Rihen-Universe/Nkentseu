// =============================================================================
// NKRenderer/Tools/Animation/NkPoseBalancer.h
// -----------------------------------------------------------------------------
// M3.4 (NkAnima — physique d'animation façon Cascadeur) : OPTIMISEUR DE POSE
// SOUS CONTRAINTE D'ÉQUILIBRE. Brique 4/6 de M3 (le cœur). Ajuste une pose
// proposée (clip d'animation ou IA) pour ramener le CENTRE DE MASSE (M3.1) au-
// dessus du POLYGONE DE SUPPORT (M3.2/M3.3) — la « signature Cascadeur ».
//
// V1 = correction par décalage horizontal du corps vers le centroïde des appuis,
// pondérée par `strength` (curseur RÉALISME ↔ INTENTION artistique : 0 = pose
// inchangée, 1 = COM ramené sur le centroïde de support). Itérable / lissable.
// ⏳ Raffinements : correction ciblée tronc/bassin (pieds plantés) via la
// hiérarchie du squelette, respect des limites d'angle articulaires (NkIKSystem),
// lissage multi-frame. Pure Foundation : AUCUN GPU, testable headless.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKRenderer/Tools/Animation/NkPoseMass.h"

namespace nkentseu {
	namespace renderer {

		// Résultat d'une correction d'équilibre.
		struct NkBalanceCorrection {
				bool wasBalanced = false;			// équilibre AVANT correction
				bool nowBalanced = false;			// équilibre APRÈS
				math::NkVec3f shift{0.f, 0.f, 0.f}; // décalage monde (horizontal) appliqué à la pose
				float32 marginBefore = 0.f;			// marge d'équilibre avant (>0 = équilibré)
				float32 marginAfter = 0.f;			// marge après
		};

		struct NkPoseBalancer {
			public:
				// Corrige `jointWorld` (IN-PLACE) pour ramener le COM au-dessus du support, par décalage
				// horizontal du corps vers le centroïde des appuis, pondéré par `strength` ∈ [0,1].
				// `mass` = modèle de masse (NkPoseMass) associé à jointWorld. `supportPts` = points de
				// contact au sol (NkContactDetector::DetectSupportPoints). Ne touche pas la composante
				// verticale (le long de `groundNormal`). Renvoie l'état avant/après + le décalage.
				static NkBalanceCorrection BalanceByShift(math::NkVec3f *jointWorld, int32 count,
														  const NkPoseMass &mass, const math::NkVec3f *supportPts,
														  int32 supportCount, float32 strength,
														  const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f,
																										   0.f});

				// Auto-test headless (aucun GPU).
				static bool SelfTest();
		};

	} // namespace renderer
} // namespace nkentseu
