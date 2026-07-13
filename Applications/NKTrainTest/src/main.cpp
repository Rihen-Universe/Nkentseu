// =============================================================================
// NKTrainTest — entraînement de bout en bout via NKTrain.
//   Modèle Dense(2->32)->relu->Dense(32->4) entraîné sur 4 amas 2D (NKData) avec
//   Adam (NKOptim) + entropie croisée (NKNN), boucle TrainEpoch (NKTrain).
//   Suivi perte + exactitude par époque ; exactitude sur un jeu de TEST séparé.
//   Section MNIST optionnelle (NK_MNIST_DIR).
// =============================================================================
#include "NKTrain/NkTrain.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cstdlib>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
	printf("=== NKTrainTest : entraînement de bout en bout (NKTrain) ===\n\n");

	// Jeux train (4x40=160) et test (4x15=60), amas 2D, graines différentes.
	const uint32 NC = 4;
	data::NkDataset train = data::MakeBlobs(NC, 40, 111u);
	data::NkDataset test = data::MakeBlobs(NC, 15, 999u);
	data::NkDataLoader trainLoader(train, 32, /*shuffle*/ true, 7u);
	data::NkDataLoader testLoader(test, 32, /*shuffle*/ false, 1u);

	// Modèle : Dense(2->32) -> relu -> Dense(32->4) (logits).
	nn::NkDense l1(2, 32, 1234u);
	nn::NkDense l2(32, NC, 5678u);
	auto forward = [&](const NkVar &x) { return l2.Forward(nn::Relu(l1.Forward(x))); };

	NkVector<NkVar> params;
	l1.Parameters(params);
	l2.Parameters(params);
	optim::NkAdam adam(params, /*lr*/ 0.01f);

	printf("-- Entraînement (4 classes, 160 exemples, Adam+CE) --\n");
	train::EpochStats st;
	for (int e = 1; e <= 60; ++e) {
		st = train::TrainEpoch(forward, adam, trainLoader);
		if (e % 10 == 0 || e == 1)
			printf("  époque %3d : perte = %.5f  exactitude train = %.1f%%\n", e, st.loss, st.acc * 100.0);
	}

	const double testAcc = train::Accuracy(forward, testLoader);
	printf("\n  exactitude finale : train = %.1f%%  |  TEST (jamais vu) = %.1f%%\n", st.acc * 100.0, testAcc * 100.0);

	int pass = 0, fail = 0;
	bool trainOk = st.acc >= 0.98;
	bool testOk = testAcc >= 0.90;
	(trainOk ? pass : fail)++;
	(testOk ? pass : fail)++;
	printf("  [ %s ] convergence entraînement (>=98%%)\n", trainOk ? "OK" : "KO");
	printf("  [ %s ] généralisation test (>=90%%)\n", testOk ? "OK" : "KO");

	// ------------------------------------------------------------------
	// Utilitaires réutilisables (Option A.1) : planificateur LR + époque
	// avec accumulation de gradient + validation forward-only.
	// ------------------------------------------------------------------
	printf("\n-- Utilitaires réutilisables (scheduler LR, accumulation, validation) --\n");

	// (a) Auto-test analytique du planificateur LR (warmup, pic, plancher, monotonie).
	const bool schedOk = train::SelfTest();
	(schedOk ? pass : fail)++;
	printf("  [ %s ] planificateur LR (warmup linéaire + cosine + plancher)\n", schedOk ? "OK" : "KO");

	// (b) Entraînement d'un modèle NEUF via TrainEpochAccum (accum=2) piloté par un
	//     scheduler LR global (warmup court + décroissance cosine). Doit converger.
	nn::NkDense a1(2, 32, 4321u);
	nn::NkDense a2(32, NC, 8765u);
	auto afwd = [&](const NkVar &x) { return a2.Forward(nn::Relu(a1.Forward(x))); };
	NkVector<NkVar> aparams;
	a1.Parameters(aparams);
	a2.Parameters(aparams);
	optim::NkAdam aopt(aparams, /*lr*/ 0.01f);

	const int ACC_EPOCHS = 60;
	train::NkLRSchedule sched;
	sched.peakLr = 0.01f;
	sched.warmupSteps = trainLoader.NumBatches() * 3; // ~3 époques de warmup
	sched.totalSteps = trainLoader.NumBatches() * ACC_EPOCHS;
	sched.minLrRatio = 0.1;
	int64 gstep = 0;

	train::EpochStats ast;
	double valLoss = 0.0;
	for (int e = 1; e <= ACC_EPOCHS; ++e) {
		ast = train::TrainEpochAccum(afwd, aopt, trainLoader, /*accum*/ 2, &sched, &gstep);
		if (e % 15 == 0 || e == 1) {
			valLoss = train::EvalLoss(afwd, testLoader);
			printf("  époque %3d : perte train = %.5f  exactitude = %.1f%%  perte val = %.5f  lr = %.5f\n",
				   e, ast.loss, ast.acc * 100.0, valLoss, (double)sched.LrAt(gstep));
		}
	}
	const double accTestAcc = train::Accuracy(afwd, testLoader);
	const bool accOk = ast.acc >= 0.95 && accTestAcc >= 0.85;
	(accOk ? pass : fail)++;
	printf("  [ %s ] convergence avec accumulation+scheduler (train %.1f%% / test %.1f%%)\n",
		   accOk ? "OK" : "KO", ast.acc * 100.0, accTestAcc * 100.0);

	// ------------------------------------------------------------------
	// MNIST (optionnel) : entraînement réel si NK_MNIST_DIR est défini.
	// ------------------------------------------------------------------
	printf("\n-- MNIST (optionnel) --\n");
	const char *dir = getenv("NK_MNIST_DIR");
	if (!dir || !*dir) {
		printf("  (NK_MNIST_DIR non défini : section MNIST ignorée)\n");
	} else {
		char img[1024], lbl[1024];
		snprintf(img, sizeof(img), "%s/train-images-idx3-ubyte", dir);
		snprintf(lbl, sizeof(lbl), "%s/train-labels-idx1-ubyte", dir);
		data::NkDataset mnist = data::LoadMnist(img, lbl);
		if (!mnist.IsValid()) {
			printf("  MNIST introuvable dans %s (section ignorée)\n", dir);
		} else {
			printf("  MNIST : %u exemples ; entraînement d'un MLP 784->64->10 (Adam+CE)...\n", mnist.Size());
			data::NkDataLoader ml(mnist, 64, true, 3u);
			nn::NkDense m1(784, 64, 11u);
			nn::NkDense m2(64, 10, 22u);
			auto mfwd = [&](const NkVar &x) { return m2.Forward(nn::Relu(m1.Forward(x))); };
			NkVector<NkVar> mp;
			m1.Parameters(mp);
			m2.Parameters(mp);
			optim::NkAdam mopt(mp, 0.001f);
			for (int e = 1; e <= 3; ++e) {
				train::EpochStats ms = train::TrainEpoch(mfwd, mopt, ml);
				printf("    époque %d : perte = %.4f  exactitude = %.1f%%\n", e, ms.loss, ms.acc * 100.0);
			}
		}
	}

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
	return fail == 0 ? 0 : 1;
}
