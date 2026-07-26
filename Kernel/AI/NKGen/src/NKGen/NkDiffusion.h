// =============================================================================
// NkDiffusion.h — DDPM (Denoising Diffusion Probabilistic Model) jouet (NKAI,
// Phase 6, Jalon 1 génératif — "diffusion / débruitage itératif").
//
// Source de l'algorithme (à suivre au pied de la lettre) :
//   Jonathan Ho, Ajay Jain, Pieter Abbeel, « Denoising Diffusion Probabilistic
//   Models », NeurIPS 2020. arXiv:2006.11239. https://arxiv.org/abs/2006.11239
//
// Principe (contrairement à l'auto-encodeur/VAE de ce module, qui compressent
// PUIS décodent) :
//   - Processus DIRECT (forward, fixe, pas appris) : on part d'un point x0 réel
//     et on ajoute du bruit gaussien pendant T pas jusqu'à obtenir du bruit pur.
//     Forme fermée (Ho et al. 2020, éq. 4) :
//       x_t = sqrt(ᾱ_t)·x0 + sqrt(1-ᾱ_t)·ε ,  ε ~ N(0,1)
//     où ᾱ_t = Π_{s=1..t} (1-β_s) est le produit cumulé d'un schedule de bruit
//     β_t (ici LINÉAIRE, comme le papier §4 / annexe B).
//   - Processus INVERSE (reverse, appris) : un petit MLP ε_θ(x_t, t) prédit le
//     bruit ajouté à l'étape t (paramétrisation « ε-prediction », Ho et al.
//     2020 §3.2 — plus stable à entraîner que prédire x0 directement ; loss L2
//     simple entre bruit prédit et bruit réel, éq. 14 du papier).
//   - Échantillonnage (Ho et al. 2020, Algorithme 2) : on part de x_T ~ N(0,1)
//     et on itère T fois la mise à jour reverse pour retrouver un point plausible
//     de la distribution d'origine.
//
// Choix d'implémentation (documentés, PAS le papier au sens strict — jouet 2D
// CPU) :
//   - Le pas de temps t est injecté dans le MLP via un embedding SINUSOÏDAL fixe
//     (même formule que l'encodage positionnel Transformer, Vaswani et al. 2017,
//     repris par Ho et al. 2020 pour conditionner sur t — cf TimeEmbedding ici).
//     Le point bruité [dataDim] et cet embedding [timeEmbedDim] sont concaténés
//     à la main (boucle C++, hors graphe autograd) au moment de construire le
//     tenseur d'entrée du MLP : NKAutograd n'offre qu'une concaténation sur
//     l'axe 0 (Concat0, cf NkVar.h) donc pas d'opérateur "axe features" — la
//     construction manuelle est l'option la plus simple à implémenter
//     correctement ici (les données sont de toute façon fabriquées à la main
//     dans ce module, cf NkMesh.cpp/NkVoxelVAE.cpp).
//   - Schedule linéaire adapté à un T réduit (T~100 au lieu de 1000 dans le
//     papier, budget CPU jouet) : β_T est agrandi (cf valeur par défaut) pour
//     que ᾱ_T → 0 malgré le nombre de pas réduit (sinon x_T ne serait pas du
//     bruit ~pur et l'échantillonnage partirait d'un point hors distribution).
//   - Variance de l'étape reverse : σ_t² = β_t (choix "large" du papier,
//     Algorithme 2 / §3.2, le plus simple des deux choix proposés).
//
// Conditionnement par CLASSE (Jalon 2 — guidage & contrôle) :
//   - Même schéma que l'embedding temporel : un vecteur ONE-HOT [numClasses]
//     (embedding FIXE, non appris — cf NkClassEmbedding) du label demandé est
//     concaténé À LA MAIN (même contrainte que pour t : pas d'opérateur de
//     concat "axe features" dans NKAutograd) à l'entrée du MLP, en plus du
//     point bruité et de l'embedding temporel. `numClasses = 0` (valeur par
//     défaut) reproduit exactement le comportement inconditionnel d'origine
//     (aucune colonne ajoutée).
//   - Choix du "label concaténé à l'entrée" : pattern classique de
//     conditionnement simple, cf Mirza & Osindero, « Conditional Generative
//     Adversarial Nets », 2014, arXiv:1411.1784 (concaténation d'un one-hot
//     de classe à l'entrée d'un générateur). PAS le « classifier-free
//     guidance » de Ho & Salimans, 2022, arXiv:2207.12598 (spécifique à la
//     diffusion) : ce papier entraîne le modèle avec un DROPOUT du label
//     (parfois non-conditionné) puis combine au sampling deux prédictions
//     (conditionnée et non-conditionnée) via une échelle de guidage `w` pour
//     renforcer l'adhérence au label. Ici : conditionnement DIRECT simple,
//     toujours avec label, sans dropout ni échelle de guidage — documenté
//     honnêtement comme une version simplifiée, pas une implémentation de CFG.
//
// Namespace : nkentseu::ai::gen.
// =============================================================================
#pragma once

#include "NKNN/NkNN.h"
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensor.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace gen {

			// ---------------------------------------------------------------------
			// NkDiffusionSchedule — constantes du bruit (β_t, α_t = 1-β_t, ᾱ_t =
			// cumprod(α_t)) précalculées pour T pas. Namespace-level : partagé par le
			// forward (QSample) et le reverse (NkDiffusion::Sample).
			// ---------------------------------------------------------------------
			struct NkDiffusionSchedule {
					NkVector<float> betas;		// β_t,  t = 0..T-1
					NkVector<float> alphas;	// α_t = 1 - β_t
					NkVector<float> alphaBars; // ᾱ_t = Π_{s<=t} α_s
					uint32 T = 0;

					// Schedule LINÉAIRE (Ho et al. 2020, §4) : β croît linéairement de
					// `betaStart` à `betaEnd` sur T pas. Valeurs par défaut adaptées à un T
					// réduit (voir commentaire de fichier) — PAS les 1e-4/0.02 du papier
					// (calibrées pour T=1000), agrandies pour que ᾱ_T → 0 avec moins de pas.
					static NkDiffusionSchedule Linear(uint32 T, float betaStart = 1e-4f, float betaEnd = 0.16f);
			};

			// Embedding sinusoidal fixe (NON appris) du pas de temps `t`, même formule
			// que l'encodage positionnel Transformer (Vaswani et al. 2017, "Attention
			// Is All You Need") : pour i=0..dim/2-1,
			//   out[2i]   = sin(t / 10000^(2i/dim))
			//   out[2i+1] = cos(t / 10000^(2i/dim))
			// (dim impair : le dernier élément reste à 0). Remplit `out[dim]`.
			void NkTimeEmbedding(uint32 t, uint32 dim, float *out);

				// Embedding de classe FIXE (NON appris), one-hot : out[numClasses] mis a
				// zero puis out[c]=1 (si c < numClasses). Meme role que NkTimeEmbedding
				// mais pour le LABEL de classe (cf commentaire de fichier - conditionnement
				// par concatenation, Mirza & Osindero 2014). Remplit `out[numClasses]`.
				void NkClassEmbedding(uint32 c, uint32 numClasses, float *out);

			// Processus direct (forward), forme fermée (Ho et al. 2020, éq. 4) :
			//   x_t = sqrt(ᾱ_t)·x0 + sqrt(1-ᾱ_t)·ε
			// `x0` [B,dataDim], `tIdx[b]` = pas de temps choisi pour l'exemple b
			// (0..T-1), `eps` [B,dataDim] = bruit déjà tiré ~N(0,1) (fourni par
			// l'appelant : c'est aussi la CIBLE de la loss L2, cf module d'appel).
			NkTensor NkDiffusionForward(const NkTensor &x0, const NkDiffusionSchedule &sched, const uint32 *tIdx,
										const NkTensor &eps);

			// ---------------------------------------------------------------------
			// NkDiffusion — réseau ε_θ(x_t, t) (processus reverse appris) + méthode
			// d'échantillonnage complète (Algorithme 2, Ho et al. 2020).
			//   MLP : Dense(dataDim+timeEmbedDim -> hidden) -> relu
			//         -> Dense(hidden -> hidden) -> relu
			//         -> Dense(hidden -> dataDim)              (sortie = bruit prédit,
			//                                                    pas d'activation finale :
			//                                                    le bruit n'est pas borné)
			// ---------------------------------------------------------------------
			class NkDiffusion {
				public:
					NkDiffusion() = default;
					NkDiffusion(uint32 dataDim, uint32 timeEmbedDim, uint32 hidden, uint32 seed = 1u, uint32 numClasses = 0u);

					// Prédit le bruit ε à partir d'une entrée déjà concaténée
					// [B, dataDim+timeEmbedDim] (cf BuildModelInput). -> [B, dataDim].
					NkVar PredictNoise(const NkVar &xtWithTime) const;

					// Construit l'entrée du MLP : concatène (boucle C++, PAS d'op autograd)
					// le point bruité `xt` [B,dataDim] avec l'embedding temporel sinusoidal
					// de chaque `tIdx[b]`. -> [B, dataDim+timeEmbedDim].
					// `classIdx` (optionnel, ignore si NumClasses()==0) : label [B] a
					// conditionner (concatene en one-hot, cf NkClassEmbedding). ->
					// [B, dataDim+timeEmbedDim+numClasses].
					NkTensor BuildModelInput(const NkTensor &xt, const uint32 *tIdx, const uint32 *classIdx = nullptr) const;

					void Parameters(NkVector<NkVar> &out) const;

					uint32 DataDim() const {
						return mDataDim;
					}

					uint32 TimeEmbedDim() const {
						return mTimeEmbedDim;
					}

					uint32 NumClasses() const {
						return mNumClasses;
					}

					// Échantillonnage complet (Ho et al. 2020, Algorithme 2) : part de
					// x_T ~ N(0,1) et itère T fois la mise à jour reverse
					//   x_{t-1} = (1/√α_t)·(x_t − (β_t/√(1-ᾱ_t))·ε_θ(x_t,t)) + σ_t·z
					// (z ~ N(0,1) si t>0, sinon z=0 ; σ_t = √β_t). Renvoie [count, dataDim].
					NkTensor Sample(uint32 count, const NkDiffusionSchedule &sched, uint32 rngSeed,
									const uint32 *classIdx = nullptr) const;

				private:
					nn::NkDense mL1, mL2, mL3;
					uint32 mDataDim = 0;
					uint32 mTimeEmbedDim = 0;
					uint32 mHidden = 0;
					uint32 mNumClasses = 0;
			};

		} // namespace gen
	} // namespace ai
} // namespace nkentseu
