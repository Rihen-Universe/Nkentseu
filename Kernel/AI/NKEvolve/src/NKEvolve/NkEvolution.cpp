// =============================================================================
// NKEvolve/NkEvolution.cpp — implémentation du moteur génétique (NKAI, Phase 5).
// =============================================================================
#include "NKEvolve/NkEvolution.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace ai {
		namespace evolve {

			namespace {

				float ClampF(float v, float lo, float hi) {
					if (v < lo)
						return lo;
					if (v > hi)
						return hi;
					return v;
				}

			} // namespace

			NkEvolution::NkEvolution(const NkEvolveConfig &config, NkFitnessFn fitnessFn, void *userData)
				: mConfig(config), mFitnessFn(fitnessFn), mUserData(userData),
				  mPopulation(config.populationSize, config.geneCount, config.geneMin, config.geneMax, config.seed),
				  mRng(config.seed ? config.seed * 2654435761u + 1u : 1u), // décorrélée de la graine de NkPopulation
				  mGeneration(0), mHasBestEver(false) {
			}

			float NkEvolution::Rand01() {
				mRng = mRng * 1664525u + 1013904223u;
				return (float)((mRng >> 8) & 0xFFFFFFu) / (float)0x1000000u; // [0,1)
			}

			float NkEvolution::RandRange(float lo, float hi) {
				return lo + Rand01() * (hi - lo);
			}

			float NkEvolution::RandGauss(float sigma) {
				// Box-Muller (forme polaire simplifiée) : u1 dans (0,1] pour éviter log(0).
				float u1 = 1.0f - Rand01(); // (0,1]
				float u2 = Rand01();		// [0,1)
				float mag = math::NkSqrt(-2.0f * math::NkLog(u1));
				return mag * math::NkCos(2.0f * 3.14159265358979323846f * u2) * sigma;
			}

			void NkEvolution::Evaluate() {
				for (nk_size i = 0; i < mPopulation.Size(); ++i) {
					NkGenome &g = mPopulation.At(i);
					if (g.evaluated)
						continue;
					g.fitness = mFitnessFn(g.genes, mUserData);
					g.evaluated = true;
				}
			}

			nk_size NkEvolution::TournamentSelect(const NkPopulation &pop) {
				nk_size best = (nk_size)(Rand01() * (float)pop.Size());
				if (best >= pop.Size())
					best = pop.Size() - 1;
				float bestFit = pop.At(best).fitness;
				const uint32 rounds = mConfig.tournamentSize > 1 ? mConfig.tournamentSize - 1 : 0;
				for (uint32 k = 0; k < rounds; ++k) {
					nk_size cand = (nk_size)(Rand01() * (float)pop.Size());
					if (cand >= pop.Size())
						cand = pop.Size() - 1;
					if (pop.At(cand).fitness > bestFit) {
						bestFit = pop.At(cand).fitness;
						best = cand;
					}
				}
				return best;
			}

			void NkEvolution::Crossover(const NkVector<float> &a, const NkVector<float> &b, NkVector<float> &childA,
										 NkVector<float> &childB) {
				const uint32 n = mConfig.geneCount;
				childA.Resize(n);
				childB.Resize(n);
				if (Rand01() < mConfig.crossoverRate) {
					// Recombinaison arithmétique complète : un seul alpha pour tout le
					// génome (chaque enfant = mélange des deux parents).
					float alpha = Rand01();
					for (uint32 i = 0; i < n; ++i) {
						childA[i] = alpha * a[i] + (1.0f - alpha) * b[i];
						childB[i] = alpha * b[i] + (1.0f - alpha) * a[i];
					}
				} else {
					// Pas de croisement : les enfants sont des copies des parents.
					for (uint32 i = 0; i < n; ++i) {
						childA[i] = a[i];
						childB[i] = b[i];
					}
				}
			}

			void NkEvolution::Mutate(NkVector<float> &genes) {
				for (uint32 i = 0; i < genes.Size(); ++i) {
					if (Rand01() < mConfig.mutationRate) {
						genes[i] = ClampF(genes[i] + RandGauss(mConfig.mutationSigma), mConfig.geneMin, mConfig.geneMax);
					}
				}
			}

			NkGenerationStats NkEvolution::RunGeneration() {
				Evaluate();

				NkGenerationStats stats;
				stats.generation = mGeneration;
				stats.bestFitness = mPopulation.BestFitness();
				stats.meanFitness = mPopulation.MeanFitness();

				// --- Mémorise le meilleur individu JAMAIS vu (toutes générations). ---
				const nk_size bestIdx = mPopulation.BestIndex();
				const NkGenome &curBest = mPopulation.At(bestIdx);
				if (!mHasBestEver || curBest.fitness > mBestEver.fitness) {
					mBestEver = curBest;
					mHasBestEver = true;
				}

				// --- Construit la génération suivante. ---
				NkVector<NkGenome> next;
				next.Reserve(mPopulation.Size());

				// Élitisme : les K meilleurs passent intacts (recherche du max répétée,
				// K est petit — élitism=2..5 typiquement, coût négligeable).
				nk_size eliteCount = mConfig.elitism < mPopulation.Size() ? mConfig.elitism : mPopulation.Size();
				NkVector<bool> taken;
				taken.Resize(mPopulation.Size(), false);
				for (nk_size e = 0; e < eliteCount; ++e) {
					nk_size best = mPopulation.Size();
					float bestFit = 0.0f;
					for (nk_size i = 0; i < mPopulation.Size(); ++i) {
						if (taken[i])
							continue;
						if (best == mPopulation.Size() || mPopulation.At(i).fitness > bestFit) {
							best = i;
							bestFit = mPopulation.At(i).fitness;
						}
					}
					if (best == mPopulation.Size())
						break;
					taken[best] = true;
					next.PushBack(mPopulation.At(best)); // copie : genes + fitness + evaluated=true
				}

				// Reste de la population : sélection par tournoi + croisement + mutation.
				while (next.Size() < mPopulation.Size()) {
					nk_size ia = TournamentSelect(mPopulation);
					nk_size ib = TournamentSelect(mPopulation);
					NkVector<float> childA, childB;
					Crossover(mPopulation.At(ia).genes, mPopulation.At(ib).genes, childA, childB);
					Mutate(childA);

					NkGenome ga;
					ga.genes = childA;
					ga.fitness = 0.0f;
					ga.evaluated = false;
					next.PushBack(ga);

					if (next.Size() < mPopulation.Size()) {
						Mutate(childB);
						NkGenome gb;
						gb.genes = childB;
						gb.fitness = 0.0f;
						gb.evaluated = false;
						next.PushBack(gb);
					}
				}

				mPopulation.Replace(static_cast<NkVector<NkGenome> &&>(next));
				++mGeneration;
				return stats;
			}

		} // namespace evolve
	} // namespace ai
} // namespace nkentseu
