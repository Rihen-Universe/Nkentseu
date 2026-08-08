// =============================================================================
// NKQwen2BackwardTest — jalon 2 du chantier QLoRA (cf Documentation/
// notes_etape4_qlora.md §2.3-§2.4 et §4) : prouve par GRADIENT NUMÉRIQUE le
// backward manuel d'une couche Qwen2 + les adaptateurs LoRA. CPU pur, sans GPU.
//
// Quatre parties :
//   (a) chaque brique ISOLÉE contre les différences finies centrées
//       (dL/dθ ≈ (L(θ+ε)−L(θ−ε))/2ε, ε=1e-3, tolérance relative 2e-2 en f32,
//       même méthode que NKAutogradTest) : RMSNorm, RoPE, attention GQA,
//       SwiGLU, LoRA seule — sur une mini-config synthétique (d=32, 2 têtes Q /
//       1 tête KV, ffn=64, T=4), poids reproductibles par graine fixe ;
//   (b) la couche COMPLÈTE : cohérence bit-à-bit avec le forward d'inférence
//       (sans LoRA, puis avec LoRA B=0), puis gradient numérique sur dX et sur
//       dA/dB de CHACUN des 7 adaptateurs (~20 coordonnées échantillonnées par
//       tenseur — tout vérifier ferait exploser le coût) ;
//   (c) un pas de descente JOUET : cible = sortie de la même couche avec un
//       LoRA « professeur », élève initialisé B=0, 50 itérations Adam
//       (optim::NkAdam réutilisé tel quel) sur A/B seuls -> la perte doit
//       être divisée par au moins 10 ;
//   (d) BONUS optionnel (argv[1] = GGUF réel) : lire tokenizer.ggml.tokens et
//       dire lequel des trois spéciaux ChatML manque à la recherche par chaîne
//       (le jalon 1 en a trouvé 2/3). Sans argument : sauté, jamais un échec.
//
// Usage : NKQwen2BackwardTest.exe [chemin_gguf_ou_blob_ollama]
// =============================================================================
#include "NKInfer/NkQwen2Backward.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKTensor/NkTensorOps.h"
#include "NKAutograd/NkVar.h"
#include "NKOptim/NkOptim.h"

#include <cstdio>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;
using namespace nkentseu::ai::infer;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

// -----------------------------------------------------------------------------
// Outils : remplissage gaussien reproductible + perte « somme pondérée ».
// La perte L = Σ y_ij·Wl_ij (poids Wl fixes aléatoires) donne dL/dy = Wl : un
// gradient de sortie RICHE (pas un simple Sum dont dY=1 partout) qui exerce
// toutes les colonnes du backward. Accumulée en double pour que le bruit f32
// du forward ne domine pas la différence centrée.
// -----------------------------------------------------------------------------
static void FillGaussian(NkTensor &t, NkLoraRng &rng, float sigma) {
	float *p = t.DataAs<float>();
	const int64 n = t.Numel();
	for (int64 i = 0; i < n; ++i)
		p[i] = sigma * rng.NextGaussian();
}

static double WeightedSum(const NkTensor &y, const NkTensor &wl) {
	if (!y.IsValid() || !wl.IsValid() || y.Numel() != wl.Numel())
		return 0.0; // sera détecté comme divergence par le check
	NkTensor yc = y.IsContiguous() ? y : y.Contiguous();
	NkTensor wc = wl.IsContiguous() ? wl : wl.Contiguous();
	const float *yp = yc.DataAs<float>();
	const float *wp = wc.DataAs<float>();
	double s = 0.0;
	const int64 n = yc.Numel();
	for (int64 i = 0; i < n; ++i)
		s += (double)yp[i] * (double)wp[i];
	return s;
}

// -----------------------------------------------------------------------------
// Vérification numérique : perturbe `param` EN PLACE (les closures de perte
// tiennent des VUES partagées du même stockage — la mutation est donc visible
// du forward rejoué), compare le gradient analytique aux différences finies
// centrées sur `nSamples` coordonnées (toutes si le tenseur est petit).
//
// Critère combiné (même logique que torch.allclose / gradcheck) :
//     |num − ana| < atol + rtol·max(|num|,|ana|)      rtol=2e-2, atol=1e-3
// POURQUOI un atol : la différence centrée d'un forward f32 porte un BRUIT
// absolu irréductible ≈ |L|·ε_f32/(2ε) (~3-4e-4 mesuré ici) — un critère
// purement relatif condamne les coordonnées de gradient minuscule alors que
// leur erreur ABSOLUE est au niveau du bruit, pas d'une dérivée fausse (une
// dérivée fausse donne des écarts O(1), observables sur les grosses coords).
// -----------------------------------------------------------------------------
template <typename LossFn>
static double NumGradCheck(const char *name, NkTensor param, const NkTensor &analytic, LossFn loss, int32 nSamples,
						   NkLoraRng &rng) {
	NkTensor ac = analytic.IsContiguous() ? analytic : analytic.Contiguous();
	const float *ap = ac.DataAs<float>();
	float *pp = param.DataAs<float>();
	const int64 n = param.Numel();
	// Choix de ε : deux erreurs opposées — le bruit f32 du forward (∝ 1/ε) et
	// la troncature O(ε²·f'''). À ε=1e-3 le bruit domine ; ε=5e-3 le divise
	// par 5 tandis que la troncature reste ~1e-5 relatif sur ces fonctions
	// lisses : c'est l'équilibre pour un forward f32.
	const float eps = 5e-3f;
	const double rtol = 2e-2, atol = 1e-3;
	const bool testAll = (n <= (int64)nSamples);
	const int32 count = testAll ? (int32)n : nSamples;
	double maxRatio = 0.0, worstAbs = 0.0, worstRel = 0.0;
	for (int32 s = 0; s < count; ++s) {
		const int64 i = testAll ? (int64)s : (int64)(rng.NextU64() % (uint64)n);
		const float o = pp[i];
		pp[i] = o + eps;
		const double Lp = loss();
		pp[i] = o - eps;
		const double Lm = loss();
		pp[i] = o;
		const double num = (Lp - Lm) / (2.0 * (double)eps);
		const double ana = (double)ap[i];
		const double absErr = std::fabs(num - ana);
		const double mag = std::fmax(std::fabs(num), std::fabs(ana));
		const double ratio = absErr / (atol + rtol * mag);
		if (ratio > maxRatio) {
			maxRatio = ratio;
			worstAbs = absErr;
			worstRel = absErr / std::fmax(mag, 1e-12);
		}
	}
	const bool ok = maxRatio < 1.0;
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %-34s pire coord : |Δ|=%.2e, rel=%.2e, score=%.2f (%d coords)\n", ok ? "OK" : "KO", name,
		   worstAbs, worstRel, maxRatio, count);
	return maxRatio;
}

// Plus grand écart absolu entre deux tenseurs de même taille.
static double MaxAbsDiff(const NkTensor &a, const NkTensor &b) {
	if (!a.IsValid() || !b.IsValid() || a.Numel() != b.Numel())
		return 1e30;
	NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
	NkTensor bc = b.IsContiguous() ? b : b.Contiguous();
	const float *pa = ac.DataAs<float>();
	const float *pb = bc.DataAs<float>();
	double m = 0.0;
	const int64 n = ac.Numel();
	for (int64 i = 0; i < n; ++i) {
		const double d = std::fabs((double)pa[i] - (double)pb[i]);
		if (d > m)
			m = d;
	}
	return m;
}

// -----------------------------------------------------------------------------
// Mini-config jouet partagée par (b) et (c) : d=32, 2 têtes Q / 1 tête KV
// (groupe GQA de 2), headDim=16, ffn=64, T=4. Poids ~N(0, 1/√in) pour des
// activations d'ordre 1 (gradients ni écrasés ni explosés).
// -----------------------------------------------------------------------------
static NkQwen2Config MiniConfig() {
	NkQwen2Config cfg;
	cfg.dModel = 32;
	cfg.nHeads = 2;
	cfg.nKVHeads = 1;
	cfg.headDim = 16;
	cfg.ffnDim = 64;
	cfg.ropeFreqBase = 10000.0f;
	cfg.rmsEps = 1e-5f;
	return cfg;
}

static NkQwen2LayerWeights MiniWeights(const NkQwen2Config &cfg, NkLoraRng &rng) {
	const int64 d = cfg.dModel, qd = (int64)cfg.nHeads * cfg.headDim, kvd = (int64)cfg.nKVHeads * cfg.headDim;
	const int64 ffn = cfg.ffnDim;
	NkQwen2LayerWeights w;
	auto gauss = [&](const NkShape &shape, float sigma) {
		NkTensor t = NkTensor::Empty(shape, NkDType::NK_F32);
		FillGaussian(t, rng, sigma);
		return t;
	};
	const float sd = (float)(1.0 / std::sqrt((double)d));
	const float sf = (float)(1.0 / std::sqrt((double)ffn));
	// Normes ~1 (comme un modèle entraîné), biais petits mais non nuls.
	w.attnNorm = gauss(NkShape{d}, 0.1f);
	w.ffnNorm = gauss(NkShape{d}, 0.1f);
	{
		float *p1 = w.attnNorm.DataAs<float>();
		float *p2 = w.ffnNorm.DataAs<float>();
		for (int64 i = 0; i < d; ++i) {
			p1[i] += 1.0f;
			p2[i] += 1.0f;
		}
	}
	w.wq = gauss(NkShape{qd, d}, sd);
	w.bq = gauss(NkShape{qd}, 0.1f);
	w.wk = gauss(NkShape{kvd, d}, sd);
	w.bk = gauss(NkShape{kvd}, 0.1f);
	w.wv = gauss(NkShape{kvd, d}, sd);
	w.bv = gauss(NkShape{kvd}, 0.1f);
	w.wo = gauss(NkShape{d, qd}, sd);
	w.wGate = gauss(NkShape{ffn, d}, sd);
	w.wUp = gauss(NkShape{ffn, d}, sd);
	w.wDown = gauss(NkShape{d, ffn}, sf);
	return w;
}

// Paire LoRA de test : A gaussienne (Create) + B RANDOMISÉE — l'init standard
// B=0 annulerait dA (du = dY·B = 0) et le gradient numérique ne prouverait
// rien ; B=0 est testé à part (bit-exactitude et descente jouet).
static NkLoraPair MakeLoraNonZero(int32 outF, int32 inF, int32 r, float32 alpha, NkLoraRng &rng) {
	NkLoraPair p = NkLoraPair::Create(outF, inF, r, alpha, 0.2f, rng);
	FillGaussian(p.B, rng, 0.2f);
	return p;
}

// =============================================================================
// PARTIE (a) : briques isolées.
// =============================================================================
static void RunBrickTests() {
	printf("-- PARTIE (a) : briques isolées vs gradient numérique (CPU pur) --\n");
	NkLoraRng rng(42);
	NkLoraRng pick(1234); // échantillonnage des coordonnées, indépendant des poids

	// ---- RMSNorm dX ----
	{
		NkTensor x = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
		FillGaussian(x, rng, 1.0f);
		NkTensor wgt = NkTensor::Empty(NkShape{32}, NkDType::NK_F32);
		FillGaussian(wgt, rng, 0.1f);
		{
			float *p = wgt.DataAs<float>();
			for (int64 i = 0; i < 32; ++i)
				p[i] += 1.0f;
		}
		NkTensor wl = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
		FillGaussian(wl, rng, 1.0f);
		const float eps = 1e-5f;
		NkTensor dX = NkRMSNormBackward(x, wgt, eps, wl);
		auto loss = [&]() { return WeightedSum(NkRMSNorm(x, wgt, eps), wl); };
		NumGradCheck("RMSNorm : dX", x, dX, loss, 30, pick);
	}

	// ---- RoPE dX (posOffset non nul : exerce la dépendance en position) ----
	{
		NkTensor x = NkTensor::Empty(NkShape{2, 4, 16}, NkDType::NK_F32);
		FillGaussian(x, rng, 1.0f);
		NkTensor wl = NkTensor::Empty(NkShape{2, 4, 16}, NkDType::NK_F32);
		FillGaussian(wl, rng, 1.0f);
		const int32 posOffset = 3;
		const float base = 10000.0f;
		NkTensor dX = wl.Clone();
		NkApplyRoPEBackward(dX, posOffset, base); // rotation inverse du gradient
		auto loss = [&]() {
			NkTensor y = x.Clone();
			NkApplyRoPE(y, posOffset, base);
			return WeightedSum(y, wl);
		};
		NumGradCheck("RoPE : dX (rotation inverse)", x, dX, loss, 30, pick);
	}

	// ---- Attention GQA : dQ, dK, dV (2 têtes Q partagent 1 tête KV) ----
	{
		NkTensor Q = NkTensor::Empty(NkShape{2, 4, 16}, NkDType::NK_F32);
		NkTensor K = NkTensor::Empty(NkShape{1, 4, 16}, NkDType::NK_F32);
		NkTensor V = NkTensor::Empty(NkShape{1, 4, 16}, NkDType::NK_F32);
		FillGaussian(Q, rng, 0.5f);
		FillGaussian(K, rng, 0.5f);
		FillGaussian(V, rng, 1.0f);
		NkTensor wl = NkTensor::Empty(NkShape{2, 4, 16}, NkDType::NK_F32);
		FillGaussian(wl, rng, 1.0f);
		NkTensor probs;
		NkTensor ctx = NkGQAAttentionForward(Q, K, V, &probs);
		Check(ctx.IsValid() && probs.IsValid(), "GQA : forward valide (ctx + probs)");
		NkTensor dQ, dK, dV;
		Check(NkGQAAttentionBackward(Q, K, V, probs, wl, dQ, dK, dV), "GQA : backward valide");
		auto loss = [&]() { return WeightedSum(NkGQAAttentionForward(Q, K, V, nullptr), wl); };
		NumGradCheck("GQA : dQ", Q, dQ, loss, 20, pick);
		NumGradCheck("GQA : dK (accumulation du groupe)", K, dK, loss, 20, pick);
		NumGradCheck("GQA : dV (accumulation du groupe)", V, dV, loss, 20, pick);
	}

	// ---- SwiGLU : dG, dU ----
	{
		NkTensor g = NkTensor::Empty(NkShape{4, 64}, NkDType::NK_F32);
		NkTensor u = NkTensor::Empty(NkShape{4, 64}, NkDType::NK_F32);
		FillGaussian(g, rng, 1.0f);
		FillGaussian(u, rng, 1.0f);
		NkTensor wl = NkTensor::Empty(NkShape{4, 64}, NkDType::NK_F32);
		FillGaussian(wl, rng, 1.0f);
		NkTensor dG, dU;
		Check(NkSwiGLUBackward(g, u, wl, dG, dU), "SwiGLU : backward valide");
		auto loss = [&]() { return WeightedSum(ops::Mul(NkSiLU(g), u), wl); };
		NumGradCheck("SwiGLU : dG (via silu')", g, dG, loss, 20, pick);
		NumGradCheck("SwiGLU : dU", u, dU, loss, 20, pick);
	}

	// ---- LoRA seule : dA, dB, dX ----
	{
		NkTensor x = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
		FillGaussian(x, rng, 1.0f);
		NkLoraPair p = MakeLoraNonZero(24, 32, 4, 8.0f, rng);
		NkTensor wl = NkTensor::Empty(NkShape{4, 24}, NkDType::NK_F32);
		FillGaussian(wl, rng, 1.0f);
		NkLoraGrad grad;
		NkTensor dX; // alloué à zéro par NkLoraBackward (sémantique +=)
		dX = NkTensor::Zeros(NkShape{4, 32}, NkDType::NK_F32);
		Check(NkLoraBackward(p, x, wl, grad, &dX), "LoRA : backward valide");
		auto loss = [&]() { return WeightedSum(NkLoraForwardDelta(p, x), wl); };
		NumGradCheck("LoRA : dA", p.A, grad.dA, loss, 20, pick);
		NumGradCheck("LoRA : dB", p.B, grad.dB, loss, 20, pick);
		NumGradCheck("LoRA : dX (traverse)", x, dX, loss, 20, pick);
	}
	printf("\n");
}

// =============================================================================
// PARTIE (b) : couche complète.
// =============================================================================
static void RunFullLayerTests() {
	printf("-- PARTIE (b) : couche Qwen2 complète (bit-exactitude + gradients) --\n");
	NkLoraRng rng(7);
	NkLoraRng pick(4321);
	const NkQwen2Config cfg = MiniConfig();
	NkQwen2LayerWeights w = MiniWeights(cfg, rng);
	NkTensor x = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
	FillGaussian(x, rng, 1.0f);

	// ---- cohérence avec le forward d'inférence (NON MODIFIÉ) ----
	{
		NkKVCacheLayer cache; // vide -> position 0, séquence complète
		NkTensor yInfer = NkQwen2LayerForward(cfg, w, x, cache);
		NkQwen2LoraSet none; // aucune paire : chemin socle pur
		NkQwen2TrainSaved saved;
		NkTensor yTrain = NkQwen2LayerForwardTrain(cfg, w, none, x, saved);
		const double diff = MaxAbsDiff(yInfer, yTrain);
		printf("    |forwardTrain - forwardInference| max = %.3e (sans LoRA)\n", diff);
		Check(yInfer.IsValid() && yTrain.IsValid() && diff == 0.0,
			  "ForwardTrain sans LoRA == NkQwen2LayerForward (écart 0.0)");
	}
	{
		// LoRA à l'init STANDARD (B=0) : delta exactement nul, la sortie doit
		// rester celle du socle — c'est la garantie « insérer ne change rien ».
		NkQwen2LoraSet lz;
		lz.q = NkLoraPair::Create(32, 32, 4, 8.0f, 0.2f, rng);
		lz.k = NkLoraPair::Create(16, 32, 4, 8.0f, 0.2f, rng);
		lz.v = NkLoraPair::Create(16, 32, 4, 8.0f, 0.2f, rng);
		lz.o = NkLoraPair::Create(32, 32, 4, 8.0f, 0.2f, rng);
		lz.gate = NkLoraPair::Create(64, 32, 4, 8.0f, 0.2f, rng);
		lz.up = NkLoraPair::Create(64, 32, 4, 8.0f, 0.2f, rng);
		lz.down = NkLoraPair::Create(32, 64, 4, 8.0f, 0.2f, rng);
		NkKVCacheLayer cache;
		NkTensor yInfer = NkQwen2LayerForward(cfg, w, x, cache);
		NkQwen2TrainSaved saved;
		NkTensor yTrain = NkQwen2LayerForwardTrain(cfg, w, lz, x, saved);
		const double diff = MaxAbsDiff(yInfer, yTrain);
		Check(diff == 0.0, "ForwardTrain avec LoRA B=0 == socle (delta initial nul, écart 0.0)");
	}

	// ---- gradients de la couche complète (LoRA non triviaux sur les 7) ----
	NkQwen2LoraSet lset;
	lset.q = MakeLoraNonZero(32, 32, 4, 8.0f, rng);
	lset.k = MakeLoraNonZero(16, 32, 4, 8.0f, rng);
	lset.v = MakeLoraNonZero(16, 32, 4, 8.0f, rng);
	lset.o = MakeLoraNonZero(32, 32, 4, 8.0f, rng);
	lset.gate = MakeLoraNonZero(64, 32, 4, 8.0f, rng);
	lset.up = MakeLoraNonZero(64, 32, 4, 8.0f, rng);
	lset.down = MakeLoraNonZero(32, 64, 4, 8.0f, rng);

	NkTensor wl = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
	FillGaussian(wl, rng, 1.0f);

	NkQwen2TrainSaved saved;
	NkTensor y = NkQwen2LayerForwardTrain(cfg, w, lset, x, saved);
	Check(y.IsValid(), "ForwardTrain avec 7 adaptateurs valide");
	NkTensor dX;
	NkQwen2LoraSetGrads grads;
	Check(NkQwen2LayerBackward(cfg, w, lset, saved, wl, dX, grads), "Backward de la couche valide");

	// Le forward est rejoué À CHAQUE perturbation (checkpointing absorbé : le
	// coût du gradient numérique, ~600 forwards, reste négligeable en jouet).
	auto loss = [&]() {
		NkQwen2TrainSaved s;
		return WeightedSum(NkQwen2LayerForwardTrain(cfg, w, lset, x, s), wl);
	};
	NumGradCheck("couche : dX (traverse la couche)", x, dX, loss, 20, pick);
	NumGradCheck("couche : dA q", lset.q.A, grads.q.dA, loss, 20, pick);
	NumGradCheck("couche : dB q", lset.q.B, grads.q.dB, loss, 20, pick);
	NumGradCheck("couche : dA k", lset.k.A, grads.k.dA, loss, 20, pick);
	NumGradCheck("couche : dB k", lset.k.B, grads.k.dB, loss, 20, pick);
	NumGradCheck("couche : dA v", lset.v.A, grads.v.dA, loss, 20, pick);
	NumGradCheck("couche : dB v", lset.v.B, grads.v.dB, loss, 20, pick);
	NumGradCheck("couche : dA o", lset.o.A, grads.o.dA, loss, 20, pick);
	NumGradCheck("couche : dB o", lset.o.B, grads.o.dB, loss, 20, pick);
	NumGradCheck("couche : dA gate", lset.gate.A, grads.gate.dA, loss, 20, pick);
	NumGradCheck("couche : dB gate", lset.gate.B, grads.gate.dB, loss, 20, pick);
	NumGradCheck("couche : dA up", lset.up.A, grads.up.dA, loss, 20, pick);
	NumGradCheck("couche : dB up", lset.up.B, grads.up.dB, loss, 20, pick);
	NumGradCheck("couche : dA down", lset.down.A, grads.down.dA, loss, 20, pick);
	NumGradCheck("couche : dB down", lset.down.B, grads.down.dB, loss, 20, pick);
	printf("\n");
}

// =============================================================================
// PARTIE (c) : descente jouet — 50 pas d'Adam sur A/B seuls.
// =============================================================================
static double MSE(const NkTensor &a, const NkTensor &b) {
	NkTensor ac = a.Contiguous();
	NkTensor bc = b.Contiguous();
	const float *pa = ac.DataAs<float>();
	const float *pb = bc.DataAs<float>();
	double s = 0.0;
	const int64 n = ac.Numel();
	for (int64 i = 0; i < n; ++i) {
		const double d = (double)pa[i] - (double)pb[i];
		s += d * d;
	}
	return s / (double)n;
}

static void RunToyDescent() {
	printf("-- PARTIE (c) : descente jouet (Adam sur A/B seuls, socle gelé) --\n");
	NkLoraRng rng(99);
	const NkQwen2Config cfg = MiniConfig();
	NkQwen2LayerWeights w = MiniWeights(cfg, rng);
	NkTensor x = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
	FillGaussian(x, rng, 1.0f);

	// Cible : la MÊME couche avec un LoRA « professeur » (même rang) — un
	// objectif ATTEIGNABLE par l'élève, contrairement à une cible arbitraire
	// qu'un rang 4 ne pourrait pas approcher d'un facteur 10 en 50 pas.
	NkQwen2LoraSet teacher;
	teacher.q = MakeLoraNonZero(32, 32, 4, 8.0f, rng);
	teacher.k = MakeLoraNonZero(16, 32, 4, 8.0f, rng);
	teacher.v = MakeLoraNonZero(16, 32, 4, 8.0f, rng);
	teacher.o = MakeLoraNonZero(32, 32, 4, 8.0f, rng);
	teacher.gate = MakeLoraNonZero(64, 32, 4, 8.0f, rng);
	teacher.up = MakeLoraNonZero(64, 32, 4, 8.0f, rng);
	teacher.down = MakeLoraNonZero(32, 64, 4, 8.0f, rng);
	NkTensor yTarget;
	{
		NkQwen2TrainSaved s;
		yTarget = NkQwen2LayerForwardTrain(cfg, w, teacher, x, s);
	}

	// Élève : init STANDARD (A gaussienne, B=0) -> part de la sortie du socle.
	NkQwen2LoraSet st;
	st.q = NkLoraPair::Create(32, 32, 4, 8.0f, 0.1f, rng);
	st.k = NkLoraPair::Create(16, 32, 4, 8.0f, 0.1f, rng);
	st.v = NkLoraPair::Create(16, 32, 4, 8.0f, 0.1f, rng);
	st.o = NkLoraPair::Create(32, 32, 4, 8.0f, 0.1f, rng);
	st.gate = NkLoraPair::Create(64, 32, 4, 8.0f, 0.1f, rng);
	st.up = NkLoraPair::Create(64, 32, 4, 8.0f, 0.1f, rng);
	st.down = NkLoraPair::Create(32, 64, 4, 8.0f, 0.1f, rng);

	// optim::NkAdam réutilisé TEL QUEL : les A/B deviennent des feuilles NkVar
	// (hors graphe — le gradient est posé À LA MAIN via SetGrad, l'autograd ne
	// connaît pas ces ops), l'optimiseur ne voit QUE les adaptateurs — le gel
	// du socle est structurel, pas un masque.
	NkVector<NkVar> params;
	NkLoraPair *pairs[7] = {&st.q, &st.k, &st.v, &st.o, &st.gate, &st.up, &st.down};
	for (int32 i = 0; i < 7; ++i) {
		params.PushBack(NkVar::Leaf(pairs[i]->A, true));
		params.PushBack(NkVar::Leaf(pairs[i]->B, true));
	}
	optim::NkAdam adam(params, 0.05f);

	double firstLoss = -1.0, lastLoss = -1.0;
	const int64 numel = 4 * 32;
	for (int32 it = 0; it < 50; ++it) {
		NkQwen2TrainSaved saved;
		NkTensor y = NkQwen2LayerForwardTrain(cfg, w, st, x, saved);
		const double L = MSE(y, yTarget);
		if (it == 0)
			firstLoss = L;
		lastLoss = L;

		// dL/dy pour L = mean((y−t)²) : 2(y−t)/N.
		NkTensor dY = NkTensor::Empty(NkShape{4, 32}, NkDType::NK_F32);
		{
			NkTensor yc = y.Contiguous();
			NkTensor tc = yTarget.Contiguous();
			const float *yp = yc.DataAs<float>();
			const float *tp = tc.DataAs<float>();
			float *dp = dY.DataAs<float>();
			for (int64 i = 0; i < numel; ++i)
				dp[i] = (float)(2.0 * ((double)yp[i] - (double)tp[i]) / (double)numel);
		}

		NkTensor dX;
		NkQwen2LoraSetGrads g;
		if (!NkQwen2LayerBackward(cfg, w, st, saved, dY, dX, g)) {
			Check(false, "backward pendant la descente");
			return;
		}
		NkLoraGrad *gr[7] = {&g.q, &g.k, &g.v, &g.o, &g.gate, &g.up, &g.down};
		for (int32 i = 0; i < 7; ++i) {
			params[(nk_size)(2 * i)].SetGrad(gr[i]->dA);
			params[(nk_size)(2 * i + 1)].SetGrad(gr[i]->dB);
		}
		adam.Step();
		// SetValue remplace le tenseur du nœud : on resynchronise les vues des
		// paires pour que le prochain forward lise les poids mis à jour.
		for (int32 i = 0; i < 7; ++i) {
			pairs[i]->A = params[(nk_size)(2 * i)].Value();
			pairs[i]->B = params[(nk_size)(2 * i + 1)].Value();
		}
	}
	printf("    perte : initiale = %.6e, finale (50 pas Adam) = %.6e, facteur = %.1fx\n", firstLoss, lastLoss,
		   lastLoss > 0.0 ? firstLoss / lastLoss : 1e30);
	Check(firstLoss > 0.0 && lastLoss < firstLoss, "la perte décroît strictement");
	Check(lastLoss * 10.0 <= firstLoss, "la perte est divisée par au moins 10 en 50 pas");
	printf("\n");
}

// =============================================================================
// PARTIE (d) : diagnostic BONUS des tokens spéciaux ChatML (GGUF optionnel).
// =============================================================================
static void RunSpecialsDiagnostic(const char *ggufPath) {
	printf("-- PARTIE (d) : diagnostic des spéciaux ChatML (GGUF optionnel) --\n");
	if (!ggufPath) {
		printf("  SAUTÉ : aucun chemin GGUF fourni en argument 1 -- ce n'est pas un échec.\n\n");
		return;
	}
	printf("  GGUF : %s\n", ggufPath);
	NkVector<NkString> tokens;
	if (!NkGGUFReadFullStringArray(ggufPath, "tokenizer.ggml.tokens", tokens)) {
		printf("  tokenizer.ggml.tokens illisible -- diagnostic impossible (pas un échec du jalon).\n\n");
		return;
	}
	Check(tokens.Size() > 100000, "vocabulaire réel lu en entier (> 100 000 tokens)");
	const char *specials[3] = {"<|endoftext|>", "<|im_start|>", "<|im_end|>"};
	int32 nFound = 0;
	for (int32 s = 0; s < 3; ++s) {
		const usize len = (usize)std::strlen(specials[s]);
		int64 id = -1;
		for (nk_size i = 0; i < tokens.Size(); ++i) {
			if ((usize)tokens[i].Size() == len && std::memcmp(tokens[i].Data(), specials[s], len) == 0) {
				id = (int64)i;
				break;
			}
		}
		if (id >= 0) {
			printf("    %-16s -> id %lld\n", specials[s], (long long)id);
			++nFound;
		} else {
			printf("    %-16s -> ABSENT de la recherche par chaîne exacte\n", specials[s]);
		}
	}
	printf("  verdict : %d/3 spéciaux trouvés par chaîne (le jalon 1 en trouvait 2/3).\n", nFound);
	// Contre-diagnostic : montre ce que contiennent RÉELLEMENT les ids
	// canoniques Qwen2 (151643=<|endoftext|>, 151644=<|im_start|>,
	// 151645=<|im_end|>) — si la recherche par chaîne échoue, c'est que le
	// vocabulaire de CE blob stocke autre chose à ces positions (octets
	// échappés pour rendre visible tout caractère non imprimable).
	for (int64 id = 151643; id <= 151645 && id < (int64)tokens.Size(); ++id) {
		const NkString &s = tokens[(nk_size)id];
		printf("    vocab[%lld] = \"", (long long)id);
		for (nk_size b = 0; b < s.Size(); ++b) {
			const unsigned char c = (unsigned char)s.Data()[b];
			if (c >= 32 && c < 127)
				printf("%c", c);
			else
				printf("\\x%02X", c);
		}
		printf("\" (%u octets)\n", (uint32)s.Size());
	}
	printf("\n");
}

int main(int argc, char **argv) {
	printf("=== NKQwen2BackwardTest : backward Qwen2 + LoRA (QLoRA jalon 2, CPU pur) ===\n\n");

	RunBrickTests();
	RunFullLayerTests();
	RunToyDescent();
	RunSpecialsDiagnostic(argc > 1 ? argv[1] : nullptr);

	printf("=== Résultat : %d/%d OK ===\n", g_pass, g_pass + g_fail);
	return g_fail == 0 ? 0 : 1;
}
