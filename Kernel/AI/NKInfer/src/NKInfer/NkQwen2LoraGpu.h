// =============================================================================
// NkQwen2LoraGpu.h — le Qwen2.5 7B ENTRAÎNABLE : socle quantifié GELÉ résident
// GPU + adaptateurs LoRA sur les 28 couches, forward/backward et boucle SFT
// entièrement sur GPU. Jalon 6 du chantier QLoRA.
//
// POURQUOI UNE CLASSE À PART ET PAS UNE EXTENSION DE NkQwen2Gpu
// -------------------------------------------------------------
// Ce n'est pas de la duplication décorative — les deux objets ont des régimes
// INCOMPATIBLES, et les mélanger aurait coûté plus cher que de les séparer :
//   1. KV-CACHE. NkQwen2Gpu est bâti autour d'un cache résident [nKV, maxSeq,
//      headDim] : c'est le mécanisme de la génération INCRÉMENTALE. L'entraîne-
//      ment voit toute la séquence d'un coup depuis la position 0 ; il n'a pas
//      de cache, et K/V y restent dans la disposition [T, nKV·headDim] produite
//      par les projections — donc des noyaux d'attention DIFFÉRENTS (l'indexa-
//      tion change, pas les maths).
//   2. LOGITS. NkQwen2Gpu ne calcule le lm_head que sur la DERNIÈRE position
//      (matérialiser [T,152064] coûterait 623 Mo à T=1024). L'entraînement a
//      besoin des logits de TOUTES les positions de la réponse.
//   3. ACTIVATIONS. Le backward exige de conserver l'entrée de chaque couche, et
//      d'allouer une trentaine de tampons de gradient que l'inférence n'a pas.
// Ajouter tout cela à NkQwen2Gpu aurait doublé sa surface et mis deux régimes
// dans un même objet — pour finalement partager ~150 lignes de chargement GGUF.
// Le jalon 5 reste donc INTACT et continue de servir la génération rapide.
//
// CHECKPOINTING (« absorbé », comme prévu au jalon 2)
// ---------------------------------------------------
// On ne garde du forward que l'ENTRÉE de chaque couche : 29 tampons [T,d], soit
// 53 Mo à T=128. Au backward, la couche l est REJOUÉE depuis son entrée juste
// avant d'être rétropropagée. Le coût est un forward supplémentaire (+50 % de
// calcul) ; le gain est décisif : garder toutes les activations coûterait
// ~3 Go à T=256 et ne tiendrait pas à côté des 4,07 Go du socle sur 8 Go de
// VRAM. Le pic mémoire est borné par UNE couche, quel que soit leur nombre.
//
// CE QUI APPREND, ET CE QUI N'APPREND PAS
// ----------------------------------------
// APPREND : A et B des adaptateurs, sur les 7 projections des 28 couches.
// GELÉ : embedding, wq/wk/wv/wo/gate/up/down quantifiés, biais Q/K/V, poids de
// RMSNorm, norme finale, lm_head. Le gel est STRUCTUREL — aucun de ces tenseurs
// n'a de tampon de gradient, l'optimiseur ne les voit pas, il n'y a pas de
// masque à oublier de poser.
//
// RANG 8, alpha 16, SUR LES SEPT PROJECTIONS — et pourquoi
// --------------------------------------------------------
//   * les SEPT, pas seulement q/v : QLoRA (Dettmers et al., arXiv:2305.14314)
//     montre que c'est l'adaptation de TOUTES les couches linéaires qui permet
//     d'égaler un fine-tuning complet ; se limiter à q/v est l'ancien réglage de
//     LoRA, pas celui qui a été validé sur des modèles quantifiés ;
//   * rang 8 et pas 16 : 20,2 M paramètres entraînables, soit 323 Mo avec les
//     gradients et les deux moments d'Adam — ce qui TIENT à côté du socle sur
//     une carte de 8 Go, là où le rang 16 en demanderait 646. Et avec quelques
//     centaines d'exemples, il y a déjà cent mille fois plus de paramètres que
//     de données : doubler le rang n'achèterait que de la mémorisation ;
//   * alpha/r = 2 : l'échelle usuelle de la littérature LoRA, gardée telle
//     quelle faute d'une mesure qui justifierait autre chose.
//
// Zéro STL. Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKTensor/NkTensor.h"
#include "NKInfer/NkGGUFLoader.h"
#include "NKInfer/NkQwen2Block.h"
#include "NKInfer/NkQwen2Gpu.h" // NkQwen2GpuWeight / NkQwen2GpuQuant : le MÊME
								// conteneur de poids quantifié qu'au jalon 5 —
								// le réutiliser interdit deux conventions de
								// forme concurrentes dans le même dépôt.
#include "NKInfer/NkLoraGpu.h"
#include "NKInfer/NkQwen2Sft.h" // NkQwen2SftExample (tokens + masque de perte)

namespace nkentseu {
	namespace ai {
		namespace infer {

			struct NkQwen2LoraGpuOptions {
					// Longueur MAXIMALE d'une séquence d'entraînement. Dimensionne
					// TOUS les tampons ; un exemple plus long est REFUSÉ (jamais
					// tronqué en silence : une réponse coupée en deux apprendrait au
					// modèle à ne pas finir ses phrases).
					int32 maxSeqLen = 128;
					int32 loraRank = 8;
					float32 loraAlpha = 16.0f;
					// Écart-type d'initialisation de A. B = 0, donc le delta initial
					// est EXACTEMENT nul quel que soit sigma ; sigma ne fixe que
					// l'échelle du premier gradient utile.
					float32 loraSigma = 0.01f;
					uint64 loraSeed = 20260806ull;
					uint32 maxLayers = 0; // 0 = toutes
					bool verbose = true;
			};

			struct NkQwen2LoraGpuStats {
					uint64 weightBytes = 0;	 // socle quantifié + normes + biais
					uint64 loraBytes = 0;	 // A + B
					uint64 optimBytes = 0;	 // gradients + moments d'Adam
					uint64 ckptBytes = 0;	 // entrées de couche conservées
					uint64 scratchBytes = 0; // activations + gradients intermédiaires
					uint32 bufferCount = 0;
					float64 loadSeconds = 0.0;
					float64 readSeconds = 0.0;
					uint64 q4Tensors = 0, q6Tensors = 0, f32Tensors = 0;
					int64 loraParams = 0;

					uint64 TotalBytes() const {
						return weightBytes + loraBytes + optimBytes + ckptBytes + scratchBytes;
					}
			};

			class NkQwen2LoraGpu {
				public:
					NkQwen2LoraGpu() = default;
					~NkQwen2LoraGpu();
					NkQwen2LoraGpu(const NkQwen2LoraGpu &) = delete;
					NkQwen2LoraGpu &operator=(const NkQwen2LoraGpu &) = delete;

					bool Load(const char *ggufPath, const NkQwen2LoraGpuOptions &opt, NkString *err = nullptr);
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
					int32 MaxSeqLen() const {
						return mOpt.maxSeqLen;
					}
					const NkQwen2LoraGpuStats &Stats() const {
						return mStats;
					}
					const char *BackendName() const;
					NkVector<NkLoraGpuSet> &Lora() {
						return mLora;
					}
					const NkVector<NkLoraGpuSet> &Lora() const {
						return mLora;
					}

					// Forward de la séquence COMPLÈTE depuis la position 0. Les logits
					// [T,V] restent en VRAM (LogitsBuffer()).
					// Les checkpoints sont TOUJOURS écrits, et ce n'est pas un oubli
					// d'option : la chaîne des couches passe DÉJÀ par eux (la sortie de
					// la couche l EST l'entrée de la couche l+1). Les garder ne coûte
					// donc rien de plus qu'une inférence — ni mémoire, ni copie.
					bool Forward(const int32 *ids, int32 T, NkString *err = nullptr);

					// Perte cross-entropie MASQUÉE sur les logits du dernier Forward.
					// `mask[t] == 0` -> la position t est du PROMPT : ni perte, ni
					// gradient (gradient STRUCTURELLEMENT nul, pas un one-hot mis à
					// zéro — cf NkQwen2Sft.h, patch 03_LOSS_MASKING).
					// Si `wantGrad`, écrit dLogits EN PLACE dans le tampon de logits
					// (ils ne servent plus après) et le backward peut suivre.
					bool Loss(const NkVector<int32> &targets, const NkVector<float32> &mask, bool wantGrad,
							  float64 &outLoss, int64 &outActive, NkString *err = nullptr);

					// Backward complet : lm_head -> norme finale -> 28 couches en ordre
					// inverse (chacune REJOUÉE depuis son checkpoint). Accumule dans les
					// dA/dB des adaptateurs. Ne produit AUCUN gradient de socle.
					bool Backward(int32 T, NkString *err = nullptr);

					// Génération gloutonne SANS KV-cache : la séquence entière est
					// réévaluée à chaque pas. C'est lent (le coût croît avec T) mais
					// c'est le MÊME chemin de calcul que l'entraînement, adaptateurs
					// compris — donc la comparaison « avant / après » ne mélange pas
					// deux implémentations.
					bool Generate(const NkVector<int32> &promptIds, int32 nNew, int32 stopId, NkVector<int32> &outIds,
								  float64 *outSeconds = nullptr, NkString *err = nullptr);

					// Somme des carrés de TOUS les gradients d'adaptateurs (diagnostic :
					// une norme qui s'effondre ou explose se voit là).
					bool GradGlobalNorm(float64 &outNorm, NkString *err = nullptr);

					bool ZeroAllGrads(NkString *err = nullptr);
					bool AdamStep(float32 lr, int64 stepIndex, NkString *err = nullptr);

					// Lignes d'embedding déquantifiées depuis le FICHIER (le tenseur
					// n'est pas monté en VRAM : 306 Mo pour 2 Ko lus par token — même
					// choix qu'au jalon 5, et surtout MÊME chemin, donc mêmes octets).
					bool EmbedTokens(const int32 *ids, int32 count, NkTensor &out, NkString *err = nullptr);

					// Relit les logits de la position `row` (V floats).
					bool DownloadLogitsRow(int32 row, int32 T, NkVector<float32> &out, NkString *err = nullptr);

					// Accès brut, pour les vérifications du test.
					uint64 LogitsBuffer() const {
						return mBufLogits;
					}

				private:
					bool UploadQuant(const NkGGUFTensorInfo &info, NkQwen2GpuWeight &out, NkString *err);
					bool UploadF32(const NkGGUFTensorInfo &info, uint64 &outBuffer, int64 expectedNumel, NkString *err);
					const NkGGUFTensorInfo *Find(const char *name) const;
					uint64 NewBuffer(uint64 bytes, uint64 &accumulator);
					bool AllocScratch(NkString *err);
					void BuildRopeTables();
					bool CreateAdapters(NkString *err);

					// Y = X·Wᵀ (socle gelé) et dX (+)= dY·W : les deux sens, agnostiques
					// du format quantifié.
					bool Matmul(const NkQwen2GpuWeight &w, uint64 xBuf, uint64 yBuf, int64 M, NkString *err);
					bool MatmulT(const NkQwen2GpuWeight &w, uint64 dyBuf, uint64 dxBuf, int64 M, bool accum,
								 NkString *err);

					// Rejoue le forward de la couche `l` depuis `xBuf` dans les tampons
					// de travail (sans toucher aux checkpoints). `outBuf` reçoit la
					// sortie de la couche si non nul.
					bool LayerForward(uint32 l, uint64 xBuf, uint64 outBuf, int64 T, NkString *err);
					bool LayerBackward(uint32 l, uint64 xBuf, int64 T, NkString *err);

					NkGGUFFile mGguf;
					NkString mPath;
					NkQwen2Config mCfg;
					NkQwen2LoraGpuOptions mOpt;
					NkQwen2LoraGpuStats mStats;

					// Poids d'une couche (socle gelé). Pas de KV-cache : cf en-tête.
					struct Layer {
							NkQwen2GpuWeight wq, wk, wv, wo, wGate, wUp, wDown;
							uint64 attnNorm = 0, ffnNorm = 0;
							uint64 bq = 0, bk = 0, bv = 0;
					};
					NkVector<Layer> mLayers;
					NkVector<NkLoraGpuSet> mLora;

					uint64 mOutputNorm = 0;
					NkQwen2GpuWeight mLmHead;
					const NkGGUFTensorInfo *mEmbedInfo = nullptr;
					uint64 mEmbedBytesPerRow = 0;

					// Checkpoints : mCkpt[l] = entrée de la couche l ; mCkpt[L] = sortie
					// de la dernière couche (= entrée de la RMSNorm finale).
					NkVector<uint64> mCkpt;

					// Forward
					uint64 mBufX = 0, mBufX1 = 0, mBufXn1 = 0, mBufXn2 = 0, mBufAttn = 0, mBufDown = 0;
					uint64 mBufQ = 0, mBufQb = 0, mBufQr = 0, mBufCtx = 0;
					uint64 mBufK = 0, mBufKb = 0, mBufKr = 0, mBufV = 0, mBufVb = 0;
					uint64 mBufScores = 0, mBufProbs = 0;
					uint64 mBufGate = 0, mBufUp = 0, mBufAct = 0;
					uint64 mBufRope = 0, mBufLogits = 0, mBufRow = 0;
					// Backward
					uint64 mBufDX = 0, mBufDX1 = 0, mBufDXn = 0, mBufDTmp = 0, mBufDNrm = 0;
					uint64 mBufDAct = 0, mBufDGate = 0, mBufDUp = 0, mBufDTmpF = 0;
					uint64 mBufDCtx = 0, mBufDQr = 0, mBufDQ = 0;
					uint64 mBufDK = 0, mBufDKr = 0, mBufDV = 0;
					uint64 mBufDScores = 0, mBufDProbs = 0;
					uint64 mBufU = 0, mBufDU = 0;
					uint64 mBufMeta = 0, mBufLoss = 0, mBufNormPart = 0;

					int64 mVocabSize = 0;
					bool mTied = false;
					bool mLoaded = false;
			};

			// -----------------------------------------------------------------------
			// Boucle SFT : UN exemple, UN pas d'Adam (B = 1, la portée de NKInfer).
			// POURQUOI PAS D'ACCUMULATION MULTI-EXEMPLES : elle demanderait de diviser
			// les gradients par le nombre d'exemples, donc un noyau qui lit et écrit
			// le même tampon (motif interdit ici), OU d'accepter que la division soit
			// « presque » absorbée par l'invariance d'échelle d'Adam — un « presque »
			// qu'on ne veut pas dans une preuve de chaîne.
			// -----------------------------------------------------------------------
			class NkQwen2LoraGpuTrainer {
				public:
					bool Init(NkQwen2LoraGpu *model, float32 lr, NkString *err = nullptr);

					// forward + CE masquée + backward + pas d'Adam. Renvoie la perte de
					// l'exemple, ou un négatif en cas d'erreur.
					double TrainExample(const NkQwen2SftExample &ex, NkString *err = nullptr);
					// Perte seule (aucun gradient) — la validation.
					double EvaluateExample(const NkQwen2SftExample &ex, NkString *err = nullptr);

					int64 StepCount() const {
						return mT;
					}
					// REPRISE d'un entraînement interrompu. Le compteur de pas n'est
					// pas décoratif : Adam corrige le biais de ses moments par b1ᵗ et
					// b2ᵗ. Repartir de t = 0 avec des moments déjà grands appliquerait
					// la correction du tout premier pas — un pas géant, et la perte
					// diverge. C'est pour ça que l'état de reprise porte `step`.
					void SetStepCount(int64 t) {
						mT = t;
					}
					float32 LearningRate() const {
						return mLr;
					}
					void SetLearningRate(float32 lr) {
						mLr = lr;
					}

				private:
					// inputs = tokens[0..n-2], cibles = tokens[1..n-1], masque de la
					// cible t = lossMask[t+1]. Le décalage vit ICI et pas dans le
					// formatage : le masque décrit les TOKENS, pas la boucle.
					bool Split(const NkQwen2SftExample &ex, NkVector<int32> &inputs, NkVector<int32> &targets,
							   NkVector<float32> &mask) const;

					NkQwen2LoraGpu *mModel = nullptr;
					int64 mT = 0;
					float32 mLr = 1e-4f;
			};

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
