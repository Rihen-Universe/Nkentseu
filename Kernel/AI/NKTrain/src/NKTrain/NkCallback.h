// =============================================================================
// NKTrain/NkCallback.h — callbacks génériques pour la boucle d'entraînement
// (NKAI, Jalon 3 : « confort »).
// -----------------------------------------------------------------------------
// `NkTrainCallback` est appelée à des points FIXES d'une boucle générique
// (`train::Fit`, cf NkTrain.h) : avant/après entraînement, avant/après époque,
// avant/après lot. Toute callback peut demander l'arrêt (`StopRequested`), ce
// que vérifie `Fit` après CHAQUE époque.
//
// Implémentations fournies :
//   - NkEarlyStopping     : arrête si la métrique surveillée (perte val, ou perte
//                           train si pas de val) ne s'améliore plus après N époques.
//   - NkLRSchedulerCallback<Opt> : pilote `opt.SetLearningRate()` à chaque LOT via
//                           un planificateur (warmup+cosine, `NkLRSchedule`, déjà
//                           dans NkTrain.h) ou en décroissance par PALIERS
//                           (`NkStepDecaySchedule`, nouveau).
//   - NkLoggingCallback   : journalise perte/exactitude/perte-val via NKLogger.
//   - NkCheckpointCallback: sauvegarde périodique (modèle + Adam + état de boucle)
//                           via NkCheckpoint.h — permet la REPRISE après interruption.
//
// Zéro std::function : dispatch virtuel classique (interface + implémentations),
// pas de STL. Namespace : nkentseu::ai::train.
// =============================================================================
#pragma once

#include "NKTrain/NkCheckpoint.h" // NkTrainState (partagé par EarlyStopping/Checkpoint)
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkFunctions.h" // NkCos (NkLRSchedule, cosine)

namespace nkentseu {
	namespace ai {
		namespace train {

			// Statistiques d'une époque (définie ici — avant NkTrain.h — car les
			// callbacks la reçoivent dans `OnEpochEnd`; NkTrain.h l'utilise aussi).
			struct EpochStats {
					double loss = 0.0; // perte moyenne (pondérée par la taille des lots)
					double acc = 0.0;  // exactitude d'entraînement (argmax)
			};

			// -----------------------------------------------------------------
			// Interface appelée aux points fixes de la boucle d'entraînement.
			// Toutes les méthodes ont un corps par défaut vide : une callback ne
			// surcharge que ce dont elle a besoin.
			// -----------------------------------------------------------------
			class NkTrainCallback {
				public:
					virtual ~NkTrainCallback() {
					}

					virtual void OnTrainBegin() {
					}

					virtual void OnTrainEnd() {
					}

					virtual void OnEpochBegin(int64 epoch) {
						(void)epoch;
					}

					// `valLoss < 0` signifie « pas de jeu de validation fourni ».
					virtual void OnEpochEnd(int64 epoch, const EpochStats &stats, double valLoss) {
						(void)epoch;
						(void)stats;
						(void)valLoss;
					}

					virtual void OnBatchBegin(int64 globalStep) {
						(void)globalStep;
					}

					virtual void OnBatchEnd(int64 globalStep, double batchLoss) {
						(void)globalStep;
						(void)batchLoss;
					}

					// La boucle `Fit` s'arrête (proprement, fin d'époque) dès qu'UNE callback
					// de la liste renvoie true ici (vérifié après chaque `OnEpochEnd`).
					virtual bool StopRequested() const {
						return false;
					}
			};

			// -----------------------------------------------------------------
			// Early stopping RÉEL : surveille une métrique (perte de validation si
			// fournie, sinon perte d'entraînement) et arrête l'entraînement si elle
			// ne s'améliore pas de plus de `minDelta` pendant `patience` époques
			// consécutives.
			// -----------------------------------------------------------------
			class NkEarlyStopping : public NkTrainCallback {
				public:
					explicit NkEarlyStopping(int32 patience, double minDelta = 0.0)
						: mPatience(patience > 0 ? patience : 1), mMinDelta(minDelta) {
					}

					void OnEpochEnd(int64 epoch, const EpochStats &stats, double valLoss) override {
						const double metric = (valLoss >= 0.0) ? valLoss : stats.loss;
						if (metric < mBest - mMinDelta) {
							mBest = metric;
							mBad = 0;
						} else {
							++mBad;
						}
						mStop = mBad >= mPatience;
						if (mStop && mStoppedEpoch < 0)
							mStoppedEpoch = epoch;
					}

					bool StopRequested() const override {
						return mStop;
					}

					// Meilleure métrique vue jusqu'ici.
					double Best() const {
						return mBest;
					}

					// Nombre d'époques consécutives sans amélioration.
					int32 BadEpochs() const {
						return mBad;
					}

					// Époque à laquelle l'arrêt a été déclenché (-1 si jamais).
					int64 StoppedEpoch() const {
						return mStoppedEpoch;
					}

					// Restaure l'état interne depuis un checkpoint (reprise après interruption :
					// ne pas repartir de zéro sur le compteur de patience).
					void RestoreState(const NkTrainState &st) {
						mBest = st.bestMetric;
						mBad = st.badEpochs;
						mStop = false;
						mStoppedEpoch = -1;
					}

				private:
					int32 mPatience = 1;
					double mMinDelta = 0.0;
					double mBest = 1.0e300;
					int32 mBad = 0;
					bool mStop = false;
					int64 mStoppedEpoch = -1;
			};

			// -----------------------------------------------------------------
			// Planificateur LR : warmup linéaire (0 → pic sur `warmupSteps`) puis
			// décroissance cosine jusqu'au plancher `minLrRatio·peak` sur `totalSteps`.
			// Le pas est GLOBAL (reprise sans re-warmup). Repris tel quel depuis NkTrain.h
			// (Option A.1) : vit ici car `NkLRSchedulerCallback` (ci-dessous) en a besoin,
			// et NkTrain.h inclut CE fichier (éviter un cycle d'include).
			// -----------------------------------------------------------------
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

			// -----------------------------------------------------------------
			// Décroissance PAR PALIERS (alternative au cosine `NkLRSchedule` ci-dessus) :
			// lr(epoch) = peakLr * decayRate ^ floor(epoch / stepSize).
			// Le pas est en ÉPOQUES (pas en pas de gradient), plus lisible pour ce style.
			// -----------------------------------------------------------------
			struct NkStepDecaySchedule {
					float peakLr = 1e-3f;
					int32 stepSize = 10; // épreuves entre deux paliers
					double decayRate = 0.5;

					float LrAt(int64 epoch) const {
						if (epoch < 1)
							epoch = 1;
						const int32 ss = (stepSize > 0) ? stepSize : 1;
						const int64 k = (epoch - 1) / ss; // nb de paliers déjà franchis
						double factor = 1.0;
						for (int64 i = 0; i < k; ++i)
							factor *= decayRate;
						return (float)((double)peakLr * factor);
					}
			};

			// -----------------------------------------------------------------
			// Pilote le LR d'un optimiseur (Adam/SGD, tout type exposant
			// `SetLearningRate(float)`) à chaque LOT via `NkLRSchedule` (warmup +
			// cosine, déjà utilisé manuellement par `TrainEpochAccum`). Templée sur
			// Opt : dérive quand même de la base non-templée `NkTrainCallback`
			// (dispatch virtuel classique via `NkVector<NkTrainCallback*>`).
			// -----------------------------------------------------------------
			template <typename Opt> class NkLRSchedulerCallback : public NkTrainCallback {
				public:
					NkLRSchedulerCallback(Opt &opt, const NkLRSchedule &sched) : mOpt(&opt), mSched(sched) {
					}

					void OnBatchBegin(int64 globalStep) override {
						mOpt->SetLearningRate(mSched.LrAt(globalStep + 1));
					}

					float LastLr() const {
						return mLastLr;
					}

				private:
					Opt *mOpt = nullptr;
					NkLRSchedule mSched;
					mutable float mLastLr = 0.0f;
			};

			// Variante PAR PALIERS (mêmes points d'accroche, décroissance par époque).
			template <typename Opt> class NkStepDecayCallback : public NkTrainCallback {
				public:
					NkStepDecayCallback(Opt &opt, const NkStepDecaySchedule &sched) : mOpt(&opt), mSched(sched) {
					}

					void OnEpochBegin(int64 epoch) override {
						mOpt->SetLearningRate(mSched.LrAt(epoch));
					}

				private:
					Opt *mOpt = nullptr;
					NkStepDecaySchedule mSched;
			};

			// -----------------------------------------------------------------
			// Journalisation périodique (NKLogger, jamais printf) de la perte /
			// exactitude d'entraînement + perte de validation.
			// -----------------------------------------------------------------
			class NkLoggingCallback : public NkTrainCallback {
				public:
					explicit NkLoggingCallback(int32 every = 1) : mEvery(every > 0 ? every : 1) {
					}

					void OnEpochEnd(int64 epoch, const EpochStats &stats, double valLoss) override;

				private:
					int32 mEvery = 1;
			};

		} // namespace train
	} // namespace ai
} // namespace nkentseu
