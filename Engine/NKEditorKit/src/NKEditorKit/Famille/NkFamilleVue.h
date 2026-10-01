#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleVue.h
// @Brief   LA COLONNE CENTRALE d'Unreal 5 de la famille : la barre de vue
//          (boutons a gauche, reperes a droite), la BARRE FLOTTANTE du viseur
//          (Selection / Deplacer / Tourner / Echelle, repere monde/local,
//          accrochages et leurs pas) et le REPERE d'axes du coin bas gauche.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT : UnkenyEditor, NkEditeurVue.cpp (BarreVue, IconeBarre,
// BarreFlottante, Repere). Le viseur lui-meme (ce qui est rendu dedans) est a
// l'application : NKCanvas chez Unkeny, l'image hors ecran de NKRenderer chez
// Nogee. Ces pieces se posent PAR-DESSUS.
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editorkit {

		/// Un bouton de la barre de vue (« Cadrer », « Grille »...).
		struct NkFamilleBoutonVue {
				const char *texte = "";
				bool enfonce = false;
		};
		/// La barre de vue : rend l'indice du bouton clique (-1 aucun).
		int32 NkFamilleBarreVue(NkFamilleCtx &c, const nkgui::NkRect &b, const NkFamilleBoutonVue *boutons, int32 n,
								const char *reperes);

		/// Les icones de la barre flottante, tracees.
		enum class NkFamilleIconeBarre : uint8 {
			Selection = 0,
			Deplacer,
			Tourner,
			Echelle,
			Monde,
			Local,
			Grille,
			Angle,
			PasEchelle,
			Aimant,
			Camera,
			Eclaire
		};
		void NkFamillePeindreIconeBarre(nkgui::NkGuiDrawList &dl, NkFamilleIconeBarre ic, const nkgui::NkRect &r,
										const nkgui::NkColor &col) noexcept;

		/// Un element de la barre flottante : une icone, ou une VALEUR (texte non
		/// vide, avec la pointe « ▾ » qui dit qu'elle se choisit).
		struct NkFamilleElementBarre {
				NkFamilleIconeBarre icone = NkFamilleIconeBarre::Selection;
				NkString texte;
				bool enfonce = false;
				bool eteint = false;	 ///< la valeur d'un accrochage eteint : attenuee
				bool separateur = false; ///< un trait AVANT cet element
				NkString bulle;			 ///< l'infobulle, sous la barre
		};
		struct NkFamilleBarreFlottanteResultat {
				int32 clic = -1;
				nkgui::NkRect rect{0.f, 0.f, 0.f, 0.f}; ///< le bouton clique (ancre d'un menu)
				nkgui::NkRect barre{0.f, 0.f, 0.f, 0.f}; ///< la barre (vide : retiree, viseur trop etroit)
		};
		/// La barre, calee sur le coin HAUT DROIT du viseur `aire`. `menuOuvert` :
		/// pas d'infobulle tant qu'un menu est ouvert.
		NkFamilleBarreFlottanteResultat NkFamilleBarreFlottante(NkFamilleCtx &c, const nkgui::NkRect &aire,
																const NkFamilleElementBarre *elements, int32 n,
																bool menuOuvert);

		/// Le repere d'axes du coin bas gauche. `axes` : les directions A L'ECRAN
		/// (y vers le bas) des axes X, Y, Z du monde ; `nbAxes` 2 (une vue 2D) ou 3.
		/// Un axe tres court (vu de face) ne trace que son point.
		void NkFamilleRepereAxes(NkFamilleCtx &c, nkgui::NkGuiDrawList &dl, const nkgui::NkRect &aire,
								 const nkgui::NkVec2 *axes, int32 nbAxes);

	} // namespace editorkit
} // namespace nkentseu
