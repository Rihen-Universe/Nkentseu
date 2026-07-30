// =============================================================================
// NKEmbodied/NkEmbodiedPolicy.h — interface de DÉCISION du corps (NKAI, Phase 6, Jalon 1).
//
// La moitié « décision » de la boucle perception -> décision -> action
// (cf. NkEmbodiedLoop.h) : convertit les observations lues par un NkSensor
// (+ l'état brut du corps, pour brancher une politique tabulaire type NKAgent
// sans repasser par un encodage) en un indice d'action discrète. Interface
// SANS std::function (règle ZÉRO STL du projet) : chaque source de décision
// est une classe concrète dérivée, choisie à la construction de la boucle.
//
// Deux implémentations de repli fournies ici (aucune politique apprise
// nécessaire pour prouver que la boucle tourne) :
//   • NkEmbodiedRandomPolicy    — action uniforme aléatoire (LCG déterministe).
//   • NkEmbodiedHeuristicPolicy — GLOUTONNE sur les observations du capteur
//     (dx/dy signés vers le but) : la preuve la plus directe que "de vraies
//     lectures de capteurs influencent réellement les actions choisies" —
//     cette politique ne consulte JAMAIS l'état brut, seulement les features.
// La politique qui réutilise un cerveau NKAgent déjà entraîné/testé vit dans
// NkEmbodiedAgentPolicy.h (dépendance plus lourde, fichier séparé).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkEmbodiedPolicy {
				public:
					virtual ~NkEmbodiedPolicy() {
					}

					// Décide une action discrète à partir des observations DÉJÀ lues
					// par un NkSensor (`observations`) et, en repli, de l'état brut du
					// corps (`rawState`, utile aux politiques tabulaires type NKAgent).
					virtual uint32 Decide(const NkVector<float> &observations, uint32 rawState) = 0;
			};

			// Repli minimal : aucune lecture des observations, action uniforme
			// aléatoire dans [0, numActions). Déterministe (LCG dédié) pour des tests
			// reproductibles. Sert de témoin de base (baseline "sans intelligence").
			class NkEmbodiedRandomPolicy : public NkEmbodiedPolicy {
				public:
					NkEmbodiedRandomPolicy(uint32 numActions, uint32 seed = 1u)
						: mNumActions(numActions ? numActions : 1), mRng(seed ? seed : 1u) {
					}

					uint32 Decide(const NkVector<float> &observations, uint32 rawState) override {
						(void)observations;
						(void)rawState;
						mRng = mRng * 1664525u + 1013904223u;
						return (mRng >> 8) % mNumActions;
					}

				private:
					uint32 mNumActions;
					uint32 mRng;
			};

			// Politique HEURISTIQUE : décide UNIQUEMENT à partir des observations du
			// capteur (dx,dy signés vers le but, indices [2] et [3] de
			// NkEmbodiedGridSensor/agent::NkAgentPerception) — se déplace vers l'axe
			// portant le plus grand écart absolu, convention d'action rl::NkGridWorld
			// (0=haut 1=bas 2=gauche 3=droite). Aucune table apprise : preuve directe
			// que les capteurs, à eux seuls, suffisent à piloter le corps vers le but.
			class NkEmbodiedHeuristicPolicy : public NkEmbodiedPolicy {
				public:
					uint32 Decide(const NkVector<float> &observations, uint32 rawState) override {
						(void)rawState;
						if (observations.Size() < 4)
							return 0; // observations incompatibles : repli conservateur

						const float dx = observations[2];
						const float dy = observations[3];
						const float adx = dx < 0.0f ? -dx : dx;
						const float ady = dy < 0.0f ? -dy : dy;

						if (adx < 1e-6f && ady < 1e-6f)
							return 1; // déjà sur le but (ou aligné) : action arbitraire (rester stable)

						if (ady >= adx)
							return dy > 0.0f ? 1u : 0u; // priorité verticale : bas (dy>0) ou haut (dy<0)
						return dx > 0.0f ? 3u : 2u;		 // priorité horizontale : droite (dx>0) ou gauche (dx<0)
					}
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
