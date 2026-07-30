// =============================================================================
// NkPolicyNet.h — réseau de politique (NKAI, Phase 4, Jalon 3 : PPO).
//
// MLP (Dense->Relu->Dense, via NKNN) qui produit SOIT des logits d'action
// DISCRETE (softmax appliqué par l'appelant / en interne selon la méthode),
// SOIT une distribution GAUSSIENNE (moyenne = sortie du MLP ; écart-type =
// paramètre appris SÉPARÉ, indépendant de l'état — convention standard des
// implémentations PPO continues de référence, ex. OpenAI Spinning Up /
// Baselines : un log-écart-type par dimension d'action, pas conditionné par
// l'état). Le MODE est fixé à la construction (pas de changement à chaud).
//
// Log-probabilité EXACTE (pas d'approximation) :
//   - discret : logSoftmax(logits)[action] = Log(Softmax(logits)) masqué par
//     un one-hot puis réduit par Sum -- utilise le nouvel opérateur
//     autograd::Log (ajouté pour ce Jalon 3, cf NKAutograd/NkVar.h) combiné à
//     autograd::Softmax (déjà différentiable, Jacobien complet).
//   - continu : densité gaussienne diagonale en forme fermée (algèbre directe
//     sur (a-μ)²·exp(-2·logσ) et -logσ, PAS de round-trip Log/Exp superflu) :
//       logp = Σ_d [ -0.5·(a_d-μ_d)²·exp(-2·logσ_d) - logσ_d ] - 0.5·D·log(2π)
//
// L'action ÉCHANTILLONNÉE (continu) N'EST PAS bornée/saturée par ce réseau —
// c'est à l'ENVIRONNEMENT de saturer l'action reçue (cf NkContinuousEnv::Step)
// avant de l'appliquer ; log-prob et entropie sont calculées sur l'action NON
// saturée. Simplification standard (PPO continu original, MuJoCo) documentée
// dans ROADMAP.md — une politique correctement bornée utiliserait une
// gaussienne "squashée" (tanh, cf SAC), hors scope de ce Jalon.
//
// Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKAutograd/NkVar.h"
#include "NKNN/NkDense.h"
#include "NKNN/NkActivations.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			enum class NkPolicyMode : uint8 { Discrete = 0, ContinuousGaussian = 1 };

			struct NkPolicyNetConfig {
					uint32 hiddenSize = 64; // largeur de la couche cachée du MLP
					float initLogStd = -0.5f; // continu seulement : log(σ) initial (σ0 = exp(-0.5) ≈ 0.61)
					uint32 seed = 1u;
					// Continu SEULEMENT : μ = actionScale·tanh(sortie brute du MLP) -- BORNE la
					// moyenne dans [-actionScale,actionScale]. Sans cette saturation, dans un espace
					// d'actions borné (l'environnement sature de toute façon l'action appliquée), le
					// gradient de politique n'a AUCUNE force de rappel une fois que μ dépasse la
					// plage utile (tout μ suffisamment extrême produit le MÊME comportement une fois
					// clampé par l'environnement) : μ dérive alors vers ±∞ au fil de l'entraînement
					// (observé empiriquement -- cf ROADMAP.md). Limite HONNÊTE documentée : ceci
					// borne seulement la MOYENNE, pas la gaussienne complète (contrairement à une
					// vraie politique "squashée" façon SAC, qui corrigerait aussi la densité de
					// probabilité par le jacobien de tanh) -- le bruit ajouté APRÈS le tanh peut
					// encore, plus rarement, sortir de [-actionScale,actionScale] (l'environnement
					// sature alors l'action reçue, cf NkContinuousEnv::Step).
					float actionScale = 1.0f;
			};

			class NkPolicyNet {
				public:
					NkPolicyNet(NkPolicyMode mode, uint32 inputDim, uint32 outputDim,
								const NkPolicyNetConfig &config = NkPolicyNetConfig());

					NkPolicyMode Mode() const {
						return mMode;
					}

					uint32 InputDim() const {
						return mInputDim;
					}

					// Discret : nombre d'actions. Continu : dimension de l'action.
					uint32 OutputDim() const {
						return mOutputDim;
					}

					// Sortie BRUTE du MLP : logits (discret, AVANT softmax) ou moyenne μ (continu).
					// `stateInput` : [1, InputDim()].
					NkVar Forward(const NkTensor &stateInput) const;

					// Log-écart-type appris (continu seulement) : [1, OutputDim()].
					const NkVar &LogStd() const {
						return mLogStd;
					}

					// Continu SEULEMENT (no-op en mode Discrete) : sature log(σ) dans [lo,hi] APRÈS
					// chaque pas d'optimiseur. Nécessaire car l'entropie gaussienne (cf Entropy())
					// est NON BORNÉE vers le haut : Σ logσ_d croît sans limite quand logσ augmente,
					// donc le bonus d'entropie PPO (qui la MAXIMISE) peut faire diverger σ vers
					// l'infini sans rien pour s'y opposer si le signal de politique est faible —
					// observé empiriquement (succès APRÈS entraînement pire qu'une politique
					// aléatoire, actions dominées par un bruit énorme) avant l'ajout de ce garde-fou.
					void ClampLogStd(float lo, float hi);

					// Échantillonne une action selon la politique COURANTE (exploration) :
					//   discret : outAction = {indice choisi} (taille 1, encodé en float).
					//   continu : outAction = vecteur échantillonné (taille OutputDim()).
					// `outLogProb` = log-probabilité de l'action choisie sous la politique courante
					// (valeur numérique, PAS de graphe de gradient conservé -- calculée via LogProb()
					// puis lue). RNG interne déterministe (LCG + Box-Muller pour le bruit gaussien).
					void SampleAction(const NkTensor &stateInput, NkVector<float> &outAction, float &outLogProb);

					// Action gloutonne (évaluation, SANS bruit) : argmax des probabilités (discret)
					// ou moyenne μ (continu).
					void GreedyAction(const NkTensor &stateInput, NkVector<float> &outAction) const;

					// Log-probabilité DIFFERENTIABLE de `action` sous l'état `stateInput` — c'est CE
					// graphe qui porte le gradient de l'objectif PPO jusqu'aux paramètres de la
					// politique. `action` : [1,1] (indice, discret, encodé en float comme
					// NkVar::SoftmaxCrossEntropyIndexed) ou [1,OutputDim()] (continu).
					NkVar LogProb(const NkTensor &stateInput, const NkTensor &action) const;

					// Entropie DIFFERENTIABLE de la distribution courante (bonus d'exploration PPO,
					// cf Schulman et al. 2017, terme S dans L^{CLIP+VF+S}).
					NkVar Entropy(const NkTensor &stateInput) const;

					// Ajoute les paramètres apprenables à `out` (W1,b1,W2,b2 [,logStd]).
					void Parameters(NkVector<NkVar> &out) const;

				private:
					float Rand01();	   // [0,1) déterministe (LCG)
					float RandNormal(); // N(0,1) déterministe (Box-Muller)

					NkPolicyMode mMode;
					uint32 mInputDim;
					uint32 mOutputDim;

					nn::NkDense mLayer1; // inputDim -> hidden
					nn::NkDense mLayer2; // hidden -> outputDim (logits ou moyenne)
					NkVar mLogStd;		 // continu seulement : feuille persistante [1,outputDim]
					float mActionScale;	 // continu seulement : cf NkPolicyNetConfig::actionScale

					uint32 mRng;
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
