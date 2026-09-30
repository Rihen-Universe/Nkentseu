//
// NkUnkenyPartie.h
// =============================================================================
// Description :
//   Jouer une scene : la TRAME et l'IMAGE du jeu. Le meme code sert a
//   « Jouer » dans UnkenyEditor et au joueur autonome (UnkenyPlayer) qui lance
//   le jeu construit : jouer dans l'editeur, c'est le jeu (regle 3 du document
//   04 de Nogee, reprise par la feuille de route d'UnkenyEditor, palier U5).
//
// Caracteristiques :
//   - NkAvancerPartie : une trame de simulation, bornee (un retour de veille
//     ou une fenetre deplacee ne fait pas traverser les murs).
//   - NkDessinerPartie : formes, sprites, matiere, effets en alpha, carte de
//     lumiere, effets additifs, dans cet ordre, et RIEN d'autre -- ni grille,
//     ni collisionneurs, ni selection : ce que l'editeur ajoute par-dessus est
//     a lui, ce qui est ici est ce que le joueur voit. (Effets et lumiere :
//     2026-09-30 ; eteints et absents, ils n'ecrivent rien dans la liste.)
//
// ⚠️ POURQUOI CE FICHIER EXISTE (2026-09-30)
//   Ces lignes vivaient dans l'editeur (NkEditeurAvancer, NkDessinerViseur).
//   Le joueur autonome aurait du les recopier ; une copie diverge au premier
//   correctif, et « le jeu construit ne se comporte pas comme dans l'editeur »
//   est le defaut le plus cher a trouver : il n'apparait que chez celui qui
//   n'a pas l'editeur. Elles descendent donc d'un etage, et l'editeur les
//   APPELLE.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYPARTIE_H__
#define __NKENTSEU_UNKENY_NKUNKENYPARTIE_H__

#include "NKGui/Core/NkGuiDrawList.h"
#include "Unkeny/Rendu/NkUnkenyEclairage.h"
#include "Unkeny/Rendu/NkUnkenyRendu.h"
#include "Unkeny/Rendu/NkUnkenyRenduEffets.h"
#include "Unkeny/Rendu/NkUnkenyRenduParticules.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		/// La plus longue trame qu'une partie accepte d'un coup, en secondes.
		/// Au-dela, le temps est JETE (le plafond de pas de NkSceneConfig fait
		/// le reste) : 50 ms, c'est vingt images par seconde, le seuil sous
		/// lequel un jeu d'action n'est de toute facon plus jouable.
		constexpr float32 NK_PARTIE_TRAME_MAX = 0.05f;

		/// Une trame de JEU : la scene avance de `dt` secondes, borne a
		/// NK_PARTIE_TRAME_MAX. Un `dt` nul ou negatif ne fait rien.
		void NkAvancerPartie(NkScene &scene, float32 dt);

		/// L'image du JEU, a travers la camera de la scene : les formes (decor,
		/// rigides sans texture), puis les sprites, puis la matiere par-dessus
		/// -- elle coule sur tout le reste. Un decor prend la couleur de son
		/// sprite, meme cache : c'est la que l'editeur range la teinte du sol.
		/// Puis les particules d'effet en alpha (la nuit les assombrit), la carte
		/// de lumiere si l'eclairage de la scene est actif, et les particules
		/// additives (elles emettent). Rend les compteurs du rendu des sprites ;
		/// `eclairage` et `particules`, facultatifs, recoivent ceux de la lumiere
		/// et le nombre de particules d'effet dessinees.
		NkStatsRendu NkDessinerPartie(nkgui::NkGuiDrawList &dl, NkScene &scene, const NkOptionsRenduParticules &rendu,
									  NkStatsEclairage2D *eclairage = nullptr, int32 *particules = nullptr);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYPARTIE_H__
