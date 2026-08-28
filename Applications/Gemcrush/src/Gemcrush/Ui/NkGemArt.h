// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemArt.h
// DESCRIPTION: Dessin d'UNE gemme, style « jeu vidéo » — volume, facettes,
//              reflet spéculaire, ombre portée, halo de sélection et marqueurs
//              de gemme spéciale.
//
//              POURQUOI UNE COUCHE SÉPARÉE : NkGem porte la LOGIQUE (couleur,
//              état, animation) ; NkGemArt porte l'APPARENCE. Redessiner le
//              jeu (thème, saison, mode daltonien) ne touche donc jamais aux
//              règles, et tester les règles ne demande aucun pixel.
//
//              Tout passe par NkGuiDrawList (NKGui) : c'est la liste de dessin
//              du moteur, déjà rendue par NkGuiCanvasBackend sur les 5 dorsales
//              (OpenGL, Vulkan, DX11, DX12, logiciel) et donc sur PC comme sur
//              Android / iOS / HarmonyOS. Aucune primitive n'est réécrite ici.
//
//              POINT D'EXTENSION : une nouvelle silhouette s'ajoute dans
//              BuildOutline() (NkGemArt.cpp) ; un nouveau marqueur de gemme
//              spéciale dans DrawSpecialOverlay().
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMART_H
#define NKENTSEU_GAME_UI_NKGEMART_H

#include "NKGui/Core/NkGuiDrawList.h"
#include "Gemcrush/NkGem.h"
#include "Gemcrush/Ui/NkGemTheme.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			/// @brief Tout ce dont le dessin a besoin — aucune référence à NkGem,
			///        pour que l'art reste utilisable seul (icônes du HUD, écran
			///        de fin de partie, aperçu d'objectif...).
			struct NkGemVisual {
					math::NkVec2f center{0.f, 0.f};
					float32 size = 48.f; ///< diamètre visuel (pas la taille de case)
					NkGemColor color = NkGemColor::NK_GEM_COLOR_RED;
					NkGemShape shape = NkGemShape::NK_GEM_SHAPE_CIRCLE;
					NkGemSpecialKind special = NkGemSpecialKind::NK_GEM_SPECIAL_NONE;

					float32 scale = 1.f; ///< animation (apparition / disparition)
					float32 alpha = 1.f; ///< fondu
					float32 lift = 0.f;  ///< 0..1 : gemme « soulevée » (sélectionnée / glissée)

					bool selected = false;
					bool hovered = false;
					bool shadow = true; ///< false pour une icône posée dans le HUD

					float32 time = 0.f; ///< horloge du jeu, pour les pulsations
			};

			// =============================================================
			// NkGemArt — sans état, tout en fonctions statiques.
			// =============================================================
			class NkGemArt {
				public:
					/// @brief Dessine la gemme complète (ombre, corps, facettes,
					///        reflet, liseré, marqueur spécial, halo).
					static void Draw(nkgui::NkGuiDrawList &dl, const NkGemVisual &v);

					/// @brief Version courte pour le HUD : une gemme posée à plat,
					///        sans ombre ni halo (chips d'objectif, légende).
					static void DrawIcon(nkgui::NkGuiDrawList &dl, NkGemColor color, const math::NkVec2f &center,
										 float32 size);

					/// @brief Éclat d'étoile (récompense, gain de score, explosion).
					///        Utilisé aussi par le HUD pour les étoiles de progression.
					static void DrawSparkle(nkgui::NkGuiDrawList &dl, const math::NkVec2f &center, float32 radius,
											const NkColor &color, float32 rotation = 0.f);

					/// @brief Nombre maximal de sommets qu'une silhouette peut avoir
					///        (étoile à 5 branches = 10 ; cercle échantillonné = 24).
					static constexpr int32 kMaxOutlinePoints = 32;

				private:
					/// @brief Remplit `outPoints` avec la silhouette demandée et rend
					///        le nombre de sommets écrits.
					static int32 BuildOutline(NkGemShape shape, const math::NkVec2f &center, float32 radius,
											  math::NkVec2f *outPoints);

					/// @param center centre EFFECTIF (celui du corps, décalage de
					///        « soulèvement » compris) — sans quoi le marqueur se
					///        décollerait de la gemme pendant un glissé.
					static void DrawSpecialOverlay(nkgui::NkGuiDrawList &dl, const NkGemVisual &v,
												   const math::NkVec2f &center, float32 radius, float32 alpha);
			};

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMART_H
