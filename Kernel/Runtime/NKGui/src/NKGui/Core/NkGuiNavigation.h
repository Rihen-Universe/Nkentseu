//
// NkGuiNavigation.h
// =============================================================================
// Description :
//   La navigation AU FOCUS dans NKGui : passer d'un controle a l'autre avec
//   la croix ou le stick d'une manette (ou les fleches), l'activer avec un
//   bouton, fermer avec un autre. C'est ce qui rend un menu de jeu jouable
//   sans souris ni doigt -- a la manette, a la telecommande de television.
//
// Caracteristiques :
//   - ⚠️ DESACTIVEE PAR DEFAUT, et sans effet tant qu'on ne l'allume pas :
//     aucune application existante ne change de conduite.
//   - Elle ne demande RIEN aux widgets : elle lit le RELEVE de l'introspection
//     (NkGuiIntrospect.h), ou chaque controle depose deja sa nature, son id et
//     son rectangle. Un controle qui existe est donc navigable, sans une
//     ligne dans NkGuiWidgets.cpp.
//   - Elle AGIT par le seul canal public, `ctx.input` -- le meme que remplit
//     toute application depuis ses evenements : le controle focalise recoit
//     un pointeur virtuel en son centre (il s'affiche donc SURVOLE, avec le
//     rendu de survol qu'il a deja), et l'activation est un clic complet
//     (position, appui, relachement) sur trois images, parce que NKGui
//     resout le survol avec une image de retard (hotIdPrev).
//   - Deplacement SPATIAL : le plus proche dans la direction demandee (ecart
//     dans l'axe + deux fois l'ecart de travers), comme les menus de console.
//
// Usage (une fois par image, AVANT ctx.BeginFrame, apres les evenements) :
//     nav.actif = true;                         // une fois
//     NkGuiNavAvancer(ctx, nav, direction, activer, annuler);
//     ctx.BeginFrame(dt); ... widgets ... NkGuiNavDessiner(ctx, nav); ctx.EndFrame();
//   `direction` vient de la croix, du stick ou des fleches (avec la
//   repetition que l'application veut) ; `activer` du bouton Sud, `annuler`
//   de l'Est. La souris qui bouge rend la main : NkGuiNavSourisBougee(nav).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NKGUI_NKGUINAVIGATION_H__
#define __NKENTSEU_NKGUI_NKGUINAVIGATION_H__

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"

namespace nkentseu {
	namespace nkgui {

		struct NkGuiContext;
		struct NkGuiInput;

		/// ⚠️ EN AJOUT SEUL : `Suivant` / `Precedent` (02/10) viennent APRES les
		///    quatre directions, aucune valeur existante ne bouge.
		enum class NkGuiNavDirection : uint8 {
			Aucune = 0,
			Haut,
			Bas,
			Gauche,
			Droite,
			/// (02/10) Tab : le controle suivant dans l'ORDRE DE LECTURE (de haut
			/// en bas, puis de gauche a droite), en bouclant.
			Suivant,
			/// (02/10) Maj+Tab : le precedent, en bouclant.
			Precedent
		};

		struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiNavigation {
				/// ⚠️ FAUX PAR DEFAUT : rien ne se passe tant qu'on ne l'allume pas.
				bool actif = false;
				/// Le controle focalise (id de sa note), et son rectangle a
				/// l'image d'avant. NKGUI_ID_NONE = aucun encore.
				NkGuiId focus = NKGUI_ID_NONE;
				NkRect focusRect = {0.f, 0.f, 0.f, 0.f};
				/// Le pointeur virtuel est-il pose ? Faux des que la souris bouge.
				bool pilote = false;
				/// Activation en cours : 0 rien, 1 viser, 2 appuyer, 3 relacher.
				int32 phase = 0;
				/// Annulation en cours : Echap pose une image, relache la suivante.
				bool echapPose = false;

				// ── AJOUTS DU 02/10 (l'interface en jeu d'Unkeny) ───────────────
				/// Sans voisin dans la direction demandee, passer a l'AUTRE BOUT
				/// (le dernier bouton d'une colonne mene au premier), comme les
				/// menus de console. Faux par defaut : la conduite d'avant.
				bool boucler = false;
				/// La CLE STABLE du controle focalise (sa note : l'identifiant du
				/// document pour un widget monte d'un `.nkgui`), vide sinon. C'est
				/// elle qu'un hote compare -- jamais le libelle, qui se traduit.
				char focusCle[48] = {};
				/// Un focus DEMANDE par l'hote (NkGuiNavFocaliserCle), servi a la
				/// prochaine NkGuiNavAvancer s'il existe a l'ecran. Vide = aucun.
				char cleDemandee[48] = {};
				/// La demande montre-t-elle le cadre (manette en main) ?
				bool demandeMontre = false;
		};

		/// Avance la navigation d'une image. A appeler AVANT BeginFrame.
		/// Sans effet si `nav.actif` est faux.
		NKENTSEU_NKGUI_API void NkGuiNavAvancer(NkGuiContext &ctx, NkGuiNavigation &nav, NkGuiNavDirection direction,
												bool activer, bool annuler) noexcept;

		/// Le cadre du focus, dans la surimpression. A appeler entre BeginFrame
		/// et EndFrame. Sans effet si la navigation n'est pas pilotee.
		NKENTSEU_NKGUI_API void NkGuiNavDessiner(NkGuiContext &ctx, const NkGuiNavigation &nav) noexcept;

		/// La souris a bouge : elle reprend la main (le pointeur virtuel s'efface).
		NKENTSEU_NKGUI_API void NkGuiNavSourisBougee(NkGuiNavigation &nav) noexcept;

		/// (02/10) DONNER le focus au controle de cle stable `cle` (l'identifiant
		/// du document) : servi a la prochaine NkGuiNavAvancer, s'il est a l'ecran
		/// et focalisable. `montrer` : le cadre apparait tout de suite (un menu
		/// ouvert a la manette) ; sinon il attend le premier geste. Une cle
		/// absente laisse le focus ou il etait.
		NKENTSEU_NKGUI_API void NkGuiNavFocaliserCle(NkGuiNavigation &nav, const char *cle, bool montrer) noexcept;

		/// (02/10) Ce que le CLAVIER demande a la navigation, lu dans `in` (les
		/// touches que l'application a deja posees) : fleches (avec repetition),
		/// Tab / Maj+Tab, Entree (activer), Echap (annuler). Rend vrai si une
		/// touche de navigation a ete lue. ⚠️ A appeler AVANT NkGuiNavAvancer, qui
		/// peut poser Echap lui-meme (annuler) : le lire apres le compterait deux
		/// fois.
		NKENTSEU_NKGUI_API bool NkGuiNavDepuisClavier(const NkGuiInput &in, NkGuiNavDirection &direction, bool &activer,
													  bool &annuler) noexcept;

	} // namespace nkgui
} // namespace nkentseu

#endif // __NKENTSEU_NKGUI_NKGUINAVIGATION_H__
