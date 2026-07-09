// =============================================================================
// NkData.h — jeux de données et lots (NKAI, Phase 3).
//
// Transforme des exemples bruts (features + étiquettes) en LOTS de tenseurs prêts
// pour l'entraînement : mélange (shuffle), regroupement en batchs, one-hot des
// cibles. Fournit aussi un chargeur MNIST (format IDX). Ne dépend que de NKTensor.
// Namespace : nkentseu::ai::data.
// =============================================================================
#pragma once

#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace data {

			// -----------------------------------------------------------------
			// Jeu de données en mémoire : features X [N, D] (F32) + étiquettes de
			// classe [N] (indices 0..C-1). Le stockage de X est partagé (refcompté).
			// -----------------------------------------------------------------
			class NkDataset {
				public:
					NkDataset() = default;
					NkDataset(const NkTensor &features, const NkVector<int32> &labels, uint32 numClasses);

					uint32 Size() const {
						return mSize;
					}

					uint32 FeatureDim() const {
						return mDim;
					}

					uint32 NumClasses() const {
						return mNumClasses;
					}

					const NkTensor &Features() const {
						return mX;
					}

					const NkVector<int32> &Labels() const {
						return mY;
					}

					bool IsValid() const {
						return mSize > 0 && mDim > 0;
					}

				private:
					NkTensor mX;		// [N, D] F32 contigu
					NkVector<int32> mY; // [N] indices de classe
					uint32 mSize = 0;
					uint32 mDim = 0;
					uint32 mNumClasses = 0;
			};

			// Un lot prêt à consommer.
			struct NkBatch {
					NkTensor inputs;		// [B, D] F32
					NkTensor targets;		// [B, C] one-hot F32
					NkVector<int32> labels; // [B] indices (pour l'exactitude)
					uint32 size = 0;		// B (le dernier lot peut être plus petit)
			};

			// -----------------------------------------------------------------
			// Chargeur : découpe un jeu en lots. Mélange optionnel (Fisher-Yates,
			// LCG déterministe) ; appeler Shuffle() à chaque époque. Le dernier lot
			// peut être partiel (drop_last = false).
			// -----------------------------------------------------------------
			class NkDataLoader {
				public:
					NkDataLoader() = default;
					NkDataLoader(const NkDataset &ds, uint32 batchSize, bool shuffle = true, uint32 seed = 1u);

					uint32 NumBatches() const {
						return mNumBatches;
					}

					uint32 BatchSize() const {
						return mBatchSize;
					}

					void Shuffle();						  // remélange l'ordre des exemples
					NkBatch GetBatch(uint32 index) const; // index dans [0, NumBatches)

				private:
					NkDataset mDs;
					NkVector<int32> mOrder; // permutation des indices [0..N)
					uint32 mBatchSize = 1;
					uint32 mNumBatches = 0;
					bool mShuffle = true;
					uint32 mSeed = 1u;
			};

			// ---- Générateurs / chargeurs ------------------------------------
			// Jeu synthétique : `numClasses` amas gaussiens 2D bien séparés
			// (déterministe). Utile pour prouver le pipeline sans fichier externe.
			NkDataset MakeBlobs(uint32 numClasses, uint32 perClass, uint32 seed = 1u);

			// Chargeur MNIST (format IDX). Images -> [N, 784] normalisées [0,1] ;
			// labels -> indices 0..9. Renvoie un NkDataset invalide si lecture KO.
			NkDataset LoadMnist(const char *imagesPath, const char *labelsPath);

		} // namespace data
	} // namespace ai
} // namespace nkentseu
