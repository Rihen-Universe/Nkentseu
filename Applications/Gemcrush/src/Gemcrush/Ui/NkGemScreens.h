// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemScreens.h
// DESCRIPTION: Écrans hors partie — barre de titre personnalisée, menu
//              principal, carte d'aventure.
//
//              LA BARRE DE TITRE EST UNE ZONE SÛRE COMME UNE AUTRE. Elle n'a
//              pas d'API à part : l'application ajoute sa hauteur à l'inset
//              HAUT, et toute la mise en page l'évite déjà. Un mécanisme de
//              plus aurait été un mécanisme de plus à tenir d'accord avec
//              l'encoche des téléphones.
//
//              LA CARTE PARTAGE SA GÉOMÉTRIE ENTRE LE DESSIN ET LE CLIC.
//              NkGemMapGeometry calcule la position d'un nœud ; DrawAdventureMap
//              dessine à partir d'elle et HitTest désigne à partir d'elle. Deux
//              copies auraient divergé au premier ajustement, et le symptôme
//              aurait été « le clic tombe à côté », qu'on aurait cherché dans
//              l'entrée.
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMSCREENS_H
#define NKENTSEU_GAME_UI_NKGEMSCREENS_H

#include "Gemcrush/Ui/NkGemHud.h"
#include "Gemcrush/NkGemLevels.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			// =============================================================
			// Barre de titre personnalisée (bureau uniquement)
			// =============================================================
			struct NkGemTitleBar {
					NkRect bar{0.f, 0.f, 0.f, 0.f};
					NkRect close{0.f, 0.f, 0.f, 0.f};
					NkRect maximize{0.f, 0.f, 0.f, 0.f};
					NkRect minimize{0.f, 0.f, 0.f, 0.f};

					/// @brief Zone qui déplace la fenêtre (barre moins les boutons).
					bool IsInDragZone(const math::NkVec2f &p) const noexcept {
						return NkGemHudButtons::Contains(bar, p) && !NkGemHudButtons::Contains(close, p) &&
							   !NkGemHudButtons::Contains(maximize, p) && !NkGemHudButtons::Contains(minimize, p);
					}
			};

			/// @brief Hauteur conseillée de la barre de titre pour cette surface.
			float32 NkGemTitleBarHeight(float32 uiScale);

			/// @brief Dessine la barre et rend ses zones cliquables.
			NkGemTitleBar NkGemDrawTitleBar(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts, float32 width,
											float32 uiScale, const char *title, bool maximized,
											const math::NkVec2f &pointer);

			// =============================================================
			// Menu principal
			// =============================================================
			struct NkGemMenuButtons {
					NkRect play{0.f, 0.f, 0.f, 0.f};	 ///< aventure (reprend au niveau courant)
					NkRect quickPlay{0.f, 0.f, 0.f, 0.f}; ///< partie libre en mode chrono
					NkRect sound{0.f, 0.f, 0.f, 0.f};	 ///< bascule son
					NkRect reset{0.f, 0.f, 0.f, 0.f};	 ///< efface la progression
			};

			NkGemMenuButtons NkGemDrawMainMenu(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts,
											   const NkGemLayout &layout, const NkGemProgress &progress, float32 time,
											   const math::NkVec2f &pointer, bool muted);

			// =============================================================
			// Carte d'aventure — chemin sinueux de nœuds numérotés
			// =============================================================
			struct NkGemMapGeometry {
					NkRect viewport{0.f, 0.f, 0.f, 0.f};
					float32 scroll = 0.f;	  ///< défilement courant (pixels)
					float32 spacing = 96.f;	  ///< distance verticale entre deux nœuds
					float32 nodeRadius = 26.f;
					float32 amplitude = 70.f; ///< largeur du serpentin
					int32 levelCount = NK_GEM_LEVEL_COUNT;

					static NkGemMapGeometry Compute(const NkGemLayout &layout, float32 scroll);

					math::NkVec2f NodeCenter(int32 levelIndex) const noexcept;

					/// @brief Défilement maximal (0 = haut de la carte).
					float32 MaxScroll() const noexcept;

					/// @brief Niveau sous le point, ou -1. MÊME géométrie que le dessin.
					int32 HitTest(const math::NkVec2f &point) const noexcept;

					/// @brief Défilement qui centre ce niveau — sert à l'ouverture,
					///        pour que le joueur voie tout de suite où il en est.
					float32 ScrollToCenter(int32 levelIndex) const noexcept;
			};

			struct NkGemMapButtons {
					NkRect back{0.f, 0.f, 0.f, 0.f};
			};

			NkGemMapButtons NkGemDrawAdventureMap(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts,
												  const NkGemLayout &layout, const NkGemMapGeometry &geometry,
												  const NkGemProgress &progress, float32 time,
												  const math::NkVec2f &pointer);

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMSCREENS_H
