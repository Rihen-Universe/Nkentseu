// =============================================================================
// NKEvolve/NkEvolution.h — moteur d'algorithme génétique (NKAI, Phase 5, Jalon 1).
//
// Boucle générationnelle classique sur des génomes réel-valués :
//   évaluation (fitness) -> élitisme -> sélection (tournoi) -> croisement
//   (arithmétique) -> mutation (gaussienne) -> génération suivante.
// AUCUNE dépendance à un problème particulier : la fonction de fitness est
// fournie par l'appelant (pointeur de fonction + userData, pas de std::function
// — convention zéro-STL du projet). Namespace : nkentseu::ai::evolve.
// =============================================================================
#pragma once

#include "NKEvolve/NkPopulation.h"

namespace nkentseu {
	namespace ai {
		namespace evolve {

			// Signature de la fonction d'adaptation : plus c'est grand, mieux c'est
			// (maximisation). `userData` transporte le contexte du problème (cible à
			// atteindre, environnement à évaluer...) sans imposer de capture/lambda.
			typedef float (*NkFitnessFn)(const NkVector<float> &genes, void *userData);

			struct NkEvolveConfig {
					nk_size populationSize = 50; // nb d'individus par génération
					uint32 geneCount = 4;		  // dimension du génome
					float geneMin = -1.0f;		  // borne basse d'un gène
					float geneMax = 1.0f;		  // borne haute d'un gène
					uint32 tournamentSize = 3;	  // nb de candidats par tournoi de sélection
					float crossoverRate = 0.8f;  // probabilité de croiser deux parents
					float mutationRate = 0.1f;	  // probabilité de mutation PAR GÈNE
					float mutationSigma = 0.2f;  // écart-type du bruit gaussien de mutation
					nk_size elitism = 2;		  // nb des meilleurs individus copiés intacts
					uint32 seed = 1u;			  // graine RNG (déterministe)
			};

			// Statistiques d'une génération (celle qui vient d'être évaluée, AVANT
			// remplacement par la génération suivante).
			struct NkGenerationStats {
					uint32 generation = 0;
					float bestFitness = 0.0f;
					float meanFitness = 0.0f;
			};

			class NkEvolution {
				public:
					// `fitnessFn` ne doit jamais être nul. `userData` est repassé tel quel
					// à chaque appel (le problème à optimiser).
					NkEvolution(const NkEvolveConfig &config, NkFitnessFn fitnessFn, void *userData = nullptr);

					// Évalue tous les individus pas encore évalués de la population COURANTE
					// (idempotent : un individu déjà évalué n'est pas recalculé).
					void Evaluate();

					// Un pas générationnel complet : Evaluate() puis élitisme + sélection +
					// croisement + mutation -> nouvelle génération. Renvoie les statistiques
					// de la génération qui vient d'être évaluée (avant le remplacement).
					NkGenerationStats RunGeneration();

					const NkPopulation &Population() const {
						return mPopulation;
					}

					// Meilleur génome JAMAIS vu (toutes générations confondues), pas
					// seulement celui de la génération courante (le meilleur peut se perdre
					// entre deux générations si l'élitisme est à 0 ou trop petit).
					const NkGenome &BestEver() const {
						return mBestEver;
					}

					uint32 Generation() const {
						return mGeneration;
					}

				private:
					float Rand01();				 // [0,1) uniforme, LCG déterministe
					float RandGauss(float sigma); // N(0, sigma^2), Box-Muller sur Rand01()
					float RandRange(float lo, float hi);

					nk_size TournamentSelect(const NkPopulation &pop);
					void Crossover(const NkVector<float> &a, const NkVector<float> &b, NkVector<float> &childA,
								   NkVector<float> &childB);
					void Mutate(NkVector<float> &genes);

					NkEvolveConfig mConfig;
					NkFitnessFn mFitnessFn;
					void *mUserData;
					NkPopulation mPopulation;
					uint32 mRng;
					uint32 mGeneration;
					NkGenome mBestEver;
					bool mHasBestEver;
			};

		} // namespace evolve
	} // namespace ai
} // namespace nkentseu
