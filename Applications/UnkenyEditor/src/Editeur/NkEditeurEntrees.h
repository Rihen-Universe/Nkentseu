//
// NkEditeurEntrees.h
// =============================================================================
// Description :
//   « Jouer » dans l'editeur : QUI recoit le clavier, la manette et le doigt.
//   Tant que la vue est active et la scene EN JEU, ils vont aux ACTIONS du jeu
//   (unkeny::NkEntreesJeu) ; sinon, a l'editeur (NKGui), comme avant.
//
// Caracteristiques :
//   - Jouer donne la main au jeu (comme le « Play In Editor » d'UE5) ; un clic
//     dans le viseur la lui rend ; un clic ailleurs, Echap, Pause ou Arreter
//     la lui reprennent -- et RELACHENT tout, sinon une touche lachee pendant
//     que le jeu ne regardait pas resterait enfoncee.
//   - ⚠️ ECHAP REND LA MAIN, IL N'ARRETE PAS : le jeu continue, et c'est le
//     SECOND Echap, recu par l'editeur, qui arrete (son raccourci de toujours).
//   - Les RELACHEMENTS de touche passent aussi a NKGui : une touche enfoncee
//     avant que le jeu prenne la main (Espace, qui lance Jouer) ne reste pas
//     collee dans l'editeur.
//   - La souris reste a l'editeur : les outils du viseur (Saisir, Couteau)
//     servent justement en jeu.
//   - Toute la logique est une fonction de l'ETAT, sans fenetre : elle
//     s'eprouve dans `--selftest` (NkEditeurLancerBancEntrees).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURENTREES_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURENTREES_H__

#include "Editeur/NkEditeurModele.h"

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkGamepadSystem.h"
#include "NKGui/Core/NkGuiContext.h"
#include "Unkeny/Entree/NkUnkenyEntreesJeu.h"

namespace nkentseu {
	namespace editeur {

		/// Les actions du jeu joue dans l'editeur. Le jour ou la scene porte ses
		/// propres actions (contoleur de plateforme, U2), c'est elle qui les
		/// nommera ; d'ici la, l'editeur joue avec celles-ci.
		enum NkActionJeuEditeur : int32 {
			NK_JEU_AVANCER = 0, ///< -1 gauche .. +1 droite
			NK_JEU_MONTER,		///< -1 bas .. +1 haut
			NK_JEU_SAUTER,
			NK_JEU_ACTION
		};

		struct NkEditeurEntrees {
				unkeny::NkEntreesJeu jeu;
				/// Le jeu a-t-il la main (clavier, manette, doigt) ?
				bool jeuALaMain = false;
				/// L'etat vu a la trame precedente : c'est la TRANSITION vers le
				/// jeu qui donne la main, pas l'etat lui-meme.
				NkEtatJeu etatVu = NkEtatJeu::NK_EDITION;

				// --- Le panneau Entrees (30/09) ------------------------------------
				nkgui::NkRect panneauRect{0.f, 0.f, 0.f, 0.f}; ///< celui de la trame d'avant
				int32 panneauDefil = 0;	 ///< premiere ligne affichee
				int32 capturee = -1;	 ///< la liaison en capture, -1 sinon
				bool modifiees = false;	 ///< des liaisons ont change depuis le dernier fichier
				NkString cheminVu;		 ///< la scene dont le fichier d'entrees a ete lu
				NkString message;		 ///< la derniere annonce du panneau
		};

		/// ZQSD / WASD et fleches, stick gauche et croix, Espace / Sud, E / Ouest.
		void NkEditeurEntreesParDefaut(NkEditeurEntrees &e);

		/// Un evenement. Rend true si le JEU le prend : l'editeur ne doit alors
		/// pas le donner a NKGui.
		/// @param viseur le rectangle du viseur, en pixels client
		/// @param editeurOccupe une boite modale ou un menu est ouvert : l'editeur
		///        a besoin du clavier (Entree, Echap), le jeu rend la main
		bool NkEditeurEntreesEvenement(NkEditeurEntrees &e, NkEtatJeu etat, const NkEvent &evenement,
									   const nkgui::NkRect &viseur, bool editeurOccupe = false);

		/// Une trame : suit l'etat du jeu, puis calcule les actions (a appeler
		/// AVANT le pas de la scene).
		void NkEditeurEntreesTrame(NkEditeurEntrees &e, NkEtatJeu etat, const NkGamepadSystem *manettes,
								   const nkgui::NkRect &viseur, bool editeurOccupe = false);

		/// Le bandeau du viseur quand le jeu a la main, avec les actions non
		/// nulles (la « surimpression des actions en jeu » de la feuille de
		/// route, U2). Ne dessine rien sinon.
		void NkEditeurDessinerEntrees(nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police, const NkEditeurEntrees &e,
									  const nkgui::NkRect &viseur);

		// --- Le panneau Entrees : ses gestes, sans fenetre ----------------------
		/// « Changer » : la prochaine touche ou le prochain bouton de manette
		/// devient l'entree de la liaison `indice` -- meme en EDITION.
		bool NkEditeurEntreesChanger(NkEditeurEntrees &e, int32 indice);
		void NkEditeurEntreesAnnulerChangement(NkEditeurEntrees &e);
		/// La liaison, en clair : « Sauter  <-  Key:SPACE », « Avancer  <-  zone ».
		NkString NkEditeurEntreesDecrire(const NkEditeurEntrees &e, int32 indice);
		/// Le fichier des entrees d'une scene : a cote d'elle, meme nom,
		/// extension .nkentrees (le texte de NkLiaisons::Ecrire).
		NkString NkEditeurEntreesFichier(const char *cheminScene);
		/// Le chemin de la scene, SANS l'ecrire dans le modele (NkEditeurChemin,
		/// lui, l'y ecrit, et le titre de l'onglet le lit : l'appeler a chaque
		/// trame changeait le titre d'une scene jamais enregistree).
		NkString NkEditeurEntreesCheminScene(const NkEditeurModele &m);
		bool NkEditeurEntreesEnregistrer(NkEditeurEntrees &e, const char *cheminScene);
		/// Relit le fichier de la scene ; sans fichier, les liaisons par defaut.
		/// Rend vrai si un fichier a ete lu sans erreur.
		bool NkEditeurEntreesCharger(NkEditeurEntrees &e, const char *cheminScene);
		/// A chaque trame : si la scene a change de chemin (Ouvrir), ses entrees
		/// sont relues. C'est ce qui « sauve dans la scene » sans toucher au
		/// format .nkscene.
		void NkEditeurEntreesSuivreScene(NkEditeurEntrees &e, const char *cheminScene);

		struct NkEditeurCadre;
		/// Le panneau « Entrees du jeu » (Fenetre > Entrees du jeu) : les actions,
		/// leurs liaisons, « Changer » par capture, Enregistrer / Recharger / Par
		/// defaut. Ne dessine rien si `c.ui.panneauEntrees` est faux.
		void NkEditeurDessinerPanneauEntrees(NkEditeurCadre &c, NkEditeurEntrees &e);

		/// (e25..e35), lance par `--selftest` apres le banc de l'editeur.
		int32 NkEditeurLancerBancEntrees();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURENTREES_H__
