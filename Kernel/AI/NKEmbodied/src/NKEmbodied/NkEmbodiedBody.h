// =============================================================================
// NKEmbodied/NkEmbodiedBody.h — corps SIMULÉ minimal (NKAI, Phase 6, Jalon 1).
//
// Le corps que le Jalon 1 exige ("boucle perception -> décision -> action,
// dans un corps simulé"). Réutilise le monde-grille déjà livré/prouvé de
// NKRL (rl::NkGridWorld, cf. NKAgentTest/NKRLTest : 7.9% -> 100% de réussite)
// comme substrat PHYSIQUE : position sur une grille N×N, un but, des trous.
// N'AJOUTE aucune nouvelle physique — NkEmbodiedBody est une fine couche
// "corps" au-dessus (état courant, compteurs de tick, dernière récompense/
// terminaison), du même ordre de responsabilité que rl::NkGridWorld lui-même
// mais VU comme un corps qu'on perçoit via des capteurs (NkSensor) et qu'on
// commande via des actionneurs (NkActuator), pas directement via ses indices
// d'action bruts. Corps réel (via Kernel/Bare) : hors scope Jalon 1 (cf.
// ROADMAP, Jalon 3).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRL/NkGridWorld.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedBody {
				public:
					// `size` = côté de la grille. Trous fournis en indices d'état
					// (y*size+x), même convention que rl::NkGridWorld.
					NkEmbodiedBody(uint32 size, uint32 startState, uint32 goalState, const NkVector<uint32> &holes,
								   float stepCost = 0.02f);

					// Réinitialise le corps (position de départ, compteurs) et renvoie
					// l'état brut initial.
					uint32 Reset();

					// Applique une action DISCRÈTE (0=haut 1=bas 2=gauche 3=droite,
					// convention rl::NkGridWorld) au corps : avance la simulation d'un
					// tick, met à jour position/récompense/terminaison. Appelée par un
					// NkActuator concret (cf. NkEmbodiedMoveActuator) — jamais
					// directement par une politique, qui ne connaît que des capteurs/
					// actionneurs génériques.
					void ApplyAction(uint32 action);

					uint32 State() const {
						return mCurState;
					}

					uint32 GoalState() const {
						return mWorld.GoalState();
					}

					uint32 Size() const {
						return mWorld.Size();
					}

					uint32 NumActions() const {
						return mWorld.NumActions();
					}

					// Dernière récompense reçue (0 avant le premier ApplyAction() suivant Reset()).
					float LastReward() const {
						return mLastReward;
					}

					bool IsDone() const {
						return mDone;
					}

					// Terminé SUR le but (par opposition à terminé dans un trou).
					bool ReachedGoal() const {
						return mDone && mReachedGoal;
					}

					uint32 TickCount() const {
						return mTicks;
					}

					// Accès à la grille sous-jacente (inspection : IsHole(), etc.).
					const rl::NkGridWorld &World() const {
						return mWorld;
					}

				private:
					rl::NkGridWorld mWorld;
					uint32 mCurState;
					float mLastReward;
					bool mDone;
					bool mReachedGoal;
					uint32 mTicks;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
