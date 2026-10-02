//
// NkUnkenySauvegardeInterne.h
// =============================================================================
// Description :
//   L'ecriture et la relecture d'UNE entite, au format des "entites" d'un
//   .nkscene, pour les autres fichiers d'Unkeny qui en ecrivent (les prefabs,
//   .nkprefab). INTERNE au moteur : un jeu passe par NkUnkenySauvegarde.h.
//
// Caracteristiques :
//   - Une seule facon d'ecrire une entite : un prefab et une scene ne peuvent
//     pas diverger sur le sens d'un champ, puisqu'ils partagent ce code.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYSAUVEGARDEINTERNE_H__
#define __NKENTSEU_UNKENY_NKUNKENYSAUVEGARDEINTERNE_H__

#include "NKContainers/String/NkString.h"
#include "NKSerialization/NkArchive.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"

namespace nkentseu {
	namespace unkeny {
		namespace interne {

			/// Une entite de photo -> objet (format v2). Les composants "jeu" sont
			/// lus dans la disposition des copieurs de `scene`.
			NkArchive EcrireEntite(const NkScene::NkPhotoEntite &e, NkScene &scene, const NkRessourcesScene &r);

			/// L'inverse, TOUT compris : composants "jeu" poses dans la disposition
			/// de `scene` (qui doit les avoir declares), texture par son nom.
			bool LireEntite(const NkArchive &a, NkScene &scene, const NkRessourcesScene &r, NkScene::NkPhotoEntite &e,
							NkString &erreur);

			/// (2026-10-02, R31) UN composant DECRIT, champ par champ, comme sous
			/// "jeu" d'une scene (les references par NOM) : l'asset .nkmesh2d
			/// (Maillage/NkUnkenyMaillageFichier.h) s'ecrit ainsi, et ne peut donc
			/// pas diverger d'un maillage sauve dans une scene.
			void EcrireComposant(NkArchive &o, const NkChampSauve *champs, uint32 n, const uint8 *octets,
								 const NkRessourcesScene &r);
			/// L'inverse, dans `octets` qui porte DEJA la valeur par defaut.
			void LireComposant(const NkArchive &o, const NkChampSauve *champs, uint32 n, uint8 *octets, NkScene &scene,
							   const NkRessourcesScene &r);

		} // namespace interne
	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYSAUVEGARDEINTERNE_H__
