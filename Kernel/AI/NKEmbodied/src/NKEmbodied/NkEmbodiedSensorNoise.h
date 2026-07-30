// =============================================================================
// NKEmbodied/NkEmbodiedSensorNoise.h — bruit capteur configurable (NKAI, Phase 6, Jalon 2).
//
// Décorateur GÉNÉRIQUE de NkSensor : enveloppe un capteur concret quelconque et
// AJOUTE du bruit (gaussien ou uniforme borné, configurable) à chaque
// composante du vecteur d'observations qu'il produit, avec une graine LCG
// déterministe (même seed -> même séquence de bruit -> résultats
// reproductibles d'un run à l'autre). Ne réimplémente rien du capteur
// enveloppé :
//   • Sense()           délègue à l'intérieur PUIS bruite le résultat.
//   • EstimateRawState() délègue tel quel au capteur interne, sur les
//     observations DÉJÀ bruitées fournies par l'appelant (cf.
//     NkEmbodiedLoop::EmbodiedTick) — c'est ce qui permet au bruit d'affecter
//     RÉELLEMENT l'état PERÇU transmis à une politique tabulaire (ex. NKAgent
//     via NkEmbodiedAgentPolicy, qui indexe par état et ignore
//     `observations`), pas seulement les observations brutes.
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include <cmath>

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKEmbodied/NkSensor.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			// Forme du bruit ajouté à chaque composante des observations.
			enum class NkEmbodiedNoiseKind {
				Uniform,  // uniforme borné, dans [-magnitude, +magnitude]
				Gaussian, // gaussien centré, écart-type = magnitude (Box-Muller)
			};

			class NkEmbodiedNoisySensor : public NkSensor {
				public:
					// `inner` non possédé (même convention que les autres capteurs/
					// actionneurs concrets du module) : sa durée de vie est gérée par
					// l'appelant, qui doit survivre à ce décorateur. `magnitude` : demi-
					// largeur (Uniform) ou écart-type (Gaussian), dans la même unité que
					// les observations du capteur enveloppé (ex. fraction de grille pour
					// NkEmbodiedGridSensor). `seed` : graine LCG déterministe — même seed
					// -> même séquence de bruit -> résultats reproductibles ; seeds
					// différentes -> séquences différentes.
					NkEmbodiedNoisySensor(const NkSensor &inner, NkEmbodiedNoiseKind kind, float magnitude,
										  uint32 seed = 1u)
						: mInner(&inner), mKind(kind), mMagnitude(magnitude), mRngState(seed ? seed : 1u) {
					}

					uint32 Dim() const override {
						return mInner->Dim();
					}

					// Délègue au capteur interne PUIS ajoute un bruit INDÉPENDANT sur
					// chaque composante (même distribution/magnitude pour toutes — pour
					// des bruits différents par composante, composer plusieurs
					// NkEmbodiedNoisySensor sur des sous-vecteurs, hors scope ici).
					void Sense(NkVector<float> &outObservations) const override {
						mInner->Sense(outObservations);
						for (nk_size i = 0; i < outObservations.Size(); ++i)
							outObservations[i] += NextNoiseSample();
					}

					// Délègue tel quel au capteur interne, sur les observations DÉJÀ
					// bruitées fournies par l'appelant (cf. Sense() ci-dessus, appelé
					// juste avant par NkEmbodiedLoop::EmbodiedTick) : c'est ce qui fait
					// que le bruit affecte réellement l'état perçu.
					bool EstimateRawState(const NkVector<float> &observations, uint32 &outState) const override {
						return mInner->EstimateRawState(observations, outState);
					}

				private:
					// Un flottant dans [0,1) via LCG 32 bits (mêmes constantes que
					// NkEmbodiedRandomPolicy, cf. NkEmbodiedPolicy.h) — déterministe,
					// mutable car Sense() est const (lecture pure du POINT DE VUE du
					// corps observé, mais le générateur de bruit interne avance).
					float NextUniform01() const {
						mRngState = mRngState * 1664525u + 1013904223u;
						const uint32 bits = (mRngState >> 8) & 0x00FFFFFFu;
						return (float)bits / (float)0x01000000u;
					}

					float NextNoiseSample() const {
						if (mKind == NkEmbodiedNoiseKind::Uniform)
							return (2.0f * NextUniform01() - 1.0f) * mMagnitude;

						// Gaussian : Box-Muller (une paire d'uniformes -> un normal(0,1)).
						float u1 = NextUniform01();
						if (u1 < 1e-7f)
							u1 = 1e-7f; // évite log(0)
						const float u2 = NextUniform01();
						const float twoPi = 6.28318530718f;
						const float z0 = sqrtf(-2.0f * logf(u1)) * cosf(twoPi * u2);
						return z0 * mMagnitude;
					}

					const NkSensor *mInner;
					NkEmbodiedNoiseKind mKind;
					float mMagnitude;
					mutable uint32 mRngState;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
