// =============================================================================
// NKQwen2SftTest — jalon 3 du chantier QLoRA (cf Documentation/
// notes_etape4_qlora.md §2.5 et §4) : la boucle SFT complète sur mini-modèle,
// CPU pur — NkQwen2Sft assemblé au-dessus des jalons 1 (tokenizer) et 2
// (backward de couche + LoRA), sans les modifier.
//
// Quatre parties :
//   (a) mini-modèle synthétique (2 couches, d=32, 2 têtes Q / 1 tête KV,
//       ffn=64, vocab=267 = mini-tokenizer synthétique du jalon 1) : gradient
//       NUMÉRIQUE de bout en bout — embedding gelé -> 2 couches chaînées ->
//       RMSNorm final + lm_head gelés -> CE MASQUÉE — sur ~10 coordonnées de
//       dA/dB de CHAQUE adaptateur de CHAQUE couche (28 tenseurs) ;
//   (b) preuve du MASQUAGE : perturber les logits d'une position de PROMPT ne
//       change pas la perte (égalité exacte), d'une position de RÉPONSE la
//       change ; le gradient des lignes masquées est exactement nul ;
//   (c) SFT jouet : 30 paires Q/R en dur (« q: couleur ciel » -> « r: bleu »,
//       6 sujets × 5 formulations), 200 pas Adam sur les adaptateurs seuls ->
//       perte divisée par >= 5, ET la perte sur 5 paires de VALIDATION (6e
//       formulation, jamais vue) baisse aussi ; le socle reste BIT-IDENTIQUE ;
//   (d) OPTIONNEL (argv[1] = blob GGUF Qwen2.5) : vocabulaire réel, ids des 3
//       spéciaux ChatML, formatage ChatML de la première paire réelle du
//       corpus BulkGen (40 premiers ids + masque). Sans argument : SAUTÉ,
//       jamais un échec.
//
// Usage : NKQwen2SftTest.exe [chemin_gguf_ou_blob_ollama] [chemin_corpus]
//   corpus par défaut : D:\Projets\Camrail\AI\BulkGen\dlg_ollama_fr.txt
// =============================================================================
#include "NKInfer/NkQwen2Sft.h"
#include "NKInfer/NkLora.h"
#include "NKFileSystem/NkFile.h"

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
// Mini-tokenizer synthétique du jalon 1 (reconstruit ici comme dans
// NKQwenTokenizerTest : 256 octets remappés + 8 fusions + 3 spéciaux ChatML =
// 267 tokens). La table bytes_to_unicode est recalculée depuis la spec GPT-2,
// indépendamment du code de production.
// -----------------------------------------------------------------------------
static int32 TestByteToCp(int32 b) {
	const bool keep = (b >= 33 && b <= 126) || (b >= 161 && b <= 172) || (b >= 174 && b <= 255);
	if (keep)
		return b;
	int32 n = 0;
	for (int32 x = 0; x < b; ++x) {
		const bool k = (x >= 33 && x <= 126) || (x >= 161 && x <= 172) || (x >= 174 && x <= 255);
		if (!k)
			n++;
	}
	return 256 + n;
}

static NkString TestCpToUtf8(int32 cp) {
	NkString s;
	if (cp < 0x80) {
		s.Append((char)cp);
	} else {
		s.Append((char)(0xC0 | (cp >> 6)));
		s.Append((char)(0x80 | (cp & 0x3F)));
	}
	return s;
}

static void BuildMiniVocab(NkVector<NkString> &tokens, NkVector<NkString> &merges) {
	for (int32 b = 0; b < 256; ++b)
		tokens.PushBack(TestCpToUtf8(TestByteToCp(b)));
	const NkString G = TestCpToUtf8(TestByteToCp((int32)' ')); // « Ġ »
	tokens.PushBack(NkString("Bo"));
	tokens.PushBack(NkString("nj"));
	tokens.PushBack(NkString("Bonj"));
	tokens.PushBack(NkString("ou"));
	tokens.PushBack(NkString("our"));
	tokens.PushBack(NkString("Bonjour"));
	NkString gl = G;
	gl.Append('l');
	tokens.PushBack(gl);
	NkString gle = gl;
	gle.Append('e');
	tokens.PushBack(gle);
	tokens.PushBack(NkString("<|endoftext|>"));
	tokens.PushBack(NkString("<|im_start|>"));
	tokens.PushBack(NkString("<|im_end|>"));

	merges.PushBack(NkString("B o"));
	merges.PushBack(NkString("n j"));
	merges.PushBack(NkString("Bo nj"));
	merges.PushBack(NkString("o u"));
	merges.PushBack(NkString("ou r"));
	merges.PushBack(NkString("Bonj our"));
	NkString mGl = G;
	mGl.Append(" l");
	merges.PushBack(mGl);
	NkString mGle = gl;
	mGle.Append(" e");
	merges.PushBack(mGle);
}

// -----------------------------------------------------------------------------
// Outils numériques (mêmes conventions que NKQwen2BackwardTest : différences
// finies centrées, critère combiné rtol/atol — cf jalon 2 pour la
// justification de ε=5e-3 et de l'atol face au bruit f32 du forward).
// -----------------------------------------------------------------------------
static void FillGaussian(NkTensor &t, NkLoraRng &rng, float sigma) {
	float *p = t.DataAs<float>();
	const int64 n = t.Numel();
	for (int64 i = 0; i < n; ++i)
		p[i] = sigma * rng.NextGaussian();
}

template <typename LossFn>
static double NumGradCheck(const char *name, NkTensor param, const NkTensor &analytic, LossFn loss, int32 nSamples,
						   NkLoraRng &rng) {
	NkTensor ac = analytic.IsContiguous() ? analytic : analytic.Contiguous();
	const float *ap = ac.DataAs<float>();
	float *pp = param.DataAs<float>();
	const int64 n = param.Numel();
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
	printf("  [ %s ] %-30s pire coord : |Δ|=%.2e, rel=%.2e, score=%.2f (%d coords)\n", ok ? "OK" : "KO", name,
		   worstAbs, worstRel, maxRatio, count);
	return maxRatio;
}

// -----------------------------------------------------------------------------
// Mini-modèle jouet : 2 couches, d=32, 2 têtes Q / 1 tête KV (headDim=16),
// ffn=64, vocab=267 (mini-tokenizer). Poids ~N(0, 1/√in), normes ~1.
// -----------------------------------------------------------------------------
static NkQwen2Config ToyConfig() {
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

static NkQwen2LayerWeights ToyWeights(const NkQwen2Config &cfg, NkLoraRng &rng) {
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

// LoRA de test NON nulle (B randomisée) : l'init standard B=0 annulerait dA
// (du = dY·B = 0) — le gradient numérique ne prouverait rien (cf jalon 2).
static NkLoraPair MakeLoraNonZero(int32 outF, int32 inF, int32 r, float32 alpha, NkLoraRng &rng) {
	NkLoraPair p = NkLoraPair::Create(outF, inF, r, alpha, 0.2f, rng);
	FillGaussian(p.B, rng, 0.2f);
	return p;
}

// Jeu complet d'adaptateurs d'une couche du jouet (dimensions de ToyConfig).
static NkQwen2LoraSet ToyLoraSet(const NkQwen2Config &cfg, int32 r, float32 alpha, float32 sigma, bool zeroB,
								 NkLoraRng &rng) {
	const int32 d = cfg.dModel, qd = cfg.nHeads * cfg.headDim, kvd = cfg.nKVHeads * cfg.headDim, ffn = cfg.ffnDim;
	NkQwen2LoraSet s;
	if (zeroB) {
		s.q = NkLoraPair::Create(qd, d, r, alpha, sigma, rng);
		s.k = NkLoraPair::Create(kvd, d, r, alpha, sigma, rng);
		s.v = NkLoraPair::Create(kvd, d, r, alpha, sigma, rng);
		s.o = NkLoraPair::Create(d, qd, r, alpha, sigma, rng);
		s.gate = NkLoraPair::Create(ffn, d, r, alpha, sigma, rng);
		s.up = NkLoraPair::Create(ffn, d, r, alpha, sigma, rng);
		s.down = NkLoraPair::Create(d, ffn, r, alpha, sigma, rng);
	} else {
		s.q = MakeLoraNonZero(qd, d, r, alpha, rng);
		s.k = MakeLoraNonZero(kvd, d, r, alpha, rng);
		s.v = MakeLoraNonZero(kvd, d, r, alpha, rng);
		s.o = MakeLoraNonZero(d, qd, r, alpha, rng);
		s.gate = MakeLoraNonZero(ffn, d, r, alpha, rng);
		s.up = MakeLoraNonZero(ffn, d, r, alpha, rng);
		s.down = MakeLoraNonZero(d, ffn, r, alpha, rng);
	}
	return s;
}

static NkQwen2SftModel BuildToyModel(int64 vocab, int32 nLayers, int32 r, float32 alpha, float32 sigma, bool zeroB,
									 NkLoraRng &rng) {
	NkQwen2SftModel m;
	m.cfg = ToyConfig();
	const int64 d = m.cfg.dModel;
	m.embedding = NkTensor::Empty(NkShape{vocab, d}, NkDType::NK_F32);
	FillGaussian(m.embedding, rng, 1.0f);
	m.lmHead = NkTensor::Empty(NkShape{vocab, d}, NkDType::NK_F32);
	FillGaussian(m.lmHead, rng, (float)(1.0 / std::sqrt((double)d)));
	m.finalNorm = NkTensor::Empty(NkShape{d}, NkDType::NK_F32);
	FillGaussian(m.finalNorm, rng, 0.1f);
	{
		float *p = m.finalNorm.DataAs<float>();
		for (int64 i = 0; i < d; ++i)
			p[i] += 1.0f;
	}
	for (int32 k = 0; k < nLayers; ++k) {
		m.layers.PushBack(ToyWeights(m.cfg, rng));
		m.lora.PushBack(ToyLoraSet(m.cfg, r, alpha, sigma, zeroB, rng));
	}
	return m;
}

// Décalage entrée/cible d'un exemple (même règle que le trainer : la position
// t est notée si le token t+1 appartient à la réponse) — refait ICI à la main
// pour que le test ne dépende pas du code privé du trainer.
static void SplitForTest(const NkQwen2SftExample &ex, NkVector<int32> &inputs, NkVector<int32> &targets,
						 NkVector<float32> &mask) {
	for (nk_size t = 0; t + 1 < ex.tokens.Size(); ++t) {
		inputs.PushBack(ex.tokens[t]);
		targets.PushBack(ex.tokens[t + 1]);
		mask.PushBack(ex.lossMask[t + 1]);
	}
}

// =============================================================================
// PARTIE (a) : gradient numérique de bout en bout (2 couches chaînées).
// =============================================================================
static void RunEndToEndGradient(const NkQwen2Tokenizer &tok) {
	printf("-- PARTIE (a) : gradient numérique de bout en bout (embedding -> 2 couches -> CE masquée) --\n");
	NkLoraRng rng(2026);
	NkLoraRng pick(777);
	NkQwen2SftModel m = BuildToyModel(tok.VocabSize(), 2, 4, 8.0f, 0.2f, /*zeroB=*/false, rng);
	Check(m.IsValid(), "modèle jouet 2 couches valide (d=32, 2 têtes/1 KV, ffn=64, V=267)");

	NkQwen2SftExample ex;
	NkString err;
	Check(NkQwen2SftFormatChatML(tok, NkString("q: couleur ciel"), NkString("r: bleu"), ex, &err),
		  "formatage ChatML de la paire jouet");
	NkVector<int32> inputs, targets;
	NkVector<float32> mask;
	SplitForTest(ex, inputs, targets, mask);

	NkQwen2SftSaved saved;
	NkTensor logits = NkQwen2SftForward(m, inputs, saved);
	Check(logits.IsValid() && logits.Shape()[0] == (int64)inputs.Size() && logits.Shape()[1] == tok.VocabSize(),
		  "forward complet : logits [T,V] valides");
	NkTensor dLogits;
	int64 active = 0;
	const double L = NkQwen2SftMaskedCE(logits, targets, mask, &dLogits, &active);
	printf("    perte initiale = %.6f, positions actives = %lld / %u\n", L, (long long)active, (uint32)targets.Size());
	Check(L > 0.0 && active > 0 && active < (int64)targets.Size(),
		  "CE masquée : perte > 0, actives = réponse seule (ni 0 ni T)");

	NkVector<NkQwen2LoraSetGrads> grads;
	Check(NkQwen2SftBackward(m, saved, dLogits, grads), "backward complet jusqu'aux dA/dB des 2 couches");

	// Perte rejouée de bout en bout à chaque perturbation (checkpointing
	// absorbé : le forward complet est rejoué, ~2 Mflops en jouet).
	auto loss = [&]() {
		NkQwen2SftSaved s;
		NkTensor lg = NkQwen2SftForward(m, inputs, s);
		return NkQwen2SftMaskedCE(lg, targets, mask, nullptr, nullptr);
	};

	const char *slotNames[7] = {"q", "k", "v", "o", "gate", "up", "down"};
	for (nk_size k = 0; k < m.lora.Size(); ++k) {
		NkLoraPair *pairs[7] = {&m.lora[k].q, &m.lora[k].k, &m.lora[k].v, &m.lora[k].o,
								&m.lora[k].gate, &m.lora[k].up, &m.lora[k].down};
		NkLoraGrad *gr[7] = {&grads[k].q, &grads[k].k, &grads[k].v, &grads[k].o,
							 &grads[k].gate, &grads[k].up, &grads[k].down};
		for (int32 j = 0; j < 7; ++j) {
			char nameA[64], nameB[64];
			snprintf(nameA, sizeof(nameA), "couche %u : dA %s", (uint32)k, slotNames[j]);
			snprintf(nameB, sizeof(nameB), "couche %u : dB %s", (uint32)k, slotNames[j]);
			NumGradCheck(nameA, pairs[j]->A, gr[j]->dA, loss, 10, pick);
			NumGradCheck(nameB, pairs[j]->B, gr[j]->dB, loss, 10, pick);
		}
	}
	printf("\n");
}

// =============================================================================
// PARTIE (b) : preuve du masquage de la perte.
// =============================================================================
static void RunMaskingProof(const NkQwen2Tokenizer &tok) {
	printf("-- PARTIE (b) : masquage — le prompt ne pèse RIEN, la réponse pèse --\n");
	NkLoraRng rng(4242);
	NkQwen2SftModel m = BuildToyModel(tok.VocabSize(), 2, 4, 8.0f, 0.2f, /*zeroB=*/false, rng);

	NkQwen2SftExample ex;
	NkQwen2SftFormatChatML(tok, NkString("q: couleur sang"), NkString("r: rouge"), ex, nullptr);
	NkVector<int32> inputs, targets;
	NkVector<float32> mask;
	SplitForTest(ex, inputs, targets, mask);

	NkQwen2SftSaved saved;
	NkTensor logits = NkQwen2SftForward(m, inputs, saved);
	NkTensor dLogits;
	const double L0 = NkQwen2SftMaskedCE(logits, targets, mask, &dLogits, nullptr);
	Check(logits.IsValid() && L0 > 0.0, "forward + CE de référence valides");

	// Première position de PROMPT (masque 0) et première de RÉPONSE (masque 1).
	int64 tPrompt = -1, tResp = -1;
	for (nk_size t = 0; t < mask.Size(); ++t) {
		if (mask[t] == 0.0f && tPrompt < 0)
			tPrompt = (int64)t;
		if (mask[t] != 0.0f && tResp < 0)
			tResp = (int64)t;
	}
	Check(tPrompt >= 0 && tResp >= 0, "l'exemple contient bien des positions masquées ET actives");

	const int64 V = logits.Shape()[1];
	{
		// Perturbation d'une ligne de PROMPT : la perte doit être IDENTIQUE (la
		// ligne n'entre ni dans la somme ni dans la normalisation).
		NkTensor pert = logits.Clone();
		pert.DataAs<float>()[tPrompt * V + 3] += 0.7f;
		const double Lp = NkQwen2SftMaskedCE(pert, targets, mask, nullptr, nullptr);
		printf("    perturbation PROMPT (t=%lld) : L=%.12f vs %.12f\n", (long long)tPrompt, Lp, L0);
		Check(Lp == L0, "perturber les logits d'une position de PROMPT ne change PAS la perte (égalité exacte)");
	}
	{
		// Même perturbation sur une ligne de RÉPONSE : la perte DOIT bouger.
		NkTensor pert = logits.Clone();
		pert.DataAs<float>()[tResp * V + 3] += 0.7f;
		const double Lr = NkQwen2SftMaskedCE(pert, targets, mask, nullptr, nullptr);
		printf("    perturbation RÉPONSE (t=%lld) : L=%.12f vs %.12f\n", (long long)tResp, Lr, L0);
		Check(std::fabs(Lr - L0) > 1e-6, "perturber les logits d'une position de RÉPONSE change la perte");
	}
	{
		// Le gradient des lignes masquées est EXACTEMENT nul (c'est ce qui
		// distingue la vraie CE masquée du « one-hot mis à zéro », dont le
		// gradient résiduel pousserait le prompt vers l'uniforme).
		const float *dp = dLogits.DataAs<float>();
		bool allZero = true;
		double maxAbsResp = 0.0;
		for (nk_size t = 0; t < mask.Size(); ++t) {
			if (mask[t] != 0.0f) {
				for (int64 c = 0; c < V; ++c)
					maxAbsResp = std::fmax(maxAbsResp, std::fabs((double)dp[(int64)t * V + c]));
				continue;
			}
			for (int64 c = 0; c < V; ++c)
				if (dp[(int64)t * V + c] != 0.0f)
					allZero = false;
		}
		Check(allZero && maxAbsResp > 0.0,
			  "dLogits : lignes de prompt EXACTEMENT nulles, lignes de réponse non nulles");
	}
	printf("\n");
}

// =============================================================================
// PARTIE (c) : SFT jouet — 30 paires, 200 pas Adam, validation jamais vue.
// =============================================================================
struct ToyPair {
		NkString q, r;
};

static void BuildToyCorpus(NkVector<ToyPair> &train, NkVector<ToyPair> &val) {
	// 6 sujets -> 6 réponses ; 5 formulations d'entraînement, une 6e (jamais
	// vue) pour la validation. La règle sujet->couleur est PARTAGÉE : la perte
	// de validation ne peut baisser que si le modèle apprend la structure des
	// réponses, pas la formulation exacte.
	const char *topics[6] = {"ciel", "sang", "feuille", "nuit", "soleil", "neige"};
	const char *colors[6] = {"bleu", "rouge", "vert", "noir", "jaune", "blanc"};
	const char *forms[5] = {"q: couleur %s", "q: dis la couleur %s", "q: quelle est la couleur %s",
							"q: donne la couleur %s", "q: couleur de %s"};
	char buf[128];
	for (int32 f = 0; f < 5; ++f) {
		for (int32 t = 0; t < 6; ++t) {
			ToyPair p;
			snprintf(buf, sizeof(buf), forms[f], topics[t]);
			p.q = NkString(buf);
			snprintf(buf, sizeof(buf), "r: %s", colors[t]);
			p.r = NkString(buf);
			train.PushBack(p);
		}
	}
	for (int32 t = 0; t < 5; ++t) {
		ToyPair p;
		snprintf(buf, sizeof(buf), "q: vite la couleur %s", topics[t]);
		p.q = NkString(buf);
		snprintf(buf, sizeof(buf), "r: %s", colors[t]);
		p.r = NkString(buf);
		val.PushBack(p);
	}
}

static void RunToySft(const NkQwen2Tokenizer &tok) {
	printf("-- PARTIE (c) : SFT jouet — 30 paires, 200 pas Adam sur les adaptateurs seuls --\n");
	NkLoraRng rng(31337);
	// Init STANDARD (B=0) : le modèle part EXACTEMENT du socle.
	NkQwen2SftModel m = BuildToyModel(tok.VocabSize(), 2, 8, 16.0f, 0.1f, /*zeroB=*/true, rng);

	NkVector<ToyPair> trainPairs, valPairs;
	BuildToyCorpus(trainPairs, valPairs);
	NkVector<NkQwen2SftExample> train, val;
	bool fmtOk = true;
	for (nk_size i = 0; i < trainPairs.Size(); ++i) {
		NkQwen2SftExample ex;
		fmtOk = fmtOk && NkQwen2SftFormatChatML(tok, trainPairs[i].q, trainPairs[i].r, ex, nullptr);
		train.PushBack(ex);
	}
	for (nk_size i = 0; i < valPairs.Size(); ++i) {
		NkQwen2SftExample ex;
		fmtOk = fmtOk && NkQwen2SftFormatChatML(tok, valPairs[i].q, valPairs[i].r, ex, nullptr);
		val.PushBack(ex);
	}
	Check(fmtOk && train.Size() == 30 && val.Size() == 5, "corpus jouet formaté : 30 paires train + 5 validation");

	// Empreintes du socle AVANT : le gel doit être prouvé bit-à-bit APRÈS.
	NkTensor wq0Before = m.layers[0].wq.Clone();
	NkTensor embBefore = m.embedding.Clone();

	NkQwen2SftTrainer trainer;
	// lr 0.02 : 0.05 (valeur de la descente MSE du jalon 2) DIVERGE ici après
	// ~50 pas (CE + softmax : les logits gagnants s'emballent, la perte remonte
	// de 2.7 à 4.2 et plafonne — observé) ; 0.02 + clip global 1.0 reste stable
	// sur les 200 pas.
	Check(trainer.Init(&m, 0.02f), "trainer initialisé (Adam sur les 28 paires A/B seules)");
	// Clipping global : filet de sécurité contre un pas précoce brutal — il
	// préserve la direction du gradient (même sémantique que NkOptim).
	trainer.SetGradClipGlobalNorm(1.0f);

	double valBefore = 0.0;
	for (nk_size i = 0; i < val.Size(); ++i)
		valBefore += trainer.EvaluateExample(val[i]);
	valBefore /= (double)val.Size();

	double firstLoss = -1.0, lastLoss = -1.0;
	const int32 nSteps = 200;
	for (int32 it = 0; it < nSteps; ++it) {
		double sum = 0.0;
		bool ok = true;
		for (nk_size i = 0; i < train.Size(); ++i) {
			const double L = trainer.AccumulateExample(train[i]);
			if (L < 0.0)
				ok = false;
			sum += L;
		}
		if (!ok || !trainer.Step()) {
			Check(false, "accumulation/pas Adam pendant le SFT jouet");
			return;
		}
		const double avg = sum / (double)train.Size();
		if (it == 0)
			firstLoss = avg;
		lastLoss = avg;
		if (it == 0 || ((it + 1) % 50) == 0)
			printf("    pas %3d : perte train (moyenne 30 paires) = %.6f\n", it + 1, avg);
	}

	double valAfter = 0.0;
	for (nk_size i = 0; i < val.Size(); ++i)
		valAfter += trainer.EvaluateExample(val[i]);
	valAfter /= (double)val.Size();

	printf("    train : %.6f -> %.6f (facteur %.1fx) ; validation (5 paires jamais vues) : %.6f -> %.6f\n",
		   firstLoss, lastLoss, lastLoss > 0.0 ? firstLoss / lastLoss : 1e30, valBefore, valAfter);
	Check(firstLoss > 0.0 && lastLoss > 0.0 && lastLoss * 5.0 <= firstLoss,
		  "perte d'entraînement divisée par au moins 5 en 200 pas");
	Check(valAfter < valBefore, "la perte de VALIDATION (paires jamais vues) baisse aussi");

	// Gel du socle : bit-identique après 200 pas (le gel est structurel — Adam
	// n'a jamais VU ces tenseurs — mais la preuve vaut mieux que l'intention).
	const bool wqSame = std::memcmp(wq0Before.DataAs<float>(), m.layers[0].wq.DataAs<float>(),
									(usize)wq0Before.Numel() * sizeof(float)) == 0;
	const bool embSame = std::memcmp(embBefore.DataAs<float>(), m.embedding.DataAs<float>(),
									 (usize)embBefore.Numel() * sizeof(float)) == 0;
	Check(wqSame && embSame, "socle BIT-IDENTIQUE après 200 pas (wq couche 0 + embedding)");
	printf("\n");
}

// =============================================================================
// PARTIE (d) : vrai tokenizer + vrai blob (optionnel).
// =============================================================================
static bool ReadFirstLines(const char *path, int32 maxLines, NkVector<NkString> &outLines) {
	NkFile f(path, NkFileMode::NK_READ_BINARY);
	if (!f.IsOpen())
		return false;
	const nk_int64 fileSize = f.Size();
	if (fileSize <= 0)
		return false;
	// Seules les premières lignes servent : lire 64 Ko suffit largement.
	const int64 want = fileSize < 65536 ? fileSize : 65536;
	NkVector<char> buf;
	buf.Resize((nk_size)want);
	const usize got = f.Read(buf.Data(), (usize)want);
	if (got == 0)
		return false;
	const int64 n = (int64)got;
	int64 lineStart = 0;
	for (int64 i = 0; i < n && (int32)outLines.Size() < maxLines; ++i) {
		if (buf[(nk_size)i] == '\n') {
			int64 end = i;
			if (end > lineStart && buf[(nk_size)(end - 1)] == '\r')
				--end; // le corpus est CRLF : le \r ne fait pas partie du texte
			outLines.PushBack(NkString(buf.Data() + lineStart, (NkString::SizeType)(end - lineStart)));
			lineStart = i + 1;
		}
	}
	return outLines.Size() > 0;
}

static void RunRealBlob(const char *ggufPath, const char *corpusPath) {
	printf("-- PARTIE (d) : vrai tokenizer + vrai blob Qwen2.5 (optionnel) --\n");
	if (!ggufPath) {
		printf("  SAUTÉ : aucun chemin GGUF fourni en argument 1 -- ce n'est pas un échec.\n\n");
		return;
	}
	printf("  GGUF : %s\n", ggufPath);
	NkQwen2Tokenizer tok;
	NkString err;
	if (!tok.LoadFromGGUF(ggufPath, &err)) {
		printf("  LoadFromGGUF a échoué (%s) -- partie (d) sautée, pas un échec.\n\n", err.CStr());
		return;
	}
	Check(tok.VocabSize() > 100000, "vocabulaire réel chargé (> 100 000 tokens)");
	printf("    <|endoftext|> -> id %d\n    <|im_start|>  -> id %d\n    <|im_end|>    -> id %d\n",
		   tok.EndOfTextId(), tok.ImStartId(), tok.ImEndId());
	Check(tok.EndOfTextId() >= 0 && tok.ImStartId() >= 0 && tok.ImEndId() >= 0,
		  "les TROIS spéciaux ChatML existent dans CE blob (contrairement au blob DeepSeek du jalon 2)");

	// Première paire exploitable du corpus réel : « Question: ... » suivie de
	// « Reponse: ... » (format vérifié du corpus BulkGen).
	NkVector<NkString> lines;
	if (!ReadFirstLines(corpusPath, 20, lines)) {
		Check(false, "corpus réel lisible");
		printf("\n");
		return;
	}
	NkString q, r;
	for (nk_size i = 0; i + 1 < lines.Size(); ++i) {
		const NkString &a = lines[i];
		const NkString &b = lines[i + 1];
		const char *qm = "Question: ";
		const char *rm = "Reponse: ";
		if (a.Size() > 10 && std::memcmp(a.Data(), qm, 10) == 0 && b.Size() > 9 &&
			std::memcmp(b.Data(), rm, 9) == 0) {
			q = NkString(a.Data() + 10, (NkString::SizeType)(a.Size() - 10));
			r = NkString(b.Data() + 9, (NkString::SizeType)(b.Size() - 9));
			break;
		}
	}
	Check(q.Size() > 0 && r.Size() > 0, "première paire Question:/Reponse: extraite du corpus réel");
	printf("    Q = \"%s\"\n    R = \"%s\"\n", q.CStr(), r.CStr());

	NkQwen2SftExample ex;
	Check(NkQwen2SftFormatChatML(tok, q, r, ex, &err), "formatage ChatML avec le VRAI vocabulaire");
	const nk_size nShow = ex.tokens.Size() < 40 ? ex.tokens.Size() : 40;
	printf("    %u tokens au total ; 40 premiers ids : [", (uint32)ex.tokens.Size());
	for (nk_size i = 0; i < nShow; ++i)
		printf("%s%d", i == 0 ? "" : ", ", ex.tokens[i]);
	printf("]\n    masque             : [");
	for (nk_size i = 0; i < nShow; ++i)
		printf("%s%d", i == 0 ? "" : ", ", ex.lossMask[i] != 0.0f ? 1 : 0);
	printf("]\n");
	// Cohérence structurelle du masque : commence par du 0 (gabarit+question),
	// finit par du 1 (réponse + <|im_end|>).
	Check(ex.lossMask.Size() == ex.tokens.Size() && ex.lossMask[0] == 0.0f &&
			  ex.lossMask[ex.lossMask.Size() - 1] != 0.0f && ex.tokens[0] == tok.ImStartId() &&
			  ex.tokens[ex.tokens.Size() - 1] == tok.ImEndId(),
		  "masque : 0 sur le prompt, 1 sur la réponse, <|im_start|> ouvre, <|im_end|> clôt");
	printf("\n");
}

int main(int argc, char **argv) {
	printf("=== NKQwen2SftTest : boucle SFT complète mini-modèle (QLoRA jalon 3, CPU pur) ===\n\n");

	// Mini-tokenizer synthétique du jalon 1 (267 tokens, 3 spéciaux ChatML).
	NkQwen2Tokenizer miniTok;
	{
		NkVector<NkString> tokens, merges;
		BuildMiniVocab(tokens, merges);
		NkString err;
		const bool loaded = miniTok.LoadFromLists(tokens, merges, &err);
		Check(loaded && miniTok.VocabSize() == 267 && miniTok.ImStartId() >= 0 && miniTok.ImEndId() >= 0,
			  "mini-tokenizer synthétique chargé (V=267, spéciaux ChatML présents)");
		if (!loaded) {
			printf("Erreur fatale tokenizer : %s\n", err.CStr());
			return 1;
		}
	}
	printf("\n");

	RunEndToEndGradient(miniTok);
	RunMaskingProof(miniTok);
	RunToySft(miniTok);
	RunRealBlob(argc > 1 ? argv[1] : nullptr,
				argc > 2 ? argv[2] : "D:\\Projets\\Camrail\\AI\\BulkGen\\dlg_ollama_fr.txt");

	printf("=== Résultat : %d/%d OK ===\n", g_pass, g_pass + g_fail);
	return g_fail == 0 ? 0 : 1;
}
