// =============================================================================
// NKDataTest — vérifie NKData : Dataset synthétique -> DataLoader (shuffle,
// batchs, one-hot), couverture complète, puis chargeur MNIST si disponible
// (variable d'environnement NK_MNIST_DIR).
// =============================================================================
#include "NKData/NkData.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

int main() {
	printf("=== NKDataTest : Dataset + DataLoader (+ MNIST si dispo) ===\n\n");

	// Jeu synthétique : 3 classes x 20 = 60 exemples 2D.
	const uint32 NC = 3, PER = 20, N = NC * PER;
	data::NkDataset ds = data::MakeBlobs(NC, PER, 123u);
	Check(ds.IsValid() && ds.Size() == N && ds.FeatureDim() == 2 && ds.NumClasses() == NC,
		  "Dataset synthétique (60 exemples, 2D, 3 classes)");

	// Histogramme attendu (par classe) depuis le dataset.
	int expected[16] = {0};
	for (uint32 i = 0; i < ds.Size(); ++i)
		expected[ds.Labels()[i]]++;

	// DataLoader : batch=16, shuffle -> 4 lots (dont un partiel de 12).
	const uint32 BS = 16;
	data::NkDataLoader loader(ds, BS, /*shuffle*/ true, /*seed*/ 7u);
	Check(loader.NumBatches() == (N + BS - 1) / BS, "NumBatches = ceil(60/16) = 4");

	// Parcourt tous les lots : compte total + histogramme + validité one-hot.
	int actual[16] = {0};
	uint32 seen = 0;
	bool oneHotOk = true, shapeOk = true;
	for (uint32 b = 0; b < loader.NumBatches(); ++b) {
		data::NkBatch batch = loader.GetBatch(b);
		seen += batch.size;
		if ((uint32)batch.inputs.Shape()[0] != batch.size || (uint32)batch.inputs.Shape()[1] != ds.FeatureDim() ||
			(uint32)batch.targets.Shape()[1] != ds.NumClasses())
			shapeOk = false;
		const float *tgt = batch.targets.Contiguous().DataAs<float>();
		for (uint32 k = 0; k < batch.size; ++k) {
			actual[batch.labels[k]]++;
			// La ligne one-hot doit sommer à 1 et avoir son 1 sur la bonne classe.
			double sum = 0.0;
			int hot = -1;
			for (uint32 c = 0; c < ds.NumClasses(); ++c) {
				float v = tgt[k * ds.NumClasses() + c];
				sum += v;
				if (v > 0.5f)
					hot = (int)c;
			}
			if (hot != batch.labels[k] || sum < 0.999 || sum > 1.001)
				oneHotOk = false;
		}
	}
	Check(seen == N, "Couverture : tous les exemples exactement une fois (60)");
	bool histOk = true;
	for (uint32 c = 0; c < NC; ++c)
		if (actual[c] != expected[c])
			histOk = false;
	Check(histOk, "Histogramme par classe préservé (pas de perte/doublon)");
	Check(shapeOk, "Formes des lots correctes ([B,D] / [B,C])");
	Check(oneHotOk, "One-hot correct (somme=1, sur la bonne classe)");

	// Le shuffle change l'ordre d'une époque à l'autre.
	data::NkBatch b0 = loader.GetBatch(0);
	loader.Shuffle();
	data::NkBatch b0b = loader.GetBatch(0);
	bool changed = false;
	uint32 m = b0.size < b0b.size ? b0.size : b0b.size;
	for (uint32 k = 0; k < m; ++k)
		if (b0.labels[k] != b0b.labels[k]) {
			changed = true;
			break;
		}
	Check(changed, "Shuffle() remélange l'ordre entre deux époques");

	// ------------------------------------------------------------------
	// MNIST (optionnel) : défini NK_MNIST_DIR pointant vers les 4 fichiers IDX
	// décompressés (train-images-idx3-ubyte, train-labels-idx1-ubyte).
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
			printf("  MNIST introuvable/illisible dans %s (fichiers IDX décompressés ?)\n", dir);
		} else {
			printf("  MNIST chargé : %u exemples, %u features (28x28), %u classes\n", mnist.Size(), mnist.FeatureDim(),
				   mnist.NumClasses());
			data::NkDataLoader ml(mnist, 64, true, 1u);
			printf("  DataLoader MNIST : %u lots de 64\n", ml.NumBatches());
			Check(mnist.FeatureDim() == 784 && mnist.NumClasses() == 10, "MNIST : 784 features, 10 classes");
		}
	}

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
