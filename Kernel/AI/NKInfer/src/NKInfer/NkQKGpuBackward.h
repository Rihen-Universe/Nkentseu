// =============================================================================
// NkQKGpuBackward.h — les noyaux qui MANQUAIENT pour rétropropager sur GPU à
// travers un socle quantifié GELÉ. Jalon 6 du chantier QLoRA.
//
// CE QUI MANQUAIT, ET POURQUOI CE N'EST PAS UN DÉTAIL
// ---------------------------------------------------
// Le jalon 4 a livré le matmul fusé déquantification-produit dans UN SEUL sens :
//     Y[M,N] = X[M,K] · W[N,K]ᵀ        (somme sur K, la dimension CONTIGUË)
// C'est le sens du forward, et c'est le sens confortable : chaque thread lit une
// ligne de W parfaitement contiguë.
//
// Le backward d'une projection GELÉE demande l'AUTRE sens :
//     dX[M,K] = dY[M,N] · W[N,K]       (somme sur N, la dimension SAUTÉE)
// Il n'y a AUCUN gradient de poids à calculer (le socle est gelé — c'est tout
// l'intérêt de LoRA), mais il faut bien faire traverser dX. Et ce produit-là ne
// se ramène pas au précédent : sommer sur N veut dire lire W[0,k], W[1,k],
// W[2,k]… c'est-à-dire un élément tous les K·(4 bits), donc un octet tous les
// 1 792 octets pour K = 3584. Naïvement, c'est une lecture par élément ET une
// relecture des métadonnées du super-bloc à chaque fois : ~10 opérations pour
// une multiplication.
//
// LA PARADE (même esprit qu'au jalon 4 : tuiler pour amortir la déquantification)
// ------------------------------------------------------------------------------
// Un groupe de travail = 8 lignes de dY (« 8 tokens ») × 32 valeurs de k
// CONSÉCUTIVES. Les 64 threads déquantifient coopérativement, pour 8 valeurs de
// n à la fois, la tranche W[n, k0..k0+31] dans 1 Ko de mémoire partagée ; les 8
// tokens la consomment tous. Le super-bloc n'est donc décodé qu'une fois par
// tuile de 8 tokens — exactement le même facteur d'amortissement que le noyau
// tuilé du forward, et le MÊME nombre total de déquantifications (K·M·N/8).
//
// POURQUOI LA TUILE FAIT 32 ET NON 256 : 32 est la granularité NATURELLE des
// deux formats — un sous-bloc Q4_K porte une paire (échelle, minimum) pour ses
// 32 valeurs, et un quadruplet Q6_K couvre 32 positions `l` à échelle et
// nibble fixés. Une tuile de 32 valeurs de k alignée sur 32 ne touche donc
// qu'UN jeu de métadonnées, lu une fois par thread. Une tuile de 256 aurait
// forcé 32 accumulateurs par thread (débordement de registres probable) pour
// aucun gain de trafic.
//
// K DOIT ÊTRE MULTIPLE DE 256 (c'est le format qui l'impose) et N MULTIPLE DE 8
// (la tuile de n). Les deux sont vrais pour toutes les projections de Qwen2.5 7B
// (N ∈ {512, 3584, 18944, 152064}) ; le cas contraire est REFUSÉ, jamais arrondi.
//
// CE FICHIER PORTE AUSSI les trois produits matriciels f32 « ordinaires » dont
// LoRA a besoin, dans les trois orientations, avec facteur d'échelle et
// accumulation optionnelle. POURQUOI ne pas prendre NkTensorGpu::RunMatMul :
// il n'expose qu'une orientation (A[M,K]·B[K,N]), sans alpha ni accumulation,
// et il travaille sur des NkTensor — or ici tout vit dans des tampons GPU bruts
// que l'appelant gère lui-même (le modèle 7B ne matérialise aucun NkTensor sur
// le chemin chaud). Trois noyaux de vingt lignes coûtent moins qu'un détour.
//
// Aucune ligne de NKTensor ni des jalons 4/5 n'est modifiée : tout passe par les
// deux lanceurs génériques publics RunConvOp / RunOp3, comme depuis le jalon 4.
//
// Zéro STL. Namespace ai::infer.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKInfer/NkQ4KGpu.h"
#include "NKInfer/NkQ6KGpu.h"

namespace nkentseu {
	namespace ai {
		namespace infer {

			// ---- Backward d'une projection GELÉE (le socle quantifié) -------------
			// dX[M,K] = dY[M,N] · W[N,K]. `accumulate` : dX += … au lieu de dX = …
			// (indispensable — dXn1 reçoit les contributions de wq, wk ET wv, et
			// dXn2 celles de wGate ET wUp ; sans accumulation il faudrait trois
			// tampons temporaires et deux additions de plus par couche).
			bool NkQ4KGpuMatmulT(const NkQ4KGpuWeight &w, uint64 dyBuffer, uint64 dxBuffer, int64 M, bool accumulate,
								 NkString *outError = nullptr);
			bool NkQ6KGpuMatmulT(const NkQ6KGpuWeight &w, uint64 dyBuffer, uint64 dxBuffer, int64 M, bool accumulate,
								 NkString *outError = nullptr);

			// ---- Produits matriciels f32 (LoRA, lm_head des adaptateurs…) ---------
			// Toutes : Y[M,N] = (accumulate ? Y : 0) + alpha · (produit).
			// Un thread par élément de sortie, n variant le plus vite (écriture de Y
			// coalescée).

			// Y[m,n] = Σ_k A[m,k]·B[n,k]   — B stocké [N,K] (convention « poids »).
			bool NkGpuMatmulABt(uint64 a, uint64 b, uint64 y, int64 M, int64 N, int64 K, float32 alpha,
								bool accumulate, NkString *outError = nullptr);
			// Y[m,n] = Σ_k A[m,k]·B[k,n]   — B stocké [K,N].
			bool NkGpuMatmulAB(uint64 a, uint64 b, uint64 y, int64 M, int64 N, int64 K, float32 alpha, bool accumulate,
							   NkString *outError = nullptr);
			// Y[m,n] = Σ_k A[k,m]·B[k,n]   — A stocké [K,M] (transposée NON matérialisée).
			bool NkGpuMatmulAtB(uint64 a, uint64 b, uint64 y, int64 M, int64 N, int64 K, float32 alpha, bool accumulate,
								NkString *outError = nullptr);

			// ---- Utilitaires de tampon -------------------------------------------
			// Mise à zéro. PAS un noyau : un `Upload` de zéros depuis le CPU, par
			// tranches. POURQUOI PAS UN SHADER — un noyau de remplissage n'a qu'un
			// seul tampon utile, alors que les deux lanceurs génériques en exigent
			// deux ou trois ; il faudrait soit lier le même tampon deux fois (légal
			// en Vulkan, pas garanti ailleurs), soit lire un tampon bidon que
			// l'optimiseur a le droit d'éliminer — et une liaison éliminée côté
			// shader mais toujours attendue côté descripteur est un bug silencieux.
			// Le coût réel est ridicule : 81 Mo de zéros par pas d'entraînement,
			// ~10 ms sur PCIe, contre ~10 s de calcul.
			bool NkGpuZeroBuffer(uint64 dst, uint64 bytes, NkString *outError = nullptr);

			// Y += alpha · X (le tampon de sortie est lu ET écrit — c'est le binding
			// 1 de RunConvOp, pas une aliasation).
			bool NkGpuAxpy(uint64 x, uint64 y, uint64 count, float32 alpha, NkString *outError = nullptr);
			// Somme des carrés d'un tampon, par blocs de 1024 éléments : sortie
			// `partials` de ceil(count/1024) floats, que l'appelant relit et somme.
			// POURQUOI pas une réduction complète sur GPU : la norme globale porte
			// sur ~20 M paramètres répartis dans 392 tampons distincts ; l'appelant
			// doit de toute façon parcourir la liste, autant qu'il additionne 392
			// petits vecteurs de partiels au passage.
			bool NkGpuSumSquares(uint64 x, uint64 partials, uint64 count, NkString *outError = nullptr);
			constexpr uint64 kNkGpuSumSquaresChunk = 1024;

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
