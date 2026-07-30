// =============================================================================
// NKEvolve/NkPopulation.h — population de génomes (NKAI, Phase 5, Jalon 1).
//
// Un simple NkVector<NkGenome> initialisé aléatoirement (RNG déterministe,
// même LCG que rl::NkQLearning/nn::NkDense pour des runs reproductibles),
// plus quelques statistiques (meilleur / moyenne). NkEvolution (moteur
// génétique) manipule cette population génération après génération.
// Namespace : nkentseu::ai::evolve.
// =============================================================================
#pragma once

#include "NKEvolve/NkGenome.h"

namespace nkentseu {
	namespace ai {
		namespace evolve {

			class NkPopulation {
				public:
					// `size` individus, `geneCount` gènes chacun, tirés uniformément dans
					// [geneMin, geneMax]. `seed=0` est remplacé par 1 (comme NkQLearning).
					NkPopulation(nk_size size, uint32 geneCount, float geneMin, float geneMax, uint32 seed = 1u);

					nk_size Size() const {
						return mIndividuals.Size();
					}

					uint32 GeneCount() const {
						return mGeneCount;
					}

					NkGenome &At(nk_size index) {
						return mIndividuals[index];
					}

					const NkGenome &At(nk_size index) const {
						return mIndividuals[index];
					}

					// Remplace le contenu (utilisé par NkEvolution pour passer à la génération
					// suivante) ; ne touche pas à mGeneCount.
					void Replace(NkVector<NkGenome> &&next) {
						mIndividuals = static_cast<NkVector<NkGenome> &&>(next);
					}

					// Index du meilleur individu (fitness la plus haute). Suppose la
					// population déjà évaluée (NkEvolution::Evaluate).
					nk_size BestIndex() const;

					float BestFitness() const {
						return mIndividuals.IsEmpty() ? 0.0f : mIndividuals[BestIndex()].fitness;
					}

					float MeanFitness() const;

				private:
					NkVector<NkGenome> mIndividuals;
					uint32 mGeneCount;
			};

		} // namespace evolve
	} // namespace ai
} // namespace nkentseu
