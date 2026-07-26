// =============================================================================
// NKEmbodied/NkActuator.h — abstraction générique d'ACTIONNEUR (NKAI, Phase 6, Jalon 1).
//
// Contrat minimal d'un actionneur : recevoir un vecteur d'actions (CONTINUES
// ou DISCRÈTES, l'actionneur concret décide du sens à leur donner) et les
// APPLIQUER à l'état d'un corps (simulé ou, plus tard via Kernel/Bare, réel).
// Deux encodages usuels supportés par convention (chaque actionneur concret
// documente celui qu'il attend) :
//   • un seul flottant  -> indice d'action discrète (ex. [2.0] = action 2) ;
//   • N flottants       -> scores/logits par action discrète (argmax) ou
//                          commandes continues directes (une par degré de
//                          liberté, ex. couple moteur gauche/droite).
// Ne connaît ni le capteur en face ni la politique qui a choisi ces actions :
// NkActuator se contente d'AGIR. C'est la moitié « action » de la boucle
// perception -> décision -> action du Jalon 1 (cf. NkEmbodiedLoop.h).
// Namespace : nkentseu::ai::embodied.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace ai {
		namespace embodied {

			class NkActuator {
				public:
					virtual ~NkActuator() {
					}

					// Nombre d'actions/degrés de liberté attendus dans `actions` (Apply).
					virtual uint32 Dim() const = 0;

					// Applique `actions` à l'état du corps commandé (effet de bord).
					virtual void Apply(const NkVector<float> &actions) = 0;
			};

		} // namespace embodied
	} // namespace ai
} // namespace nkentseu
