// =============================================================================
// NKGpuBenchTest — mesurer le gain GPU vs CPU sur le matmul (cœur de l'entraînement).
//   Le matmul domine les couches Dense (et, via im2col, la conv). On chronomètre
//   ops::Matmul sur CPU vs sur GPU (kernels NkSL, NkTensorGpu) pour plusieurs
//   tailles, et on VÉRIFIE que les résultats coïncident. Quantifie le payoff de
//   porter l'entraînement sur GPU.
// =============================================================================
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

#include <cstdio>
#include <chrono>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static NkTensor RandMat(uint32 r, uint32 c, uint32 &s) {
	NkTensor t = NkTensor::Zeros(NkShape{(int64)r, (int64)c});
	float *p = t.DataAs<float>();
	for (int64 i = 0; i < (int64)r * c; ++i) {
		s = s * 1664525u + 1013904223u;
		p[i] = (float)((s >> 9) & 0x3FF) / 1024.f - 0.5f;
	}
	return t;
}

static double MaxAbsDiff(const NkTensor &a, const NkTensor &b) {
	NkTensor ac = a.Contiguous(), bc = b.Contiguous();
	const float *ap = ac.DataAs<float>();
	const float *bp = bc.DataAs<float>();
	int64 n = NkShapeNumel(ac.Shape());
	double m = 0;
	for (int64 i = 0; i < n; ++i) {
		double d = std::fabs((double)ap[i] - (double)bp[i]);
		if (d > m)
			m = d;
	}
	return m;
}

int main() {
	printf("=== NKGpuBenchTest : matmul CPU vs GPU (NkSL compute) ===\n\n");

	// Détection GPU via un probe ToGPU.
	{
		uint32 s0 = 1u;
		NkTensor probe = RandMat(4, 4, s0).ToGPU();
		printf("  GPU dispo : %s\n\n", (probe.Device() == NkDevice::NK_GPU) ? "oui" : "NON");
	}

	const uint32 sizes[] = {128, 256, 384, 512};
	int pass = 0, fail = 0;
	printf("  %-8s %-14s %-14s %-10s %-12s\n", "taille", "CPU (ms)", "GPU (ms)", "speedup", "err max");
	for (uint32 si = 0; si < 4; ++si) {
		const uint32 M = sizes[si], K = sizes[si], N = sizes[si];
		uint32 s = 12345u + si * 777u;
		NkTensor A = RandMat(M, K, s), B = RandMat(K, N, s);

		// CPU
		auto t0 = std::chrono::high_resolution_clock::now();
		NkTensor Ccpu = ops::Matmul(A, B);
		auto t1 = std::chrono::high_resolution_clock::now();
		double cpuMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

		// GPU (transfert + calcul + retour)
		double gpuMs = -1.0;
		double err = -1.0;
		NkTensor Ag = A.ToGPU(), Bg = B.ToGPU();
		if (Ag.Device() == NkDevice::NK_GPU && Bg.Device() == NkDevice::NK_GPU) {
			auto g0 = std::chrono::high_resolution_clock::now();
			NkTensor Cg = ops::Matmul(Ag, Bg);
			NkTensor Cgpu = Cg.ToCPU();
			auto g1 = std::chrono::high_resolution_clock::now();
			gpuMs = std::chrono::duration<double, std::milli>(g1 - g0).count();
			err = MaxAbsDiff(Ccpu, Cgpu);
		}
		double sp = (gpuMs > 0) ? cpuMs / gpuMs : 0.0;
		printf("  %-8u %-14.1f %-14.1f %-9.1fx %.2e\n", M, cpuMs, gpuMs, sp, err);
		bool ok = (gpuMs > 0) && (err < 1e-2);
		(ok ? pass : fail)++;
	}
	printf("\n  (err = |CPU - GPU| max ; doit être ~0 = mêmes résultats)\n");
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
	return fail == 0 ? 0 : 1;
}
