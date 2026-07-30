// =============================================================================
// NKGGUFInspectTest — jalon testable du loader GGUF (NKInfer, Phase 7).
//
// Ouvre un VRAI blob de modèle Ollama (format GGUF, llama.cpp/Ollama) et affiche
// son architecture réelle (métadonnées + liste des tenseurs) SANS jamais charger
// les données tensor (qui pèsent plusieurs Go) — seul le header et les tables de
// métadonnées/tensor_info sont lus, plus une validation arithmétique que chaque
// tenseur "tient" dans la taille réelle du fichier.
//
// Usage : NKGGUFInspectTest.exe [chemin_gguf_ou_blob_ollama]
//         (à défaut, variable d'env NK_GGUF_PATH, puis un blob Ollama connu)
// =============================================================================
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkGGUFDequant.h"
#include "NKTensor/NkFp16.h"

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
// Statistiques simples sur un buffer f32 déquantifié : sert à vérifier qu'un
// tenseur déquantifié est "sain" (pas de NaN/Inf, ordre de grandeur plausible
// pour des poids de réseau de neurones — typiquement centrés près de 0, faible
// écart-type) sans connaître les valeurs exactes attendues (on n'a pas de
// référence PyTorch/llama.cpp sous la main dans ce dépôt).
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

// =============================================================================
// Tests synthétiques de déquantification (ne dépendent d'AUCUN fichier GGUF
// réel) : valident l'algorithme de NkGGUFDequant lui-même contre des blocs
// bruts construits/encodés à la main selon la spec ggml (cf NkGGUFDequant.h
// pour les sources exactes), avec des valeurs attendues calculées
// indépendamment (à la main, pas en réutilisant le code de production).
// =============================================================================

// ---- Q8_0 : round-trip complet (quantifie NOUS-MÊMES puis déquantifie via
// NkGGUFDequant) : erreur doit rester <= un demi pas de quantification. ----
static void TestQ8_0RoundTrip() {
	printf("-- Test synthétique Q8_0 (round-trip quantif -> déquantif) --\n");
	constexpr int kN = 32;
	float vals[kN];
	for (int i = 0; i < kN; ++i)
		vals[i] = 0.37f * std::sin(0.6f * (float)i) - 0.05f * (float)(i % 5);

	float amax = 0.0f;
	for (int i = 0; i < kN; ++i)
		amax = std::fmax(amax, std::fabs(vals[i]));
	const float scale = amax / 127.0f;

	uint8 raw[34];
	uint16 dBits = ai::NkF32ToF16Bits(scale);
	std::memcpy(raw, &dBits, 2);
	for (int i = 0; i < kN; ++i) {
		int32 q = (int32)std::lround(vals[i] / scale);
		if (q > 127)
			q = 127;
		if (q < -128)
			q = -128;
		raw[2 + i] = (uint8)(int8)q;
	}

	NkVector<float32> out;
	NkString err;
	bool ok = ai::infer::NkGGUFDequantizeRaw((uint32)ai::infer::NkGGUFTensorType::NK_GGML_Q8_0, raw, sizeof(raw), kN,
											 out, &err);
	Check(ok, "NkGGUFDequantizeRaw(Q8_0) réussi sur bloc synthétique");
	if (!ok) {
		printf("  Erreur : %s\n", err.CStr());
		return;
	}

	// Le d rematérialisé (f16->f32) diffère légèrement de `scale` (arrondi f16) :
	// on relit le d effectivement utilisé pour le calcul de tolérance.
	const float dUsed = ai::NkF16BitsToF32(dBits);
	float maxErr = 0.0f;
	for (int i = 0; i < kN; ++i)
		maxErr = std::fmax(maxErr, std::fabs(out[(NkVector<float32>::SizeType)i] - vals[i]));
	printf("  scale=%.6g (f16: %.6g), erreur max round-trip = %.6g (tolérance = %.6g)\n", scale, dUsed, maxErr,
		   dUsed * 0.5f + 1e-6f);
	Check(maxErr <= dUsed * 0.5f + 1e-6f, "Q8_0 round-trip : erreur <= demi-pas de quantification");
	printf("\n");
}

// ---- Q4_K : bloc à valeurs connues, encodage 6 bits fait À LA MAIN (dérivé
// manuellement des formules de get_scale_min_k4, PAS en réutilisant le code de
// production), pour vérifier que le désempaquetage est bit-exact. ----
static void TestQ4_KKnownValues() {
	printf("-- Test synthétique Q4_K (bloc à valeurs connues, encodage manuel) --\n");
	uint8 raw[144];
	std::memset(raw, 0, sizeof(raw));

	// d=1.0, dmin=0.0 (mins désactivés pour ce test : on isole les échelles).
	uint16 dBits = ai::NkF32ToF16Bits(1.0f);
	uint16 dminBits = ai::NkF32ToF16Bits(0.0f);
	std::memcpy(raw + 0, &dBits, 2);
	std::memcpy(raw + 2, &dminBits, 2);
	uint8 *scales = raw + 4; // scales[12]
	uint8 *qs = raw + 4 + 12; // qs[128]

	// Sous-bloc j=0 (chemin "j<4" de get_scale_min_k4) : sc=10, m=3 (m sans
	// effet ici car dmin=0). q[0] = sc&63 ; q[4] = m&63.
	scales[0] = 10 & 63;
	scales[4] = 3 & 63;
	// Sous-bloc j=4 (chemin "j>=4", le plus piégeux : bits hauts empruntés à
	// q[0]/q[4]) : on VEUT sc=45, m=20. Dérivation manuelle (cf commentaire
	// NkGGUFDequant.cpp / GetScaleMinK4) :
	//   d4 = (q[8] & 0xF) | ((q[0] >> 6) << 4)  -> q[8] bas nibble = 45 & 0xF = 13 (0xD)
	//                                              q[0] bits hauts = 45 >> 4 = 2
	//   m4 = (q[8] >> 4)  | ((q[4] >> 6) << 4)  -> q[8] haut nibble = 20 & 0xF = 4
	//                                              q[4] bits hauts = 20 >> 4 = 1
	scales[0] = (uint8)((2 << 6) | (10 & 63)); // q[0] : hauts=2 (pour d4), bas=10 (d0)
	scales[4] = (uint8)((1 << 6) | (3 & 63));	// q[4] : hauts=1 (pour m4), bas=3  (m0)
	scales[8] = (uint8)(((20 & 0xF) << 4) | (45 & 0xF)); // q[8] : haut=m4 bas nibble, bas=d4 bas nibble

	// qs : la boucle de dequantize_row_q4_K traite 4 tranches de 64 éléments de
	// sortie, chacune consommant 32 octets de `qs` (2 nibbles/octet) et DEUX
	// indices de sous-bloc (is, is+1) pour (nibble bas, nibble haut) :
	//   tranche 0 : qs[ 0..31] -> is=0,1 -> y[  0..63]
	//   tranche 1 : qs[32..63] -> is=2,3 -> y[ 64..127]
	//   tranche 2 : qs[64..95] -> is=4,5 -> y[128..191]
	//   tranche 3 : qs[96..127]-> is=6,7 -> y[192..255]
	// Sous-bloc j=0 -> tranche 0, nibble bas ; sous-bloc j=4 -> tranche 2,
	// nibble bas (qs[64..95]). Valeur nibble = 5 pour tous, calcul simple.
	for (int i = 0; i < 32; ++i)
		qs[i] = 5; // tranche 0, nibbles bas -> sous-bloc j=0 -> y[0..31]
	for (int i = 0; i < 32; ++i)
		qs[64 + i] = 5; // tranche 2, nibbles bas -> sous-bloc j=4 -> y[128..159]

	NkVector<float32> out;
	NkString err;
	bool ok = ai::infer::NkGGUFDequantizeRaw((uint32)ai::infer::NkGGUFTensorType::NK_GGML_Q4_K, raw, sizeof(raw), 256,
											 out, &err);
	Check(ok, "NkGGUFDequantizeRaw(Q4_K) réussi sur bloc synthétique");
	if (!ok) {
		printf("  Erreur : %s\n", err.CStr());
		return;
	}

	// Attendu : élément 0 (sous-bloc j=0, d1=1*10=10, m1=0) = 10*(5&0xF)-0 = 50.
	// Élément 128 (tranche 2, nibble bas, sous-bloc j=4, d1=1*45=45) =
	// 45*(5&0xF)-0 = 225.
	const float e0 = out[0];
	const float e128 = out[128];
	printf("  élément[0]   = %.6g (attendu 50)\n", e0);
	printf("  élément[128] = %.6g (attendu 225)\n", e128);
	Check(std::fabs(e0 - 50.0f) < 1e-3f, "Q4_K sous-bloc j=0 (chemin j<4) : valeur exacte");
	Check(std::fabs(e128 - 225.0f) < 1e-3f, "Q4_K sous-bloc j=4 (chemin j>=4, bits empruntés) : valeur exacte");
	printf("\n");
}

// ---- Q6_K : un seul quadruplet (l=0, n=0) à valeurs connues, calculé à la
// main indépendamment (cf commentaire) pour vérifier le désempaquetage 6 bits
// signé (4 bits bas ql + 2 bits hauts qh) et le double niveau d'échelle. ----
static void TestQ6_KKnownValues() {
	printf("-- Test synthétique Q6_K (bloc à valeurs connues, encodage manuel) --\n");
	uint8 raw[210];
	std::memset(raw, 0, sizeof(raw));
	uint8 *ql = raw;		  // ql[128]
	uint8 *qh = raw + 128;	  // qh[64]
	int8 *scales = (int8 *)(raw + 128 + 64); // scales[16]
	uint8 *dPtr = raw + 128 + 64 + 16;		  // d (uint16 en fin de bloc)

	// ql[0] : nibble bas=12 (0xC) -> q1 avant offset ; nibble haut=3 -> q3 avant offset.
	ql[0] = (uint8)((3 << 4) | 12);
	// ql[32] : nibble bas=5 -> q2 ; nibble haut=2 -> q4.
	ql[32] = (uint8)((2 << 4) | 5);
	// qh[0] : paires de 2 bits (bit1:0=0 pour q1, bit3:2=3 pour q2, bit5:4=2 pour q3, bit7:6=1 pour q4).
	qh[0] = (uint8)((1 << 6) | (2 << 4) | (3 << 2) | 0);
	// Échelles utilisées pour l=0 (is=0) : sc[0], sc[2], sc[4], sc[6].
	scales[0] = 2;
	scales[2] = -3;
	scales[4] = 5;
	scales[6] = -1;
	// d = 0.5.
	uint16 dBits = ai::NkF32ToF16Bits(0.5f);
	std::memcpy(dPtr, &dBits, 2);

	NkVector<float32> out;
	NkString err;
	bool ok = ai::infer::NkGGUFDequantizeRaw((uint32)ai::infer::NkGGUFTensorType::NK_GGML_Q6_K, raw, sizeof(raw), 256,
											 out, &err);
	Check(ok, "NkGGUFDequantizeRaw(Q6_K) réussi sur bloc synthétique");
	if (!ok) {
		printf("  Erreur : %s\n", err.CStr());
		return;
	}

	// Calcul manuel indépendant (cf. dérivation dans NkGGUFDequant.h) :
	//  q1=(12|0<<4)-32=-20 -> y[0]  = 0.5*2*(-20)  = -20.0
	//  q2=(5|3<<4)-32=21   -> y[32] = 0.5*(-3)*21  = -31.5
	//  q3=(3|2<<4)-32=3    -> y[64] = 0.5*5*3      =   7.5
	//  q4=(2|1<<4)-32=-14  -> y[96] = 0.5*(-1)*(-14)=  7.0
	const float e0 = out[0], e32 = out[32], e64 = out[64], e96 = out[96];
	printf("  y[0]=%.6g y[32]=%.6g y[64]=%.6g y[96]=%.6g (attendus -20, -31.5, 7.5, 7)\n", e0, e32, e64, e96);
	Check(std::fabs(e0 - (-20.0f)) < 1e-3f, "Q6_K y[0]  == -20.0");
	Check(std::fabs(e32 - (-31.5f)) < 1e-3f, "Q6_K y[32] == -31.5");
	Check(std::fabs(e64 - 7.5f) < 1e-3f, "Q6_K y[64]  ==   7.5");
	Check(std::fabs(e96 - 7.0f) < 1e-3f, "Q6_K y[96]  ==   7.0");
	printf("\n");
}

static void PrintMetaString(const infer::NkGGUFFile &gguf, const char *key) {
	NkString v;
	if (infer::NkGGUFGetString(gguf, key, v))
		printf("  %-32s = %s\n", key, v.CStr());
}

static void PrintMetaUInt(const infer::NkGGUFFile &gguf, const char *key) {
	uint64 v = 0;
	if (infer::NkGGUFGetUInt(gguf, key, v))
		printf("  %-32s = %llu\n", key, (unsigned long long)v);
}

static void PrintMetaFloat(const infer::NkGGUFFile &gguf, const char *key) {
	float64 v = 0;
	if (infer::NkGGUFGetFloat(gguf, key, v))
		printf("  %-32s = %g\n", key, v);
}

int main(int argc, char **argv) {
	printf("=== NKGGUFInspectTest : parseur GGUF (NKInfer, Phase 7) ===\n\n");

	// -------------------------------------------------------------------
	// Tests synthétiques de NkGGUFDequant (Jalon 3) : ne dépendent d'aucun
	// fichier, valident l'algorithme de déquantification lui-même.
	// -------------------------------------------------------------------
	TestQ8_0RoundTrip();
	TestQ4_KKnownValues();
	TestQ6_KKnownValues();

	const char *path = nullptr;
	if (argc > 1) {
		path = argv[1];
	} else if (const char *envPath = getenv("NK_GGUF_PATH")) {
		path = envPath;
	} else {
		// Blob Ollama réel utilisé pour valider ce loader (Qwen2.5 7B Instruct,
		// vérifié magic "GGUF" via xxd avant l'implémentation). Chemin local à la
		// machine de développement : passer un chemin en argv[1] ou NK_GGUF_PATH
		// ailleurs.
		path = "C:/Users/Rihen/.ollama/models/blobs/"
			   "sha256-2bada8a7450677000f678be90653b85d364de7db25eb5ea54136ada5f3933730";
	}
	printf("Fichier : %s\n\n", path);

	infer::NkGGUFFile gguf;
	bool loaded = infer::NkGGUFLoader::Load(path, gguf);
	Check(loaded && gguf.valid, "NkGGUFLoader::Load réussi (magic/version/metadata/tensor_info parsés)");
	if (!loaded || !gguf.valid) {
		printf("  Erreur : %s\n", gguf.error.CStr());
		printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
		return 1;
	}

	// -------------------------------------------------------------------
	// En-tête.
	// -------------------------------------------------------------------
	printf("-- En-tête --\n");
	printf("  version              = %u\n", gguf.version);
	printf("  tensor_count         = %llu\n", (unsigned long long)gguf.tensorCount);
	printf("  metadata_kv_count    = %llu\n", (unsigned long long)gguf.metadataCount);
	printf("  alignment            = %llu\n", (unsigned long long)gguf.alignment);
	printf("  tensorDataOffset     = %llu (octets)\n", (unsigned long long)gguf.tensorDataOffset);
	printf("  fileSize             = %llu octets (%.2f Go)\n", (unsigned long long)gguf.fileSize,
		   (double)gguf.fileSize / (1024.0 * 1024.0 * 1024.0));
	printf("\n");

	Check(gguf.version == 2 || gguf.version == 3, "version GGUF == 2 ou 3");
	Check(gguf.tensorCount > 0, "tensor_count > 0");
	Check(gguf.metadataCount > 0, "metadata_kv_count > 0");
	Check(gguf.metadata.Size() == gguf.metadataCount, "nombre de métadonnées effectivement lues == metadata_kv_count");
	Check(gguf.tensors.Size() == gguf.tensorCount, "nombre de tensor_info effectivement lus == tensor_count");
	Check(gguf.tensorDataOffset > 0 && gguf.tensorDataOffset <= gguf.fileSize,
		  "tensorDataOffset cohérent (dans les bornes du fichier)");

	// -------------------------------------------------------------------
	// Métadonnées clés (nom d'architecture, tailles du modèle, tokenizer...).
	// -------------------------------------------------------------------
	printf("-- Métadonnées clés --\n");
	NkString arch;
	bool hasArch = infer::NkGGUFGetString(gguf, "general.architecture", arch);
	Check(hasArch, "general.architecture présent (identifie la famille du modèle)");

	PrintMetaString(gguf, "general.architecture");
	PrintMetaString(gguf, "general.name");
	PrintMetaString(gguf, "general.type");
	PrintMetaString(gguf, "general.basename");
	PrintMetaString(gguf, "general.size_label");
	PrintMetaString(gguf, "general.license");
	PrintMetaUInt(gguf, "general.quantization_version");
	PrintMetaUInt(gguf, "general.file_type");

	if (hasArch) {
		NkString key;
#define NK_ARCH_KEY(suffix) (key = arch, key.Append(suffix), key.CStr())
		PrintMetaUInt(gguf, NK_ARCH_KEY(".block_count"));
		PrintMetaUInt(gguf, NK_ARCH_KEY(".context_length"));
		PrintMetaUInt(gguf, NK_ARCH_KEY(".embedding_length"));
		PrintMetaUInt(gguf, NK_ARCH_KEY(".feed_forward_length"));
		PrintMetaUInt(gguf, NK_ARCH_KEY(".attention.head_count"));
		PrintMetaUInt(gguf, NK_ARCH_KEY(".attention.head_count_kv"));
		PrintMetaFloat(gguf, NK_ARCH_KEY(".attention.layer_norm_rms_epsilon"));
		PrintMetaFloat(gguf, NK_ARCH_KEY(".rope.freq_base"));
#undef NK_ARCH_KEY
	}

	PrintMetaString(gguf, "tokenizer.ggml.model");
	uint64 vocabSize = infer::NkGGUFArrayLength(gguf, "tokenizer.ggml.tokens");
	if (vocabSize > 0)
		printf("  %-32s = %llu (longueur du tableau tokenizer.ggml.tokens)\n", "vocab_size", (unsigned long long)vocabSize);
	Check(vocabSize > 1000, "vocab_size (tokenizer.ggml.tokens) plausible (> 1000 tokens)");

	// Aperçu des premiers tokens du vocabulaire (si présents) — preuve que le
	// tableau de métadonnées STRING est bien parcouru, pas juste sauté.
	const infer::NkGGUFMetadataKV *tokKv = infer::NkGGUFFindMeta(gguf, "tokenizer.ggml.tokens");
	if (tokKv && tokKv->preview.Size() > 0) {
		printf("  Premiers tokens du vocabulaire : ");
		for (uint32 i = 0; i < tokKv->preview.Size(); ++i) {
			if (tokKv->preview[i].type == infer::NkGGUFValueType::NK_GGUF_STRING)
				printf("\"%s\" ", tokKv->preview[i].str.CStr());
		}
		printf("\n");
	}
	printf("\n");

	// -------------------------------------------------------------------
	// Tenseurs : quelques exemples + validation globale de taille.
	// -------------------------------------------------------------------
	printf("-- Tenseurs (%u premiers sur %llu) --\n", (uint32)(gguf.tensors.Size() < 12 ? gguf.tensors.Size() : 12u),
		   (unsigned long long)gguf.tensorCount);
	uint64 totalKnownBytes = 0;
	uint32 nSizeKnown = 0, nSizeUnknown = 0;
	for (uint32 i = 0; i < gguf.tensors.Size(); ++i) {
		const infer::NkGGUFTensorInfo &t = gguf.tensors[i];
		if (t.sizeKnown) {
			totalKnownBytes += t.sizeBytes;
			nSizeKnown++;
		} else {
			nSizeUnknown++;
		}
		if (i < 12) {
			printf("  [%3u] %-40s dims=(", i, t.name.CStr());
			for (uint32 d = 0; d < t.dims.Size(); ++d)
				printf("%s%llu", d == 0 ? "" : ",", (unsigned long long)t.dims[d]);
			printf(") type=%-8s offset=%llu\n", infer::NkGGUFTensorTypeName(t.rawType), (unsigned long long)t.offset);
			if (t.sizeKnown)
				printf("        -> %llu octets attendus (%.2f Ko)\n", (unsigned long long)t.sizeBytes,
					   (double)t.sizeBytes / 1024.0);
			else
				printf("        -> taille non validée (type de quantification non tabulé dans ce loader)\n");
		}
	}
	printf("\n");
	printf("  Types de quantification reconnus (taille validée) : %u/%u tenseurs (%u non tabulés)\n", nSizeKnown,
		   (uint32)gguf.tensors.Size(), nSizeUnknown);
	printf("  Somme des tailles attendues (types connus)         : %llu octets (%.2f Go)\n",
		   (unsigned long long)totalKnownBytes, (double)totalKnownBytes / (1024.0 * 1024.0 * 1024.0));
	printf("\n");

	Check(nSizeKnown > 0, "au moins un tenseur avec un type de quantification tabulé (taille validée)");
	Check(gguf.tensors[0].name.Length() > 0, "le premier tenseur a un nom non vide");
	Check(gguf.tensors[0].dims.Size() > 0, "le premier tenseur a au moins une dimension");

	// La zone de données tensor (offset + somme des tailles connues) ne doit
	// jamais dépasser la taille réelle du fichier — déjà vérifié par le loader
	// lui-même (Load() aurait renvoyé false sinon), on le re-vérifie ici pour
	// que l'échec soit visible dans CE test si jamais la logique divergeait.
	Check(gguf.tensorDataOffset + totalKnownBytes <= gguf.fileSize,
		  "zone de données tensor (offset + tailles connues) tient dans le fichier réel");

	// -------------------------------------------------------------------
	// Déquantification réelle sur le VRAI blob (NkGGUFDequant, Jalon 3) :
	// pour chaque type de quantification supporté rencontré dans ce fichier,
	// déquantifie un tenseur réel et vérifie que le résultat est sain (pas de
	// NaN/Inf, statistiques plausibles pour des poids de réseau de neurones).
	// -------------------------------------------------------------------
	printf("-- Déquantification réelle (NkGGUFDequant, sur les VRAIS tenseurs du blob) --\n");
	struct DequantPick {
			uint32 rawType;
			int32 idx;
	};
	DequantPick picks[] = {
		{(uint32)infer::NkGGUFTensorType::NK_GGML_Q4_K, -1},
		{(uint32)infer::NkGGUFTensorType::NK_GGML_Q6_K, -1},
		{(uint32)infer::NkGGUFTensorType::NK_GGML_Q8_0, -1},
		{(uint32)infer::NkGGUFTensorType::NK_GGML_F16, -1},
		{(uint32)infer::NkGGUFTensorType::NK_GGML_F32, -1},
	};
	for (uint32 i = 0; i < gguf.tensors.Size(); ++i) {
		const infer::NkGGUFTensorInfo &t = gguf.tensors[i];
		if (!t.sizeKnown)
			continue;
		for (auto &p : picks) {
			if (p.idx < 0 && t.rawType == p.rawType)
				p.idx = (int32)i;
		}
	}

	int32 nDequantOk = 0;
	for (auto &p : picks) {
		if (p.idx < 0) {
			printf("  (aucun tenseur %s trouvé dans ce blob -- ignoré)\n", infer::NkGGUFTensorTypeName(p.rawType));
			continue;
		}
		const infer::NkGGUFTensorInfo &t = gguf.tensors[(uint32)p.idx];
		printf("  [%s] tenseur '%s' (%llu éléments, %llu octets bruts)\n", infer::NkGGUFTensorTypeName(t.rawType),
			   t.name.CStr(), (unsigned long long)t.numElements, (unsigned long long)t.sizeBytes);

		NkTensor outTensor;
		NkString err;
		bool ok = infer::NkGGUFDequantizeTensor(path, gguf, t, outTensor, &err);
		NkString checkName("NkGGUFDequantizeTensor réussi pour ");
		checkName.Append(infer::NkGGUFTensorTypeName(t.rawType));
		Check(ok, checkName.CStr());
		if (!ok) {
			printf("    Erreur : %s\n", err.CStr());
			continue;
		}

		TensorStats st = ComputeStats(outTensor.DataAs<float>(), (uint64)outTensor.Numel());
		printf("    stats : n=%llu mean=%.6g stddev=%.6g min=%.6g max=%.6g NaN=%llu Inf=%llu\n",
			   (unsigned long long)st.n, st.mean, st.stddev, st.minVal, st.maxVal,
			   (unsigned long long)st.nanCount, (unsigned long long)st.infCount);

		NkString noNanName(infer::NkGGUFTensorTypeName(t.rawType));
		noNanName.Append(" : aucun NaN/Inf après déquantification");
		Check(st.nanCount == 0 && st.infCount == 0, noNanName.CStr());

		// Plage plausible pour des poids de réseau de neurones pré-entraîné :
		// typiquement centrés près de 0, écart-type petit (souvent < 1).
		// Marge large (10) pour ne pas être fragile sur des tenseurs atypiques
		// (ex: biais, embeddings) tout en détectant un bug de déquantification
		// grossier (valeurs de l'ordre de 1e3-1e10 typiques d'un mauvais décalage
		// de bits ou d'une échelle non appliquée).
		NkString plausibleName(infer::NkGGUFTensorTypeName(t.rawType));
		plausibleName.Append(" : moyenne/écart-type dans une plage plausible (poids de NN)");
		Check(std::fabs(st.mean) < 10.0 && st.stddev < 10.0 && st.stddev >= 0.0, plausibleName.CStr());

		nDequantOk++;
	}
	printf("\n");
	Check(nDequantOk >= 2, "au moins 2 types de quantification réellement déquantifiés sur le vrai blob Qwen2.5");

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", g_pass, g_fail);
	return g_fail == 0 ? 0 : 1;
}
