// =============================================================================
// NkDQN.h — Deep Q-Network (NKAI, Phase 4, Jalon 2).
//
// DQN minimal (Mnih et al., 2015) : réseau Q = MLP Dense->Relu->Dense (NKNN),
// réseau CIBLE de même architecture (copie PÉRIODIQUE et DURE des poids du
// réseau en ligne, aucun gradient ne le traverse), replay buffer FIFO borné
// (NkReplayBuffer), politique ε-greedy (décroissance pilotée par l'appelant
// via SetEpsilon, même convention que rl::NkQLearning), perte MSE + Adam
// (NKOptim).
//
// Cible de Bellman : y = r + γ·max_a' Qcible(s',a')·(1−terminé), calculée à
// partir du réseau CIBLE (stabilise l'apprentissage : la cible ne bouge pas à
// chaque pas de gradient du réseau en ligne).
//
// Encodage d'état : ONE-HOT [1, numStates] — générique, fonctionne pour tout
// rl::NkEnvironment discret sans encodeur dédié par environnement. Limite
// documentée (ROADMAP.md) : pas de partage de features entre états proches
// (contrairement à un encodage position/attributs), la fonction apprise reste
// proche d'une table dans son expressivité d'entrée — mais l'architecture,
// elle, est un DQN complet (réseau + replay + cible + Bellman + Adam).
//
// Sélection de l'action Q(s,a) prédite (différentiable) : puisque NKAutograd
// n'expose pas de « gather » par indices, on masque Q(s,·) par un one-hot de
// l'action jouée (Mul élémentaire) puis on RÉDUIT la ligne par un Matmul avec
// un vecteur colonne de 1 ([numActions,1]) -> [batch,1]. Matmul est
// différentiable (cf NkVar.cpp, NK_MATMUL), donc le gradient remonte
// correctement jusqu'aux poids du réseau EN LIGNE pour l'action jouée
// uniquement — exactement l'effet d'un gather, sans qu'il existe en tant
// que tel dans NKAutograd.
//
// Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKRL/NkEnv.h"
#include "NKRL/NkReplayBuffer.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			struct NkDQNConfig {
					uint32 hiddenSize = 64;		// largeur de la couche cachée du MLP Q
					float lr = 1e-3f;				// pas d'apprentissage Adam
					float gamma = 0.99f;			// facteur d'actualisation
					nk_size replayCapacity = 5000; // capacité du replay buffer (allocation unique)
					nk_size batchSize = 32;		// taille du mini-lot échantillonné par pas de gradient
					nk_size minReplaySize = 200;	// pas d'entraînement tant que le buffer n'a pas cette taille
					uint32 targetSyncInterval = 200; // pas de gradient entre 2 synchronisations dures du réseau cible
					uint32 seed = 1u;
			};

			// DQN : réseau Q + réseau cible + replay buffer + ε-greedy + entraînement TD.
			class NkDQN {
				public:
					NkDQN(uint32 numStates, uint32 numActions, const NkDQNConfig &config = NkDQNConfig());

					// Action ε-greedy (exploration) pour l'entraînement.
					uint32 SelectAction(uint32 state);
					// Action gloutonne (argmax du réseau EN LIGNE) pour l'évaluation.
					uint32 SelectGreedy(uint32 state) const;

					void SetEpsilon(float e) {
						mEpsilon = e;
					}

					float Epsilon() const {
						return mEpsilon;
					}

					// Enregistre une transition vécue dans le replay buffer (aucun
					// apprentissage déclenché ici : cf TrainStep()).
					void Remember(uint32 state, uint32 action, float reward, uint32 nextState, bool done);

					// Un pas de gradient : échantillonne un mini-lot du replay buffer,
					// calcule la cible de Bellman via le réseau CIBLE (sans gradient),
					// rétropropage la perte MSE dans le réseau EN LIGNE (Adam), puis
					// synchronise durement le réseau cible tous les
					// `config.targetSyncInterval` pas de gradient. Ne fait rien et
					// renvoie false si le buffer n'a pas encore `minReplaySize`
					// transitions (échauffement).
					bool TrainStep();

					nk_size ReplaySize() const {
						return mReplay.Size();
					}

					uint64 GradientSteps() const {
						return mGradientSteps;
					}

				private:
					NkTensor EncodeOneHot(uint32 state) const;					   // [1, numStates]
					NkTensor ForwardOnlineRaw(const NkTensor &input) const;		   // tenseur brut (inférence)
					NkTensor ForwardTargetRaw(const NkTensor &input) const;		   // idem, réseau cible
					void SyncTarget();											   // copie EN LIGNE -> CIBLE (clone profond)
					float Rand01();												   // [0,1) déterministe (LCG)

					uint32 mNumStates;
					uint32 mNumActions;
					NkDQNConfig mConfig;

					nn::NkDense mOnline1; // numStates -> hidden
					nn::NkDense mOnline2; // hidden -> numActions
					nn::NkDense mTarget1; // même archi, poids synchronisés périodiquement
					nn::NkDense mTarget2;

					NkVector<NkVar> mOnlineParams; // W1,b1,W2,b2 du réseau EN LIGNE (feuilles NkVar)
					optim::NkAdam mOptim;

					NkReplayBuffer mReplay;

					float mEpsilon;
					uint32 mRng;
					uint64 mGradientSteps;
			};

			// Résultat d'un épisode joué avec un NkDQN (même convention que rl::EpisodeResult).
			struct NkDQNEpisodeResult {
					float reward = 0.0f;
					bool reachedGoal = false;
					uint32 steps = 0;
			};

			// Déroule un épisode complet. `learn=true` : ε-greedy, mémorisation de
			// chaque transition + un pas de gradient (TrainStep) par pas de temps ;
			// `learn=false` : politique gloutonne pure, sans apprentissage ni
			// mémorisation (évaluation).
			inline NkDQNEpisodeResult RunDQNEpisode(NkEnvironment &env, NkDQN &agent, bool learn, uint32 maxSteps) {
				NkDQNEpisodeResult res;
				uint32 s = env.Reset();
				bool done = false;
				float lastR = 0.0f;
				for (; res.steps < maxSteps && !done; ++res.steps) {
					uint32 a = learn ? agent.SelectAction(s) : agent.SelectGreedy(s);
					uint32 sNext;
					float r;
					env.Step(a, sNext, r, done);
					if (learn) {
						agent.Remember(s, a, r, sNext, done);
						agent.TrainStep();
					}
					res.reward += r;
					lastR = r;
					s = sNext;
				}
				res.reachedGoal = done && (lastR > 0.5f); // récompense +1 = but
				return res;
			}

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
