// =============================================================================
// NkQ4KGpu.h — poids Q4_K RÉSIDENTS sur GPU + matmul fusionné déquant-produit.
// Jalon 4 du chantier QLoRA (Documentation/notes_etape4_qlora.md §2.2, §4).
//
// POURQUOI CE MODULE EXISTE
// -------------------------
// Aujourd'hui le seul chemin disponible est « Q4_K du fichier -> f32 complet »
// (NkGGUFDequant) : 26 Go pour le 7B, 932 Mo par couche. Tenable en streaming
// d'inférence, INTENABLE pour l'entraînement LoRA (chaque pas retouche chaque
// couche). On garde donc les super-blocs Q4_K TELS QUELS en VRAM (4,4 Go pour
// tout le modèle) et on déquantifie À LA VOLÉE, dans le noyau, au fil de la
// lecture des blocs : la VRAM ne voit jamais la version f32.
//
// POURQUOI DANS NKInfer ET PAS DANS NKTensor
// ------------------------------------------
// 1. Le format de bloc Q4_K est un concept ggml/GGUF, et la VÉRITÉ de référence
//    (NkGGUFDequant.cpp, transcrite de ggml-quants.c) vit déjà ici. Mettre le
//    layout de bloc dans NKTensor le dupliquerait, avec le risque classique de
//    deux transcriptions qui divergent.
// 2. NKTensor ne connaît AUCUN dtype quantifié par blocs (NkDType = F32/F64/
//    I32/I64/U8/F16) : y ajouter Q4_K obligerait à modifier NkDType, NkTensor,
//    NkTensorOps et le macro de dispatch — soit exactement les fichiers
//    existants que ce jalon s'interdit de toucher, pour un gain nul ici.
// 3. Ce n'est PAS un contournement d'infrastructure : NkTensorGpu expose déjà
//    publiquement TOUT ce qu'il faut — CreateBuffer/Upload/Download pour les
//    tampons, et deux lanceurs de noyaux NkSL génériques,
//      * RunConvOp(nom, src, in, out, p12, count)  : 2 storage buffers + UBO
//        12 uints au binding 2, dispatch 1D local_size_x=64 ;
//      * RunOp3(nom, src, a, b, c, p12, count)     : 3 storage buffers + UBO
//        12 uints au binding 3, même dispatch.
//    Les noyaux ci-dessous respectent EXACTEMENT ces conventions de bindings,
//    et le type déclaré côté shader (uint[] au lieu de float[]) est libre : le
//    descripteur est un NK_STORAGE_BUFFER non typé. Aucune ligne de NKTensor
//    n'est modifiée, le cache de pipelines et la sélection de backend sont ceux
//    de NkTensorGpu.
//
// CONVENTION DE FORME (piège n°3 des notes : dims ggml inversées)
// ---------------------------------------------------------------
// Un poids de projection GGUF a ne[0] = in_features (dimension CONTIGUË) et
// ne[1] = out_features ; NkGGUFDequantizeTensor l'expose donc en [out, in].
// On garde RIGOUREUSEMENT la même convention : `rows` = N = out_features,
// `cols` = K = in_features, et la ligne n occupe `cols/256` super-blocs
// CONSÉCUTIFS en mémoire. Le produit calculé est donc
//     Y[M,N] = X[M,K] · W[N,K]ᵀ,   soit Y[m,n] = Σ_k X[m,k]·W[n,k]
// c'est-à-dire « X × W_q4k[K,N] » au sens mathématique de l'énoncé, mais SANS
// jamais matérialiser de transposée (piège n°4) : chaque thread lit une ligne
// de W parfaitement contiguë, exactement comme NkLinearNoBias côté CPU.
//
// Zéro STL. Formats : Q4_K ici, Q6_K dans NkQ6KGpu.h (même patron, mais bloc de
// 210 octets NON divisible par 4 : la lecture y est octet par octet).
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Constantes du format (spec ggml : QK_K=256, K_SCALE_SIZE=12,
			// sizeof(block_q4_K) = 2 + 2 + 12 + 128 = 144 octets).
			constexpr uint64 kNkQ4KBlockElems = 256;
			constexpr uint64 kNkQ4KBlockBytes = 144;

			// Octets EXACTS attendus pour un poids [rows, cols] en Q4_K, ou 0 si
			// `cols` n'est pas un multiple de 256 (le format n'a alors pas de
			// représentation ligne-par-ligne : on refuse plutôt que d'arrondir).
			uint64 NkQ4KExpectedBytes(int64 rows, int64 cols);

			// -----------------------------------------------------------------
			// Poids Q4_K résident GPU : le tampon contient les super-blocs BRUTS
			// tels que lus du GGUF (aucune déquantification à l'upload — c'est
			// tout l'intérêt : 4,4 Go au lieu de 28). Les métadonnées de bloc
			// (échelles/minimums 6 bits empaquetés, super-échelles fp16) sont
			// DANS les blocs, il n'y a donc rien à uploader à côté.
			// -----------------------------------------------------------------
			struct NkQ4KGpuWeight {
					uint64 buffer = 0;		 // id opaque NkTensorGpu (0 = invalide)
					int64 rows = 0;			 // N = out_features
					int64 cols = 0;			 // K = in_features (multiple de 256)
					int64 blocksPerRow = 0;	 // cols / 256
					uint64 byteCount = 0;	 // octets réellement uploadés

					bool IsValid() const {
						return buffer != 0 && rows > 0 && cols > 0;
					}

					// Éléments logiques du poids (rows·cols).
					int64 Numel() const {
						return rows * cols;
					}
			};

			// Upload des blocs bruts. `rawByteCount` doit valoir EXACTEMENT
			// NkQ4KExpectedBytes(rows, cols) (contrôle au octet près, comme le
			// loader GGUF le fait déjà pour les tailles de tenseur). Le GPU doit
			// être disponible (NkTensorGpu::IsAvailable).
			bool NkQ4KGpuUpload(const void *rawBlocks, uint64 rawByteCount, int64 rows, int64 cols,
								NkQ4KGpuWeight &out, NkString *outError = nullptr);

			// Libère le tampon GPU et invalide la structure (idempotent).
			void NkQ4KGpuRelease(NkQ4KGpuWeight &w);

			// ---- Noyau 1 : déquantification seule (validation / diagnostic) ---
			// Écrit rows·cols floats dans `outBuffer` (tampon GPU f32 déjà créé,
			// assez grand). Sert à prouver, élément par élément, que le décodage
			// GPU est celui de NkGGUFDequantizeRaw.
			bool NkQ4KGpuDequantizeTo(const NkQ4KGpuWeight &w, uint64 outBuffer, NkString *outError = nullptr);

			// Variante confort : alloue le tampon, dispatch, relit sur CPU.
			// À N'UTILISER QUE sur de petits tenseurs (la sortie f32 pèse 7,1×
			// le poids quantifié : c'est justement ce qu'on veut éviter en vrai).
			bool NkQ4KGpuDequantize(const NkQ4KGpuWeight &w, NkVector<float32> &out, NkString *outError = nullptr);

			// -----------------------------------------------------------------
			// Choix du noyau de matmul.
			//
			// POURQUOI DEUX NOYAUX, ET POURQUOI GARDER LE PREMIER
			// ---------------------------------------------------
			// NK_SIMPLE : un thread = un élément de sortie, chaque thread relit
			//   TOUTE la ligne de W dont il a besoin. À M = 1 c'est optimal (la
			//   ligne n'est lue qu'une fois de toute façon) et le noyau est sans
			//   mémoire partagée ni barrière. Mais le poids quantifié est relu M
			//   FOIS : dès que M grandit — et l'entraînement LoRA travaille sur des
			//   séquences entières, pas sur 4 tokens — c'est la bande passante qui
			//   devient le mur.
			// NK_TILED : un groupe de travail = une tuile de 8 colonnes × 8 tokens.
			//   Les 64 threads déquantifient COOPÉRATIVEMENT 8 super-blocs de W
			//   dans la mémoire partagée (8 Ko), puis les 8 tokens de la tuile les
			//   consomment tous. W n'est donc lu qu'une fois par tuile au lieu
			//   d'une fois par token : le trafic est divisé par 8 dès M ≥ 8.
			// NK_AUTO : choisit sur la MESURE (cf. NKQ4MatmulTest, section (f)).
			//
			// CE QUE LA MESURE A DIT (RTX 3070, Vulkan, K = N = 3584) :
			//   M=64 : 3,7 à 4,4× · M=256 : 5,3× en faveur du tuilé, de façon
			//   REPRODUCTIBLE. À M = 1 et M = 8 le résultat OSCILLE d'une exécution
			//   à l'autre (0,7× à 1,9×) : à cette taille les deux noyaux tiennent
			//   dans ~1 ms dont l'essentiel est le coût FIXE d'un dispatch, et non
			//   du calcul. On ne tranche donc pas sur du bruit : NK_AUTO garde le
			//   noyau simple sous M = 8, où il est au pire à égalité et où il n'a ni
			//   mémoire partagée ni barrière à payer.
			//
			// POURQUOI LE NOYAU SIMPLE RESTE, alors qu'il ne gagne nulle part :
			//   1. c'est l'ORACLE — les deux noyaux somment dans le même ordre sur
			//      les mêmes valeurs, donc ils doivent donner le MÊME résultat ; un
			//      écart dénonce une course sur la mémoire partagée, que le débit ne
			//      montrerait pas ;
			//   2. c'est le REPLI d'un backend qui refuserait la mémoire partagée ou
			//      les barrières (NK_AUTO y retombe automatiquement si le dispatch
			//      tuilé échoue) ;
			//   3. il ne coûte rien : le pipeline n'est compilé que s'il est demandé.
			// -----------------------------------------------------------------
			enum class NkQ4KMatmulKernel {
				NK_AUTO = 0,
				NK_SIMPLE = 1,
				NK_TILED = 2,
			};

			// Seuil de bascule de NK_AUTO : M strictement inférieur -> noyau simple.
			// 8 = la première taille où le tuilage est mesurablement devant de façon
			// reproductible. Valeur issue de la mesure, pas d'une intuition.
			constexpr int64 kNkQ4KTiledMinM = 8;

			// ---- Noyau 2 : matmul FUSÉ déquantification-produit ---------------
			// Y[M,N] = X[M,K] · W[N,K]ᵀ, X et Y en f32, W jamais déquantifié en
			// mémoire. `xBuffer` (M·K floats) et `yBuffer` (M·N floats) sont des
			// tampons GPU déjà créés par l'appelant (NkTensorGpu::CreateBuffer).
			bool NkQ4KGpuMatmul(const NkQ4KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M,
								NkString *outError = nullptr);

			// Même chose, en FORÇANT un noyau précis. Sert à la mesure comparative
			// (on ne peut pas dire si le tuilage a servi sans pouvoir exécuter les
			// deux sur la même machine, le même poids et le même X).
			bool NkQ4KGpuMatmulEx(const NkQ4KGpuWeight &w, uint64 xBuffer, uint64 yBuffer, int64 M,
								  NkQ4KMatmulKernel kernel, NkString *outError = nullptr);

			// Variante confort : upload de X, dispatch, readback de Y.
			bool NkQ4KGpuMatmulCpu(const NkQ4KGpuWeight &w, const float32 *x, int64 M, NkVector<float32> &outY,
								   NkString *outError = nullptr);

			bool NkQ4KGpuMatmulCpuEx(const NkQ4KGpuWeight &w, const float32 *x, int64 M, NkQ4KMatmulKernel kernel,
									 NkVector<float32> &outY, NkString *outError = nullptr);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
