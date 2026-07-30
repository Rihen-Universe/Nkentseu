// =============================================================================
// NKTrain/NkCallback.cpp — parties non-templates des callbacks (NKAI).
// =============================================================================
#include "NKTrain/NkCallback.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace ai {
		namespace train {

			void NkLoggingCallback::OnEpochEnd(int64 epoch, const EpochStats &stats, double valLoss) {
				if (epoch % mEvery != 0)
					return;
				if (valLoss >= 0.0)
					logger.Info("époque {0} : perte train = {1} exactitude = {2}% perte val = {3}", epoch, stats.loss,
								stats.acc * 100.0, valLoss);
				else
					logger.Info("époque {0} : perte train = {1} exactitude = {2}%", epoch, stats.loss, stats.acc * 100.0);
			}

		} // namespace train
	} // namespace ai
} // namespace nkentseu
