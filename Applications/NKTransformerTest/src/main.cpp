// =============================================================================
// NKTransformerTest — valide l'attention multi-têtes causale (brique 6) :
//   (1) gradient-check du forward+backward complet (batched matmul + permute +
//       softmax causal + Dense) par rapport à l'entrée ;
//   (2) attention GPU-résidente == attention CPU (mêmes poids).
// =============================================================================
#include "NKNN/NkNN.h"
#include "NKOptim/NkOptim.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKGpt/NkSampling.h" // échantillonnage top-k / top-p (KV-cache brique A.3)

#include <cstdio>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_ok = 0, g_fail = 0;

static void check(bool c, const char *w) {
	printf("  [%s] %s\n", c ? " OK " : "FAIL", w);
	if (c)
		++g_ok;
	else
		++g_fail;
}

static NkTensor Mat(const NkShape &s, const float *d) {
	return NkTensor::FromData(s, d, NkDType::NK_F32);
}

// Gradient-check : compare le gradient analytique à des différences finies centrées.
template <typename F> static void GradCheck(const char *name, const NkTensor &x0, F buildLoss) {
	NkVar x = NkVar::Leaf(x0, true);
	NkVar loss = buildLoss(x);
	loss.Backward();
	NkTensor grad = x.Grad().ToCPU().Contiguous();
	const int64 n = x0.Numel();
	NkTensor xc = x0.Contiguous();
	const float *gp = grad.DataAs<float>();
	double maxErr = 0.0;
	const double eps = 1e-3;
	for (int64 i = 0; i < n; ++i) {
		NkTensor xp = xc.Clone();
		NkTensor xm = xc.Clone();
		xp.DataAs<float>()[i] += (float)eps;
		xm.DataAs<float>()[i] -= (float)eps;
		NkVar lp = buildLoss(NkVar::Leaf(xp, false));
		NkVar lm = buildLoss(NkVar::Leaf(xm, false));
		double num = (lp.Value().ToCPU().GetItem(NkShape{(int64)0}) - lm.Value().ToCPU().GetItem(NkShape{(int64)0})) /
					 (2.0 * eps);
		double d = std::fabs(num - (double)gp[i]);
		if (d > maxErr)
			maxErr = d;
	}
	printf("  [%s] %-16s  err max vs diff. finies = %.2e\n", maxErr < 1e-2 ? " OK " : "FAIL", name, maxErr);
	if (maxErr < 1e-2)
		++g_ok;
	else
		++g_fail;
}

int main() {
	printf("=== NKTransformerTest : attention multi-têtes causale ===\n\n");

	// Entrée [B=1, T=3, d=4], 2 têtes (hd=2).
	float xd[12];
	for (int i = 0; i < 12; ++i)
		xd[i] = (float)std::sin(0.5 * (i + 1)) * 0.5f;
	NkShape xs;
	xs.PushBack(1);
	xs.PushBack(3);
	xs.PushBack(4);

	// (1) gradient-check du forward+backward complet par rapport à l'entrée.
	{
		nn::NkMultiHeadAttention attn(4, 2, 7u);
		GradCheck("Attention dX", Mat(xs, xd), [&attn](NkVar x) { return autograd::Sum(attn.Forward(x)); });
	}

	// (2) attention GPU-résidente == CPU (mêmes poids via même graine).
	NkTensorGpu &gpu = NkTensorGpu::Get();
	printf("\nGPU compute : %s (%s)\n", gpu.IsAvailable() ? "OUI" : "NON", gpu.BackendName());
	if (gpu.IsAvailable()) {
		nn::NkMultiHeadAttention attnC(4, 2, 7u), attnG(4, 2, 7u); // même graine -> mêmes poids
		NkTensor X = Mat(xs, xd);
		NkVar yC = attnC.Forward(NkVar::Leaf(X, false)); // CPU
		NkVector<NkVar> ps;
		attnG.Parameters(ps);
		for (uint32 i = 0; i < ps.Size(); ++i)
			ps[i].SetValue(ps[i].Value().ToGPU());
		NkVar yG = attnG.Forward(NkVar::Leaf(X.ToGPU(), false)); // GPU-résident
		NkTensor gc = yG.Value().ToCPU().Contiguous();
		NkTensor cc = yC.Value().Contiguous();
		float e = 0;
		const float *a = gc.DataAs<float>();
		const float *b = cc.DataAs<float>();
		for (int64 i = 0; i < cc.Numel(); ++i) {
			float d = std::fabs(a[i] - b[i]);
			if (d > e)
				e = d;
		}
		printf("  sortie GPU vs CPU : err max = %.2e, forme = [%lld,%lld,%lld]\n", e, (long long)gc.Shape()[0],
			   (long long)gc.Shape()[1], (long long)gc.Shape()[2]);
		check(yG.Value().Device() == NkDevice::NK_GPU && gc.Numel() == 12 && e < 1e-3f,
			  "attention GPU-résidente == CPU");
	}

	// (3) JALON brique 7 : un petit GPT sur-apprend une séquence fixe (perte -> ~0).
	{
		printf("\n-- GPT : sur-apprentissage d'une séquence (brique 7) --\n");
		nn::NkGPT gpt(/*vocab*/ 8, /*d*/ 16, /*heads*/ 2, /*layers*/ 2, /*maxT*/ 8, 42u);
		float toks[8] = {1, 2, 3, 4, 5, 6, 7, 0};
		int tgt[8] = {2, 3, 4, 5, 6, 7, 0, 1};
		NkShape tsh;
		tsh.PushBack(1);
		tsh.PushBack(8);
		NkTensor tokTensor = Mat(tsh, toks);
		NkTensor oneHot = NkTensor::Zeros(NkShape{(int64)8, (int64)8});
		{
			float *p = oneHot.DataAs<float>();
			for (int i = 0; i < 8; ++i)
				p[i * 8 + tgt[i]] = 1.f;
		}
		NkVector<NkVar> params;
		gpt.Parameters(params);
		optim::NkAdam adam(params, 0.01f);
		double loss0 = 0, lossN = 0;
		for (int s = 0; s < 200; ++s) {
			NkVar logits = gpt.Forward(tokTensor); // [8, 8]
			NkVar loss = autograd::SoftmaxCrossEntropy(logits, NkVar::Leaf(oneHot, false));
			loss.Backward();
			adam.Step();
			adam.ZeroGrad();
			double lv = loss.Value().ToCPU().GetItem(NkShape{(int64)0});
			if (s == 0)
				loss0 = lv;
			lossN = lv;
			if (s % 40 == 0 || s == 199)
				printf("    step %3d : perte = %.4f\n", s, lv);
		}
		check(lossN < 0.15 && lossN < loss0, "GPT sur-apprend la séquence (perte -> ~0)");

		// (3b) KV-cache : le décodage INCRÉMENTAL (un token à la fois) doit produire
		//      EXACTEMENT les mêmes logits que Forward sur la séquence complète.
		{
			NkVar full = gpt.Forward(tokTensor); // [8,8]
			NkTensor fc = full.Value().ToCPU().Contiguous();
			const float *fp = fc.DataAs<float>();
			nn::NkKVCache cache;
			cache.Reset(2);
			double maxErr = 0.0;
			for (int t = 0; t < 8; ++t) {
				NkTensor tok = NkTensor::Full(NkShape{(int64)1, (int64)1}, (double)toks[t]);
				NkVar step = gpt.ForwardStep(tok, cache);
				NkTensor sc = step.Value().ToCPU().Contiguous();
				const float *spp = sc.DataAs<float>();
				for (int v = 0; v < 8; ++v) {
					double dd = std::fabs((double)spp[v] - (double)fp[t * 8 + v]);
					if (dd > maxErr)
						maxErr = dd;
				}
			}
			printf("    KV-cache err max vs Forward complet = %.2e\n", maxErr);
			check(maxErr < 1e-3, "KV-cache : logits incrémentaux == Forward complet");
		}
	}

	// (4) Échantillonnage top-k / top-p (NkSampling.h).
	{
		printf("\n-- Échantillonnage (température / top-k / top-p) --\n");
		float lg[5] = {0.1f, 3.0f, 0.2f, 2.0f, -1.0f}; // argmax = index 1
		uint64 rng = 12345u;
		gpt::NkSampleParams pk;
		pk.temperature = 1.0;
		pk.topK = 1; // top-k=1 => toujours l'argmax
		bool allArgmax = true;
		for (int i = 0; i < 30; ++i)
			if (gpt::NkSampleToken(lg, 5, pk, rng) != 1)
				allArgmax = false;
		check(allArgmax, "top-k=1 = argmax déterministe");

		gpt::NkSampleParams pt;
		pt.temperature = 0.0; // température nulle => argmax
		check(gpt::NkSampleToken(lg, 5, pt, rng) == 1, "température 0 = argmax");

		gpt::NkSampleParams pp;
		pp.temperature = 1.0;
		pp.topP = 0.9; // nucleus : exclut la longue traîne (index 4, proba minuscule)
		bool noTail = true;
		for (int i = 0; i < 300; ++i)
			if (gpt::NkSampleToken(lg, 5, pp, rng) == 4)
				noTail = false;
		check(noTail, "top-p exclut la longue traîne");
	}

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
	gpu.Shutdown();
	return g_fail;
}
