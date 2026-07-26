// =============================================================================
// NKAgent/NkAgent.cpp — implémentation de l'agent cognitif (NKAI, Phase 4).
//
// Jalon 2 : chaque transition mémorisée est pondérée par une IMPORTANCE =
// |erreur TD| (proxy classique de « surprise », cf. prioritized experience
// replay, Schaul et al. 2015). Choix justifié : la table Q est tabulaire et
// accessible, donc l'erreur TD exacte se calcule en O(nb actions) — plus
// informatif que |récompense| (qui n'aurait distingué que les transitions
// terminales) pour un coût quasi nul ; en début d'apprentissage (Q≈0) elle se
// réduit d'ailleurs à ~|récompense| sur les terminaux. L'importance sert à la
// RÉTENTION (la mémoire pleine évince le moins important) ; le REPLAY tire
// uniformément (pas de correction par importance sampling à gérer — la
// version proportionnelle aux priorités est une extension future).
//
// Jalon 3 : buts & planification. Choix « une table Q PAR but » (plutôt
// qu'un état augmenté du but concaténé aux features d'une politique unique) :
// la table Q tabulaire de NKRL n'a pas de notion de conditionnement — lui
// ajouter un « état-but » reviendrait à multiplier son espace d'états par le
// nombre de buts possibles ET à réapprendre le lien reward-action pour
// CHAQUE combinaison (état, but) depuis zéro, sans rien partager. Une
// politique dédiée par but est strictement équivalente (même table, adressée
// différemment) mais s'exprime directement avec NkAgentPolicy/NkQLearning
// SANS toucher NKRL, et permet un but différent = un sous-problème de RL plus
// COURT et mieux récompensé (reward shaping local : +1 sur SA cible) — c'est
// justement ce qui accélère l'apprentissage face à la tâche composée (cf.
// NKAgentTest, test 3 : décomposer « clé PUIS porte » en deux sous-problèmes
// courts bat un agent plat qui doit découvrir la trajectoire complète par
// exploration aléatoire). La planification elle-même = NkGoalStack (LIFO) :
// empiler le but final puis le sous-but dessus fait poursuivre le sous-but
// EN PREMIER, bascule automatique au but suivant une fois atteint/expiré
// (StepWithGoals ci-dessous).
// =============================================================================
#include "NKAgent/NkAgent.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			NkAgent::NkAgent(const rl::NkGridWorld &world, const NkAgentConfig &config)
				: mPerception(world),
				  mPolicy(world.NumStates(), world.NumActions(), config.alpha, config.gamma, config.epsilon, config.seed),
				  mMemory(config.memoryCapacity), mGamma(config.gamma), mNumActions(world.NumActions()),
				  mReplayEnabled(config.replayEnabled), mReplayBatchSize(config.replayBatchSize),
				  mReplayInterval(config.replayInterval ? config.replayInterval : 1u), mLearnSteps(0),
				  mTotalReplayed(0),
				  // Graine décorrélée de celle de la politique (même config.seed ne
				  // doit pas synchroniser exploration et tirage de replay).
				  mRng((config.seed ^ 0x9E3779B9u) ? (config.seed ^ 0x9E3779B9u) : 1u),
				  mNumStates(world.NumStates()), mBaseAlpha(config.alpha), mBaseGamma(config.gamma),
				  mBaseEpsilon(config.epsilon), mBaseSeed(config.seed ? config.seed : 1u) {
			}

			NkAgent::NkAgent(uint32 numStates, uint32 numActions, uint32 gridSize, uint32 finalGoalX, uint32 finalGoalY,
							  const NkAgentConfig &config)
				: mPerception(gridSize, finalGoalX, finalGoalY),
				  mPolicy(numStates, numActions, config.alpha, config.gamma, config.epsilon, config.seed),
				  mMemory(config.memoryCapacity), mGamma(config.gamma), mNumActions(numActions),
				  mReplayEnabled(config.replayEnabled), mReplayBatchSize(config.replayBatchSize),
				  mReplayInterval(config.replayInterval ? config.replayInterval : 1u), mLearnSteps(0),
				  mTotalReplayed(0), mRng((config.seed ^ 0x9E3779B9u) ? (config.seed ^ 0x9E3779B9u) : 1u),
				  mNumStates(numStates), mBaseAlpha(config.alpha), mBaseGamma(config.gamma),
				  mBaseEpsilon(config.epsilon), mBaseSeed(config.seed ? config.seed : 1u) {
			}

			float NkAgent::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			nk_size NkAgent::Replay(nk_size batchSize) {
				if (mMemory.IsEmpty())
					return 0;
				const nk_size n = mMemory.Size();
				for (nk_size k = 0; k < batchSize; ++k) {
					nk_size j = (nk_size)(Rand01() * (float)n);
					if (j >= n)
						j = n - 1;
					const NkAgentTransition &t = mMemory.At(j);
					// Rejoue la transition mémorisée à travers la mise à jour TD de la
					// politique (NkQLearning::Update) — aucune réimplémentation du RL.
					mPolicy.Update(t.rawState, t.action, t.reward, t.nextRawState, t.done);
				}
				mTotalReplayed += batchSize;
				return batchSize;
			}

			NkAgentStepResult NkAgent::Step(rl::NkGridWorld &world, uint32 rawState, bool learn) {
				NkAgentTransition t;
				mPerception.Encode(rawState, t.observation);
				t.rawState = rawState;

				// Décision : politique ε-greedy à l'entraînement, gloutonne à l'évaluation.
				t.action = learn ? mPolicy.SelectAction(rawState) : mPolicy.SelectGreedy(rawState);

				uint32 sNext;
				float r;
				bool done;
				world.Step(t.action, sNext, r, done);

				mPerception.Encode(sNext, t.nextObservation);
				t.reward = r;
				t.nextRawState = sNext;
				t.done = done;

				// Importance = |erreur TD| AVANT mise à jour (la « surprise » de cette
				// transition au moment où elle est vécue).
				float target = r;
				if (!done) {
					float best = mPolicy.QLearning().Q(sNext, 0);
					for (uint32 a = 1; a < mNumActions; ++a) {
						const float q = mPolicy.QLearning().Q(sNext, a);
						if (q > best)
							best = q;
					}
					target += mGamma * best;
				}
				float tdError = target - mPolicy.QLearning().Q(rawState, t.action);
				if (tdError < 0.0f)
					tdError = -tdError;

				if (learn)
					mPolicy.Update(rawState, t.action, r, sNext, done);

				mMemory.Push(t, tdError); // le pas devient un souvenir pondéré, learn ou pas

				// Jalon 2 : experience replay périodique (uniquement en apprentissage).
				if (learn && mReplayEnabled) {
					++mLearnSteps;
					if (mLearnSteps % mReplayInterval == 0)
						Replay(mReplayBatchSize);
				}

				NkAgentStepResult res;
				res.action = t.action;
				res.reward = r;
				res.nextRawState = sNext;
				res.done = done;
				return res;
			}

			NkAgentPolicy &NkAgent::PolicyForGoal(uint32 targetState) {
				for (nk_size i = 0; i < mGoalPolicies.Size(); ++i)
					if (mGoalPolicies[i].targetState == targetState)
						return mGoalPolicies[i].policy;

				// Graine décorrélée PAR but (et de celle du replay) : deux buts ne
				// doivent pas partager la même suite d'exploration ε-greedy.
				uint32 seed = mBaseSeed ^ (targetState * 2654435761u + 0x9E3779B9u);
				if (!seed)
					seed = 1u;
				mGoalPolicies.EmplaceBack(targetState, mNumStates, mNumActions, mBaseAlpha, mBaseGamma, mBaseEpsilon, seed);
				return mGoalPolicies[mGoalPolicies.Size() - 1].policy;
			}

			NkAgentStepResult NkAgent::StepWithPersonality(rl::NkGridWorld &world, uint32 rawState,
															const NkAgentPersonality &personality, uint32 &rngState) {
				NkAgentTransition t;
				mPerception.Encode(rawState, t.observation);
				t.rawState = rawState;

				// Seul le CHOIX de l'action change (personnalité, cf
				// NkAgentPersonality.h) — la politique/table Q n'est ni modifiée ni
				// réapprise ici (pas de mPolicy.Update : évaluation, pas
				// entraînement).
				t.action =
					NkSelectActionWithPersonality(mPolicy.QLearning(), world, rawState, mNumActions, personality, rngState);

				uint32 sNext;
				float r;
				bool done;
				world.Step(t.action, sNext, r, done);

				mPerception.Encode(sNext, t.nextObservation);
				t.reward = r;
				t.nextRawState = sNext;
				t.done = done;
				mMemory.Push(t); // le vécu, importance par défaut = |récompense|

				NkAgentStepResult res;
				res.action = t.action;
				res.reward = r;
				res.nextRawState = sNext;
				res.done = done;
				return res;
			}

			NkAgentGoalStepResult NkAgent::StepWithGoals(rl::NkEnvironment &world, uint32 rawState, bool learn,
														  NkGoalStack &goals) {
				NkAgentGoalStepResult res;

				if (goals.IsEmpty()) {
					// Aucun but actif : rien à poursuivre. Cas dégénéré non exercé par
					// le scénario de preuve (RunAgentEpisodeWithGoals s'arrête dès que
					// la pile se vide) — ne consomme pas de pas dans l'environnement.
					res.action = 0;
					res.reward = 0.0f;
					res.nextRawState = rawState;
					res.done = false;
					res.goalTarget = rawState;
					return res;
				}

				NkAgentGoal &g = goals.Current();
				const uint32 targetState = g.targetState;
				res.goalTarget = targetState;
				NkAgentPolicy &pol = PolicyForGoal(targetState);

				NkAgentTransition t;
				t.rawState = rawState;
				t.action = learn ? pol.SelectAction(rawState) : pol.SelectGreedy(rawState);

				uint32 sNext;
				float r;
				bool envDone;
				world.Step(t.action, sNext, r, envDone);

				t.reward = r; // la mémoire garde la récompense RÉELLE (le vécu), pas la récompense façonnée
				t.nextRawState = sNext;
				t.done = envDone;
				mMemory.Push(t); // importance par défaut = |récompense réelle| (cf. NkAgentMemory::Push)

				const bool goalReached = (sNext == targetState);
				// Échec « dur » de l'épisode (ex. trou) alors que ce n'était pas la
				// cible poursuivie : récompense façonnée négative pour CE but aussi
				// (l'épisode s'arrête de toute façon, mais la politique du but doit
				// apprendre à éviter ce piège).
				const bool envFail = envDone && !goalReached && r < 0.0f;
				// Récompense façonnée LOCALE au but : +1 en l'atteignant, -1 sur un
				// échec dur non lié à ce but, sinon la récompense de pas réelle de
				// l'environnement (le petit coût par pas de NkGridWorld/
				// NkKeyDoorGridWorld) — évite de réinventer/dupliquer ce coût ici.
				const float shaped = goalReached ? 1.0f : (envFail ? -1.0f : r);
				const bool pseudoDone = goalReached || envFail;
				if (learn)
					pol.Update(rawState, t.action, shaped, sNext, pseudoDone);

				++g.stepsElapsed;
				if (goalReached) {
					g.status = NkGoalStatus::Achieved;
					res.goalReached = true;
					goals.Pop(); // dévoile le but sous-jacent (planification : bascule)
				} else if (g.maxSteps > 0 && g.stepsElapsed >= g.maxSteps) {
					g.status = NkGoalStatus::Failed; // jugé inatteignable dans ce budget
					res.goalFailed = true;
					goals.Pop(); // abandon honnête : passe au but suivant de la pile (s'il y en a)
				}

				res.action = t.action;
				res.reward = r;
				res.nextRawState = sNext;
				res.done = envDone;
				return res;
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
