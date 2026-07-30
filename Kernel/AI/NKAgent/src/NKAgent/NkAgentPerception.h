// =============================================================================
// NKAgent/NkAgentPerception.h — perception (le présent) (NKAI, Phase 4).
//
// Transforme l'état BRUT d'un environnement NKRL (un simple indice, cf.
// rl::NkEnvironment) en un vecteur de FEATURES exploitable par l'agent :
// position normalisée + direction vers le but. Première implémentation :
// spécialisée pour rl::NkGridWorld (le seul environnement NKRL livré à ce
// jour, cf. Kernel/AI/NKRL). ⚠️ Pas encore d'interface d'encodage générique
// par-dessus rl::NkEnvironment : à généraliser quand un 2e environnement
// existera (Jalon 1 de la ROADMAP NKAgent). Jalon 3 : constructeur générique
// ajouté (taille+but en primitives) pour les agents pilotés par une pile de
// buts (NkGoalStack) sur un environnement quelconque, sans coupler cette
// classe à un type d'environnement concret supplémentaire.
// Namespace : nkentseu::ai::agent.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRL/NkGridWorld.h"

namespace nkentseu {
	namespace ai {
		namespace agent {

			class NkAgentPerception {
				public:
					explicit NkAgentPerception(const rl::NkGridWorld &world);

					// Constructeur générique (Jalon 3) : découplé d'un rl::NkGridWorld
					// concret — pour les agents pilotés par une pile de buts
					// (NkGoalStack) sur un environnement quelconque (ex.
					// rl::NkKeyDoorGridWorld). `goalX/goalY` = le point de repère
					// affiché par Encode() (le but "final" par convention, purement
					// informatif : la décision Jalon 3 passe par des politiques par
					// but, pas par ces features).
					NkAgentPerception(uint32 size, uint32 goalX, uint32 goalY);

					// Dimension du vecteur de features produit par Encode().
					uint32 FeatureDim() const {
						return 4;
					}

					// Encode un état brut (indice y*size+x) en features :
					//   [0] x normalisé            dans [0,1]
					//   [1] y normalisé            dans [0,1]
					//   [2] dx signé vers le but    dans [-1,1]
					//   [3] dy signé vers le but    dans [-1,1]
					void Encode(uint32 rawState, NkVector<float> &outFeatures) const;

				private:
					uint32 mSize;
					uint32 mGoalX;
					uint32 mGoalY;
			};

		} // namespace agent
	} // namespace ai
} // namespace nkentseu
