// =============================================================================
// NKObjectGenTest — générer des OBJETS choisis : BANANE, ROCHER, ARBRE.
//   On n'a pas de dataset réel -> on DÉFINIT ces 3 objets comme données
//   (prototypes procéduraux 16³ + bruit), on entraîne le VAE 3D, puis on GÉNÈRE
//   chaque objet en décodant depuis le CENTROÏDE latent de sa classe (= conditionnement
//   simple). Chaque objet -> voxels -> maillage -> OBJ chargeable dans le moteur.
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static const uint32 G = 16, VOX = G * G * G;

static inline uint32 Idx(uint32 x, uint32 y, uint32 z) {
	return z * G * G + y * G + x;
}

// ---- Prototypes des 3 objets (occupation 0/1) --------------------------------
static void Proto(uint32 kind, float *out) {
	for (uint32 i = 0; i < VOX; ++i)
		out[i] = 0.f;
	for (uint32 z = 0; z < G; ++z)
		for (uint32 y = 0; y < G; ++y)
			for (uint32 x = 0; x < G; ++x) {
				float fx = (float)x, fy = (float)y, fz = (float)z;
				bool on = false;
				if (kind == 0) {
					// BANANE : tube courbé (arc) dans le plan x-z, épaisseur ~2.
					float best = 1e9f;
					for (int s = 0; s <= 20; ++s) {
						float t = (float)s / 20.f;
						float cx = 3.f + 10.f * t;
						float cz = 6.f + 4.f * std::sin(3.14159f * t); // l'arc monte puis descend
						float cy = 8.f;
						float d = (fx - cx) * (fx - cx) + (fy - cy) * (fy - cy) + (fz - cz) * (fz - cz);
						if (d < best)
							best = d;
					}
					on = best <= 4.0f; // rayon ~2
				} else if (kind == 1) {
					// ROCHER : union de sphères décalées -> blob irrégulier.
					const float cs[4][4] = {{8, 8, 8, 5.0f}, {6, 9, 7, 3.2f}, {10, 7, 9, 3.4f}, {8, 6, 10, 3.0f}};
					for (int i = 0; i < 4; ++i) {
						float d = (fx - cs[i][0]) * (fx - cs[i][0]) + (fy - cs[i][1]) * (fy - cs[i][1]) +
								  (fz - cs[i][2]) * (fz - cs[i][2]);
						if (d <= cs[i][3] * cs[i][3]) {
							on = true;
							break;
						}
					}
				} else {
					// ARBRE : tronc vertical + feuillage sphérique en haut.
					bool trunk = (std::fabs(fx - 8.f) <= 1.5f && std::fabs(fy - 8.f) <= 1.5f && z >= 1 && z <= 9);
					float df = (fx - 8.f) * (fx - 8.f) + (fy - 8.f) * (fy - 8.f) + (fz - 12.f) * (fz - 12.f);
					bool foliage = df <= 16.f; // rayon 4 en z=12
					on = trunk || foliage;
				}
				out[Idx(x, y, z)] = on ? 1.f : 0.f;
			}
}

static void PrintSlices(const float *v, const char *title) {
	printf("    %s  (tranches z=0,3,6,9,12,15)\n", title);
	const int zs[6] = {0, 3, 6, 9, 12, 15};
	const char *ramp = " .:oO#";
	for (uint32 y = 0; y < G; ++y) {
		printf("      ");
		for (int zi = 0; zi < 6; ++zi) {
			uint32 z = (uint32)zs[zi];
			for (uint32 x = 0; x < G; ++x) {
				float val = v[Idx(x, y, z)];
				if (val < 0)
					val = 0;
				if (val > 1)
					val = 1;
				printf("%c", ramp[(int)(val * 5.f + 0.5f)]);
			}
			printf(" ");
		}
		printf("\n");
	}
}

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

int main() {
	printf("=== NKObjectGenTest : générer banane / rocher / arbre (VAE 3D 16³) ===\n\n");

	const uint32 NCLS = 3, PER = 8, N = NCLS * PER;
	const char *names[3] = {"banane", "rocher", "arbre"};
	NkTensor X = NkTensor::Zeros(NkShape{(int64)N, 1, (int64)G, (int64)G, (int64)G});
	{
		float *xp = X.DataAs<float>();
		float proto[VOX];
		uint32 s = 1234u;
		for (uint32 c = 0; c < NCLS; ++c) {
			Proto(c, proto);
			for (uint32 k = 0; k < PER; ++k) {
				uint32 i = c * PER + k;
				for (uint32 j = 0; j < VOX; ++j) {
					s = s * 1664525u + 1013904223u;
					float noise = ((float)((s >> 9) & 0x7FFFu) / 32767.f - 0.5f) * 0.15f;
					float v = proto[j] + noise;
					xp[i * VOX + j] = v < 0 ? 0 : (v > 1 ? 1 : v);
				}
			}
		}
	}

	const uint32 LAT = 16, CH = 8;
	gen::NkVoxelVAE vae(G, CH, LAT, 4242u);
	NkVector<NkVar> params;
	vae.Parameters(params);
	optim::NkAdam adam(params, 0.004f);

	NkVar xin = NkVar::Leaf(X, false);
	NkTensor epsT = NkTensor::Zeros(NkShape{(int64)N, (int64)LAT});
	uint32 rng = 777u;

	printf("-- Entraînement du VAE 3D sur {banane, rocher, arbre} --\n");
	double reconMse = 0.0;
	for (int e = 0; e <= 600; ++e) {
		adam.ZeroGrad(); // Backward() n'efface plus les feuilles : un pas = un gradient
		NkVar mu, logvar;
		vae.Encode(xin, mu, logvar);
		FillRandn(epsT, rng);
		NkVar z = vae.Reparam(mu, logvar, NkVar::Leaf(epsT, false));
		NkVar xhat = vae.Decode(z);
		NkVar recon = autograd::MSE(xhat, xin);
		NkVar kl = gen::KLDivergence(mu, logvar);
		NkVar loss = autograd::Add(recon, autograd::MulScalar(kl, 0.0002));
		loss.Backward();
		adam.Step();
		reconMse = recon.Value().GetItem(NkShape{(int64)0});
		if (e % 100 == 0)
			printf("  époque %3d : recon MSE = %.5f\n", e, reconMse);
	}

	// Centroïde latent par classe (= "conditionnement" : où vit chaque objet).
	NkVar muV, lvV;
	vae.Encode(xin, muV, lvV);
	NkTensor muT = muV.Value().Contiguous(); // [N, LAT]
	const float *mp = muT.DataAs<float>();

	int pass = 0, fail = 0;
	auto check = [&](bool ok, const char *n) {
		(ok ? pass : fail)++;
		printf("  [ %s ] %s\n", ok ? "OK" : "KO", n);
	};

	printf("\n-- GÉNÉRATION de chaque objet (décodage depuis son centroïde latent) --\n");
	for (uint32 c = 0; c < NCLS; ++c) {
		NkTensor zc = NkTensor::Zeros(NkShape{1, (int64)LAT});
		float *zp = zc.DataAs<float>();
		for (uint32 l = 0; l < LAT; ++l) {
			double m = 0;
			for (uint32 k = 0; k < PER; ++k)
				m += mp[(c * PER + k) * LAT + l];
			zp[l] = (float)(m / PER); // centroïde de la classe
		}
		NkTensor vox = vae.Decode(NkVar::Leaf(zc, false)).Value().Contiguous(); // [1,1,16³]
		const float *gp = vox.DataAs<float>();
		PrintSlices(gp, names[c]);
		uint32 occ = 0;
		for (uint32 j = 0; j < VOX; ++j)
			if (gp[j] > 0.5f)
				++occ;
		gen::NkMesh mesh = gen::VoxelsToMesh(gp, G, 0.5f, 1.0f);
		char path[64];
		snprintf(path, sizeof(path), "Build/nkgen_%s.obj", names[c]);
		bool saved = gen::SaveMeshObj(path, mesh);
		printf("    -> %s : %u voxels, %u tris, OBJ=%s\n\n", names[c], occ, mesh.TriangleCount(), saved ? path : "KO");
		check(occ > 20 && mesh.TriangleCount() > 0 && saved, names[c]);
	}
	check(reconMse < 0.06, "VAE 3D reconstruit correctement");

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
	return fail == 0 ? 0 : 1;
}
