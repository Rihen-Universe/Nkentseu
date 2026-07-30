// =============================================================================
// NkReach2DMulti.h — variante MULTI-AGENT de NkReach2D (NKAI, Phase 4, Jalon 3 :
// preuve multi-agent minimale).
//
// PLUSIEURS agents-points évoluent dans le MÊME monde carré, chacun avec sa
// propre cible, et sont avancés SIMULTANÉMENT par un seul appel Step() (toutes
// les actions du pas de temps sont appliquées avant que quiconque ne calcule
// sa récompense) — c'est ce qui rend les agents réellement CO-PRÉSENTS dans un
// monde partagé plutôt que N copies indépendantes de NkReach2D exécutées en
// parallèle sans interaction. Le seul couplage inter-agents implémenté est une
// pénalité de COLLISION (distance < collisionRadius entre deux agents = malus
// de récompense pour les deux) — donc "ne pas se percuter" est un objectif
// réel, mais AUCUNE coordination/communication n'existe entre les politiques :
// chaque agent a sa PROPRE politique PPO (NkPPO), entraînée indépendamment sur
// ses propres transitions. Ce n'est PAS du MARL coopératif/compétitif avancé
// (pas de récompense partagée, pas d'observation des actions d'autrui, pas de
// centralised training) — cf ROADMAP.md pour l'honnêteté sur le niveau atteint.
//
// Un agent qui a atteint sa cible est GELÉ (position figée, récompense nulle,
// `done` reste vrai) pour le reste de l'épisode à horizon FIXE : ceci garde
// Step() synchrone entre agents (tous avancent ensemble jusqu'à maxSteps) sans
// pour autant fausser la mesure de succès individuelle (mesurée dès l'instant
// où l'agent atteint sa cible, pas seulement à la fin de l'épisode). Namespace
// : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			class NkReach2DMulti {
				public:
					NkReach2DMulti(uint32 numAgents, float worldSize = 10.0f, float maxStep = 1.0f,
								   float reachRadius = 0.5f, float stepCost = 0.01f, float distScale = 0.05f,
								   float collisionRadius = 0.6f, float collisionPenalty = 0.5f, uint32 seed = 1u);

					uint32 NumAgents() const {
						return mNumAgents;
					}

					float WorldSize() const {
						return mWorldSize;
					}

					// Observation d'un agent : (x, y, tx, ty, [dx_j, dy_j pour chaque AUTRE agent j]).
					// StateDim = 4 + 2*(NumAgents()-1) — permet à l'agent d'anticiper les autres
					// positions (nécessaire pour espérer éviter les collisions), sans coordination
					// explicite (chaque agent traite ces dimensions comme une simple observation).
					uint32 StateDim() const {
						return 4 + 2 * (mNumAgents > 0 ? mNumAgents - 1 : 0);
					}

					uint32 ActionDim() const {
						return 2;
					}

					float ActionLow(uint32) const {
						return -mMaxStep;
					}

					float ActionHigh(uint32) const {
						return mMaxStep;
					}

					// Réinitialise TOUS les agents (positions + cibles tirées au hasard, distinctes,
					// non-collisionnantes au départ). `states[i]` = observation initiale de l'agent i.
					void Reset(NkVector<NkVector<float>> &states);

					// Avance TOUS les agents d'un pas SIMULTANÉMENT : `actions[i]` = action voulue de
					// l'agent i (ignorée si l'agent i est déjà `done`, cf en-tête). Remplit
					// `nextStates[i]`, `rewards[i]`, `dones[i]`.
					void Step(const NkVector<NkVector<float>> &actions, NkVector<NkVector<float>> &nextStates,
							  NkVector<float> &rewards, NkVector<bool> &dones);

					float DistanceToTarget(uint32 agent) const;
					bool IsDone(uint32 agent) const {
						return agent < mDone.Size() ? mDone[agent] : true;
					}

				private:
					float Rand01();
					void BuildObservation(uint32 agent, NkVector<float> &out) const;

					uint32 mNumAgents;
					float mWorldSize;
					float mMaxStep;
					float mReachRadius;
					float mStepCost;
					float mDistScale;
					float mCollisionRadius;
					float mCollisionPenalty;
					uint32 mRng;

					NkVector<float> mX, mY;   // positions courantes, une par agent
					NkVector<float> mTx, mTy; // cibles courantes, une par agent
					NkVector<bool> mDone;	  // agent déjà arrivé (gelé) ?
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
