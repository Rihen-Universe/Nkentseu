//
// NkUnkenyControles.h
// =============================================================================
// Description :
//   Les CONTROLEURS DE PERSONNAGE d'une scene 2D, en donnees pures : un
//   controleur RIGIDE (un corps NkCorps2D) et un controleur MOU (un corps de
//   particules NkCorpsMou2D). Ils disent QUELLES actions lire et COMMENT
//   bouger ; le systeme qui les fait agir vit dans Jeu/NkUnkenyControleurs.h.
//
// Caracteristiques :
//   - Meme regle que NkUnkenyComposants.h : aucun pointeur, aucun type
//     d'interface ; copiables bit a bit (photographies telles quelles).
//   - Ils lisent des ACTIONS (Entree/NkUnkenyActions.h) par leur INDEX, jamais
//     une touche : un banc joue une partie entiere sans fenetre.
//   - Deux moities : les REGLAGES (ecrits dans .nkscene) et l'ETAT (ecrit par
//     le systeme a chaque pas fixe ; garde par la photo de Jouer / Arreter,
//     pas par le fichier).
//
// Algorithmes implementes :
//   - Marche a acceleration bornee vers une vitesse visee (vitesse max tenue)
//   - Saut au sol seulement, avec tolerance de bord (« coyote time ») et
//     tampon de saut (une demande juste avant d'atterrir est servie a
//     l'atterrissage)
//   - Controle en l'air reglable (fraction de l'acceleration au sol)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYCONTROLES_H__
#define __NKENTSEU_UNKENY_NKUNKENYCONTROLES_H__

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace unkeny {

		using math::NkVec2f;

		/// Les reglages communs aux deux controleurs.
		/// ⚠️ LES INDEX D'ACTION SONT CEUX DU JEU : le moteur ne connait aucune
		/// action par son nom. 0 et 1 par defaut, comme un jeu qui commence par
		/// « axe horizontal » puis « sauter ».
		struct NkReglagesControle2D {
				int32 actionX = 0;			 ///< action d'AXE horizontal (-1..1)
				int32 actionSauter = 1;		 ///< action de saut (bouton)
				float32 vitesseMax = 5.f;	 ///< m/s visee par l'axe a fond
				float32 acceleration = 40.f; ///< m/s^2 vers la vitesse visee, au sol
				float32 freinage = 40.f;	 ///< m/s^2 quand l'axe est relache, au sol
				float32 controleAir = 0.5f;	 ///< [0,1] part de l'acceleration en l'air (0 = aucune prise)
				float32 vitesseSaut = 6.f;	 ///< m/s vers le haut au depart du saut
				float32 coyote = 0.1f;		 ///< s : on peut encore sauter apres avoir QUITTE un bord (pas apres un saut)
				float32 tamponSaut = 0.1f;	 ///< s : une demande de saut attend au plus autant le sol
				float32 penteMax = 0.7f;	 ///< normale y minimale d'un sol (0,7 ~ 45 degres)
				bool actif = true;
		};

		/// Ce que le systeme ECRIT a chaque pas fixe. Un jeu peut le LIRE (animer
		/// « au sol », jouer un son de saut) ; il ne devrait pas l'ecrire.
		struct NkEtatControle2D {
				bool auSol = false;
				NkVec2f normaleSol{0.f, 0.f};
				float32 tempsHorsSol = 1000.f; ///< s depuis le dernier pas au sol
				float32 tempsDemande = 1000.f; ///< s depuis la demande de saut en attente
				bool demandeEnAttente = false;
				bool sautEnCours = false; ///< entre un saut et le prochain sol : le coyote ne vaut plus
				uint32 sauts = 0;		  ///< sauts accordes
				uint32 sautsRefuses = 0;  ///< demandes perimees sans sol (en l'air, hors coyote et tampon)
				/// Le bouton de saut au pas fixe PRECEDENT. ⚠️ Le controleur detecte
				/// lui-meme l'appui : NkAction::VientDEtrePressee vaut pour toute la
				/// TRAME, donc pour ses deux pas fixes a 30 i/s -- un saut servi au
				/// premier aurait ete redemande au second.
				bool boutonAvant = false;
		};

		/// Controleur d'un corps RIGIDE (l'entite porte NkCorps2D). Le sol se lit
		/// dans les contacts du monde (NkPhysicsWorld::BodyContacts) : toute forme
		/// convient, sans lancer de rayon. Une plateforme qui bouge emporte le
		/// personnage (sa vitesse s'ajoute a la vitesse visee).
		struct NkControleRigide2D {
				NkReglagesControle2D reglages;
				NkEtatControle2D etat;
		};

		/// Controleur d'un corps MOU (l'entite porte NkCorpsMou2D). Il pousse EN
		/// AJOUTANT (NkParticules2D::AjouterVitesse) : la gravite et la forme
		/// vivent ; le sol se lit dans les contacts tenus sur le pas
		/// (NkParticules2D::ContactCorps).
		/// Une PARTIE nommee (NkParticules2D::CreerPartie) peut etre seule
		/// poussee, et une autre servir de pieds : « tete » pilotee, « pieds » au
		/// sol. Nom vide = le corps entier.
		struct NkControleMou2D {
				NkReglagesControle2D reglages;
				char partiePoussee[24] = {}; ///< partie a pousser ; vide = tout le corps
				char partieSol[24] = {};	 ///< partie qui dit « au sol » ; vide = tout le corps
				/// [0,1] part de la ROTATION d'ensemble retiree a chaque pas fixe
				/// (NkParticules2D::FreinerRotation). 0 = le corps roule librement.
				/// ⚠️ Pousse sur un sol qui frotte, un corps mou ROULE comme une roue, et
				/// une gelee carree rebondit sur ses coins : mesure du banc Gelee, elle
				/// franchissait des trous SANS sauter. Un personnage veut ~0,5.
				float32 redressement = 0.f;
				NkEtatControle2D etat;
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYCONTROLES_H__
