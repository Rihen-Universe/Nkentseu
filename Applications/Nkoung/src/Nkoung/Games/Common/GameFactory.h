// =============================================================================
// Games/Common/GameFactory.h
// Factory pattern pour instancier un jeu par son ID.
// =============================================================================
#pragma once

#ifndef NKOUNG_GAME_FACTORY_H
#define NKOUNG_GAME_FACTORY_H

#include "Games/Common/NkoungGame.h"
#include "Games/Common/GameMetadata.h"
#include "NKMemory/NkUniquePtr.h"

namespace nkoung {

    // =========================================================================
    // GameFactory — crée des instances de jeu
    // =========================================================================
    class GameFactory {
    public:
        /// Crée une instance du jeu correspondant à l'ID.
        /// @param id L'ID du jeu à instancier.
        /// @param allocator L'allocateur à utiliser (nullptr = mémoire système).
        /// @return NkUniquePtr<NkoungGame> vers le jeu créé, ou nullptr si erreur.
        static nkentseu::memory::NkUniquePtr<NkoungGame>
        CreateGame(GameId id, nkentseu::memory::NkAllocator* allocator = nullptr) noexcept;

        /// Retourne les infos du jeu à partir de son ID.
        /// @param id L'ID du jeu.
        /// @return const GameInfo* vers les infos, ou nullptr si invalide.
        static const GameInfo* GetGameInfo(GameId id) noexcept;

        /// Retourne le nombre total de jeux.
        static nkentseu::nk_uint32 GetGameCount() noexcept;

        /// Itère sur tous les jeux.
        /// @return const GameInfo* vers le tableau des infos (taille = GetGameCount()).
        static const GameInfo* GetAllGames() noexcept;
    };

}  // namespace nkoung

#endif // NKOUNG_GAME_FACTORY_H
