// =============================================================================
// NKData/NkAugment.h — augmentation + découpage train/val/test (NKAI, Jalon 3 :
// robustesse).
// -----------------------------------------------------------------------------
// - `SplitDataset` : découpe un `NkDataset` en train/val/test (mélange déterministe
//   Fisher-Yates, même LCG que `NkDataLoader::Shuffle`) — pas de fuite (chaque
//   exemple appartient à EXACTEMENT une part).
// - `ConcatDatasets` : recolle deux jeux (même dimension/nb de classes) — sert à
//   composer original + augmenté en un seul jeu d'entraînement.
// - Augmentation IMAGE : `AugmentFlipHorizontal` (miroir gauche-droite d'un jeu
//   image-comme-vecteur [N, rows*cols], ex. MNIST) — préserve label/forme.
// - Augmentation NUMÉRIQUE générique : `AugmentGaussianNoise` (jitter gaussien
//   Box-Muller sur les features) — fonctionne sur N'IMPORTE QUEL NkDataset, pas
//   seulement des images.
// - Augmentation TEXTE (séquences d'identifiants, sortie BPE/NkVocab) :
//   `AugmentTokenDropout` — supprime aléatoirement des tokens (régularisation
//   classique façon word-dropout).
// Namespace : nkentseu::ai::data. Zéro STL.
// =============================================================================
#pragma once

#include "NKData/NkData.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// Résultat d'un découpage train/val/test.
			struct NkSplit {
					NkDataset train;
					NkDataset val;
					NkDataset test;
			};

			// Mélange (Fisher-Yates, seed déterministe) puis découpe `ds` en 3 parts selon
			// les fractions données (normalisées si leur somme != 1). Aucune fuite : chaque
			// exemple original apparaît dans EXACTEMENT une des 3 parts.
			NkSplit SplitDataset(const NkDataset &ds, double trainFrac, double valFrac, double testFrac,
								 uint32 seed = 1u);

			// Concatène deux jeux (même FeatureDim/NumClasses) en un seul NkDataset — sert à
			// composer un jeu d'entraînement « original + augmenté ». Renvoie `a` seul si `b`
			// est incompatible (dimension/nb de classes différents).
			NkDataset ConcatDatasets(const NkDataset &a, const NkDataset &b);

			// ---- Augmentation image ------------------------------------------------------
			// Miroir horizontal de chaque exemple, interprété comme une image [rows,cols]
			// (row-major, comme MNIST : FeatureDim() == rows*cols). Renvoie un NOUVEAU jeu de
			// même taille (labels inchangés) — à combiner avec l'original via ConcatDatasets.
			NkDataset AugmentFlipHorizontal(const NkDataset &ds, uint32 rows, uint32 cols);

			// ---- Augmentation numérique générique -----------------------------------------
			// Ajoute un bruit gaussien N(0, stddev²) (Box-Muller, LCG déterministe) à chaque
			// feature. Fonctionne sur n'importe quel NkDataset (pas seulement des images).
			NkDataset AugmentGaussianNoise(const NkDataset &ds, float stddev, uint32 seed = 1u);

			// ---- Augmentation texte (séquences d'identifiants) -----------------------------
			// Supprime aléatoirement chaque token avec probabilité `dropProb` (word-dropout).
			// Garde toujours AU MOINS un token (si la séquence d'entrée est non vide) pour
			// éviter une séquence vide dégénérée.
			NkVector<int32> AugmentTokenDropout(const NkVector<int32> &ids, float dropProb, uint32 seed = 1u);

		} // namespace data
	} // namespace ai
} // namespace nkentseu
