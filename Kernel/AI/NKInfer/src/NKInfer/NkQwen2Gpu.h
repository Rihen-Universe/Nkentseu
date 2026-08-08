// =============================================================================
// NkQwen2Gpu.h — le modèle Qwen2.5 7B ASSEMBLÉ et RÉSIDENT GPU.
// Jalon 5 du chantier QLoRA (Documentation/notes_etape4_qlora.md §4).
//
// POURQUOI CE MODULE EXISTE
// -------------------------
// Jusqu'ici, l'orchestration du 7B vivait dans le `main()` d'un TEST
// (Applications/NKLLMInferTest) : chargement couche par couche, déquantification
// complète en f32 (932 Mo par couche, relue à CHAQUE pas), forward CPU, lm_head.
// Cela a prouvé que l'architecture est juste — 28 couches réelles, génération
// autorégressive réelle — mais à ~81 s par token en Debug, et sans que rien de
// tout cela ne soit réutilisable ailleurs qu'à l'intérieur de ce main.
//
// Ce module sort cette orchestration du test et la pose là où elle appartient :
//   * les poids restent QUANTIFIÉS de bout en bout — lus bruts du GGUF, uploadés
//     tels quels (NkQ4KGpuUpload / NkQ6KGpuUpload, jalon 4), jamais déquantifiés
//     côté CPU, jamais matérialisés en f32 en VRAM. 4,6 Go au lieu de 28 ;
//   * le forward des 28 couches est intégralement GPU : les sept projections par
//     couche passent par le matmul FUSÉ déquantification-produit du jalon 4, et
//     tout le reste (RMSNorm, RoPE, GQA, softmax causal, SwiGLU, résiduels) par
//     des noyaux NkSL écrits ici ;
//   * le KV-cache est lui aussi résident GPU (aucun aller-retour par pas) ;
//   * la vérité de référence reste NkQwen2Block.cpp (CPU f32) : ce module
//     reproduit son calcul opération par opération, dans le MÊME ORDRE de
//     sommation, pour que l'écart mesuré soit celui de l'arithmétique flottante
//     et de rien d'autre.
//
// CE QUI RESTE VOLONTAIREMENT CÔTÉ CPU, ET POURQUOI
// -------------------------------------------------
// 1. La LECTURE DES LIGNES D'EMBEDDING. `token_embd.weight` pèse 306 Mo en
//    Q4_K ; ce blob possède un `output.weight` distinct (embeddings NON liées),
//    donc la table d'embedding n'est utile qu'en LECTURE DE LIGNES : une par
//    token, 2016 octets de blocs Q4_K. La monter en VRAM coûterait 306 Mo pour
//    économiser une lecture de 2 Ko par token — et c'est justement ce tenseur
//    qui dépasse les 128 Mo garantis par D3D11 (piège n°1 du jalon). On lit donc
//    la ligne dans le fichier et on la déquantifie avec NkGGUFDequantizeRaw : la
//    référence CPU utilise EXACTEMENT le même chemin, ce qui élimine une source
//    de divergence à l'entrée même du modèle.
//    (Si un modèle a des embeddings LIÉES — pas `output.weight` —, alors
//    `token_embd.weight` sert de lm_head et EST monté en VRAM : le cas est géré.)
// 2. Les TABLES cos/sin du RoPE. Elles sont calculées en DOUBLE au chargement,
//    avec la formule EXACTE de NkApplyRoPE (pow/cos/sin en double), puis
//    uploadées. Un `pow(1e6, -2i/128)` calculé en float dans le shader donnerait
//    un theta décalé de ~1e-6 relatif, soit une erreur d'angle qui grandit avec
//    la position : autant ne pas l'introduire du tout, pour 256 Ko de table.
// 3. L'ÉCHANTILLONNAGE (NkSampling) : il porte sur UN vecteur de 152 064 logits
//    déjà relu, et il doit rester bit-identique à celui de la référence.
//
// CONVENTION DE FORME : celle du GGUF et du jalon 4 — un poids est [N=out, K=in]
// avec K contigu, donc Y = X·Wᵀ sans jamais matérialiser de transposée.
//
// Zéro STL. Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKTensor/NkTensor.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkQ4KGpu.h"
#include "NKInfer/NkQ6KGpu.h"
#include "NKInfer/NkQwen2Block.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// -----------------------------------------------------------------
			// Un poids de projection résident GPU, quel que soit son format.
			//
			// POURQUOI UNE UNION EXPLICITE ET PAS UN POLYMORPHISME : dans un GGUF
			// « Q4_K_M », le format varie D'UN TENSEUR À L'AUTRE (ici `ffn_down`
			// est en Q6_K, le reste en Q4_K) et c'est le FICHIER qui décide, pas
			// nous. Le type doit donc être une donnée lue, pas un paramètre de
			// compilation. Deux champs valent mieux qu'un virtuel : il n'y a que
			// deux formats possibles et le dispatch tient en un `switch`.
			// -----------------------------------------------------------------
			enum class NkQwen2GpuQuant : uint32 {
				NK_NONE = 0,
				NK_Q4K = 1,
				NK_Q6K = 2,
			};

			struct NkQwen2GpuWeight {
					NkQwen2GpuQuant kind = NkQwen2GpuQuant::NK_NONE;
					NkQ4KGpuWeight q4;
					NkQ6KGpuWeight q6;

					bool IsValid() const {
						return (kind == NkQwen2GpuQuant::NK_Q4K && q4.IsValid()) ||
							   (kind == NkQwen2GpuQuant::NK_Q6K && q6.IsValid());
					}
					int64 Rows() const {
						return kind == NkQwen2GpuQuant::NK_Q4K ? q4.rows : q6.rows;
					}
					int64 Cols() const {
						return kind == NkQwen2GpuQuant::NK_Q4K ? q4.cols : q6.cols;
					}
					uint64 Bytes() const {
						return kind == NkQwen2GpuQuant::NK_Q4K ? q4.byteCount : q6.byteCount;
					}
			};

			// Poids d'UNE couche, tous résidents GPU. Les normes et les biais sont
			// des tampons f32 (ils sont en F32 dans le GGUF et pèsent quelques Ko :
			// les quantifier n'aurait rien économisé et aurait ajouté un format).
			struct NkQwen2GpuLayer {
					NkQwen2GpuWeight wq, wk, wv, wo;
					NkQwen2GpuWeight wGate, wUp, wDown;
					uint64 attnNorm = 0, ffnNorm = 0; // f32 [d]
					uint64 bq = 0, bk = 0, bv = 0;	  // f32 [qd] / [kvd]
					uint64 kCache = 0, vCache = 0;	  // f32 [nKV, maxSeq, headDim]
			};

			struct NkQwen2GpuOptions {
					// Longueur MAXIMALE de séquence (prompt + généré). Dimensionne le
					// KV-cache et les tampons de scores : ils sont alloués UNE fois,
					// jamais réalloués en cours de génération.
					int32 maxSeqLen = 256;
					// Nombre maximal de tokens traités en un seul appel de Forward
					// (préfill). Un préfill plus long est découpé automatiquement.
					int32 maxBatchTokens = 64;
					// 0 = toutes les couches du fichier. Utile pour un test partiel.
					uint32 maxLayers = 0;
					bool verbose = true;
			};

			// Comptabilité MÉMOIRE : additionne les octets réellement demandés à
			// NkTensorGpu::CreateBuffer. C'est une comptabilité EXACTE de ce que ce
			// module alloue — pas une mesure de l'occupation VRAM totale du
			// processus, qui inclut le pilote, le compositeur et les tampons de
			// transit. Les deux chiffres sont différents et il faut le dire.
			struct NkQwen2GpuStats {
					uint64 weightBytes = 0;	 // poids quantifiés + normes + biais
					uint64 kvBytes = 0;		 // KV-cache
					uint64 scratchBytes = 0; // activations, scores, logits, tables RoPE
					uint32 bufferCount = 0;
					float64 loadSeconds = 0.0;
					float64 readSeconds = 0.0; // part passée à lire le fichier
					uint64 q4Tensors = 0, q6Tensors = 0, f32Tensors = 0;

					uint64 TotalBytes() const {
						return weightBytes + kvBytes + scratchBytes;
					}
			};

			// Capture d'états intermédiaires — c'est l'INSTRUMENT de la validation
			// du jalon : comparer les logits finaux ne dirait pas OÙ un écart naît.
			// `layers` liste les indices de couche dont l'état de SORTIE doit être
			// relu sur CPU (`hidden[i]` correspond à `layers[i]`).
			struct NkQwen2GpuTrace {
					NkVector<int32> layers;
					NkVector<NkTensor> hidden; // [T,d] f32 CPU, rempli par Forward
					NkTensor finalNorm;		   // [T,d] après la RMSNorm finale
					bool captureFinalNorm = false;
			};

			class NkQwen2Gpu {
				public:
					NkQwen2Gpu() = default;
					~NkQwen2Gpu();
					NkQwen2Gpu(const NkQwen2Gpu &) = delete;
					NkQwen2Gpu &operator=(const NkQwen2Gpu &) = delete;

					// Charge le modèle : métadonnées, hyperparamètres RÉELS (aucune
					// valeur supposée), puis upload des poids BRUTS quantifiés,
					// tenseur par tenseur. Le GPU doit être disponible.
					bool Load(const char *ggufPath, const NkQwen2GpuOptions &opt, NkString *err = nullptr);
					void Release();

					bool IsLoaded() const {
						return mLoaded;
					}
					const NkQwen2Config &Config() const {
						return mCfg;
					}
					uint32 LayerCount() const {
						return mLayers.Size();
					}
					int64 VocabSize() const {
						return mVocabSize;
					}
					bool TiedEmbeddings() const {
						return mTied;
					}
					const NkQwen2GpuStats &Stats() const {
						return mStats;
					}
					const char *BackendName() const;

					// ---- KV-cache ------------------------------------------------
					void ResetCache();
					int32 CacheLength() const {
						return mCacheLen;
					}

					// Déquantifie les LIGNES d'embedding des tokens demandés (lecture
					// fichier + NkGGUFDequantizeRaw — cf en-tête). Sortie [count, d].
					bool EmbedTokens(const int32 *ids, int32 count, NkTensor &out, NkString *err = nullptr);

					// Forward de `count` tokens (préfill si count>1, pas incrémental si
					// count==1), à partir de la position CacheLength(). Renvoie les
					// logits [1, vocab] de la DERNIÈRE position uniquement : c'est tout
					// ce dont la génération a besoin, et matérialiser [T,152064] coûte
					// 623 Mo à T=1024 (piège n°10 des notes).
					// `trace` (optionnel) fait relire des états intermédiaires.
					bool Forward(const int32 *ids, int32 count, NkTensor &outLogits, NkQwen2GpuTrace *trace = nullptr,
								 NkString *err = nullptr);

					// Génération autorégressive complète : préfill du prompt (découpé en
					// tranches de maxBatchTokens) puis `nNew` tokens. `temperature <= 0`
					// -> glouton déterministe ; sinon top-k stochastique reproductible.
					// `stopId` (>= 0) interrompt la génération dès qu'il est produit.
					bool Generate(const NkVector<int32> &promptIds, int32 nNew, float32 temperature, int32 topK,
								  uint32 &rngState, int32 stopId, NkVector<int32> &outIds,
								  float64 *outPrefillSeconds = nullptr, float64 *outMsPerToken = nullptr,
								  NkString *err = nullptr);

				private:
					// Upload d'un tenseur du GGUF, dans SON format d'origine.
					bool UploadQuant(const NkGGUFTensorInfo &info, NkQwen2GpuWeight &out, NkString *err);
					bool UploadF32(const NkGGUFTensorInfo &info, uint64 &outBuffer, int64 expectedNumel, NkString *err);
					const NkGGUFTensorInfo *Find(const char *name) const;

					uint64 NewBuffer(uint64 bytes, uint64 &accumulator);
					bool AllocScratch(NkString *err);
					void BuildRopeTables();

					// Matmul fusé, format-agnostique : Y[M,N] = X[M,K]·W[N,K]ᵀ.
					bool Matmul(const NkQwen2GpuWeight &w, uint64 xBuf, uint64 yBuf, int64 M, NkString *err);

					bool ForwardChunk(const int32 *ids, int32 count, NkTensor *outLogits, NkQwen2GpuTrace *trace,
									  NkString *err);

					NkGGUFFile mGguf;
					NkString mPath;
					NkQwen2Config mCfg;
					NkQwen2GpuOptions mOpt;
					NkQwen2GpuStats mStats;
					NkVector<NkQwen2GpuLayer> mLayers;

					uint64 mOutputNorm = 0;	 // f32 [d]
					NkQwen2GpuWeight mLmHead; // output.weight, ou token_embd si lié
					const NkGGUFTensorInfo *mEmbedInfo = nullptr;
					uint64 mEmbedBytesPerRow = 0;

					// Tampons d'activation (alloués une fois — cf AllocScratch).
					uint64 mBufX = 0, mBufX1 = 0, mBufXn = 0, mBufAttn = 0, mBufDown = 0;
					uint64 mBufQ = 0, mBufQb = 0, mBufQr = 0, mBufCtx = 0;
					uint64 mBufK = 0, mBufKb = 0, mBufKr = 0, mBufV = 0, mBufVb = 0;
					uint64 mBufScores = 0, mBufProbs = 0;
					uint64 mBufGate = 0, mBufUp = 0, mBufAct = 0;
					uint64 mBufLast = 0, mBufLogits = 0, mBufRope = 0;

					int64 mVocabSize = 0;
					int32 mCacheLen = 0;
					bool mTied = false;
					bool mLoaded = false;
			};

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
