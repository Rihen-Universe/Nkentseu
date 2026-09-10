// =============================================================================
// NkUnkenyRendu.h — dessiner une scene 2D
//
// A QUOI SERT CE FICHIER
//   Il parcourt les entites qui ont un transform et un sprite, et il les
//   dessine a travers la camera. C'est tout : il ne modifie aucun composant.
//
// ⚠️ L'ORDRE DE DESSIN EST UNE DONNEE, PAS UN HASARD
//   NKECS n'offre AUCUNE garantie sur l'ordre d'iteration des archetypes — et
//   c'est normal, ce n'est pas son travail. Un rendu qui dessine dans l'ordre
//   de la requete produit donc un empilement qui CHANGE quand on ajoute un
//   composant a une entite. Le defaut se presente comme « le personnage passe
//   parfois derriere le decor », un coup sur deux, sans rien qui l'explique.
//   On trie donc explicitement par `NkSprite2D::couche`. Le tri coute ; ne pas
//   trier coute plus cher, et plus tard.
//
// ⚠️ ET LE HORS-CHAMP N'EST PAS DESSINE
//   Une scene de mille entites dont trente sont visibles ne doit pas emettre
//   mille quads. Le test se fait sur la ZONE VISIBLE de la camera, en monde —
//   pas apres conversion en pixels, ce qui reviendrait a payer la conversion
//   pour rien.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un type de dessin (ligne, cercle, texte de scene) -> ici
//   - un effet propre a un jeu                          -> chez le jeu, apres
//                                                          l'appel a Dessiner
// =============================================================================
#pragma once

#include "NKGui/Core/NkGuiContext.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		struct NkStatsRendu {
				int32 entitesVues = 0;	  ///< entites avec sprite visible
				int32 entitesDessinees = 0; ///< celles qui ont passe le hors-champ
		};

		/// Dessine tous les sprites de la scene, tries par couche.
		/// Rend de quoi mesurer : sans compteur, « pourquoi c'est lent » n'a pas
		/// de reponse, et « le culling marche-t-il » non plus.
		NkStatsRendu NkDessinerScene(nkgui::NkGuiDrawList &dl, NkScene &scene);

		/// ⚠️ TOUTES LES COULEURS D'UNKENY SONT EN 0xRRGGBBAA — l'alpha en
		/// QUEUE. Ecrite en ARGB par habitude, `0x20FFFFFF` ne donne pas un
		/// blanc a 12 % : elle donne un CYAN OPAQUE, et `0x8000FF00` donne du
		/// transparent pur. Les deux se compilent, les deux « marchent », et
		/// seule l'image le dit — defaut trouve sur une capture le 2026-09-01.
		/// La convention est celle de NkSprite2D::couleur : elle ne change pas
		/// d'un fichier a l'autre.

		/// Contours des collisionneurs, en superposition. C'est l'outil qui rend
		/// un defaut de physique VISIBLE : un sprite decale de son collisionneur
		/// se cherche pendant des heures sans lui, et se voit en une seconde
		/// avec.
		void NkDessinerCollisionneurs(nkgui::NkGuiDrawList &dl, NkScene &scene, uint32 couleur = 0x00E07AC0u);

		/// Une grille de reperage en coordonnees de MONDE. Elle dit ou est
		/// l'origine et quelle taille fait une unite — les deux questions qu'on
		/// se pose devant une scene qui ne s'affiche pas ou l'on croyait.
		void NkDessinerGrille(nkgui::NkGuiDrawList &dl, const NkVue2D &camera, float32 pas = 1.f,
							  uint32 couleur = 0xFFFFFF1Eu, uint32 couleurAxes = 0xFFFFFF5Au);

	} // namespace unkeny
} // namespace nkentseu
