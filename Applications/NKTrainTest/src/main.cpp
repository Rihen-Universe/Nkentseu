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
