// =============================================================================
// NkDropout.h — couche de Dropout INVERSÉ (NKAI, régularisation).
//
// Absent du code jusqu'ici (audit 2026-07-26) : seule existait
// `NKData::AugmentTokenDropout`, une augmentation de DONNÉES au niveau
// tokenizer/corpus — pas une couche `nn::` insérée dans un forward pass.
//
// Dropout INVERSÉ (« inverted dropout », standard PyTorch/TF depuis ~2015) :
//   - En ENTRAÎNEMENT (IsTraining()==true) : chaque activation est mise à zéro
//     avec probabilité p (tirage Bernoulli i.i.d. par élément), et les
//     activations SURVIVANTES sont mises à l'échelle par 1/(1-p). Cette mise à
//     l'échelle au moment de l'entraînement (plutôt qu'à l'évaluation, comme
//     dans le dropout "classique") garantit que l'ESPÉRANCE de la sortie est
//     inchangée par rapport au mode évaluation.
//   - En ÉVALUATION (IsTraining()==false) : Forward est un NO-OP strict
//     (identité, aucune mise à l'échelle nécessaire grâce à l'inversion
//     ci-dessus) — comportement standard.
//
// Construite entièrement sur les primitives autograd EXISTANTES (Mul, MulScalar) :
// le masque Bernoulli est un NkTensor CONSTANT (non différentiable, comme les
// indices d'Embedding) multiplié élément par élément avec l'entrée -> le gradient
// est automatiquement bloqué où le masque vaut 0 et transmis à l'échelle ailleurs,
// exactement la dérivée attendue de la couche PyTorch équivalente. Aucune nouvelle
// primitive NkAutoOp n'était nécessaire.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"

namespace nkentseu {
	namespace ai {
		namespace nn {

			class NkDropout {
				public:
					NkDropout() = default;

					// `p` : probabilité de DROP (mettre à zéro) de chaque activation, dans [0,1].
					// `seed` : graine du LCG déterministe utilisé pour tirer les masques (des
					// masques DIFFÉRENTS sont tirés à chaque appel de Forward, la graine interne
					// avance à chaque tirage — reproductible pour une graine de départ donnée).
					explicit NkDropout(float p, uint32 seed = 1u) : mP(p), mSeed(seed ? seed : 1u) {
					}

					// Applique le dropout. NON-const : fait avancer le générateur interne à
					// chaque appel en mode entraînement (un nouveau masque à chaque forward).
					NkVar Forward(const NkVar &x) {
						if (!mTraining || mP <= 0.0f)
							return x; // évaluation OU p=0 : no-op strict, rien ajouté au graphe
						if (mP >= 1.0f)
							return autograd::MulScalar(x, 0.0); // cas dégénéré : tout est droppé

						NkTensor mask = MakeMask(x.Value().Shape());
						NkVar maskVar = NkVar::Leaf(mask, false); // masque constant, non différentiable
						NkVar dropped = autograd::Mul(x, maskVar);
						return autograd::MulScalar(dropped, 1.0 / (1.0 - (double)mP));
					}

					void SetTraining(bool training) {
						mTraining = training;
					}

					bool IsTraining() const {
						return mTraining;
					}

					float P() const {
						return mP;
					}

					// Pas de paramètre entraînable (couche purement fonctionnelle) — présent
					// pour uniformité avec les autres couches (Dense/Conv/...) dans NkSequential.
					void Parameters(NkVector<NkVar> & /*out*/) const {
					}

				private:
					// Tire un masque Bernoulli(1-p) (1.0 = gardé, 0.0 = droppé), F32, forme `shape`.
					NkTensor MakeMask(const NkShape &shape) {
						NkTensor m = NkTensor::Zeros(shape);
						float *mp = m.DataAs<float>();
						const int64 n = NkShapeNumel(shape);
						for (int64 i = 0; i < n; ++i) {
							mSeed = mSeed * 1664525u + 1013904223u;
							const double u = (double)((mSeed >> 8) & 0xFFFFu) / 65535.0; // [0,1]
							mp[i] = (u >= (double)mP) ? 1.0f : 0.0f;					  // P(garde) = 1-p
						}
						return m;
					}

					float mP = 0.5f;
					uint32 mSeed = 1u;
					bool mTraining = true; // mode par défaut = entraînement (comme PyTorch nn.Dropout)
			};

		} // namespace nn
	} // namespace ai
} // namespace nkentseu
