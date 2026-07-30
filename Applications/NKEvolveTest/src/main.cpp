// =============================================================================
// NKEvolveTest — une POPULATION progresse au fil des générations par sélection
// + croisement + mutation, sans aucun gradient (NKEvolve, Phase 5, Jalon 1).
//
// Problème choisi : faire évoluer un génome de 6 gènes réels pour qu'il
// s'approche d'un vecteur CIBLE fixe (fitness = 1/(1+distance^2), maximum = 1
// à la cible exacte). Volontairement un problème JOUET plutôt qu'une tâche
// d'agent (cf. NKAgent/NKRL) : c'est le même moteur générationnel générique
// (NkEvolution) qui ferait évoluer des hyperparamètres de politique ou des
// poids de réseau — la fonction de fitness est un simple pointeur de fonction,
// interchangeable sans toucher au moteur. Preuve demandée : le fitness MOYEN
// de la population progresse génération après génération.
// =============================================================================
#include "NKEvolve/NkEvolve.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::ai;

namespace {

	// Vecteur cible fixe (6 dimensions), dans les bornes du génome [-5,5].
	const float kTarget[6] = {2.5f, -1.5f, 4.0f, -3.0f, 0.5f, -4.5f};
	const uint32 kGeneCount = 6;

	// Fitness = 1 / (1 + distance_euclidienne^2 à la cible). Maximum = 1.0
	// (atteint uniquement si genes == target). userData inutilisé ici (la
	// cible est un compile-time const), mais la signature l'accepte pour
	// rester générique (un autre problème y passerait son propre contexte).
	float TargetMatchFitness(const NkVector<float> &genes, void * /*userData*/) {
		float sumSq = 0.0f;
		for (uint32 i = 0; i < kGeneCount; ++i) {
			float d = genes[i] - kTarget[i];
			sumSq += d * d;
		}
		return 1.0f / (1.0f + sumSq);
	}

} // namespace

int main() {
	logger.Info("=== NKEvolveTest : une population evolue (selection+croisement+mutation) vers un meilleur fitness ===");

	evolve::NkEvolveConfig config;
	config.populationSize = 80;
	config.geneCount = kGeneCount;
	config.geneMin = -5.0f;
	config.geneMax = 5.0f;
	config.tournamentSize = 3;
	config.crossoverRate = 0.8f;
	config.mutationRate = 0.15f;
	config.mutationSigma = 0.4f;
	config.elitism = 4;
	config.seed = 42u;

	evolve::NkEvolution ga(config, TargetMatchFitness, nullptr);

	const uint32 generations = 200;
	logger.Info("-- Evolution ({0} generations, population={1}, genes={2}) --", generations, config.populationSize,
				config.geneCount);

	float firstMean = 0.0f;
	float lastBest = 0.0f;
	float lastMean = 0.0f;
	for (uint32 gen = 0; gen < generations; ++gen) {
		evolve::NkGenerationStats stats = ga.RunGeneration();
		if (gen == 0)
			firstMean = stats.meanFitness;
		lastBest = stats.bestFitness;
		lastMean = stats.meanFitness;

		if (gen % 20 == 0 || gen == generations - 1) {
			logger.Info("  generation {0} : fitness moyen={1} meilleur={2}", stats.generation, (double)stats.meanFitness,
						(double)stats.bestFitness);
		}
	}

	logger.Info("-- Progression : fitness moyen {0} -> {1} (delta={2}) --", (double)firstMean, (double)lastMean,
				(double)(lastMean - firstMean));

	// Le meilleur génome jamais vu (toutes générations, grâce à l'élitisme +
	// suivi explicite) doit être proche de la cible.
	const evolve::NkGenome &best = ga.BestEver();
	float maxAbsErr = 0.0f;
	for (uint32 i = 0; i < kGeneCount; ++i) {
		float err = best.genes[i] - kTarget[i];
		if (err < 0.0f)
			err = -err;
		if (err > maxAbsErr)
			maxAbsErr = err;
	}
	logger.Info("  meilleur genome jamais vu : fitness={0} (evaluations={1})", (double)best.fitness, ga.Generation());
	for (uint32 i = 0; i < kGeneCount; ++i)
		logger.Info("    gene[{0}] = {1}  (cible = {2})", i, (double)best.genes[i], (double)kTarget[i]);

	// Critère de succès : progrès net du fitness moyen de la population ET le
	// meilleur génome approche la cible à moins de 0.5 par composante.
	const bool progressed = lastMean > firstMean + 0.05f;
	const bool converged = maxAbsErr < 0.5f;
	const bool ok = progressed && converged;

	logger.Info("[ {0} ] population progresse (moyenne {1} -> {2}) ET meilleur genome converge (erreur max={3})",
				ok ? "OK" : "KO", (double)firstMean, (double)lastMean, (double)maxAbsErr);
	logger.Info("=== Resultat : {0} OK, {1} echec(s) ===", ok ? 1 : 0, ok ? 0 : 1);
	return ok ? 0 : 1;
}
