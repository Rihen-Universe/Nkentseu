// =============================================================================
// NKEmbodied/NkEmbodiedMoveActuator.h — actionneur de déplacement (NKAI, Phase 6, Jalon 1).
//
// Implémentation CONCRÈTE de NkActuator pour un NkEmbodiedBody (corps
// grille) : reçoit un vecteur d'actions et le traduit en UN déplacement
// discret (0=haut 1=bas 2=gauche 3=droite, convention rl::NkGridWorld)
// appliqué au corps commandé. Deux encodages acceptés (cf. NkActuator.h) :
//   • Dim()==1  : `actions[0]` = indice d'action discrète déjà choisi
//                 (arrondi + saturé dans [0, NumActions()-1]) — cas d'une
//                 politique qui décide déjà en discret (ex. table Q).
//   • Dim()==N  : `actions` = N scores/logits, un par action (ex. sortie
//                 continue d'une politique) — argmax = action appliquée.
// Ne connaît ni le capteur en face ni la politique qui a produit `actions` :
// Apply() se contente de traduire et de commander le corps.
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKEmbodied/NkActuator.h"
#include "NKEmbodied/NkEmbodiedBody.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedMoveActuator : public NkActuator {
				public:
					// `body` non possédé : sa durée de vie est gérée par l'appelant.
					explicit NkEmbodiedMoveActuator(NkEmbodiedBody &body) : mBody(&body) {
					}

					uint32 Dim() const override {
						return mBody->NumActions();
					}

					void Apply(const NkVector<float> &actions) override {
						uint32 action = 0;
						if (actions.Size() == 1) {
							// Encodage discret direct : un seul flottant = indice d'action.
							float a = actions[0];
							if (a < 0.0f)
								a = 0.0f;
							action = (uint32)(a + 0.5f);
						} else if (actions.Size() > 1) {
							// Encodage scores/logits : argmax = action choisie.
							float best = actions[0];
							for (nk_size i = 1; i < actions.Size(); ++i) {
								if (actions[i] > best) {
									best = actions[i];
									action = (uint32)i;
								}
							}
						}
						const uint32 maxAction = mBody->NumActions() ? mBody->NumActions() - 1 : 0;
						if (action > maxAction)
							action = maxAction;
						mBody->ApplyAction(action);
					}

				private:
					NkEmbodiedBody *mBody;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
