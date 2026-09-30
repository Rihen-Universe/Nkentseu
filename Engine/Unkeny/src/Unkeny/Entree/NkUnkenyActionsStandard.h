//
// NkUnkenyActionsStandard.h
// =============================================================================
// Description :
//   Les ACTIONS STANDARD d'un jeu Unkeny, NOMMEES, et le SEUL endroit qui fixe
//   leurs indices : Avancer (axe X), Sauter, Monter (axe Y), Action, Pause.
//   Les controleurs de personnage (Scene/NkUnkenyControles.h), l'editeur
//   (NkEditeurEntrees) et le joueur autonome (NkJoueurEntrees) les prennent
//   ICI ; aucun ne recopie un numero.
//
// Caracteristiques :
//   - Les indices sont ceux que les controleurs lisaient deja et que les
//     .nkscene ecrits portent (actionX = 0, actionSauter = 1) : aucun fichier
//     existant ne change de sens. Monter, Action et Pause viennent apres.
//   - On les cherche aussi par NOM (NkActionStandard, NkIndiceAction) : un
//     fichier d'entrees (.nkentrees) ne parle que de noms, et un jeu peut
//     ajouter les siennes a partir de NK_ACTIONS_STANDARD.
//   - NkLiaisonsStandard : les liaisons par defaut (clavier, manette, doigt),
//     les MEMES dans l'editeur et dans le jeu construit.
//
// ⚠️ POURQUOI CE FICHIER EXISTE (2026-09-30)
//   L'editeur mettait Sauter en 2, les controleurs le lisaient en 1, et le
//   joueur autonome avait recopie l'editeur : Espace ne faisait pas sauter le
//   heros du jalon « Gelee » en Jouer. Trois recopies d'un numero, trois
//   facons de diverger ; il n'en reste qu'une, ici.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYACTIONSSTANDARD_H__
#define __NKENTSEU_UNKENY_NKUNKENYACTIONSSTANDARD_H__

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {

		class NkLiaisons;

		/// Les actions standard, par indice dans unkeny::NkActions.
		enum NkActionStandard : int32 {
			NK_ACTION_AVANCER = 0, ///< axe X : -1 gauche .. +1 droite (NkReglagesControle2D::actionX)
			NK_ACTION_SAUTER = 1,  ///< bouton (NkReglagesControle2D::actionSauter)
			NK_ACTION_MONTER = 2,  ///< axe Y : -1 bas .. +1 haut (echelles, nage, menus)
			NK_ACTION_ACTION = 3,  ///< bouton : interagir, ramasser, tirer
			NK_ACTION_PAUSE = 4,   ///< bouton : le jeu se met en pause
			NK_ACTIONS_STANDARD = 5 ///< leur nombre ; les actions propres a un jeu commencent ici
		};

		/// « Avancer », « Sauter », « Monter », « Action », « Pause » ; nullptr
		/// hors des actions standard.
		const char *NkNomActionStandard(int32 action) noexcept;

		/// L'indice d'une action standard par son nom (casse exacte), -1 sinon.
		int32 NkActionStandardParNom(const char *nom) noexcept;

		/// Vrai pour un axe (Avancer, Monter), faux pour un bouton.
		bool NkActionStandardEstUnAxe(int32 action) noexcept;

		/// Donne aux liaisons les noms des actions standard (a leurs indices).
		/// Les noms d'autres actions ne sont pas touches.
		void NkNommerActionsStandard(NkLiaisons &liaisons) noexcept;

		/// L'indice d'une action par son NOM : celui que les liaisons lui donnent,
		/// sinon celui des actions standard, sinon -1.
		int32 NkIndiceAction(const NkLiaisons &liaisons, const char *nom) noexcept;

		/// Les liaisons par defaut : vide la table, nomme les actions standard, et
		///   Avancer <- D / A (ZQSD en AZERTY), fleches, stick gauche X, croix ;
		///   Monter  <- W / S, fleches, stick gauche Y ;
		///   Sauter  <- Espace, bouton Sud, zone en bas a droite de l'ecran ;
		///   Action  <- E, bouton Ouest ;
		///   Pause   <- P, Start ;
		///   joystick virtuel (Avancer, Monter) a gauche de l'ecran.
		/// Les boutons et axes de manette valent pour TOUS les joueurs.
		void NkLiaisonsStandard(NkLiaisons &liaisons) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYACTIONSSTANDARD_H__
