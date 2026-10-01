//
// NkEditeurFenetreConstruire.h
// =============================================================================
// Description :
//   La fenetre « Construire » de l'editeur (Fichier > Construire…) : choisir
//   la plateforme, le profil, le nom, l'icone sombre et claire, le dossier de
//   sortie, le moteur (precompile ou sources) ; lancer ; suivre la construction
//   dans l'onglet JOURNAL de la fenetre (phases, progression, erreurs, journal
//   filtre : NkEditeurOngletJournal.cpp) ; lire le resultat.
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
#include "Livraison/NkEditeurDeroulement.h"

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
				/// (2026-10-01, retour 6 de Rihen) Le dossier de sortie est RETENU PAR
				/// PROJET (NkEditeurProjet.h) : `sortieDe` est le projet dont `sortie`
				/// vient ; `--sortie=` l'IMPOSE (garde la priorite a l'ouverture).
				bool sortieImposee = false;
				NkString sortieDe;
				/// « Parcourir… » : une DEMANDE, lue par la trame, qui ouvre LE
				/// selecteur de l'editeur en mode dossier (NK_DOSSIER_SORTIE).
				bool parcourirDemande = false;
				nkgui::NkRect boutonParcourir{0.f, 0.f, 0.f, 0.f}; ///< le banc y vise
				/// `--fenetre=construire-auto` : « Construire » est presse tout seul
				/// apres ce nombre de trames (le viseur a alors cadre la scene).
				/// C'est ce qui eprouve la fenetre SANS souris. 0 = jamais.
				int32 lancementAuto = 0;
				NkDisponibilite dispo[NK_NB_PLATEFORMES];

				NkEtatConstruction etat = NkEtatConstruction::NK_REPOS;
				NkPlanConstruction plan;
				/// Le deroulement (commandes, moteur, journal) : le meme que
				/// `--construire=`. `lignesVues` : ce qui est deja passe au tiroir.
				NkDeroulementConstruction deroulement;
				usize lignesVues = 0;
				NkString annonce; ///< la ligne d'etat de la fenetre

				// ── L'onglet Journal (2026-10-01) ─────────────────────────────
				/// 0 Reglages, 1 Journal : « Construire » y bascule.
				int32 onglet = 0;
				/// 0 tout, 1 avertissements et erreurs, 2 erreurs seules.
				int32 filtre = 0;
				/// La premiere ligne montree ; `suivre` : collee a la derniere,
				/// jusqu'a ce qu'on remonte a la molette.
				float32 defilement = 0.f;
				bool suivre = true;
				/// Un retour bref (« Journal copié ») et l'heure ou il s'efface.
				NkString retour;
				float32 retourJusqua = 0.f;
		};

		/// Ouvre la fenetre, avec les reglages precedents ou ceux par defaut, et
		/// refait la detection des chaines (elles ont pu etre installees).
		void NkEditeurOuvrirConstruire(NkEditeurConstruction &k, NkEditeurModele &m);

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

		/// L'onglet Journal de la fenetre, dans `zone` (NkEditeurOngletJournal.cpp) :
		/// les phases et leur etat, la barre de progression, le resume des
		/// erreurs, puis le journal ligne par ligne, colore et filtre -- a la
		/// maniere de l'Output Log d'Unreal Engine 5.
		void NkEditeurDessinerJournalConstruction(NkEditeurCadre &c, NkEditeurConstruction &k, const nkgui::NkRect &zone);

		// ── Les pieces de l'Output Log, PARTAGEES (2026-10-01) ────────────────
		// L'onglet Journal de « Construire » et le tiroir « Journal » de l'editeur
		// (NkEditeurTiroir.cpp) les emploient tous deux : rien n'est recopie.
		/// Le filtre : 0 tout, 1 avertissements et erreurs, 2 erreurs seules.
		bool NkEditeurNiveauMontre(NkNiveauLigne n, int32 filtre) noexcept;
		/// La couleur d'une ligne de ce niveau (roles StatusErr, StatusWarn,
		/// StatusOk du theme ; la palette de l'editeur pour le reste).
		nkgui::NkColor NkEditeurCouleurNiveau(const NkEditeurCadre &c, NkNiveauLigne n);
		/// Le fond d'une ligne d'erreur ou d'avertissement : un voile de sa
		/// couleur et un trait de 2 px a gauche. Rien pour les autres niveaux.
		void NkEditeurBandeNiveau(const NkEditeurCadre &c, nkgui::NkGuiDrawList &dl, const nkgui::NkRect &r, NkNiveauLigne n);
		/// Les trois puces « Tout N », « Avertissements N », « Erreurs N », a
		/// partir de (x, y), hauteur `h` ; un clic change `filtre` (et `*change`).
		/// `rects` (3, facultatif) recoit leurs rectangles. Rend le x d'apres.
		float32 NkEditeurPucesNiveau(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 h, int32 tout,
									 int32 avertissements, int32 erreurs, int32 &filtre, bool *change = nullptr,
									 nkgui::NkRect *rects = nullptr);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURFENETRECONSTRUIRE_H__
