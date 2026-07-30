// =============================================================================
// NKCivilization/NkCivAgentSystem.h — le système NKECS qui fait vivre le
// micro-monde (Phase 5, Jalon 1) : à chaque Execute() (un TICK), chaque entité
// portant (NkCivAgentRef, NkCivPosition) perçoit l'occupation courante de la
// grille partagée et agit selon SA politique apprise (gloutonne, Phase 4).
//
// Reprend exactement la logique prouvée par le prototype pré-ECS de
// NKCivilizationTest (2026-07-23), mais pilotée par un ecs::NkSystem standard
// sur un ecs::NkWorld — plus de tableau `pos[]` ni de boucle à la main :
//   - ordre d'action DÉTERMINISTE dans le tick (champ NkCivAgentRef::turnOrder,
//     trié à chaque Execute — l'ordre d'itération des archétypes n'est pas un
//     contrat) ;
//   - règle de collision : la case visée occupée par un AUTRE agent actif
//     (et qui n'est pas le but commun) => l'agent reste sur place ce tick
//     (blockedCount++, mesure d'interaction n°1) ;
//   - ressources consommables : entrer sur une case ressource encore
//     disponible la consomme (premier arrivé, resourcesCollected++, mesure
//     d'interaction n°2 — compétition) ;
//   - trous : y entrer termine l'agent (done, pas de but) ; but commun :
//     ouvert à tous, termine l'agent avec reachedGoal.
// Les agents terminés n'occupent plus l'espace (absorbés par le but/le trou).
//
// Observation (Phase 5, outils d'observation) : un NkCivRecorder OPTIONNEL
// (pointeur non possédant, SetRecorder) peut être branché — chaque Execute()
// y capture alors la frame du tick (position + action + événements par
// agent, ressources restantes), sans changer la logique de simulation d'un
// bit (le recorder est un OBSERVATEUR pur, jamais une dépendance de calcul).
//
// Jalon 2 — règle sociale (2026-07-25) : quand `grid->SocialSharingEnabled()`
// est ON, la collision est exemptée sur les cases ressource (récolte
// partagée possible), et `grid->RegenerateTick()` est appelé une fois par
// Execute() (no-op si aucune ressource n'a de regenPerTick). OFF par défaut
// -> comportement condition A (Jalon 1) inchangé au bit près.
// Namespace : nkentseu::ai::civ.
// =============================================================================
#pragma once

#include "NKECS/System/NkSystem.h"
#include "NKECS/World/NkWorld.h"
#include "NKCivilization/NkCivComponents.h"
#include "NKCivilization/NkCivGridState.h"
#include "NKCivilization/NkCivRecorder.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			class NkCivAgentSystem : public ecs::NkSystem {
				public:
					// `grid` non possédé : le substrat partagé (durée de vie gérée par
					// l'appelant, comme les agents référencés par NkCivAgentRef).
					explicit NkCivAgentSystem(NkCivGridState *grid) : mGrid(grid) {
					}

					ecs::NkSystemDesc Describe() const override {
						return ecs::NkSystemDesc{}
							.Writes<NkCivAgentRef>()
							.Writes<NkCivPosition>()
							.InGroup(ecs::NkSystemGroup::Update)
							.Sequential() // l'ordre du tour est un contrat : pas de parallélisme
							.Named("NkCivAgentSystem");
					}

					// Un TICK du micro-monde.
					void Execute(ecs::NkWorld &world, float32 dt) override;

					// Branche un enregistreur de trajectoires (non possédé, peut être
					// nullptr pour désactiver). À appeler AVANT le premier Execute() —
					// c'est l'appelant qui a fait NkCivRecorder::BeginSession().
					void SetRecorder(NkCivRecorder *recorder) {
						mRecorder = recorder;
					}

					// ── Mesures globales (preuves du jalon) ──────────────────────────
					uint32 Ticks() const {
						return mTicks;
					}

					uint32 TotalBlockedEvents() const {
						return mTotalBlocked;
					}

					uint32 TotalResourcesConsumed() const {
						return mTotalResources;
					}

					// Des agents actifs (non terminés) restaient-ils au dernier tick ?
					bool AnyActiveLastTick() const {
						return mAnyActiveLastTick;
					}

				private:
					NkCivGridState *mGrid; // non possédé
					NkCivRecorder *mRecorder = nullptr; // non possédé, optionnel
					uint32 mTicks = 0;
					uint32 mTotalBlocked = 0;
					uint32 mTotalResources = 0;
					bool mAnyActiveLastTick = false;
			};

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
