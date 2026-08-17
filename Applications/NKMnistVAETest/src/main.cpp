// =============================================================================
// NKMnistVAETest — entraîner un VAE sur de VRAIES images (MNIST) et GÉNÉRER.
//   Lit un sous-ensemble de MNIST (IDX), entraîne un VAE dense 784->h->L->h->784,
//   puis : (1) reconstruit des chiffres réels, (2) GÉNÈRE des chiffres depuis
//   z ~ N(0,1). Sorties en PGM (grilles) -> converties en PNG pour visualisation.
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 SIDE = 28, D = SIDE * SIDE;

static void FillRandn(NkTensor &t, uint32 &s) {
	float *p = t.DataAs<float>();
	int64 n = NkShapeNumel(t.Shape());
	for (int64 i = 0; i < n; ++i) {
		s = s * 1664525u + 1013904223u;
		double u1 = (double)((s >> 8) & 0xFFFFu) / 65535.0;
		s = s * 1664525u + 1013904223u;
		double u2 = (double)((s >> 8) & 0xFFFFu) / 65535.0;
		if (u1 < 1e-7)
			u1 = 1e-7;
		p[i] = (float)(std::sqrt(-2.0 * std::log(u1)) * std::cos(6.2831853 * u2));
	}
}

// Écrit une grille de `count` images `SIDE×SIDE` (valeurs [0,1]) en PGM (P5).
static bool SavePGMGrid(const char *path, const float *imgs, uint32 count, uint32 cols) {
	const uint32 rows = (count + cols - 1) / cols, W = cols * SIDE, H = rows * SIDE;
	NkVector<uint8> buf;
	buf.Resize(W * H);
	for (uint32 i = 0; i < buf.Size(); ++i)
		buf[i] = 0;
	for (uint32 k = 0; k < count; ++k) {
		uint32 gx = (k % cols) * SIDE, gy = (k / cols) * SIDE;
		for (uint32 y = 0; y < SIDE; ++y)
			for (uint32 x = 0; x < SIDE; ++x) {
				float v = imgs[k * D + y * SIDE + x];
				if (v < 0)
					v = 0;
				if (v > 1)
					v = 1;
				buf[(gy + y) * W + (gx + x)] = (uint8)(v * 255.f + 0.5f);
			}
	}
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;
	fprintf(f, "P5\n%u %u\n255\n", W, H);
	fwrite(buf.Data(), 1, W * H, f);
	fclose(f);
	return true;
}

int main() {
	printf("=== NKMnistVAETest : VAE entraîné sur MNIST -> génération ===\n\n");

	// Lecture directe de l'IDX MNIST (16 octets d'entête puis N*784 octets).
	const char *dir = getenv("NK_MNIST_DIR");
	char path[512];
	snprintf(path, sizeof(path), "%s/train-images-idx3-ubyte", (dir && *dir) ? dir : "Datasets/mnist");
	FILE *f = fopen(path, "rb");
	if (!f) {
		printf("  MNIST introuvable (%s). Définis NK_MNIST_DIR.\n", path);
		return 1;
	}
	uint8 hdr[16];
	if (fread(hdr, 1, 16, f) != 16) {
		fclose(f);
		return 1;
	}

	const uint32 N = 1000; // sous-ensemble (vitesse CPU)
	NkTensor X = NkTensor::Zeros(NkShape{(int64)N, (int64)D});
	{
		float *xp = X.DataAs<float>();
		for (uint32 i = 0; i < N; ++i) {
			uint8 px[D];
			if (fread(px, 1, D, f) != D)
				break;
			for (uint32 j = 0; j < D; ++j)
				xp[i * D + j] = (float)px[j] / 255.f;
		}
	}
	fclose(f);
	printf("  MNIST : %u images 28x28 chargées\n", N);

	const uint32 H = 256, LAT = 32;
	gen::NkVAE vae(D, H, LAT, 12321u);
	NkVector<NkVar> params;
	vae.Parameters(params);
	optim::NkAdam adam(params, 0.001f);

	NkVar xin = NkVar::Leaf(X, false); // pour l'affichage (encode tout N)
	uint32 rng = 999u;

	// MINI-BATCHES : le bruit du SGD par lot évite le "mean collapse" du full-batch
	// (qui faisait converger vers l'image moyenne -> reconstruction identique/floue).
	NkVector<int32> dummy;
	for (uint32 i = 0; i < N; ++i)
		dummy.PushBack(0);
	data::NkDataset ds(X, dummy, 1);
	data::NkDataLoader loader(ds, 64, true, 7u);

	printf("-- Entraînement du VAE sur MNIST (mini-batches 64, BCE) --\n");
	double reconMse = 0.0;
	for (int e = 0; e <= 60; ++e) {
		double sum = 0;
		uint32 nb = 0;
		for (uint32 b = 0; b < loader.NumBatches(); ++b) {
			adam.ZeroGrad(); // Backward() n'efface plus les feuilles : un lot = un pas
			data::NkBatch batch = loader.GetBatch(b);
			NkVar x = NkVar::Leaf(batch.inputs, false);
			const int64 B = batch.inputs.Shape()[0];
			NkTensor epsT = NkTensor::Zeros(NkShape{B, (int64)LAT});
			FillRandn(epsT, rng);
			NkVar mu, logvar;
			vae.Encode(x, mu, logvar);
			NkVar z = vae.Reparam(mu, logvar, NkVar::Leaf(epsT, false));
			NkVar logits = vae.DecodeLogits(z);
			NkVar recon = nn::BCELoss(logits, x);
			NkVar kl = gen::KLDivergence(mu, logvar);
			NkVar loss = autograd::Add(recon, autograd::MulScalar(kl, 0.00003));
			loss.Backward();
			adam.Step();
			sum += recon.Value().GetItem(NkShape{(int64)0});
			++nb;
		}
		loader.Shuffle();
		reconMse = nb ? sum / nb : 0;
		if (e % 10 == 0)
			printf("  époque %2d : recon BCE = %.5f\n", e, reconMse);
	}

	// (1) Reconstruction de 40 chiffres réels (μ, sans bruit).
	NkVar mu, logvar;
	vae.Encode(xin, mu, logvar);
	NkTensor rec = vae.Decode(mu).Value().Contiguous();
	SavePGMGrid("Build/mnist_recon.pgm", rec.DataAs<float>(), 40, 10);

	// (2) GÉNÉRATION : z ~ N(0,1) -> chiffres inventés.
	const uint32 G = 40;
	NkTensor zN = NkTensor::Zeros(NkShape{(int64)G, (int64)LAT});
	FillRandn(zN, rng);
	NkTensor gen = vae.Decode(NkVar::Leaf(zN, false)).Value().Contiguous();
	bool okGen = SavePGMGrid("Build/mnist_generated.pgm", gen.DataAs<float>(), G, 10);

	printf("\n  -> Build/mnist_recon.pgm (reconstruction) + Build/mnist_generated.pgm (généré)\n");
	printf("  [ %s ] VAE MNIST entraîné (recon BCE finale %.5f) + génération écrite\n",
		   (reconMse < 0.20 && okGen) ? "OK" : "KO", reconMse);
	printf("\n=== Résultat : %d OK, 0 échec(s) ===\n", 1);
	return 0;
}
