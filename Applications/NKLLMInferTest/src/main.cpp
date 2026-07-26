// =============================================================================
// NKLLMInferTest — exécution transformer réelle (NKInfer, Jalon 3 : « Exécuter
// une architecture transformer », « génération token par token + KV-cache »,
// « stratégies d'échantillonnage »).
//
// Deux parties, cf rapport dans NKInfer/ROADMAP.md et rapports_evolution/ :
//
//   PARTIE 1 (Option B — pipeline complet, mini-modèle JOUET) : construit un
//   petit modèle Qwen2 (même architecture EXACTE : RMSNorm, RoPE style NEOX,
//   attention GQA, MLP SwiGLU — cf NKInfer/NkQwen2Block.h) avec des poids
//   aléatoires déterministes (non entraînés — mission : prouver la CORRECTION
//   du pipeline, pas la qualité du texte). Preuve décisive : un forward COMPLET
//   sur toute la séquence doit être NUMÉRIQUEMENT IDENTIQUE à un forward
//   INCRÉMENTAL token-par-token via le KV-cache (les deux calculent la même
//   attention causale, juste dans un ordre différent) — si le KV-cache ou le
//   masque causal avait un bug, ce test le révélerait immédiatement. Puis
//   échantillonnage (greedy déterministe + top-k/température reproductible).
//
//   PARTIE 2 (Option A — poids RÉELS) : charge le VRAI blob Qwen2.5 7B Instruct
//   (le même que Jalon 3/loader/dequant), lit ses VRAIS hyperparamètres
//   (aucune valeur supposée), déquantifie ses VRAIS poids couche par couche
//   (streaming : une couche à la fois, libérée avant la suivante -- tenir les
//   28 couches déquantifiées simultanément en f32 dépasserait largement la RAM
//   disponible) et exécute un VRAI forward pass token par token à travers
//   TOUTES les couches réelles, avec RoPE/GQA/RMSNorm/SwiGLU réels sur des
//   VRAIS poids déquantifiés. Le token prédit est décodé via le VRAI
//   vocabulaire (tokenizer.ggml.tokens, lu en entier via
//   NkGGUFReadFullStringArray). AUCUN encodeur BPE n'est implémenté ici (hors
//   scope de ce jalon) : le prompt est réduit au token spécial BOS (réel, lu
//   dans les métadonnées) ; les tokens suivants sont de VRAIES prédictions du
//   modèle, ré-injectées en boucle (génération autorégressive réelle, mesurée).
//
// Usage : NKLLMInferTest.exe [chemin_gguf_ou_blob_ollama]
// Variables d'env (PARTIE 2, réglages sans recompiler) :
//   NK_GGUF_PATH       chemin du blob (si argv[1] absent)
//   NK_QWEN_NLAYERS    nombre de couches réelles à exécuter (défaut : toutes,
//                      qwen2.block_count)
//   NK_QWEN_NEWTOKENS  nombre de tokens à générer APRÈS le premier (défaut 2)
// =============================================================================
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKInfer/NkQwen2Block.h"
#include "NKInfer/NkKVCache.h"
#include "NKInfer/NkSampling.h"
#include "NKTensor/NkTensorOps.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;

static int g_pass = 0, g_fail = 0;

static void Check(bool ok, const char *name) {
	(ok ? g_pass : g_fail)++;
	printf("  [ %s ] %s\n", ok ? "OK" : "KO", name);
}

// =============================================================================
// Statistiques simples (mêmes que NKGGUFInspectTest) : détecte NaN/Inf et donne
// un ordre de grandeur plausible d'activations/poids.
// =============================================================================
struct TensorStats {
		double mean = 0.0;
		double stddev = 0.0;
		float minVal = 0.0f;
		float maxVal = 0.0f;
		uint64 nanCount = 0;
		uint64 infCount = 0;
		uint64 n = 0;
};

static TensorStats ComputeStats(const float *data, uint64 n) {
	TensorStats s;
	s.n = n;
	if (n == 0)
		return s;
	double sum = 0.0;
	float mn = 0.0f, mx = 0.0f;
	bool haveMinMax = false;
	for (uint64 i = 0; i < n; ++i) {
		float v = data[i];
		if (std::isnan(v)) {
			s.nanCount++;
			continue;
		}
		if (std::isinf(v)) {
			s.infCount++;
			continue;
		}
		sum += (double)v;
		if (!haveMinMax) {
			mn = mx = v;
			haveMinMax = true;
		} else {
			if (v < mn)
				mn = v;
			if (v > mx)
				mx = v;
		}
	}
	uint64 valid = n - s.nanCount - s.infCount;
	s.mean = valid > 0 ? sum / (double)valid : 0.0;
	double var = 0.0;
	for (uint64 i = 0; i < n; ++i) {
		float v = data[i];
		if (std::isnan(v) || std::isinf(v))
			continue;
		double d = (double)v - s.mean;
		var += d * d;
	}
	s.stddev = valid > 0 ? std::sqrt(var / (double)valid) : 0.0;
	s.minVal = mn;
	s.maxVal = mx;
	return s;
}

// Le calcul des logits (lm_head) réutilise infer::NkLinearNoBias (NKInfer/
// NkQwen2Block.h) plutôt qu'un matmul + transposée locale : cette dernière
// forcerait une copie élément-par-élément de la matrice de sortie ENTIÈRE
// (jusqu'à 152064 lignes pour Qwen2.5) via NkTensor::Contiguous() générique --
// prohibitif. NkLinearNoBias calcule x·W^T ligne par ligne SANS matérialiser
// W^T (voir sa justification détaillée dans NkQwen2Block.cpp).

// =============================================================================
// PARTIE 1 — mini-modèle jouet : forward complet == forward incrémental (KV-cache).
// =============================================================================
namespace {

	// LCG déterministe — même schéma que NKNN/NkTransformer.h::RandnTensor (déjà
	// utilisé dans ce dépôt pour initialiser des poids de façon reproductible).
	NkTensor MakeRandn(const NkShape &shape, double scale, uint32 seed) {
		NkTensor t = NkTensor::Zeros(shape);
		float *d = t.DataAs<float>();
		const int64 n = t.Numel();
		uint32 st = seed * 2654435761u + 1u;
		for (int64 i = 0; i < n; ++i) {
			st = st * 1664525u + 1013904223u;
			double u = (double)((st >> 8) & 0xFFFFFFu) / 16777216.0; // [0,1)
			d[i] = (float)((u - 0.5) * 2.0 * scale);
		}
		return t;
	}

} // namespace

static void RunToyPipelineTest() {
	printf("=== PARTIE 1 : mini-modèle jouet (RMSNorm/RoPE/GQA/SwiGLU/KV-cache/échantillonnage) ===\n\n");

	infer::NkQwen2Config cfg;
	cfg.dModel = 32;
	cfg.nHeads = 4;
	cfg.nKVHeads = 2; // GQA : 2 têtes Q partagent chaque tête KV
	cfg.headDim = cfg.dModel / cfg.nHeads; // 8
	cfg.ffnDim = 64;
	cfg.ropeFreqBase = 10000.0f;
	cfg.rmsEps = 1e-6f;
	Check(cfg.IsValid(), "NkQwen2Config jouet valide (dModel=32, nHeads=4, nKVHeads=2, headDim=8)");

	const uint32 kLayers = 2;
	const int64 kVocab = 50;
	const int64 T = 6;

	NkTensor embed = MakeRandn(NkShape{kVocab, (int64)cfg.dModel}, 0.5, 1);

	NkVector<infer::NkQwen2LayerWeights> weights;
	for (uint32 l = 0; l < kLayers; ++l) {
		infer::NkQwen2LayerWeights w;
		const uint32 s = 100u + l * 37u;
		const int64 d = cfg.dModel, qd = (int64)cfg.nHeads * cfg.headDim, kvd = (int64)cfg.nKVHeads * cfg.headDim;
		w.attnNorm = NkTensor::Ones(NkShape{d});
		w.wq = MakeRandn(NkShape{qd, d}, 0.2, s + 1);
		w.bq = MakeRandn(NkShape{qd}, 0.05, s + 2);
		w.wk = MakeRandn(NkShape{kvd, d}, 0.2, s + 3);
		w.bk = MakeRandn(NkShape{kvd}, 0.05, s + 4);
		w.wv = MakeRandn(NkShape{kvd, d}, 0.2, s + 5);
		w.bv = MakeRandn(NkShape{kvd}, 0.05, s + 6);
		w.wo = MakeRandn(NkShape{d, qd}, 0.2, s + 7);
		w.ffnNorm = NkTensor::Ones(NkShape{d});
		w.wGate = MakeRandn(NkShape{(int64)cfg.ffnDim, d}, 0.2, s + 8);
		w.wUp = MakeRandn(NkShape{(int64)cfg.ffnDim, d}, 0.2, s + 9);
		w.wDown = MakeRandn(NkShape{d, (int64)cfg.ffnDim}, 0.2, s + 10);
		Check(w.IsValid(cfg), "poids jouet couche valides (formes cohérentes avec cfg)");
		weights.PushBack(w);
	}
	NkTensor finalNorm = NkTensor::Ones(NkShape{(int64)cfg.dModel});
	NkTensor lmHead = MakeRandn(NkShape{kVocab, (int64)cfg.dModel}, 0.3, 999);

	int64 tokenIds[6] = {3, 17, 8, 44, 1, 9};

	// ---- Forward COMPLET (préfill T=6 d'un coup, cache vide au départ) ------
	NkTensor xFull = NkTensor::Zeros(NkShape{T, (int64)cfg.dModel});
	for (int64 t = 0; t < T; ++t)
		std::memcpy(xFull.DataAs<float>() + t * cfg.dModel, embed.DataAs<float>() + tokenIds[t] * cfg.dModel,
					(usize)cfg.dModel * sizeof(float));

	NkVector<infer::NkKVCacheLayer> cacheFull;
	cacheFull.Resize(kLayers);
	NkTensor hFull = xFull;
	for (uint32 l = 0; l < kLayers; ++l)
		hFull = infer::NkQwen2LayerForward(cfg, weights[l], hFull, cacheFull[l]);
	Check(hFull.IsValid(), "forward complet (T=6, préfill) réussi sur toutes les couches");
	NkTensor hFullNorm = infer::NkRMSNorm(hFull, finalNorm, cfg.rmsEps);
	NkTensor hFullLast = hFullNorm.Slice(0, T - 1, T, 1).Reshape(NkShape{1, (int64)cfg.dModel});

	// ---- Forward INCRÉMENTAL (T=1 à chaque pas, KV-cache, positions 0..5) ----
	NkVector<infer::NkKVCacheLayer> cacheInc;
	cacheInc.Resize(kLayers);
	NkTensor hInc;
	for (int64 t = 0; t < T; ++t) {
		NkTensor xt = NkTensor::Zeros(NkShape{1, (int64)cfg.dModel});
		std::memcpy(xt.DataAs<float>(), embed.DataAs<float>() + tokenIds[t] * cfg.dModel,
					(usize)cfg.dModel * sizeof(float));
		NkTensor h = xt;
		for (uint32 l = 0; l < kLayers; ++l)
			h = infer::NkQwen2LayerForward(cfg, weights[l], h, cacheInc[l]);
		hInc = h;
	}
	Check(hInc.IsValid(), "forward incrémental (6x T=1, KV-cache) réussi sur toutes les couches");
	NkTensor hIncNorm = infer::NkRMSNorm(hInc, finalNorm, cfg.rmsEps);

	double maxDiff = 0.0;
	if (hFullLast.IsValid() && hIncNorm.IsValid()) {
		const float *a = hFullLast.DataAs<float>();
		const float *b = hIncNorm.DataAs<float>();
		for (int64 i = 0; i < cfg.dModel; ++i)
			maxDiff = std::fmax(maxDiff, std::fabs((double)a[i] - (double)b[i]));
	}
	printf("  écart max (forward complet vs incrémental, dernière position) = %.3g\n", maxDiff);
	Check(hFullLast.IsValid() && hIncNorm.IsValid() && ops::AllClose(hFullLast, hIncNorm, 1e-3),
		  "COHÉRENCE : forward complet == forward incrémental (KV-cache/GQA/RoPE/masque causal corrects)");
	Check(maxDiff < 1e-3, "écart numérique < 1e-3 (accumulation flottante attendue, pas un bug)");

	// ---- Échantillonnage sur les VRAIS logits jouets -------------------------
	NkTensor logitsFull = infer::NkLinearNoBias(hFullLast, lmHead); // [1,vocab]
	Check(logitsFull.IsValid(), "calcul des logits jouets (lm_head) réussi");

	int32 g1 = infer::NkSampleGreedy(logitsFull);
	int32 g2 = infer::NkSampleGreedy(logitsFull);
	printf("  greedy(logits) = %d (déterministe : rappelé deux fois -> %d)\n", g1, g2);
	Check(g1 >= 0 && g1 == g2, "NkSampleGreedy déterministe (même résultat à chaque appel)");

	uint32 seedA1 = 12345, seedA2 = 12345, seedB = 999;
	int32 s1 = infer::NkSampleTopK(logitsFull, 0.8f, 8, seedA1);
	int32 s2 = infer::NkSampleTopK(logitsFull, 0.8f, 8, seedA2);
	int32 s3 = infer::NkSampleTopK(logitsFull, 0.8f, 8, seedB);
	printf("  topK(graine=12345) = %d ; rejoué (même graine) = %d ; autre graine(999) = %d\n", s1, s2, s3);
	Check(s1 >= 0 && s1 == s2, "NkSampleTopK reproductible (même graine initiale -> même tirage)");

	printf("\n");
}

// =============================================================================
// PARTIE 2 — poids RÉELS (Qwen2.5 7B Instruct, GGUF).
// =============================================================================
namespace {

	const infer::NkGGUFTensorInfo *FindTensor(const infer::NkGGUFFile &gguf, const char *name) {
		for (uint32 i = 0; i < gguf.tensors.Size(); ++i)
			if (gguf.tensors[i].name.Compare(name) == 0)
				return &gguf.tensors[i];
		return nullptr;
	}

	bool DequantNamed(const char *path, const infer::NkGGUFFile &gguf, const char *name, NkTensor &out) {
		const infer::NkGGUFTensorInfo *t = FindTensor(gguf, name);
		if (!t) {
			fprintf(stderr, "  [erreur] tenseur '%s' introuvable dans ce blob\n", name);
			return false;
		}
		NkString err;
		if (!infer::NkGGUFDequantizeTensor(path, gguf, *t, out, &err)) {
			fprintf(stderr, "  [erreur] déquantification de '%s' échouée : %s\n", name, err.CStr());
			return false;
		}
		return true;
	}

	bool LoadLayerWeights(const char *path, const infer::NkGGUFFile &gguf, uint32 layer, infer::NkQwen2LayerWeights &w) {
		char buf[160];
		bool ok = true;
#define NK_LOAD(field, suffix)                                                                                       \
	std::snprintf(buf, sizeof(buf), "blk.%u." suffix, layer);                                                         \
	ok = ok && DequantNamed(path, gguf, buf, w.field)
		NK_LOAD(attnNorm, "attn_norm.weight");
		NK_LOAD(wq, "attn_q.weight");
		NK_LOAD(bq, "attn_q.bias");
		NK_LOAD(wk, "attn_k.weight");
		NK_LOAD(bk, "attn_k.bias");
		NK_LOAD(wv, "attn_v.weight");
		NK_LOAD(bv, "attn_v.bias");
		NK_LOAD(wo, "attn_output.weight");
		NK_LOAD(ffnNorm, "ffn_norm.weight");
		NK_LOAD(wGate, "ffn_gate.weight");
		NK_LOAD(wUp, "ffn_up.weight");
		NK_LOAD(wDown, "ffn_down.weight");
#undef NK_LOAD
		return ok;
	}

	// Déquantifie SEULEMENT les lignes `tokenIds` d'un tenseur d'embedding 2D
	// [vocab,d] (Q4_K/Q6_K/Q8_0/F16/F32) SANS matérialiser les 545M éléments du
	// tenseur entier : chaque ligne (un token) est un bloc CONTIGU d'octets dans
	// le fichier (ne0=d = dimension la plus rapide côté ggml, donc une ligne =
	// exactement `sizeBytes / vocab` octets consécutifs, exact tant que d est un
	// multiple de la taille de bloc — vrai ici : 3584 est multiple de 256/32/1).
	bool DequantEmbeddingRows(const char *path, const infer::NkGGUFFile &gguf, const infer::NkGGUFTensorInfo &t,
							  const NkVector<int64> &tokenIds, NkTensor &outEmbeds) {
		if (!t.sizeKnown || t.dims.Size() != 2)
			return false;
		const int64 rowLen = (int64)t.dims[0];
		const int64 vocab = (int64)t.dims[1];
		if (vocab <= 0 || t.numElements != (uint64)(rowLen * vocab))
			return false;
		const uint64 bytesPerRow = t.sizeBytes / (uint64)vocab;

		NkFile f(path, NkFileMode::NK_READ_BINARY);
		if (!f.IsOpen())
			return false;
		outEmbeds = NkTensor::Zeros(NkShape{(int64)tokenIds.Size(), rowLen});
		NkVector<uint8> raw;
		raw.Resize((NkVector<uint8>::SizeType)bytesPerRow);
		for (uint32 ti = 0; ti < tokenIds.Size(); ++ti) {
			const int64 tok = tokenIds[ti];
			if (tok < 0 || tok >= vocab)
				return false;
			const uint64 absOffset = gguf.tensorDataOffset + t.offset + (uint64)tok * bytesPerRow;
			if (!f.Seek((nk_int64)absOffset, NkSeekOrigin::NK_BEGIN))
				return false;
			const usize got = f.Read(raw.Data(), (usize)bytesPerRow);
			if (got != (usize)bytesPerRow)
				return false;
			NkVector<float32> rowData;
			if (!infer::NkGGUFDequantizeRaw(t.rawType, raw.Data(), raw.Size(), (uint64)rowLen, rowData, nullptr))
				return false;
			std::memcpy(outEmbeds.DataAs<float>() + (int64)ti * rowLen, rowData.Data(), (usize)rowLen * sizeof(float));
		}
		return true;
	}

	void PrintMetaUInt(const infer::NkGGUFFile &gguf, const char *key) {
		uint64 v = 0;
		if (infer::NkGGUFGetUInt(gguf, key, v))
			printf("  %-40s = %llu\n", key, (unsigned long long)v);
	}
	void PrintMetaFloat(const infer::NkGGUFFile &gguf, const char *key) {
		float64 v = 0;
		if (infer::NkGGUFGetFloat(gguf, key, v))
			printf("  %-40s = %g\n", key, v);
	}

} // namespace

static void RunRealQwen2Test(const char *path) {
	printf("=== PARTIE 2 : poids RÉELS (Qwen2.5 7B Instruct, %s) ===\n\n", path);

	infer::NkGGUFFile gguf;
	bool loaded = infer::NkGGUFLoader::Load(path, gguf);
	Check(loaded && gguf.valid, "NkGGUFLoader::Load réussi sur le blob réel");
	if (!loaded || !gguf.valid) {
		printf("  Erreur : %s\n", gguf.error.CStr());
		return;
	}

	NkString arch;
	Check(infer::NkGGUFGetString(gguf, "general.architecture", arch) && arch.Compare("qwen2") == 0,
		  "general.architecture == \"qwen2\"");

	uint64 blockCount = 0, dModel = 0, ffnDim = 0, headCount = 0, headCountKv = 0;
	float64 ropeFreqBase = 0.0, rmsEps = 0.0;
	Check(infer::NkGGUFGetUInt(gguf, "qwen2.block_count", blockCount), "qwen2.block_count lu");
	Check(infer::NkGGUFGetUInt(gguf, "qwen2.embedding_length", dModel), "qwen2.embedding_length lu");
	Check(infer::NkGGUFGetUInt(gguf, "qwen2.feed_forward_length", ffnDim), "qwen2.feed_forward_length lu");
	Check(infer::NkGGUFGetUInt(gguf, "qwen2.attention.head_count", headCount), "qwen2.attention.head_count lu");
	Check(infer::NkGGUFGetUInt(gguf, "qwen2.attention.head_count_kv", headCountKv),
		  "qwen2.attention.head_count_kv lu");
	Check(infer::NkGGUFGetFloat(gguf, "qwen2.rope.freq_base", ropeFreqBase), "qwen2.rope.freq_base lu");
	Check(infer::NkGGUFGetFloat(gguf, "qwen2.attention.layer_norm_rms_epsilon", rmsEps),
		  "qwen2.attention.layer_norm_rms_epsilon lu");

	printf("\n-- Hyperparamètres réels (lus du GGUF, aucune valeur supposée) --\n");
	PrintMetaUInt(gguf, "qwen2.block_count");
	PrintMetaUInt(gguf, "qwen2.embedding_length");
	PrintMetaUInt(gguf, "qwen2.feed_forward_length");
	PrintMetaUInt(gguf, "qwen2.attention.head_count");
	PrintMetaUInt(gguf, "qwen2.attention.head_count_kv");
	PrintMetaFloat(gguf, "qwen2.rope.freq_base");
	PrintMetaFloat(gguf, "qwen2.attention.layer_norm_rms_epsilon");
	printf("\n");

	infer::NkQwen2Config cfg;
	cfg.dModel = (int32)dModel;
	cfg.nHeads = (int32)headCount;
	cfg.nKVHeads = (int32)headCountKv;
	cfg.headDim = (headCount > 0) ? (int32)(dModel / headCount) : 0;
	cfg.ffnDim = (int32)ffnDim;
	cfg.ropeFreqBase = (float32)ropeFreqBase;
	cfg.rmsEps = (float32)rmsEps;
	Check(cfg.IsValid(), "NkQwen2Config réel valide (formes cohérentes, GQA détecté si nKVHeads < nHeads)");
	printf("  headDim = %d, ratio GQA (nHeads/nKVHeads) = %d\n\n", cfg.headDim,
		   cfg.nKVHeads > 0 ? cfg.nHeads / cfg.nKVHeads : 0);
	if (!cfg.IsValid())
		return;

	// ---- token spécial BOS (réel) comme unique prompt : aucun encodeur BPE
	// n'est implémenté ici (hors scope de ce jalon) — cf en-tête de fichier. ----
	uint64 bosId = 0;
	bool hasBos = infer::NkGGUFGetUInt(gguf, "tokenizer.ggml.bos_token_id", bosId);
	Check(hasBos, "tokenizer.ggml.bos_token_id lu (utilisé comme prompt réel à 1 token)");
	printf("  Prompt = [token %llu] (BOS réel, PAS de texte encodé -- aucun BPE implémenté)\n\n",
		   (unsigned long long)bosId);

	const infer::NkGGUFTensorInfo *embTensor = FindTensor(gguf, "token_embd.weight");
	Check(embTensor != nullptr, "tenseur 'token_embd.weight' trouvé");
	if (!embTensor)
		return;

	NkChrono totalClock;

	// ---- Sanity check rapide sur 2 vraies couches (borne le risque avant le
	// run complet potentiellement long sur les 28 couches) --------------------
	const uint32 nSanity = blockCount >= 2 ? 2u : (uint32)blockCount;
	{
		NkVector<int64> ids;
		ids.PushBack((int64)bosId);
		NkTensor x;
		Check(DequantEmbeddingRows(path, gguf, *embTensor, ids, x), "embedding (lignes sélectives) du token BOS réel");
		printf("-- Sanity check (%u vraie(s) couche(s), poids réels déquantifiés) --\n", nSanity);
		for (uint32 l = 0; l < nSanity && x.IsValid(); ++l) {
			NkChrono layerClock;
			infer::NkQwen2LayerWeights w;
			bool ok = LoadLayerWeights(path, gguf, l, w);
			char chkName[64];
			std::snprintf(chkName, sizeof(chkName), "couche %u : poids réels déquantifiés", l);
			Check(ok, chkName);
			if (!ok)
				break;
			infer::NkKVCacheLayer cache;
			x = infer::NkQwen2LayerForward(cfg, w, x, cache);
			std::snprintf(chkName, sizeof(chkName), "couche %u : forward réel réussi (%.3f s)", l,
						  layerClock.Elapsed().ToSeconds());
			Check(x.IsValid(), chkName);
			if (x.IsValid()) {
				TensorStats st = ComputeStats(x.DataAs<float>(), (uint64)x.Numel());
				printf("      stats activation : mean=%.6g stddev=%.6g min=%.6g max=%.6g NaN=%llu Inf=%llu\n", st.mean,
					   st.stddev, st.minVal, st.maxVal, (unsigned long long)st.nanCount, (unsigned long long)st.infCount);
				std::snprintf(chkName, sizeof(chkName), "couche %u : activations saines (0 NaN/Inf)", l);
				Check(st.nanCount == 0 && st.infCount == 0, chkName);
			}
		}
		printf("\n");
	}

	// ---- Run complet : TOUTES les couches réelles (ou NK_QWEN_NLAYERS) sur un
	// vrai forward + génération autorégressive réelle via KV-cache -----------
	uint32 nLayers = (uint32)blockCount;
	if (const char *envN = getenv("NK_QWEN_NLAYERS"))
		nLayers = (uint32)atoi(envN);
	int32 nNewTokens = 2;
	if (const char *envT = getenv("NK_QWEN_NEWTOKENS"))
		nNewTokens = atoi(envT);
	if (nNewTokens < 0)
		nNewTokens = 0;

	printf("-- Run complet réel (%u couche(s) sur %llu réelles, %d token(s) additionnel(s)) --\n", nLayers,
		   (unsigned long long)blockCount, nNewTokens);

	NkVector<int64> ids0;
	ids0.PushBack((int64)bosId);
	NkTensor xGen;
	if (!DequantEmbeddingRows(path, gguf, *embTensor, ids0, xGen)) {
		Check(false, "embedding du prompt réel (run complet)");
		return;
	}

	infer::NkKVCache cache;
	cache.Reset(nLayers);

	NkTensor outputNorm;
	bool haveOutputNorm = DequantNamed(path, gguf, "output_norm.weight", outputNorm);
	Check(haveOutputNorm, "output_norm.weight (norme finale) déquantifié");

	const bool tiedEmbeddings = (FindTensor(gguf, "output.weight") == nullptr);
	NkTensor lmHeadW;
	{
		NkChrono lmClock;
		bool ok = tiedEmbeddings ? infer::NkGGUFDequantizeTensor(path, gguf, *embTensor, lmHeadW, nullptr)
								  : DequantNamed(path, gguf, "output.weight", lmHeadW);
		char msg[128];
		std::snprintf(msg, sizeof(msg), "lm_head (%s) déquantifié en entier (%.3f s)",
					  tiedEmbeddings ? "tied: token_embd.weight" : "output.weight", lmClock.Elapsed().ToSeconds());
		Check(ok, msg);
		if (!ok)
			return;
	}

	NkVector<NkString> vocab;
	bool haveVocab = infer::NkGGUFReadFullStringArray(path, "tokenizer.ggml.tokens", vocab);
	Check(haveVocab, "vocabulaire complet (tokenizer.ggml.tokens) relu en entier pour décoder les tokens générés");

	auto DecodeToken = [&](int32 id) -> const char * {
		if (haveVocab && id >= 0 && (uint32)id < vocab.Size())
			return vocab[(uint32)id].CStr();
		return "<?>";
	};

	uint32 rngState = 42u;
	NkVector<int32> generated;
	generated.PushBack((int32)bosId);

	for (int32 step = 0; step <= nNewTokens; ++step) {
		NkChrono stepClock;
		NkTensor h = xGen;
		bool ok = true;
		for (uint32 l = 0; l < nLayers && ok; ++l) {
			NkChrono layerClock;
			infer::NkQwen2LayerWeights w;
			ok = LoadLayerWeights(path, gguf, l, w);
			if (!ok) {
				fprintf(stderr, "  [erreur] échec chargement couche %u au pas %d\n", l, step);
				break;
			}
			h = infer::NkQwen2LayerForward(cfg, w, h, cache.layers[l]);
			ok = h.IsValid();
			if (step == 0 && (l % 7 == 0 || l + 1 == nLayers))
				printf("    couche %u/%u traitée (%.3f s)\n", l + 1, nLayers, layerClock.Elapsed().ToSeconds());
		}
		if (!ok) {
			Check(false, "forward réel (toutes couches) — échec, voir stderr");
			break;
		}
		NkTensor hn = infer::NkRMSNorm(h, outputNorm, cfg.rmsEps);
		NkTensor logits = infer::NkLinearNoBias(hn, lmHeadW); // [1,vocab]
		if (!logits.IsValid()) {
			Check(false, "calcul des logits réels (lm_head) — échec");
			break;
		}
		TensorStats lst = ComputeStats(logits.DataAs<float>(), (uint64)logits.Numel());
		char lstMsg[96];
		std::snprintf(lstMsg, sizeof(lstMsg), "pas %d : logits réels sains (0 NaN/Inf)", step);
		Check(lst.nanCount == 0 && lst.infCount == 0, lstMsg);

		int32 nextId = (step == 0) ? infer::NkSampleGreedy(logits) // 1er token : glouton (déterministe)
									: infer::NkSampleTopK(logits, 0.8f, 40, rngState); // suivants : top-k stochastique
		printf("  [pas %d, %.3f s] token prédit = %d -> \"%s\"  (mean=%.4g std=%.4g min=%.4g max=%.4g)\n", step,
			   stepClock.Elapsed().ToSeconds(), nextId, DecodeToken(nextId), lst.mean, lst.stddev, lst.minVal,
			   lst.maxVal);
		Check(nextId >= 0, "un token a été échantillonné (glouton ou top-k) à partir des logits réels");
		generated.PushBack(nextId);

		if (step < nNewTokens) {
			NkVector<int64> nid;
			nid.PushBack((int64)nextId);
			if (!DequantEmbeddingRows(path, gguf, *embTensor, nid, xGen)) {
				Check(false, "embedding du token généré (pas suivant)");
				break;
			}
		}
	}

	printf("\n  Séquence générée (ids) : ");
	for (uint32 i = 0; i < generated.Size(); ++i)
		printf("%d ", generated[i]);
	printf("\n  Séquence générée (texte, décodage vocabulaire réel) : ");
	for (uint32 i = 0; i < generated.Size(); ++i)
		printf("%s", DecodeToken(generated[i]));
	printf("\n");

	printf("\n  Temps total PARTIE 2 (chargement + %u couche(s) réelles x %d pas + lm_head) : %.3f s\n", nLayers,
		   nNewTokens + 1, totalClock.Elapsed().ToSeconds());
	printf("\n");
}

int main(int argc, char **argv) {
	printf("=== NKLLMInferTest : exécution transformer Qwen2 réelle (NKInfer, Jalon 3) ===\n\n");

	RunToyPipelineTest();

	const char *path = nullptr;
	if (argc > 1) {
		path = argv[1];
	} else if (const char *envPath = getenv("NK_GGUF_PATH")) {
		path = envPath;
	} else {
		path = "C:/Users/Rihen/.ollama/models/blobs/"
			   "sha256-2bada8a7450677000f678be90653b85d364de7db25eb5ea54136ada5f3933730";
	}
	RunRealQwen2Test(path);

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
