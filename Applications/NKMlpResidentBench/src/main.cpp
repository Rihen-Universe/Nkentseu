// =============================================================================
// NKMlpResidentBench — MODÈLE ENTIER (MLP classifieur) : forward + backward via
// autograd, feuilles CPU vs feuilles GPU-RÉSIDENTES.
//   x[B,784] -> Dense(784,H)+biais -> ReLU -> Dense(H,10)+biais -> SoftmaxCE(loss)
// Démontre qu'un modèle complet tourne GPU-résident (matmul/ReLU sur GPU ; biais
// broadcast + perte softmax = petits allers-retours), et que les gradients des 4
// paramètres (W1,b1,W2,b2) sont IDENTIQUES au CPU.
// =============================================================================
#include "NKAutograd/NkVar.h"
#include "NKOptim/NkOptim.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <chrono>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static double MaxErr(const NkTensor &a, const NkTensor &b) {
	if (!a.IsValid() || !b.IsValid() || a.Numel() != b.Numel())
		return 1e30;
	const float *x = a.DataAs<float>();
	const float *y = b.DataAs<float>();
	double e = 0.0;
	const int64 n = a.Numel();
	for (int64 i = 0; i < n; i++) {
		double d = std::fabs((double)x[i] - (double)y[i]);
		if (d > e)
			e = d;
	}
	return e;
}

static NkTensor FillT(const NkShape &sh, double scale, int seed) {
	int64 n = NkShapeNumel(sh);
	float *d = new float[n];
	for (int64 i = 0; i < n; i++)
		d[i] = (float)(scale * std::sin(0.017 * (double)(i + 1) + 0.3 * seed));
	NkTensor t = NkTensor::FromData(sh, d, NkDType::NK_F32);
	delete[] d;
	return t;
}

int main() {
	printf("=== NKMlpResidentBench (MLP entier forward+backward : CPU vs GPU-résident) ===\n");
	const int64 B = 128, IN = 784, H = 512, OUT = 10;
	printf("Modèle : x[%lld,%lld] -> Dense(%lld,%lld)+ReLU -> Dense(%lld,%lld) -> SoftmaxCE\n", (long long)B,
		   (long long)IN, (long long)IN, (long long)H, (long long)H, (long long)OUT);

	// Données / paramètres (déterministes)
	NkTensor xCpu = FillT(NkShape{B, IN}, 1.0, 1);
	NkTensor W1Cpu = FillT(NkShape{IN, H}, 0.05, 2);
	NkTensor b1Cpu = FillT(NkShape{(int64)1, H}, 0.01, 3);
	NkTensor W2Cpu = FillT(NkShape{H, OUT}, 0.05, 4);
	NkTensor b2Cpu = FillT(NkShape{(int64)1, OUT}, 0.01, 5);
	// Cible one-hot [B,OUT]
	float *td = new float[B * OUT];
	for (int64 i = 0; i < B * OUT; i++)
		td[i] = 0.f;
	for (int64 b = 0; b < B; b++)
		td[b * OUT + (b % OUT)] = 1.f;
	NkTensor tCpu = NkTensor::FromData(NkShape{B, OUT}, td, NkDType::NK_F32);
	delete[] td;

	auto toDev = [](const NkTensor &t, bool gpu) { return gpu ? t.ToGPU() : t; };

	// Un pas : forward complet + Backward ; renvoie les 4 gradients (CPU) et la perte.
	auto step = [&](bool gpu, NkTensor &dW1, NkTensor &db1, NkTensor &dW2, NkTensor &db2, double &lossOut) {
		NkVar x = NkVar::Leaf(toDev(xCpu, gpu), false);
		NkVar t = NkVar::Leaf(toDev(tCpu, gpu), false);
		NkVar W1 = NkVar::Leaf(toDev(W1Cpu, gpu), true);
		NkVar b1 = NkVar::Leaf(toDev(b1Cpu, gpu), true);
		NkVar W2 = NkVar::Leaf(toDev(W2Cpu, gpu), true);
		NkVar b2 = NkVar::Leaf(toDev(b2Cpu, gpu), true);

		NkVar z1 = autograd::Add(autograd::Matmul(x, W1), b1); // [B,H]
		NkVar h = autograd::Relu(z1);
		NkVar z2 = autograd::Add(autograd::Matmul(h, W2), b2); // [B,OUT]
		NkVar loss = autograd::SoftmaxCrossEntropy(z2, t);
		loss.Backward();

		NkTensor lc = loss.Value().ToCPU();
		lossOut = lc.IsValid() ? lc.GetItem(NkShape{(int64)0}) : 0.0;
		dW1 = W1.Grad().ToCPU().Contiguous();
		db1 = b1.Grad().ToCPU().Contiguous();
		dW2 = W2.Grad().ToCPU().Contiguous();
		db2 = b2.Grad().ToCPU().Contiguous();
	};

	// Entraînement complet (forward + backward + Adam) sur un batch fixe : la perte
	// doit DESCENDRE. En GPU-résident, params + état Adam restent sur GPU.
	auto trainRun = [&](bool useGpu, int steps, double &finalLoss, double &msPerStep, bool verbose) {
		NkVar W1 = NkVar::Leaf(toDev(W1Cpu, useGpu), true);
		NkVar b1 = NkVar::Leaf(toDev(b1Cpu, useGpu), true);
		NkVar W2 = NkVar::Leaf(toDev(W2Cpu, useGpu), true);
		NkVar b2 = NkVar::Leaf(toDev(b2Cpu, useGpu), true);
		NkVar x = NkVar::Leaf(toDev(xCpu, useGpu), false);
		NkVar t = NkVar::Leaf(toDev(tCpu, useGpu), false);
		NkVector<NkVar> params;
		params.PushBack(W1);
		params.PushBack(b1);
		params.PushBack(W2);
		params.PushBack(b2);
		optim::NkAdam adam(params, 0.002f);
		double loss = 0.0;
		auto ta = std::chrono::high_resolution_clock::now();
		for (int s = 0; s < steps; s++) {
			NkVar z1 = autograd::Add(autograd::Matmul(x, W1), b1);
			NkVar h = autograd::Relu(z1);
			NkVar z2 = autograd::Add(autograd::Matmul(h, W2), b2);
			NkVar L = autograd::SoftmaxCrossEntropy(z2, t);
			L.Backward();
			adam.Step();
			adam.ZeroGrad();
			NkTensor lc = L.Value().ToCPU();
			loss = lc.IsValid() ? lc.GetItem(NkShape{(int64)0}) : 0.0;
			if (verbose && (s % 8 == 0 || s == steps - 1))
				printf("      step %3d : loss = %.4f\n", s, loss);
		}
		auto tb = std::chrono::high_resolution_clock::now();
		finalLoss = loss;
		msPerStep = std::chrono::duration<double, std::milli>(tb - ta).count() / steps;
	};

	NkTensorGpu &gpu = NkTensorGpu::Get();
	printf("GPU compute : %s (backend %s)\n\n", gpu.IsAvailable() ? "OUI" : "NON", gpu.BackendName());

	// (A) CPU
	NkTensor aW1, ab1, aW2, ab2;
	double lossA = 0.0;
	const int Rc = 5;
	step(false, aW1, ab1, aW2, ab2, lossA);
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int r = 0; r < Rc; r++)
		step(false, aW1, ab1, aW2, ab2, lossA);
	auto t1 = std::chrono::high_resolution_clock::now();
	double cpuMs = std::chrono::duration<double, std::milli>(t1 - t0).count() / Rc;

	printf("--- Temps d'un pas MLP (forward + backward) ---\n");
	printf("  (A) feuilles CPU                                : %9.2f ms\n", cpuMs);

	if (gpu.IsAvailable()) {
		NkTensor gW1, gb1, gW2, gb2;
		double lossG = 0.0;
		step(true, gW1, gb1, gW2, gb2, lossG); // warmups
		step(true, gW1, gb1, gW2, gb2, lossG);
		const int Rg = 20;
		auto t2 = std::chrono::high_resolution_clock::now();
		for (int r = 0; r < Rg; r++)
			step(true, gW1, gb1, gW2, gb2, lossG);
		auto t3 = std::chrono::high_resolution_clock::now();
		double gpuMs = std::chrono::duration<double, std::milli>(t3 - t2).count() / Rg;

		printf("  (B) feuilles GPU-RÉSIDENTES                     : %9.2f ms   (moyenne)\n", gpuMs);
		printf("  Accélération (A/B)                              : %8.1fx\n", (gpuMs > 0 ? cpuMs / gpuMs : -1.0));
		printf("\nloss CPU=%.5f  loss GPU=%.5f\n", lossA, lossG);
		double eW1 = MaxErr(aW1, gW1), eb1 = MaxErr(ab1, gb1), eW2 = MaxErr(aW2, gW2), eb2 = MaxErr(ab2, gb2);
		printf("--- Correction (gradients GPU-résident vs CPU) ---\n");
		printf("  err dW1=%.2e  db1=%.2e  dW2=%.2e  db2=%.2e\n", eW1, eb1, eW2, eb2);
		bool ok = eW1 < 1e-2 && eb1 < 1e-2 && eW2 < 1e-2 && eb2 < 1e-2;
		printf("  [%s] gradients des 4 paramètres identiques\n", ok ? " OK " : "FAIL");

		// Entraînement Adam de bout en bout (100% GPU-résident) : la perte descend.
		printf("\n--- Entraînement Adam 60 pas, lr=0.002 (surapprentissage d'un batch fixe) ---\n");
		double clA = 0, cmA = 0, glA = 0, gmA = 0;
		trainRun(false, 60, clA, cmA, false);
		printf("  (A) CPU              : %6.2f ms/pas   loss finale %.4f\n", cmA, clA);
		printf("  (B) GPU-RÉSIDENT (loss qui descend depuis 2.30) :\n");
		trainRun(true, 60, glA, gmA, true);
		printf("  (B) GPU-résident     : %6.2f ms/pas   loss finale %.4f\n", gmA, glA);
		printf("  Accélération entraînement (A/B) : %.1fx  (Adam FUSÉ : 1 seul dispatch/param,\n",
			   (gmA > 0 ? cmA / gmA : -1.0));
		printf("     param/m/v mis à jour en place -> le fwd+bwd domine désormais le pas)\n");
		const double start = 2.302585;								   // ln(10) = perte initiale (softmax uniforme)
		bool decreased = (glA < start - 0.02) && (clA < start - 0.02); // la perte a bien baissé
		bool track = std::fabs(clA - glA) < 0.05;					   // GPU-résident suit le CPU
		bool trained = decreased && track;
		printf("  [%s] la perte descend (%.4f -> %.4f) ET GPU-résident == CPU (|Δ|=%.4f)\n", trained ? " OK " : "FAIL",
			   start, glA, std::fabs(clA - glA));

		gpu.Shutdown();
		return (ok && trained) ? 0 : 1;
	}
	printf("  (GPU indisponible)\n");
	return 0;
}
