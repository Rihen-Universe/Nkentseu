// =============================================================================
// NKAgent/NkAgentPersonality.h — traits de personnalité (NKAI, Phase 4, Jalon 4).
//
// Objectif du jalon : des agents avec des TRAITS différents doivent produire un
// comportement MESURABLEMENT différent dans le MÊME scénario, sans réimplémenter
// le RL (NKRL) ni la perception/mémoire (NkAgentPerception/NkAgentMemory,
// inchangées) — uniquement au niveau du CHOIX de l'action, en re-pondérant les
// valeurs Q DÉJÀ APPRISES (rl::NkQLearning::Q, déjà exposées publiquement via
// NkAgentPolicy::QLearning()) selon 3 traits :
//
//   - boldness (audace)    [0,1] : 1 = suit l'action de plus haute valeur Q SANS
//     ajustement (comportement de base, identique à SelectGreedy) ; 0 = évite
//     fortement les cases DANGEREUSES (un trou, ou une case ADJACENTE à un
//     trou) même si leur Q est légèrement supérieure — un détour « prudent ».
//   - curiosity (curiosité) [0,1] : probabilité, À LA DÉCISION (même en
//     évaluation, indépendant de l'epsilon d'apprentissage de la politique),
//     de tenter une action ALÉATOIRE plutôt que la politique — un agent
//     curieux prend plus de risques « par accident » (peut heurter un trou
//     qu'un agent prudent évite systématiquement).
//   - patience              [0,2] : multiplicateur appliqué au budget de pas
//     (maxSteps) d'un but (NkAgentGoal, Jalon 3) avant abandon — un agent
//     patient persévère plus longtemps sur un but difficile avant de le
//     déclarer Failed (cf NkPatienceAdjustedMaxSteps).
//
// Design : FONCTIONS LIBRES (pas de nouvel état dans NkAgent) opérant sur des
// données DÉJÀ exposées publiquement (NkAgentPolicy::QLearning(), rl::
// NkGridWorld::IsHole/Size) + un flux RNG explicite fourni par l'appelant
// (même convention que infer::NkSampleTopK : uint32& rngState, modifié EN
// PLACE, reproductible à graine égale) — pas de réimplémentation du RL, pas de
// modification de NkAgent::Step/StepWithGoals existants (Jalons 1-3 intacts,
// zéro risque de régression). Un seul point d'intégration ADDITIF dans
// NkAgent : NkAgent::StepWithPersonality (NkAgent.h/.cpp), qui compose ces
// fonctions au lieu d'appeler SelectAction/SelectGreedy directement.
// Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKRL/NkQLearning.h"
#include "NKRL/NkGridWorld.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			struct NkAgentPersonality {
					float boldness = 1.0f;	 // 1 = comportement de base (aucun ajustement de risque)
					float curiosity = 0.0f; // 0 = comportement de base (aucune exploration à la décision)
					float patience = 1.0f;	 // 1 = comportement de base (budget de but inchangé)

					// Profils nommés utilisés par NKAgentTest (Jalon 4, ablation personnalité).
					static NkAgentPersonality Bold() {
						NkAgentPersonality p;
						p.boldness = 1.0f;
						p.curiosity = 0.35f;
						p.patience = 0.6f;
						return p;
					}
					static NkAgentPersonality Cautious() {
						NkAgentPersonality p;
						p.boldness = 0.0f;
						p.curiosity = 0.02f;
						p.patience = 1.6f;
						return p;
					}
			};

			// État brut -> (x,y), puis application de l'action DOCUMENTÉE par
			// rl::NkGridWorld.h (0=haut 1=bas 2=gauche 3=droite ; sortie de grille =
			// reste sur place) — reproduite ICI en pur calcul (SANS appeler
			// world.Step(), qui ferait réellement avancer l'environnement) pour
			// pouvoir évaluer « et si j'agis ainsi ? » sans effet de bord. Contrat
			// tiré du commentaire d'en-tête de NkGridWorld.h (source unique).
			uint32 NkGridWorldPeek(const rl::NkGridWorld &world, uint32 state, uint32 action);

			// `state` est-il un trou OU adjacent (4-voisinage) à un trou ? N'utilise
			// que rl::NkGridWorld::IsHole (API publique existante), jamais la liste
			// interne des trous. Sert à mesurer le nombre de pas « au bord du
			// danger » empruntés par un agent (métrique de preuve, NKAgentTest).
			bool NkGridWorldIsRiskyState(const rl::NkGridWorld &world, uint32 state);

			// Score de risque d'une ACTION depuis `state` : 2.0 si elle mène SUR un
			// trou, 1.0 si elle mène sur une case adjacente à un trou (cf
			// NkGridWorldIsRiskyState), 0.0 sinon.
			float NkGridWorldRisk(const rl::NkGridWorld &world, uint32 state, uint32 action);

			// Choisit une action pour `state` en tenant compte de Q (déjà appris,
			// via `q`) et de la personnalité : curiosité -> exploration aléatoire
			// (probabilité = personality.curiosity, tirage via `rngState`, MÊME
			// convention que infer::NkSampleTopK) ; sinon argmax(Q(s,a) - (1-boldness)
			// * kRiskWeight * risque(s,a)). `numActions` = nb d'actions (4 pour
			// NkGridWorld). Ne modifie PAS `q` (lecture seule).
			uint32 NkSelectActionWithPersonality(const rl::NkQLearning &q, const rl::NkGridWorld &world, uint32 state,
												  uint32 numActions, const NkAgentPersonality &personality,
												  uint32 &rngState);

			// Budget de pas effectif d'un but pour un agent `patience` (Jalon 3,
			// NkAgentGoal::maxSteps) : arrondi à l'entier le plus proche, jamais < 1
			// (0 = illimité, reste illimité quel que soit `patience`).
			uint32 NkPatienceAdjustedMaxSteps(uint32 baseMaxSteps, float patience);

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
