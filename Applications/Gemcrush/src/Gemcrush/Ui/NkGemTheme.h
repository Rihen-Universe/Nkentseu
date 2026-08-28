// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemTheme.h
// DESCRIPTION: Jetons de design de GemCrush — LA source unique des couleurs,
//              rayons, épaisseurs et cibles tactiles de l'interface.
//
//              POURQUOI CE FICHIER EXISTE : aucune couleur ni aucun rayon ne
//              doit être écrit en dur ailleurs. Changer l'ambiance du jeu
//              (thème, événement saisonnier, mode daltonien) se fait ICI et
//              nulle part ailleurs.
//
//              POINT D'EXTENSION : pour ajouter une 7e gemme, ajouter sa valeur
//              à NkGemColor (NkGem.h) puis SA LIGNE dans NkGemPalette() et
//              NkGemShapeForColor() ci-dessous. Rien d'autre à toucher :
//              l'art et le HUD lisent tout depuis ces deux fonctions.
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMTHEME_H
#define NKENTSEU_GAME_UI_NKGEMTHEME_H

#include "NKMath/NKMath.h"
#include "NKMath/NkColor.h"
#include "Gemcrush/NkGem.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using math::NkColor;

			// =============================================================
			// Palette d'une gemme — quatre teintes, pas une seule.
			//
			// Une gemme plate (un seul remplissage) ressemble à un pion de
			// prototype. Une gemme TAILLÉE demande au minimum :
			//   core  : le cœur, là où la lumière ressort           (clair)
			//   mid   : la teinte d'identité, celle qu'on nomme      (saturée)
			//   deep  : l'ombre du volume et le liseré de contour    (sombre)
			//   glow  : le halo de sélection / d'explosion (additif) (vif)
			// =============================================================
			struct NkGemPaletteEntry {
					NkColor core;
					NkColor mid;
					NkColor deep;
					NkColor glow;
			};

			/// @brief Palette de la couleur logique demandée.
			inline NkGemPaletteEntry NkGemPalette(NkGemColor color) noexcept {
				switch (color) {
					case NkGemColor::NK_GEM_COLOR_RED: // rubis
						return {NkColor(255, 154, 154), NkColor(226, 38, 75), NkColor(120, 12, 42),
								NkColor(255, 77, 109)};
					case NkGemColor::NK_GEM_COLOR_GREEN: // émeraude
						return {NkColor(160, 250, 200), NkColor(34, 197, 94), NkColor(10, 88, 46),
								NkColor(74, 222, 128)};
					case NkGemColor::NK_GEM_COLOR_BLUE: // saphir
						return {NkColor(160, 208, 255), NkColor(46, 123, 232), NkColor(16, 52, 116),
								NkColor(96, 165, 250)};
					case NkGemColor::NK_GEM_COLOR_YELLOW: // citrine
						return {NkColor(255, 245, 180), NkColor(245, 197, 24), NkColor(133, 90, 0),
								NkColor(255, 216, 77)};
					case NkGemColor::NK_GEM_COLOR_PURPLE: // améthyste
						return {NkColor(226, 190, 255), NkColor(155, 77, 224), NkColor(70, 26, 132),
								NkColor(192, 132, 252)};
					case NkGemColor::NK_GEM_COLOR_ORANGE: // ambre
						return {NkColor(255, 205, 155), NkColor(249, 115, 22), NkColor(126, 48, 6),
								NkColor(251, 146, 60)};
					default:
						return {NkColor(220, 220, 230), NkColor(150, 150, 165), NkColor(70, 70, 85),
								NkColor(200, 200, 220)};
				}
			}

			/// @brief Forme attribuée à chaque couleur.
			///
			/// RÈGLE D'ACCESSIBILITÉ, et c'est une décision de conception, pas
			/// une préférence : un joueur daltonien ne distingue pas le rouge
			/// du vert. La SILHOUETTE porte donc la même information que la
			/// teinte — deux couleurs n'ont jamais la même forme. C'est ce que
			/// font Bejeweled et Candy Crush.
			inline NkGemShape NkGemShapeForColor(NkGemColor color) noexcept {
				switch (color) {
					case NkGemColor::NK_GEM_COLOR_RED:
						return NkGemShape::NK_GEM_SHAPE_DIAMOND; // taille losange
					case NkGemColor::NK_GEM_COLOR_GREEN:
						return NkGemShape::NK_GEM_SHAPE_HEXAGON; // taille émeraude
					case NkGemColor::NK_GEM_COLOR_BLUE:
						return NkGemShape::NK_GEM_SHAPE_CIRCLE; // taille brillant
					case NkGemColor::NK_GEM_COLOR_YELLOW:
						return NkGemShape::NK_GEM_SHAPE_STAR; // taille étoile
					case NkGemColor::NK_GEM_COLOR_PURPLE:
						return NkGemShape::NK_GEM_SHAPE_TRIANGLE; // taille trilliant
					case NkGemColor::NK_GEM_COLOR_ORANGE:
						return NkGemShape::NK_GEM_SHAPE_SQUARE; // taille coussin
					default:
						return NkGemShape::NK_GEM_SHAPE_CIRCLE;
				}
			}

			// =============================================================
			// Jetons d'interface
			// =============================================================
			struct NkGemTheme {
					// -- Fond de scène ------------------------------------
					NkColor backdropTop{18, 15, 45, 255};	 ///< haut du dégradé de fond
					NkColor backdropBottom{44, 24, 82, 255}; ///< bas du dégradé de fond
					NkColor vignette{6, 4, 18, 150};		 ///< assombrissement des bords

					// -- Panneaux (HUD, plateau, modales) -----------------
					NkColor panel{29, 25, 62, 235};		   ///< remplissage de panneau
					NkColor panelTop{47, 40, 92, 235};	   ///< haut du dégradé de panneau
					NkColor panelBorder{92, 78, 158, 255}; ///< liseré extérieur
					NkColor panelSheen{255, 255, 255, 38}; ///< filet clair du bord haut
					NkColor shadow{0, 0, 0, 90};		   ///< ombre portée

					// -- Cases du plateau (damier) ------------------------
					NkColor cellEven{38, 32, 76, 255};
					NkColor cellOdd{45, 38, 88, 255};
					NkColor cellHint{255, 194, 75, 70};	   ///< case désignée par l'indice
					NkColor cellTarget{255, 255, 255, 40}; ///< case visée pendant un glissé

					// -- Texte --------------------------------------------
					NkColor textPrimary{244, 241, 255, 255};
					NkColor textSecondary{170, 160, 214, 255};
					NkColor textOnAccent{40, 24, 6, 255};

					// -- Accents ------------------------------------------
					NkColor accent{255, 194, 75, 255};	   ///< or : score, étoiles, focus
					NkColor accentDeep{201, 134, 20, 255}; ///< or sombre : socle des boutons
					NkColor accentCool{91, 225, 255, 255}; ///< cyan : coups restants, indice
					NkColor danger{255, 92, 108, 255};	   ///< rouge : derniers coups

					// -- Géométrie ----------------------------------------
					float32 radiusPanel = 22.f;	 ///< rayon des grands panneaux
					float32 radiusChip = 14.f;	 ///< rayon des pastilles / boutons
					float32 radiusCell = 8.f;	 ///< rayon des cases du damier
					float32 borderWidth = 2.f;	 ///< épaisseur de liseré
					float32 gemInset = 0.86f;	 ///< part de la case occupée par la gemme

					// -- Cible tactile minimale ---------------------------
					// 44 points : plancher recommandé par les guides iOS ET
					// Android (48 dp). En dessous, un doigt rate le bouton.
					float32 minTouchTarget = 44.f;
			};

			/// @brief Thème courant (le jeu n'en a qu'un).
			inline const NkGemTheme &NkTheme() noexcept {
				static const NkGemTheme kTheme;
				return kTheme;
			}

			// =============================================================
			// Aides de couleur (aucune dépendance, tout inline)
			// =============================================================
			inline NkColor NkWithAlpha(const NkColor &c, float32 alpha01) noexcept {
				NkColor out = c;
				out.a = static_cast<uint8>(math::NkClamp(alpha01, 0.f, 1.f) * static_cast<float32>(c.a));
				return out;
			}

			inline NkColor NkLerpColor(const NkColor &a, const NkColor &b, float32 t) noexcept {
				const float32 k = math::NkClamp(t, 0.f, 1.f);
				const float32 r = static_cast<float32>(a.r) + (static_cast<float32>(b.r) - static_cast<float32>(a.r)) * k;
				const float32 g = static_cast<float32>(a.g) + (static_cast<float32>(b.g) - static_cast<float32>(a.g)) * k;
				const float32 b2 = static_cast<float32>(a.b) + (static_cast<float32>(b.b) - static_cast<float32>(a.b)) * k;
				const float32 al = static_cast<float32>(a.a) + (static_cast<float32>(b.a) - static_cast<float32>(a.a)) * k;
				return NkColor(static_cast<uint8>(r), static_cast<uint8>(g), static_cast<uint8>(b2),
							   static_cast<uint8>(al));
			}

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMTHEME_H
