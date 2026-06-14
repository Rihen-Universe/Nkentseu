// =============================================================================
// Games/Common/NkoungGameLevel.h
// Classe abstraite pour les niveaux (utile pour jeux multi-level).
// =============================================================================
#pragma once

#ifndef NKOUNG_GAME_LEVEL_H
#define NKOUNG_GAME_LEVEL_H

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkoung {

    // =========================================================================
    // NkoungGameLevel — classe de base pour les niveaux d'un jeu
    // =========================================================================
    class NkoungGameLevel {
    public:
        virtual ~NkoungGameLevel() = default;

        /// Charge le niveau depuis une source (fichier, données intégrées, etc.).
        /// @return true si succès.
        virtual bool Load() noexcept = 0;

        /// Décharge le niveau et libère les ressources.
        virtual void Unload() noexcept = 0;

        /// Retourne le titre du niveau (ex. "Level 1 - Tutorial").
        virtual const char* GetTitle() const noexcept = 0;

        /// Retourne true si le niveau est prêt à être joué.
        virtual bool IsValid() const noexcept = 0;

        /// ID unique du niveau.
        virtual nkentseu::nk_uint32 GetId() const noexcept = 0;
    };

}  // namespace nkoung

#endif // NKOUNG_GAME_LEVEL_H
