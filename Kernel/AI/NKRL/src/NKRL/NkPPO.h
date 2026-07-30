// =============================================================================
// NkPPO.h — Proximal Policy Optimization (NKAI, Phase 4, Jalon 3).
//
// Référence : Schulman, Wolski, Dhariwal, Radford, Klimov, "Proximal Policy
// Optimization Algorithms", arXiv:1707.06347 (2017) — objectif de substitution
// CLIPPÉ L^CLIP(θ) = E[min(r_t(θ)·A_t, clip(r_t(θ),1-ε,1+ε)·A_t)], où
// r_t(θ) = π_θ(a_t|s_t) / π_θold(a_t|s_t). On ajoute le bonus d'entropie S
// (terme L^{CLIP+VF+S} du papier, §5). PAS de version REINFORCE simple
// implémentée séparément : PPO EST l'algorithme de policy-gradient de ce
// Jalon 3, sa perte se réduit d'ailleurs au gradient de politique standard
// pondéré par l'avantage quand r_t≈1 (début d'entraînement / petit pas), donc
// REINFORCE n'apporte rien de plus qui ne soit déjà couvert.
//
// Avantage : GAE (Generalized Advantage Estimation), Schulman, Moritz, Levine,
// Jordan, Abbeel, "High-Dimensional Continuous Control Using Generalized
// Advantage Estimation", arXiv:1506.02438 (2016) —
//   δ_t = r_t + γ·V(s_{t+1})·(1-done_t) − V(s_t)
//   A_t = δ_t + γλ·(1-done_t)·A_{t+1}                (récursion arrière)
// Implémentation COMPLÈTE (pas de simplification par rapport au papier), sur
// UN SEUL rollout séquentiel (pas de parallélisation multi-environnements).
//
// Rollout ON-POLICY : contrairement à rl::NkReplayBuffer (DQN, hors-politique,
// FIFO, rejoué), le rollout de NkPPO est un simple TAMPON borné à
// `config.rolloutSize` transitions, entièrement VIDÉ après chaque mise à jour
// (`config.epochs` passes d'optimisation sur les MÊMES transitions, avantages/
// retours calculés UNE SEULE FOIS par rollout — convention standard PPO, cf
// implémentations de référence type Spinning Up / Baselines).
//
// Politique : rl::NkPolicyNet (discrète OU gaussienne continue, cf ce header).
// Critique (fonction de valeur V) : petit MLP SÉPARÉ (poids et optimiseur
// Adam INDÉPENDANTS de la politique), entraîné par régression MSE sur les
// retours GAE.
//
// Limites HONNÊTES (cf ROADMAP.md) : pas de minibatching à l'intérieur d'une
// mise à jour (toutes les transitions du rollout sont utilisées à CHAQUE
// époque, pas de sous-échantillonnage) ; pas de collecte multi-environnements
// parallèle (un seul rollout séquentiel) ; chaque transition est traitée par
// un forward/backward INDIVIDUEL (batch=1) faute d'opérateur "gather"/slice
// différentiable dans NKAutograd pour un traitement batché de la perte
// clippée par échantillon (même contrainte documentée pour NkDQN, cf
// NkDQN.h) — coût CPU plus élevé qu'un PPO batché de référence, acceptable à
// l'échelle des environnements-jouets de ce Jalon.
// =============================================================================
#pragma once

#include "NKRL/NkPolicyNet.h"
#include "NKOptim/NkOptim.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			struct NkPPOConfig {
					uint32 hiddenSize = 64; // largeur cachée du MLP de politique ET du critique
					float policyLr = 3e-4f;
					float valueLr = 1e-3f;
					float gamma = 0.99f;	  // facteur d'actualisation
					float gaeLambda = 0.95f; // λ du GAE (0 = TD(0), 1 = Monte-Carlo)
					float clipEps = 0.2f;	  // ε de l'objectif clippé PPO
					uint32 epochs = 4;		  // passes d'optimisation par rollout collecté
					nk_size rolloutSize = 128; // transitions collectées avant une mise à jour
					float entropyCoef = 0.01f; // poids du bonus d'entropie (exploration)
					float initLogStd = -0.5f;  // continu seulement : cf NkPolicyNetConfig
					float actionScale = 1.0f;  // continu seulement : cf NkPolicyNetConfig::actionScale
					uint32 seed = 1u;
			};

			// Agent PPO : politique (discrète ou gaussienne continue) + critique + rollout on-policy.
			class NkPPO {
				public:
					NkPPO(NkPolicyMode mode, uint32 inputDim, uint32 numActionsOrActionDim,
						  const NkPPOConfig &config = NkPPOConfig());

					// Échantillonne une action via la politique COURANTE (exploration) et calcule
					// V(s) via le critique — à appeler à chaque pas de collecte de rollout.
					void SelectAction(const NkVector<float> &state, NkVector<float> &outAction, float &outLogProb,
									  float &outValue);

					// Action gloutonne (évaluation, SANS exploration).
					void GreedyAction(const NkVector<float> &state, NkVector<float> &outAction) const;

					// Enregistre une transition (état AVANT action, action jouée, récompense,
					// terminal, logProb ET valeur CAPTURÉES à la collecte — nécessaires au ratio
					// PPO et au GAE). Ne déclenche PAS d'entraînement (cf TrainStepIfReady).
					void Remember(const NkVector<float> &state, const NkVector<float> &action, float reward,
								  bool done, float logProbOld, float value);

					// Déclenche une mise à jour COMPLÈTE (GAE + PPO clippé + critique, `epochs`
					// passes) dès que `rolloutSize` transitions sont accumulées, OU si
					// `forceUpdate` (fin d'épisode, rollout coupé plus tôt) ; sinon ne fait rien et
					// renvoie false. `lastState` = état COURANT (après la dernière transition
					// enregistrée), utilisé pour le bootstrap V(s) du GAE si le rollout est coupé
					// en cours d'épisode (dernière transition non terminale).
					bool TrainStepIfReady(const NkVector<float> &lastState, bool forceUpdate = false);

					uint64 UpdateCount() const {
						return mUpdateCount;
					}

					nk_size RolloutSize() const {
						return mStates.Size();
					}

					NkPolicyMode Mode() const {
						return mPolicy.Mode();
					}

					const NkPolicyNet &Policy() const {
						return mPolicy;
					}

				private:
					NkTensor EncodeState(const NkVector<float> &state) const; // [1,inputDim]
					NkVar ForwardValue(const NkTensor &stateInput) const;	   // critique -> [1,1]
					void Update(const NkVector<float> &lastState);
					void ClearRollout();

					NkPolicyNet mPolicy;
					nn::NkDense mValue1; // critique : inputDim -> hidden
					nn::NkDense mValue2; // critique : hidden -> 1

					NkVector<NkVar> mPolicyParams;
					NkVector<NkVar> mValueParams;
					optim::NkAdam mPolicyOptim;
					optim::NkAdam mValueOptim;

					NkPPOConfig mConfig;
					uint32 mInputDim;

					// Rollout ON-POLICY (vidé après chaque mise à jour, cf en-tête).
					NkVector<NkVector<float>> mStates;
					NkVector<NkVector<float>> mActions;
					NkVector<float> mRewards;
					NkVector<bool> mDones;
					NkVector<float> mLogProbsOld;
					NkVector<float> mValuesOld;

					uint64 mUpdateCount;
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
