//
// NkJoueurEntrees.h
// =============================================================================
// Description :
//   Les entrees du joueur autonome : le CLAVIER vers les actions du jeu
//   (unkeny::NkActions), et les touches de la coquille (quitter, pause).
//
// Caracteristiques :
//   - Deux fonctions et une structure : c'est le POINT DE BRANCHEMENT. Le
//     joueur n'appelle rien d'autre, et rien d'autre ne lit une touche.
//   - Les correspondances sont celles que la branche `comble/entrees-jeu`
//     donne a l'editeur (NkEditeurEntreesParDefaut) : D / A et fleches =
//     Avancer, W / S et fleches = Monter, Espace = Sauter, E = Action. Le jeu
//     joue au clavier la meme chose dans l'editeur et une fois construit.
//
// ⚠️ CE FICHIER EST UN RELAIS, PAS UNE SOURCE D'ENTREE DE PLUS
//   La manette, le tactile et les correspondances EN DONNEES arrivent par la
//   branche `comble/entrees-jeu` (unkeny::NkEntreesJeu, NkUnkenyLiaisons).
//   A sa fusion, ce fichier ne garde que sa FACADE : NkJoueurEntreesEvenement
//   appelle `jeu.Lire(e)`, NkJoueurEntreesTrame appelle
//   `jeu.Trame(&NkWESystem::Gamepads())`, et la table de touches ci-dessous
//   disparait. Rien d'autre du joueur n'a a changer.
//
//   ⚠️ Les INDICES d'action ne sont pas encore une convention du depot :
//   l'editeur de `comble/entrees-jeu` met Sauter en 2, les controleurs de
//   `comble/physique-2d-jeu` le lisent en 1 (NkReglagesControle2D). Ils sont
//   ici ceux de l'editeur, nommes, pour qu'un seul endroit change le jour ou
//   le depot tranche.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYPLAYER_NKJOUEURENTREES_H__
#define __NKENTSEU_UNKENYPLAYER_NKJOUEURENTREES_H__

#include "NKEvent/NkEvent.h"
#include "Unkeny/Entree/NkUnkenyActions.h"

namespace nkentseu {
	namespace joueur {

		/// Les actions du jeu, par indice (ceux de NkEditeurEntrees, branche
		/// comble/entrees-jeu).
		enum NkActionJoueur : int32 {
			NK_JOUEUR_AVANCER = 0, ///< axe : gauche -1, droite +1
			NK_JOUEUR_MONTER = 1,  ///< axe : bas -1, haut +1
			NK_JOUEUR_SAUTER = 2,
			NK_JOUEUR_ACTION = 3
		};

		/// Ce que la COQUILLE doit faire d'une touche (le jeu n'en sait rien).
		enum class NkDemandeJoueur : uint8 { NK_RIEN = 0, NK_QUITTER, NK_BASCULER_PAUSE };

		struct NkJoueurEntrees {
				unkeny::NkActions actions;
				/// Les touches tenues, par NkKey : un appui arrive UNE fois, l'action
				/// doit rester enfoncee tant que la touche l'est.
				bool tenues[512] = {};
		};

		/// Un evenement de la fenetre. Rend ce que la coquille doit faire ;
		/// les touches du jeu, elles, vont aux actions (a la trame suivante).
		NkDemandeJoueur NkJoueurEntreesEvenement(NkJoueurEntrees &e, const NkEvent &evenement);

		/// Une fois par trame, AVANT le pas de la scene : les actions prennent
		/// l'etat des touches tenues.
		void NkJoueurEntreesTrame(NkJoueurEntrees &e);

		/// La fenetre a perdu le focus : plus aucune touche n'est tenue (sinon
		/// le heros continue d'avancer tout seul apres un Alt+Tab).
		void NkJoueurEntreesRelacher(NkJoueurEntrees &e);

	} // namespace joueur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYPLAYER_NKJOUEURENTREES_H__
