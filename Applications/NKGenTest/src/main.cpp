// =============================================================================
// NKGenTest — auto-encodeur génératif (NKGen).
//   Jeu synthétique d'« images » 8x8 (4 motifs + bruit). On entraîne un
//   auto-encodeur (reconstruction MSE + Adam), on mesure l'erreur, on affiche
//   original vs reconstruction, puis on GÉNÈRE en interpolant dans l'espace
//   latent entre deux images (preuve que le latent est signifiant).
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 W = 8, D = W * W; // images 8x8 = 64

// 4 motifs prototypes (barre verticale, horizontale, diagonale, cadre).
static void Prototype(uint32 kind, float *out) {
	for (uint32 i = 0; i < D; ++i)
		out[i] = 0.0f;
	for (uint32 y = 0; y < W; ++y)
		for (uint32 x = 0; x < W; ++x) {
			bool on = false;
			switch (kind) {
				case 0:
					on = (x == 3 || x == 4);
					break; // barre verticale
				case 1:
					on = (y == 3 || y == 4);
					break; // barre horizontale
				case 2:
					on = (x == y || x + 1 == y || x == y + 1);
					break; // diagonale
				default:
					on = (x == 0 || y == 0 || x == W - 1 || y == W - 1); // cadre
			}
			if (on)
				out[y * W + x] = 1.0f;
		}
}

static void PrintImage(const float *img, const char *title) {
	printf("    %s\n", title);
	const char *ramp = " .:oO#";
	for (uint32 y = 0; y < W; ++y) {
		printf("      ");
		for (uint32 x = 0; x < W; ++x) {
			float v = img[y * W + x];
			if (v < 0.f)
				v = 0.f;
			if (v > 1.f)
				v = 1.f;
			int idx = (int)(v * 5.0f + 0.5f);
			printf("%c", ramp[idx]);
		}
		printf("\n");
	}
}

int main() {
	printf("=== NKGenTest : auto-encodeur génératif (8x8) ===\n\n");

	// Jeu : 4 motifs x 60 = 240 images bruitées.
	const uint32 NCLS = 4, PER = 60, N = NCLS * PER;
	NkTensor X = NkTensor::Zeros(NkShape{(int64)N, (int64)D});
	NkVector<int32> labels;
	{
		float *xp = X.DataAs<float>();
		float proto[D];
		uint32 s = 2024u;
		for (uint32 c = 0; c < NCLS; ++c) {
			Prototype(c, proto);
			for (uint32 k = 0; k < PER; ++k) {
				uint32 i = c * PER + k;
				for (uint32 j = 0; j < D; ++j) {
					s = s * 1664525u + 1013904223u;
					float noise = ((float)((s >> 9) & 0x7FFFu) / 32767.0f - 0.5f) * 0.3f;
					float v = proto[j] + noise;
					xp[i * D + j] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				}
				labels.PushBack((int32)c);
			}
		}
	}
	data::NkDataset ds(X, labels, NCLS);
	data::NkDataLoader loader(ds, 32, true, 7u);

	// Auto-encodeur : 64 -> 32 -> 4 (latent) -> 32 -> 64.
	gen::NkAutoencoder ae(D, 32, 4, 123u);
	NkVector<NkVar> params;
	ae.Parameters(params);
	optim::NkAdam adam(params, 0.005f);

	printf("-- Entraînement (reconstruction MSE + Adam) --\n");
	double lastMse = 0.0;
	for (int e = 1; e <= 400; ++e) {
		double sum = 0.0;
		uint32 nb = 0;
		for (uint32 b = 0; b < loader.NumBatches(); ++b) {
			data::NkBatch batch = loader.GetBatch(b);
			NkVar x = NkVar::Leaf(batch.inputs, false);
			NkVar recon = ae.Forward(x);
			NkVar loss = autograd::MSE(recon, x); // reconstruire l'entrée
			loss.Backward();
			adam.Step();
			sum += loss.Value().GetItem(NkShape{(int64)0});
			++nb;
		}
		loader.Shuffle();
		lastMse = nb ? sum / nb : 0.0;
		if (e % 80 == 0 || e == 1)
			printf("  époque %3d : erreur de reconstruction (MSE) = %.5f\n", e, lastMse);
	}

	// Original vs reconstruction sur un exemple de chaque motif.
	printf("\n-- Reconstruction (original -> reconstruit) --\n");
	const float *xp = X.DataAs<float>();
	for (uint32 c = 0; c < NCLS; ++c) {
		uint32 i = c * PER + 0;
		NkTensor one = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + i * D, NkDType::NK_F32);
		NkTensor rec = ae.Forward(NkVar::Leaf(one, false)).Value().Contiguous();
		char t1[32], t2[32];
		snprintf(t1, sizeof(t1), "motif %u (original)", c);
		snprintf(t2, sizeof(t2), "motif %u (reconstruit)", c);
		PrintImage(one.DataAs<float>(), t1);
		PrintImage(rec.DataAs<float>(), t2);
	}

	// GÉNÉRATION : interpoler dans l'espace latent entre le motif 0 et le motif 1.
	printf("\n-- Génération : interpolation latente motif 0 -> motif 1 --\n");
	// imgA = premier exemple du motif 0 ; imgB = premier exemple du motif 1.
	NkTensor imgA = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + (0 * PER) * D, NkDType::NK_F32);
	NkTensor imgB = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + (1 * PER) * D, NkDType::NK_F32);
	NkTensor zA = ae.Encode(NkVar::Leaf(imgA, false)).Value();
	NkTensor zB = ae.Encode(NkVar::Leaf(imgB, false)).Value();
	const float ts[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
	for (int k = 0; k < 5; ++k) {
		float t = ts[k];
		NkTensor zt = ops::Add(ops::MulScalar(zA, 1.0 - t), ops::MulScalar(zB, t));
		NkTensor gen = ae.Decode(NkVar::Leaf(zt, false)).Value().Contiguous();
		char title[48];
		snprintf(title, sizeof(title), "t = %.2f  (généré depuis le latent)", t);
		PrintImage(gen.DataAs<float>(), title);
	}

	const bool ok = lastMse < 0.02;
	printf("\n  [ %s ] auto-encodeur : reconstruction MSE finale = %.5f (< 0.02)\n", ok ? "OK" : "KO", lastMse);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", ok ? 1 : 0, ok ? 0 : 1);
	return ok ? 0 : 1;
}
