// =============================================================================
// NkTrain.h — boucle d'entraînement (NKAI, Phase 3).
//
// Assemble ce que produisent les autres modules : un modèle (fonction forward :
// entrée -> logits), une perte (entropie croisée), un optimiseur (NKOptim) et un
// flux de lots (NKData). `TrainEpoch` exécute forward → perte → backward → pas
// sur tous les lots d'une époque ; `Accuracy` évalue sans mise à jour.
//
// Générique par TEMPLATE sur le forward et l'optimiseur (pas de std::function,
// contrainte « sans STL ») : n'importe quel callable NkVar(const NkVar&) et tout
// optimiseur exposant Step() conviennent. Namespace : nkentseu::ai::train.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKNN/NkNN.h"
#include "NKData/NkData.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace train {

			// Statistiques d'une époque.
			struct EpochStats {
					double loss = 0.0; // perte moyenne (pondérée par la taille des lots)
					double acc = 0.0;  // exactitude d'entraînement (argmax)
			};

			// ---- Helpers compilés (NkTrain.cpp) -----------------------------
			// Exactitude d'un lot de logits [B,C] vs étiquettes [B] (argmax par ligne).
			uint32 CountCorrect(const NkTensor &logits, const NkVector<int32> &labels);

			// -----------------------------------------------------------------
			// Une époque d'entraînement de CLASSIFICATION (entropie croisée).
			// `forward` : callable NkVar(const NkVar& x) renvoyant les LOGITS [B,C].
			// `opt`     : optimiseur avec .Step() (ex. optim::NkAdam / NkSGD).
			// Remélange le loader en fin d'époque. Renvoie perte moyenne + exactitude.
			// -----------------------------------------------------------------
			template <typename Forward, typename Opt>
			EpochStats TrainEpoch(Forward &&forward, Opt &opt, data::NkDataLoader &loader) {
				double sumLoss = 0.0;
				uint32 correct = 0, total = 0;
				for (uint32 b = 0; b < loader.NumBatches(); ++b) {
					data::NkBatch batch = loader.GetBatch(b);
					if (batch.size == 0)
						continue;

					NkVar x = NkVar::Leaf(batch.inputs, false);
					NkVar t = NkVar::Leaf(batch.targets, false);
					NkVar logits = forward(x);
					NkVar loss = nn::CrossEntropyLoss(logits, t);

					loss.Backward();
					opt.Step();

					sumLoss += loss.Value().GetItem(NkShape{(int64)0}) * (double)batch.size;
					correct += CountCorrect(logits.Value(), batch.labels);
					total += batch.size;
				}
				loader.Shuffle(); // ordre différent à la prochaine époque

				EpochStats s;
				s.loss = (total > 0) ? sumLoss / (double)total : 0.0;
				s.acc = (total > 0) ? (double)correct / (double)total : 0.0;
				return s;
			}

			// -----------------------------------------------------------------
			// Exactitude d'un modèle sur tout un loader (aucune mise à jour).
			// -----------------------------------------------------------------
			template <typename Forward> double Accuracy(Forward &&forward, data::NkDataLoader &loader) {
				uint32 correct = 0, total = 0;
				for (uint32 b = 0; b < loader.NumBatches(); ++b) {
					data::NkBatch batch = loader.GetBatch(b);
					if (batch.size == 0)
						continue;
					NkVar x = NkVar::Leaf(batch.inputs, false);
					NkVar logits = forward(x);
					correct += CountCorrect(logits.Value(), batch.labels);
					total += batch.size;
				}
				return (total > 0) ? (double)correct / (double)total : 0.0;
			}

		} // namespace train
	} // namespace ai
} // namespace nkentseu
