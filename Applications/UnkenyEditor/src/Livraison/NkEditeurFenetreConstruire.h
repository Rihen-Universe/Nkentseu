//
// NkEditeurFenetreConstruire.h
// =============================================================================
// Description :
//   La fenetre « Construire » de l'editeur (Fichier > Construire…) : choisir
//   la plateforme, le profil, le nom, l'icone sombre et claire, le dossier de
//   sortie ; lancer ; suivre Jenga dans l'onglet Journal ; lire le resultat.
//
// Caracteristiques :
//   - DESSINEE PAR L'EDITEUR, dans `dlOverlay`, comme la boite « non
//     enregistree » : pas de boite du systeme, et modale de la meme facon
//     (tant qu'elle est ouverte, ni le corps ni les menus ne recoivent la
//     souris, ni les raccourcis le clavier).
//   - Une plateforme sans chaine reste AFFICHEE, avec la raison en clair : on
//     ne grise pas en silence (demande de la mission U5, et risque « Jenga
//     hors du depot » de la feuille de route, § 7.2).
//   - Elle ne construit rien elle-meme : elle appelle NkPreparerConstruction,
//     lance les etapes par NkEditeurProcessus, puis NkAcheverConstruction --
//     les fonctions qu'appelle aussi `--construire=`.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURFENETRECONSTRUIRE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURFENETRECONSTRUIRE_H__

#include "Editeur/NkEditeurInterface.h"
#include "Livraison/NkEditeurConstruire.h"
#include "Livraison/NkEditeurProcessus.h"

namespace nkentseu {
	namespace editeur {

		enum class NkEtatConstruction : uint8 {
			NK_REPOS = 0,  ///< la fenetre attend « Construire »
			NK_EN_COURS,   ///< une etape tourne
			NK_VERIFIE,	   ///< le jeu produit relit ses donnees (--verifier)
			NK_REUSSIE,
			NK_ECHOUEE
		};

		/// L'etat de la fenetre et de la construction en cours. Sur le TAS
		/// (possede par NkEditeurApp) : elle porte un processus et son fil.
		struct NkEditeurConstruction {
				bool ouverte = false;
				NkDemandeConstruction demande;
				char nom[64] = {};
				char sortie[320] = {};
				char iconeSombre[320] = {};
				char iconeClaire[320] = {};
				int32 focus = -1; ///< 0 nom, 1 sortie, 2 icone sombre, 3 icone claire
				/// `--fenetre=construire-auto` : « Construire » est presse tout seul
				/// apres ce nombre de trames (le viseur a alors cadre la scene).
				/// C'est ce qui eprouve la fenetre SANS souris. 0 = jamais.
				int32 lancementAuto = 0;
				NkDisponibilite dispo[NK_NB_PLATEFORMES];

				NkEtatConstruction etat = NkEtatConstruction::NK_REPOS;
				NkPlanConstruction plan;
				usize etape = 0;
				NkEditeurProcessus processus;
				NkString annonce; ///< la ligne d'etat de la fenetre
		};

		/// Ouvre la fenetre, avec les reglages precedents ou ceux par defaut, et
		/// refait la detection des chaines (elles ont pu etre installees).
		void NkEditeurOuvrirConstruire(NkEditeurConstruction &k, const NkEditeurModele &m);

		/// Lance la construction avec les reglages de la fenetre : cuisson et
		/// workspace tout de suite, puis la premiere etape Jenga. Le viseur de
		/// l'interface donne la vue de reference. false = rien de lance (le
		/// Journal dit pourquoi).
		bool NkEditeurLancerConstruction(NkEditeurConstruction &k, NkEditeurModele &m, NkEditeurInterface &ui);

		/// Une trame : les lignes de Jenga vont au Journal ; une etape finie
		/// lance la suivante, puis le rangement et la verification.
		void NkEditeurAvancerConstruction(NkEditeurConstruction &k, NkEditeurModele &m, NkEditeurInterface &ui);

		/// La fenetre, dans dlOverlay. Rien si elle est fermee.
		void NkEditeurDessinerConstruire(NkEditeurCadre &c, NkEditeurConstruction &k);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURFENETRECONSTRUIRE_H__
