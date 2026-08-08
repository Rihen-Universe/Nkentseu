// =============================================================================
// NKQwen2SftGpuTest — jalon 6 du chantier QLoRA : l'affinage LoRA RÉEL du
// Qwen2.5 7B sur GPU.
//
// CE QUE CE TEST PROUVE, DANS L'ORDRE
// -----------------------------------
//  (a) LE BACKEND RÉELLEMENT OBTENU est affiché. Rappel de la leçon du jalon 4 :
//      NK_TENSOR_API=software est SILENCIEUSEMENT ignoré, donc un test vert ne
//      dit rien du chemin qu'il a emprunté tant qu'on ne l'imprime pas.
//  (b) LE NOYAU NEUF DU JALON — le matmul TRANSPOSÉ fusé déquantification-produit
//      (dX = dY·W, la somme sur la dimension SAUTÉE) — est comparé à une
//      référence CPU calculée depuis NkGGUFDequantizeRaw, sur de VRAIS tenseurs
//      du blob, en Q4_K ET en Q6_K. C'est la seule pièce dont personne n'avait
//      encore la preuve ; les fragments de décodage de bloc y sont recopiés de
//      NkQ4KGpu/NkQ6KGpu, et c'est cette section qui interdit qu'ils dérivent.
//  (c) LES TROIS PRODUITS f32 (ABt/AB/AtB) qui portent LoRA, contre CPU.
//  (d) LE GRADIENT COMPLET, par différences finies DIRECTIONNELLES sur les
//      ~20 M paramètres d'adaptateurs à la fois. POURQUOI PAS COORDONNÉE PAR
//      COORDONNÉE : perturber UN scalaire de rang 8 dans la couche 12 change la
//      perte de moins que le bruit d'un forward float32 sur 7 milliards de
//      paramètres — on mesurerait le bruit. Une direction aléatoire ALIGNE la
//      perturbation sur les 20 millions de coordonnées à la fois : le signal
//      grandit en √N, le bruit non. C'est le contrôle standard, et c'est le seul
//      qui soit honnête ici.
//  (e) L'ENTRAÎNEMENT sur un échantillon d'un corpus FRANÇAIS réel, avec une
//      VALIDATION DISJOINTE (jamais vue). Les deux courbes sont imprimées. Si la
//      validation ne baisse pas, le test le DIT — c'est de la mémorisation, et
//      le taire serait mentir.
//  (f) NKLA : sauvegarde, écrasement des adaptateurs en VRAM, rechargement, et
//      preuve que les logits redeviennent EXACTEMENT les mêmes (|Δ| == 0 : même
//      code, mêmes poids, GPU déterministe — ici on a le droit d'exiger l'égalité
//      stricte, contrairement à une comparaison CPU/GPU).
//  (g) LES MÊMES 5 QUESTIONS avant et après, côte à côte.
//
// CONTRAINTE DE RÉALISME, écrite noir sur blanc : à ~0,6 s/token de forward, une
// époque sur les 100 017 paires du corpus prendrait des JOURS. Ce test ne
// prétend pas la faire. Il prouve la CHAÎNE de bout en bout sur le vrai modèle
// et il MESURE, pour que la décision suivante repose sur des chiffres.
//
// Usage : NKQwen2SftGpuTest.exe [chemin_gguf_ou_blob_ollama]
// Variables d'env : NK_GGUF_PATH, NK_SFT_CORPUS, NK_SFT_TRAIN (200),
//   NK_SFT_VALID (20), NK_SFT_LR (1e-4), NK_SFT_MAXT (128), NK_SFT_RANK (8),
//   NK_SFT_GENTOK (24), NK_SFT_SKIPGEN=1, NK_SFT_NKLA (chemin de sortie).
//
// Zéro STL.
// =============================================================================
#include "NKInfer/NkQwen2LoraGpu.h"
#include "NKInfer/NkLoraGpu.h"
#include "NKInfer/NkQKGpuBackward.h"
#include "NKInfer/NkQwen2Tokenizer.h"
#include "NKInfer/NkQwen2Sft.h"
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

// Racine carrée en double — <cmath>, comme partout dans NKInfer (NKMath ne
// couvre que les types géométriques, il n'a pas de scalaire float64 ici).
static float64 Sqrt64(float64 v) {
	return (float64)sqrt((double)v);
}

static void check(bool cond, const char *what) {
	printf("  [%s] %s\n", cond ? " OK " : "FAIL", what);
	if (cond)
		++g_ok;
	else
		++g_fail;
}

// Écart max absolu et max relatif normalisé par l'AMPLITUDE de la référence —
// même convention qu'aux jalons 4 et 5 : une erreur relative par ÉLÉMENT
// exploserait sur les sorties proches de zéro et ne dirait rien.
static void Compare(const float32 *a, const float32 *b, int64 n, float64 &maxAbs, float64 &maxRel) {
	maxAbs = 0.0;
	float64 scale = 0.0;
	for (int64 i = 0; i < n; ++i) {
		const float64 dv = a[i] > b[i] ? (float64)a[i] - (float64)b[i] : (float64)b[i] - (float64)a[i];
		if (dv > maxAbs)
			maxAbs = dv;
		const float64 m = b[i] < 0.0f ? -(float64)b[i] : (float64)b[i];
		if (m > scale)
			scale = m;
	}
	maxRel = (scale > 0.0) ? (maxAbs / scale) : maxAbs;
}

// RNG local reproductible (xorshift64*) — le même moteur que NkLoraRng, mais on
// n'a besoin ici que de valeurs de test, pas d'une gaussienne calibrée.
struct Rng {
		uint64 s = 0x243F6A8885A308D3ull;
		float32 Next() {
			s ^= s >> 12;
			s ^= s << 25;
			s ^= s >> 27;
			const uint64 v = s * 2685821657736338717ull;
			return (float32)((double)(v >> 11) / 9007199254740992.0) * 2.0f - 1.0f;
		}
};

static const char *EnvOr(const char *name, const char *def) {
	const char *v = getenv(name);
	return (v && v[0]) ? v : def;
}
static int32 EnvInt(const char *name, int32 def) {
	const char *v = getenv(name);
	return (v && v[0]) ? (int32)atoi(v) : def;
}
static float32 EnvFloat(const char *name, float32 def) {
	const char *v = getenv(name);
	return (v && v[0]) ? (float32)atof(v) : def;
}

// =============================================================================
// (b) Référence CPU du matmul TRANSPOSÉ, sur une TRANCHE d'un vrai tenseur.
//
// Les lignes d'un tenseur GGUF sont contiguës et un super-bloc ne dépend
// d'aucun autre : une tranche de lignes est donc un tenseur quantifié VALIDE à
// elle seule (c'est déjà ce qu'exploitait la preuve bit-à-bit du jalon 4). On
// prend 64 lignes — assez pour exercer les huit tuiles de n, assez peu pour que
// la déquantification CPU de référence reste instantanée.
// =============================================================================
namespace {

	const NkGGUFTensorInfo *FindTensor(const NkGGUFFile &g, const char *name) {
		for (uint32 i = 0; i < g.tensors.Size(); ++i)
			if (g.tensors[i].name.Compare(name) == 0)
				return &g.tensors[i];
		return nullptr;
	}

	// dX[m,k] = Σ_n dY[m,n]·W[n,k], n CROISSANT — le même ordre que le noyau.
	void CpuMatmulT(const float32 *dY, const float32 *W, float32 *dX, int64 M, int64 N, int64 K) {
		for (int64 m = 0; m < M; ++m) {
			float32 *row = dX + m * K;
			for (int64 k = 0; k < K; ++k)
				row[k] = 0.0f;
			for (int64 n = 0; n < N; ++n) {
				const float32 g = dY[m * N + n];
				const float32 *wr = W + n * K;
				for (int64 k = 0; k < K; ++k)
					row[k] += g * wr[k];
			}
		}
	}

	bool CheckMatmulTOnTensor(const char *path, const NkGGUFFile &gguf, const char *tensorName, int64 sliceRows,
							  int64 M, const char *label) {
		const NkGGUFTensorInfo *info = FindTensor(gguf, tensorName);
		if (!info || info->dims.Size() != 2) {
			printf("    tenseur '%s' introuvable\n", tensorName);
			return false;
		}
		const int64 K = (int64)info->dims[0]; // in_features (contigu)
		NkVector<uint8> raw;
		NkString err;
		if (!NkGGUFReadTensorRawBytes(path, gguf, *info, raw, &err)) {
			printf("    lecture de '%s' : %s\n", tensorName, err.CStr());
			return false;
		}
		const bool isQ4 = (info->rawType == (uint32)NkGGUFTensorType::NK_GGML_Q4_K);
		const uint64 sliceBytes =
			isQ4 ? NkQ4KExpectedBytes(sliceRows, K) : NkQ6KExpectedBytes(sliceRows, K);
		if (sliceBytes == 0 || sliceBytes > (uint64)raw.Size()) {
			printf("    tranche impossible sur '%s'\n", tensorName);
			return false;
		}

		// Référence CPU : la MÊME déquantification que partout ailleurs.
		NkVector<float32> W;
		if (!NkGGUFDequantizeRaw(info->rawType, raw.Data(), sliceBytes, (uint64)(sliceRows * K), W, &err)) {
			printf("    déquantification CPU : %s\n", err.CStr());
			return false;
		}

		Rng rng;
		NkVector<float32> dY;
		dY.Resize((NkVector<float32>::SizeType)(M * sliceRows));
		for (uint32 i = 0; i < dY.Size(); ++i)
			dY[i] = rng.Next() * 0.1f;
		NkVector<float32> refX;
		refX.Resize((NkVector<float32>::SizeType)(M * K));
		CpuMatmulT(dY.Data(), W.Data(), refX.Data(), M, sliceRows, K);

		NkTensorGpu &gpu = NkTensorGpu::Get();
		uint64 bufDY = gpu.CreateBuffer((nk_size)((uint64)M * (uint64)sliceRows * sizeof(float32)));
		uint64 bufDX = gpu.CreateBuffer((nk_size)((uint64)M * (uint64)K * sizeof(float32)));
		bool ok = (bufDY != 0 && bufDX != 0) &&
				  gpu.Upload(bufDY, dY.Data(), (nk_size)((uint64)M * (uint64)sliceRows * sizeof(float32)));
		NkQ4KGpuWeight w4;
		NkQ6KGpuWeight w6;
		if (ok) {
			if (isQ4)
				ok = NkQ4KGpuUpload(raw.Data(), sliceBytes, sliceRows, K, w4, &err);
			else
				ok = NkQ6KGpuUpload(raw.Data(), sliceBytes, sliceRows, K, w6, &err);
		}
		if (ok)
			ok = isQ4 ? NkQ4KGpuMatmulT(w4, bufDY, bufDX, M, false, &err)
					  : NkQ6KGpuMatmulT(w6, bufDY, bufDX, M, false, &err);
		NkVector<float32> gotX;
		if (ok) {
			gotX.Resize((NkVector<float32>::SizeType)(M * K));
			ok = gpu.Download(bufDX, gotX.Data(), (nk_size)((uint64)M * (uint64)K * sizeof(float32)));
		}
		if (!ok)
			printf("    échec GPU : %s\n", err.CStr());

		float64 maxAbs = 0.0, maxRel = 1.0;
		if (ok)
			Compare(gotX.Data(), refX.Data(), M * K, maxAbs, maxRel);
		printf("    %s [%s] N=%lld K=%lld M=%lld : |Δ|max = %.3e, rel = %.3e\n", label,
			   NkGGUFTensorTypeName(info->rawType), (long long)sliceRows, (long long)K, (long long)M, maxAbs, maxRel);

		if (isQ4)
			NkQ4KGpuRelease(w4);
		else
			NkQ6KGpuRelease(w6);
		if (bufDY)
			gpu.DestroyBuffer(bufDY);
		if (bufDX)
			gpu.DestroyBuffer(bufDX);
		// Seuil 1e-4 : la somme porte sur N=64 termes en float32 des DEUX côtés,
		// et le GPU peut contracter en FMA là où le CPU ne le fait pas. Exiger
		// l'égalité bit-à-bit ici serait exiger un accident — c'est la
		// déquantification SEULE qui doit tomber juste (prouvée au jalon 4).
		return ok && maxRel <= 1e-4;
	}

	// ---- (c) produits f32 -------------------------------------------------
	bool CheckF32Matmuls() {
		NkTensorGpu &gpu = NkTensorGpu::Get();
		const int64 M = 13, N = 24, K = 37; // volontairement non alignés sur 8
		Rng rng;
		NkVector<float32> A, B, ref, got;
		A.Resize((NkVector<float32>::SizeType)(M * K));
		B.Resize((NkVector<float32>::SizeType)(N * K));
		for (uint32 i = 0; i < A.Size(); ++i)
			A[i] = rng.Next();
		for (uint32 i = 0; i < B.Size(); ++i)
			B[i] = rng.Next();
		ref.Resize((NkVector<float32>::SizeType)(M * N));
		got.Resize((NkVector<float32>::SizeType)(M * N));

		uint64 bA = gpu.CreateBuffer((nk_size)(A.Size() * sizeof(float32)));
		uint64 bB = gpu.CreateBuffer((nk_size)(B.Size() * sizeof(float32)));
		uint64 bY = gpu.CreateBuffer((nk_size)((uint64)M * (uint64)N * sizeof(float32)));
		bool ok = bA && bB && bY && gpu.Upload(bA, A.Data(), (nk_size)(A.Size() * sizeof(float32))) &&
				  gpu.Upload(bB, B.Data(), (nk_size)(B.Size() * sizeof(float32)));
		NkString err;
		float64 worst = 0.0;

		// ABt : Y[m,n] = Σ_k A[m,k]·B[n,k]
		if (ok) {
			for (int64 m = 0; m < M; ++m)
				for (int64 n = 0; n < N; ++n) {
					float32 s = 0.0f;
					for (int64 k = 0; k < K; ++k)
						s += A[(uint32)(m * K + k)] * B[(uint32)(n * K + k)];
					ref[(uint32)(m * N + n)] = s * 2.0f;
				}
			ok = NkGpuMatmulABt(bA, bB, bY, M, N, K, 2.0f, false, &err) &&
				 gpu.Download(bY, got.Data(), (nk_size)((uint64)M * (uint64)N * sizeof(float32)));
			float64 a1 = 0.0, r1 = 1.0;
			if (ok) {
				Compare(got.Data(), ref.Data(), M * N, a1, r1);
				worst = r1 > worst ? r1 : worst;
			}
		}
		// AB : Y[m,n] = Σ_k A[m,k]·B2[k,n]  (B relu en [K,N])
		if (ok) {
			for (int64 m = 0; m < M; ++m)
				for (int64 n = 0; n < N; ++n) {
					float32 s = 0.0f;
					for (int64 k = 0; k < K; ++k)
						s += A[(uint32)(m * K + k)] * B[(uint32)(k * N + n)];
					ref[(uint32)(m * N + n)] = s;
				}
			ok = NkGpuMatmulAB(bA, bB, bY, M, N, K, 1.0f, false, &err) &&
				 gpu.Download(bY, got.Data(), (nk_size)((uint64)M * (uint64)N * sizeof(float32)));
			float64 a2 = 0.0, r2 = 1.0;
			if (ok) {
				Compare(got.Data(), ref.Data(), M * N, a2, r2);
				worst = r2 > worst ? r2 : worst;
			}
		}
		// AtB : Y[m,n] = Σ_k A2[k,m]·B2[k,n]
		if (ok) {
			for (int64 m = 0; m < M; ++m)
				for (int64 n = 0; n < N; ++n) {
					float32 s = 0.0f;
					for (int64 k = 0; k < K; ++k)
						s += A[(uint32)(k * M + m)] * B[(uint32)(k * N + n)];
					ref[(uint32)(m * N + n)] = s;
				}
			ok = NkGpuMatmulAtB(bA, bB, bY, M, N, K, 1.0f, false, &err) &&
				 gpu.Download(bY, got.Data(), (nk_size)((uint64)M * (uint64)N * sizeof(float32)));
			float64 a3 = 0.0, r3 = 1.0;
			if (ok) {
				Compare(got.Data(), ref.Data(), M * N, a3, r3);
				worst = r3 > worst ? r3 : worst;
			}
		}
		printf("    trois orientations f32 (ABt / AB / AtB), M=%lld N=%lld K=%lld : rel max = %.3e\n", (long long)M,
			   (long long)N, (long long)K, worst);
		if (!ok)
			printf("    échec : %s\n", err.CStr());
		if (bA)
			gpu.DestroyBuffer(bA);
		if (bB)
			gpu.DestroyBuffer(bB);
		if (bY)
			gpu.DestroyBuffer(bY);
		return ok && worst <= 1e-5;
	}

	// ---- corpus -----------------------------------------------------------
	struct QaPair {
			NkString q, r;
	};

	// Lit les `maxBytes` premiers octets et en extrait les paires
	// « Question: … / Reponse: … ». Le corpus fait 25 Mo pour 100 017 paires ;
	// on n'en lit qu'un début, c'est assez pour quelques centaines d'exemples et
	// ça évite de tenir 25 Mo en mémoire pour rien.
	bool LoadCorpus(const char *path, uint64 maxBytes, NkVector<QaPair> &out) {
		NkFile f(path, NkFileMode::NK_READ_BINARY);
		if (!f.IsOpen())
			return false;
		NkVector<char> buf;
		buf.Resize((NkVector<char>::SizeType)(maxBytes + 1));
		const usize got = f.Read(buf.Data(), (usize)maxBytes);
		buf[(NkVector<char>::SizeType)got] = 0;
		const char *p = buf.Data();
		const char *end = p + got;
		NkString pendingQ;
		bool haveQ = false;
		NkVector<char> line;
		while (p < end) {
			const char *nl = p;
			while (nl < end && *nl != '\n')
				++nl;
			int64 len = nl - p;
			while (len > 0 && (p[len - 1] == '\r'))
				--len;
			line.Resize((NkVector<char>::SizeType)(len + 1));
			if (len > 0)
				memcpy(line.Data(), p, (usize)len);
			line[(NkVector<char>::SizeType)len] = 0;
			const char *s = line.Data();
			if (len > 10 && strncmp(s, "Question: ", 10) == 0) {
				pendingQ = NkString(s + 10);
				haveQ = true;
			} else if (len > 9 && strncmp(s, "Reponse: ", 9) == 0 && haveQ) {
				QaPair qa;
				qa.q = pendingQ;
				qa.r = NkString(s + 9);
				out.PushBack(qa);
				haveQ = false;
			}
			p = (nl < end) ? nl + 1 : end;
		}
		return out.Size() > 0;
	}

	// ---- (g) génération d'une réponse ------------------------------------
	NkString AskModel(NkQwen2LoraGpu &model, const NkQwen2Tokenizer &tok, const NkString &question, int32 maxNew,
					  float64 &seconds) {
		NkString prompt = NkString("<|im_start|>user\n") + question +
						  NkString("<|im_end|>\n<|im_start|>assistant\n");
		NkVector<int32> ids;
		if (!tok.EncodeWithSpecials(prompt, ids))
			return NkString("<échec d'encodage>");
		NkVector<int32> outIds;
		NkString err;
		if (!model.Generate(ids, maxNew, tok.ImEndId(), outIds, &seconds, &err))
			return NkString("<échec : ") + err + NkString(">");
		// Le <|im_end|> final est retiré de l'affichage : c'est un marqueur, pas
		// une réponse.
		NkVector<int32> shown;
		for (uint32 i = 0; i < outIds.Size(); ++i)
			if (outIds[i] != tok.ImEndId())
				shown.PushBack(outIds[i]);
		return tok.Decode(shown);
	}

} // namespace

// =============================================================================
int main(int argc, char **argv) {
	printf("\n=== NKQwen2SftGpuTest — jalon 6 : affinage LoRA RÉEL du 7B sur GPU ===\n\n");

	// Vulkan verrouillé : output.weight (Q6_K, 447 Mo) dépasse les 128 Mo que
	// D3D11 GARANTIT par ressource. On respecte la variable si elle est déjà
	// posée, et on AFFICHE le backend réellement obtenu — parce que
	// NK_TENSOR_API=software est silencieusement ignoré (leçon du jalon 4).
	if (!getenv("NK_TENSOR_API"))
		_putenv_s("NK_TENSOR_API", "vulkan");

	const char *path = (argc > 1) ? argv[1]
								  : EnvOr("NK_GGUF_PATH",
										  "C:/Users/Rihen/.ollama/models/blobs/"
										  "sha256-2bada8a7450677000f678be90653b85d364de7db25eb5ea54136ada5f3933730");
	const char *corpusPath = EnvOr("NK_SFT_CORPUS", "D:/Projets/Camrail/AI/BulkGen/dlg_ollama_fr.txt");
	const int32 nTrain = EnvInt("NK_SFT_TRAIN", 200);
	const int32 nValid = EnvInt("NK_SFT_VALID", 20);
	const float32 lr = EnvFloat("NK_SFT_LR", 1e-4f);
	const int32 maxT = EnvInt("NK_SFT_MAXT", 128);
	const int32 rank = EnvInt("NK_SFT_RANK", 8);
	const int32 genTok = EnvInt("NK_SFT_GENTOK", 24);
	const bool skipGen = EnvInt("NK_SFT_SKIPGEN", 0) != 0;
	const char *nklaPath = EnvOr("NK_SFT_NKLA", "qwen2_lora_r8.nkla");

	NkTensorGpu &gpu = NkTensorGpu::Get();
	printf("(a) BACKEND RÉELLEMENT OBTENU\n");
	printf("    NK_TENSOR_API demandé = %s\n", EnvOr("NK_TENSOR_API", "(non posé)"));
	printf("    backend = %s\n", gpu.BackendName());
	check(gpu.IsAvailable(), "un device compute GPU est disponible");
	if (!gpu.IsAvailable()) {
		printf("\nAucun GPU : le jalon 6 n'a pas de repli CPU réaliste. Arrêt.\n");
		return 1;
	}

	// =====================================================================
	printf("\n(b) NOYAU NEUF : matmul TRANSPOSÉ fusé (dX = dY·W) vs référence CPU\n");
	{
		NkGGUFFile gguf;
		if (!NkGGUFLoader::Load(path, gguf) || !gguf.valid) {
			printf("    GGUF illisible : %s\n", path);
			check(false, "GGUF lisible");
		} else {
			check(CheckMatmulTOnTensor(path, gguf, "blk.0.attn_q.weight", 64, 16, "Q4_K attn_q"),
				  "matmul transposé Q4_K == CPU (vrai tenseur du blob)");
			check(CheckMatmulTOnTensor(path, gguf, "blk.0.ffn_down.weight", 64, 16, "Q6_K ffn_down"),
				  "matmul transposé Q6_K == CPU (vrai tenseur du blob, blocs de 210 octets)");
			// Deuxième M, pour exercer le cas M non multiple de 8 ET le régime
			// de l'entraînement (M = longueur de séquence).
			check(CheckMatmulTOnTensor(path, gguf, "blk.0.attn_q.weight", 64, 100, "Q4_K attn_q, M=100"),
				  "matmul transposé Q4_K == CPU à M=100 (M non multiple de 8)");
		}
	}

	printf("\n(c) PRODUITS f32 DES ADAPTATEURS\n");
	check(CheckF32Matmuls(), "les trois orientations f32 (ABt/AB/AtB) == CPU");

	// =====================================================================
	printf("\n(d) CHARGEMENT DU MODÈLE ENTRAÎNABLE\n");
	NkQwen2LoraGpuOptions opt;
	opt.maxSeqLen = maxT;
	opt.loraRank = rank;
	opt.loraAlpha = (float32)(2 * rank); // alpha/r = 2, l'échelle usuelle
	opt.verbose = true;
	NkQwen2LoraGpu model;
	NkString err;
	NkChrono loadClk;
	const bool loaded = model.Load(path, opt, &err);
	const float64 loadSec = loadClk.Elapsed().ToSeconds();
	if (!loaded)
		printf("    erreur : %s\n", err.CStr());
	check(loaded, "socle 7B quantifié résident GPU + adaptateurs LoRA sur les 28 couches");
	if (!loaded)
		return 1;

	const NkQwen2LoraGpuStats &st = model.Stats();
	printf("    chargement %.1f s (dont %.1f s de lecture disque)\n", loadSec, st.readSeconds);
	printf("    VRAM comptabilisée %.2f Go = socle %.2f Go + LoRA %.0f Mo + Adam %.0f Mo\n"
		   "                       + checkpoints %.0f Mo + activations/gradients %.0f Mo (%u tampons)\n",
		   (double)st.TotalBytes() / 1073741824.0, (double)st.weightBytes / 1073741824.0,
		   (double)st.loraBytes / 1048576.0, (double)st.optimBytes / 1048576.0, (double)st.ckptBytes / 1048576.0,
		   (double)st.scratchBytes / 1048576.0, st.bufferCount);
	printf("    tenseurs : %llu Q4_K, %llu Q6_K, %llu F32 — %lld paramètres ENTRAÎNABLES sur ~7,6 G (%.3f %%)\n",
		   (unsigned long long)st.q4Tensors, (unsigned long long)st.q6Tensors, (unsigned long long)st.f32Tensors,
		   (long long)st.loraParams, 100.0 * (double)st.loraParams / 7.6e9);

	// ---- tokenizer ----
	NkQwen2Tokenizer tok;
	if (!tok.LoadFromGGUF(path, &err)) {
		printf("    tokenizer : %s\n", err.CStr());
		check(false, "tokenizer chargé");
		return 1;
	}
	check(tok.ImStartId() == 151644 && tok.ImEndId() == 151645, "tokens ChatML résolus (<|im_start|>=151644, <|im_end|>=151645)");

	// ---- corpus ----
	NkVector<QaPair> pairs;
	if (!LoadCorpus(corpusPath, 4ull * 1024ull * 1024ull, pairs)) {
		printf("    corpus illisible : %s\n", corpusPath);
		check(false, "corpus chargé");
		return 1;
	}
	printf("    corpus : %u paires lues dans les 4 premiers Mo de %s\n", (unsigned)pairs.Size(), corpusPath);

	// Formatage ChatML + masque de perte. Les exemples trop longs sont ÉCARTÉS,
	// jamais tronqués : une réponse coupée apprendrait au modèle à ne pas finir
	// ses phrases.
	NkVector<NkQwen2SftExample> examples;
	NkVector<uint32> srcIndex;
	for (uint32 i = 0; i < pairs.Size(); ++i) {
		NkQwen2SftExample ex;
		if (!NkQwen2SftFormatChatML(tok, pairs[i].q, pairs[i].r, ex, &err))
			continue;
		if ((int32)ex.tokens.Size() > maxT + 1)
			continue;
		examples.PushBack(ex);
		srcIndex.PushBack(i);
	}
	printf("    %u exemples formatés tiennent dans maxT=%d\n", (unsigned)examples.Size(), maxT);
	check(examples.Size() >= (uint32)(nTrain + nValid + 500), "assez d'exemples pour un entraînement ET une validation disjointe");

	// Train = les nTrain premiers ; validation = nValid pris 500 exemples PLUS
	// LOIN, donc sur d'autres sujets — une validation prise juste après aurait
	// partagé le thème du dernier lot d'entraînement et aurait flatté le score.
	const uint32 validStart = (uint32)nTrain + 500u;
	int64 sumTok = 0;
	for (int32 i = 0; i < nTrain && (uint32)i < examples.Size(); ++i)
		sumTok += (int64)examples[(uint32)i].tokens.Size();
	printf("    entraînement : %d exemples (longueur moyenne %.1f tokens) — validation : %d exemples, à partir de l'index %u\n",
		   nTrain, (double)sumTok / (double)nTrain, nValid, validStart);

	// =====================================================================
	// (e) LES 5 QUESTIONS — AVANT
	// =====================================================================
	static const char *kQuestions[5] = {
		"Quelle est la capitale du Cameroun ?",
		"Qu'est-ce qu'un syllogisme ?",
		"Comment resoudre une enigme de logique ?",
		"Qu'est-ce qu'une proposition logique ?",
		"Pourquoi le ciel est-il bleu ?",
	};
	NkString beforeAns[5];
	if (!skipGen) {
		printf("\n(e) LES 5 QUESTIONS — AVANT AFFINAGE (B = 0, donc le socle SEUL)\n");
		for (int i = 0; i < 5; ++i) {
			float64 sec = 0.0;
			beforeAns[i] = AskModel(model, tok, NkString(kQuestions[i]), genTok, sec);
			printf("    Q%d : %s\n     -> %s   [%.1f s]\n", i + 1, kQuestions[i], beforeAns[i].CStr(), sec);
		}
	}

	// =====================================================================
	// (f) PREUVE DU GRADIENT — différences finies DIRECTIONNELLES
	// =====================================================================
	printf("\n(f) PREUVE DU GRADIENT COMPLET (différences finies directionnelles)\n");
	{
		NkQwen2LoraGpuTrainer probe;
		probe.Init(&model, lr, &err);
		const NkQwen2SftExample &ex = examples[0];
		NkVector<int32> inputs, targets;
		NkVector<float32> mask;
		for (uint32 t = 0; t + 1 < ex.tokens.Size(); ++t) {
			inputs.PushBack(ex.tokens[t]);
			targets.PushBack(ex.tokens[t + 1]);
			mask.PushBack(ex.lossMask[t + 1]);
		}
		const int32 T = (int32)inputs.Size();

		// 1. gradient analytique
		bool ok = model.ZeroAllGrads(&err) && model.Forward(inputs.Data(), T, &err);
		float64 L0 = 0.0;
		int64 active = 0;
		ok = ok && model.Loss(targets, mask, true, L0, active, &err);
		ok = ok && model.Backward(T, &err);
		printf("    exemple sonde : T=%d, %lld positions actives, perte initiale %.6f\n", T, (long long)active, L0);

		// 2. tout descendre sur CPU : paramètres, gradients, et une direction
		NkVector<NkLoraGpuSet> &sets = model.Lora();
		NkVector<float32> params, grads, dir, tmp;
		int64 total = 0;
		for (uint32 l = 0; l < sets.Size() && ok; ++l)
			for (int32 i = 0; i < NkLoraGpuSet::kCount && ok; ++i)
				total += sets[l].At(i)->Params();
		params.Resize((NkVector<float32>::SizeType)total);
		grads.Resize((NkVector<float32>::SizeType)total);
		dir.Resize((NkVector<float32>::SizeType)total);
		int64 off = 0;
		for (uint32 l = 0; l < sets.Size() && ok; ++l) {
			for (int32 i = 0; i < NkLoraGpuSet::kCount && ok; ++i) {
				const NkLoraGpuPair &pr = *sets[l].At(i);
				const uint64 counts[2] = {(uint64)pr.NumelA(), (uint64)pr.NumelB()};
				const uint64 pb[2] = {pr.A, pr.B};
				const uint64 gb[2] = {pr.dA, pr.dB};
				for (int s = 0; s < 2 && ok; ++s) {
					ok = gpu.Download(pb[s], params.Data() + off, (nk_size)(counts[s] * sizeof(float32))) &&
						 gpu.Download(gb[s], grads.Data() + off, (nk_size)(counts[s] * sizeof(float32)));
					off += (int64)counts[s];
				}
			}
		}
		check(ok && off == total, "paramètres et gradients relus depuis la VRAM");

		// LA DIRECTION EST LE GRADIENT LUI-MÊME, normalisé à un RMS de 1 par
		// coordonnée. POURQUOI PAS UNE DIRECTION ALÉATOIRE — c'est le premier
		// réglage essayé, et il ÉCHOUE pour une raison instructive : sur 20 M
		// coordonnées de signes indépendants, Σ g_i δ_i s'annule presque
		// entièrement (mesuré : 0,537 au lieu de 25 553 pour la même amplitude de
		// perturbation). La différence de pertes tombe alors à 1,2e-4 sur une
		// perte de 2,24 calculée en float32 à travers 7 milliards de paramètres :
		// on mesure le BRUIT du forward, pas la dérivée. Aligner δ sur g donne le
		// signal MAXIMAL à perturbation égale — et le contrôle reste indépendant,
		// puisque les deux pertes ne passent que par le FORWARD, jamais par le
		// backward qu'on veut juger.
		//
		// ε est ADAPTATIF : choisi pour que la perte bouge d'environ 1 %. Trop
		// petit, on retombe dans le bruit ; trop grand, le terme d'ordre 2 mord.
		float64 rms = 0.0;
		for (int64 i = 0; i < total; ++i)
			rms += (float64)grads[(uint32)i] * (float64)grads[(uint32)i];
		const float64 gNorm = rms > 0.0 ? Sqrt64(rms) : 0.0;
		rms = total > 0 ? Sqrt64(rms / (float64)total) : 0.0;
		float64 analytic = 0.0;
		if (rms > 0.0) {
			for (int64 i = 0; i < total; ++i) {
				dir[(uint32)i] = (float32)((float64)grads[(uint32)i] / rms);
				analytic += (float64)grads[(uint32)i] * (float64)dir[(uint32)i];
			}
		}
		float64 target = 0.01 * (L0 > 0.1 ? L0 : 0.1);
		float32 epsFd = (analytic > 0.0) ? (float32)(target / analytic) : 1e-6f;
		if (epsFd < 1e-9f)
			epsFd = 1e-9f;
		if (epsFd > 1e-2f)
			epsFd = 1e-2f;
		printf("    ||g||₂ = %.4f, RMS(g) = %.3e, dérivée directionnelle analytique = %.3f, ε = %.3e\n", gNorm, rms,
			   analytic, (double)epsFd);

		// 3. L(θ ± εδ) : DEUX forwards de plus, rien d'autre.
		float64 Lp = 0.0, Lm = 0.0;
		tmp.Resize((NkVector<float32>::SizeType)total);
		for (int sign = 0; sign < 2 && ok; ++sign) {
			const float32 e = (sign == 0) ? epsFd : -epsFd;
			for (int64 i = 0; i < total; ++i)
				tmp[(uint32)i] = params[(uint32)i] + e * dir[(uint32)i];
			int64 o2 = 0;
			for (uint32 l = 0; l < sets.Size() && ok; ++l) {
				for (int32 i = 0; i < NkLoraGpuSet::kCount && ok; ++i) {
					const NkLoraGpuPair &pr = *sets[l].At(i);
					const uint64 counts[2] = {(uint64)pr.NumelA(), (uint64)pr.NumelB()};
					const uint64 pb[2] = {pr.A, pr.B};
					for (int s = 0; s < 2 && ok; ++s) {
						ok = gpu.Upload(pb[s], tmp.Data() + o2, (nk_size)(counts[s] * sizeof(float32)));
						o2 += (int64)counts[s];
					}
				}
			}
			float64 L = 0.0;
			int64 a2 = 0;
			ok = ok && model.Forward(inputs.Data(), T, &err) && model.Loss(targets, mask, false, L, a2, &err);
			if (sign == 0)
				Lp = L;
			else
				Lm = L;
		}
		// 4. restauration EXACTE des paramètres d'origine
		{
			int64 o3 = 0;
			for (uint32 l = 0; l < sets.Size() && ok; ++l)
				for (int32 i = 0; i < NkLoraGpuSet::kCount && ok; ++i) {
					const NkLoraGpuPair &pr = *sets[l].At(i);
					const uint64 counts[2] = {(uint64)pr.NumelA(), (uint64)pr.NumelB()};
					const uint64 pb[2] = {pr.A, pr.B};
					for (int s = 0; s < 2 && ok; ++s) {
						ok = gpu.Upload(pb[s], params.Data() + o3, (nk_size)(counts[s] * sizeof(float32)));
						o3 += (int64)counts[s];
					}
				}
		}
		const float64 numeric = (Lp - Lm) / (2.0 * (float64)epsFd);
		const float64 den = (analytic < 0 ? -analytic : analytic) + (numeric < 0 ? -numeric : numeric);
		const float64 rel = den > 0.0 ? ((analytic - numeric) < 0 ? numeric - analytic : analytic - numeric) / den : 0.0;
		printf("    L(θ+εδ) = %.6f, L(θ−εδ) = %.6f, ε = %g\n", Lp, Lm, (double)epsFd);
		printf("    dérivée directionnelle : analytique %.6f, numérique %.6f, écart relatif %.3e\n", analytic, numeric,
			   rel);
		check(ok && rel < 2e-2,
			  "gradient GPU des 28 couches == différences finies (chaîne backward complète prouvée)");
	}

	// =====================================================================
	// (g) ENTRAÎNEMENT + VALIDATION DISJOINTE
	// =====================================================================
	printf("\n(g) ENTRAÎNEMENT (%d exemples, lr = %g, Adam, B = 1) + VALIDATION (%d exemples jamais vus)\n", nTrain,
		   (double)lr, nValid);
	NkQwen2LoraGpuTrainer trainer;
	if (!trainer.Init(&model, lr, &err)) {
		check(false, "trainer initialisé");
		return 1;
	}

	auto Validate = [&](void) -> float64 {
		float64 s = 0.0;
		int32 n = 0;
		for (int32 i = 0; i < nValid; ++i) {
			const uint32 idx = validStart + (uint32)i;
			if (idx >= examples.Size())
				break;
			NkString e2;
			const double L = trainer.EvaluateExample(examples[idx], &e2);
			if (L >= 0.0) {
				s += L;
				++n;
			}
		}
		return n > 0 ? s / (float64)n : -1.0;
	};

	const float64 valid0 = Validate();
	printf("    validation AVANT tout pas : %.4f\n", valid0);

	float64 firstBlockTrain = 0.0, lastBlockTrain = 0.0;
	float64 validLast = valid0;
	float64 totalTrainSec = 0.0;
	const int32 blockSize = 25;
	int32 done = 0;
	bool trainOk = true;
	for (int32 i = 0; i < nTrain && trainOk; ++i) {
		if ((uint32)i >= examples.Size())
			break;
		NkChrono stepClk;
		const double L = trainer.TrainExample(examples[(uint32)i], &err);
		const float64 sec = stepClk.Elapsed().ToSeconds();
		totalTrainSec += sec;
		if (L < 0.0) {
			printf("    pas %d : %s\n", i, err.CStr());
			trainOk = false;
			break;
		}
		++done;
		if (i < blockSize)
			firstBlockTrain += L;
		if (i >= nTrain - blockSize)
			lastBlockTrain += L;
		if ((i + 1) % blockSize == 0 || i == 0) {
			const float64 v = Validate();
			validLast = v;
			printf("    pas %4d/%d | perte train %.4f | validation %.4f | %.2f s/pas (T=%u)\n", i + 1, nTrain, L, v,
				   sec, (unsigned)examples[(uint32)i].tokens.Size());
		}
	}
	check(trainOk && done > 0, "l'entraînement a tourné jusqu'au bout sans erreur GPU");
	const float64 trainStart = firstBlockTrain / (float64)(done < blockSize ? done : blockSize);
	const float64 trainEnd = lastBlockTrain / (float64)blockSize;
	const float64 msPerStep = done > 0 ? totalTrainSec * 1000.0 / (float64)done : 0.0;
	printf("\n    RÉSULTAT : perte train %.4f (25 premiers) -> %.4f (25 derniers) | validation %.4f -> %.4f\n",
		   trainStart, trainEnd, valid0, validLast);
	printf("    temps : %.2f s/pas en moyenne (%.0f ms), %.1f s pour %d pas\n", msPerStep / 1000.0, msPerStep,
		   totalTrainSec, done);
	// EXTRAPOLATION HONNÊTE — le chiffre que le jalon devait produire.
	printf("    EXTRAPOLATION : une époque sur les 100 017 paires = %.0f s = %.1f h = %.1f jours\n",
		   msPerStep * 100017.0 / 1000.0, msPerStep * 100017.0 / 3600000.0, msPerStep * 100017.0 / 86400000.0);
	check(trainEnd < trainStart, "la perte d'ENTRAÎNEMENT baisse nettement");
	check(validLast < valid0, "la perte de VALIDATION baisse aussi (ce n'est donc pas de la seule mémorisation)");

	float64 gnorm = 0.0;
	if (model.GradGlobalNorm(gnorm, &err))
		printf("    norme L2 globale du dernier gradient : %.4e\n", gnorm);

	// =====================================================================
	// (h) NKLA : sauvegarde -> écrasement -> rechargement -> mêmes logits
	// =====================================================================
	printf("\n(h) FORMAT NKLA : sauvegarde, écrasement en VRAM, rechargement\n");
	{
		const NkQwen2SftExample &probe = examples[0];
		NkVector<int32> pin;
		for (uint32 t = 0; t + 1 < probe.tokens.Size(); ++t)
			pin.PushBack(probe.tokens[t]);
		const int32 T = (int32)pin.Size();

		NkVector<float32> logitsRef, logitsZero, logitsBack;
		bool ok = model.Forward(pin.Data(), T, &err) && model.DownloadLogitsRow(T - 1, T, logitsRef, &err);

		NkLoraGpuNklaInfo info;
		ok = ok && NkLoraGpuSaveNKLA(nklaPath, model.Lora(), rank, opt.loraAlpha, model.Config().dModel,
									 model.Config().ffnDim, model.Config().nHeads * model.Config().headDim,
									 model.Config().nKVHeads * model.Config().headDim, &info, &err);
		if (ok)
			printf("    écrit : %s — %llu paramètres, %.1f Mo, rang %u, alpha %g, %u couches × %u projections\n",
				   nklaPath, (unsigned long long)info.paramCount, (double)info.fileBytes / 1048576.0, info.rank,
				   (double)info.alpha, info.layerCount, info.projCount);
		check(ok, "adaptateurs sauvegardés au format NKLA (en-tête + empreinte FNV-1a)");

		// ÉCRASEMENT : on met A et B à zéro en VRAM. Le modèle redevient le socle
		// nu, donc les logits DOIVENT changer — sinon l'adaptateur ne servait à
		// rien et le test suivant ne prouverait rien.
		if (ok) {
			NkVector<NkLoraGpuSet> &sets = model.Lora();
			for (uint32 l = 0; l < sets.Size() && ok; ++l)
				for (int32 i = 0; i < NkLoraGpuSet::kCount && ok; ++i) {
					const NkLoraGpuPair &pr = *sets[l].At(i);
					ok = NkGpuZeroBuffer(pr.A, (uint64)pr.NumelA() * sizeof(float32), &err) &&
						 NkGpuZeroBuffer(pr.B, (uint64)pr.NumelB() * sizeof(float32), &err);
				}
		}
		ok = ok && model.Forward(pin.Data(), T, &err) && model.DownloadLogitsRow(T - 1, T, logitsZero, &err);
		float64 aZ = 0.0, rZ = 0.0;
		if (ok)
			Compare(logitsZero.Data(), logitsRef.Data(), (int64)logitsRef.Size(), aZ, rZ);
		printf("    après écrasement des adaptateurs : |Δ|max sur les logits = %.4e (doit être > 0)\n", aZ);
		check(ok && aZ > 0.0, "les adaptateurs entraînés changent VRAIMENT la sortie du modèle");

		NkLoraGpuNklaInfo back;
		ok = ok && NkLoraGpuLoadNKLA(nklaPath, model.Lora(), &back, &err);
		if (!ok)
			printf("    rechargement : %s\n", err.CStr());
		ok = ok && model.Forward(pin.Data(), T, &err) && model.DownloadLogitsRow(T - 1, T, logitsBack, &err);
		float64 aB = 1.0, rB = 1.0;
		if (ok)
			Compare(logitsBack.Data(), logitsRef.Data(), (int64)logitsRef.Size(), aB, rB);
		printf("    après rechargement NKLA : |Δ|max sur les %lld logits = %.4e\n", (long long)logitsRef.Size(), aB);
		// ICI on a le droit d'exiger l'égalité STRICTE : mêmes poids, même code,
		// même GPU, dispatch déterministe. Un écart, même de 1e-7, dénoncerait
		// une perte de précision à l'écriture ou à la relecture du fichier.
		check(ok && aB == 0.0, "le modèle rechargé donne EXACTEMENT les mêmes logits (|Δ| == 0)");
	}

	// =====================================================================
	// (i) LES 5 QUESTIONS — APRÈS
	// =====================================================================
	if (!skipGen) {
		printf("\n(i) LES 5 QUESTIONS — APRÈS AFFINAGE\n");
		for (int i = 0; i < 5; ++i) {
			float64 sec = 0.0;
			const NkString after = AskModel(model, tok, NkString(kQuestions[i]), genTok, sec);
			printf("    Q%d : %s\n     AVANT : %s\n     APRÈS : %s   [%.1f s]\n", i + 1, kQuestions[i],
				   beforeAns[i].CStr(), after.CStr(), sec);
		}
	}

	printf("\n=== BILAN : %d OK, %d échec(s) — backend = %s ===\n\n", g_ok, g_fail, gpu.BackendName());
	return g_fail == 0 ? 0 : 1;
}
