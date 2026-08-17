// =============================================================================
// NKGen3DTest — génération de FORMES 3D par auto-encodeur (NKGen, vision 3D).
//   Voxels 6x6x6 (=216). 4 formes procédurales (sphère, cube, cylindre, pyramide)
//   + bruit. On entraîne l'auto-encodeur à reconstruire, puis on GÉNÈRE une forme
//   3D inédite en interpolant dans l'espace latent (sphère -> cube). Preuve que la
//   même stack from-scratch produit de la géométrie 3D. Affichage : montage des
//   tranches Z.
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKData/NkData.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 G = 6, D = G * G * G; // grille 6x6x6 = 216 voxels

static inline uint32 Idx(uint32 x, uint32 y, uint32 z) {
	return z * G * G + y * G + x;
}

// 4 formes prototypes remplies (occupation 0/1).
static void Shape(uint32 kind, float *out) {
	const float c = (float)(G - 1) * 0.5f; // centre
	for (uint32 z = 0; z < G; ++z)
		for (uint32 y = 0; y < G; ++y)
			for (uint32 x = 0; x < G; ++x) {
				float fx = (float)x - c, fy = (float)y - c, fz = (float)z - c;
				bool on = false;
				switch (kind) {
					case 0:
						on = (fx * fx + fy * fy + fz * fz) <= 6.3f;
						break; // sphère
					case 1:
						on = (x >= 1 && x <= 4 && y >= 1 && y <= 4 && z >= 1 && z <= 4);
						break; // cube
					case 2:
						on = (fx * fx + fy * fy) <= 4.2f;
						break; // cylindre (axe z)
					default: { // pyramide
						float r = 2.5f - (float)z * 0.45f;
						on = (std::fabs(fx) <= r && std::fabs(fy) <= r);
					} break;
				}
				out[Idx(x, y, z)] = on ? 1.0f : 0.0f;
			}
}

// Affiche une forme 3D : montage horizontal des G tranches Z (chaque tranche GxG).
static void PrintShape(const float *v, const char *title) {
	printf("    %s   (tranches z=0..%u ->)\n", title, G - 1);
	const char *ramp = " .:oO#";
	for (uint32 y = 0; y < G; ++y) {
		printf("      ");
		for (uint32 z = 0; z < G; ++z) {
			for (uint32 x = 0; x < G; ++x) {
				float val = v[Idx(x, y, z)];
				if (val < 0.f)
					val = 0.f;
				if (val > 1.f)
					val = 1.f;
				printf("%c", ramp[(int)(val * 5.0f + 0.5f)]);
			}
			printf("  "); // séparateur entre tranches
		}
		printf("\n");
	}
}

int main() {
	printf("=== NKGen3DTest : génération de formes 3D (voxels 6x6x6) ===\n\n");

	const uint32 NCLS = 4, PER = 50, N = NCLS * PER;
	NkTensor X = NkTensor::Zeros(NkShape{(int64)N, (int64)D});
	NkVector<int32> labels;
	{
		float *xp = X.DataAs<float>();
		float proto[D];
		uint32 s = 7777u;
		for (uint32 c = 0; c < NCLS; ++c) {
			Shape(c, proto);
			for (uint32 k = 0; k < PER; ++k) {
				uint32 i = c * PER + k;
				for (uint32 j = 0; j < D; ++j) {
					s = s * 1664525u + 1013904223u;
					float noise = ((float)((s >> 9) & 0x7FFFu) / 32767.0f - 0.5f) * 0.25f;
					float val = proto[j] + noise;
					xp[i * D + j] = val < 0.f ? 0.f : (val > 1.f ? 1.f : val);
				}
				labels.PushBack((int32)c);
			}
		}
	}
	data::NkDataset ds(X, labels, NCLS);
	data::NkDataLoader loader(ds, 32, true, 5u);

	// Auto-encodeur 3D : 216 -> 48 -> 6 (latent) -> 48 -> 216.
	gen::NkAutoencoder ae(D, 48, 6, 321u);
	NkVector<NkVar> params;
	ae.Parameters(params);
	optim::NkAdam adam(params, 0.005f);

	printf("-- Entraînement (reconstruction 3D, MSE + Adam) --\n");
	double lastMse = 0.0;
	for (int e = 1; e <= 500; ++e) {
		double sum = 0.0;
		uint32 nb = 0;
		for (uint32 b = 0; b < loader.NumBatches(); ++b) {
			adam.ZeroGrad(); // Backward() n'efface plus les feuilles : un lot = un pas
			data::NkBatch batch = loader.GetBatch(b);
			NkVar x = NkVar::Leaf(batch.inputs, false);
			NkVar recon = ae.Forward(x);
			NkVar loss = autograd::MSE(recon, x);
			loss.Backward();
			adam.Step();
			sum += loss.Value().GetItem(NkShape{(int64)0});
			++nb;
		}
		loader.Shuffle();
		lastMse = nb ? sum / nb : 0.0;
		if (e % 100 == 0 || e == 1)
			printf("  époque %3d : MSE reconstruction 3D = %.5f\n", e, lastMse);
	}

	// Reconstruction d'une forme de chaque type.
	printf("\n-- Reconstruction 3D --\n");
	const float *xp = X.DataAs<float>();
	const char *names[4] = {"sphère", "cube", "cylindre", "pyramide"};
	for (uint32 c = 0; c < NCLS; ++c) {
		uint32 i = c * PER;
		NkTensor one = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + i * D, NkDType::NK_F32);
		NkTensor rec = ae.Forward(NkVar::Leaf(one, false)).Value().Contiguous();
		char t[64];
		snprintf(t, sizeof(t), "%s reconstruite", names[c]);
		PrintShape(rec.DataAs<float>(), t);
	}

	// GÉNÉRATION 3D : morphing latent sphère -> cube.
	printf("\n-- Génération 3D : morphing latent sphère -> cube --\n");
	NkTensor sph = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + (0 * PER) * D, NkDType::NK_F32);
	NkTensor box = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + (1 * PER) * D, NkDType::NK_F32);
	NkTensor zS = ae.Encode(NkVar::Leaf(sph, false)).Value();
	NkTensor zB = ae.Encode(NkVar::Leaf(box, false)).Value();
	const float ts[3] = {0.25f, 0.5f, 0.75f};
	for (int k = 0; k < 3; ++k) {
		float t = ts[k];
		NkTensor zt = ops::Add(ops::MulScalar(zS, 1.0 - t), ops::MulScalar(zB, t));
		NkTensor gen = ae.Decode(NkVar::Leaf(zt, false)).Value().Contiguous();
		char t1[48];
		snprintf(t1, sizeof(t1), "forme générée (t=%.2f)", t);
		PrintShape(gen.DataAs<float>(), t1);
	}

	const bool ok = lastMse < 0.03;
	printf("\n  [ %s ] auto-encodeur 3D : MSE finale = %.5f (< 0.03)\n", ok ? "OK" : "KO", lastMse);
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", ok ? 1 : 0, ok ? 0 : 1);
	return ok ? 0 : 1;
}
