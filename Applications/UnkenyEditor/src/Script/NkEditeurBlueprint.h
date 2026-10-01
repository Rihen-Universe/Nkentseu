//
// NkEditeurBlueprint.h
// =============================================================================
// Description :
//   L'EDITEUR DE BLUEPRINT A LA UNREAL 5 (demande de Rihen du 01/10), au style
//   de SA charte (References/Blueprint/CHARTE.md) : un ENSEMBLE AUTONOME qui se
//   dessine dans un rectangle donne -- sa barre d'outils (Compiler avec son
//   etat, Enregistrer, Rechercher, Reglages de classe, Valeurs par defaut,
//   Simuler), le panneau « Mon Blueprint » (Graphes, Fonctions, Macros,
//   Variables, Repartiteurs), la toile et ses onglets de graphe, les Details
//   de l'element choisi, et en bas les Resultats de compilation, de recherche
//   et la Console de simulation.
//
// Caracteristiques :
//   - AUTONOME (contrainte d'integration du 01/10) : il ne touche ni la barre
//     d'onglets ni l'aiguillage du dessin de l'application ; ce qui depend de
//     l'editeur (compiler ET donner au registre, jouer la scene, la trace) lui
//     est donne par NkHoteBlueprint. L'integration le mettra dans un onglet
//     de document ; aujourd'hui NkEditeurGraphe.cpp l'ouvre a la place de la
//     vue, en plein.
//   - La partie GENERIQUE est dans NKEditorKit (toile, menu des noeuds, Mon
//     Blueprint, jetons du style) ; ici, le DOMAINE (le document, le catalogue,
//     le compilateur) et la mise en page.
//   - Annuler / refaire : le DOCUMENT entier (NkBpHistorique).
//
// Gestes (UE5) :
//   clic droit dans le vide = le menu des noeuds (recherche, categories) ;
//   lacher un fil dans le vide = le menu SENSIBLE AU CONTEXTE ; glisser une
//   variable de Mon Blueprint sur la toile = Lire / Ecrire (Ctrl = lire,
//   Alt = ecrire) ; double-clic sur une fonction = son graphe ; C = un cadre
//   autour du choix ; double-clic sur un fil = un relais ; Ctrl+C / X / V / D ;
//   Maj+W / A / S / D aligne ; Q redresse ; Ctrl+Z / Y ; Ctrl+S compile et
//   enregistre ; Ctrl+F cherche ; F5 compile.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURBLUEPRINT_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURBLUEPRINT_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkCanevasNoeuds.h"
#include "NKEditorKit/Components/NkMenuNoeuds.h"
#include "NKEditorKit/Components/NkMonBlueprint.h"
#include "NKGui/Core/NkGuiContext.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkBpDocument.h"
#include "Unkeny/Script/NkUnkenyScripts.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurBlueprintEtat;

		/// Ce que l'editeur de Blueprint demande a celui qui l'heberge.
		struct NkHoteBlueprint {
				void *donnees = nullptr;
				/// Ecrit le .nkbp (GRAF + MODL + DOCU) et donne le module au
				/// registre. false : `err` dit pourquoi (le graphe est ecrit quand
				/// meme). `message` : ce qu'il faut dire.
				bool (*Compiler)(void *d, NkEditeurBlueprintEtat &e, NkErreurBp &err, NkString &message) = nullptr;
				/// La trace d'execution de ce Blueprint (Simuler), ou nul.
				const unkeny::NkTraceBp *(*Trace)(void *d, const char *ref) = nullptr;
				/// Simuler : demarrer (true) ou arreter (false) la partie, trace allumee.
				void (*Simuler)(void *d, bool demarrer) = nullptr;
				bool (*EnJeu)(void *d) = nullptr;
				/// « Fermer ».
				void (*Fermer)(void *d) = nullptr;
				/// Une ligne pour le Journal de l'application.
				void (*Journal)(void *d, const char *ligne, bool faute) = nullptr;
		};

		/// Ce que montrent les Details.
		enum class NkChoixBp : uint8 { NK_RIEN = 0, NK_VARIABLE, NK_GRAPHE, NK_REPARTITEUR, NK_NOEUD, NK_CLASSE, NK_DEFAUTS };

		/// Une ligne des resultats (compilation, recherche, console).
		struct NkLigneResultatBp {
				NkString texte;
				uint8 genre = 0u; ///< 0 information, 1 reussite, 2 erreur
				int32 graphe = -1;
				graph::NkNodeId noeud = graph::NK_NODE_INVALID;
		};

		/// Un petit menu surgissant (choix d'un type, menu d'un noeud...).
		struct NkPopupBp {
				bool ouvert = false;
				uint8 genre = 0u; ///< a l'appelant (NkEditeurBlueprint.cpp)
				float32 x = 0.f, y = 0.f;
				NkVector<NkString> libelles;
				NkVector<int32> ids;
				NkVector<bool> grises;
				int32 cible = -1; ///< ce sur quoi il agit (variable, prise...)
				float32 gx = 0.f, gy = 0.f; ///< un point du graphe (deposer une variable)
				NkVector<nkgui::NkRect> rects;
		};

		struct NkEditeurBlueprintEtat {
				bool ouvert = false;
				NkString chemin; ///< le .nkbp, absolu
				NkString ref;	 ///< « Contenu/Scripts/Porte.nkbp »
				NkString nom;	 ///< « Porte »
				NkDocumentBp doc;
				NkBpHistorique historique;
				bool modifie = false;

				// --- La compilation ---------------------------------------------
				/// 0 a compiler, 1 compile, 2 en erreur (l'icone du bouton).
				uint8 etatCompil = 0u;
				NkString message;
				bool erreur = false;
				NkErreurBp derniereErreur;

				// --- Les onglets de graphe, et une toile par graphe ---------------
				NkVector<uint32> onglets; ///< indices dans doc.graphes
				uint32 actif = 0u;		  ///< le graphe a l'ecran
				NkVector<editorkit::NkEtatCanevas> toiles;
				NkVector<bool> cadrer;

				// --- Les panneaux ---------------------------------------------------
				editorkit::NkEtatMonBlueprint mon;
				editorkit::NkEtatMenuNoeuds menu;
				NkVector<NkEntreeMenuBp> menuEntrees;	///< ce que propose le menu ouvert
				uint32 menuGraphe = 0u;
				float32 menuX = 0.f, menuY = 0.f;		///< ou poser (graphe)
				graph::NkNodeId menuNoeud = graph::NK_NODE_INVALID; ///< le fil lache depuis...
				int32 menuPrise = -1;
				NkPopupBp popup;

				// --- Les Details ----------------------------------------------------
				NkChoixBp choix = NkChoixBp::NK_RIEN;
				int32 choixIndice = -1; ///< variable, graphe, repartiteur
				int32 champ = -1;		///< le champ des Details qui a le clavier
				char tampon[256] = {};
				float32 defilDetails = 0.f;

				// --- En bas ---------------------------------------------------------
				int32 ongletBas = 0; ///< 0 compilation, 1 recherche, 2 console de simulation
				NkVector<NkLigneResultatBp> resultats;
				NkVector<NkLigneResultatBp> trouves;
				char recherche[64] = {};
				bool rechercheFocus = false;
				float32 defilBas = 0.f;

				// --- Le code d'un noeud en cours d'edition -----------------------------
				graph::NkNodeId codeNoeud = graph::NK_NODE_INVALID;
				uint32 codeGraphe = 0u;
				char code[1024] = {};
				uint32 caret = 0u;
				float32 clignote = 0.f;

				// --- Simuler --------------------------------------------------------
				bool simulation = false;
				const unkeny::NkTraceBp *trace = nullptr; ///< pose a chaque trame par l'hote

				// --- La geometrie de la derniere trame (bancs, captures) ---------
				nkgui::NkRect zone{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect zoneToile{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectMon{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectDetails{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectBas{0.f, 0.f, 0.f, 0.f};
				/// Les boutons de la barre : Compiler, Enregistrer, Rechercher, Reglages
				/// de classe, Valeurs par defaut, Simuler, Fermer.
				nkgui::NkRect boutons[7] = {};
				NkVector<nkgui::NkRect> rectsOnglets;
				/// Les champs des Details a la derniere trame : (id, rect).
				NkVector<int32> champsIds;
				NkVector<nkgui::NkRect> champsRects;

				editorkit::NkEtatCanevas &Toile() {
					return toiles[actif];
				}
				graph::NkNodeGraph &Graphe() {
					return doc.graphes[actif].graphe;
				}
		};

		/// Les boutons de la barre (indices de `boutons`).
		enum NkBoutonBp : int32 {
			NK_BP_COMPILER = 0,
			NK_BP_ENREGISTRER,
			NK_BP_RECHERCHER,
			NK_BP_REGLAGES,
			NK_BP_DEFAUTS,
			NK_BP_SIMULER,
			NK_BP_FERMER
		};

		/// Prend le document : onglets, toiles, historique, choix remis a zero.
		void NkEditeurBlueprintCharger(NkEditeurBlueprintEtat &e);
		/// Dessine l'editeur dans `r` et le fait vivre. `petite` peut valoir
		/// `police`. Rend true si l'editeur a pris le clavier (un champ, la toile).
		bool NkEditeurBlueprintDessiner(NkEditeurBlueprintEtat &e, NkHoteBlueprint &hote, nkgui::NkGuiContext &ctx, nkgui::NkGuiFont *police,
										const nkgui::NkRect &r);
		/// Compile (et enregistre) par l'hote, et montre le resultat.
		bool NkEditeurBlueprintCompiler(NkEditeurBlueprintEtat &e, NkHoteBlueprint &hote);
		/// Ouvre (ou montre) l'onglet du graphe `g`.
		void NkEditeurBlueprintOnglet(NkEditeurBlueprintEtat &e, uint32 g);
		/// Retient l'etat du document (annuler / refaire) et le marque modifie.
		void NkEditeurBlueprintRetenir(NkEditeurBlueprintEtat &e);
		/// Pose le noeud de cle `cle` (menu) dans le graphe actif a (x, y) ; s'il
		/// vient d'une prise (menu sensible au contexte), le relie. Rend le noeud.
		graph::NkNodeId NkEditeurBlueprintPoser(NkEditeurBlueprintEtat &e, const char *cle, float32 x, float32 y, graph::NkNodeId depuis = graph::NK_NODE_INVALID,
												int32 prise = -1);
		/// Les entrees du menu de la toile pour le graphe actif ; filtrees par la
		/// prise (n, k) si `contexte` (le menu sensible au contexte).
		void NkEditeurBlueprintEntrees(NkEditeurBlueprintEtat &e, bool contexte, graph::NkNodeId n, int32 k, NkVector<NkEntreeMenuBp> &sortie);
		/// « Reduire en fonction » : le choix de la toile devient une fonction
		/// (ses fils qui entrent et sortent deviennent ses entrees et sorties) et
		/// un appel prend sa place. Rend l'indice du graphe cree, ou -1.
		int32 NkEditeurBlueprintReduireEnFonction(NkEditeurBlueprintEtat &e);
		/// Cherche `texte` dans tout le document (noeuds, valeurs, code, noms).
		void NkEditeurBlueprintChercher(NkEditeurBlueprintEtat &e, const char *texte);
		/// Le domaine de la toile (NkEditeurBlueprintDomaine.cpp).
		editorkit::NkDomaineCanevas NkEditeurBlueprintDomaine(NkEditeurBlueprintEtat &e);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURBLUEPRINT_H__
