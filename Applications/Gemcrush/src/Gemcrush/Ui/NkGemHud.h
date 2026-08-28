// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemHud.h
// DESCRIPTION: Mise en page RESPONSIVE et interface de jeu de GemCrush —
//              fond, cadre du plateau, barre de score, objectifs, boosters,
//              écrans de pause et de fin de partie.
//
//              DEUX ORIENTATIONS, UN SEUL CODE : NkGemLayout::Compute() décide
//              seule où vont les zones, à partir de la taille réelle de la
//              surface ET de la ZONE SÛRE demandée à la plateforme. Le reste
//              du jeu ne connaît que des rectangles déjà calculés.
//
//              CE QUI VA JUSQU'AU BORD ET CE QUI N'Y VA PAS :
//                - le FOND traverse l'écran entier (encoche comprise) ;
//                - tout ce qui se LIT ou se TOUCHE reste dans la zone sûre.
//              C'est la règle mobile du dépôt, appliquée ici littéralement.
//
//              UN BOUTON DESSINÉ EST UN BOUTON QUI RÉPOND : chaque bouton rend
//              son rectangle dans NkGemHudButtons, et l'application le teste.
//              Aucun bouton décoratif n'est dessiné.
//
// AUTEUR: Rihen
// DATE: 2026-08-27
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMHUD_H
#define NKENTSEU_GAME_UI_NKGEMHUD_H

#include "NKGui/Core/NkGuiDrawList.h"
#include "NKGui/Core/NkGuiFont.h"
#include "NKEvent/NkSafeArea.h"
#include "Gemcrush/Ui/NkGemTheme.h"
#include "Gemcrush/Ui/NkGemArt.h"
#include "Gemcrush/NkGemLevels.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			using nkgui::NkRect;

			// =============================================================
			// Polices — trois tailles, trois rôles, trois atlas DISTINCTS.
			//
			// PIÈGE PAYÉ AILLEURS DANS LE DÉPÔT : toute NkGuiFont porte le
			// même texId par défaut ('NKFT'). Deux polices chargées sans
			// changer ce texId partagent le même atlas côté backend et
			// s'écrasent mutuellement. main.cpp attribue donc texId + 1, + 2.
			// =============================================================
			struct NkGemFonts {
					nkgui::NkGuiFont *body = nullptr;  ///< libellés, compteurs
					nkgui::NkGuiFont *title = nullptr; ///< score, titres d'écran
					nkgui::NkGuiFont *small = nullptr; ///< légendes, unités
			};

			// =============================================================
			// Mise en page
			// =============================================================
			struct NkGemLayout {
					float32 width = 0.f;  ///< largeur de la surface, en pixels
					float32 height = 0.f; ///< hauteur de la surface, en pixels
					float32 scale = 1.f;  ///< échelle d'interface (densité d'écran)
					bool landscape = false;

					NkRect safe{0.f, 0.f, 0.f, 0.f};	  ///< zone sûre (hors encoche / barres)
					NkRect topBar{0.f, 0.f, 0.f, 0.f};	  ///< score + objectifs (colonne en paysage)
					NkRect boardPanel{0.f, 0.f, 0.f, 0.f}; ///< cadre décoratif du plateau
					NkRect boardArea{0.f, 0.f, 0.f, 0.f};  ///< intérieur strict du plateau
					NkRect bottomBar{0.f, 0.f, 0.f, 0.f};  ///< boosters

					float32 cellSize = 48.f;
					math::NkVec2f boardOrigin{0.f, 0.f};
					int32 rowCount = 8;
					int32 columnCount = 8;

					/// @brief Calcule TOUTE la mise en page. Seule fonction à
					///        appeler au démarrage et à chaque redimensionnement
					///        ou rotation.
					/// @param density echelle DPI de l ecran (NkWindow::GetDpiScale).
					///        C est ELLE qui dit combien de pixels vaut un doigt ou
					///        une lettre lisible ; le nombre de pixels seul ne le
					///        dit pas — une fenetre PC de 760 px et un telephone de
					///        760 px n ont pas la meme taille physique.
					static NkGemLayout Compute(float32 width, float32 height, const NkSafeAreaInsets &insets,
											   int32 rowCount, int32 columnCount, float32 density = 1.f);

					/// @brief Taille de police conseillée pour cette surface.
					///        Basée sur le PLUS PETIT côté : une rotation ne la
					///        change donc pas, et les atlas ne sont pas rechargés
					///        pour rien.
					static float32 SuggestedBodyFontPx(float32 width, float32 height, float32 density = 1.f);
			};

			// =============================================================
			// État affiché — le HUD ne calcule rien, il montre.
			// =============================================================
			struct NkGemObjective {
					NkGemColor color = NkGemColor::NK_GEM_COLOR_NONE;
					int32 collected = 0;
					int32 goal = 0;
			};

			struct NkGemHudState {
					int32 level = 1;
					NkGemMode mode = NkGemMode::NK_MODE_MOVES;

					// Mode CHRONO. `timeTotal` sert la barre : sans lui, on ne
					// saurait pas quelle FRACTION il reste, seulement combien.
					float32 timeLeft = 0.f;
					float32 timeTotal = 0.f;
					int32 score = 0;		 ///< score réel
					int32 displayedScore = 0; ///< score affiché (rattrape le réel : ça donne du poids au gain)
					int32 targetScore = 2000; ///< seuil de la 3e étoile
					int32 movesLeft = 25;
					int32 movesTotal = 25; ///< sert la fonte des étoiles (part consommée)

					NkGemObjective objectives[3];
					int32 objectiveCount = 0;

					int32 comboCount = 0;	 ///< cascades enchaînées
					float32 comboTimer = 0.f; ///< > 0 : la bannière de combo est visible

					bool paused = false;
					bool gameOver = false;
					bool victory = false;
					bool hintActive = false;
			};

			/// @brief Part de la ressource déjà dépensée (0 = intacte, 1 = épuisée) :
			///        le temps en mode CHRONO, les coups sinon.
			///
			/// ⚠️ UNE SEULE SOURCE, exposée exprès. Le HUD s'en sert pour faire
			/// fondre les étoiles, les règles de partie pour décider combien on en
			/// gagne. Deux calculs auraient divergé, et le symptôme aurait été
			/// « l'écran de fin donne 2 étoiles alors que la barre en montrait 3 ».
			float32 NkGemSpentFraction(const NkGemHudState &state) noexcept;

			/// @brief Tous les objectifs sont-ils atteints ? C'est la CONDITION DE
			///        VICTOIRE, partagée par le HUD et les règles de partie.
			bool NkGemObjectivesComplete(const NkGemHudState &state) noexcept;

			/// @brief La ressource (temps ou coups) doit-elle encore se consommer ?
			///
			/// ⚠️ ELLE SE GÈLE DÈS QUE L'OBJECTIF EST ATTEINT, sans attendre que
			/// le plateau se stabilise. Défaut mesuré le 2026-08-28 : en mode
			/// CHRONO, le temps continuait de couler pendant la cascade qui suit
			/// le coup gagnant. Le joueur atteignait son but, regardait sa
			/// réaction en chaîne, et perdait une étoile EN LA REGARDANT — le
			/// nombre validé ne reflétait plus le temps auquel il avait réussi.
			/// Une cascade assez longue pouvait même vider le chronomètre.
			bool NkGemResourceRuns(const NkGemHudState &state) noexcept;

			// =============================================================
			// Rectangles des boutons rendus par le HUD.
			//
			// Un rectangle de largeur nulle veut dire « ce bouton n'est pas
			// à l'écran en ce moment » — l'application ne doit pas le tester.
			// =============================================================
			struct NkGemHudButtons {
					NkRect pause{0.f, 0.f, 0.f, 0.f};
					NkRect hint{0.f, 0.f, 0.f, 0.f};
					NkRect shuffle{0.f, 0.f, 0.f, 0.f};
					NkRect resume{0.f, 0.f, 0.f, 0.f};
					NkRect restart{0.f, 0.f, 0.f, 0.f};
					NkRect quit{0.f, 0.f, 0.f, 0.f};  ///< retour a la carte
					NkRect home{0.f, 0.f, 0.f, 0.f};  ///< retour a l'accueil
					NkRect next{0.f, 0.f, 0.f, 0.f};  ///< niveau suivant (victoire seulement)

					static bool Contains(const NkRect &r, const math::NkVec2f &p) noexcept {
						return r.w > 0.f && r.h > 0.f && p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y &&
							   p.y <= r.y + r.h;
					}
			};

			// =============================================================
			// NkGemHud — sans état, tout en fonctions statiques.
			// =============================================================
			class NkGemHud {
				public:
					/// @brief Fond de scène : dégradé, lueurs, vignette. Va
					///        jusqu'aux bords physiques (encoche comprise).
					static void DrawBackground(nkgui::NkGuiDrawList &dl, const NkGemLayout &layout, float32 time);

					/// @brief Cadre du plateau + damier des cases.
					/// @param hintCellA/hintCellB cases mises en avant par l'indice
					///        (x = ligne, y = colonne ; {-1,-1} = aucune).
					/// @param targetCell case survolée pendant un glissé.
					static void DrawBoardFrame(nkgui::NkGuiDrawList &dl, const NkGemLayout &layout, float32 time,
											   const math::NkVec2i &hintCellA, const math::NkVec2i &hintCellB,
											   const math::NkVec2i &targetCell);

					/// @brief Barre de score, objectifs, coups restants, boosters.
					/// @return les rectangles cliquables réellement dessinés.
					static NkGemHudButtons DrawHud(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts,
												   const NkGemLayout &layout, const NkGemHudState &state,
												   float32 time, const math::NkVec2f &pointer);

					/// @brief Écran de pause / fin de partie (couche par-dessus).
					/// @return rectangles des boutons de l'écran affiché.
					static NkGemHudButtons DrawOverlay(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts,
													   const NkGemLayout &layout, const NkGemHudState &state,
													   float32 time, const math::NkVec2f &pointer);

					/// @brief Bannière « Combo x3 ! » au centre du plateau.
					static void DrawComboBanner(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts,
												const NkGemLayout &layout, const NkGemHudState &state);
			};

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMHUD_H
