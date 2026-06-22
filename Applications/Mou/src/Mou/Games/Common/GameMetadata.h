// =============================================================================
// Games/Common/GameMetadata.h
// Énumérations et métadonnées partagées par les jeux Mú.
// =============================================================================
#pragma once

#ifndef MOU_GAME_METADATA_H
#define MOU_GAME_METADATA_H

#include "NKCore/NkTypes.h"

namespace mou {

    // Tous les jeux de la plateforme.
    enum class GameId : nkentseu::nk_int32 {
        Couleurs = 0,   // Reconnaître / trier les couleurs (3 ans+)
        Compter,        // Dénombrer 1 à 10 (4 ans+)
        Calcul,         // Additions / soustractions ≤ 10 (5 ans+)
        Formes,         // Encastrement des formes (3 ans+)
        Animaux,        // Reconnaître les animaux (3 ans+)
        Memoire,        // Retrouver les paires (4 ans+)
        Count
    };

    // Scène courante de la plateforme.
    enum class AppScene : nkentseu::nk_int32 {
        IntroRihen = 0, // logo studio Rihen
        IntroNoge,      // logo moteur Noge
        Splash,         // intro/accueil (mascotte)
        MainMenu,       // sélection de jeu
        Settings,       // réglages (volume musique / effets)
        LevelSelect,    // choix du niveau (à venir)
        GameScene,      // jeu en cours
        Count
    };

    // État de disponibilité d'un jeu.
    enum class GameStatus : nkentseu::nk_int32 {
        NotStarted = 0,
        Prototype,
        AlphaBuild,
        BetaBuild,
        Released,
        Archived
    };

    // Infos descriptives d'un jeu (pour les cartes du menu).
    struct GameInfo {
        GameId       id;
        const char*  title;
        const char*  subtitle;
        const char*  description;
        GameStatus   status;
        bool         playable;
    };

}  // namespace mou

#endif // MOU_GAME_METADATA_H
