// =============================================================================
// NKGenMeshTest — pont IA -> moteur : une forme 3D GÉNÉRÉE devient un MAILLAGE.
//   1) Entraîne l'auto-encodeur 3D (voxels).                       [NKGen]
//   2) Génère une forme inédite (morphing latent sphère->cube).
//   3) Convertit les voxels en MAILLAGE de triangles (culling).    [NKGen/NkMesh]
//   4) Exporte un .OBJ chargeable dans le moteur + valide le maillage.
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

static const uint32 G = 6, D = G * G * G;

static inline uint32 Idx(uint32 x, uint32 y, uint32 z) {
	return z * G * G + y * G + x;
}

static void Shape(uint32 kind, float *out) {
	const float c = (float)(G - 1) * 0.5f;
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
						break; // cylindre
					default: {
						float r = 2.5f - (float)z * 0.45f;
						on = (std::fabs(fx) <= r && std::fabs(fy) <= r);
					}
				}
				out[Idx(x, y, z)] = on ? 1.0f : 0.0f;
			}
}

static void PrintSlices(const float *v, const char *title) {
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
			printf("  ");
		}
		printf("\n");
	}
}

int main() {
	printf("=== NKGenMeshTest : forme 3D générée -> maillage OBJ ===\n\n");

	// Jeu voxels (sphère, cube, cylindre, pyramide + bruit).
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

	// Auto-encodeur 3D + entraînement.
	gen::NkAutoencoder ae(D, 48, 6, 321u);
	NkVector<NkVar> params;
	ae.Parameters(params);
	optim::NkAdam adam(params, 0.005f);
	printf("-- Entraînement de l'auto-encodeur 3D --\n");
	double mse = 0.0;
	for (int e = 1; e <= 400; ++e) {
		double sum = 0.0;
		uint32 nb = 0;
		for (uint32 b = 0; b < loader.NumBatches(); ++b) {
			adam.ZeroGrad(); // Backward() n'efface plus les feuilles : un lot = un pas
			data::NkBatch batch = loader.GetBatch(b);
			NkVar x = NkVar::Leaf(batch.inputs, false);
			NkVar loss = autograd::MSE(ae.Forward(x), x);
			loss.Backward();
			adam.Step();
			sum += loss.Value().GetItem(NkShape{(int64)0});
			++nb;
		}
		loader.Shuffle();
		mse = nb ? sum / nb : 0.0;
		if (e % 200 == 0)
			printf("  époque %3d : MSE = %.5f\n", e, mse);
	}

	// Génère une forme inédite : latent à mi-chemin sphère<->cube.
	const float *xp = X.DataAs<float>();
	NkTensor sph = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + (0 * PER) * D, NkDType::NK_F32);
	NkTensor box = NkTensor::FromData(NkShape{(int64)1, (int64)D}, xp + (1 * PER) * D, NkDType::NK_F32);
	NkTensor zS = ae.Encode(NkVar::Leaf(sph, false)).Value();
	NkTensor zB = ae.Encode(NkVar::Leaf(box, false)).Value();
	NkTensor zt = ops::Add(ops::MulScalar(zS, 0.5), ops::MulScalar(zB, 0.5));
	NkTensor gen = ae.Decode(NkVar::Leaf(zt, false)).Value().Contiguous();

	printf("\n-- Forme générée (voxels) --\n");
	PrintSlices(gen.DataAs<float>(), "forme générée (t=0.5)");

	// Voxels -> maillage -> OBJ.
	printf("\n-- Maillage (voxels -> triangles, faces internes supprimées) --\n");
	gen::NkMesh mesh = gen::VoxelsToMesh(gen.DataAs<float>(), G, /*iso*/ 0.5f, /*cell*/ 1.0f);
	const uint32 vc = mesh.VertexCount(), tc = mesh.TriangleCount();
	printf("  maillage : %u sommets, %u triangles\n", vc, tc);

	const char *objPath = "Build/nkgen_shape.obj";
	bool saved = gen::SaveMeshObj(objPath, mesh);
	printf("  export OBJ : %s -> %s\n", saved ? "OK" : "KO", objPath);

	// Validation : maillage non vide + indices valides + fichier écrit.
	bool idxOk = true;
	for (uint32 i = 0; i < mesh.triangles.Size(); ++i)
		if (mesh.triangles[i] >= vc) {
			idxOk = false;
			break;
		}

	int pass = 0, fail = 0;
	auto check = [&](bool ok, const char *n) {
		(ok ? pass : fail)++;
		printf("  [ %s ] %s\n", ok ? "OK" : "KO", n);
	};
	check(mse < 0.03, "auto-encodeur 3D convergé (MSE < 0.03)");
	check(vc > 0 && tc > 0, "maillage non vide (sommets + triangles générés)");
	check(idxOk, "tous les indices de triangle sont valides");
	check(saved, "fichier OBJ écrit (chargeable dans le moteur)");

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
	return fail == 0 ? 0 : 1;
}
