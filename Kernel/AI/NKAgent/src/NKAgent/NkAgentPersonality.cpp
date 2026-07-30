// =============================================================================
// NKAgent/NkAgentPersonality.cpp — voir NkAgentPersonality.h pour la portée, le
// design (fonctions libres, RNG explicite) et les 3 traits (boldness/curiosity/
// patience).
// =============================================================================
#include "NKAgent/NkAgentPersonality.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			namespace {

				// Poids du terme de risque dans le score ajusté — assez grand pour
				// faire basculer une case « limite » (Q proches) vers un détour prudent,
				// sans annuler un écart de Q franc (route bien meilleure mais risquée).
				// Choisi empiriquement pour la grille 5x5 à trous de NKAgentTest (Q
				// appris dans environ [-1,1], cf NkAgent.cpp/NKAgentTest) — vérifié à
				// l'exécution (cf NKAgentTest, Jalon 4a).
				constexpr float kRiskWeight = 1.5f;

				// Même schéma LCG que le reste du dépôt (NkAgent::Rand01,
				// infer::NkSampleTopK, NKNN::RandnTensor) — reproduit ici pour ne pas
				// dépendre d'un état interne de NkAgent (fonctions libres).
				uint32 PersonalityRand01Draw(uint32 &rngState) {
					rngState = rngState * 1664525u + 1013904223u;
					return rngState;
				}

				float Rand01From(uint32 raw) {
					return (float)((raw >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
				}

			} // namespace

			uint32 NkGridWorldPeek(const rl::NkGridWorld &world, uint32 state, uint32 action) {
				const uint32 size = world.Size();
				int32 x = (int32)(state % size);
				int32 y = (int32)(state / size);
				switch (action) {
					case 0:
						y -= 1;
						break; // haut
					case 1:
						y += 1;
						break; // bas
					case 2:
						x -= 1;
						break; // gauche
					case 3:
						x += 1;
						break; // droite
					default:
						break;
				}
				if (x < 0 || x >= (int32)size || y < 0 || y >= (int32)size)
					return state; // sortie de grille = reste sur place (cf NkGridWorld.h)
				return (uint32)y * size + (uint32)x;
			}

			bool NkGridWorldIsRiskyState(const rl::NkGridWorld &world, uint32 state) {
				if (world.IsHole(state))
					return true;
				const uint32 size = world.Size();
				const int32 x = (int32)(state % size);
				const int32 y = (int32)(state / size);
				static const int32 dx[4] = {0, 0, -1, 1};
				static const int32 dy[4] = {-1, 1, 0, 0};
				for (int i = 0; i < 4; ++i) {
					const int32 ax = x + dx[i];
					const int32 ay = y + dy[i];
					if (ax < 0 || ax >= (int32)size || ay < 0 || ay >= (int32)size)
						continue;
					if (world.IsHole((uint32)ay * size + (uint32)ax))
						return true;
				}
				return false;
			}

			float NkGridWorldRisk(const rl::NkGridWorld &world, uint32 state, uint32 action) {
				const uint32 next = NkGridWorldPeek(world, state, action);
				if (world.IsHole(next))
					return 2.0f;
				return NkGridWorldIsRiskyState(world, next) ? 1.0f : 0.0f;
			}

			uint32 NkSelectActionWithPersonality(const rl::NkQLearning &q, const rl::NkGridWorld &world, uint32 state,
												  uint32 numActions, const NkAgentPersonality &personality,
												  uint32 &rngState) {
				const float u = Rand01From(PersonalityRand01Draw(rngState));
				if (u < personality.curiosity) {
					const float u2 = Rand01From(PersonalityRand01Draw(rngState));
					uint32 a = (uint32)(u2 * (float)numActions);
					if (a >= numActions)
						a = numActions - 1;
					return a;
				}

				const float riskScale = (1.0f - personality.boldness) * kRiskWeight;
				float best = q.Q(state, 0) - riskScale * NkGridWorldRisk(world, state, 0);
				uint32 bestA = 0;
				for (uint32 a = 1; a < numActions; ++a) {
					const float score = q.Q(state, a) - riskScale * NkGridWorldRisk(world, state, a);
					if (score > best) {
						best = score;
						bestA = a;
					}
				}
				return bestA;
			}

			uint32 NkPatienceAdjustedMaxSteps(uint32 baseMaxSteps, float patience) {
				if (baseMaxSteps == 0)
					return 0; // illimité : la patience n'a rien à moduler
				if (patience < 0.0f)
					patience = 0.0f;
				uint32 adjusted = (uint32)((float)baseMaxSteps * patience + 0.5f);
				return adjusted < 1u ? 1u : adjusted;
			}

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
