// =============================================================================
// NKAnimPhysics/NkClipBalancePass.h
// -----------------------------------------------------------------------------
// M3.6 (NkAnima — physique d'animation façon Cascadeur) : PASSE D'ÉQUILIBRE SUR
// UN CLIP. Brique 6/6 (capstone M3). Applique la correction d'équilibre (M3.4)
// en POST-TRAITEMENT NON DESTRUCTIF sur une SÉQUENCE de poses (un clip) : chaque
// frame est ramenée en équilibre, avec LISSAGE TEMPOREL (NkBalanceSmoother) pour
// éviter les à-coups. L'entrée n'est pas modifiée (on écrit une copie corrigée) →
// toggle par personnage/scène, ré-éditable. Pure Foundation : AUCUN GPU, headless.
//
// ⚠️ RENOMMÉ LE 2026-08-14 — s'appelait `NkPhysAnimBridge`, et ce nom mentait
// deux fois : ce n'est NI un pont, NI de la physique. Aucune dynamique de corps
// rigide ici, aucune force, aucune intégration — uniquement de la CINÉMATIQUE
// (on déplace des positions de joints pour ramener le centre de masse au-dessus
// du polygone de support). Le module n'inclut ni NKPhysics ni NKCollision.
// Le vrai pont physique↔animation existe ailleurs et s'appelle `NkRagdoll`
// (`NKPhysics/NkRagdoll.h` : Build / ReadPose / SetActive / SetPoseTargets).
// Si un jour ce fichier doit parler à la physique, il passe PAR LUI.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKAnimPhysics/NkPoseMass.h"

namespace nkentseu {
	namespace animphys {

		struct NkClipBalancePass {
			public:
				// Corrige un CLIP (séquence de poses) pour l'équilibre, non destructif.
				// `posesIn` = frameCount × jointCount positions monde, row-major (frame*jointCount + j).
				// `plantedMask` = jointCount bools (appuis fixes, non déplacés). `strength` ∈ [0,1] = dosage
				// réalisme↔intention. `smoothMaxDeltaPerFrame` = borne la variation du décalage appliqué d'une
				// frame à l'autre (mètres ; grand = pas de lissage, petit = très lisse). Écrit `posesOut`
				// (redimensionné à frameCount*jointCount). Renvoie le nombre de frames devenues équilibrées.
				static int32 Correct(const math::NkVec3f *posesIn, int32 frameCount, int32 jointCount,
									  const NkPoseMass &mass, const bool *plantedMask, const math::NkVec3f *supportPts,
									  int32 supportCount, float32 strength, float32 smoothMaxDeltaPerFrame,
									  NkVector<math::NkVec3f> &posesOut,
									  const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f, 0.f});

				// Auto-test headless (aucun GPU).
				static bool SelfTest();
		};

	} // namespace animphys
} // namespace nkentseu
