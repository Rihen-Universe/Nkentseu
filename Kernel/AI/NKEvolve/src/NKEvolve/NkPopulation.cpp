// =============================================================================
// NKEvolve/NkPopulation.cpp — implémentation de la population (NKAI, Phase 5).
// =============================================================================
#include "NKEvolve/NkPopulation.h"

namespace nkentseu {
	namespace ai {
		namespace evolve {

			namespace {

				// LCG déterministe (même constantes que rl::NkQLearning::Rand01 et
				// nn::NkDense) : runs reproductibles à graine égale.
				float Lcg01(uint32 &state) {
					state = state * 1664525u + 1013904223u;
					return (float)((state >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
				}

			} // namespace

			NkPopulation::NkPopulation(nk_size size, uint32 geneCount, float geneMin, float geneMax, uint32 seed)
				: mGeneCount(geneCount) {
				uint32 rng = seed ? seed : 1u;
				const float lo = geneMin < geneMax ? geneMin : geneMax;
				const float hi = geneMin < geneMax ? geneMax : geneMin;
				const float span = hi - lo;

				mIndividuals.Resize(size ? size : 1);
				for (nk_size i = 0; i < mIndividuals.Size(); ++i) {
					NkGenome &g = mIndividuals[i];
					g.genes.Resize(geneCount);
					for (uint32 k = 0; k < geneCount; ++k)
						g.genes[k] = lo + Lcg01(rng) * span;
					g.fitness = 0.0f;
					g.evaluated = false;
				}
			}

			nk_size NkPopulation::BestIndex() const {
				nk_size best = 0;
				float bv = mIndividuals.IsEmpty() ? 0.0f : mIndividuals[0].fitness;
				for (nk_size i = 1; i < mIndividuals.Size(); ++i)
					if (mIndividuals[i].fitness > bv) {
						bv = mIndividuals[i].fitness;
						best = i;
					}
				return best;
			}

			float NkPopulation::MeanFitness() const {
				if (mIndividuals.IsEmpty())
					return 0.0f;
				double sum = 0.0;
				for (nk_size i = 0; i < mIndividuals.Size(); ++i)
					sum += (double)mIndividuals[i].fitness;
				return (float)(sum / (double)mIndividuals.Size());
			}

		} // namespace evolve
	} // namespace ai
} // namespace nkentseu
