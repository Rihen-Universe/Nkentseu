// =============================================================================
// NKEmbodied/NkEmbodiedSafety.h — sécurité / watchdog (NKAI, Phase 6, Jalon 2).
//
// Surveille le corps (et, en option, l'actionneur qui le commande) tick par
// tick et déclenche un ARRÊT D'URGENCE (Tripped()) dès qu'un critère concret
// de danger est franchi, parmi deux :
//   1. ÉTAT BLOQUÉ : l'état du corps ne change plus pendant N ticks
//      consécutifs alors qu'il n'est pas terminé (ex. le corps "pousse dans
//      un mur"/collision répétée sans jamais avancer — le monde-grille
//      simulé n'a pas de sortie de grille explicite, "sortir = rester sur
//      place" cf. rl::NkGridWorld::Step, donc ce critère EST la sortie de
//      grille/collision répétée pour ce corps).
//   2. ACTIONNEUR SATURÉ : l'actionneur qui commande le corps a rejeté/retenu
//      (cf. NkEmbodiedLimitedActuator::LastCommandHeld()) ou écrêté (cf.
//      LastAmplitudeSaturated()) la commande demandée pendant M ticks
//      consécutifs — signe que la boucle de décision va plus vite que ce que
//      l'actionneur peut réellement exécuter.
// Une fois déclenché (Tripped()==true), ce moniteur NE FAIT RIEN d'autre par
// lui-même : c'est à l'appelant de réagir (ex. cesser d'appeler
// EmbodiedTick(), basculer vers une action de repli sûre). Comme un vrai
// watchdog matériel : ce module DÉTECTE, il n'agit pas à la place du système
// qui l'utilise.
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedSafetyMonitor {
				public:
					// `stuckTicksThreshold` : nb de ticks consécutifs à état inchangé
					// avant arrêt d'urgence. `saturationTicksThreshold` : nb de ticks
					// consécutifs d'actionneur saturé/retenu avant arrêt d'urgence. 0 ->
					// repli à 1 (évite un seuil nul qui déclencherait dès le 1er tick).
					explicit NkEmbodiedSafetyMonitor(uint32 stuckTicksThreshold = 3u, uint32 saturationTicksThreshold = 3u)
						: mStuckThreshold(stuckTicksThreshold ? stuckTicksThreshold : 1u),
						  mSaturationThreshold(saturationTicksThreshold ? saturationTicksThreshold : 1u) {
					}

					// À appeler APRÈS chaque tick (cf. NkEmbodiedLoop::EmbodiedTick).
					// `stateBefore`/`stateAfter` = état brut VÉRITÉ TERRAIN du corps
					// avant/après l'action de ce tick. `actuatorSaturated` = vrai si
					// l'actionneur de ce tick a été rate-limité/écrêté (cf.
					// NkEmbodiedLimitedActuator). Renvoie true si l'arrêt d'urgence
					// vient de se déclencher À CE TICK (transition, pas "déjà
					// déclenché") — utile pour ne journaliser l'événement qu'une fois.
					// N'a plus aucun effet une fois déclenché (Reset() requis pour
					// réarmer).
					bool Observe(uint32 stateBefore, uint32 stateAfter, bool actuatorSaturated) {
						if (mTripped)
							return false;

						if (stateAfter == stateBefore)
							++mStuckStreak;
						else
							mStuckStreak = 0;

						if (actuatorSaturated)
							++mSaturationStreak;
						else
							mSaturationStreak = 0;

						if (mStuckStreak >= mStuckThreshold) {
							mTripped = true;
							mReason = "etat bloque : etat inchange sur des ticks consecutifs (mur/collision repetee)";
						} else if (mSaturationStreak >= mSaturationThreshold) {
							mTripped = true;
							mReason = "actionneur sature/rate-limite sur des ticks consecutifs";
						}
						return mTripped;
					}

					bool Tripped() const {
						return mTripped;
					}

					const char *Reason() const {
						return mReason;
					}

					uint32 StuckStreak() const {
						return mStuckStreak;
					}

					uint32 SaturationStreak() const {
						return mSaturationStreak;
					}

					void Reset() {
						mTripped = false;
						mStuckStreak = 0;
						mSaturationStreak = 0;
						mReason = "";
					}

				private:
					uint32 mStuckThreshold;
					uint32 mSaturationThreshold;
					uint32 mStuckStreak = 0;
					uint32 mSaturationStreak = 0;
					bool mTripped = false;
					const char *mReason = "";
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
