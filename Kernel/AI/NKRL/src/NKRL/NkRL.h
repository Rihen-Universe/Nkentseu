// =============================================================================
// NkRL.h — apprentissage par renforcement (NKAI, Phase 4) — header PARAPLUIE.
//
// Regroupe les concepts du module :
//   • NkEnv.h              — interface d'environnement (états/actions DISCRETS)
//   • NkGridWorld.h        — monde-grille jouet
//   • NkKeyDoorGridWorld.h — monde-grille à deux étapes (clé PUIS porte)
//   • NkQLearning.h        — agent Q-learning tabulaire (Jalon 1)
//   • NkReplayBuffer.h     — mémoire de rejeu bornée (Jalon 2)
//   • NkDQN.h              — Deep Q-Network : réseau + cible + replay (Jalon 2)
//   • NkContinuousEnv.h    — interface d'environnement à ESPACES CONTINUS (Jalon 3)
//   • NkReach2D.h          — monde à actions continues, mono-agent (Jalon 3)
//   • NkReach2DMulti.h     — variante multi-agent (preuve minimale, Jalon 3)
//   • NkPolicyNet.h        — réseau de politique discret/gaussien (Jalon 3)
//   • NkPPO.h              — PPO : rollout + GAE + objectif clippé (Jalon 3)
// + les helpers RunEpisode / RunDQNEpisode / RunPPOEpisodeDiscrete /
// RunPPOEpisodeContinuous (déroulent un épisode, avec ou sans apprentissage).
// Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKRL/NkEnv.h"
#include "NKRL/NkGridWorld.h"
#include "NKRL/NkKeyDoorGridWorld.h"
#include "NKRL/NkQLearning.h"
#include "NKRL/NkReplayBuffer.h"
#include "NKRL/NkDQN.h"
#include "NKRL/NkContinuousEnv.h"
#include "NKRL/NkReach2D.h"
#include "NKRL/NkReach2DMulti.h"
#include "NKRL/NkPolicyNet.h"
#include "NKRL/NkPPO.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			struct EpisodeResult {
					float reward = 0.0f;	  // récompense cumulée
					bool reachedGoal = false; // le but a-t-il été atteint ?
					uint32 steps = 0;
			};

			// Déroule un épisode complet. `learn=true` : ε-greedy + mise à jour Q ;
			// `learn=false` : politique gloutonne, sans apprentissage (évaluation).
			inline EpisodeResult RunEpisode(NkEnvironment &env, NkQLearning &agent, bool learn, uint32 maxSteps) {
				EpisodeResult res;
				uint32 s = env.Reset();
				bool done = false;
				float lastR = 0.0f;
				for (; res.steps < maxSteps && !done; ++res.steps) {
					uint32 a = learn ? agent.SelectAction(s) : agent.SelectGreedy(s);
					uint32 sNext;
					float r;
					env.Step(a, sNext, r, done);
					if (learn)
						agent.Update(s, a, r, sNext, done);
					res.reward += r;
					lastR = r;
					s = sNext;
				}
				res.reachedGoal = done && (lastR > 0.5f); // récompense +1 = but
				return res;
			}

			// Résultat d'un épisode joué avec un NkPPO (même convention que rl::EpisodeResult).
			struct NkPPOEpisodeResult {
					float reward = 0.0f;
					bool reachedGoal = false;
					uint32 steps = 0;
			};

			// Déroule un épisode complet PPO sur un environnement DISCRET (rl::NkEnvironment),
			// en encodant l'état en ONE-HOT (même convention que rl::NkDQN, cf NkDQN.h) — permet
			// de valider le mode NkPolicyMode::Discrete de NkPolicyNet/NkPPO sur un environnement
			// déjà existant (rl::NkGridWorld) sans dupliquer sa logique. `learn=true` :
			// échantillonnage stochastique + mémorisation + mise à jour PPO dès que le rollout est
			// plein OU l'épisode terminé ; `learn=false` : politique gloutonne pure (évaluation).
			inline NkPPOEpisodeResult RunPPOEpisodeDiscrete(NkEnvironment &env, NkPPO &agent, bool learn,
															uint32 maxSteps) {
				NkPPOEpisodeResult res;
				uint32 s = env.Reset();
				bool done = false;
				float lastR = 0.0f;
				const uint32 numStates = env.NumStates();
				for (; res.steps < maxSteps && !done; ++res.steps) {
					NkVector<float> obs;
					obs.Resize(numStates);
					for (uint32 i = 0; i < numStates; ++i)
						obs[i] = (i == s) ? 1.0f : 0.0f;

					NkVector<float> actionVec;
					float logp = 0.0f, value = 0.0f;
					if (learn)
						agent.SelectAction(obs, actionVec, logp, value);
					else
						agent.GreedyAction(obs, actionVec);
					uint32 a = (actionVec.Size() > 0) ? (uint32)(actionVec[0] + 0.5f) : 0;
					if (a >= env.NumActions())
						a = env.NumActions() - 1;

					uint32 sNext;
					float r;
					env.Step(a, sNext, r, done);
					res.reward += r;
					lastR = r;

					if (learn) {
						NkVector<float> nextObs;
						nextObs.Resize(numStates);
						for (uint32 i = 0; i < numStates; ++i)
							nextObs[i] = (i == sNext) ? 1.0f : 0.0f;
						agent.Remember(obs, actionVec, r, done, logp, value);
						agent.TrainStepIfReady(nextObs, done);
					}
					s = sNext;
				}
				res.reachedGoal = done && (lastR > 0.5f);
				return res;
			}

			// Déroule un épisode complet PPO sur un environnement CONTINU (rl::NkContinuousEnv,
			// ex. rl::NkReach2D) — mode NkPolicyMode::ContinuousGaussian.
			inline NkPPOEpisodeResult RunPPOEpisodeContinuous(NkContinuousEnv &env, NkPPO &agent, bool learn,
															  uint32 maxSteps) {
				NkPPOEpisodeResult res;
				NkVector<float> s;
				env.Reset(s);
				bool done = false;
				float lastR = 0.0f;
				for (; res.steps < maxSteps && !done; ++res.steps) {
					NkVector<float> actionVec;
					float logp = 0.0f, value = 0.0f;
					if (learn)
						agent.SelectAction(s, actionVec, logp, value);
					else
						agent.GreedyAction(s, actionVec);

					NkVector<float> sNext;
					float r;
					env.Step(actionVec, sNext, r, done);
					res.reward += r;
					lastR = r;

					if (learn) {
						agent.Remember(s, actionVec, r, done, logp, value);
						agent.TrainStepIfReady(sNext, done);
					}
					s = sNext;
				}
				res.reachedGoal = done && (lastR > 0.5f);
				return res;
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
