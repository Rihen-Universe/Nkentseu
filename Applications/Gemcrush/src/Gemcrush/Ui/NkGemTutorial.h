// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/Ui/NkGemTutorial.h
// DESCRIPTION: Didacticiel des trois premiers niveaux.
//
//              UN NIVEAU, UNE CHOSE À APPRENDRE :
//                niveau 1 -> le GESTE      (glisser une gemme vers sa voisine)
//                niveau 2 -> le BUT        (l'objectif décide de la victoire)
//                niveau 3 -> les SPÉCIALES (aligner 4 en crée une)
//              Au-delà, plus rien : un didacticiel qui continue après qu'on a
//              compris devient une gêne.
//
//              ⚠️ CHAQUE ÉTAPE SE TERMINE DE DEUX FAÇONS : le joueur fait la
//              chose, OU le temps passe. La seconde n'est pas un détail — une
//              consigne qui attend un geste précis bloque le jeu chez celui qui
//              en fait un autre, et sur mobile il ne peut que fermer
//              l'application. Le banc vérifie cette sortie de secours.
//
//              Le didacticiel ne CHANGE RIEN aux règles : il montre et il
//              commente. Le joueur peut l'ignorer et jouer normalement.
//
// AUTEUR: Rihen
// DATE: 2026-08-28
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_UI_NKGEMTUTORIAL_H
#define NKENTSEU_GAME_UI_NKGEMTUTORIAL_H

#include "Gemcrush/Ui/NkGemHud.h"

namespace nkentseu {
	namespace game {
		namespace ui {

			/// @brief Ce que le joueur vient de faire. Le didacticiel n'observe
			///        pas le plateau : l'application le lui DIT, ce qui garde les
			///        deux indépendants.
			enum class NkGemTutorialEvent : uint8 {
				SwapDone,		 ///< un échange a été joué (valide ou non)
				MatchResolved,	 ///< un alignement a été résolu
				ObjectiveGained, ///< au moins une gemme d'objectif collectée
				SpecialCreated	 ///< une gemme spéciale vient de naître
			};

			class NkGemTutorial {
				public:
					/// @brief Arme le script du niveau. Au-delà du 3e, rien ne
					///        s'affiche et IsActive() rend false immédiatement.
					void Begin(int32 levelIndex);

					/// @brief Signale une action du joueur ; fait avancer l'étape si
					///        c'est celle qu'on attendait.
					void Notify(NkGemTutorialEvent event);

					void Update(float32 deltaTime);

					void Draw(nkgui::NkGuiDrawList &dl, const NkGemFonts &fonts, const NkGemLayout &layout);

					bool IsActive() const noexcept {
						return mActive;
					}

					/// @brief true tant que le niveau 1 n'a pas vu son premier
					///        échange : l'indice reste allumé en permanence, au lieu
					///        d'attendre que le joueur trouve le bouton INDICE qu'on
					///        ne lui a pas encore expliqué.
					bool WantsPermanentHint() const noexcept {
						return mActive && mPermanentHint;
					}

					/// @brief Durée maximale d'une consigne sans action du joueur.
					static constexpr float32 kStepTimeout = 12.f;
					/// @brief Durée du message de félicitation qui clôt une étape.
					static constexpr float32 kPraiseDuration = 2.2f;

				private:
					bool mActive = false;
					bool mPermanentHint = false;
					int32 mLevel = 0;
					int32 mStep = 0;		 ///< 0 = consigne, 1 = félicitation, 2 = fini
					float32 mTime = 0.f;
					float32 mPulse = 0.f;

					const char *Title() const noexcept;
					const char *Body() const noexcept;
					NkGemTutorialEvent AwaitedEvent() const noexcept;
					void NextStep();
			};

		} // namespace ui
	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_UI_NKGEMTUTORIAL_H
