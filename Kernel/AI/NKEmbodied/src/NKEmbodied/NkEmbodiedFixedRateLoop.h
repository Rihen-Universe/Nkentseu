// =============================================================================
// NKEmbodied/NkEmbodiedFixedRateLoop.h — boucle à fréquence de décision fixe (NKAI, Phase 6, Jalon 2).
//
// Découple le taux de DÉCISION (perception -> politique -> action, cf.
// NkEmbodiedLoop::EmbodiedTick) du taux d'APPEL (le "taux de simulation" : la
// cadence à laquelle Advance() est appelé, ex. une boucle de rendu/simulation
// tournant à une fréquence quelconque, pas forcément synchronisée avec la
// fréquence de contrôle voulue). Même pattern "accumulateur de temps fixe"
// que physics::NkPhysicsWorld::Advance() (cf. Kernel/Runtime/NKPhysics/src/
// NKPhysics/NkPhysicsWorld.h) : on accumule le temps réel écoulé (`realDt`)
// et on exécute un EmbodiedTick() à chaque fois que l'accumulateur dépasse la
// PÉRIODE DE DÉCISION fixe (1/decisionHz), indépendamment de la fréquence ou
// de l'irrégularité des appels à Advance(). Ne réimplémente pas la boucle
// perception -> décision -> action : délègue intégralement à EmbodiedTick()
// (cf. NkEmbodiedLoop.h).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKEmbodied/NkEmbodiedLoop.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedFixedRateLoop {
				public:
					// `decisionHz` : nombre de décisions perception->décision->action par
					// seconde (fréquence de contrôle fixe). <= 0 -> repli à 1 Hz (évite
					// une période nulle / une division par zéro).
					explicit NkEmbodiedFixedRateLoop(float decisionHz)
						: mDecisionPeriod(decisionHz > 0.0f ? 1.0f / decisionHz : 1.0f) {
					}

					// Accumule `realDt` secondes écoulées depuis le dernier appel, et
					// exécute AU PLUS `maxDecisions` EmbodiedTick() (au rythme fixe
					// configuré), en s'arrêtant plus tôt si le corps est terminé. Le
					// temps accumulé non consommé N'EST JAMAIS perdu : il reste pour le
					// prochain appel (cf. Accumulator()). Renvoie le nombre de décisions
					// RÉELLEMENT exécutées par CET appel (peut être 0 si `realDt` n'a pas
					// suffi à franchir une période entière).
					uint32 Advance(float realDt, NkEmbodiedBody &body, const NkSensor &sensor, NkActuator &actuator,
								   NkEmbodiedPolicy &policy, uint32 maxDecisions = 8u) {
						mAccumulator += realDt;
						uint32 decisionsThisCall = 0;
						while (mAccumulator >= mDecisionPeriod && decisionsThisCall < maxDecisions && !body.IsDone()) {
							EmbodiedTick(body, sensor, actuator, policy);
							mAccumulator -= mDecisionPeriod;
							++decisionsThisCall;
							++mDecisionCount;
						}
						return decisionsThisCall;
					}

					float DecisionHz() const {
						return mDecisionPeriod > 0.0f ? 1.0f / mDecisionPeriod : 0.0f;
					}

					float DecisionPeriod() const {
						return mDecisionPeriod;
					}

					// Temps accumulé non encore consommé (toujours < DecisionPeriod(),
					// sauf si `maxDecisions` a plafonné un rattrapage dans Advance()).
					float Accumulator() const {
						return mAccumulator;
					}

					// Nombre total de décisions exécutées depuis la construction (ou le
					// dernier Reset()) — utile pour vérifier le découplage taux de
					// décision / taux d'appel dans les tests : Advance() appelé à haute
					// fréquence doit produire un DecisionCount() proche de
					// temps_total * DecisionHz(), PAS un par appel.
					uint32 DecisionCount() const {
						return mDecisionCount;
					}

					void Reset() {
						mAccumulator = 0.0f;
						mDecisionCount = 0;
					}

				private:
					float mDecisionPeriod;
					float mAccumulator = 0.0f;
					uint32 mDecisionCount = 0;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
