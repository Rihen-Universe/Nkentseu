// =============================================================================
// NKAgent/NkAgent.h — agent cognitif : mémoire (passé), perception (présent),
// politique de décision (NKAI, Phase 4, Jalon 1 de la ROADMAP NKAgent).
//
// Assemble les trois briques du module dans une boucle
//   perception -> décision -> action -> mémorisation :
//   • NkAgentMemory     — le passé (buffer borné de transitions vécues)
//   • NkAgentPerception — le présent (état brut -> features)
//   • NkAgentPolicy     — la décision (enveloppe rl::NkQLearning de NKRL)
//
// Jalon 3 (buts & planification, le futur) : ajoute une pile de buts actifs
// (NkGoalStack, NkAgentGoal — état-cible + priorité + budget de pas) et un pas
// CONSCIENT DU BUT (StepWithGoals) : l'agent poursuit le sommet de la pile via
// une politique DÉDIÉE à ce but (une table Q PAR but — PolicyForGoal, choix
// justifié dans NkAgent.cpp), bascule automatiquement au but suivant quand il
// est atteint ou expiré (statut Failed = « jugé inatteignable », remplace une
// replanification complète). Pas de raisonnement NKInfer (Jalon 4). Branché
// sur rl::NkGridWorld pour le chemin Jalon 1/2 historique ; le chemin Jalon 3
// (StepWithGoals) est générique (rl::NkEnvironment), via le constructeur
// générique ci-dessous.
// Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKAgent/NkAgentGoal.h"
#include "NKAgent/NkAgentMemory.h"
#include "NKAgent/NkAgentPerception.h"
#include "NKAgent/NkAgentPersonality.h"
#include "NKAgent/NkAgentPolicy.h"
#include "NKAgent/NkGoalStack.h"
#include "NKRL/NkEnv.h"
#include "NKRL/NkGridWorld.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			struct NkAgentConfig {
					nk_size memoryCapacity = 200; // nb de transitions récentes conservées (le passé)
					float alpha = 0.1f;			 // taux d'apprentissage TD
					float gamma = 0.99f;			 // facteur d'actualisation
					float epsilon = 0.1f;			 // exploration ε-greedy initiale
					uint32 seed = 1u;				 // graine RNG (déterministe)

					// ── Jalon 2 : experience replay (apprentissage depuis la mémoire) ──
					// Quand actif (et learn=true), tous les `replayInterval` pas
					// d'apprentissage, l'agent rejoue `replayBatchSize` transitions
					// tirées uniformément de sa mémoire à travers la mise à jour TD de
					// la politique (NkQLearning::Update) — experience replay classique.
					// Désactivé par défaut : comportement Jalon 1 inchangé.
					bool replayEnabled = false;
					nk_size replayBatchSize = 8;  // transitions rejouées par lot
					uint32 replayInterval = 1;	  // rejoue un lot tous les N pas appris
			};

			// Résultat d'un pas unique de l'agent (perception -> décision -> action).
			struct NkAgentStepResult {
					uint32 action = 0;
					float reward = 0.0f;
					uint32 nextRawState = 0;
					bool done = false;
			};

			// Résultat d'un pas CONSCIENT DU BUT (Jalon 3, cf. NkAgent::StepWithGoals).
			// `reward`/`nextRawState`/`done` restent la vérité ENVIRONNEMENT (le vécu
			// réel, indépendant du but poursuivi) ; `goalReached`/`goalFailed`
			// décrivent le sort du but qui était ACTIF au début de ce pas.
			struct NkAgentGoalStepResult : NkAgentStepResult {
					uint32 goalTarget = 0;	  // but actif au début de ce pas
					bool goalReached = false; // ce but vient d'être atteint (=> dépilé)
					bool goalFailed = false;  // ce but vient d'expirer, budget dépassé (=> dépilé)
			};

			class NkAgent {
				public:
					NkAgent(const rl::NkGridWorld &world, const NkAgentConfig &config = NkAgentConfig());

					// Constructeur générique (Jalon 3) : PAS lié à un rl::NkGridWorld
					// concret — pour piloter n'importe quel rl::NkEnvironment discret via
					// une pile de buts (ex. rl::NkKeyDoorGridWorld). `gridSize/finalGoalX/
					// finalGoalY` alimentent uniquement Perceive() (cosmétique/
					// journalisation) ; la décision Jalon 3 passe par des politiques PAR
					// but (PolicyForGoal), pas par ces features.
					NkAgent(uint32 numStates, uint32 numActions, uint32 gridSize, uint32 finalGoalX, uint32 finalGoalY,
							const NkAgentConfig &config = NkAgentConfig());

					// Perçoit un état brut : encode en features, sans effet de bord.
					void Perceive(uint32 rawState, NkVector<float> &outFeatures) const {
						mPerception.Encode(rawState, outFeatures);
					}

					// Un pas complet : perçoit `rawState`, décide (politique), agit sur `world`,
					// mémorise la transition vécue. `learn=true` explore (ε-greedy) et met à jour
					// la politique ; `learn=false` = exploitation pure (évaluation). Dans les deux
					// cas la transition est ajoutée à la mémoire (le vécu, apprentissage à part).
					NkAgentStepResult Step(rl::NkGridWorld &world, uint32 rawState, bool learn);

					// Jalon 2 — memory replay : rejoue `batchSize` transitions tirées
					// uniformément de la mémoire à travers NkQLearning::Update (via la
					// politique). Retourne le nombre de transitions effectivement
					// rejouées (0 si la mémoire est vide). Appelée automatiquement par
					// Step() quand config.replayEnabled, mais utilisable directement.
					nk_size Replay(nk_size batchSize);

					// Nombre total de transitions rejouées depuis la construction
					// (statistique de preuve pour le jalon).
					uint64 TotalReplayed() const {
						return mTotalReplayed;
					}

					NkAgentMemory &Memory() {
						return mMemory;
					}

					const NkAgentMemory &Memory() const {
						return mMemory;
					}

					NkAgentPolicy &Policy() {
						return mPolicy;
					}

					const NkAgentPolicy &Policy() const {
						return mPolicy;
					}

					const NkAgentPerception &Perception() const {
						return mPerception;
					}

					// Jalon 3 — politique DÉDIÉE à un but : une table Q PAR but (état
					// augmenté déjà couvert par numStates si l'environnement encode le
					// contexte, ex. NkKeyDoorGridWorld ; la spécialisation vient ici de
					// la fonction de RÉCOMPENSE LOCALE au but — +1 en atteignant sa
					// cible —, distincte de la récompense globale de l'environnement).
					// Find-or-create : la première politique pour `targetState` est
					// créée avec les hyperparamètres de construction de l'agent, graine
					// décorrélée PAR but (évite que deux buts partagent la même suite
					// d'exploration). Exposée publiquement pour permettre le pilotage
					// externe de l'epsilon par but (comme Policy().SetEpsilon() au
					// Jalon 1), utile pendant l'entraînement dirigé par but.
					NkAgentPolicy &PolicyForGoal(uint32 targetState);

					// Un pas GUIDÉ PAR LE BUT ACTIF de `goals` (son sommet) : agit dans
					// `world` via PolicyForGoal(goals.Current().targetState), avec une
					// récompense LOCALE façonnée pour ce but (+1 si atteint, -1 si
					// l'environnement se termine négativement — ex. trou —, sinon la
					// récompense réelle de pas de l'environnement) ; fait ensuite
					// AVANCER la pile : but atteint -> Achieved + Pop() (dévoile le but
					// suivant) ; but expiré (`goals.Current().maxSteps` pas sans succès)
					// -> Failed + Pop() (abandon honnête, pas de recherche de plan
					// alternatif — réutilise juste le statut d'échec). La transition
					// mémorisée (NkAgentMemory) garde la récompense RÉELLE de
					// l'environnement (le vécu), pas la récompense façonnée. `learn=true`
					// explore (ε-greedy) et met à jour la politique du but courant.
					NkAgentGoalStepResult StepWithGoals(rl::NkEnvironment &world, uint32 rawState, bool learn,
														 NkGoalStack &goals);

					// Jalon 4a (personnalité) : un pas ADDITIF, analogue à Step(), mais
					// dont le CHOIX de l'action passe par NkSelectActionWithPersonality
					// (NkAgentPersonality.h) au lieu de mPolicy.SelectAction/
					// SelectGreedy — deux agents partageant EXACTEMENT la même politique
					// apprise (même table Q, via mPolicy.QLearning()) peuvent ainsi
					// différer de comportement MESURABLEMENT selon leurs traits (cf
					// NKAgentTest, Jalon 4a). N'apprend PAS (pas de mPolicy.Update ici :
					// ce pas sert à l'ÉVALUATION d'un comportement, pas à
					// l'entraînement — Step()/StepWithGoals restent le chemin
					// d'apprentissage, inchangés) ; la transition est tout de même
					// mémorisée (le vécu). `rngState` : flux dédié de l'appelant (même
					// convention que infer::NkSampleTopK), pour un tirage de curiosité
					// reproductible et INDÉPENDANT du RNG interne de replay (mRng).
					NkAgentStepResult StepWithPersonality(rl::NkGridWorld &world, uint32 rawState,
														   const NkAgentPersonality &personality, uint32 &rngState);

				private:
					float Rand01(); // [0,1) déterministe (LCG dédié, indépendant de la politique)

					// Jalon 3 : entrée du registre de politiques par but (find-or-create
					// dans PolicyForGoal). Regroupe la cible et sa table Q dédiée.
					struct NkGoalPolicyEntry {
							uint32 targetState;
							NkAgentPolicy policy;
							NkGoalPolicyEntry(uint32 target, uint32 numStates, uint32 numActions, float alpha, float gamma,
											   float epsilon, uint32 seed)
								: targetState(target), policy(numStates, numActions, alpha, gamma, epsilon, seed) {
							}
					};

					NkAgentPerception mPerception;
					NkAgentPolicy mPolicy;
					NkAgentMemory mMemory;

					// Jalon 2 : paramètres/état du replay + calcul d'importance TD.
					float mGamma;			// facteur d'actualisation (pour l'erreur TD)
					uint32 mNumActions;	// nb d'actions de l'environnement (max_a Q)
					bool mReplayEnabled;
					nk_size mReplayBatchSize;
					uint32 mReplayInterval;
					uint32 mLearnSteps;	// pas d'apprentissage effectués (cadence du replay)
					uint64 mTotalReplayed; // statistique cumulée
					uint32 mRng;			// LCG du tirage uniforme de replay

					// Jalon 3 : hyperparamètres de base (réutilisés par PolicyForGoal
					// pour créer chaque nouvelle politique par but) + registre.
					uint32 mNumStates;
					float mBaseAlpha;
					float mBaseGamma;
					float mBaseEpsilon;
					uint32 mBaseSeed;
					NkVector<NkGoalPolicyEntry> mGoalPolicies;
			};

			// Résultat d'un épisode complet mené par un NkAgent (analogue à rl::EpisodeResult,
			// mais en passant par la couche NkAgent : perception + mémoire + politique).
			struct NkAgentEpisodeResult {
					float reward = 0.0f;
					bool reachedGoal = false;
					uint32 steps = 0;
			};

			// Déroule un épisode complet à travers la couche NkAgent (pas directement via
			// rl::RunEpisode) : preuve que perception -> politique -> action -> mémoire
			// fonctionne de bout en bout au-dessus de NKRL.
			inline NkAgentEpisodeResult RunAgentEpisode(rl::NkGridWorld &world, NkAgent &agent, bool learn,
														 uint32 maxSteps) {
				NkAgentEpisodeResult res;
				uint32 s = world.Reset();
				float lastR = 0.0f;
				bool done = false;
				for (; res.steps < maxSteps && !done; ++res.steps) {
					NkAgentStepResult step = agent.Step(world, s, learn);
					res.reward += step.reward;
					lastR = step.reward;
					done = step.done;
					s = step.nextRawState;
				}
				res.reachedGoal = done && (lastR > 0.5f); // récompense +1 = but (cf. NkGridWorld)
				return res;
			}

			// Résultat d'un épisode CONSCIENT DU BUT (Jalon 3, cf. RunAgentEpisodeWithGoals).
			struct NkAgentGoalEpisodeResult {
					float reward = 0.0f;	  // récompense ENVIRONNEMENT cumulée (le vécu réel)
					bool reachedGoal = false; // épisode terminé sur une récompense > 0 (cf. NkGridWorld)
					uint32 steps = 0;
					uint32 goalsAchieved = 0; // nb de buts atteints pendant l'épisode
					uint32 goalsFailed = 0;	 // nb de buts abandonnés (budget de pas dépassé)
			};

			// Déroule un épisode complet à travers la couche NkAgent EN SUIVANT une
			// pile de buts (`goals`, déjà remplie par l'appelant — ex. Push(but final)
			// puis Push(sous-but) pour « sous-but PUIS but final ») : preuve que la
			// planification (décomposition en sous-buts + bascule automatique) résout
			// une tâche à PLUSIEURS étapes obligées, au-dessus du même NKRL/NkAgent
			// que Jalon 1/2 (aucune réimplémentation du RL). S'arrête tôt si la pile
			// se vide avant la fin de l'épisode (plus aucun but à poursuivre).
			inline NkAgentGoalEpisodeResult RunAgentEpisodeWithGoals(rl::NkEnvironment &world, NkAgent &agent,
																	  bool learn, uint32 maxSteps, NkGoalStack &goals) {
				NkAgentGoalEpisodeResult res;
				uint32 s = world.Reset();
				float lastR = 0.0f;
				bool done = false;
				for (; res.steps < maxSteps && !done; ++res.steps) {
					NkAgentGoalStepResult step = agent.StepWithGoals(world, s, learn, goals);
					res.reward += step.reward;
					lastR = step.reward;
					done = step.done;
					s = step.nextRawState;
					if (step.goalReached)
						++res.goalsAchieved;
					if (step.goalFailed)
						++res.goalsFailed;
					if (goals.IsEmpty() && !done) // plus de but actif : rien à poursuivre
						break;
				}
				res.reachedGoal = done && (lastR > 0.5f);
				return res;
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
