//
// NkJoueurEntrees.h
// =============================================================================
// Description :
//   Les entrees du joueur autonome : clavier, manette et doigt vers les
//   actions du jeu (unkeny::NkEntreesJeu, sur la carte du noyau NkInputMap),
//   et les touches de la coquille (quitter).
//
// Caracteristiques :
//   - Quelques fonctions et une structure : c'est le POINT DE BRANCHEMENT. Le
//     joueur n'appelle rien d'autre, et rien d'autre ne lit une touche.
//   - Les actions sont les actions STANDARD d'Unkeny
//     (Unkeny/Entree/NkUnkenyActionsStandard.h) : les MEMES indices que les
//     controleurs de personnage et que l'editeur. Les liaisons par defaut sont
//     les liaisons standard ; un jeu construit emporte les siennes
//     (entrees.nkentrees, relu par NkJoueurEntreesLire).
//   - Pause est une ACTION (P, Start) : la manette met aussi en pause.
//     Echap reste a la coquille (quitter), il ne va pas au jeu.
//
// ⚠️ 30/09 : CE FICHIER N'A PLUS DE TABLE DE TOUCHES
//   Il recopiait les numeros de l'editeur (Sauter en 2) quand les controleurs
//   lisaient Sauter en 1 : Espace ne faisait pas sauter le heros de « Gelee ».
//   Les numeros vivent desormais dans UNE table, dans Unkeny.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYPLAYER_NKJOUEURENTREES_H__
#define __NKENTSEU_UNKENYPLAYER_NKJOUEURENTREES_H__

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkGamepadSystem.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Entree/NkUnkenyEntreesJeu.h"

namespace nkentseu {
	namespace unkeny {
		class NkScene;
	}

	namespace joueur {

		/// Les actions du jeu : celles d'Unkeny, par leur nom standard.
		enum NkActionJoueur : int32 {
			NK_JOUEUR_AVANCER = unkeny::NK_ACTION_AVANCER, ///< axe : gauche -1, droite +1
			NK_JOUEUR_SAUTER = unkeny::NK_ACTION_SAUTER,
			NK_JOUEUR_MONTER = unkeny::NK_ACTION_MONTER, ///< axe : bas -1, haut +1
			NK_JOUEUR_ACTION = unkeny::NK_ACTION_ACTION,
			NK_JOUEUR_PAUSE = unkeny::NK_ACTION_PAUSE
		};

		/// Ce que la COQUILLE doit faire (le jeu n'en sait rien).
		enum class NkDemandeJoueur : uint8 { NK_RIEN = 0, NK_QUITTER, NK_BASCULER_PAUSE };

		struct NkJoueurEntrees {
				unkeny::NkEntreesJeu jeu;
				/// L'action qui met en pause, cherchee par son NOM dans les
				/// liaisons lues (NK_JOUEUR_PAUSE par defaut).
				int32 pause = NK_JOUEUR_PAUSE;

				NkJoueurEntrees() noexcept;

				/// Les actions du joueur 1, celles que lisent les controleurs.
				const unkeny::NkActions &Actions() const noexcept {
					return jeu.Actions(0);
				}
		};

		/// Remplace les liaisons par celles d'un texte (.nkentrees, cuit avec le
		/// jeu). Les actions standard sont nommees d'abord : le texte ne parle
		/// que de noms. Un texte vide garde les liaisons standard. Une ligne
		/// refusee est dans le rapport, avec son numero ; les autres s'appliquent.
		unkeny::NkRapportLiaisons NkJoueurEntreesLire(NkJoueurEntrees &e, const NkString &texte);

		/// Les controleurs de personnage de la scene (NkControleRigide2D,
		/// NkControleMou2D) lisent les actions du joueur 1. A appeler une fois,
		/// apres le chargement de la scene. Rend l'id du systeme (0 = refus).
		uint32 NkJoueurEntreesBrancher(NkJoueurEntrees &e, unkeny::NkScene &scene);

		/// La surface de jeu, en pixels (zones d'ecran, joystick virtuel).
		void NkJoueurEntreesSurface(NkJoueurEntrees &e, float32 largeur, float32 hauteur);

		/// Un evenement de la fenetre. Rend ce que la coquille doit faire ;
		/// le reste va au jeu (lu a la trame suivante).
		NkDemandeJoueur NkJoueurEntreesEvenement(NkJoueurEntrees &e, const NkEvent &evenement);

		/// Une fois par trame, AVANT le pas de la scene : les actions prennent
		/// l'etat des entrees. Rend NK_BASCULER_PAUSE a l'APPUI de l'action Pause.
		/// @param manettes nul : pas de manette (un banc)
		NkDemandeJoueur NkJoueurEntreesTrame(NkJoueurEntrees &e, const NkGamepadSystem *manettes = nullptr,
											 float32 dt = 1.f / 60.f);

		/// La fenetre a perdu le focus : plus rien n'est tenu (sinon le heros
		/// continue d'avancer tout seul apres un Alt+Tab).
		void NkJoueurEntreesRelacher(NkJoueurEntrees &e);

		/// Le banc des entrees du joueur (j1..j3), lance par `--selftest` apres
		/// celui de la livraison.
		int32 NkJoueurLancerBancEntrees();

	} // namespace joueur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYPLAYER_NKJOUEURENTREES_H__
