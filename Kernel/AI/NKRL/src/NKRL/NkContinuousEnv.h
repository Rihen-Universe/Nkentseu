// =============================================================================
// NkContinuousEnv.h — interface d'environnement à ESPACES CONTINUS (NKAI, Phase 4,
// Jalon 3 : policy-gradient / PPO).
//
// Contrat minimal d'un monde à OBSERVATIONS et ACTIONS continues (vecteurs de
// float), par opposition à rl::NkEnvironment (Jalon 1/2, états/actions
// DISCRETS par indices, cf NkEnv.h). Utilisé par rl::NkPPO en mode
// ContinuousGaussian (cf NkPolicyNet.h). Namespace : nkentseu::ai::rl.
//
// Volontairement MONO-agent (Reset/Step manipulent UN SEUL vecteur d'état/
// action) : la preuve multi-agent (Jalon 3) utilise une classe d'environnement
// séparée (rl::NkReach2DMulti) qui avance PLUSIEURS agents dans le même monde
// en un seul appel Step — cf NkReach2DMulti.h pour la justification.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace rl {

			class NkContinuousEnv {
				public:
					virtual ~NkContinuousEnv() {
					}

					virtual uint32 StateDim() const = 0;  // dimension du vecteur d'observation
					virtual uint32 ActionDim() const = 0; // dimension du vecteur d'action

					// Bornes de l'espace d'action (l'environnement SATURE l'action reçue dans
					// Step à ces bornes ; la politique gaussienne elle-même reste non bornée,
					// cf NkPolicyNet.h — simplification standard, documentée dans ROADMAP.md).
					virtual float ActionLow(uint32 dim) const = 0;
					virtual float ActionHigh(uint32 dim) const = 0;

					// Réinitialise l'épisode ; remplit `state` (taille StateDim()).
					virtual void Reset(NkVector<float> &state) = 0;

					// Applique `action` (taille ActionDim(), saturée en interne aux bornes) ;
					// remplit `nextState`, la récompense et si l'épisode est terminé.
					virtual void Step(const NkVector<float> &action, NkVector<float> &nextState, float &reward,
									  bool &done) = 0;
			};

		} // namespace rl
	} // namespace ai
} // namespace nkentseu
