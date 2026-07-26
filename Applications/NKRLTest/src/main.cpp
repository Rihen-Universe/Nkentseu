// =============================================================================
// NKRLTest — un agent apprend SEUL à résoudre un monde-grille (NKRL, Phase 4).
//
// Test 1 (Jalon 1) : grille 5x5 avec des trous. Q-learning tabulaire +
//   ε-greedy décroissant. On entraîne, puis on évalue la politique gloutonne
//   (doit atteindre le but ~100% du temps) et on affiche la politique
//   apprise. Jalon « ça vit ».
//
// Test 2 (Jalon 2) : DQN (réseau Q + replay buffer + réseau cible, NkDQN) sur
//   le monde CLÉ-PUIS-PORTE (rl::NkKeyDoorGridWorld, chemin optimal = 8 pas :
//   4 pour la clé, 4 pour la porte). Compare, sur plusieurs graines, une
//   politique ALÉATOIRE pure (repère « avant entraînement », aucun
//   apprentissage) à la politique gloutonne du DQN APRÈS entraînement (réseau
//   en ligne entraîné par TD + cible de Bellman calculée via un réseau CIBLE
//   synchronisé périodiquement, mini-lots rejoués depuis un replay buffer
//   borné). Le DQN doit nettement dépasser la politique aléatoire en taux de
//   réussite ET en récompense moyenne.
//
// Test 3 (Jalon 3, sanity discret) : PPO (rl::NkPPO, mode Discrete) sur le
//   MÊME monde-grille que le Jalon 1 -- valide le mode discret avant de
//   passer au vrai objet du Jalon 3 (actions continues, Test 4).
//
// Test 4 (Jalon 3, ACTIONS CONTINUES) : PPO (politique gaussienne + critique +
//   GAE + objectif clippé) sur rl::NkReach2D (point 2D, déplacement continu
//   borné, cible tirée au hasard). AVANT (politique aléatoire continue) vs
//   APRÈS entraînement, sur 5 graines.
//
// Test 5 (Jalon 3, preuve MULTI-AGENT minimale) : 2 agents PPO INDÉPENDANTS
//   (politiques et rollouts séparés) dans le même monde (rl::NkReach2DMulti),
//   couplés uniquement par une pénalité de collision -- PAS de coordination,
//   PAS de MARL avancé (cf NkReach2DMulti.h).
// =============================================================================
#include "NKRL/NkRL.h"
#include "NKContainers/String/NkString.h"
#include "NKLogger/NkLog.h"

#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	// Résultat agrégé (taux de réussite + récompense moyenne) d'un run d'évaluation.
	struct RunStats {
			double successRate = 0.0;
			double avgReward = 0.0;
	};

	// Politique ALÉATOIRE pure (aucun apprentissage, aucun réseau) : action
	// uniforme parmi NumActions() à chaque pas. Le repère « avant entraînement ».
	// RNG local (LCG), indépendant de tout agent, pour un avant/après honnête.
	// Prend l'interface DE BASE (rl::NkEnvironment) -- réutilisée pour Test 2 (DQN,
	// clé-puis-porte) ET Test 3 (PPO discret, monde-grille) sans dupliquer la logique.
	RunStats RunRandomPolicy(rl::NkEnvironment &env, uint32 seed, int episodes, uint32 maxSteps) {
		uint32 rng = seed ? seed : 1u;
		int reached = 0;
		double totalReward = 0.0;
		for (int e = 0; e < episodes; ++e) {
			uint32 s = env.Reset();
			(void)s;
			bool done = false;
			float lastR = 0.0f;
			double epReward = 0.0;
			for (uint32 step = 0; step < maxSteps && !done; ++step) {
				rng = rng * 1664525u + 1013904223u;
				const float u = (float)((rng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
				uint32 a = (uint32)(u * (float)env.NumActions());
				if (a >= env.NumActions())
					a = env.NumActions() - 1;
				uint32 sNext;
				float r;
				env.Step(a, sNext, r, done);
				epReward += (double)r;
				lastR = r;
				s = sNext;
			}
			if (done && lastR > 0.5f)
				++reached;
			totalReward += epReward;
		}
		RunStats res;
		res.successRate = (double)reached / (double)episodes;
		res.avgReward = totalReward / (double)episodes;
		return res;
	}

	// Entraîne un DQN NEUF (graine `seed`) pendant `episodes` (ε décroissant,
	// même schéma que le Jalon 1), puis évalue sa politique gloutonne pure
	// (aucune exploration, aucun apprentissage) sur `evalEpisodes`.
	RunStats TrainAndEvalDQN(uint32 size, uint32 start, uint32 key, uint32 door, const NkVector<uint32> &holes,
							 int episodes, uint32 maxSteps, int evalEpisodes, uint32 seed) {
		rl::NkKeyDoorGridWorld env(size, start, key, door, holes, /*stepCost*/ 0.01f);

		rl::NkDQNConfig cfg;
		cfg.hiddenSize = 32;
		cfg.lr = 0.001f;
		cfg.gamma = 0.99f;
		cfg.replayCapacity = 3000;
		cfg.batchSize = 32;
		cfg.minReplaySize = 200;
		cfg.targetSyncInterval = 200;
		cfg.seed = seed;
		rl::NkDQN agent(env.NumStates(), env.NumActions(), cfg);

		for (int e = 1; e <= episodes; ++e) {
			// ε : 1.0 -> 0.05 sur les 80 premiers % des épisodes (puis exploite).
			float eps = 1.0f - (float)e / (float)(episodes * 0.8);
			if (eps < 0.05f)
				eps = 0.05f;
			agent.SetEpsilon(eps);
			rl::RunDQNEpisode(env, agent, /*learn*/ true, maxSteps);
		}

		int reached = 0;
		double totalReward = 0.0;
		for (int e = 0; e < evalEpisodes; ++e) {
			rl::NkDQNEpisodeResult r = rl::RunDQNEpisode(env, agent, /*learn*/ false, maxSteps);
			reached += r.reachedGoal ? 1 : 0;
			totalReward += (double)r.reward;
		}
		RunStats res;
		res.successRate = (double)reached / (double)evalEpisodes;
		res.avgReward = totalReward / (double)evalEpisodes;
		return res;
	}

	// Construit la ligne "politique apprise" d'une rangée de la grille (flèche = action
	// gloutonne de l'agent). Une seule chaîne -> un seul logger.Info par ligne.
	NkString PolicyRow(rl::NkQLearning &agent, const rl::NkGridWorld &env, uint32 y) {
		static const char *arrow[4] = {"^", "v", "<", ">"};
		const uint32 N = env.Size();
		NkString row;
		for (uint32 x = 0; x < N; ++x) {
			const uint32 s = y * N + x;
			row += ' ';
			if (s == env.GoalState())
				row += 'G';
			else if (env.IsHole(s))
				row += 'O';
			else
				row += arrow[agent.SelectGreedy(s)];
		}
		return row;
	}

	// =========================================================================
	// Test 3 (Jalon 3, sanity DISCRET) : PPO (rl::NkPPO, mode Discrete) sur le
	// MÊME monde-grille que le Jalon 1 (rl::NkGridWorld) -- valide que le mode
	// discret de NkPolicyNet/NkPPO fonctionne réellement (pas seulement le mode
	// continu, qui est la nouveauté testée en profondeur au Test 4).
	// =========================================================================
	RunStats TrainAndEvalPPODiscrete(rl::NkGridWorld &env, int episodes, uint32 maxSteps, int evalEpisodes,
									 uint32 seed) {
		rl::NkPPOConfig cfg;
		cfg.hiddenSize = 32;
		cfg.policyLr = 3e-4f;
		cfg.valueLr = 1e-3f;
		cfg.gamma = 0.99f;
		cfg.gaeLambda = 0.95f;
		cfg.clipEps = 0.2f;
		cfg.epochs = 4;
		cfg.rolloutSize = 64;
		cfg.entropyCoef = 0.01f;
		cfg.seed = seed;
		rl::NkPPO agent(rl::NkPolicyMode::Discrete, env.NumStates(), env.NumActions(), cfg);

		for (int e = 1; e <= episodes; ++e)
			rl::RunPPOEpisodeDiscrete(env, agent, /*learn*/ true, maxSteps);

		int reached = 0;
		double totalReward = 0.0;
		for (int e = 0; e < evalEpisodes; ++e) {
			rl::NkPPOEpisodeResult r = rl::RunPPOEpisodeDiscrete(env, agent, /*learn*/ false, maxSteps);
			reached += r.reachedGoal ? 1 : 0;
			totalReward += (double)r.reward;
		}
		RunStats res;
		res.successRate = (double)reached / (double)evalEpisodes;
		res.avgReward = totalReward / (double)evalEpisodes;
		return res;
	}

	// =========================================================================
	// Test 4 (Jalon 3, ACTIONS CONTINUES) : PPO sur rl::NkReach2D (point 2D,
	// déplacement continu (dx,dy) borné, cible tirée au hasard à chaque épisode).
	// =========================================================================
	RunStats RunRandomPolicyContinuous(rl::NkReach2D &env, uint32 seed, int episodes, uint32 maxSteps) {
		uint32 rng = seed ? seed : 1u;
		int reached = 0;
		double totalReward = 0.0;
		for (int e = 0; e < episodes; ++e) {
			NkVector<float> s;
			env.Reset(s);
			bool done = false;
			float lastR = 0.0f;
			double epReward = 0.0;
			for (uint32 step = 0; step < maxSteps && !done; ++step) {
				rng = rng * 1664525u + 1013904223u;
				const float u1 = (float)((rng >> 8) & 0xFFFFFFu) / (float)0x1000000u;
				rng = rng * 1664525u + 1013904223u;
				const float u2 = (float)((rng >> 8) & 0xFFFFFFu) / (float)0x1000000u;
				NkVector<float> action;
				action.PushBack(u1 * 2.0f - 1.0f); // U[-1,1] -- saturé par l'env aux bornes réelles
				action.PushBack(u2 * 2.0f - 1.0f);
				NkVector<float> sNext;
				float r;
				env.Step(action, sNext, r, done);
				epReward += (double)r;
				lastR = r;
				s = sNext;
			}
			if (done && lastR > 0.5f)
				++reached;
			totalReward += epReward;
		}
		RunStats res;
		res.successRate = (double)reached / (double)episodes;
		res.avgReward = totalReward / (double)episodes;
		return res;
	}

	RunStats TrainAndEvalPPOContinuous(float worldSize, float maxStep, float reachRadius, float stepCost,
									   float distScale, int episodes, uint32 maxSteps, int evalEpisodes,
									   uint32 seed) {
		rl::NkReach2D env(worldSize, maxStep, reachRadius, stepCost, distScale, seed);

		rl::NkPPOConfig cfg;
		cfg.hiddenSize = 64;
		cfg.policyLr = 3e-4f;
		cfg.valueLr = 1e-3f;
		cfg.gamma = 0.99f;
		cfg.gaeLambda = 0.95f;
		cfg.clipEps = 0.2f;
		cfg.epochs = 4;
		cfg.rolloutSize = 128;
		cfg.entropyCoef = 0.01f;
		cfg.initLogStd = -0.5f;
		cfg.actionScale = maxStep;
		cfg.seed = seed;
		rl::NkPPO agent(rl::NkPolicyMode::ContinuousGaussian, env.StateDim(), env.ActionDim(), cfg);

		for (int e = 1; e <= episodes; ++e)
			rl::RunPPOEpisodeContinuous(env, agent, /*learn*/ true, maxSteps);

		int reached = 0;
		double totalReward = 0.0;
		for (int e = 0; e < evalEpisodes; ++e) {
			rl::NkPPOEpisodeResult r = rl::RunPPOEpisodeContinuous(env, agent, /*learn*/ false, maxSteps);
			reached += r.reachedGoal ? 1 : 0;
			totalReward += (double)r.reward;
		}
		RunStats res;
		res.successRate = (double)reached / (double)evalEpisodes;
		res.avgReward = totalReward / (double)evalEpisodes;
		return res;
	}

	// =========================================================================
	// Test 5 (Jalon 3, PREUVE MULTI-AGENT minimale) : 2 agents PPO INDÉPENDANTS
	// (politiques séparées, PAS de politique partagée, PAS de coordination) dans
	// le MÊME monde (rl::NkReach2DMulti), chacun avec sa propre cible, couplés
	// uniquement par une pénalité de collision. Ce n'est PAS du MARL avancé --
	// cf en-tête NkReach2DMulti.h et ROADMAP.md pour l'honnêteté sur le niveau
	// atteint. Le taux de collision (fraction des pas où les 2 agents sont plus
	// proches que collisionRadius) est mesuré via la distance relative déjà
	// présente dans l'observation (nextStates[0][4..5]), sans changer l'API de
	// l'environnement.
	// =========================================================================
	struct MultiAgentStats {
			double successRate0 = 0.0, successRate1 = 0.0;
			double avgReward0 = 0.0, avgReward1 = 0.0;
			double collisionRate = 0.0; // fraction des pas (tous épisodes confondus) en collision
	};

	// `obsAgent0[4..5]` = position relative de l'AUTRE agent, NORMALISÉE par worldSize (cf
	// NkReach2DMulti::BuildObservation) -- on la remultiplie par worldSize pour comparer la
	// distance à `collisionRadius`, qui est exprimé en unités RÉELLES du monde.
	float AgentPairDistance(const NkVector<float> &obsAgent0, float worldSize) {
		const float dx = (obsAgent0.Size() > 4 ? obsAgent0[4] : 0.0f) * worldSize;
		const float dy = (obsAgent0.Size() > 5 ? obsAgent0[5] : 0.0f) * worldSize;
		return std::sqrt(dx * dx + dy * dy);
	}

	MultiAgentStats RunRandomPolicyMulti(rl::NkReach2DMulti &env, float collisionRadius, uint32 seed, int episodes,
										 uint32 maxSteps) {
		uint32 rng = seed ? seed : 1u;
		int reached0 = 0, reached1 = 0;
		double totalR0 = 0.0, totalR1 = 0.0;
		long long collisionSteps = 0, totalSteps = 0;
		for (int e = 0; e < episodes; ++e) {
			NkVector<NkVector<float>> states;
			env.Reset(states);
			bool done0 = false, done1 = false;
			double epR0 = 0.0, epR1 = 0.0;
			for (uint32 step = 0; step < maxSteps && !(done0 && done1); ++step) {
				NkVector<NkVector<float>> actions;
				for (uint32 i = 0; i < 2; ++i) {
					rng = rng * 1664525u + 1013904223u;
					const float u1 = (float)((rng >> 8) & 0xFFFFFFu) / (float)0x1000000u;
					rng = rng * 1664525u + 1013904223u;
					const float u2 = (float)((rng >> 8) & 0xFFFFFFu) / (float)0x1000000u;
					NkVector<float> a;
					a.PushBack(u1 * 2.0f - 1.0f);
					a.PushBack(u2 * 2.0f - 1.0f);
					actions.PushBack(a);
				}
				NkVector<NkVector<float>> nextStates;
				NkVector<float> rewards;
				NkVector<bool> dones;
				env.Step(actions, nextStates, rewards, dones);
				if (!done0) {
					epR0 += (double)rewards[0];
					if (dones[0])
						done0 = true;
				}
				if (!done1) {
					epR1 += (double)rewards[1];
					if (dones[1])
						done1 = true;
				}
				if (AgentPairDistance(nextStates[0], env.WorldSize()) < collisionRadius)
					++collisionSteps;
				++totalSteps;
				states = nextStates;
			}
			if (done0)
				++reached0;
			if (done1)
				++reached1;
			totalR0 += epR0;
			totalR1 += epR1;
		}
		MultiAgentStats res;
		res.successRate0 = (double)reached0 / (double)episodes;
		res.successRate1 = (double)reached1 / (double)episodes;
		res.avgReward0 = totalR0 / (double)episodes;
		res.avgReward1 = totalR1 / (double)episodes;
		res.collisionRate = totalSteps > 0 ? (double)collisionSteps / (double)totalSteps : 0.0;
		return res;
	}

	MultiAgentStats TrainAndEvalMultiPPO(float worldSize, float maxStep, float reachRadius, float stepCost,
										 float distScale, float collisionRadius, float collisionPenalty, int episodes,
										 uint32 maxSteps, int evalEpisodes, uint32 seed) {
		rl::NkReach2DMulti env(2, worldSize, maxStep, reachRadius, stepCost, distScale, collisionRadius,
							   collisionPenalty, seed);

		rl::NkPPOConfig cfg0;
		cfg0.hiddenSize = 64;
		cfg0.rolloutSize = 128;
		cfg0.epochs = 4;
		cfg0.entropyCoef = 0.01f;
		cfg0.actionScale = maxStep;
		cfg0.seed = seed * 2u + 1u;
		rl::NkPPOConfig cfg1 = cfg0;
		cfg1.seed = seed * 2u + 2u;

		rl::NkPPO agent0(rl::NkPolicyMode::ContinuousGaussian, env.StateDim(), env.ActionDim(), cfg0);
		rl::NkPPO agent1(rl::NkPolicyMode::ContinuousGaussian, env.StateDim(), env.ActionDim(), cfg1);

		// ---- Entraînement : les 2 agents collectent + s'entraînent SIMULTANÉMENT dans le
		// même monde (Step() commun), chacun avec sa PROPRE politique/rollout/optimiseur. ----
		for (int e = 1; e <= episodes; ++e) {
			NkVector<NkVector<float>> states;
			env.Reset(states);
			bool done0 = false, done1 = false;
			for (uint32 step = 0; step < maxSteps && !(done0 && done1); ++step) {
				NkVector<float> action0, action1;
				float logp0 = 0.0f, val0 = 0.0f, logp1 = 0.0f, val1 = 0.0f;
				if (!done0)
					agent0.SelectAction(states[0], action0, logp0, val0);
				else {
					action0.PushBack(0.0f);
					action0.PushBack(0.0f);
				}
				if (!done1)
					agent1.SelectAction(states[1], action1, logp1, val1);
				else {
					action1.PushBack(0.0f);
					action1.PushBack(0.0f);
				}

				NkVector<NkVector<float>> actions;
				actions.PushBack(action0);
				actions.PushBack(action1);
				NkVector<NkVector<float>> nextStates;
				NkVector<float> rewards;
				NkVector<bool> dones;
				env.Step(actions, nextStates, rewards, dones);

				if (!done0) {
					agent0.Remember(states[0], action0, rewards[0], dones[0], logp0, val0);
					agent0.TrainStepIfReady(nextStates[0], dones[0]);
					done0 = dones[0];
				}
				if (!done1) {
					agent1.Remember(states[1], action1, rewards[1], dones[1], logp1, val1);
					agent1.TrainStepIfReady(nextStates[1], dones[1]);
					done1 = dones[1];
				}
				states = nextStates;
			}
		}

		// ---- Évaluation gloutonne (sans exploration) ----
		int reached0 = 0, reached1 = 0;
		double totalR0 = 0.0, totalR1 = 0.0;
		long long collisionSteps = 0, totalSteps = 0;
		for (int e = 0; e < evalEpisodes; ++e) {
			NkVector<NkVector<float>> states;
			env.Reset(states);
			bool done0 = false, done1 = false;
			double epR0 = 0.0, epR1 = 0.0;
			for (uint32 step = 0; step < maxSteps && !(done0 && done1); ++step) {
				NkVector<float> action0, action1;
				if (!done0)
					agent0.GreedyAction(states[0], action0);
				else {
					action0.PushBack(0.0f);
					action0.PushBack(0.0f);
				}
				if (!done1)
					agent1.GreedyAction(states[1], action1);
				else {
					action1.PushBack(0.0f);
					action1.PushBack(0.0f);
				}
				NkVector<NkVector<float>> actions;
				actions.PushBack(action0);
				actions.PushBack(action1);
				NkVector<NkVector<float>> nextStates;
				NkVector<float> rewards;
				NkVector<bool> dones;
				env.Step(actions, nextStates, rewards, dones);
				if (!done0) {
					epR0 += (double)rewards[0];
					if (dones[0])
						done0 = true;
				}
				if (!done1) {
					epR1 += (double)rewards[1];
					if (dones[1])
						done1 = true;
				}
				if (AgentPairDistance(nextStates[0], env.WorldSize()) < collisionRadius)
					++collisionSteps;
				++totalSteps;
				states = nextStates;
			}
			if (done0)
				++reached0;
			if (done1)
				++reached1;
			totalR0 += epR0;
			totalR1 += epR1;
		}

		MultiAgentStats res;
		res.successRate0 = (double)reached0 / (double)evalEpisodes;
		res.successRate1 = (double)reached1 / (double)evalEpisodes;
		res.avgReward0 = totalR0 / (double)evalEpisodes;
		res.avgReward1 = totalR1 / (double)evalEpisodes;
		res.collisionRate = totalSteps > 0 ? (double)collisionSteps / (double)totalSteps : 0.0;
		return res;
	}

} // namespace

int main() {
	logger.Info("=== NKRLTest : un agent apprend a traverser un monde-grille ===");

	logger.Info("-- Jalon 1 : Q-learning tabulaire --");
	// Grille 5x5, départ en haut-gauche (0), but en bas-droite (24), 3 trous.
	const uint32 N = 5, START = 0, GOAL = 24;
	NkVector<uint32> holes;
	holes.PushBack(6);
	holes.PushBack(12);
	holes.PushBack(18);
	rl::NkGridWorld env(N, START, GOAL, holes, /*stepCost*/ 0.02f);

	rl::NkQLearning agent(env.NumStates(), env.NumActions(),
						  /*alpha*/ 0.1f, /*gamma*/ 0.99f, /*epsilon*/ 1.0f, /*seed*/ 42u);

	const int episodes = 4000;
	const uint32 maxSteps = 100;

	logger.Info("-- Entrainement ({0} episodes, epsilon decroissant) --", episodes);
	int winReach = 0;
	double winReward = 0.0;
	for (int e = 1; e <= episodes; ++e) {
		// epsilon : 1.0 -> 0.05 sur les 80 premiers % des episodes (puis exploite).
		float eps = 1.0f - (float)e / (float)(episodes * 0.8);
		if (eps < 0.05f)
			eps = 0.05f;
		agent.SetEpsilon(eps);

		rl::EpisodeResult r = rl::RunEpisode(env, agent, /*learn*/ true, maxSteps);
		winReach += r.reachedGoal ? 1 : 0;
		winReward += r.reward;

		if (e % 800 == 0) {
			logger.Info("  episode {0} : succes(800 derniers) = {1}%  recompense moy = {2}  eps={3}", e,
						(double)winReach / 8.0, winReward / 800.0, (double)eps);
			winReach = 0;
			winReward = 0.0;
		}
	}

	// Évaluation : politique gloutonne pure (aucune exploration, aucun apprentissage).
	const int evalN = 200;
	int reached = 0;
	for (int e = 0; e < evalN; ++e) {
		rl::EpisodeResult r = rl::RunEpisode(env, agent, /*learn*/ false, maxSteps);
		reached += r.reachedGoal ? 1 : 0;
	}
	const double successRate = (double)reached / (double)evalN;
	logger.Info("  evaluation gloutonne : {0}/{1} episodes atteignent le but ({2}%)", reached, evalN,
				successRate * 100.0);

	// Affiche la politique apprise (flèche = action gloutonne par case).
	logger.Info("-- Politique apprise (S=depart G=but O=trou) --");
	for (uint32 y = 0; y < N; ++y)
		logger.Info("  {0}", PolicyRow(agent, env, y).CStr());

	const bool ok = successRate >= 0.95;
	logger.Info("[ {0} ] l'agent a appris a resoudre le monde-grille (>=95%)", ok ? "OK" : "KO");

	// =========================================================================
	// Test 2 (Jalon 2) : DQN -- monde CLÉ-PUIS-PORTE, AVANT (aléatoire) vs
	// APRÈS entraînement (réseau + replay buffer + réseau cible).
	// =========================================================================
	logger.Info("-- Jalon 2 : DQN (reseau + replay buffer + reseau cible) -- AVANT (aleatoire) vs APRES entrainement --");

	const uint32 DQN_N = 5, DQN_START = 0, DQN_KEY = 12, DQN_DOOR = 24;
	NkVector<uint32> dqnHoles; // pas de trous : isole la difficulté du chaînage clé->porte (chemin optimal = 8 pas)
	const int dqnEpisodes = 600;
	const uint32 dqnMaxSteps = 40;
	const int dqnEvalEpisodes = 100;
	const uint32 dqnSeeds[] = {11u, 22u, 33u, 44u, 55u};
	const int dqnNumSeeds = 5;

	double sumBeforeSucc = 0.0, sumAfterSucc = 0.0;
	double sumBeforeRew = 0.0, sumAfterRew = 0.0;
	for (int i = 0; i < dqnNumSeeds; ++i) {
		rl::NkKeyDoorGridWorld baselineEnv(DQN_N, DQN_START, DQN_KEY, DQN_DOOR, dqnHoles, /*stepCost*/ 0.01f);
		RunStats before = RunRandomPolicy(baselineEnv, dqnSeeds[i], dqnEvalEpisodes, dqnMaxSteps);
		RunStats after = TrainAndEvalDQN(DQN_N, DQN_START, DQN_KEY, DQN_DOOR, dqnHoles, dqnEpisodes, dqnMaxSteps,
										 dqnEvalEpisodes, dqnSeeds[i]);
		sumBeforeSucc += before.successRate;
		sumAfterSucc += after.successRate;
		sumBeforeRew += before.avgReward;
		sumAfterRew += after.avgReward;
		logger.Info("  graine {0} : AVANT (aleatoire) succes={1}% recompense={2}   APRES (DQN) succes={3}% "
					"recompense={4}",
					dqnSeeds[i], before.successRate * 100.0, before.avgReward, after.successRate * 100.0,
					after.avgReward);
	}
	const double meanBeforeSucc = sumBeforeSucc / (double)dqnNumSeeds;
	const double meanAfterSucc = sumAfterSucc / (double)dqnNumSeeds;
	const double meanBeforeRew = sumBeforeRew / (double)dqnNumSeeds;
	const double meanAfterRew = sumAfterRew / (double)dqnNumSeeds;
	logger.Info("  moyenne ({0} graines) : AVANT succes={1}% recompense={2}   APRES succes={3}% recompense={4}",
				dqnNumSeeds, meanBeforeSucc * 100.0, meanBeforeRew, meanAfterSucc * 100.0, meanAfterRew);

	const bool ok2 = (meanAfterSucc > meanBeforeSucc) && (meanAfterRew > meanBeforeRew);
	logger.Info("[ {0} ] le DQN entraine (reseau+replay+cible) depasse la politique aleatoire (succes ET recompense)",
				ok2 ? "OK" : "KO");

	// =========================================================================
	// Test 3 (Jalon 3, sanity DISCRET) : PPO sur le MEME monde-grille que le
	// Jalon 1 -- valide le mode Discrete de NkPolicyNet/NkPPO.
	// =========================================================================
	logger.Info("-- Jalon 3a : PPO discret (sanity, meme monde-grille que le Jalon 1) --");

	rl::NkGridWorld ppoDiscreteEnv(N, START, GOAL, holes, /*stepCost*/ 0.02f);
	RunStats ppoDiscreteBefore = RunRandomPolicy(ppoDiscreteEnv, /*seed*/ 7u, /*episodes*/ 200, /*maxSteps*/ 100);
	RunStats ppoDiscreteAfter =
		TrainAndEvalPPODiscrete(ppoDiscreteEnv, /*episodes*/ 1500, /*maxSteps*/ 100, /*evalEpisodes*/ 200, /*seed*/ 7u);
	logger.Info("  AVANT (aleatoire) succes={0}% recompense={1}   APRES (PPO discret) succes={2}% recompense={3}",
				ppoDiscreteBefore.successRate * 100.0, ppoDiscreteBefore.avgReward,
				ppoDiscreteAfter.successRate * 100.0, ppoDiscreteAfter.avgReward);
	const bool ok3 = (ppoDiscreteAfter.successRate > ppoDiscreteBefore.successRate) &&
					 (ppoDiscreteAfter.avgReward > ppoDiscreteBefore.avgReward);
	logger.Info("[ {0} ] PPO discret entraine depasse la politique aleatoire (succes ET recompense)",
				ok3 ? "OK" : "KO");

	// =========================================================================
	// Test 4 (Jalon 3, ACTIONS CONTINUES) : PPO sur rl::NkReach2D -- AVANT
	// (politique aleatoire continue) vs APRES entrainement, sur 5 graines.
	// =========================================================================
	logger.Info("-- Jalon 3b : PPO actions continues (rl::NkReach2D) -- AVANT (aleatoire) vs APRES entrainement --");

	const float R2D_WORLD = 10.0f, R2D_MAXSTEP = 1.0f, R2D_REACH = 0.5f, R2D_STEPCOST = 0.01f, R2D_DISTSCALE = 0.05f;
	const int r2dEpisodes = 500;
	const uint32 r2dMaxSteps = 40;
	const int r2dEvalEpisodes = 100;
	const uint32 r2dSeeds[] = {11u, 22u, 33u, 44u, 55u};
	const int r2dNumSeeds = 5;

	double sumR2DBeforeSucc = 0.0, sumR2DAfterSucc = 0.0;
	double sumR2DBeforeRew = 0.0, sumR2DAfterRew = 0.0;
	for (int i = 0; i < r2dNumSeeds; ++i) {
		rl::NkReach2D baselineEnv(R2D_WORLD, R2D_MAXSTEP, R2D_REACH, R2D_STEPCOST, R2D_DISTSCALE, r2dSeeds[i]);
		RunStats before = RunRandomPolicyContinuous(baselineEnv, r2dSeeds[i], r2dEvalEpisodes, r2dMaxSteps);
		RunStats after = TrainAndEvalPPOContinuous(R2D_WORLD, R2D_MAXSTEP, R2D_REACH, R2D_STEPCOST, R2D_DISTSCALE,
												   r2dEpisodes, r2dMaxSteps, r2dEvalEpisodes, r2dSeeds[i]);
		sumR2DBeforeSucc += before.successRate;
		sumR2DAfterSucc += after.successRate;
		sumR2DBeforeRew += before.avgReward;
		sumR2DAfterRew += after.avgReward;
		logger.Info("  graine {0} : AVANT (aleatoire) succes={1}% recompense={2}   APRES (PPO) succes={3}% "
					"recompense={4}",
					r2dSeeds[i], before.successRate * 100.0, before.avgReward, after.successRate * 100.0,
					after.avgReward);
	}
	const double meanR2DBeforeSucc = sumR2DBeforeSucc / (double)r2dNumSeeds;
	const double meanR2DAfterSucc = sumR2DAfterSucc / (double)r2dNumSeeds;
	const double meanR2DBeforeRew = sumR2DBeforeRew / (double)r2dNumSeeds;
	const double meanR2DAfterRew = sumR2DAfterRew / (double)r2dNumSeeds;
	logger.Info("  moyenne ({0} graines) : AVANT succes={1}% recompense={2}   APRES succes={3}% recompense={4}",
				r2dNumSeeds, meanR2DBeforeSucc * 100.0, meanR2DBeforeRew, meanR2DAfterSucc * 100.0, meanR2DAfterRew);

	const bool ok4 = (meanR2DAfterSucc > meanR2DBeforeSucc) && (meanR2DAfterRew > meanR2DBeforeRew);
	logger.Info("[ {0} ] PPO continu entraine (gaussienne+critique+GAE+clip) depasse la politique aleatoire "
				"(succes ET recompense)",
				ok4 ? "OK" : "KO");

	// =========================================================================
	// Test 5 (Jalon 3, preuve MULTI-AGENT minimale) : 2 agents PPO INDEPENDANTS
	// dans rl::NkReach2DMulti -- AVANT (aleatoire) vs APRES entrainement, 3 graines.
	// =========================================================================
	logger.Info("-- Jalon 3c : preuve multi-agent (2 PPO independants, meme monde, rl::NkReach2DMulti) --");

	const float MA_COLLISION_R = 0.6f, MA_COLLISION_PEN = 0.5f;
	const int maEpisodes = 350;
	const uint32 maMaxSteps = 40;
	const int maEvalEpisodes = 100;
	const uint32 maSeeds[] = {11u, 22u, 33u};
	const int maNumSeeds = 3;

	double sumMABeforeSucc = 0.0, sumMAAfterSucc = 0.0;
	double sumMABeforeRew = 0.0, sumMAAfterRew = 0.0;
	double sumMABeforeColl = 0.0, sumMAAfterColl = 0.0;
	for (int i = 0; i < maNumSeeds; ++i) {
		rl::NkReach2DMulti baselineEnv(2, R2D_WORLD, R2D_MAXSTEP, R2D_REACH, R2D_STEPCOST, R2D_DISTSCALE,
									   MA_COLLISION_R, MA_COLLISION_PEN, maSeeds[i]);
		MultiAgentStats before = RunRandomPolicyMulti(baselineEnv, MA_COLLISION_R, maSeeds[i], maEvalEpisodes, maMaxSteps);
		MultiAgentStats after =
			TrainAndEvalMultiPPO(R2D_WORLD, R2D_MAXSTEP, R2D_REACH, R2D_STEPCOST, R2D_DISTSCALE, MA_COLLISION_R,
								 MA_COLLISION_PEN, maEpisodes, maMaxSteps, maEvalEpisodes, maSeeds[i]);
		const double beforeSucc = (before.successRate0 + before.successRate1) * 0.5;
		const double afterSucc = (after.successRate0 + after.successRate1) * 0.5;
		const double beforeRew = (before.avgReward0 + before.avgReward1) * 0.5;
		const double afterRew = (after.avgReward0 + after.avgReward1) * 0.5;
		sumMABeforeSucc += beforeSucc;
		sumMAAfterSucc += afterSucc;
		sumMABeforeRew += beforeRew;
		sumMAAfterRew += afterRew;
		sumMABeforeColl += before.collisionRate;
		sumMAAfterColl += after.collisionRate;
		logger.Info("  graine {0} : AVANT agent0 succes={1}% agent1 succes={2}% collisions={3}%   APRES agent0 "
					"succes={4}% agent1 succes={5}% collisions={6}%",
					maSeeds[i], before.successRate0 * 100.0, before.successRate1 * 100.0, before.collisionRate * 100.0,
					after.successRate0 * 100.0, after.successRate1 * 100.0, after.collisionRate * 100.0);
	}
	const double meanMABeforeSucc = sumMABeforeSucc / (double)maNumSeeds;
	const double meanMAAfterSucc = sumMAAfterSucc / (double)maNumSeeds;
	const double meanMABeforeRew = sumMABeforeRew / (double)maNumSeeds;
	const double meanMAAfterRew = sumMAAfterRew / (double)maNumSeeds;
	const double meanMABeforeColl = sumMABeforeColl / (double)maNumSeeds;
	const double meanMAAfterColl = sumMAAfterColl / (double)maNumSeeds;
	logger.Info("  moyenne ({0} graines) : AVANT succes(moy 2 agents)={1}% recompense(moy)={2} collisions={3}%   "
				"APRES succes(moy)={4}% recompense(moy)={5} collisions={6}%",
				maNumSeeds, meanMABeforeSucc * 100.0, meanMABeforeRew, meanMABeforeColl * 100.0,
				meanMAAfterSucc * 100.0, meanMAAfterRew, meanMAAfterColl * 100.0);

	const bool ok5 = (meanMAAfterSucc > meanMABeforeSucc) && (meanMAAfterRew > meanMABeforeRew);
	logger.Info("[ {0} ] les 2 agents PPO independants progressent (succes ET recompense) dans le monde partage",
				ok5 ? "OK" : "KO");

	const int nOk = (ok ? 1 : 0) + (ok2 ? 1 : 0) + (ok3 ? 1 : 0) + (ok4 ? 1 : 0) + (ok5 ? 1 : 0);
	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", nOk, 5 - nOk);
	return (ok && ok2 && ok3 && ok4 && ok5) ? 0 : 1;
}
