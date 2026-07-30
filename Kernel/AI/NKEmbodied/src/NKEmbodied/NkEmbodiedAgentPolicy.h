// =============================================================================
// NKEmbodied/NkEmbodiedAgentPolicy.h — pont vers un cerveau NKAgent (NKAI, Phase 6, Jalon 1).
//
// La décision du Jalon 1 ("boucle perception -> décision (NKAgent) -> action")
// : ENVELOPPE un agent::NkAgent déjà construit (et, typiquement, déjà
// ENTRAÎNÉ sur le même monde-grille que le corps, cf. NKAgentTest) sous
// l'interface générique NkEmbodiedPolicy. NE RÉIMPLÉMENTE RIEN — délègue
// intégralement à la politique Q-learning tabulaire de NKAgent (elle-même une
// enveloppe de rl::NkQLearning, cf. NKAgent/NkAgentPolicy.h) : NKEmbodied
// relie, il n'apprend pas. `rawState` (fourni par la boucle, cf.
// NkEmbodiedLoop.h) est exactement l'état que la table Q de NKAgent attend ;
// `observations` porte la MÊME information sous forme de features (même
// encodage que le capteur, cf. NkEmbodiedGridSensor qui réutilise
// agent::NkAgentPerception) — ignorée ici car la table Q tabulaire indexe
// directement par état brut, mais gardée dans la signature pour rester
// interchangeable avec NkEmbodiedHeuristicPolicy (qui, elle, ne lit QUE les
// observations).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKAgent/NkAgent.h"
#include "NKEmbodied/NkEmbodiedPolicy.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedAgentPolicy : public NkEmbodiedPolicy {
				public:
					// `agent` non possédé : sa durée de vie (et son entraînement) sont
					// gérés par l'appelant. `greedy=true` -> exploitation pure
					// (SelectGreedy, aucune exploration) ; `greedy=false` -> ε-greedy
					// (SelectAction) si l'appelant souhaite continuer d'explorer/
					// apprendre pendant que le corps agit (hors scope de la preuve du
					// Jalon 1, mais la politique le permet sans changement de code).
					explicit NkEmbodiedAgentPolicy(agent::NkAgent &agent, bool greedy = true)
						: mAgent(&agent), mGreedy(greedy) {
					}

					uint32 Decide(const NkVector<float> &observations, uint32 rawState) override {
						(void)observations; // la table Q tabulaire indexe par etat brut, pas par features
						return mGreedy ? mAgent->Policy().SelectGreedy(rawState) : mAgent->Policy().SelectAction(rawState);
					}

				private:
					agent::NkAgent *mAgent;
					bool mGreedy;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
