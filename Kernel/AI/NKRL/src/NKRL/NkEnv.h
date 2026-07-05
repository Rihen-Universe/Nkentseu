// =============================================================================
// NkEnv.h — interface d'environnement d'apprentissage par renforcement (NKAI, Phase 4).
//
// Contrat minimal d'un monde où un agent agit : réinitialiser, faire un pas
// (action -> état suivant, récompense, terminal). États et actions DISCRETS
// (indices) pour le Q-learning tabulaire du Jalon 1. Namespace : nkentseu::ai::rl.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace ai {
        namespace rl {

            class NkEnvironment {
            public:
                virtual ~NkEnvironment() {}

                virtual uint32 NumStates()  const = 0;   // taille de l'espace d'états
                virtual uint32 NumActions() const = 0;   // taille de l'espace d'actions

                // Réinitialise l'épisode et renvoie l'état initial.
                virtual uint32 Reset() = 0;

                // Applique `action` : renvoie l'état suivant, la récompense et si
                // l'épisode est terminé.
                virtual void Step(uint32 action, uint32& nextState,
                                  float& reward, bool& done) = 0;
            };

        } // namespace rl
    } // namespace ai
} // namespace nkentseu
