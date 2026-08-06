// =============================================================================
// NkQ6KGpu.h — poids Q6_K RÉSIDENTS sur GPU + matmul fusionné déquant-produit.
// Jalon 5 du chantier QLoRA — PRÉREQUIS DUR : sans lui le forward 7B ne peut
// même pas CHARGER le modèle.
//
// POURQUOI Q6_K EST OBLIGATOIRE
// -----------------------------
// Dans un GGUF « Q4_K_M », tout n'est pas en Q4_K : llama.cpp promeut en Q6_K
// les tenseurs auxquels la sortie est le plus sensible. Sur le blob Qwen2.5-7B
// de référence, `blk.*.ffn_down.weight` est en Q6_K (55,7 Mo par couche) —
// 28 tenseurs sur 339. Un chemin GPU qui ne sait lire que Q4_K s'arrête donc au
// deuxième tenseur du modèle. Ce n'est pas une optimisation : c'est la
// condition d'existence du jalon suivant.
//
// CE QUI DIFFÈRE DE Q4_K, ET POURQUOI C'EST LE VRAI TRAVAIL
// ---------------------------------------------------------
// Un super-bloc Q4_K fait 144 octets = 36 mots de 32 bits PILE. Le shader peut
// donc lire le tampon brut comme un `uint[]` et dire « le bloc b commence au
// mot b*36 ». Un super-bloc Q6_K fait **210 octets — non divisible par 4** :
//   * le bloc b commence au demi-mot, au trois-quart de mot, etc., selon b
//     (210 mod 4 = 2, donc le décalage cycle sur 0, 2, 0, 2… en octets) ;
//   * un champ de 2 octets (la super-échelle fp16, aux octets 208-209 du bloc)
//     est à CHEVAL sur deux `uint` un bloc sur deux.
// Il n'existe pas de SSBO d'octets sans extension (`uint8_t` demande
// VK_KHR_8bit_storage, absent d'OpenGL et non garanti en HLSL SM5). Toute
// lecture passe donc par un accès OCTET PAR OCTET calculé en absolu :
//     mot = offset >> 2 ; octet = (mot >> ((offset & 3) * 8)) & 0xFF
// C'est correct parce que le GPU lit ses mots en LITTLE-ENDIAN, comme le x86
// qui a écrit le fichier GGUF. Sur une machine gros-boutiste, cette hypothèse
// tomberait — elle est vraie de toutes les plateformes visées (x86-64, ARM en
// mode little-endian), mais elle est ÉCRITE ici parce qu'elle est invisible
// dans le code.
//
// Disposition d'un bloc (ggml-common.h, block_q6_K, 210 octets) :
//   [  0..127] ql[128]     : 4 bits BAS de chaque quant (2 quants par octet)
//   [128..191] qh[64]      : 2 bits HAUTS, 4 quants par octet
//   [192..207] scales[16]  : une échelle int8 SIGNÉE par sous-bloc de 16
//   [208..209] d           : super-échelle fp16 du super-bloc
//
// CONVENTION DE FORME : identique à NkQ4KGpu (`rows` = N = out_features,
// `cols` = K = in_features, ligne contiguë), donc Y = X · Wᵀ sans transposée.
//
// Zéro STL.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Constantes du format (spec ggml : QK_K = 256,
			// sizeof(block_q6_K) = 128 + 64 + 16 + 2 = 210 octets).
			constexpr uint64 kNkQ6KBlockElems = 256;
			constexpr uint64 kNkQ6KBlockBytes = 210;

			// Octets EXACTS attendus pour un poids [rows, cols] en Q6_K, ou 0 si
			// `cols` n'est pas un multiple de 256.
			uint64 NkQ6KExpectedBytes(int64 rows, int64 cols);

			// -----------------------------------------------------------------
			// Poids Q6_K résident GPU : super-blocs BRUTS, jamais déquantifiés en
			// VRAM. Le rapport est moins spectaculaire qu'en Q4_K (6,5 bits par
			// poids au lieu de 4,5) mais reste 4,9× plus léger que du f32.
			// -----------------------------------------------------------------
			struct NkQ6KGpuWeight {
					uint64 buffer = 0;		// id opaque NkTensorGpu (0 = invalide)
					int64 rows = 0;			// N = out_features
					int64 cols = 0;			// K = in_features (multiple de 256)
					int64 blocksPerRow = 0; // cols / 256
					uint64 byteCount = 0;	// octets réellement uploadés

					bool IsValid() const {
						return buffer != 0 && rows > 0 && cols > 0;
					}

					int64 Numel() const {
						return rows * cols;
					}
			};

			// Upload des blocs bruts. `rawByteCount` doit valoir EXACTEMENT
			// NkQ6KExpectedBytes(rows, cols).
			bool NkQ6KGpuUpload(const void *rawBlocks, uint64 rawByteCount, int64 rows, int64 cols,
								NkQ6KGpuWeight &out, NkString *outError = nullptr);

			// Libère le tampon GPU et invalide la structure (idempotent).
			void NkQ6KGpuRelease(NkQ6KGpuWeight &w);

			// ---- Noyau 1 : déquantification seule (validation / diagnostic) ---
			bool NkQ6KGpuDequantizeTo(const NkQ6KGpuWeight &w, uint64 outBuffer, NkString *outError = nullptr);

			// Variante confort : alloue, dispatch, relit sur CPU. La sortie f32
			// pèse 4,9× le poids quantifié : à réserver aux petits tenseurs.
			bool NkQ6KGpuDequantize(const NkQ6KGpuWeight &w, NkVector<float32> &out, NkString *outError = nullptr);

			// -----------------------------------------------------------------
			// Choix du noyau de matmul — même dilemme et mêmes conclusions qu'en
			// Q4_K (cf. NkQ4KGpu.h pour le raisonnement complet).
			//
			// Q6_K MÉRITE LE TUILAGE AUTANT QUE Q4_K, et pour une raison concrète :
			// sur un Q4_K_M, le tenseur promu en Q6_K est `ffn_down` — une des sept
			// projections que l'entraînement LoRA traverse à chaque pas, sur des
			// séquences entières. Laisser ce seul chemin en noyau simple aurait
			// annulé une partie du gain obtenu sur les six autres.
			// -----------------------------------------------------------------
			enum class NkQ6KMatmulKernel {
				NK_AUTO = 0,
				NK_SIMPLE = 1,
				NK_TILED = 2,
			};

			constexpr int64 kNkQ6KTiledMinM = 8;

			// ---- Noyau 2 : matmul FUSÉ déquantification-produit ---------------
			// Y[M,N] = X[M,K] · W[N,K]ᵀ, X et Y en f32, W jamais déquantifié.
			bool NkQ6KGpuMatmul(const NkQ6KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M,
								NkString *outError = nullptr);

			// Même chose en FORÇANT un noyau précis (mesure comparative).
			bool NkQ6KGpuMatmulEx(const NkQ6KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M,
								  NkQ6KMatmulKernel kernel, NkString *outError = nullptr);

			// Variante confort : upload de X, dispatch, readback de Y.
			bool NkQ6KGpuMatmulCpu(const NkQ6KGpuWeight &w, const float32 *x, int64 M, NkVector<float32> &outY,
								   NkString *outError = nullptr);

			bool NkQ6KGpuMatmulCpuEx(const NkQ6KGpuWeight &w, const float32 *x, int64 M, NkQ6KMatmulKernel kernel,
									 NkVector<float32> &outY, NkString *outError = nullptr);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
