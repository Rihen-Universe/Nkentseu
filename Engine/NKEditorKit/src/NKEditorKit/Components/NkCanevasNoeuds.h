//
// NkCanevasNoeuds.h
// =============================================================================
// Description :
//   LE CANEVAS NODAL du kit (couche 2 de NKGraph : « le canevas, dans
//   NKEditorKit »). Il dessine un graph::NkNodeGraph et l'EDITE : deplacer un
//   noeud, tirer un fil d'une prise a une autre (refus NOMME par NkLinkErrorName),
//   couper les fils d'une prise, saisir la valeur d'une entree libre, supprimer
//   le noeud choisi, se deplacer et zoomer.
//
// Caracteristiques :
//   - GENERIQUE : il ne connait aucun domaine. Couleurs des types, en-tetes,
//     texte et saisie des valeurs par defaut viennent du DOMAINE (couche 3) par
//     NkDomaineCanevas ; les longueurs et couleurs du dessin par NkStyleCanevas.
//     Premier client : les Blueprints d'UnkenyEditor (2026-10-01) ; les
//     materiaux et le graphe des etats de l'Animateur peuvent s'en servir tels
//     quels.
//   - Peint avec nkgui::NkGuiDrawList (pas de primitive de Bezier : les fils
//     sont des cubiques ECHANTILLONNEES en polyligne, G4 du document 01).
//   - Deux familles de prises : EXECUTION (triangle blanc, fil epais) et
//     DONNEES (disque de la couleur du type). Une entree de donnee LIBRE montre
//     sa valeur ; un clic l'edite (Entree valide, Echap annule).
//   - Mode immediat : l'etat (vue, choix, geste en cours) est a l'appelant
//     (NkEtatCanevas), qui lit en retour `modifie` (pour son historique) et
//     `menuDemande` (clic droit dans le vide : a lui d'ouvrir sa palette).
//   - Les GESTES : molette = zoom autour du curseur ; bouton du milieu, droit
//     ou gauche dans le vide = deplacer la vue ; gauche sur un noeud = le
//     choisir et le deplacer ; gauche depuis une prise = tirer un fil ; droit
//     sur une prise = couper ses fils ; Suppr = supprimer le noeud choisi.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKEDITORKIT_NKCANEVASNOEUDS_H__
#define __NKENTSEU_NKEDITORKIT_NKCANEVASNOEUDS_H__

#include "NKContainers/String/NkString.h"
#include "NKGraph/NkNodeGraph.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKGui/Core/NkGuiInput.h"

namespace nkentseu {
	namespace editorkit {

		/// Les longueurs et les couleurs du dessin. Les valeurs par defaut suivent
		/// UI_SPEC § 10ter (fil d'execution blanc et epais).
		struct NkStyleCanevas {
				nkgui::NkColor fond{24, 24, 24, 255};
				nkgui::NkColor grille{34, 34, 34, 255};
				nkgui::NkColor grilleForte{44, 44, 44, 255};
				nkgui::NkColor corps{42, 42, 42, 240};
				nkgui::NkColor bord{12, 12, 12, 255};
				nkgui::NkColor selection{255, 170, 40, 255};
				nkgui::NkColor erreur{230, 60, 50, 255};
				nkgui::NkColor texte{220, 220, 220, 255};
				nkgui::NkColor attenue{150, 150, 150, 255};
				nkgui::NkColor champ{20, 20, 20, 255};
				nkgui::NkColor exec{240, 240, 240, 255};
				float32 largeurNoeud = 220.f;
				float32 hauteurEntete = 24.f;
				float32 hauteurRangee = 22.f;
				float32 rayonPrise = 5.f;
				float32 pasGrille = 32.f;
				float32 largeurChamp = 76.f;
		};

		/// Ce que le DOMAINE dit au canevas. Chaque rappel est facultatif.
		struct NkDomaineCanevas {
				void *donnees = nullptr;
				/// La couleur d'une prise de donnees de type `t`.
				nkgui::NkColor (*CouleurType)(void *d, const graph::NkNodeGraph &g, graph::NkTypeId t) = nullptr;
				/// La couleur de l'en-tete d'un noeud.
				nkgui::NkColor (*CouleurEntete)(void *d, const graph::NkNode &n) = nullptr;
				/// La valeur d'une entree LIBRE en texte ; vide = pas de champ.
				NkString (*TexteDefaut)(void *d, const graph::NkNodeGraph &g, const graph::NkNode &n,
										const graph::NkSocket &s) = nullptr;
				/// Applique une saisie a l'entree `prise`. false : refusee.
				bool (*PoserDefaut)(void *d, graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise,
									const char *texte) = nullptr;
		};

		/// L'etat du canevas, garde par l'appelant d'une trame a l'autre.
		struct NkEtatCanevas {
				/// Le point du graphe au coin haut-gauche de la zone, et le zoom.
				float32 vueX = -40.f, vueY = -40.f;
				float32 zoom = 1.f;
				graph::NkNodeId selection = graph::NK_NODE_INVALID;
				/// Pose par l'appelant : le noeud d'une erreur, entoure de rouge.
				graph::NkNodeId enErreur = graph::NK_NODE_INVALID;

				// --- Le geste en cours -------------------------------------------
				int32 geste = 0; ///< 0 aucun, 1 deplacer un noeud, 2 tirer un fil, 3 vue
				graph::NkNodeId gesteNoeud = graph::NK_NODE_INVALID;
				int32 gestePrise = -1;
				float32 gesteDx = 0.f, gesteDy = 0.f; ///< saisie (noeud) ou depart (vue)
				float32 appuiX = 0.f, appuiY = 0.f;
				int32 bouton = 0;

				// --- La saisie d'une valeur --------------------------------------
				graph::NkNodeId saisieNoeud = graph::NK_NODE_INVALID;
				int32 saisiePrise = -1;
				char saisie[96] = {};

				// --- Ce que la trame rend ----------------------------------------
				bool modifie = false;	  ///< le graphe a change (a retenir dans l'historique)
				bool menuDemande = false; ///< clic droit dans le vide
				float32 menuX = 0.f, menuY = 0.f; ///< ou, en coordonnees du GRAPHE
				NkString refus;			  ///< le dernier fil refuse, et pourquoi
				float32 ageRefus = 99.f;
				bool clavierPris = false; ///< le canevas a pris le clavier (Suppr, saisie)
		};

		/// Dessine le graphe dans `zone` et applique les gestes de `in` (dont la
		/// souris est dans `zone`). `police` peut etre nulle (pas de texte).
		void NkCanevasNoeuds(nkgui::NkGuiDrawList &dl, nkgui::NkGuiInput &in, nkgui::NkGuiFont *police,
							 const nkgui::NkRect &zone, graph::NkNodeGraph &g, NkEtatCanevas &e, const NkStyleCanevas &s,
							 const NkDomaineCanevas &d);

		// --- La geometrie (bancs, captures, palette) ---------------------------
		nkgui::NkVec2 NkCanevasVersEcran(const NkEtatCanevas &e, const nkgui::NkRect &zone, float32 x, float32 y) noexcept;
		nkgui::NkVec2 NkCanevasDepuisEcran(const NkEtatCanevas &e, const nkgui::NkRect &zone, const nkgui::NkVec2 &p) noexcept;
		nkgui::NkRect NkCanevasRectNoeud(const NkEtatCanevas &e, const nkgui::NkRect &zone, const graph::NkNode &n,
										 const NkStyleCanevas &s) noexcept;
		/// La position A L'ECRAN de la prise `k` de `n`. false : pas de prise k.
		bool NkCanevasPrise(const NkEtatCanevas &e, const nkgui::NkRect &zone, const graph::NkNode &n, int32 k,
							const NkStyleCanevas &s, nkgui::NkVec2 &sortie) noexcept;
		/// Cadre le graphe dans la zone (zoom borne a [0,7 ; 1,25] : lisible ; plus
		/// grand que la vue, il part de son coin haut-gauche).
		void NkCanevasCadrer(NkEtatCanevas &e, const nkgui::NkRect &zone, const graph::NkNodeGraph &g,
							 const NkStyleCanevas &s) noexcept;

	} // namespace editorkit
} // namespace nkentseu

#endif // __NKENTSEU_NKEDITORKIT_NKCANEVASNOEUDS_H__
