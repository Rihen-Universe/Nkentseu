// =============================================================================
// NKCivilization/NkCivAgentSystem.cpp — implémentation du tick du micro-monde
// (Phase 5, Jalon 1, substrat NKECS).
// =============================================================================
#include "NKCivilization/NkCivAgentSystem.h"

namespace nkentseu {
	namespace ai {
		namespace civ {

			void NkCivAgentSystem::Execute(ecs::NkWorld &world, float32 /*dt*/) {
				// 1) Collecte des habitants via une QUERY NKECS (le substrat réel :
				//    les agents sont des entités, pas des objets d'une boucle main).
				NkVector<ecs::NkEntityId> ids;
				world.Query<NkCivAgentRef, NkCivPosition>().ForEach(
					[&ids](ecs::NkEntityId id, NkCivAgentRef &, NkCivPosition &) { ids.PushBack(id); });

				// 2) Ordre de tour DÉTERMINISTE : tri par turnOrder (tri par insertion,
				//    N petit ; l'ordre d'itération des archétypes n'est pas un contrat).
				for (nk_size i = 1; i < ids.Size(); ++i) {
					const ecs::NkEntityId key = ids[i];
					const uint32 keyOrder = world.GetRef<NkCivAgentRef>(key).turnOrder;
					nk_size j = i;
					while (j > 0 && world.GetRef<NkCivAgentRef>(ids[j - 1]).turnOrder > keyOrder) {
						ids[j] = ids[j - 1];
						--j;
					}
					ids[j] = key;
				}

				// 3) Le tour : chacun, dans l'ordre, perçoit l'occupation COURANTE
				//    (les mouvements déjà joués ce tick comptent — sémantique
				//    identique au prototype séquentiel prouvé) et agit.
				if (mRecorder)
					mRecorder->BeginTick(mTicks);

				bool anyActive = false;
				for (nk_size i = 0; i < ids.Size(); ++i) {
					NkCivAgentRef &ac = world.GetRef<NkCivAgentRef>(ids[i]);
					if (ac.done || ac.agent == nullptr) {
						// Agent déjà terminé (avant ce tick) : échantillon "figé"
						// quand même enregistré (position inchangée, flag DONE) pour
						// que chaque frame porte un échantillon par agent.
						if (mRecorder && ac.agent != nullptr) {
							const NkCivPosition &frozenPos = world.GetRef<NkCivPosition>(ids[i]);
							mRecorder->RecordAgent(ac.turnOrder, frozenPos.state, NK_CIV_REC_NO_ACTION,
													NK_CIV_REC_DONE);
						}
						continue;
					}
					anyActive = true;

					NkCivPosition &pos = world.GetRef<NkCivPosition>(ids[i]);
					const uint32 beforeState = pos.state;

					// Décision : politique APPRISE (Phase 4), gloutonne pure.
					const uint32 action = ac.agent->Policy().SelectGreedy(pos.state);
					const uint32 next = mGrid->NextState(pos.state, action);

					// Règle de collision : case visée occupée par un AUTRE agent
					// ACTIF, et qui n'est pas le but commun => bloqué ce tick.
					// Jalon 2 — règle sociale (SocialSharingEnabled) : une case
					// ressource devient un site de RÉCOLTE PARTAGÉE, exempté de la
					// collision -> plusieurs agents peuvent la co-occuper le même
					// tick au lieu que le premier arrivé bloque physiquement les
					// suivants (condition B de l'expérience Jalon 2 ; OFF par défaut
					// => comportement condition A / Jalon 1 inchangé au bit près).
					bool blocked = false;
					const bool exemptSharedResource = mGrid->SocialSharingEnabled() && mGrid->IsResourceCell(next);
					if (!mGrid->IsGoal(next) && !exemptSharedResource) {
						for (nk_size j = 0; j < ids.Size(); ++j) {
							if (j == i)
								continue;
							const NkCivAgentRef &other = world.GetRef<NkCivAgentRef>(ids[j]);
							if (other.done)
								continue; // les terminés n'occupent plus l'espace
							if (world.GetRef<NkCivPosition>(ids[j]).state == next) {
								blocked = true;
								break;
							}
						}
					}

					++ac.steps;
					uint8 recFlags = NK_CIV_REC_ACTIVE;
					if (blocked) {
						++ac.blockedCount;
						++mTotalBlocked; // interaction n°1 : l'espace se dispute
						recFlags |= NK_CIV_REC_BLOCKED;
						if (mRecorder)
							mRecorder->RecordAgent(ac.turnOrder, beforeState, (uint8)action, recFlags);
						continue;		  // reste sur place ce tick
					}

					pos.state = next;
					recFlags |= NK_CIV_REC_MOVED;

					// Interaction n°2 : compétition de ressources (premier arrivé).
					if (mGrid->ConsumeResource(pos.state)) {
						++ac.resourcesCollected;
						++mTotalResources;
						recFlags |= NK_CIV_REC_COLLECTED;
					}

					if (mGrid->IsGoal(pos.state)) {
						ac.done = true;
						ac.reachedGoal = true;
						recFlags |= NK_CIV_REC_REACHED_GOAL | NK_CIV_REC_DONE;
					} else if (mGrid->IsHole(pos.state)) {
						ac.done = true;
						ac.reachedGoal = false;
						recFlags |= NK_CIV_REC_FELL | NK_CIV_REC_DONE;
					}

					if (mRecorder)
						mRecorder->RecordAgent(ac.turnOrder, pos.state, (uint8)action, recFlags);
				}

				// Jalon 2 — régénération des ressources renouvelables (no-op si toutes
				// les cases ont regenPerTick=0, donc sans effet en condition A).
				mGrid->RegenerateTick();

				if (mRecorder) {
					const nk_size remaining = mGrid->ResourcesRemaining();
					mRecorder->EndTick((uint32)remaining);
				}

				mAnyActiveLastTick = anyActive;
				++mTicks;
			}

		} // namespace civ
	} // namespace ai
} // namespace nkentseu
