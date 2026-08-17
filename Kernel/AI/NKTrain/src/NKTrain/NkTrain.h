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
#include "NKTrain/NkCallback.h" // EpochStats + NkTrainCallback (boucle Fit générique, callbacks)
#include "NKTrain/NkCheckpoint.h" // NkTrainState (reprise après interruption)

namespace nkentseu {
	namespace ai {
		namespace train {

			// ---- Helpers compilés (NkTrain.cpp) -----------------------------
			// Exactitude d'un lot de logits [B,C] vs étiquettes [B] (argmax par ligne).
			uint32 CountCorrect(const NkTensor &logits, const NkVector<int32> &labels);

			// =================================================================
			// ⚠️ OÙ VIDER LE GRADIENT — LA DISTINCTION À NE JAMAIS « HARMONISER »
			//
			// Depuis le 2026-08-16, `NkVar::Backward()` ne remet PLUS à zéro les
			// feuilles à gradient requis (les paramètres) : c'est l'appelant qui
			// vide, avec `ZeroGrad()`. Les trois boucles de ce fichier n'ont donc
			// PAS le même placement, et ce n'est pas une incohérence :
			//
			//   TrainEpoch       -> ZeroGrad UNE FOIS PAR LOT     (un pas = un lot)
			//   Fit              -> ZeroGrad UNE FOIS PAR LOT     (un pas = un lot)
			//   TrainEpochAccum  -> ZeroGrad UNE FOIS PAR LOT EFFECTIF,
			//                       c.-à-d. AVANT la boucle des micro-lots —
			//                       JAMAIS à l'intérieur.
			//
			// Poser un `ZeroGrad()` par micro-lot dans `TrainEpochAccum` annulerait
			// exactement ce que l'option A répare : chaque micro-lot effacerait le
			// précédent, `accum` ne servirait plus à rien, et le lot effectif comme
			// le taux seraient `accum` fois plus petits qu'annoncés — en silence.
			// =================================================================

			// -----------------------------------------------------------------
			// Une époque d'entraînement de CLASSIFICATION (entropie croisée).
			// `forward` : callable NkVar(const NkVar& x) renvoyant les LOGITS [B,C].
			// `opt`     : optimiseur avec .Step() (ex. optim::NkAdam / NkSGD).
			// Remélange le loader en fin d'époque. Renvoie perte moyenne + exactitude.
			// PAS d'accumulation ici : un lot = un pas -> ZeroGrad à chaque tour.
			// -----------------------------------------------------------------
			template <typename Forward, typename Opt>
			EpochStats TrainEpoch(Forward &&forward, Opt &opt, data::NkDataLoader &loader) {
				double sumLoss = 0.0;
				uint32 correct = 0, total = 0;
				for (uint32 b = 0; b < loader.NumBatches(); ++b) {
					data::NkBatch batch = loader.GetBatch(b);
					if (batch.size == 0)
						continue;

					opt.ZeroGrad(); // un lot = un pas : le gradient repart de zéro

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

				// NkLRSchedule (warmup+cosine) déplacé dans NkCallback.h (nécessaire à
				// NkLRSchedulerCallback, inclus PAR ce fichier — évite un cycle d'include).

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
						// ⚠️ ICI ET NULLE PART AILLEURS : une fois par LOT EFFECTIF, donc
						// AVANT la boucle des micro-lots. Le descendre d'un cran (dans le
						// `for m`) ferait effacer chaque micro-lot par le suivant et
						// viderait `accum` de tout sens. Cf. le bloc en tête de fichier.
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

				// -----------------------------------------------------------------
				// Métriques de VALIDATION génériques : perte + exactitude en UN SEUL passage
				// (évite de reparcourir le loader deux fois comme EvalLoss+Accuracy séparés).
				// -----------------------------------------------------------------
				struct EvalMetrics {
						double loss = 0.0;
						double acc = 0.0;
				};

				template <typename Forward, typename Loss = NkCrossEntropyLoss>
				EvalMetrics Evaluate(Forward &&forward, data::NkDataLoader &loader, Loss &&lossFn = Loss{}) {
					double sumLoss = 0.0;
					uint32 correct = 0, total = 0;
					for (uint32 b = 0; b < loader.NumBatches(); ++b) {
						data::NkBatch batch = loader.GetBatch(b);
						if (batch.size == 0)
							continue;
						NkVar x = NkVar::Leaf(batch.inputs, false);
						NkVar t = NkVar::Leaf(batch.targets, false);
						NkVar logits = forward(x);
						NkVar loss = lossFn(logits, t);
						sumLoss += LossScalar(loss) * (double)batch.size;
						correct += CountCorrect(logits.Value(), batch.labels);
						total += batch.size;
					}
					EvalMetrics m;
					m.loss = (total > 0) ? sumLoss / (double)total : 0.0;
					m.acc = (total > 0) ? (double)correct / (double)total : 0.0;
					return m;
				}

				// -----------------------------------------------------------------
				// BOUCLE D'ENTRAÎNEMENT GÉNÉRIQUE PILOTÉE PAR CALLBACKS (Jalon 3).
				// Exécute les époques [fromEpoch, toEpoch] (INCLUS) sur `trainLoader`,
				// appelant les callbacks aux points fixes définis par `NkTrainCallback`
				// (avant/après entraînement, avant/après époque, avant/après lot).
				// S'arrête PLUS TÔT (fin d'époque) dès qu'une callback demande l'arrêt
				// (`StopRequested()` — ex. NkEarlyStopping). `valLoader` optionnel (nullptr
				// => `valLoss` transmis aux callbacks vaut -1). `globalStep` optionnel :
				// compteur de pas de gradient PERSISTANT (repris depuis un checkpoint pour
				// une reprise fidèle du LR schedule, cf NkLRSchedulerCallback).
				// `fromEpoch` > 1 => REPRISE après interruption (avec `globalStep` restauré
				// depuis `NkTrainState::globalStep` et les callbacks depuis leur propre état,
				// ex. `NkEarlyStopping::RestoreState`).
				// -----------------------------------------------------------------
				template <typename Forward, typename Opt, typename Loss = NkCrossEntropyLoss>
				EpochStats Fit(Forward &&forward, Opt &opt, data::NkDataLoader &trainLoader,
							   data::NkDataLoader *valLoader, int64 fromEpoch, int64 toEpoch,
							   NkVector<NkTrainCallback *> &callbacks, int64 *globalStep = nullptr,
							   Loss &&lossFn = Loss{}) {
					EpochStats last;
					for (uint32 i = 0; i < callbacks.Size(); ++i)
						callbacks[i]->OnTrainBegin();

					bool stop = false;
					for (int64 epoch = fromEpoch; epoch <= toEpoch && !stop; ++epoch) {
						for (uint32 i = 0; i < callbacks.Size(); ++i)
							callbacks[i]->OnEpochBegin(epoch);

						double sumLoss = 0.0;
						uint32 correct = 0, total = 0;
						const uint32 nb = trainLoader.NumBatches();
						for (uint32 b = 0; b < nb; ++b) {
							const int64 gs = globalStep ? *globalStep : (int64)b;
							for (uint32 i = 0; i < callbacks.Size(); ++i)
								callbacks[i]->OnBatchBegin(gs);

							data::NkBatch batch = trainLoader.GetBatch(b);
							if (batch.size == 0)
								continue;

							opt.ZeroGrad(); // un lot = un pas (voir le bloc en tête de fichier)

							NkVar x = NkVar::Leaf(batch.inputs, false);
							NkVar t = NkVar::Leaf(batch.targets, false);
							NkVar logits = forward(x);
							NkVar loss = lossFn(logits, t);
							loss.Backward();
							opt.Step();

							const double lv = LossScalar(loss);
							sumLoss += lv * (double)batch.size;
							correct += CountCorrect(logits.Value(), batch.labels);
							total += batch.size;

							for (uint32 i = 0; i < callbacks.Size(); ++i)
								callbacks[i]->OnBatchEnd(gs, lv);
							if (globalStep)
								++(*globalStep);
						}
						trainLoader.Shuffle();
						last.loss = (total > 0) ? sumLoss / (double)total : 0.0;
						last.acc = (total > 0) ? (double)correct / (double)total : 0.0;

						double valLoss = -1.0;
						if (valLoader)
							valLoss = EvalLoss(forward, *valLoader, lossFn);

						for (uint32 i = 0; i < callbacks.Size(); ++i)
							callbacks[i]->OnEpochEnd(epoch, last, valLoss);
						for (uint32 i = 0; i < callbacks.Size(); ++i)
							if (callbacks[i]->StopRequested()) {
								stop = true;
								break;
							}
					}

					for (uint32 i = 0; i < callbacks.Size(); ++i)
						callbacks[i]->OnTrainEnd();
					return last;
				}

				// Auto-test des utilitaires (schedule LR : warmup, pic, plancher, monotonie).
				bool SelfTest();

		} // namespace train
	} // namespace ai
} // namespace nkentseu
