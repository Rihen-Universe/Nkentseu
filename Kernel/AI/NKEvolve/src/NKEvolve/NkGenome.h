// =============================================================================
// NKEvolve/NkGenome.h — génome d'un individu (NKAI, Phase 5, Jalon 1).
//
// Représentation la plus simple qui marche : un vecteur de gènes RÉELS
// (float). Générique — les gènes peuvent encoder n'importe quel problème
// réel-valué (coordonnées d'une fonction à optimiser, hyperparamètres d'une
// politique, poids d'un petit réseau...). Namespace : nkentseu::ai::evolve.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace evolve {

			// Un individu de la population : ses gènes + son adaptation (fitness).
			// `evaluated` évite de recalculer un fitness déjà connu (ex. élites
			// recopiées intactes d'une génération à l'autre).
			struct NkGenome {
					NkVector<float> genes;
					float fitness = 0.0f;
					bool evaluated = false;
			};

		} // namespace evolve
	} // namespace ai
} // namespace nkentseu
