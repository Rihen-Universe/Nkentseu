// =============================================================================
// NKAnimPhysics/NkPoseMass.h
// -----------------------------------------------------------------------------
// M3.1 (NkAnima — physique d'animation façon Cascadeur) : DISTRIBUTION DE MASSE
// + CENTRE DE MASSE (COM) d'une pose de squelette.
//
// Modèle : une masse (relative) par JOINT. Le centre de masse monde d'une pose =
//     COM = Σ_j  masse[j] · positionMonde[j]   /   Σ_j masse[j]
// où positionMonde[j] = colonne translation de la matrice monde du joint.
//
// C'est la 1re brique (ordre M3 strict : masse → équilibre → contacts → optim →
// auto-pose). Pure Foundation (NKMath + NKContainers) : AUCUN GPU, testable en
// headless. Le rendu du COM (sphère/croix debug) est laissé au consommateur
// (démo/éditeur) via NkRender3D::DrawDebug* — le module reste pur et réutilisable
// jeu (Noge) + app (NkAnima).
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace animphys {

		// Modèle de masse d'un squelette : une masse relative par joint.
		struct NkPoseMass {
			public:
				NkVector<float32> jointMass; // masse relative par joint (défaut uniforme = 1)

				// Masse uniforme (1.0) pour `jointCount` joints. Base neutre : COM = barycentre.
				void SetUniform(int32 jointCount);

				// Fractions anthropométriques (Dempster) déduites du NOM de chaque joint
				// (mots-clés : head/neck/spine/chest/torso/hip/pelvis, arm/shoulder/elbow/
				// hand/wrist, leg/thigh/knee/shin/foot/ankle...). Fallback = petite masse
				// résiduelle pour les joints non reconnus. Approximation : la masse d'un
				// segment est portée par SON joint (proximal) — suffisant pour le COM global.
				void SetAnthropometric(const NkVector<NkString> &jointNames);

				// Somme des masses.
				float32 TotalMass() const;

				// Centre de masse MONDE de la pose. `jointWorld` = matrices monde par joint
				// (colonne translation = position). Renvoie {0,0,0} si masse totale nulle ou
				// count incohérent avec jointMass.
				math::NkVec3f ComputeCOM(const math::NkMat4f *jointWorld, int32 count) const;

				// Variante pratique quand les positions monde sont déjà extraites.
				math::NkVec3f ComputeCOMFromPositions(const math::NkVec3f *worldPos, int32 count) const;

				// Auto-test headless (aucun GPU) : valide la formule du COM sur des cas connus.
				// Renvoie true si tout passe. Utilisable par une app console de test.
				static bool SelfTest();
		};

	} // namespace animphys
} // namespace nkentseu
