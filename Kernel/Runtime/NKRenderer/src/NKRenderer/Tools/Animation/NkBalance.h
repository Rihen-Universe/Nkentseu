// =============================================================================
// NKRenderer/Tools/Animation/NkBalance.h
// -----------------------------------------------------------------------------
// M3.2 (NkAnima — physique d'animation façon Cascadeur) : SOLVEUR D'ÉQUILIBRE.
// Brique 2/6 de M3 (après masse/COM). Test STATIQUE : le centre de masse (COM,
// cf. NkPoseMass) projeté sur le sol tombe-t-il dans le POLYGONE DE SUPPORT
// (enveloppe convexe des points de contact au sol : pieds, mains si quadrupède) ?
//   équilibré  ⇔  proj(COM) ∈ polygone de support.
// Fournit aussi une MARGE signée (distance COM→bord : >0 dedans, <0 dehors) →
// mesure de stabilité, et le début du déséquilibre DYNAMIQUE (vitesse du COM
// projetée : vers où il « bascule »).
//
// Pure Foundation (NKMath + NKContainers) : AUCUN GPU, testable headless. Le
// rendu (polygone + point COM) est laissé au consommateur (démo/éditeur).
//
// ⚠️ Des points de contact PONCTUELS (2 pieds = 1 segment) donnent un polygone
// dégénéré : pour un vrai équilibre latéral, fournir les COINS de chaque appui
// (≥3 points formant une aire). Le module gère 0/1/2/≥3 points proprement.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		// Résultat d'un test d'équilibre statique.
		struct NkBalanceResult {
				bool balanced = false;			   // proj(COM) dans le polygone de support
				float32 margin = -1.0f;			   // distance signée COM→bord (>0 dedans, <0 dehors), en mètres
				math::NkVec2f comOnPlane{0.f, 0.f}; // COM projeté dans le repère du plan (u,v)
				int32 supportCount = 0;			   // nb de points de l'enveloppe de support
		};

		struct NkBalance {
			public:
				// Test d'équilibre STATIQUE. `com` = centre de masse monde (NkPoseMass::ComputeCOM).
				// `supportPts` = points de contact au sol (positions monde). `groundNormal` = normale
				// du sol (défaut +Y). Projette tout sur le plan, construit l'enveloppe convexe des
				// appuis, teste l'appartenance du COM et calcule la marge signée.
				static NkBalanceResult EvaluateStatic(const math::NkVec3f &com, const math::NkVec3f *supportPts,
													  int32 count,
													  const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f, 0.f});

				// Direction de bascule (dynamique naissante) : composante de la vitesse du COM dans
				// le plan du sol, normalisée. {0,0} si vitesse ~nulle. Sert à anticiper la chute.
				static math::NkVec2f TipDirection(const math::NkVec3f &comVelocity,
												  const math::NkVec3f &groundNormal = math::NkVec3f{0.f, 1.f, 0.f});

				// Auto-test headless (aucun GPU).
				static bool SelfTest();
		};

	} // namespace renderer
} // namespace nkentseu
