// =============================================================================
// NkArchitectures.h — architectures PRÊTES À L'EMPLOI (NKAI) : NkMLP, NkCNN.
//
// Absent du code jusqu'ici (audit 2026-07-26) : seules les BRIQUES (NkDense,
// NkConv2D, MaxPool2D, Flatten) existaient ; chaque app (NKNNTest, NKConvTest,
// NKTrainTest...) recomposait son MLP/CNN "à la main" dans son propre forward.
// `nn::NkGPT` (`NkTransformer.h`) était la SEULE architecture assemblée en une
// classe réutilisable — ce fichier généralise le même principe au MLP et au CNN,
// au-dessus du conteneur générique `NkSequential` (`NkSequential.h`).
// =============================================================================
#pragma once

#include "NKNN/NkSequential.h"
#include "NKNN/NkActivations.h"
#include "NKNN/NkConv.h" // nn::MaxPool2D / nn::Flatten (fonctions libres)
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace nn {

			// -------------------------------------------------------------------
			// NkMLP — perceptron multicouche prêt à l'emploi.
			//   layerSizes = [in, hidden1, ..., hiddenK, out]  (>= 2 entrées).
			//   Dense -> ReLU -> [Dropout] entre chaque paire de couches CACHÉES ;
			//   la DERNIÈRE couche (-> out) est un Dense NU (logits bruts, sans
			//   activation) — laisse l'appelant choisir Sigmoid/Softmax/CrossEntropy
			//   selon la tâche, comme le reste de NKNN (cf. NkLosses.h).
			//   `dropoutP` = 0 (défaut) désactive le dropout (aucune couche ajoutée).
			// -------------------------------------------------------------------
			class NkMLP {
				public:
					NkMLP() = default;

					NkMLP(const NkVector<uint32> &layerSizes, float dropoutP = 0.0f, uint32 seed = 1u) {
						const uint32 n = (uint32)layerSizes.Size();
						for (uint32 i = 0; i + 1 < n; ++i) {
							const bool isLast = (i + 2 == n);
							mSeq.AddDense(NkDense(layerSizes[i], layerSizes[i + 1], seed + i * 97u + 1u));
							if (!isLast) {
								mSeq.AddFn([](const NkVar &x) { return nn::Relu(x); });
								if (dropoutP > 0.0f)
									mSeq.AddDropout(NkDropout(dropoutP, seed + i * 131u + 7u));
							}
						}
					}

					NkVar Forward(const NkVar &x) {
						return mSeq.Forward(x);
					}

					void Parameters(NkVector<NkVar> &out) const {
						mSeq.Parameters(out);
					}

					// Bascule Dropout en mode entraînement/évaluation (no-op si dropoutP==0,
					// aucune couche Dropout n'a été ajoutée).
					void SetTraining(bool training) {
						mSeq.SetTraining(training);
					}

					NkSequential &Layers() {
						return mSeq;
					}

				private:
					NkSequential mSeq;
			};

			// -------------------------------------------------------------------
			// NkCNN — réseau convolutionnel prêt à l'emploi (classification d'images).
			//   convChannels = [Cin, C1, C2, ...] (>= 2 : canal d'entrée + >=1 bloc conv).
			//   Chaque bloc : Conv2D(k, pad="same") -> ReLU -> [MaxPool2D(2,2) si pool].
			//   Puis Flatten -> Dense(-> numClasses) (logits bruts, comme NkMLP).
			//   `inputHW` = hauteur=largeur de l'image d'entrée (carrée) ; nécessaire
			//   pour calculer la taille aplatie avant la couche Dense finale (chaque
			//   MaxPool2D(2,2) divise H,W par 2).
			// -------------------------------------------------------------------
			class NkCNN {
				public:
					NkCNN() = default;

					NkCNN(const NkVector<uint32> &convChannels, uint32 kernel, uint32 inputHW, uint32 numClasses,
						  bool pool = true, uint32 seed = 1u) {
						uint32 hw = inputHW;
						const uint32 pad = kernel / 2; // "same" pour un noyau impair (3->1, 5->2, ...)
						const uint32 n = (uint32)convChannels.Size();
						for (uint32 i = 0; i + 1 < n; ++i) {
							mSeq.AddConv2D(
								NkConv2D(convChannels[i], convChannels[i + 1], kernel, 1, pad, seed + i * 53u + 3u));
							mSeq.AddFn([](const NkVar &x) { return nn::Relu(x); });
							if (pool) {
								mSeq.AddFn([](const NkVar &x) { return nn::MaxPool2D(x, 2, 2); });
								hw /= 2;
							}
						}
						mSeq.AddFn([](const NkVar &x) { return nn::Flatten(x); });
						const uint32 lastC = convChannels[n - 1];
						mSeq.AddDense(NkDense(lastC * hw * hw, numClasses, seed + 999u));
					}

					NkVar Forward(const NkVar &x) {
						return mSeq.Forward(x);
					}

					void Parameters(NkVector<NkVar> &out) const {
						mSeq.Parameters(out);
					}

					void SetTraining(bool training) {
						mSeq.SetTraining(training);
					}

					NkSequential &Layers() {
						return mSeq;
					}

				private:
					NkSequential mSeq;
			};

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
