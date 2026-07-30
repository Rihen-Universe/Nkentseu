// =============================================================================
// NKEmbodied/NkEmbodiedLoop.h — boucle perception -> décision -> action (NKAI, Phase 6, Jalon 1).
//
// Le cœur du Jalon 1 : à chaque tick, (1) un NkSensor LIT le corps
// (perception), (2) une NkEmbodiedPolicy DÉCIDE une action à partir de ces
// observations (décision — NKAgent ou repli heuristique/aléatoire), (3) un
// NkActuator APPLIQUE cette action au corps (action, effet de bord réel sur
// NkEmbodiedBody). Boucle générique : ne connaît ni le type concret du corps,
// ni celui du capteur/actionneur/politique — juste les trois interfaces. Pas
// de fréquence fixe / temps réel ici (Jalon 2), ni de bruit capteur / limites
// actionneur (Jalon 2) : un tick = un pas de simulation, au rythme de
// l'appelant.
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKEmbodied/NkActuator.h"
#include "NKEmbodied/NkEmbodiedBody.h"
#include "NKEmbodied/NkEmbodiedPolicy.h"
#include "NKEmbodied/NkSensor.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			// Trace d'UN tick de boucle (preuve, pour journalisation/tests, que les
			// observations perçues ont réellement influencé l'action choisie).
			struct NkEmbodiedTickResult {
					uint32 rawStateBefore = 0;		 // état VRAI du corps avant l'action (vérité terrain)
					uint32 perceivedStateBefore = 0; // état ESTIMÉ à partir du capteur (Jalon 2, cf. NkSensor::EstimateRawState)
					NkVector<float> observations; // ce que le capteur a réellement lu ce tick (potentiellement bruité)
					uint32 action = 0;			   // ce que la politique a réellement décidé
					uint32 rawStateAfter = 0;
					float reward = 0.0f;
					bool done = false;
			};

			// Un tick unique : perçoit (sensor), décide (policy), agit (actuator).
			// `body` sert uniquement à lire l'état brut AVANT/APRÈS (pour la trace) —
			// c'est l'actionneur, pas cet appel, qui mute le corps.
			//
			// Jalon 2 (bruit capteur) : la politique décide à partir de
			// `perceivedStateBefore`, PAS de la vérité terrain — si le capteur sait
			// estimer l'état à partir de ses observations (cf.
			// NkSensor::EstimateRawState, ex. NkEmbodiedGridSensor), cette estimation
			// peut différer de l'état réel quand le capteur est bruité (cf.
			// NkEmbodiedNoisySensor) : c'est ce décalage qui affecte réellement une
			// politique tabulaire (ex. NKAgent, qui indexe par état et ignore
			// `observations`). Un capteur qui ne sait pas estimer (repli par défaut de
			// NkSensor) laisse `perceivedStateBefore == rawStateBefore` : comportement
			// du Jalon 1 inchangé.
			inline NkEmbodiedTickResult EmbodiedTick(NkEmbodiedBody &body, const NkSensor &sensor, NkActuator &actuator,
													  NkEmbodiedPolicy &policy) {
				NkEmbodiedTickResult res;
				res.rawStateBefore = body.State();
				res.perceivedStateBefore = res.rawStateBefore; // repli : vérité terrain si le capteur ne sait pas estimer

				sensor.Sense(res.observations);										  // perception : lecture réelle (ou bruitée) du corps
				sensor.EstimateRawState(res.observations, res.perceivedStateBefore); // écrase si le capteur sait estimer

				res.action = policy.Decide(res.observations, res.perceivedStateBefore); // décision

				NkVector<float> actionVec;
				actionVec.PushBack((float)res.action);
				actuator.Apply(actionVec); // action : mute réellement le corps

				res.rawStateAfter = body.State();
				res.reward = body.LastReward();
				res.done = body.IsDone();
				return res;
			}

			// Résultat d'un épisode complet mené par la boucle Jalon 1.
			struct NkEmbodiedEpisodeResult {
					uint32 ticks = 0;
					float rewardSum = 0.0f;
					bool reachedGoal = false;
			};

			// Déroule un épisode complet : Reset() puis EmbodiedTick() jusqu'à
			// `maxTicks` ou jusqu'à ce que le corps soit terminé (but atteint ou
			// trou). Preuve de bout en bout que la boucle perception -> décision ->
			// action tourne réellement N ticks sans erreur.
			inline NkEmbodiedEpisodeResult RunEmbodiedEpisode(NkEmbodiedBody &body, const NkSensor &sensor,
															   NkActuator &actuator, NkEmbodiedPolicy &policy,
															   uint32 maxTicks) {
				NkEmbodiedEpisodeResult res;
				body.Reset();
				for (; res.ticks < maxTicks && !body.IsDone(); ++res.ticks) {
					NkEmbodiedTickResult tick = EmbodiedTick(body, sensor, actuator, policy);
					res.rewardSum += tick.reward;
				}
				res.reachedGoal = body.ReachedGoal();
				return res;
			}

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
