// =============================================================================
// NkLoraGpu.h — adaptateurs LoRA RÉSIDENTS GPU + persistance « NKLA ».
// Jalon 6 du chantier QLoRA.
//
// CE QUE CE MODULE AJOUTE À NkLora (jalon 2, CPU, prouvé au gradient numérique)
// ----------------------------------------------------------------------------
// Les MÊMES formules, mais sur des tampons GPU : A [r,in], B [out,r], leurs
// gradients, et les deux moments d'Adam — soit HUIT tampons par projection. Rien
// ne redescend jamais sur CPU pendant l'entraînement ; le seul aller-retour est
// la sauvegarde NKLA, explicitement demandée.
//
//   forward  : y += (alpha/r) · (x·Aᵀ)·Bᵀ
//   backward : u  = x·Aᵀ                (RECALCULÉ — moins cher que le stocker)
//              dB += scale · dYᵀ·u
//              du  = scale · dY·B
//              dA += duᵀ·x
//              dX += du·A
// Aucun gradient pour le socle : il n'apparaît nulle part dans ces formules.
//
// POURQUOI HUIT TAMPONS ET PAS UN SEUL, PLAT
// -------------------------------------------
// Un seul grand tampon « tous les paramètres » permettrait UN pas d'Adam pour
// tout le modèle. Mais `NkTensorGpu::RunAdam` prend des tampons entiers, et
// surtout : les produits matriciels de LoRA ont besoin de A et de B comme
// matrices INDÉPENDANTES avec leurs propres formes. Aplatir obligerait à passer
// des OFFSETS à tous les noyaux — c'est-à-dire à réécrire les lanceurs
// génériques de NKTensor, que ce chantier s'interdit de toucher. Le prix est
// 392 tampons de plus (28 couches × 7 projections × 2) ; le pilote les gère.
//
// FORMAT NKLA (« NKentseu Lora Adapters ») — nouveau, décrit ici en entier
// ------------------------------------------------------------------------
//   octets 0-3    : magie 'N','K','L','A'
//   4-7           : version (uint32, = 1)
//   8-11          : nombre de couches (uint32)
//   12-15         : nombre de projections par couche (uint32, = 7)
//   16-19         : rang r (uint32)
//   20-23         : alpha (float32)
//   24-27         : dModel (int32)      \  redondants avec le GGUF, mais c'est
//   28-31         : ffnDim (int32)       > EXACTEMENT le rôle d'un en-tête : un
//   32-35         : qDim (int32)         / adaptateur rechargé sur un modèle qui
//   36-39         : kvDim (int32)       /  n'est pas le sien doit être REFUSÉ,
//                                          pas appliqué de travers.
//   40-43         : flags (cf. NkNklaFlags) — 0 en version 1
//   44-47         : réservé (0)
//   puis, dans l'ordre couche croissante × {q,k,v,o,gate,up,down} :
//     uint32 présent (0 = pas d'adaptateur sur cette projection)
//     int32 outFeatures, int32 inFeatures
//     float32 A[r × in]  (ligne par ligne)
//     float32 B[out × r]
//     si flags & NK_NKLA_TRAIN_STATE :
//       float32 mA[r × in], mB[out × r]   (Adam, 1er moment)
//       float32 vA[r × in], vB[out × r]   (Adam, 2e moment)
//   si flags & NK_NKLA_TRAIN_STATE, le bloc d'état d'entraînement :
//     int64 step, int64 corpusPos, int64 epoch, uint64 rngState
//     float32 lr, beta1, beta2, eps
//   enfin : uint64 empreinte FNV-1a 64 de TOUT ce qui précède.
//
// VERSION 2 : ajoute les flags et, sous condition, moments + état. Un fichier
// version 1 reste lu tel quel (flags = 0) — la version existe pour ça.
//
// L'empreinte n'est pas de la coquetterie : sans elle, un fichier tronqué par un
// disque plein se recharge silencieusement et l'entraînement repart d'un état
// faux, ce qui coûterait exactement les heures que la sauvegarde est censée
// protéger.
//
// Zéro STL. Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKInfer/NkLora.h" // NkLoraRng : la MÊME source d'aléa qu'au jalon 2

namespace nkentseu {
	namespace ai {
		namespace infer {

			// Une paire d'adaptateurs résidente GPU. Les tampons d'entraînement
			// (gradients + moments) ne sont alloués que si `training` est demandé :
			// une inférence avec adaptateurs n'en a pas besoin, et ils pèsent trois
			// fois les paramètres eux-mêmes.
			struct NkLoraGpuPair {
					uint64 A = 0, B = 0;   // paramètres
					uint64 dA = 0, dB = 0; // gradients accumulés
					uint64 mA = 0, mB = 0; // Adam, 1er moment
					uint64 vA = 0, vB = 0; // Adam, 2e moment
					int32 r = 0, inF = 0, outF = 0;
					float32 alpha = 0.0f;

					bool IsValid() const {
						return A != 0 && B != 0 && r > 0 && inF > 0 && outF > 0;
					}
					bool HasGrads() const {
						return dA != 0 && dB != 0;
					}
					float32 Scale() const {
						return r > 0 ? alpha / (float32)r : 0.0f;
					}
					int64 NumelA() const {
						return (int64)r * (int64)inF;
					}
					int64 NumelB() const {
						return (int64)outF * (int64)r;
					}
					int64 Params() const {
						return NumelA() + NumelB();
					}
			};

			// Les sept projections d'une couche Qwen2. Une paire invalide = pas
			// d'adaptateur sur cette projection (le socle passe seul).
			struct NkLoraGpuSet {
					NkLoraGpuPair q, k, v, o, gate, up, down;

					// Accès indexé (0..6) dans l'ORDRE CANONIQUE q,k,v,o,gate,up,down
					// — c'est celui du fichier NKLA et celui de l'itération du
					// trainer. Un ordre écrit une seule fois ne peut pas diverger.
					NkLoraGpuPair *At(int32 i);
					const NkLoraGpuPair *At(int32 i) const;
					static constexpr int32 kCount = 7;
					static const char *Name(int32 i);
			};

			// Crée les tampons et initialise A ~ N(0, sigma), B = 0 (delta initial
			// EXACTEMENT nul : insérer l'adaptateur ne change pas la sortie du
			// socle — la propriété prouvée bit-à-bit au jalon 2, et celle qui rend
			// la comparaison « avant/après » honnête).
			// `outAllocatedBytes` est incrémenté de ce qui a été demandé au pilote.
			bool NkLoraGpuCreate(NkLoraGpuPair &p, int32 outFeatures, int32 inFeatures, int32 r, float32 alpha,
								 float32 sigma, NkLoraRng &rng, bool training, uint64 *outAllocatedBytes,
								 NkString *err = nullptr);
			void NkLoraGpuRelease(NkLoraGpuPair &p);

			// y[T,out] += scale · (x[T,in]·Aᵀ)·Bᵀ. `uBuf` est un tampon de travail
			// d'au moins T·r floats fourni par l'appelant (le modèle en garde un
			// seul, réutilisé par toutes les projections : r est minuscule).
			bool NkLoraGpuForward(const NkLoraGpuPair &p, uint64 xBuf, uint64 yBuf, int64 T, uint64 uBuf,
								  NkString *err = nullptr);

			// Backward complet d'une paire. `xBuf` = l'ENTRÉE du forward, `dyBuf` =
			// le gradient de la SORTIE, `dxBuf` = là où accumuler la contribution
			// qui traverse (peut être 0 : sous la couche 0 personne ne l'attend).
			// `uBuf`/`duBuf` : deux tampons de travail de T·r floats.
			bool NkLoraGpuBackward(const NkLoraGpuPair &p, uint64 xBuf, uint64 dyBuf, int64 T, uint64 uBuf,
								   uint64 duBuf, uint64 dxBuf, NkString *err = nullptr);

			// dA = dB = 0.
			bool NkLoraGpuZeroGrads(const NkLoraGpuPair &p, NkString *err = nullptr);

			// Un pas d'Adam sur A et B (les gradients sont supposés DÉJÀ moyennés et
			// éventuellement écrêtés par l'appelant). b1t = 1−β1ᵗ, b2t = 1−β2ᵗ.
			bool NkLoraGpuAdamStep(const NkLoraGpuPair &p, float32 lr, float32 b1, float32 b2, float32 eps, float32 b1t,
								   float32 b2t, NkString *err = nullptr);

			// (Il n'y a PAS de « multiplier les gradients » : le trainer fait UN pas
			// par exemple — B = 1, la portée de tout NKInfer depuis le jalon 2 —, donc
			// aucune moyenne n'est à faire. Une mise à l'échelle exigerait un noyau qui
			// lit ET écrit le même tampon sur deux liaisons, le seul motif que ce
			// chantier s'est interdit ; cf NkGpuZeroBuffer.)

			// ---- Persistance NKLA --------------------------------------------------
			// Drapeaux de l'en-tête (octets 40-43, ex-réservés). En AJOUT SEUL.
			enum NkNklaFlags : uint32 {
				// Le fichier porte, en plus de A et B, les DEUX moments d'Adam par
				// projection et le bloc d'état d'entraînement final. C'est ce qui
				// distingue une REPRISE d'une simple livraison d'adaptateurs.
				NK_NKLA_TRAIN_STATE = 1u << 0,
			};

			// État nécessaire pour REPRENDRE un entraînement interrompu (coupure de
			// courant, machine éteinte, plantage). Sans lui, une reprise repart avec
			// des moments d'Adam nuls et un compteur de pas à zéro : la correction de
			// biais d'Adam (b1ᵗ, b2ᵗ) redevient celle du tout premier pas, ce qui
			// produit un pas géant sur des moments déjà grands — l'entraînement
			// diverge, et le fichier de reprise aurait coûté les heures qu'il devait
			// protéger.
			struct NkLoraTrainState {
					int64 step = 0;		 ///< t d'Adam (nombre de pas déjà faits)
					int64 corpusPos = 0; ///< index de l'exemple SUIVANT à traiter
					int64 epoch = 0;
					uint64 rngState = 0; ///< pour que le mélange reste reproductible
					float32 lr = 0.0f, beta1 = 0.0f, beta2 = 0.0f, eps = 0.0f;
			};

			struct NkLoraGpuNklaInfo {
					uint32 version = 0;
					uint32 flags = 0; ///< cf. NkNklaFlags
					uint32 layerCount = 0;
					uint32 projCount = 0;
					uint32 rank = 0;
					float32 alpha = 0.0f;
					int32 dModel = 0, ffnDim = 0, qDim = 0, kvDim = 0;
					uint64 paramCount = 0;
					uint64 fileBytes = 0;
			};

			/// `train` non nul = écrire un fichier de REPRISE (moments + état).
			/// Il pèse 3× un fichier d'adaptateurs seuls, et reste utilisable tel
			/// quel pour l'inférence : le lecteur ignore ce qu'il n'a pas demandé.
			bool NkLoraGpuSaveNKLA(const char *path, const NkVector<NkLoraGpuSet> &sets, int32 rank, float32 alpha,
								   int32 dModel, int32 ffnDim, int32 qDim, int32 kvDim, NkLoraGpuNklaInfo *outInfo,
								   NkString *err = nullptr, const NkLoraTrainState *train = nullptr);

			// Recharge DANS des paires DÉJÀ créées (mêmes formes) : le rechargement
			// ne fabrique pas de tampons, il remplit ceux du modèle courant. Toute
			// incohérence d'en-tête ou d'empreinte est un REFUS, jamais une
			// tolérance.
			/// `outTrain` non nul : demande l'état de reprise. Il n'est REMPLI que si
			/// le fichier le porte ; sinon il reste tel quel et l'appelant le voit à
			/// `outInfo->flags`. Réclamer un état absent n'est pas une erreur — un
			/// fichier d'adaptateurs livré reste chargeable pour repartir de zéro.
			bool NkLoraGpuLoadNKLA(const char *path, NkVector<NkLoraGpuSet> &sets, NkLoraGpuNklaInfo *outInfo,
								   NkString *err = nullptr, NkLoraTrainState *outTrain = nullptr);

			// Lit seulement l'en-tête (diagnostic).
			bool NkLoraGpuInspectNKLA(const char *path, NkLoraGpuNklaInfo &out, NkString *err = nullptr);

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
