// =============================================================================
// NkTrain.cpp — helpers compilés de la boucle d'entraînement (NKAI, Phase 3).
// =============================================================================
#include "NKTrain/NkTrain.h"
#include "NKTensor/NkTensorOps.h" // ops::Argmax (GPU-natif quand les logits résident)

namespace nkentseu {
	namespace ai {
		namespace train {

			uint32 CountCorrect(const NkTensor &logits, const NkVector<int32> &labels) {
				if (logits.Rank() < 2)
					return 0;
				const uint32 B = (uint32)logits.Shape()[0];
				// Argmax par ligne via l'API réduction : calculé sur GPU si les logits y
				// résident (seuls les B indices reviennent), sinon sur CPU. Sortie i64 [B].
				NkTensor pred = ops::Argmax(logits, 1);
				if (!pred.IsValid())
					return 0;
				uint32 correct = 0;
				for (uint32 i = 0; i < B && i < labels.Size(); ++i)
					if ((int32)pred.GetItem(NkShape{(int64)i}) == labels[i])
						++correct;
				return correct;
			}

			// -----------------------------------------------------------------
			// Auto-test des utilitaires réutilisables : propriétés du planificateur
			// LR (warmup linéaire, pic atteint, plancher cosine, monotonie décroissante).
			// Headless, aucun GPU ni loader requis.
			// -----------------------------------------------------------------
			bool SelfTest() {
				bool ok = true;

				NkLRSchedule sched;
				sched.peakLr = 1e-3f;
				sched.warmupSteps = 100;
				sched.totalSteps = 1000;
				sched.minLrRatio = 0.1;

				// (1) Warmup : croissance linéaire strictement croissante de ~0 au pic.
				const float lr1 = sched.LrAt(1);
				const float lr50 = sched.LrAt(50);
				const float lrPeak = sched.LrAt(sched.warmupSteps);
				if (!(lr1 > 0.0f && lr1 < lr50 && lr50 < lrPeak))
					ok = false;
				// Au pas de warmup, on doit tomber exactement sur le pic.
				if (math::NkFabs(lrPeak - sched.peakLr) > 1e-9f)
					ok = false;
				// Point médian du warmup : ~ moitié du pic (linéaire, 50/100).
				if (math::NkFabs(lr50 - 0.5f * sched.peakLr) > 1e-6f)
					ok = false;

				// (2) Décroissance cosine STRICTEMENT décroissante après le warmup.
				float prev = lrPeak;
				for (int64 g = sched.warmupSteps + 1; g <= sched.totalSteps; g += 50) {
					const float cur = sched.LrAt(g);
					if (cur > prev + 1e-9f) {
						ok = false;
						break;
					}
					prev = cur;
				}

				// (3) Plancher : au dernier pas, LR == minLrRatio * pic (cos(pi)=-1 → 0).
				const float lrEnd = sched.LrAt(sched.totalSteps);
				const float floor = (float)(sched.minLrRatio) * sched.peakLr;
				if (math::NkFabs(lrEnd - floor) > 1e-6f)
					ok = false;

				// (4) Au-delà de l'horizon : reste borné au plancher (pas de remontée).
				if (math::NkFabs(sched.LrAt(sched.totalSteps + 500) - floor) > 1e-6f)
					ok = false;

				// (5) Sans warmup : g=1 doit déjà être ~ pic (prog≈0 → cos(0)=1).
				NkLRSchedule noWarm;
				noWarm.peakLr = 2e-3f;
				noWarm.warmupSteps = 0;
				noWarm.totalSteps = 500;
				noWarm.minLrRatio = 0.05;
				if (noWarm.LrAt(1) <= 0.9f * noWarm.peakLr)
					ok = false;

				return ok;
			}

		} // namespace train
	} // namespace ai
} // namespace nkentseu
