// =============================================================================
// NKEmbodied/NkEmbodiedActuatorLimits.h — limites actionneur (NKAI, Phase 6, Jalon 2).
//
// Décorateur GÉNÉRIQUE de NkActuator : enveloppe un actionneur concret
// quelconque et lui impose deux contraintes RÉALISTES avant de transmettre la
// commande :
//   1. SATURATION D'AMPLITUDE : chaque composante de `actions` est écrêtée
//      dans [-maxAmplitude, +maxAmplitude] (un vrai actionneur ne peut pas
//      recevoir une commande arbitrairement grande).
//   2. LIMITE DE FRÉQUENCE DE COMMANDE : au plus UNE commande NOUVELLE toutes
//      les `minTicksBetweenCommands` appels à Apply() — entre deux
//      acceptations, la DERNIÈRE commande acceptée est ré-appliquée telle
//      quelle (latence d'application, façon "zero-order hold" d'un vrai
//      contrôleur moteur qui n'échantillonne pas plus vite que sa fréquence
//      propre). `minTicksBetweenCommands=1` -> aucune limite (chaque appel
//      est accepté, comportement Jalon 1 inchangé).
// Ne réimplémente pas l'actionneur enveloppé : Apply() clippe/retarde PUIS
// délègue intégralement à l'actionneur interne. Expose les compteurs et le
// dernier état (saturé/retenu) pour vérification PAR ASSERTION (tests) et
// pour alimenter un NkEmbodiedSafetyMonitor (cf. NkEmbodiedSafety.h).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKEmbodied/NkActuator.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedLimitedActuator : public NkActuator {
				public:
					// `inner` non possédé (durée de vie gérée par l'appelant, qui doit
					// survivre à ce décorateur).
					NkEmbodiedLimitedActuator(NkActuator &inner, uint32 minTicksBetweenCommands, float maxAmplitude)
						: mInner(&inner), mMinTicksBetweenCommands(minTicksBetweenCommands ? minTicksBetweenCommands : 1u),
						  mMaxAmplitude(maxAmplitude) {
					}

					uint32 Dim() const override {
						return mInner->Dim();
					}

					void Apply(const NkVector<float> &actions) override {
						// 1. Saturation d'amplitude : écrête AVANT toute décision de
						// fréquence, sur la commande demandée ce tick.
						mLastAmplitudeSaturated = false;
						NkVector<float> clamped;
						clamped.Resize(actions.Size());
						for (nk_size i = 0; i < actions.Size(); ++i) {
							float v = actions[i];
							if (v > mMaxAmplitude) {
								v = mMaxAmplitude;
								mLastAmplitudeSaturated = true;
							} else if (v < -mMaxAmplitude) {
								v = -mMaxAmplitude;
								mLastAmplitudeSaturated = true;
							}
							clamped[i] = v;
						}

						// 2. Limite de fréquence : accepte la commande écrêtée SEULEMENT
						// si assez de ticks se sont écoulés depuis la dernière acceptée ;
						// sinon, ré-applique la dernière commande acceptée (latence).
						++mTicksSinceAccepted;
						const bool accept = !mHasAccepted || mTicksSinceAccepted >= mMinTicksBetweenCommands;
						mLastCommandHeld = !accept;
						if (accept) {
							mLastAccepted = clamped;
							mHasAccepted = true;
							mTicksSinceAccepted = 0;
							++mAcceptedCount;
						} else {
							++mHeldCount;
						}

						mInner->Apply(mLastAccepted);
					}

					// Vrai si la DERNIÈRE commande transmise à l'actionneur interne a été
					// écrêtée sur au moins une composante (saturation d'amplitude), au
					// dernier appel à Apply().
					bool LastAmplitudeSaturated() const {
						return mLastAmplitudeSaturated;
					}

					// Vrai si le DERNIER appel à Apply() a été rejeté par la limite de
					// fréquence (commande retenue = ré-application de la précédente).
					bool LastCommandHeld() const {
						return mLastCommandHeld;
					}

					uint32 AcceptedCount() const {
						return mAcceptedCount;
					}

					uint32 HeldCount() const {
						return mHeldCount;
					}

					// Commande RÉELLEMENT transmise à l'actionneur interne au dernier
					// appel (après écrêtage ET après limite de fréquence) — pour
					// vérification directe par assertion (tests), indépendamment de ce
					// que l'actionneur interne en fait ensuite.
					const NkVector<float> &LastForwardedCommand() const {
						return mLastAccepted;
					}

				private:
					NkActuator *mInner;
					uint32 mMinTicksBetweenCommands;
					float mMaxAmplitude;
					uint32 mTicksSinceAccepted = 0;
					bool mHasAccepted = false;
					bool mLastAmplitudeSaturated = false;
					bool mLastCommandHeld = false;
					uint32 mAcceptedCount = 0;
					uint32 mHeldCount = 0;
					NkVector<float> mLastAccepted;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
