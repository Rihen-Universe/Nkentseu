// =============================================================================
// FICHIER: NKECS/GameObject/NkActor.cpp
// DESCRIPTION: Implémentations des hooks d'acteurs (Phase P3)
// =============================================================================
#include "NkActor.h"

namespace nkentseu {
    namespace ecs {

        // Les méthodes BeginPlay/Tick/EndPlay sont inline dans le header
        // car elles sont purement virtuelles par défaut et appellées via vtable
        // par le système de cycle de vie. Ce fichier sert de point d'ancrage
        // pour l'édition de liens et les futures spécialisations.

        // Hook pour débogage global des acteurs (optionnel)
        void NkActor::BeginPlay() noexcept {
            // Par défaut : rien
        }

        void NkActor::Tick(float32) noexcept {
            // Par défaut : rien
        }

        void NkActor::EndPlay() noexcept {
            // Par défaut : rien
        }

    } // namespace ecs
} // namespace nkentseu