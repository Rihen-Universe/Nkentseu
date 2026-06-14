// =============================================================================
// Games/Common/GameMetadata.h
// Énumérations et metadonnées partagées par tous les jeux Nkoung.
// =============================================================================
#pragma once

#ifndef NKOUNG_GAME_METADATA_H
#define NKOUNG_GAME_METADATA_H

#include "NKCore/NkTypes.h"

namespace nkoung {

    // =========================================================================
    // Énumération de tous les jeux disponibles
    // =========================================================================
    enum class GameId : nkentseu::nk_int32 {
        LaserPuzzle = 0,          // Puzzle avec laser et miroirs
        Territories,              // Stratégie au tour par tour
        Labyrinth,                // Top-down aventure/puzzle
        Bridges,                  // Puzzle de routes et ponts
        Flow,                     // Puzzle de flux/canalisation
        Tactics,                  // Stratégie plus lourde
        Count                     // Nombre total de jeux (pour boucles)
    };

    // =========================================================================
    // Scène actuelle de la plateforme
    // =========================================================================
    enum class AppScene : nkentseu::nk_int32 {
        PlatformMenu = 0,         // Sélection de jeu, menu principal
        GameScene,                // Jeu en cours
        Count
    };

    // =========================================================================
    // État de disponibilité d'un jeu
    // =========================================================================
    enum class GameStatus : nkentseu::nk_int32 {
        NotStarted = 0,           // Pas encore implémenté
        Prototype,                // Prototype jouable
        AlphaBuild,               // Jeu jouable mais incomplet
        BetaBuild,                // Quasi-complet, bugs possibles
        Released,                 // Version finale stable
        Archived                  // Ancienne version archivée
    };

    // =========================================================================
    // Informations descriptives d'un jeu
    // =========================================================================
    struct GameInfo {
        GameId id;
        const char* title;
        const char* subtitle;
        const char* description;
        GameStatus status;
        bool playable;  // Si false, le jeu n'est pas lanç able depuis le menu
    };

}  // namespace nkoung

#endif // NKOUNG_GAME_METADATA_H
