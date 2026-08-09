// =============================================================================
// NKLlamaBlockTest — le bloc « moderne » (RMSNorm + RoPE + SwiGLU) apprend-il ?
// -----------------------------------------------------------------------------
// Les trois opérations qui le composent sont déjà vérifiées une par une contre
// les différences finies (`NKAutogradTest`). Ce n'est pas suffisant : des
// dérivées justes assemblées de travers donnent un modèle qui n'apprend rien,
// sans que rien ne signale l'erreur. La preuve qu'il manque est donc celle-ci —
// **le bloc SUR-APPREND une séquence connue**, c'est-à-dire que la chaîne
// complète avant + arrière + optimiseur boucle correctement.
//
// On compare au passage avec `NkGPT` (LayerNorm + positions apprises + GELU) sur
// EXACTEMENT la même tâche, le même budget et la même graine. Le but n'est pas
// de désigner un vainqueur — la tâche est un jouet, et un jouet ne départage
// rien — mais de vérifier que le nouveau bloc n'est pas hors-jeu.
//
// ⚠️ CPU STRICT, par construction : ce test ne crée AUCUN device GPU. Une seule
// carte dans la machine, et un entraînement long peut l'occuper ; un second
// contexte Vulkan ne renverrait aucune erreur, il déborderait en mémoire système
// et rendrait n'importe quoi.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0;
static int g_fail = 0;

static void Verdict(bool ok, const char *quoi) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", quoi);
}

// Séquence à retenir : un motif court et répétitif, donc apprenable par un tout
// petit modèle, mais pas trivial (le même jeton revient à des positions
// différentes, ce qui oblige à se servir du contexte).
static const int32 kVocab = 12;
static const int32 kT = 24;
static int32 gSeq[kT + 1];

static void ConstruireSequence() {
	// 0 1 2 3 0 1 2 4 0 1 2 5 ... : le 4e jeton dépend du groupe, donc de la
	// position ; un modèle sans contexte ne peut pas le deviner.
	int k = 0;
	for (int g = 0; g < 100 && k <= kT; ++g) {
		const int motif[4] = {0, 1, 2, 3 + (g % (kVocab - 3))};
		for (int i = 0; i < 4 && k <= kT; ++i)
			gSeq[k++] = (int32)motif[i];
	}
}

// Entrée x[1,T] = jetons 0..T-1, cible = jetons 1..T (prédire le suivant).
static void FabriquerLot(NkTensor &x, NkTensor &cible) {
	x = NkTensor::Zeros(NkShape{(int64)1, (int64)kT});
	cible = NkTensor::Zeros(NkShape{(int64)kT});
	float *xp = x.DataAs<float>();
	float *cp = cible.DataAs<float>();
	for (int t = 0; t < kT; ++t) {
		xp[t] = (float)gSeq[t];
		cp[t] = (float)gSeq[t + 1];
	}
}

// Boucle d'entraînement générique sur n'importe quel modèle exposant
// Forward(NkTensor)->logits et Parameters().
template <typename M> static double SurApprendre(M &modele, const char *nom, int pas, double lr) {
	NkVector<NkVar> params;
	modele.Parameters(params);
	optim::NkAdam adam(params, (float)lr);
	NkTensor x, cible;
	FabriquerLot(x, cible);
	double premiere = 0.0, derniere = 0.0;
	for (int s = 1; s <= pas; ++s) {
		adam.ZeroGrad();
		NkVar logits = modele.Forward(x);
		NkVar perte = autograd::SoftmaxCrossEntropyIndexed(logits, NkVar::Leaf(cible, false));
		perte.Backward();
		adam.Step();
		derniere = perte.Value().GetItem(NkShape{(int64)0});
		if (s == 1)
			premiere = derniere;
		if (s == 1 || s % 50 == 0 || s == pas)
			printf("    %-6s pas %4d : perte = %.5f\n", nom, s, derniere);
	}
	// La perte de depart est SUPERIEURE a ln(vocabulaire) = %.4f : ce plancher ne
	// vaut que pour des logits uniformes, or la tete de sortie demarre avec une
	// initialisation de Xavier, pas a zero. Ce qui compte n'est donc pas la
	// valeur de depart mais la distance parcourue.
	printf("    %-6s : %.4f -> %.4f  (ln(%d) = %.4f, plancher d'un tirage uniforme)\n", nom, premiere, derniere,
		   kVocab, std::log((double)kVocab));
	return derniere;
}

// Exactitude de prédiction du jeton suivant sur la séquence apprise.
template <typename M> static double Exactitude(M &modele) {
	NkTensor x, cible;
	FabriquerLot(x, cible);
	NkVar logits = modele.Forward(x);
	NkTensor lg = logits.Value().ToCPU().Contiguous();
	const float *lp = lg.DataAs<float>();
	const float *cp = cible.DataAs<float>();
	int bons = 0;
	for (int t = 0; t < kT; ++t) {
		int arg = 0;
		float best = lp[(int64)t * kVocab];
		for (int v = 1; v < kVocab; ++v)
			if (lp[(int64)t * kVocab + v] > best) {
				best = lp[(int64)t * kVocab + v];
				arg = v;
			}
		if (arg == (int)cp[t])
			++bons;
	}
	return (double)bons / (double)kT;
}

int main() {
	setvbuf(stdout, nullptr, _IONBF, 0);
	printf("=== NKLlamaBlockTest — RMSNorm + RoPE + SwiGLU assembles ===\n\n");
	printf("  CPU strict : aucun device GPU n'est cree (le GPU peut etre pris par un entrainement).\n\n");
	ConstruireSequence();

	const uint32 d = 32, tetes = 4, couches = 2;
	const int pas = 300;
	const double lr = 3e-3;

	printf("  Modele : vocabulaire %d, T=%d, d=%u, %u tetes, %u couches\n\n", kVocab, kT, d, tetes, couches);

	printf("  -- 1. Bloc moderne (RMSNorm + RoPE + SwiGLU) --\n");
	nn::NkLlamaLM llama(kVocab, d, tetes, couches, 1234u);
	const double perteLlama = SurApprendre(llama, "llama", pas, lr);
	const double accLlama = Exactitude(llama);
	printf("    llama  : exactitude sur la sequence apprise = %.1f%%\n\n", accLlama * 100.0);

	printf("  -- 2. Bloc historique (LayerNorm + positions apprises + GELU) --\n");
	nn::NkGPT gpt(kVocab, d, tetes, couches, (uint32)kT, 1234u);
	const double perteGpt = SurApprendre(gpt, "gpt", pas, lr);
	const double accGpt = Exactitude(gpt);
	printf("    gpt    : exactitude sur la sequence apprise = %.1f%%\n\n", accGpt * 100.0);

	printf("  == RESULTAT ==\n");
	// Ce qui est EXIGE : que le nouveau bloc apprenne vraiment. Une perte qui
	// descend loin sous ln(vocabulaire) prouve que le gradient traverse
	// reellement RMSNorm, RoPE et SwiGLU jusqu'aux poids.
	Verdict(perteLlama < 0.05, "le bloc moderne sur-apprend la sequence (perte < 0,05)");
	Verdict(accLlama > 0.99, "il predit le jeton suivant sans faute");
	Verdict(perteGpt < 0.05, "le bloc historique en fait autant (non-regression)");
	// Ce qui n'est PAS exige : que l'un batte l'autre. La tache est un jouet.
	printf("  (perte finale : moderne %.5f, historique %.5f — sur un jouet, cet ecart ne\n"
		   "   departage rien et ne doit pas etre presente comme une comparaison.)\n",
		   perteLlama, perteGpt);

	printf("\n  Resultat : %d OK, %d echec(s)\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
