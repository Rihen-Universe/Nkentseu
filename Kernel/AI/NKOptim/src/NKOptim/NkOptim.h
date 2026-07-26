// =============================================================================
// NkOptim.h — optimiseurs (NKAI, Phase 2).
//
// Un optimiseur détient une liste de paramètres (feuilles NkVar persistantes) et
// applique la règle de mise à jour à partir de leur gradient (rempli par
// NkVar::Backward). Il modifie les paramètres EN PLACE (NkVar::SetValue) pour que
// le prochain forward réutilise les poids mis à jour. Namespace : nkentseu::ai::optim.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace optim {

			// -----------------------------------------------------------------
			// Clipping de gradient (Pascanu, Mikolov & Bengio, « On the difficulty of
			// training Recurrent Neural Networks », ICML 2013 — arXiv:1211.5063).
			// Modifie les gradients de `params` EN PLACE (via NkVar::SetGrad), typiquement
			// juste AVANT le pas d'optimiseur (Step()).
			//   NK_CLIP_NONE           : désactivé (comportement par défaut, inchangé).
			//   NK_CLIP_BY_VALUE       : g_i = clamp(g_i, -clipValue, clipValue), élément
			//                            par élément.
			//   NK_CLIP_BY_GLOBAL_NORM : g_i *= min(1, clipValue / (‖g‖₂ + ε)) où ‖g‖₂ est
			//                            la norme L2 GLOBALE (tous paramètres confondus,
			//                            comme un unique vecteur concaténé) — préserve la
			//                            DIRECTION du gradient global, contrairement au
			//                            clipping par valeur.
			// -----------------------------------------------------------------
			enum class NkGradClipMode : uint8 {
				NK_CLIP_NONE = 0,
				NK_CLIP_BY_VALUE,
				NK_CLIP_BY_GLOBAL_NORM,
			};

			// Applique le clipping. Renvoie la norme L2 globale mesurée AVANT clipping
			// (0.0 en mode NK_CLIP_BY_VALUE ou NK_CLIP_NONE, ou si aucun paramètre n'a de
			// gradient valide). Fonction libre réutilisable indépendamment d'un optimiseur.
			double ClipGradients(NkVector<NkVar> &params, float clipValue, NkGradClipMode mode);

			// -----------------------------------------------------------------
			// SGD — descente de gradient stochastique, avec momentum optionnel.
			//   sans momentum : p ← p − lr·g
			//   avec momentum : v ← μ·v + g ; p ← p − lr·v
			// -----------------------------------------------------------------
			class NkSGD {
				public:
					NkSGD() = default;
					NkSGD(const NkVector<NkVar> &params, float lr, float momentum = 0.0f);

					void Step();	 // applique un pas sur tous les paramètres
					void ZeroGrad(); // remet à zéro les gradients des paramètres

					float LearningRate() const {
						return mLr;
					}

					void SetLearningRate(float lr) {
						mLr = lr;
					}

					// Active/désactive le clipping de gradient appliqué EN DÉBUT de Step(),
					// avant la mise à jour des paramètres. Désactivé par défaut (NK_CLIP_NONE)
					// -> comportement de Step() STRICTEMENT inchangé sans appel explicite.
					void SetGradClip(NkGradClipMode mode, float clipValue) {
						mClipMode = mode;
						mClipValue = clipValue;
					}

					NkGradClipMode GradClipMode() const {
						return mClipMode;
					}

					float GradClipValue() const {
						return mClipValue;
					}

				private:
					NkVector<NkVar> mParams;
					NkVector<NkTensor> mVelocity; // état momentum (par paramètre)
					float mLr = 0.01f;
					float mMomentum = 0.0f;
					NkGradClipMode mClipMode = NkGradClipMode::NK_CLIP_NONE;
					float mClipValue = 0.0f;
			};

			// -----------------------------------------------------------------
			// Adam — moments adaptatifs (Kingma & Ba, 2014).
			//   m ← β1·m + (1−β1)·g ;  v ← β2·v + (1−β2)·g²
			//   m̂ = m/(1−β1ᵗ) ;  v̂ = v/(1−β2ᵗ)
			//   p ← p − lr·m̂ / (√v̂ + ε)
			// -----------------------------------------------------------------
			class NkAdam {
				public:
					NkAdam() = default;
					// `weightDecay` > 0 -> AdamW (weight decay découplé).
					NkAdam(const NkVector<NkVar> &params, float lr = 0.001f, float beta1 = 0.9f, float beta2 = 0.999f,
						   float eps = 1e-8f, float weightDecay = 0.0f);

					void Step();
					void ZeroGrad();

					float LearningRate() const {
						return mLr;
					}

					void SetLearningRate(float lr) {
						mLr = lr;
					}

					// --- État interne (pour checkpoint / reprise parfaite du schedule) ---
					// Compteur de pas (correction de biais) + moments m/v par paramètre.

					int64 StepCount() const {
						return mT;
					}

					void SetStepCount(int64 t) {
						mT = t;
					}

					const NkVector<NkTensor> &FirstMoments() const {
						return mM;
					}

					const NkVector<NkTensor> &SecondMoments() const {
						return mV;
					}

					// Restaure les moments (déplacés sur le device de chaque paramètre).
					void SetMoments(const NkVector<NkTensor> &m, const NkVector<NkTensor> &v);

					// Active/désactive le clipping de gradient appliqué EN DÉBUT de Step(),
					// avant la mise à jour des moments/paramètres. Désactivé par défaut
					// (NK_CLIP_NONE) -> comportement de Step() STRICTEMENT inchangé sans
					// appel explicite (y compris la voie GPU fusée, qui n'est pas touchée).
					void SetGradClip(NkGradClipMode mode, float clipValue) {
						mClipMode = mode;
						mClipValue = clipValue;
					}

					NkGradClipMode GradClipMode() const {
						return mClipMode;
					}

					float GradClipValue() const {
						return mClipValue;
					}

				private:
					NkVector<NkVar> mParams;
					NkVector<NkTensor> mM; // 1er moment (par paramètre)
					NkVector<NkTensor> mV; // 2e moment
					float mLr = 0.001f;
					float mB1 = 0.9f;
					float mB2 = 0.999f;
					float mEps = 1e-8f;
					float mWd = 0.0f; // weight decay (AdamW si > 0)
					int64 mT = 0;	  // compteur de pas (pour la correction de biais)
					NkGradClipMode mClipMode = NkGradClipMode::NK_CLIP_NONE;
					float mClipValue = 0.0f;
			};

		} // namespace optim
	} // namespace ai
} // namespace nkentseu
