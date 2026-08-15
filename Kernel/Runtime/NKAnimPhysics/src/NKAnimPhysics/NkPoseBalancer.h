// =============================================================================
// NKAnimPhysics/NkPoseBalancer.h
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
#include "NKAnimPhysics/NkPoseMass.h"

namespace nkentseu {
	namespace animphys {

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
				// V1 — Corrige `jointWorld` (IN-PLACE) pour ramener le COM au-dessus du support, par décalage
				// horizontal du CORPS ENTIER vers le centroïde des appuis, pondéré par `strength` ∈ [0,1].
				// `mass` = modèle de masse (NkPoseMass) associé à jointWorld. `supportPts` = points de
				// contact au sol (NkContactDetector::DetectSupportPoints). Ne touche pas la composante
				// verticale (le long de `groundNormal`). Renvoie l'état avant/après + le décalage.
				// ⚠️ Grossier (glisse tout le personnage) — préférer BalanceByUpperShift dès qu'on connaît les appuis.
				static NkBalanceCorrection BalanceByShift(math::NkVec3f *jointWorld, int32 count,
														  const NkPoseMass &mass, const math::NkVec3f *supportPts,
														  int32 supportCount, float32 strength,
														  const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f,
																										   0.f});

				// V2 — Correction « PIEDS PLANTÉS » : ne déplace QUE les joints NON plantés (le haut du corps),
				// itérativement, pour amener le COM au-dessus du support SANS bouger les appuis (réaliste :
				// on balance le bassin/tronc, les pieds restent au sol). `plantedMask[j]` = true si le joint j
				// est un appui fixe (ne pas déplacer). `strength` ∈ [0,1] = curseur réalisme↔intention.
				// `maxIters` = itérations de convergence (la correction est quasi-linéaire → 2-3 suffisent).
				static NkBalanceCorrection BalanceByUpperShift(math::NkVec3f *jointWorld, int32 count,
															   const NkPoseMass &mass, const bool *plantedMask,
															   const math::NkVec3f *supportPts, int32 supportCount,
															   float32 strength, int32 maxIters = 6,
															   const math::NkVec3f &groundNormal = math::NkVec3f{
																   0.f, 1.f, 0.f});

				// Auto-test headless (aucun GPU).
				static bool SelfTest();
		};

		// Lissage MULTI-FRAME de la correction d'équilibre (anti-à-coups). Limite la VITESSE de
		// variation du décalage appliqué d'une frame à l'autre → pas de saut brusque quand la pose
		// (ou la cible) change vite. Stateful : une instance par personnage/piste.
		struct NkBalanceSmoother {
			public:
				// Renvoie le décalage lissé à appliquer cette frame : se rapproche de `target` d'au plus
				// `maxDeltaPerFrame` (mètres) par rapport à la frame précédente.
				math::NkVec3f Smooth(const math::NkVec3f &target, float32 maxDeltaPerFrame);
				void Reset();

			private:
				math::NkVec3f mPrev{0.f, 0.f, 0.f};
				bool mHas = false;
		};

	} // namespace animphys
} // namespace nkentseu
