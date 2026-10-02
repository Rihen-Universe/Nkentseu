//
// NkMonBlueprint.h
// =============================================================================
// Description :
//   LE PANNEAU « MON BLUEPRINT » d'Unreal 5, generique : des SECTIONS
//   repliables (Graphes, Fonctions, Macros, Variables, Repartiteurs...) avec
//   leur « + », des elements (une pastille de couleur -- le type d'une
//   variable --, un libelle, un sous-texte), une recherche, « + Ajouter ».
//   Il SIGNALE (choisi, double-clic, renomme, supprimer, glisse-depose, menu) :
//   ce que ces gestes font est a l'appelant. Nogee, NkAnimaEditor et NKUIDesign
//   peuvent le reprendre tel quel.
//
// Caracteristiques :
//   - Renommer EN PLACE : F2, ou un clic lent sur l'element deja choisi ;
//     Entree valide, Echap annule.
//   - GLISSER un element hors du panneau (une variable vers la toile) : le
//     panneau rend la position du lacher ; l'appelant dessine ce qu'il veut
//     pendant le geste (`glisseEnCours`).
//   - Les couleurs et tailles : les jetons (NkStyleNodal.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKEDITORKIT_NKMONBLUEPRINT_H__
#define __NKENTSEU_NKEDITORKIT_NKMONBLUEPRINT_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKEditorKit/Components/NkStyleNodal.h"
#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"

namespace nkentseu {
	namespace editorkit {

		struct NkElementPanneau {
				NkString libelle;
				NkString sousTexte; ///< a droite, attenue (« Booléen »)
				bool pastille = false;
				nkgui::NkColor couleur{154, 163, 173, 255};
				/// Le glyphe dans la pastille (« V/F ») ; vide : une capsule pleine.
				NkString glyphe;
				int32 id = -1;
				/// Un OEIL dessine avant le sous-texte : « modifiable par instance »
				/// (l'oeil d'UE5 ; la police n'a pas de glyphe sur lequel compter).
				bool oeil = false;
		};

		struct NkSectionPanneau {
				NkString titre;
				bool plus = true; ///< un « + » pour en ajouter
				NkVector<NkElementPanneau> elements;
		};

		struct NkEtatMonBlueprint {
				NkVector<NkString> replies;
				char filtre[48] = {};
				bool filtreFocus = false;
				int32 choixSection = -1, choixId = -1;
				float32 defil = 0.f;
				// --- Le renommage en place ---------------------------------------
				int32 renommeSection = -1, renommeId = -1;
				char renomme[64] = {};
				// --- Le geste « glisser » ------------------------------------------
				bool appui = false;
				int32 appuiSection = -1, appuiId = -1;
				float32 appuiX = 0.f, appuiY = 0.f;
				bool glisse = false;
				float32 ageChoix = 99.f; ///< depuis le dernier choix (le clic lent renomme)
				// --- Le menu « + Ajouter » ----------------------------------------
				bool menuAjouter = false;
				// --- La geometrie de la derniere trame (bancs, captures) ---------
				NkVector<nkgui::NkRect> rects;
				NkVector<int32> sections, ids;
				NkVector<nkgui::NkRect> rectsPlus;
				nkgui::NkRect rectAjouter{0.f, 0.f, 0.f, 0.f};
				NkVector<nkgui::NkRect> rectsMenuAjouter;
		};

		struct NkResultatMonBlueprint {
				int32 ajouter = -1; ///< la section dont on a demande un element neuf
				int32 choisiSection = -1, choisiId = -1;
				bool doubleClic = false;
				bool menu = false; ///< clic droit sur l'element choisi
				float32 menuX = 0.f, menuY = 0.f;
				bool renomme = false;
				NkString nouveauNom;
				bool supprimer = false; ///< Suppr sur l'element choisi
				bool glisseEnCours = false;
				bool depose = false; ///< le glisse est lache HORS du panneau
				int32 glisseSection = -1, glisseId = -1;
				float32 x = 0.f, y = 0.f; ///< ou (ecran)
				bool clavierPris = false;
		};

		/// Dessine le panneau dans `r` et le fait vivre.
		NkResultatMonBlueprint NkMonBlueprint(nkgui::NkGuiContext &ctx, nkgui::NkGuiDrawList &dl, nkgui::NkGuiFont *police,
											  const nkgui::NkRect &r, const NkVector<NkSectionPanneau> &sections,
											  NkEtatMonBlueprint &e, const NkJetonsNodal &s, const char *titre);

		/// Commence le renommage de l'element (section, id), son nom actuel `nom`.
		void NkMonBlueprintRenommer(NkEtatMonBlueprint &e, int32 section, int32 id, const char *nom);

		// --- Petites ICONES dessinees (la police n'a ni ✓ ni ✗ ni ◆) -----------
		/// Un oeil (amande + pupille) centre en (cx, cy), de largeur `w`.
		void NkIconeOeil(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, float32 w, const nkgui::NkColor &c);
		/// Une coche (reussi), dans le carre de cote `w` centre en (cx, cy).
		void NkIconeCoche(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, float32 w, const nkgui::NkColor &c);
		/// Une croix (echec), dans le carre de cote `w` centre en (cx, cy).
		void NkIconeCroix(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, float32 w, const nkgui::NkColor &c);

	} // namespace editorkit
} // namespace nkentseu

#endif // __NKENTSEU_NKEDITORKIT_NKMONBLUEPRINT_H__
