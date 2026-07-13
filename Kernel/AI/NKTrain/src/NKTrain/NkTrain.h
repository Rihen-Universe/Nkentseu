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
#include "NKMath/NkFunctions.h" // NkCos (scheduler LR cosine)

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


				// -----------------------------------------------------------------
				// Utilitaires d'entraînement RÉUTILISABLES (factorisés depuis
				// NkGptTrainer, Option A.1) — pour tout entraîneur (parole, gen, agents).
				// -----------------------------------------------------------------

				// Planificateur LR : warmup linéaire (0 → pic sur `warmupSteps`) puis
				// décroissance cosine jusqu'au plancher `minLrRatio·peak` sur `totalSteps`.
				// Le pas est GLOBAL (reprise sans re-warmup).
				struct NkLRSchedule {
						float peakLr = 1e-3f;
						int64 warmupSteps = 0;
						int64 totalSteps = 1;
						double minLrRatio = 0.1;

						float LrAt(int64 globalStep) const {
							if (globalStep < 1)
								globalStep = 1;
							if (warmupSteps > 0 && globalStep <= warmupSteps)
								return peakLr * (float)globalStep / (float)warmupSteps;
							const int64 horizon = (totalSteps > warmupSteps) ? totalSteps : (warmupSteps + 1);
							double prog = (double)(globalStep - warmupSteps) / (double)(horizon - warmupSteps);
							if (prog < 0.0)
								prog = 0.0;
							else if (prog > 1.0)
								prog = 1.0;
							const double cosv = 0.5 * (1.0 + (double)math::NkCos((float)(3.14159265358979323846 * prog)));
							return (float)(peakLr * (minLrRatio + (1.0 - minLrRatio) * cosv));
						}
				};

				// Loss par défaut : entropie croisée softmax sur cibles one-hot.
				struct NkCrossEntropyLoss {
						NkVar operator()(const NkVar &logits, const NkVar &targets) const {
							return nn::CrossEntropyLoss(logits, targets);
						}
				};

				// Scalaire de perte (ramené CPU si résident GPU).
				inline double LossScalar(const NkVar &loss) {
					return loss.Value().ToCPU().GetItem(NkShape{(int64)0});
				}

				// Une époque avec ACCUMULATION DE GRADIENT + scheduler LR optionnels et loss
				// CONFIGURABLE : accumule `accum` lots (perte ×1/accum) avant chaque `Step`.
				template <typename Forward, typename Opt, typename Loss = NkCrossEntropyLoss>
				EpochStats TrainEpochAccum(Forward &&forward, Opt &opt, data::NkDataLoader &loader, int32 accum,
										   const NkLRSchedule *sched = nullptr, int64 *globalStep = nullptr,
										   Loss &&lossFn = Loss{}) {
					if (accum < 1)
						accum = 1;
					double sumLoss = 0.0;
					uint32 correct = 0, total = 0;
					const uint32 nb = loader.NumBatches();
					uint32 b = 0;
					while (b < nb) {
						if (sched && globalStep)
							opt.SetLearningRate(sched->LrAt(*globalStep + 1));
						opt.ZeroGrad();
						for (int32 m = 0; m < accum && b < nb; ++m, ++b) {
							data::NkBatch batch = loader.GetBatch(b);
							if (batch.size == 0)
								continue;
							NkVar x = NkVar::Leaf(batch.inputs, false);
							NkVar t = NkVar::Leaf(batch.targets, false);
							NkVar logits = forward(x);
							NkVar loss = lossFn(logits, t);
							NkVar scaled = (accum > 1) ? autograd::MulScalar(loss, 1.0 / (double)accum) : loss;
							scaled.Backward();
							const double lv = LossScalar(loss);
							sumLoss += lv * (double)batch.size;
							correct += CountCorrect(logits.Value(), batch.labels);
							total += batch.size;
						}
						opt.Step();
						if (globalStep)
							++(*globalStep);
					}
					loader.Shuffle();
					EpochStats s;
					s.loss = (total > 0) ? sumLoss / (double)total : 0.0;
					s.acc = (total > 0) ? (double)correct / (double)total : 0.0;
					return s;
				}

				// Perte de VALIDATION (forward seul, aucun gradient) avec loss configurable.
				template <typename Forward, typename Loss = NkCrossEntropyLoss>
				double EvalLoss(Forward &&forward, data::NkDataLoader &loader, Loss &&lossFn = Loss{}) {
					double sumLoss = 0.0;
					uint32 total = 0;
					for (uint32 b = 0; b < loader.NumBatches(); ++b) {
						data::NkBatch batch = loader.GetBatch(b);
						if (batch.size == 0)
							continue;
						NkVar x = NkVar::Leaf(batch.inputs, false);
						NkVar t = NkVar::Leaf(batch.targets, false);
						NkVar logits = forward(x);
						NkVar loss = lossFn(logits, t);
						sumLoss += LossScalar(loss) * (double)batch.size;
						total += batch.size;
					}
					return (total > 0) ? sumLoss / (double)total : 0.0;
				}

				// Auto-test des utilitaires (schedule LR : warmup, pic, plancher, monotonie).
				bool SelfTest();

		} // namespace train
	} // namespace ai
} // namespace nkentseu
