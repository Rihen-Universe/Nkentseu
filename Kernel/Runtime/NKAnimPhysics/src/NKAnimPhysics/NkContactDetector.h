// =============================================================================
// NKAnimPhysics/NkContactDetector.h
// -----------------------------------------------------------------------------
// M3.3 (NkAnima — physique d'animation façon Cascadeur) : SOLVEUR DE CONTACTS.
// Brique 3/6 de M3. Détecte quelles EXTRÉMITÉS (pieds, mains) touchent le sol,
// produit les POINTS DE SUPPORT (extrémités en contact, projetées au sol) qui
// alimentent le polygone de support de NkBalance (M3.2). Ferme la boucle :
//     pose → COM (M3.1)  +  contacts au sol (M3.3)  →  équilibre (M3.2).
//
// V1 = sol PLAN (point + normale). Le raycast dans une heightfield/collision
// (pentes, escaliers) et le foot-locking temporel (anti-glissement, stateful)
// sont des raffinements ultérieurs. Pure Foundation (NKMath + NKContainers) :
// AUCUN GPU, testable headless.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace animphys {

		// Contact d'une extrémité avec le sol.
		struct NkGroundContact {
				bool inContact = false;			  // extrémité au niveau du sol (à `threshold` près)
				math::NkVec3f point{0.f, 0.f, 0.f}; // extrémité projetée sur le plan du sol
				float32 penetration = 0.f;		  // >0 sous le sol, <0 au-dessus (distance signée inversée)
		};

		struct NkContactDetector {
			public:
				// Contact d'UNE extrémité avec un sol plan (planePoint + planeNormal, normale unitaire).
				// En contact si la distance signée au plan <= threshold (marche même légèrement au-dessus,
				// tolérance de contact). `point` = extrémité projetée orthogonalement sur le plan.
				static NkGroundContact DetectPlane(const math::NkVec3f &footWorld, const math::NkVec3f &planePoint,
												   const math::NkVec3f &planeNormal, float32 threshold);

				// Détecte les contacts de N extrémités et remplit `outSupport` avec les points de
				// support (extrémités EN CONTACT, projetées au sol) — prêts pour NkBalance::EvaluateStatic.
				// `outContacts` (optionnel, peut être nullptr) reçoit le détail par extrémité.
				// Retourne le nombre d'extrémités en contact.
				static int32 DetectSupportPoints(const math::NkVec3f *footWorld, int32 footCount,
												 const math::NkVec3f &planePoint, const math::NkVec3f &planeNormal,
												 float32 threshold, NkVector<math::NkVec3f> &outSupport,
												 NkVector<NkGroundContact> *outContacts = nullptr);

				// Auto-test headless (aucun GPU), inclut un test d'intégration M3.1+M3.2+M3.3.
				static bool SelfTest();
		};

	} // namespace animphys
} // namespace nkentseu
