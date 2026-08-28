// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemSplash.h
// DESCRIPTION: Écrans d'ouverture — marque RIHEN, puis mention du moteur.
//
//              DESSINÉS PAR PRIMITIVES, AUCUNE TEXTURE. C'est le modèle de
//              `Applications/Pong/src/Pong/UI/Scenes/NogeIntroScene.cpp`
//              (« logo dessiné entièrement par primitives »), et non celui de
//              RihenIntroScene, qui lit 72 PNG depuis les assets : GemCrush
//              n'embarque aucun fichier, et c'est ce qui lui permet de démarrer
//              à l'identique sur les six plateformes.
//
//              ⚠️ IL EST TOUJOURS SAUTABLE. Un écran d'ouverture qu'on ne peut
//              pas passer est une taxe payée à chaque lancement, y compris par
//              celui qui teste le jeu vingt fois par heure. Un appui, une
//              touche, un doigt : on passe.
//
//              POINT D'EXTENSION : le jour où un vrai logo Rihen doit
//              apparaître, il se charge en texture et se dessine à la place du
//              mot — la structure de phases ne change pas.
//
// AUTEUR: Rihen
// DATE: 2026-08-28
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMSPLASH_H
#define NKENTSEU_GAME_UI_NKGEMSPLASH_H

#include "Gemcrush/Ui/NkGemHud.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			// =============================================================
			// NkGemSplash — deux volets enchaînés, puis terminé.
			// =============================================================
			class NkGemSplash {
				public:
					/// @brief Avance l'horloge. Rend true quand tout est fini.
					bool Update(float32 deltaTime);

					void Draw(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout);

					bool IsDone() const noexcept {
						return mDone;
					}

					/// @brief Passe au volet suivant, ou termine si c'est le dernier.
					///        Appelé sur n'importe quelle entrée du joueur.
					void Skip();

					// -- Minutage, en un seul endroit -------------------------
					// Volet = fondu entrant, maintien, fondu sortant. Deux volets
					// -> ~4,4 s au total si on ne saute pas. C'est court : au-delà,
					// l'ouverture devient une attente et non une signature.
					static constexpr float32 kFadeIn = 0.45f;
					static constexpr float32 kHold = 1.20f;
					static constexpr float32 kFadeOut = 0.40f;
					static constexpr float32 kPanelDuration = kFadeIn + kHold + kFadeOut;

				private:
					int32 mPanel = 0;	  ///< 0 = Rihen, 1 = moteur
					float32 mTime = 0.f;  ///< temps écoulé DANS le volet courant
					bool mDone = false;

					/// @brief Opacité 0..1 du volet courant (fondu entrant/sortant).
					float32 PanelAlpha() const noexcept;

					void DrawRihen(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
								   float32 alpha);
					void DrawEngine(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout,
									float32 alpha);
			};

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMSPLASH_H
