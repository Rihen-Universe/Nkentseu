// =============================================================================
// NkUnkenyActif.h — l'entite ACTIVE ou non (le GameObject.active de Unity)
//
// A QUOI SERT CE FICHIER
//   Une entite ETEINTE et toute sa descendance n'existent plus pour le jeu,
//   en edition comme en jeu, dans l'editeur comme dans le jeu construit :
//     - ni rendues   : sprites, formes, matiere, lumieres, ombres portees ;
//     - ni simulees  : son corps rigide SORT du solveur (sa description reste
//                      dans NkCorps2D, corpsId = 0), son corps mou est gele
//                      (NkCorpsP2D::actif, NKPhysics) ; ni vitesse manuelle ;
//     - ni scriptees : controleurs, animateurs, animations, emetteurs, sons.
//   Rallumee, elle repart d'ou elle est, immobile.
//
// ⚠️ CE N'EST PAS L'OEIL DE L'OUTLINER
//   L'oeil (NkDrapeauxEditeur::cache, UnkenyEditor) est un confort de
//   L'EDITEUR : il ne cache qu'en EDITION, et le jeu montre tout. L'activite,
//   elle, est une propriete DU JEU : sauvee dans la scene et dans les prefabs,
//   lue par le joueur autonome. Les deux coexistent, et l'un ne touche pas
//   l'autre.
//
// ⚠️ NKECS N'A PAS DE DRAPEAU « ACTIF » (mesure du 2026-09-30 : ni composant ni
//   drapeau d'entite, seulement NkSystem::SetEnabled). D'ou ce composant, porte
//   par l'entite, declare a la photo et au fichier par NkScene::Init.
//   ABSENT = ACTIVE : un fichier d'avant, qui ne le porte pas, se relit tel quel.
//
// LE CODE D'UN JEU
//   Ses systemes (NkScene::AjouterSysteme) parcourent le monde eux-memes : ils
//   testent NkScene::EstActive(id), ou NkEntiteActive(monde, id) ici.
// =============================================================================
#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYACTIF_H__
#define __NKENTSEU_UNKENY_NKUNKENYACTIF_H__

#include "NKECS/Hierarchy/NkHierarchy.h"
#include "NKECS/World/NkWorld.h"

namespace nkentseu {
	namespace unkeny {

		/// Le drapeau PROPRE de l'entite (celui de la case des Details). L'etat en
		/// EFFET tient aussi compte des ancetres : NkEntiteActive.
		struct NkActif2D {
				bool actif = true;
		};

		/// L'entite est-elle active EN EFFET : ni elle ni aucun de ses ancetres
		/// n'est eteint. Bornee (64 niveaux) : une boucle de parents, impossible
		/// par Rattacher, ne ferait pas tourner le jeu en rond.
		inline bool NkEntiteActive(const ecs::NkWorld &monde, ecs::NkEntityId id) noexcept {
			for (int32 niveau = 0; niveau < 64 && id.IsValid() && monde.IsAlive(id); ++niveau) {
				const NkActif2D *a = monde.Get<NkActif2D>(id);
				if (a != nullptr && !a->actif) {
					return false;
				}
				id = ecs::NkGetParent(monde, id);
			}
			return true;
		}

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYACTIF_H__
