// =============================================================================
// NkGradScaler.h — loss scaling dynamique (NKAI, mixed precision, brique 2/3).
//
// Pattern standard (Micikevicius et al., 2017 « Mixed Precision Training ») :
//   1. scaledLoss = loss * scale         (avant Backward)
//   2. scaledLoss.Backward()             (les gradients sont mis à l'échelle par `scale`
//                                          -> évite le flush-to-zero des petits gradients
//                                          en demi-précision)
//   3. Unscale(params) : divise chaque gradient par `scale` ; détecte un overflow
//      (Inf/NaN, symptôme classique d'une échelle trop grande) :
//        - overflow -> le pas d'optimiseur DOIT être sauté, `scale` recule (backoff)
//        - sinon    -> pas normal ; après `growthInterval` pas propres consécutifs,
//                      `scale` avance (growth) pour rester le plus haut possible sans
//                      déborder (maximise la plage dynamique utile en f16).
//
// Le graphe d'autodiff (NKAutograd) calcule aujourd'hui les gradients en F32 (CPU) ;
// ce scaler est déjà UTILE tel quel dans ce cas (stabilise les petits gradients avant
// qu'un futur backend f16 réel ne les flush-to-zero) et devient indispensable dès que
// les activations/gradients passeront en F16 (NkFp16, cf NkTensor/NkFp16.h). Namespace
// nkentseu::ai::optim, sans STL (NkVector, pas std::vector).
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace optim {

			class NkGradScaler {
				public:
					// initScale : échelle de départ (2^16 = 65536, valeur usuelle PyTorch AMP).
					// growthFactor/backoffFactor : facteurs multiplicatifs (croissance/recul).
					// growthInterval : nb de pas SANS overflow avant de retenter une échelle plus
					// grande. minScale : plancher (jamais en dessous, évite un flush total à 0).
					explicit NkGradScaler(float initScale = 65536.0f, float growthFactor = 2.0f,
										  float backoffFactor = 0.5f, int32 growthInterval = 2000,
										  float minScale = 1.0f);

					// Multiplie la perte par l'échelle courante (à appeler AVANT Backward()).
					NkVar Scale(const NkVar &loss) const;

					// Détecte un Inf/NaN dans un tenseur flottant (CPU ; passe par ToCPU() si GPU).
					// Balaie tous les dtypes flottants (F32/F64/F16) via NK_DTYPE_DISPATCH_FLOAT.
					static bool HasInfNan(const NkTensor &t);

					// À appeler APRÈS Backward(), AVANT opt.Step(). Parcourt `params`, détecte un
					// overflow dans un gradient quelconque :
					//   - overflow détecté  : gradients NON modifiés (l'appelant doit les remettre
					//     à zéro et NE PAS appeler Step() ce pas-ci), scale *= backoffFactor
					//     (plancher minScale), compteur de pas propres remis à 0, renvoie false.
					//   - pas d'overflow    : chaque gradient est divisé par `scale` EN PLACE
					//     (dé-mise à l'échelle), le compteur de pas propres avance ; au bout de
					//     `growthInterval` pas propres consécutifs, scale *= growthFactor (remet le
					//     compteur à 0), renvoie true (l'appelant peut appeler Step()).
					bool Unscale(const NkVector<NkVar> &params);

					float Scale() const {
						return mScale;
					}

					int32 GoodSteps() const {
						return mGoodSteps;
					}

					int32 OverflowCount() const {
						return mOverflowCount;
					}

				private:
					float mScale;
					float mGrowthFactor;
					float mBackoffFactor;
					int32 mGrowthInterval;
					float mMinScale;
					int32 mGoodSteps = 0;
					int32 mOverflowCount = 0; // diagnostic (nb total de pas sautés depuis la création)
			};

		} // namespace optim
	} // namespace ai
} // namespace nkentseu
