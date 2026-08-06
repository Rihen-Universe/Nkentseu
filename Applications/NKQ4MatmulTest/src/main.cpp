// =============================================================================
// NKQ4MatmulTest — chantier QLoRA : poids K-quants RÉSIDENTS sur GPU et matmul
// FUSÉ déquantification-produit (NKInfer/NkQ4KGpu + NkQ6KGpu, noyaux NkSL).
//
// Ce que le test prouve, dans l'ordre :
//   (a) DÉQUANTIFICATION GPU == CPU, ÉLÉMENT PAR ÉLÉMENT. Des super-blocs Q4_K
//       sont fabriqués EN DUR côté CPU (aucun fichier requis), uploadés bruts,
//       déquantifiés par le noyau `q4k_dequant`, relus, puis comparés à
//       NkGGUFDequantizeRaw — la VÉRITÉ du dépôt (transcription de ggml).
//       Critère : |Δ| == 0 exactement (mêmes entiers, même formule, même ordre).
//   (b) MATMUL FUSÉ GPU == (dequant CPU puis matmul CPU f32) sur des dimensions
//       de vraie projection 7B : K=3584, N=3584 puis N=18944, M=4 tokens.
//       Critère : erreur relative <= 1e-4.
//   (c) DÉBIT mesuré (Go/s de poids quantifiés lus, GFLOPS) — pour information.
//   (d) Si le blob Qwen2.5 réel est fourni : UN vrai tenseur Q4_K de la couche 0
//       est chargé, uploadé tel quel, et le matmul GPU est comparé à la même
//       référence CPU.
//   (e) Q6_K : mêmes preuves (bit-à-bit sur synthétique, matmul, vrai tenseur).
//       Q6_K est un PRÉREQUIS et non un bonus : dans un GGUF Q4_K_M, certains
//       tenseurs — sur ce blob `blk.*.ffn_down.weight` — sont en Q6_K, et un
//       chemin GPU qui ne sait pas les lire ne peut pas charger le modèle.
//   (f) GEMM : le noyau simple et le noyau TUILÉ mesurés côte à côte à
//       M = 1, 8, 64, 256. Sans ces chiffres, on ne peut pas savoir si le
//       tuilage a servi — ni à partir de quel M il faut basculer.
//
// Usage : NKQ4MatmulTest.exe [chemin_gguf_ou_blob_ollama]
//   (sans argument : tout sauf (d) et le Q6_K réel — aucun fichier nécessaire)
//
// Le backend GPU se choisit par la variable d'environnement NK_TENSOR_API
// (vulkan | opengl | dx11 | dx12 | metal), lue par NkTensorGpu::EnsureInit.
// Sans elle, l'ordre d'essai est Vulkan, Metal, OpenGL, DX11, DX12.
//
// Zéro STL : NkVector/NkString, RNG maison, NkChrono pour le temps.
// =============================================================================
#include "NKInfer/NkQ4KGpu.h"
#include "NKInfer/NkQ6KGpu.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkLora.h" // NkLoraRng : RNG reproductible du dépôt (jalon 2)
#include "NKTensor/NkTensorGpu.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
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

// -----------------------------------------------------------------------------
// Fabrication de super-blocs Q4_K EN DUR (aucun GGUF nécessaire).
//
// Disposition d'un bloc (spec ggml, 144 octets) :
//   [0..1]   d      : super-échelle des échelles, fp16
//   [2..3]   dmin   : super-échelle des minimums, fp16
//   [4..15]  scales : 8 paires (échelle, minimum) 6 bits empaquetées sur 12 o
//   [16..143] qs    : 256 nibbles de 4 bits
//
// `stress` = true : d/dmin balayent TOUT le domaine fp16, dénormaux et zéros
// compris — c'est ce qui met à l'épreuve le décodeur fp16 du shader, et la
// comparaison (a) étant EXACTE, la dynamique extrême ne gêne pas.
// `stress` = false : d/dmin dans la plage d'un vrai modèle (~2^-9..2^-5), pour
// que la comparaison (b) mesure l'erreur du PRODUIT et non une annulation
// catastrophique fabriquée de toutes pièces.
// -----------------------------------------------------------------------------
static void BuildSyntheticQ4K(int64 rows, int64 cols, uint64 seed, bool stress, NkVector<uint8> &raw) {
	const uint64 nBlocks = (uint64)rows * ((uint64)cols / kNkQ4KBlockElems);
	raw.Resize((NkVector<uint8>::SizeType)(nBlocks * kNkQ4KBlockBytes));
	NkLoraRng rng(seed);
	uint8 *p = raw.Data();

	for (uint64 b = 0; b < nBlocks; ++b) {
		uint8 *blk = p + b * kNkQ4KBlockBytes;

		uint32 dBits = 0, mBits = 0;
		if (stress) {
			// Cas particuliers déterministes : dénormal (branche de normalisation
			// du décodeur), zéro, et signe négatif — trois chemins qu'un jeu
			// purement aléatoire raterait presque toujours.
			if (b % 7 == 3) {
				dBits = (uint32)(1 + (rng.NextU64() % 1023)); // dénormal fp16
			} else if (b % 11 == 5) {
				dBits = 0; // +0
			} else {
				const uint32 e = (uint32)(1 + rng.NextU64() % 30);
				const uint32 m = (uint32)(rng.NextU64() % 1024);
				const uint32 s = (rng.NextU64() % 8 == 0) ? 1u : 0u;
				dBits = (s << 15) | (e << 10) | m;
			}
			if (b % 5 == 2) {
				mBits = (uint32)(1 + (rng.NextU64() % 1023)); // dmin dénormal
			} else {
				const uint32 e = (uint32)(1 + rng.NextU64() % 30);
				const uint32 m = (uint32)(rng.NextU64() % 1024);
				dBits |= 0; // (lisibilité : dBits déjà fixé)
				mBits = (e << 10) | m;
			}
		} else {
			const uint32 e1 = (uint32)(6 + rng.NextU64() % 4); // 2^-9 .. 2^-5
			const uint32 e2 = (uint32)(6 + rng.NextU64() % 4);
			dBits = (e1 << 10) | (uint32)(rng.NextU64() % 1024);
			mBits = (e2 << 10) | (uint32)(rng.NextU64() % 1024);
		}
		blk[0] = (uint8)(dBits & 255u);
		blk[1] = (uint8)((dBits >> 8) & 255u);
		blk[2] = (uint8)(mBits & 255u);
		blk[3] = (uint8)((mBits >> 8) & 255u);

		// scales : octets ARBITRAIRES. Le désempaquetage 6 bits utilise aussi les
		// 2 bits de poids fort de scales[0..3] : des octets pleinement aléatoires
		// sont donc le stress correct de get_scale_min_k4.
		for (int i = 0; i < 12; ++i)
			blk[4 + i] = (uint8)(rng.NextU64() & 255u);
		for (int i = 0; i < 128; ++i)
			blk[16 + i] = (uint8)(rng.NextU64() & 255u);
	}
}

// -----------------------------------------------------------------------------
// Fabrication de super-blocs Q6_K EN DUR (aucun GGUF nécessaire).
//
// Disposition d'un bloc (spec ggml, 210 octets — NON divisible par 4, c'est tout
// le sujet du noyau) :
//   [  0..127] ql     : 4 bits BAS de chaque quant (2 quants par octet)
//   [128..191] qh     : 2 bits HAUTS, 4 quants par octet
//   [192..207] scales : 16 échelles int8 SIGNÉES (une par sous-bloc de 16)
//   [208..209] d      : super-échelle fp16
//
// ql/qh/scales reçoivent des octets PLEINEMENT ALÉATOIRES : c'est le seul moyen
// d'exercer les échelles NÉGATIVES (bit 7 à 1), que le shader doit étendre en
// signe à la main. Une erreur d'extension de signe passerait inaperçue sur des
// échelles toutes positives — et inverserait le signe du poids en production.
//
// `stress` = true : d balaye tout le domaine fp16 (dénormaux, zéro, négatifs),
// mêmes cas particuliers déterministes qu'en Q4_K et pour la même raison : ces
// branches ne sortiraient jamais d'un tirage purement aléatoire.
// -----------------------------------------------------------------------------
static void BuildSyntheticQ6K(int64 rows, int64 cols, uint64 seed, bool stress, NkVector<uint8> &raw) {
	const uint64 nBlocks = (uint64)rows * ((uint64)cols / kNkQ6KBlockElems);
	raw.Resize((NkVector<uint8>::SizeType)(nBlocks * kNkQ6KBlockBytes));
	NkLoraRng rng(seed);
	uint8 *p = raw.Data();

	for (uint64 b = 0; b < nBlocks; ++b) {
		uint8 *blk = p + b * kNkQ6KBlockBytes;
		for (int i = 0; i < 208; ++i)
			blk[i] = (uint8)(rng.NextU64() & 255u);

		uint32 dBits = 0;
		if (stress) {
			if (b % 7 == 3) {
				dBits = (uint32)(1 + (rng.NextU64() % 1023)); // dénormal fp16
			} else if (b % 11 == 5) {
				dBits = 0; // +0
			} else {
				const uint32 e = (uint32)(1 + rng.NextU64() % 30);
				const uint32 m = (uint32)(rng.NextU64() % 1024);
				const uint32 s = (rng.NextU64() % 8 == 0) ? 1u : 0u;
				dBits = (s << 15) | (e << 10) | m;
			}
		} else {
			// Plage d'un vrai modèle (~2^-9..2^-5) : la comparaison du PRODUIT doit
			// mesurer l'erreur du noyau, pas une annulation catastrophique fabriquée.
			const uint32 e = (uint32)(6 + rng.NextU64() % 4);
			dBits = (e << 10) | (uint32)(rng.NextU64() % 1024);
		}
		blk[208] = (uint8)(dBits & 255u);
		blk[209] = (uint8)((dBits >> 8) & 255u);
	}
}

// Référence CPU : Y[m,n] = Σ_k X[m,k]·Wd[n,k] (Wd = poids DÉJÀ déquantifié en
// f32, disposition [N,K] comme le GGUF). Accumulation f32, k croissant — le
// même ordre que le noyau GPU, pour que l'écart mesuré soit celui du matériel
// et non celui de deux algorithmes différents.
static void CpuMatmulNT(const float32 *x, int64 M, int64 K, const float32 *wd, int64 N, NkVector<float32> &out) {
	out.Resize((NkVector<float32>::SizeType)(M * N));
	for (int64 m = 0; m < M; ++m) {
		const float32 *xr = x + m * K;
		for (int64 n = 0; n < N; ++n) {
			const float32 *wr = wd + n * K;
			float32 acc = 0.0f;
			for (int64 k = 0; k < K; ++k)
				acc += xr[k] * wr[k];
			out[(NkVector<float32>::SizeType)(m * N + n)] = acc;
		}
	}
}

// Écart max absolu et max relatif (normalisé par l'amplitude de la référence :
// une erreur relative par ÉLÉMENT exploserait sur les rares sorties proches de
// zéro, ce qui ne dit rien de la qualité du noyau).
static void Compare(const NkVector<float32> &a, const NkVector<float32> &b, float64 &maxAbs, float64 &maxRel) {
	maxAbs = 0.0;
	float64 scale = 0.0;
	const NkVector<float32>::SizeType n = a.Size() < b.Size() ? a.Size() : b.Size();
	for (NkVector<float32>::SizeType i = 0; i < n; ++i) {
		const float64 d = fabs((float64)a[i] - (float64)b[i]);
		if (d > maxAbs)
			maxAbs = d;
		const float64 m = fabs((float64)b[i]);
		if (m > scale)
			scale = m;
	}
	maxRel = (scale > 0.0) ? (maxAbs / scale) : maxAbs;
}

// -----------------------------------------------------------------------------
// (a) Déquantification GPU seule == NkGGUFDequantizeRaw, élément par élément.
// -----------------------------------------------------------------------------
static void TestDequantExact(int64 rows, int64 cols, uint64 seed) {
	printf("\n-- (a) dequant GPU vs CPU : [%lld, %lld] (%lld blocs) --\n", (long long)rows, (long long)cols,
		   (long long)((uint64)rows * (uint64)cols / kNkQ4KBlockElems));

	NkVector<uint8> raw;
	BuildSyntheticQ4K(rows, cols, seed, /*stress*/ true, raw);

	NkVector<float32> cpu;
	NkString err;
	const bool cpuOk = NkGGUFDequantizeRaw((uint32)NkGGUFTensorType::NK_GGML_Q4_K, raw.Data(), (uint64)raw.Size(),
										   (uint64)(rows * cols), cpu, &err);
	check(cpuOk, "référence CPU NkGGUFDequantizeRaw(Q4_K) sur les blocs synthétiques");
	if (!cpuOk) {
		printf("     %s\n", err.CStr());
		return;
	}

	NkQ4KGpuWeight w;
	const bool up = NkQ4KGpuUpload(raw.Data(), (uint64)raw.Size(), rows, cols, w, &err);
	check(up, "upload des blocs Q4_K BRUTS sur GPU (aucune déquantification à l'upload)");
	if (!up) {
		printf("     %s\n", err.CStr());
		return;
	}

	NkVector<float32> gpu;
	const bool dq = NkQ4KGpuDequantize(w, gpu, &err);
	check(dq && gpu.Size() == cpu.Size(), "noyau NkSL q4k_dequant : dispatch + readback");
	if (!dq) {
		printf("     %s\n", err.CStr());
		NkQ4KGpuRelease(w);
		return;
	}

	// Comparaison BIT À BIT : les deux côtés font exactement les mêmes opérations
	// sur les mêmes entiers, donc tout écart non nul est un bug, pas du bruit.
	uint64 diffCount = 0;
	float64 maxAbs = 0.0;
	int64 firstBad = -1;
	for (NkVector<float32>::SizeType i = 0; i < cpu.Size(); ++i) {
		if (gpu[i] != cpu[i]) {
			++diffCount;
			if (firstBad < 0)
				firstBad = (int64)i;
			const float64 d = fabs((float64)gpu[i] - (float64)cpu[i]);
			if (d > maxAbs)
				maxAbs = d;
		}
	}
	printf("     éléments = %llu, différents = %llu, |Δ| max = %.3e\n", (unsigned long long)cpu.Size(),
		   (unsigned long long)diffCount, maxAbs);
	if (firstBad >= 0)
		printf("     premier écart à l'index %lld : gpu=%.9g cpu=%.9g\n", (long long)firstBad,
			   (double)gpu[(NkVector<float32>::SizeType)firstBad], (double)cpu[(NkVector<float32>::SizeType)firstBad]);
	check(diffCount == 0, "déquantification GPU IDENTIQUE au CPU (|Δ| == 0 sur tous les éléments)");

	NkQ4KGpuRelease(w);
}

// -----------------------------------------------------------------------------
// (b) + (c) matmul fusé sur dimensions réelles de projection 7B + débit.
// -----------------------------------------------------------------------------
static void TestMatmulDims(int64 M, int64 K, int64 N, uint64 seed, const char *label) {
	printf("\n-- (b) matmul_q4k : M=%lld K=%lld N=%lld  (%s) --\n", (long long)M, (long long)K, (long long)N, label);

	NkVector<uint8> raw;
	BuildSyntheticQ4K(N, K, seed, /*stress*/ false, raw);
	printf("     poids Q4_K = %.2f Mo (f32 équivalent : %.2f Mo)\n", (double)raw.Size() / 1048576.0,
		   (double)((uint64)N * (uint64)K * 4ull) / 1048576.0);

	NkVector<float32> x;
	x.Resize((NkVector<float32>::SizeType)(M * K));
	NkLoraRng rng(seed ^ 0xABCDEFull);
	for (NkVector<float32>::SizeType i = 0; i < x.Size(); ++i)
		x[i] = rng.NextGaussian() * 0.05f;

	NkString err;
	NkQ4KGpuWeight w;
	if (!NkQ4KGpuUpload(raw.Data(), (uint64)raw.Size(), N, K, w, &err)) {
		check(false, "upload du poids Q4_K sur GPU");
		printf("     %s\n", err.CStr());
		return;
	}

	// --- GPU : matmul FUSÉ (le poids reste quantifié en VRAM) ---------------
	// Passe de CHAUFFE non chronométrée : le TOUT PREMIER dispatch d'un noyau
	// paye la chaîne NkSL -> GLSL-Vulkan -> SPIR-V -> pipeline (~20 ms, mise en
	// cache par nom dans NkTensorGpu). L'inclure ferait passer un coût de
	// compilation unique pour un coût de calcul.
	NkVector<float32> ygpu;
	bool gOk = NkQ4KGpuMatmulCpu(w, x.Data(), M, ygpu, &err);
	NkChrono chrono;
	if (gOk)
		gOk = NkQ4KGpuMatmulCpu(w, x.Data(), M, ygpu, &err);
	const float64 gpuMs = chrono.Elapsed().ToMilliseconds();
	if (!gOk) {
		check(false, "matmul_q4k GPU");
		printf("     %s\n", err.CStr());
		NkQ4KGpuRelease(w);
		return;
	}

	// --- Référence CPU : déquantification COMPLÈTE puis matmul f32 ----------
	NkVector<float32> wd;
	if (!NkGGUFDequantizeRaw((uint32)NkGGUFTensorType::NK_GGML_Q4_K, raw.Data(), (uint64)raw.Size(), (uint64)(N * K),
							 wd, &err)) {
		check(false, "référence CPU : déquantification complète du poids");
		printf("     %s\n", err.CStr());
		NkQ4KGpuRelease(w);
		return;
	}
	NkChrono chronoCpu;
	NkVector<float32> ycpu;
	CpuMatmulNT(x.Data(), M, K, wd.Data(), N, ycpu);
	const float64 cpuMs = chronoCpu.Elapsed().ToMilliseconds();

	float64 maxAbs = 0.0, maxRel = 0.0;
	Compare(ygpu, ycpu, maxAbs, maxRel);

	// (c) Débit : chaque thread relit la ligne de W dont il a besoin, donc M
	// passes sur le poids quantifié. GFLOPS comptés en 2·M·N·K (mult + add).
	const float64 bytesRead = (float64)raw.Size() * (float64)M;
	const float64 gbps = (gpuMs > 0.0) ? (bytesRead / (gpuMs * 1e-3) / 1e9) : 0.0;
	const float64 gflops = (gpuMs > 0.0) ? (2.0 * (float64)M * (float64)N * (float64)K / (gpuMs * 1e-3) / 1e9) : 0.0;
	printf("     GPU %.2f ms  |  CPU réf %.2f ms  |  %.2f Go/s lus  |  %.2f GFLOPS\n", gpuMs, cpuMs, gbps, gflops);
	printf("     |Δ| max = %.3e, erreur relative = %.3e (seuil 1e-4)\n", maxAbs, maxRel);
	check(ygpu.Size() == ycpu.Size() && maxRel <= 1e-4, "matmul_q4k GPU == dequant CPU + matmul CPU f32 (rel <= 1e-4)");

	NkQ4KGpuRelease(w);
}

// -----------------------------------------------------------------------------
// (d) Vrai tenseur Q4_K du blob Qwen2.5.
// -----------------------------------------------------------------------------
static void TestRealTensor(const char *path) {
	printf("\n-- (d) tenseur Q4_K RÉEL du GGUF --\n     %s\n", path);

	NkGGUFFile file;
	if (!NkGGUFLoader::Load(path, file)) {
		check(false, "chargement du GGUF");
		printf("     %s\n", file.error.CStr());
		return;
	}
	printf("     GGUF v%u, %llu tenseurs\n", file.version, (unsigned long long)file.tensorCount);

	// On cherche une projection de la COUCHE 0 réellement stockée en Q4_K (dans
	// un Q4_K_M, certains tenseurs sont en Q6_K : on ne suppose rien, on lit le
	// type déclaré). `dims` est dans l'ordre ggml : ne[0] = in (K), ne[1] = out (N).
	const NkGGUFTensorInfo *chosen = nullptr;
	for (NkVector<NkGGUFTensorInfo>::SizeType i = 0; i < file.tensors.Size(); ++i) {
		const NkGGUFTensorInfo &t = file.tensors[i];
		if (t.rawType != (uint32)NkGGUFTensorType::NK_GGML_Q4_K || t.dims.Size() != 2 || !t.sizeKnown)
			continue;
		if (t.name.Find("blk.0.") == NkString::npos)
			continue;
		if (t.dims[0] % kNkQ4KBlockElems != 0)
			continue;
		chosen = &t;
		break;
	}
	if (!chosen) {
		check(false, "trouver un tenseur Q4_K 2D dans la couche 0");
		return;
	}
	const int64 K = (int64)chosen->dims[0];
	const int64 N = (int64)chosen->dims[1];
	printf("     tenseur : %s  [K=%lld, N=%lld]  %.2f Mo bruts\n", chosen->name.CStr(), (long long)K, (long long)N,
		   (double)chosen->sizeBytes / 1048576.0);

	NkVector<uint8> raw;
	NkString err;
	if (!NkGGUFReadTensorRawBytes(path, file, *chosen, raw, &err)) {
		check(false, "lecture des octets bruts du tenseur");
		printf("     %s\n", err.CStr());
		return;
	}
	check(true, "lecture des octets BRUTS d'un vrai tenseur Q4_K (couche 0)");

	NkQ4KGpuWeight w;
	if (!NkQ4KGpuUpload(raw.Data(), (uint64)raw.Size(), N, K, w, &err)) {
		check(false, "upload du vrai tenseur Q4_K sur GPU");
		printf("     %s\n", err.CStr());
		return;
	}
	check(true, "upload BRUT du vrai tenseur (aucune déquantification à l'upload)");

	const int64 M = 4;
	NkVector<float32> x;
	x.Resize((NkVector<float32>::SizeType)(M * K));
	NkLoraRng rng(20260805ull);
	for (NkVector<float32>::SizeType i = 0; i < x.Size(); ++i)
		x[i] = rng.NextGaussian() * 0.05f;

	NkVector<float32> ygpu;
	bool gOk = NkQ4KGpuMatmulCpu(w, x.Data(), M, ygpu, &err); // chauffe (cf (b))
	NkChrono chrono;
	if (gOk)
		gOk = NkQ4KGpuMatmulCpu(w, x.Data(), M, ygpu, &err);
	const float64 gpuMs = chrono.Elapsed().ToMilliseconds();
	if (!gOk) {
		check(false, "matmul_q4k GPU sur le vrai tenseur");
		printf("     %s\n", err.CStr());
		NkQ4KGpuRelease(w);
		return;
	}

	NkVector<float32> wd;
	if (!NkGGUFDequantizeRaw(chosen->rawType, raw.Data(), (uint64)raw.Size(), (uint64)(N * K), wd, &err)) {
		check(false, "référence CPU : déquantification complète du vrai tenseur");
		printf("     %s\n", err.CStr());
		NkQ4KGpuRelease(w);
		return;
	}
	NkVector<float32> ycpu;
	CpuMatmulNT(x.Data(), M, K, wd.Data(), N, ycpu);

	float64 maxAbs = 0.0, maxRel = 0.0;
	Compare(ygpu, ycpu, maxAbs, maxRel);
	const float64 gbps = (gpuMs > 0.0) ? ((float64)raw.Size() * (float64)M / (gpuMs * 1e-3) / 1e9) : 0.0;
	const float64 gflops = (gpuMs > 0.0) ? (2.0 * (float64)M * (float64)N * (float64)K / (gpuMs * 1e-3) / 1e9) : 0.0;
	printf("     GPU %.2f ms  |  %.2f Go/s lus  |  %.2f GFLOPS\n", gpuMs, gbps, gflops);
	printf("     |Δ| max = %.3e, erreur relative = %.3e (seuil 1e-4)\n", maxAbs, maxRel);
	check(maxRel <= 1e-4, "vrai tenseur Q4_K : matmul GPU == référence CPU (rel <= 1e-4)");

	NkQ4KGpuRelease(w);
}

// =============================================================================
// (e) Q6_K — bloc de 210 octets, NON divisible par 4.
// =============================================================================

// (e1) Déquantification GPU seule == NkGGUFDequantizeRaw(Q6_K), bit à bit.
static void TestQ6KDequantExact(int64 rows, int64 cols, uint64 seed) {
	printf("\n-- (e1) dequant Q6_K GPU vs CPU : [%lld, %lld] (%lld blocs de 210 o) --\n", (long long)rows,
		   (long long)cols, (long long)((uint64)rows * (uint64)cols / kNkQ6KBlockElems));

	NkVector<uint8> raw;
	BuildSyntheticQ6K(rows, cols, seed, /*stress*/ true, raw);

	NkVector<float32> cpu;
	NkString err;
	const bool cpuOk = NkGGUFDequantizeRaw((uint32)NkGGUFTensorType::NK_GGML_Q6_K, raw.Data(), (uint64)raw.Size(),
										   (uint64)(rows * cols), cpu, &err);
	check(cpuOk, "référence CPU NkGGUFDequantizeRaw(Q6_K) sur les blocs synthétiques");
	if (!cpuOk) {
		printf("     %s\n", err.CStr());
		return;
	}

	NkQ6KGpuWeight w;
	const bool up = NkQ6KGpuUpload(raw.Data(), (uint64)raw.Size(), rows, cols, w, &err);
	check(up, "upload des blocs Q6_K BRUTS sur GPU");
	if (!up) {
		printf("     %s\n", err.CStr());
		return;
	}

	NkVector<float32> gpu;
	const bool dq = NkQ6KGpuDequantize(w, gpu, &err);
	check(dq && gpu.Size() == cpu.Size(), "noyau NkSL q6k_dequant : dispatch + readback");
	if (!dq) {
		printf("     %s\n", err.CStr());
		NkQ6KGpuRelease(w);
		return;
	}

	uint64 diffCount = 0;
	float64 maxAbs = 0.0;
	int64 firstBad = -1;
	for (NkVector<float32>::SizeType i = 0; i < cpu.Size(); ++i) {
		if (gpu[i] != cpu[i]) {
			++diffCount;
			if (firstBad < 0)
				firstBad = (int64)i;
			const float64 d = fabs((float64)gpu[i] - (float64)cpu[i]);
			if (d > maxAbs)
				maxAbs = d;
		}
	}
	printf("     éléments = %llu, différents = %llu, |Δ| max = %.3e\n", (unsigned long long)cpu.Size(),
		   (unsigned long long)diffCount, maxAbs);
	if (firstBad >= 0)
		printf("     premier écart à l'index %lld : gpu=%.9g cpu=%.9g\n", (long long)firstBad,
			   (double)gpu[(NkVector<float32>::SizeType)firstBad], (double)cpu[(NkVector<float32>::SizeType)firstBad]);
	check(diffCount == 0, "déquantification Q6_K GPU IDENTIQUE au CPU (|Δ| == 0 sur tous les éléments)");

	NkQ6KGpuRelease(w);
}

// (e2) Matmul Q6_K fusé vs (dequant CPU + matmul CPU f32).
static void TestQ6KMatmulDims(int64 M, int64 K, int64 N, uint64 seed, const char *label) {
	printf("\n-- (e2) matmul_q6k : M=%lld K=%lld N=%lld  (%s) --\n", (long long)M, (long long)K, (long long)N, label);

	NkVector<uint8> raw;
	BuildSyntheticQ6K(N, K, seed, /*stress*/ false, raw);
	printf("     poids Q6_K = %.2f Mo (f32 équivalent : %.2f Mo)\n", (double)raw.Size() / 1048576.0,
		   (double)((uint64)N * (uint64)K * 4ull) / 1048576.0);

	NkVector<float32> x;
	x.Resize((NkVector<float32>::SizeType)(M * K));
	NkLoraRng rng(seed ^ 0x123456ull);
	for (NkVector<float32>::SizeType i = 0; i < x.Size(); ++i)
		x[i] = rng.NextGaussian() * 0.05f;

	NkString err;
	NkQ6KGpuWeight w;
	if (!NkQ6KGpuUpload(raw.Data(), (uint64)raw.Size(), N, K, w, &err)) {
		check(false, "upload du poids Q6_K sur GPU");
		printf("     %s\n", err.CStr());
		return;
	}

	NkVector<float32> ygpu;
	bool gOk = NkQ6KGpuMatmulCpu(w, x.Data(), M, ygpu, &err); // chauffe (compilation du pipeline)
	NkChrono chrono;
	if (gOk)
		gOk = NkQ6KGpuMatmulCpu(w, x.Data(), M, ygpu, &err);
	const float64 gpuMs = chrono.Elapsed().ToMilliseconds();
	if (!gOk) {
		check(false, "matmul_q6k GPU");
		printf("     %s\n", err.CStr());
		NkQ6KGpuRelease(w);
		return;
	}

	NkVector<float32> wd;
	if (!NkGGUFDequantizeRaw((uint32)NkGGUFTensorType::NK_GGML_Q6_K, raw.Data(), (uint64)raw.Size(), (uint64)(N * K),
							 wd, &err)) {
		check(false, "référence CPU : déquantification complète du poids Q6_K");
		printf("     %s\n", err.CStr());
		NkQ6KGpuRelease(w);
		return;
	}
	NkVector<float32> ycpu;
	CpuMatmulNT(x.Data(), M, K, wd.Data(), N, ycpu);

	float64 maxAbs = 0.0, maxRel = 0.0;
	Compare(ygpu, ycpu, maxAbs, maxRel);
	const float64 gbps = (gpuMs > 0.0) ? ((float64)raw.Size() * (float64)M / (gpuMs * 1e-3) / 1e9) : 0.0;
	const float64 gflops = (gpuMs > 0.0) ? (2.0 * (float64)M * (float64)N * (float64)K / (gpuMs * 1e-3) / 1e9) : 0.0;
	printf("     GPU %.2f ms  |  %.2f Go/s lus  |  %.2f GFLOPS\n", gpuMs, gbps, gflops);
	printf("     |Δ| max = %.3e, erreur relative = %.3e (seuil 1e-4)\n", maxAbs, maxRel);
	check(ygpu.Size() == ycpu.Size() && maxRel <= 1e-4, "matmul_q6k GPU == dequant CPU + matmul CPU f32 (rel <= 1e-4)");

	NkQ6KGpuRelease(w);
}

// (e3) VRAI tenseur Q6_K du blob. Dans un Q4_K_M, c'est typiquement ffn_down.
//
// La preuve BIT À BIT se fait sur une TRANCHE de lignes et non sur le tenseur
// entier : les lignes d'un poids GGUF sont contiguës (la ligne n occupe bpr
// super-blocs consécutifs), donc les R premières lignes forment un tenseur Q6_K
// [R, K] parfaitement valide, avec les VRAIS octets du modèle. Déquantifier les
// 68 millions d'éléments du tenseur complet coûterait 271 Mo de VRAM, 271 Mo de
// transfert et 271 Mo côté CPU pour ne rien prouver de plus : le décodage d'un
// bloc ne dépend d'aucun autre. Le MATMUL, lui, tourne sur le tenseur ENTIER.
static void TestQ6KRealTensor(const char *path) {
	printf("\n-- (e3) tenseur Q6_K RÉEL du GGUF --\n     %s\n", path);

	NkGGUFFile file;
	if (!NkGGUFLoader::Load(path, file)) {
		check(false, "chargement du GGUF (Q6_K)");
		printf("     %s\n", file.error.CStr());
		return;
	}

	const NkGGUFTensorInfo *chosen = nullptr;
	for (NkVector<NkGGUFTensorInfo>::SizeType i = 0; i < file.tensors.Size(); ++i) {
		const NkGGUFTensorInfo &t = file.tensors[i];
		if (t.rawType != (uint32)NkGGUFTensorType::NK_GGML_Q6_K || t.dims.Size() != 2 || !t.sizeKnown)
			continue;
		if (t.dims[0] % kNkQ6KBlockElems != 0)
			continue;
		chosen = &t;
		break;
	}
	if (!chosen) {
		check(false, "trouver un tenseur Q6_K 2D dans le GGUF");
		return;
	}
	const int64 K = (int64)chosen->dims[0];
	const int64 N = (int64)chosen->dims[1];
	printf("     tenseur : %s  [K=%lld, N=%lld]  %.2f Mo bruts\n", chosen->name.CStr(), (long long)K, (long long)N,
		   (double)chosen->sizeBytes / 1048576.0);
	check(true, "un tenseur Q6_K existe bien dans ce Q4_K_M (c'est pourquoi Q6_K est un prérequis)");

	NkVector<uint8> raw;
	NkString err;
	if (!NkGGUFReadTensorRawBytes(path, file, *chosen, raw, &err)) {
		check(false, "lecture des octets bruts du tenseur Q6_K");
		printf("     %s\n", err.CStr());
		return;
	}

	// --- Preuve BIT À BIT sur une tranche de lignes -------------------------
	const int64 bpr = K / (int64)kNkQ6KBlockElems;
	int64 sliceRows = 512;
	if (sliceRows > N)
		sliceRows = N;
	const uint64 sliceBytes = (uint64)sliceRows * (uint64)bpr * kNkQ6KBlockBytes;

	NkQ6KGpuWeight ws;
	if (!NkQ6KGpuUpload(raw.Data(), sliceBytes, sliceRows, K, ws, &err)) {
		check(false, "upload de la tranche Q6_K réelle");
		printf("     %s\n", err.CStr());
		return;
	}
	NkVector<float32> cpuSlice, gpuSlice;
	bool ok = NkGGUFDequantizeRaw(chosen->rawType, raw.Data(), sliceBytes, (uint64)(sliceRows * K), cpuSlice, &err);
	if (ok)
		ok = NkQ6KGpuDequantize(ws, gpuSlice, &err);
	if (!ok) {
		check(false, "déquantification de la tranche réelle (CPU et GPU)");
		printf("     %s\n", err.CStr());
		NkQ6KGpuRelease(ws);
		return;
	}
	uint64 diffCount = 0;
	int64 firstBad = -1;
	for (NkVector<float32>::SizeType i = 0; i < cpuSlice.Size(); ++i) {
		if (gpuSlice[i] != cpuSlice[i]) {
			++diffCount;
			if (firstBad < 0)
				firstBad = (int64)i;
		}
	}
	printf("     tranche : %lld lignes, %llu éléments, différents = %llu\n", (long long)sliceRows,
		   (unsigned long long)cpuSlice.Size(), (unsigned long long)diffCount);
	if (firstBad >= 0)
		printf("     premier écart à l'index %lld : gpu=%.9g cpu=%.9g\n", (long long)firstBad,
			   (double)gpuSlice[(NkVector<float32>::SizeType)firstBad],
			   (double)cpuSlice[(NkVector<float32>::SizeType)firstBad]);
	check(diffCount == 0, "VRAI tenseur Q6_K : dequant GPU IDENTIQUE au CPU (|Δ| == 0)");
	NkQ6KGpuRelease(ws);
	cpuSlice.Clear();
	gpuSlice.Clear();

	// --- Matmul sur le tenseur ENTIER ---------------------------------------
	NkQ6KGpuWeight w;
	if (!NkQ6KGpuUpload(raw.Data(), (uint64)raw.Size(), N, K, w, &err)) {
		check(false, "upload du vrai tenseur Q6_K complet sur GPU");
		printf("     %s\n", err.CStr());
		return;
	}
	const int64 M = 4;
	NkVector<float32> x;
	x.Resize((NkVector<float32>::SizeType)(M * K));
	NkLoraRng rng(20260805ull);
	for (NkVector<float32>::SizeType i = 0; i < x.Size(); ++i)
		x[i] = rng.NextGaussian() * 0.05f;

	NkVector<float32> ygpu;
	bool gOk = NkQ6KGpuMatmulCpu(w, x.Data(), M, ygpu, &err); // chauffe
	NkChrono chrono;
	if (gOk)
		gOk = NkQ6KGpuMatmulCpu(w, x.Data(), M, ygpu, &err);
	const float64 gpuMs = chrono.Elapsed().ToMilliseconds();
	if (!gOk) {
		check(false, "matmul_q6k GPU sur le vrai tenseur");
		printf("     %s\n", err.CStr());
		NkQ6KGpuRelease(w);
		return;
	}

	NkVector<float32> wd;
	if (!NkGGUFDequantizeRaw(chosen->rawType, raw.Data(), (uint64)raw.Size(), (uint64)(N * K), wd, &err)) {
		check(false, "référence CPU : déquantification complète du vrai tenseur Q6_K");
		printf("     %s\n", err.CStr());
		NkQ6KGpuRelease(w);
		return;
	}
	NkVector<float32> ycpu;
	CpuMatmulNT(x.Data(), M, K, wd.Data(), N, ycpu);

	float64 maxAbs = 0.0, maxRel = 0.0;
	Compare(ygpu, ycpu, maxAbs, maxRel);
	const float64 gbps = (gpuMs > 0.0) ? ((float64)raw.Size() * (float64)M / (gpuMs * 1e-3) / 1e9) : 0.0;
	const float64 gflops = (gpuMs > 0.0) ? (2.0 * (float64)M * (float64)N * (float64)K / (gpuMs * 1e-3) / 1e9) : 0.0;
	printf("     GPU %.2f ms  |  %.2f Go/s lus  |  %.2f GFLOPS\n", gpuMs, gbps, gflops);
	printf("     |Δ| max = %.3e, erreur relative = %.3e (seuil 1e-4)\n", maxAbs, maxRel);
	check(maxRel <= 1e-4, "vrai tenseur Q6_K : matmul GPU == référence CPU (rel <= 1e-4)");

	NkQ6KGpuRelease(w);
}

// =============================================================================
// (f) GEMM : noyau SIMPLE vs noyau TUILÉ, mesuré à M = 1, 8, 64, 256.
//
// POURQUOI CETTE MESURE EST OBLIGATOIRE ET NON DÉCORATIVE
// -------------------------------------------------------
// Le tuilage est une HYPOTHÈSE : « relire W une fois par tuile au lieu d'une
// fois par token doit payer dès que M grandit ». Une hypothèse sur le
// comportement mémoire d'un GPU ne se démontre pas au tableau — elle se mesure,
// sur cette machine, avec ce poids. Sans les deux colonnes côte à côte, on ne
// saurait ni si le tuilage a servi, ni à partir de quel M basculer.
//
// CE QUI EST CHRONOMÉTRÉ : le DISPATCH SEUL. X est uploadé une fois avant la
// mesure et Y n'est pas relu : à M = 256, X pèse 3,7 Mo et Y 3,7 Mo — les
// inclure ferait passer un coût de transfert pour un coût de calcul, exactement
// l'erreur que la passe de chauffe évite déjà pour la compilation du pipeline.
//
// LES DEUX Go/s NE SE COMPARENT PAS DIRECTEMENT : chaque noyau lit un VOLUME
// différent de W (M passes pour le simple, ceil(M/8) pour le tuilé). Le débit
// affiché est celui que le noyau soutient RÉELLEMENT sur son propre trafic ; la
// grandeur comparable entre les deux est le GFLOPS, qui rapporte le même travail
// utile (2·M·N·K) au même temps.
// =============================================================================
static void BenchGemm(int64 K, int64 N, uint64 seed) {
	printf("\n=====================================================================\n");
	printf("(f) GEMM Q4_K : noyau SIMPLE vs TUILÉ — K=%lld, N=%lld\n", (long long)K, (long long)N);
	printf("=====================================================================\n");

	NkVector<uint8> raw;
	BuildSyntheticQ4K(N, K, seed, /*stress*/ false, raw);
	const float64 wBytes = (float64)raw.Size();
	printf("poids Q4_K : %.2f Mo\n", wBytes / 1048576.0);

	NkString err;
	NkQ4KGpuWeight w;
	if (!NkQ4KGpuUpload(raw.Data(), (uint64)raw.Size(), N, K, w, &err)) {
		check(false, "(f) upload du poids pour la mesure GEMM");
		printf("     %s\n", err.CStr());
		return;
	}

	NkTensorGpu &gpu = NkTensorGpu::Get();
	const int64 kMs[4] = {1, 8, 64, 256};

	printf("\n  %5s | %-7s | %9s | %9s | %9s | %8s\n", "M", "noyau", "ms", "GFLOPS", "Go/s W", "gain");
	printf("  ------+---------+-----------+-----------+-----------+---------\n");

	bool allMatch = true;
	bool tiledWinsAt8 = false;
	for (int mi = 0; mi < 4; ++mi) {
		const int64 M = kMs[mi];
		const uint64 xn = (uint64)M * (uint64)K;
		const uint64 yn = (uint64)M * (uint64)N;

		NkVector<float32> x;
		x.Resize((NkVector<float32>::SizeType)xn);
		NkLoraRng rng(seed ^ (uint64)M);
		for (NkVector<float32>::SizeType i = 0; i < x.Size(); ++i)
			x[i] = rng.NextGaussian() * 0.05f;

		uint64 xbuf = gpu.CreateBuffer((nk_size)(xn * sizeof(float32)));
		uint64 ybuf = gpu.CreateBuffer((nk_size)(yn * sizeof(float32)));
		if (xbuf == 0 || ybuf == 0 || !gpu.Upload(xbuf, x.Data(), (nk_size)(xn * sizeof(float32)))) {
			check(false, "(f) tampons GPU pour la mesure");
			if (xbuf)
				gpu.DestroyBuffer(xbuf);
			if (ybuf)
				gpu.DestroyBuffer(ybuf);
			continue;
		}

		// BEAUCOUP de répétitions à petit M, PEU à grand M — et pour deux raisons
		// opposées. À M = 1 le dispatch entier dure moins d'une milliseconde et le
		// coût FIXE (création du command buffer, allocation du descriptor set,
		// WaitIdle) pèse autant que le calcul : sans moyenne sur 20 passes, on
		// mesure surtout le bruit du pilote. À M = 256 une seule passe du noyau
		// simple coûte déjà 110 ms : répéter n'apprend rien et allonge le test.
		const int reps = (M == 1) ? 20 : ((M <= 8) ? 10 : 2);
		float64 ms[2] = {0.0, 0.0};
		NkVector<float32> yOut[2];
		bool okBoth = true;

		for (int ki = 0; ki < 2; ++ki) {
			const NkQ4KMatmulKernel kern =
				(ki == 0) ? NkQ4KMatmulKernel::NK_SIMPLE : NkQ4KMatmulKernel::NK_TILED;
			if (!NkQ4KGpuMatmulEx(w, xbuf, ybuf, M, kern, &err)) { // chauffe
				printf("     échec (%s) : %s\n", ki == 0 ? "simple" : "tuilé", err.CStr());
				okBoth = false;
				break;
			}
			NkChrono chrono;
			for (int r = 0; r < reps; ++r)
				NkQ4KGpuMatmulEx(w, xbuf, ybuf, M, kern, &err);
			// BARRIÈRE DE FIN, DANS LE CHRONO. `WaitIdle()` n'est pas une vraie
			// clôture sur tous les backends : sur DX11 le contexte immédiat rend la
			// main avant que le GPU ait fini, et la première mesure a affiché
			// 35 000 GFLOPS pour une carte qui en fait 20 000 au maximum théorique —
			// on chronométrait la SOUMISSION, pas le calcul. Relire ne serait-ce
			// qu'un float force la synchronisation réelle ; 4 octets ne polluent
			// pas la mesure alors qu'un readback complet (3,7 Mo à M = 256) la
			// fausserait dans l'autre sens.
			float32 fence = 0.0f;
			gpu.Download(ybuf, &fence, sizeof(float32));
			ms[ki] = chrono.Elapsed().ToMilliseconds() / (float64)reps;
			(void)fence;

			yOut[ki].Resize((NkVector<float32>::SizeType)yn);
			gpu.Download(ybuf, yOut[ki].Data(), (nk_size)(yn * sizeof(float32)));
		}

		if (okBoth) {
			for (int ki = 0; ki < 2; ++ki) {
				const float64 gflops = 2.0 * (float64)M * (float64)N * (float64)K / (ms[ki] * 1e-3) / 1e9;
				// Trafic W propre à chaque noyau : le simple relit la ligne pour
				// CHAQUE token, le tuilé une fois par tuile de 8 tokens.
				const float64 passes = (ki == 0) ? (float64)M : (float64)((M + 7) / 8);
				const float64 gbps = wBytes * passes / (ms[ki] * 1e-3) / 1e9;
				if (ki == 0)
					printf("  %5lld | %-7s | %9.3f | %9.2f | %9.2f | %8s\n", (long long)M, "simple", ms[0], gflops,
						   gbps, "-");
				else
					printf("  %5lld | %-7s | %9.3f | %9.2f | %9.2f | %7.2fx\n", (long long)M, "tuilé", ms[1], gflops,
						   gbps, ms[0] / ms[1]);
			}
			// Les deux noyaux somment dans le MÊME ordre (k croissant) sur les
			// MÊMES valeurs déquantifiées : ils doivent donner le même résultat.
			// Un écart signalerait une course sur la mémoire partagée ou une
			// barrière manquante — le genre de bug qui ne se voit pas au débit.
			float64 mAbs = 0.0, mRel = 0.0;
			Compare(yOut[1], yOut[0], mAbs, mRel);
			if (mRel > 1e-6) {
				allMatch = false;
				printf("        !! tuilé != simple : |Δ| max = %.3e, rel = %.3e\n", mAbs, mRel);
			}
			// L'assertion porte sur M = 256, PAS sur M = 1 ou 8. À petit M, les
			// deux noyaux tiennent dans ~1 ms dont l'essentiel est le coût fixe
			// d'un dispatch : d'une exécution à l'autre le vainqueur change, et
			// affirmer quoi que ce soit là-dessus serait affirmer du bruit. Le
			// tuilage vise le régime où W est relu des dizaines de fois — c'est
			// là, et seulement là, qu'il doit prouver quelque chose.
			if (M == 256 && ms[1] < ms[0] * 0.5)
				tiledWinsAt8 = true;
		}

		gpu.DestroyBuffer(xbuf);
		gpu.DestroyBuffer(ybuf);
	}

	printf("  (à M <= 8, les deux noyaux tiennent dans ~1 ms dont l'essentiel est le coût FIXE d'un\n");
	printf("   dispatch — command buffer, descriptor set, WaitIdle : la comparaison n'y est pas\n");
	printf("   concluante et le vainqueur change d'une exécution à l'autre. Le seuil NK_AUTO est\n");
	printf("   donc placé à M = 8, là où le tuilage commence à être mesurablement devant.)\n");
	check(allMatch, "(f) noyau tuilé == noyau simple sur les 4 tailles (rel <= 1e-6)");
	check(tiledWinsAt8, "(f) le tuilage divise le temps par plus de 2 à M = 256 (régime LoRA)");
	NkQ4KGpuRelease(w);
}

// Même mesure pour Q6_K. Elle n'est pas décorative : sur un GGUF Q4_K_M, le
// tenseur promu en Q6_K est `ffn_down`, l'une des sept projections que
// l'entraînement LoRA traverse à chaque pas. Un tuilage qui ne couvrirait que
// Q4_K laisserait ce chemin-là au régime lent.
static void BenchGemmQ6K(int64 K, int64 N, uint64 seed) {
	printf("\n=====================================================================\n");
	printf("(f2) GEMM Q6_K : noyau SIMPLE vs TUILÉ — K=%lld, N=%lld\n", (long long)K, (long long)N);
	printf("=====================================================================\n");

	NkVector<uint8> raw;
	BuildSyntheticQ6K(N, K, seed, /*stress*/ false, raw);
	const float64 wBytes = (float64)raw.Size();
	printf("poids Q6_K : %.2f Mo\n", wBytes / 1048576.0);

	NkString err;
	NkQ6KGpuWeight w;
	if (!NkQ6KGpuUpload(raw.Data(), (uint64)raw.Size(), N, K, w, &err)) {
		check(false, "(f2) upload du poids Q6_K pour la mesure GEMM");
		printf("     %s\n", err.CStr());
		return;
	}

	NkTensorGpu &gpu = NkTensorGpu::Get();
	const int64 kMs[4] = {1, 8, 64, 256};

	printf("\n  %5s | %-7s | %9s | %9s | %9s | %8s\n", "M", "noyau", "ms", "GFLOPS", "Go/s W", "gain");
	printf("  ------+---------+-----------+-----------+-----------+---------\n");

	bool allMatch = true;
	bool tiledWins = false;
	for (int mi = 0; mi < 4; ++mi) {
		const int64 M = kMs[mi];
		const uint64 xn = (uint64)M * (uint64)K;
		const uint64 yn = (uint64)M * (uint64)N;

		NkVector<float32> x;
		x.Resize((NkVector<float32>::SizeType)xn);
		NkLoraRng rng(seed ^ (uint64)M);
		for (NkVector<float32>::SizeType i = 0; i < x.Size(); ++i)
			x[i] = rng.NextGaussian() * 0.05f;

		uint64 xbuf = gpu.CreateBuffer((nk_size)(xn * sizeof(float32)));
		uint64 ybuf = gpu.CreateBuffer((nk_size)(yn * sizeof(float32)));
		if (xbuf == 0 || ybuf == 0 || !gpu.Upload(xbuf, x.Data(), (nk_size)(xn * sizeof(float32)))) {
			check(false, "(f2) tampons GPU pour la mesure");
			if (xbuf)
				gpu.DestroyBuffer(xbuf);
			if (ybuf)
				gpu.DestroyBuffer(ybuf);
			continue;
		}

		const int reps = (M == 1) ? 20 : ((M <= 8) ? 10 : 2);
		float64 ms[2] = {0.0, 0.0};
		NkVector<float32> yOut[2];
		bool okBoth = true;

		for (int ki = 0; ki < 2; ++ki) {
			const NkQ6KMatmulKernel kern =
				(ki == 0) ? NkQ6KMatmulKernel::NK_SIMPLE : NkQ6KMatmulKernel::NK_TILED;
			if (!NkQ6KGpuMatmulEx(w, xbuf, ybuf, M, kern, &err)) { // chauffe
				printf("     échec (%s) : %s\n", ki == 0 ? "simple" : "tuilé", err.CStr());
				okBoth = false;
				break;
			}
			NkChrono chrono;
			for (int r = 0; r < reps; ++r)
				NkQ6KGpuMatmulEx(w, xbuf, ybuf, M, kern, &err);
			float32 fence = 0.0f; // cf. BenchGemm : WaitIdle ne clôture pas sur DX11
			gpu.Download(ybuf, &fence, sizeof(float32));
			ms[ki] = chrono.Elapsed().ToMilliseconds() / (float64)reps;
			(void)fence;

			yOut[ki].Resize((NkVector<float32>::SizeType)yn);
			gpu.Download(ybuf, yOut[ki].Data(), (nk_size)(yn * sizeof(float32)));
		}

		if (okBoth) {
			for (int ki = 0; ki < 2; ++ki) {
				const float64 gflops = 2.0 * (float64)M * (float64)N * (float64)K / (ms[ki] * 1e-3) / 1e9;
				const float64 passes = (ki == 0) ? (float64)M : (float64)((M + 7) / 8);
				const float64 gbps = wBytes * passes / (ms[ki] * 1e-3) / 1e9;
				if (ki == 0)
					printf("  %5lld | %-7s | %9.3f | %9.2f | %9.2f | %8s\n", (long long)M, "simple", ms[0], gflops,
						   gbps, "-");
				else
					printf("  %5lld | %-7s | %9.3f | %9.2f | %9.2f | %7.2fx\n", (long long)M, "tuilé", ms[1], gflops,
						   gbps, ms[0] / ms[1]);
			}
			float64 mAbs = 0.0, mRel = 0.0;
			Compare(yOut[1], yOut[0], mAbs, mRel);
			if (mRel > 1e-6) {
				allMatch = false;
				printf("        !! tuilé != simple : |Δ| max = %.3e, rel = %.3e\n", mAbs, mRel);
			}
			if (M == 256 && ms[1] < ms[0] * 0.5)
				tiledWins = true;
		}

		gpu.DestroyBuffer(xbuf);
		gpu.DestroyBuffer(ybuf);
	}

	check(allMatch, "(f2) noyau Q6_K tuilé == noyau simple sur les 4 tailles (rel <= 1e-6)");
	check(tiledWins, "(f2) le tuilage Q6_K divise le temps par plus de 2 à M = 256");
	NkQ6KGpuRelease(w);
}

int main(int argc, char **argv) {
	printf("=== NKQ4MatmulTest — jalon 4/5 QLoRA : K-quants résidents GPU + matmul fusé ===\n");

	NkTensorGpu &gpu = NkTensorGpu::Get();
	printf("GPU compute disponible : %d  (backend : %s)\n", (int)gpu.IsAvailable(), gpu.BackendName());
	if (!gpu.IsAvailable()) {
		printf("Aucun device compute -> ce jalon est intrinsèquement GPU, test ignoré.\n");
		return 0;
	}

	// (a) exactitude du décodage de bloc, deux tailles.
	TestDequantExact(5, 512, 12345ull);
	TestDequantExact(37, 768, 998877ull);

	// (b) + (c) dimensions réelles d'une projection Qwen2.5 7B (d=3584,
	// ffn=18944) avec M = 4 tokens : c'est le régime de l'inférence et de
	// l'entraînement LoRA, pas celui d'un GEMM carré.
	TestMatmulDims(4, 3584, 3584, 424242ull, "attn_q / attn_output");
	TestMatmulDims(4, 3584, 18944, 777777ull, "ffn_gate / ffn_up");

	// (e) Q6_K : bloc de 210 octets, non divisible par 4 — le VRAI travail neuf.
	TestQ6KDequantExact(5, 512, 246810ull);
	TestQ6KDequantExact(37, 768, 135791ull);
	TestQ6KMatmulDims(4, 3584, 3584, 314159ull, "projection carrée 7B");

	// (d) + (e3) blob réel, si fourni.
	if (argc > 1) {
		TestRealTensor(argv[1]);
		TestQ6KRealTensor(argv[1]);
	} else {
		printf("\n-- (d)/(e3) ignorés : aucun chemin GGUF passé en argument --\n");
	}

	// (f) mesure GEMM avant/après tuilage, sur une projection carrée réelle.
	BenchGemm(3584, 3584, 909090ull);
	BenchGemmQ6K(3584, 3584, 707070ull);

	const int total = g_ok + g_fail;
	printf("\n=== Résultat : %d/%d OK ===\n", g_ok, total);
	gpu.Shutdown();
	return g_fail;
}
