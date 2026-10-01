//
// NkMenuNoeuds.h
// =============================================================================
// Description :
//   LE MENU DES NOEUDS de la toile (le clic droit d'Unreal 5 ; charte, planche
//   07 : « un SEUL panneau de recherche, filtre ») : une recherche qui a le
//   clavier des l'ouverture, des CATEGORIES repliables (« Variables|Etat »),
//   la case « Sensible au contexte » et, quand on lache un fil dans le vide,
//   le FILTRE ECRIT et RETIRABLE (« qui acceptent : 1.0 reel ✕ ») ; un
//   resultat vide DIT son perimetre (« un resultat negatif sans son perimetre
//   est une rumeur »).
//
// Caracteristiques :
//   - Generique : les entrees (libelle, categorie, aide, couleur) viennent de
//     l'appelant, qui decide aussi de ce que « compatible » veut dire (il
//     passe la liste deja filtree quand la case est cochee).
//   - Clavier : Haut / Bas, Entree choisit, Echap ferme ; la recherche ignore
//     la casse ET les accents (« evenement » trouve « Événement »).
//   - Les couleurs et tailles : les jetons (NkStyleNodal.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKEDITORKIT_NKMENUNOEUDS_H__
#define __NKENTSEU_NKEDITORKIT_NKMENUNOEUDS_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkStyleNodal.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace editorkit {

		struct NkEntreeMenuNoeud {
				NkString libelle;
				NkString categorie; ///< « Variables|État » : sous-categories par « | »
				NkString aide;
				nkgui::NkColor couleur{154, 163, 173, 255}; ///< la pastille (categorie du noeud)
				int32 id = -1;
		};

		struct NkEtatMenuNoeuds {
				bool ouvert = false;
				float32 x = 0.f, y = 0.f; ///< le coin haut-gauche voulu (ecran)
				char filtre[64] = {};
				bool filtreFocus = false;
				/// La case « Sensible au contexte » (l'appelant filtre si elle est cochee).
				bool sensibleContexte = true;
				/// Le FILTRE de contexte, ecrit (« qui acceptent ») ; vide : aucun.
				NkString contexte;
				nkgui::NkColor contexteCouleur{154, 163, 173, 255};
				NkString contexteGlyphe;
				float32 defil = 0.f;
				int32 clavier = -1; ///< l'entree surlignee au clavier (indice d'affichage)
				NkVector<NkString> replies; ///< les categories repliees
				// --- La geometrie de la derniere trame (bancs, captures) ---------
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectRecherche{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectCase{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect rectContexte{0.f, 0.f, 0.f, 0.f}; ///< le « ✕ » du filtre
				NkVector<nkgui::NkRect> rects;
				NkVector<int32> ids;
				uint32 nbVisibles = 0u;

				/// Ouvre le menu en (x, y), vide la recherche, lui donne le clavier.
				void Ouvrir(float32 px, float32 py, const char *ctx = nullptr) {
					ouvert = true;
					x = px;
					y = py;
					filtre[0] = '\0';
					filtreFocus = true;
					defil = 0.f;
					clavier = -1;
					contexte = ctx != nullptr ? ctx : "";
				}
		};

		/// Ce que rend le menu.
		enum NkResultatMenuNoeuds : int32 {
			NK_MENU_RIEN = -1,	 ///< ouvert, rien de choisi
			NK_MENU_FERME = -2,	 ///< ferme (Echap, clic dehors, croix)
			NK_MENU_CONTEXTE = -3 ///< la case ou la croix du contexte a change : refiltrer
		};

		/// Dessine le menu (s'il est ouvert) et le fait vivre. Rend l'`id` de
		/// l'entree choisie, ou un NkResultatMenuNoeuds. Les entrees passees sont
		/// DEJA filtrees par le contexte (si l'appelant le veut) ; le menu filtre
		/// par la recherche. `titre` : « Toutes les actions pour ce Blueprint ».
		int32 NkMenuNoeuds(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police, NkEtatMenuNoeuds &e,
						   const NkVector<NkEntreeMenuNoeud> &entrees, const NkJetonsNodal &s, const char *titre);

		/// Le texte replie (minuscules, sans accents) : la recherche du menu.
		NkString NkReplierTexte(const char *texte);
		bool NkContientReplie(const char *texte, const char *motif);

	} // namespace editorkit
} // namespace nkentseu

#endif // __NKENTSEU_NKEDITORKIT_NKMENUNOEUDS_H__
