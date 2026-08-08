// =============================================================================
// NKQwen2GpuTest — jalon 5 du chantier QLoRA : le forward COMPLET du Qwen2.5 7B
// sur GPU (NKInfer/NkQwen2Gpu), prouvé identique à la référence CPU.
//
// CE QUE CE TEST PROUVE, DANS L'ORDRE
// -----------------------------------
//   (a) MÊME CALCUL QUE LE CPU. Sur une MÊME séquence courte de vrais tokens :
//       l'état caché après la couche 0, après la DERNIÈRE couche, après la
//       RMSNorm finale, et les 152 064 logits, sont comparés à la référence CPU
//       f32 (NkQwen2Block.cpp, poids déquantifiés en entier couche par couche —
//       le chemin déjà validé au jalon 3). Critère : erreur relative <= 1e-3.
//       Pourquoi PAS bit-à-bit ici, alors que la déquantification du jalon 4
//       l'était : la déquantification est une fonction d'entiers, elle DOIT
//       tomber juste ; le forward est une somme de millions de flottants, et
//       l'ordre des sommes diffère nécessairement entre 28 threads GPU et une
//       boucle CPU. Exiger l'égalité exacte serait exiger un accident.
//       La comparaison est faite DEUX fois : sur un préfill (T=4, cache vide)
//       ET sur un pas incrémental (T=1, cache rempli) — c'est ce second cas qui
//       exercerait un décalage d'indexation du KV-cache résident GPU.
//   (b) GÉNÉRATION RÉELLE. Un prompt EN FRANÇAIS, encodé par le tokenizer BPE
//       byte-level du jalon 1, gabarit ChatML de Qwen ; ~20 tokens générés et
//       décodés. Le texte doit être cohérent — pas du charabia.
//   (c) MESURES. VRAM comptabilisée, temps de chargement, temps par token, et
//       le rapport avec la référence CPU sur EXACTEMENT le même travail.
//
// BACKEND — POURQUOI VULKAN EST VERROUILLÉ ICI
// --------------------------------------------
// `output.weight` de ce blob est un tenseur Q6_K de 447 Mo et `token_embd` en
// pèse 306 : au-dessus des 128 Mo que D3D11 GARANTIT par ressource (la limite
// réelle est souvent plus haute, mais on ne s'appuie pas sur ce qui n'est pas
// garanti). Ce test force donc NK_TENSOR_API=vulkan s'il n'est pas déjà défini,
// et AFFICHE le backend réellement obtenu. Il ne découpe PAS les gros tenseurs
// en tranches de lignes : c'est l'autre parade possible, elle n'a pas été
// nécessaire ici et prétendre le contraire serait faux.
// Rappel du jalon précédent : NK_TENSOR_API=software est SILENCIEUSEMENT
// ignoré (on retombe sur l'ordre par défaut). Le backend affiché ci-dessous est
// donc le seul auquel se fier.
//
// Usage : NKQwen2GpuTest.exe [chemin_gguf_ou_blob_ollama]
// Variables d'env : NK_GGUF_PATH, NK_QWEN2GPU_NEWTOKENS (défaut 20),
//                   NK_QWEN2GPU_SKIPCPU=1 (saute la référence CPU, qui coûte
//                   plusieurs minutes — la génération et les mesures restent).
//
// Zéro STL.
// =============================================================================
#include "NKInfer/NkQwen2Gpu.h"
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKInfer/NkQwen2Block.h"
#include "NKInfer/NkKVCache.h"
#include "NKInfer/NkSampling.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKTensor/NkTensorGpu.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

using namespace nkentseu;
using namespace nkentseu::ai;
using namespace nkentseu::ai::infer;

static int g_ok = 0, g_fail = 0;

static void check(bool cond, const char *what) {
	printf("  [%s] %s\n", cond ? " OK " : "FAIL", what);
	if (cond)
		++g_ok;
	else
		++g_fail;
}

// Écart max absolu et max relatif normalisé par l'amplitude de la référence —
// MÊME convention qu'au jalon 4 (NKQ4MatmulTest) : une erreur relative par
// ÉLÉMENT exploserait sur les rares sorties proches de zéro et ne dirait rien.
static void Compare(const float32 *a, const float32 *b, int64 n, float64 &maxAbs, float64 &maxRel) {
	maxAbs = 0.0;
	float64 scale = 0.0;
	for (int64 i = 0; i < n; ++i) {
		const float64 dv = std::fabs((float64)a[i] - (float64)b[i]);
		if (dv > maxAbs)
			maxAbs = dv;
		const float64 m = std::fabs((float64)b[i]);
		if (m > scale)
			scale = m;
	}
	maxRel = (scale > 0.0) ? (maxAbs / scale) : maxAbs;
}

static int64 ArgMax(const float32 *v, int64 n) {
	int64 best = 0;
	for (int64 i = 1; i < n; ++i)
		if (v[i] > v[best])
			best = i;
	return best;
}

// =============================================================================
// Référence CPU — le chemin du jalon 3, repris tel quel : poids déquantifiés en
// f32 UNE COUCHE À LA FOIS (26 Go si on les gardait toutes : impossible), puis
// NkQwen2LayerForward. C'est LA vérité contre laquelle le GPU est jugé.
// =============================================================================
namespace {

	const NkGGUFTensorInfo *FindTensor(const NkGGUFFile &g, const char *name) {
		for (uint32 i = 0; i < g.tensors.Size(); ++i)
			if (g.tensors[i].name.Compare(name) == 0)
				return &g.tensors[i];
		return nullptr;
	}

	bool DequantNamed(const char *path, const NkGGUFFile &g, const char *name, NkTensor &out) {
		const NkGGUFTensorInfo *t = FindTensor(g, name);
		if (!t) {
			fprintf(stderr, "  [erreur] tenseur '%s' introuvable\n", name);
			return false;
		}
		NkString err;
		if (!NkGGUFDequantizeTensor(path, g, *t, out, &err)) {
			fprintf(stderr, "  [erreur] dequant '%s' : %s\n", name, err.CStr());
			return false;
		}
		return true;
	}

	bool LoadLayerWeights(const char *path, const NkGGUFFile &g, uint32 layer, NkQwen2LayerWeights &w) {
		char buf[160];
		bool ok = true;
#define NK_LOAD(field, suffix)                                                                                       \
	std::snprintf(buf, sizeof(buf), "blk.%u." suffix, layer);                                                         \
	ok = ok && DequantNamed(path, g, buf, w.field)
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

	// Lignes d'embedding déquantifiées sélectivement — même fonction qu'au
	// jalon 3 (et que celle du module GPU) : c'est délibéré, l'entrée des deux
	// chemins doit être IDENTIQUE, sinon on comparerait déjà deux choses.
	bool EmbedRowsCpu(const char *path, const NkGGUFFile &g, const NkGGUFTensorInfo &t, const NkVector<int32> &ids,
					  NkTensor &out) {
		if (!t.sizeKnown || t.dims.Size() != 2)
			return false;
		const int64 rowLen = (int64)t.dims[0];
		const int64 vocab = (int64)t.dims[1];
		const uint64 bytesPerRow = t.sizeBytes / (uint64)vocab;
		NkFile f(path, NkFileMode::NK_READ_BINARY);
		if (!f.IsOpen())
			return false;
		out = NkTensor::Zeros(NkShape{(int64)ids.Size(), rowLen});
		NkVector<uint8> raw;
		raw.Resize((NkVector<uint8>::SizeType)bytesPerRow);
		for (uint32 i = 0; i < ids.Size(); ++i) {
			const int64 tok = ids[i];
			if (tok < 0 || tok >= vocab)
				return false;
			const uint64 off = g.tensorDataOffset + t.offset + (uint64)tok * bytesPerRow;
			if (!f.Seek((nk_int64)off, NkSeekOrigin::NK_BEGIN))
				return false;
			if (f.Read(raw.Data(), (usize)bytesPerRow) != (usize)bytesPerRow)
				return false;
			NkVector<float32> row;
			if (!NkGGUFDequantizeRaw(t.rawType, raw.Data(), raw.Size(), (uint64)rowLen, row, nullptr))
				return false;
			std::memcpy(out.DataAs<float>() + (int64)i * rowLen, row.Data(), (usize)rowLen * sizeof(float));
		}
		return true;
	}

	// lm_head CPU PAR TRANCHES DE LIGNES.
	//
	// POURQUOI PAS « déquantifier output.weight en entier » comme au jalon 3 :
	// [152064, 3584] en f32 pèse 2,18 Go, et ce test tient déjà une couche
	// déquantifiée (932 Mo) plus le modèle GPU. Les lignes d'un tenseur GGUF
	// étant CONTIGUËS et la déquantification d'un super-bloc ne dépendant
	// d'aucun autre, une tranche de lignes est un tenseur valide à elle seule :
	// on en traite 8192 à la fois (117 Mo). Le calcul est celui de
	// NkLinearNoBias — même ordre de somme (k croissant), même type
	// d'accumulateur (float) : la tranche ne change AUCUN résultat.
	bool CpuLmHead(const char *path, const NkGGUFFile &g, const NkGGUFTensorInfo &t, const float32 *h, int64 d,
				   NkVector<float32> &outLogits) {
		const int64 cols = (int64)t.dims[0];
		const int64 rows = (int64)t.dims[1];
		if (cols != d || !t.sizeKnown)
			return false;
		const uint64 bytesPerRow = t.sizeBytes / (uint64)rows;
		NkFile f(path, NkFileMode::NK_READ_BINARY);
		if (!f.IsOpen())
			return false;
		outLogits.Resize((NkVector<float32>::SizeType)rows);
		const int64 chunk = 8192;
		NkVector<uint8> raw;
		NkVector<float32> w;
		for (int64 r0 = 0; r0 < rows; r0 += chunk) {
			const int64 nr = (r0 + chunk <= rows) ? chunk : (rows - r0);
			const uint64 need = (uint64)nr * bytesPerRow;
			raw.Resize((NkVector<uint8>::SizeType)need);
			const uint64 off = g.tensorDataOffset + t.offset + (uint64)r0 * bytesPerRow;
			if (!f.Seek((nk_int64)off, NkSeekOrigin::NK_BEGIN))
				return false;
			if (f.Read(raw.Data(), (usize)need) != (usize)need)
				return false;
			if (!NkGGUFDequantizeRaw(t.rawType, raw.Data(), need, (uint64)(nr * cols), w, nullptr))
				return false;
			const float32 *wp = w.Data();
			for (int64 n = 0; n < nr; ++n) {
				const float32 *wn = wp + n * cols;
				float acc = 0.0f;
				for (int64 k = 0; k < cols; ++k)
					acc += h[k] * wn[k];
				outLogits[(NkVector<float32>::SizeType)(r0 + n)] = acc;
			}
		}
		return true;
	}

	// État de la référence CPU entre deux appels (KV-cache + norme finale).
	struct CpuModel {
			const char *path = nullptr;
			const NkGGUFFile *gguf = nullptr;
			NkQwen2Config cfg;
			uint32 nLayers = 0;
			NkKVCache cache;
			NkTensor outputNorm;
			const NkGGUFTensorInfo *embed = nullptr;
			const NkGGUFTensorInfo *lmHead = nullptr;

			// Un appel = un forward de `ids` à partir de la position courante du
			// cache. Renvoie l'état caché après la couche 0, après la dernière,
			// après la RMSNorm finale, et les logits de la dernière position.
			bool Step(const NkVector<int32> &ids, NkTensor &h0, NkTensor &hLast, NkTensor &hNorm,
					  NkVector<float32> &logits, float64 &seconds) {
				NkChrono clk;
				NkTensor x;
				if (!EmbedRowsCpu(path, *gguf, *embed, ids, x))
					return false;
				for (uint32 l = 0; l < nLayers; ++l) {
					NkQwen2LayerWeights w;
					if (!LoadLayerWeights(path, *gguf, l, w))
						return false;
					x = NkQwen2LayerForward(cfg, w, x, cache.layers[l]);
					if (!x.IsValid())
						return false;
					if (l == 0)
						h0 = x.Clone();
					if (l + 1 == nLayers)
						hLast = x.Clone();
					if ((l % 7) == 0 || l + 1 == nLayers)
						printf("      [CPU] couche %u/%u (%.1f s cumulées)\n", l + 1, nLayers,
							   clk.Elapsed().ToSeconds());
				}
				hNorm = NkRMSNorm(x, outputNorm, cfg.rmsEps);
				if (!hNorm.IsValid())
					return false;
				const int64 T = hNorm.Shape()[0], d = hNorm.Shape()[1];
				if (!CpuLmHead(path, *gguf, *lmHead, hNorm.DataAs<float>() + (T - 1) * d, d, logits))
					return false;
				seconds = clk.Elapsed().ToSeconds();
				return true;
			}
	};

	void PrintBytes(const char *label, uint64 b) {
		printf("    %-34s %8.1f Mo (%.3f Go)\n", label, (double)b / 1048576.0, (double)b / 1073741824.0);
	}

} // namespace

// =============================================================================
int main(int argc, char **argv) {
	printf("=== NKQwen2GpuTest : forward 7B COMPLET sur GPU == référence CPU (QLoRA, jalon 5) ===\n\n");

	// ---- Verrou de backend, AVANT toute utilisation du GPU -------------------
	{
		const char *cur = getenv("NK_TENSOR_API");
		if (!cur || cur[0] == 0) {
#if defined(_WIN32)
			_putenv_s("NK_TENSOR_API", "vulkan");
#else
			setenv("NK_TENSOR_API", "vulkan", 1);
#endif
			printf("  NK_TENSOR_API non défini -> VERROUILLÉ sur \"vulkan\" (output.weight = 447 Mo > 128 Mo garantis par D3D11)\n");
		} else {
			printf("  NK_TENSOR_API = \"%s\" (fourni par l'environnement, respecté tel quel)\n", cur);
		}
	}

	const char *path = nullptr;
	if (argc > 1)
		path = argv[1];
	else if (const char *e = getenv("NK_GGUF_PATH"))
		path = e;
	else
		path = "C:/Users/Rihen/.ollama/models/blobs/"
			   "sha256-2bada8a7450677000f678be90653b85d364de7db25eb5ea54136ada5f3933730";
	printf("  blob : %s\n\n", path);

	NkTensorGpu &gpu = NkTensorGpu::Get();
	const bool gpuOk = gpu.IsAvailable();
	printf("  backend compute RÉELLEMENT obtenu : %s\n", gpu.BackendName());
	check(gpuOk, "device compute GPU disponible");
	if (!gpuOk) {
		printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
		return 1;
	}
	const bool isVulkan = std::strstr(gpu.BackendName(), "ulkan") != nullptr;
	if (!isVulkan)
		printf("  !! ATTENTION : backend != Vulkan — les tenseurs de 306/447 Mo peuvent être refusés.\n");

	// =========================================================================
	// Chargement du modèle GPU
	// =========================================================================
	printf("\n-- Chargement du modèle, poids BRUTS quantifiés uploadés tels quels --\n");
	NkQwen2GpuOptions opt;
	opt.maxSeqLen = 192;	  // prompt ChatML (~40) + 20 générés : large marge
	opt.maxBatchTokens = 64; // préfill d'un bloc
	opt.verbose = true;

	NkQwen2Gpu model;
	NkString err;
	NkChrono loadClk;
	const bool loaded = model.Load(path, opt, &err);
	const float64 loadSec = loadClk.Elapsed().ToSeconds();
	if (!loaded)
		printf("  erreur : %s\n", err.CStr());
	check(loaded, "modèle 7B chargé et RÉSIDENT GPU (28 couches, poids jamais déquantifiés côté CPU)");
	if (!loaded) {
		printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
		return 1;
	}

	const NkQwen2GpuStats &st = model.Stats();
	printf("\n  -- VRAM comptabilisée (octets réellement demandés à CreateBuffer) --\n");
	PrintBytes("poids quantifiés + normes", st.weightBytes);
	PrintBytes("KV-cache resident", st.kvBytes);
	PrintBytes("activations / scores / logits", st.scratchBytes);
	PrintBytes("TOTAL", st.TotalBytes());
	printf("    %u tampons GPU · %llu tenseurs Q4_K, %llu Q6_K, %llu F32\n", st.bufferCount,
		   (unsigned long long)st.q4Tensors, (unsigned long long)st.q6Tensors, (unsigned long long)st.f32Tensors);
	printf("    chargement : %.1f s (dont %.1f s de lecture disque)\n", loadSec, st.readSeconds);
	check(model.LayerCount() == 28, "28 couches résidentes");
	check(st.TotalBytes() < 7ull * 1073741824ull, "VRAM comptabilisée < 7 Go (budget de la carte 8 Go)");

	// =========================================================================
	// Tokenizer (jalon 1) — sert au (a) comme au (b)
	// =========================================================================
	NkQwen2Tokenizer tok;
	NkString terr;
	const bool tokOk = tok.LoadFromGGUF(path, &terr);
	check(tokOk, "tokenizer BPE byte-level Qwen2 chargé depuis le GGUF (jalon 1)");
	if (!tokOk)
		printf("  erreur : %s\n", terr.CStr());

	// =========================================================================
	// (a) GPU == CPU
	// =========================================================================
	const bool skipCpu = (getenv("NK_QWEN2GPU_SKIPCPU") != nullptr);
	NkGGUFFile gguf;
	NkGGUFLoader::Load(path, gguf);

	if (skipCpu) {
		printf("\n-- (a) référence CPU SAUTÉE (NK_QWEN2GPU_SKIPCPU) --\n");
	} else if (tokOk) {
		printf("\n-- (a) forward GPU == référence CPU (poids déquantifiés f32, NkQwen2Block) --\n");

		// Séquence de vrais tokens : préfill de 4, puis 1 pas incrémental.
		NkVector<int32> all;
		tok.Encode(NkString("Le Cameroun est un pays"), all);
		while (all.Size() < 5)
			all.PushBack(tok.EndOfTextId() >= 0 ? tok.EndOfTextId() : 0);
		NkVector<int32> prefill, step;
		for (uint32 i = 0; i < 4; ++i)
			prefill.PushBack(all[i]);
		step.PushBack(all[4]);
		printf("     séquence : ");
		for (uint32 i = 0; i < 5; ++i)
			printf("%d ", all[i]);
		printf("(\"%s\")\n", tok.Decode(all).CStr());

		CpuModel cpu;
		cpu.path = path;
		cpu.gguf = &gguf;
		cpu.cfg = model.Config();
		cpu.nLayers = model.LayerCount();
		cpu.cache.Reset(cpu.nLayers);
		cpu.embed = FindTensor(gguf, "token_embd.weight");
		cpu.lmHead = FindTensor(gguf, "output.weight");
		if (!cpu.lmHead)
			cpu.lmHead = cpu.embed; // embeddings liées
		const bool normOk = DequantNamed(path, gguf, "output_norm.weight", cpu.outputNorm);
		check(normOk && cpu.embed != nullptr && cpu.lmHead != nullptr, "référence CPU prête (norme finale + tenseurs)");

		for (int phase = 0; phase < 2 && normOk; ++phase) {
			const NkVector<int32> &ids = (phase == 0) ? prefill : step;
			const char *what = (phase == 0) ? "préfill T=4 (cache vide)" : "pas incrémental T=1 (cache rempli)";
			printf("\n     --- %s ---\n", what);

			NkTensor c0, cL, cN;
			NkVector<float32> cLogits;
			float64 cpuSec = 0.0;
			const bool cok = cpu.Step(ids, c0, cL, cN, cLogits, cpuSec);
			char msg[160];
			std::snprintf(msg, sizeof(msg), "%s : référence CPU calculée (%.1f s)", what, cpuSec);
			check(cok, msg);
			if (!cok)
				break;

			NkQwen2GpuTrace trace;
			trace.layers.PushBack(0);
			trace.layers.PushBack((int32)cpu.nLayers - 1);
			trace.hidden.Resize(2);
			trace.captureFinalNorm = true;
			NkTensor gLogits;
			NkChrono gclk;
			const bool gok = model.Forward(&ids[0], (int32)ids.Size(), gLogits, &trace, &err);
			const float64 gpuSec = gclk.Elapsed().ToSeconds();
			if (!gok)
				printf("     erreur GPU : %s\n", err.CStr());
			std::snprintf(msg, sizeof(msg), "%s : forward GPU réussi (%.3f s)", what, gpuSec);
			check(gok, msg);
			if (!gok)
				break;

			printf("     accélération GPU/CPU sur ce MÊME travail : %.0fx (%.1f s -> %.3f s)\n", cpuSec / gpuSec,
				   cpuSec, gpuSec);

			struct {
					const char *label;
					const NkTensor *cpuT;
					const NkTensor *gpuT;
			} cmp[] = {
				{"état caché après la couche 0", &c0, &trace.hidden[0]},
				{"état caché après la dernière couche", &cL, &trace.hidden[1]},
				{"état après la RMSNorm finale", &cN, &trace.finalNorm},
			};
			for (int i = 0; i < 3; ++i) {
				float64 mAbs = 0.0, mRel = 0.0;
				const bool shapeOk = cmp[i].gpuT->IsValid() && cmp[i].cpuT->IsValid() &&
									 cmp[i].gpuT->Numel() == cmp[i].cpuT->Numel();
				if (shapeOk)
					Compare(cmp[i].gpuT->DataAs<float>(), cmp[i].cpuT->DataAs<float>(), cmp[i].cpuT->Numel(), mAbs,
							mRel);
				printf("     %-38s |Δ|max = %.3e, rel = %.3e\n", cmp[i].label, mAbs, mRel);
				std::snprintf(msg, sizeof(msg), "%s : %s GPU == CPU (rel <= 1e-3)", what, cmp[i].label);
				check(shapeOk && mRel <= 1e-3, msg);
			}

			float64 lAbs = 0.0, lRel = 0.0;
			const bool sizeOk = gLogits.IsValid() && (int64)cLogits.Size() == gLogits.Numel();
			if (sizeOk)
				Compare(gLogits.DataAs<float>(), cLogits.Data(), gLogits.Numel(), lAbs, lRel);
			const int64 topGpu = sizeOk ? ArgMax(gLogits.DataAs<float>(), gLogits.Numel()) : -1;
			const int64 topCpu = sizeOk ? ArgMax(cLogits.Data(), (int64)cLogits.Size()) : -2;
			printf("     %-38s |Δ|max = %.3e, rel = %.3e\n", "logits (152064)", lAbs, lRel);
			printf("     top-1 GPU = %lld (\"%s\") · top-1 CPU = %lld (\"%s\")\n", (long long)topGpu,
				   tok.DecodeOne((int32)topGpu).CStr(), (long long)topCpu, tok.DecodeOne((int32)topCpu).CStr());
			std::snprintf(msg, sizeof(msg), "%s : logits GPU == CPU (rel <= 1e-3)", what);
			check(sizeOk && lRel <= 1e-3, msg);
			std::snprintf(msg, sizeof(msg), "%s : token top-1 IDENTIQUE entre GPU et CPU", what);
			check(topGpu == topCpu, msg);
		}
	}

	// =========================================================================
	// (b) génération réelle, prompt français, gabarit ChatML
	// =========================================================================
	if (tokOk) {
		printf("\n-- (b) génération réelle (prompt français, ChatML, tokenizer du jalon 1) --\n");
		model.ResetCache();

		NkString prompt("<|im_start|>system\nTu es un assistant francophone. Réponds brièvement.<|im_end|>\n"
						"<|im_start|>user\nQuelle est la capitale du Cameroun ?<|im_end|>\n"
						"<|im_start|>assistant\n");
		NkVector<int32> ids;
		const bool encOk = tok.EncodeWithSpecials(prompt, ids);
		printf("     prompt encodé : %llu tokens (dont <|im_start|>=%d, <|im_end|>=%d)\n", (unsigned long long)ids.Size(), tok.ImStartId(),
			   tok.ImEndId());
		check(encOk && ids.Size() > 10, "prompt français encodé par le BPE byte-level (ChatML)");

		int32 nNew = 20;
		if (const char *e = getenv("NK_QWEN2GPU_NEWTOKENS"))
			nNew = atoi(e);

		uint32 rng = 42u;
		NkVector<int32> out;
		float64 prefillSec = 0.0, msPerTok = 0.0;
		NkChrono genClk;
		const bool genOk = model.Generate(ids, nNew, /*temperature*/ 0.0f, /*topK*/ 0, rng, tok.ImEndId(), out,
										  &prefillSec, &msPerTok, &err);
		const float64 genSec = genClk.Elapsed().ToSeconds();
		if (!genOk)
			printf("     erreur : %s\n", err.CStr());
		check(genOk && out.Size() > 0, "génération autorégressive réelle sur GPU");

		if (genOk) {
			printf("     ids générés :");
			for (uint32 i = 0; i < out.Size(); ++i)
				printf(" %d", out[i]);
			printf("\n\n     ---------- TEXTE GÉNÉRÉ ----------\n     %s\n     ----------------------------------\n\n",
				   tok.Decode(out).CStr());
			printf("     préfill (%llu tokens) : %.2f s · génération : %.2f s pour %llu tokens · %.0f ms/token\n",
				   (unsigned long long)ids.Size(), prefillSec, genSec - prefillSec, (unsigned long long)out.Size(), msPerTok);
			// Un texte cohérent contient au moins une lettre latine et n'est pas
			// une répétition d'un seul token : un test de non-charabia MINIMAL et
			// automatique. Le jugement de qualité, lui, reste humain — l'afficher
			// est le seul moyen honnête de le soumettre.
			NkString txt = tok.Decode(out);
			bool hasLetter = false;
			for (NkString::SizeType i = 0; i < txt.Size(); ++i) {
				const char c = txt.CStr()[i];
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
					hasLetter = true;
			}
			bool allSame = out.Size() > 1;
			for (uint32 i = 1; i < out.Size(); ++i)
				if (out[i] != out[0])
					allSame = false;
			check(hasLetter && !allSame, "texte généré non dégénéré (lettres présentes, pas un token répété)");
		}
	}

	// =========================================================================
	// (c) mesures : temps par token en régime établi
	// =========================================================================
	{
		printf("\n-- (c) mesures en régime établi --\n");
		model.ResetCache();
		NkVector<int32> seed;
		seed.PushBack(tok.EndOfTextId() >= 0 ? tok.EndOfTextId() : 0);
		NkTensor logits;
		bool ok = model.Forward(&seed[0], 1, logits, nullptr, &err);
		// Une passe de chauffe : le PREMIER dispatch d'un noyau paye la chaîne
		// NkSL -> SPIR-V -> pipeline. Sans elle, on publierait un coût de
		// compilation déguisé en coût de calcul (leçon du jalon 4).
		const int32 kMeasured = 5;
		NkChrono clk;
		for (int32 i = 0; i < kMeasured && ok; ++i) {
			int32 next = NkSampleGreedy(logits);
			ok = model.Forward(&next, 1, logits, nullptr, &err);
		}
		const float64 sec = clk.Elapsed().ToSeconds();
		check(ok, "5 pas de génération supplémentaires (mesure)");
		if (ok) {
			printf("     %.0f ms/token (moyenne sur %d pas, cache déjà chaud)\n", sec * 1000.0 / kMeasured, kMeasured);
			printf("     VRAM totale comptabilisée : %.2f Go · chargement : %.1f s\n",
				   (double)st.TotalBytes() / 1073741824.0, loadSec);
		}
	}

	model.Release();
	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_ok, g_fail);
	printf("=== %d/%d OK ===\n", g_ok, g_ok + g_fail);
	return g_fail == 0 ? 0 : 1;
}
