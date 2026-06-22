// =============================================================================
// Games/Common/GameFactory.h
// Factory : instancie un jeu Mú par son ID.
// =============================================================================
#pragma once

#ifndef MOU_GAME_FACTORY_H
#define MOU_GAME_FACTORY_H

#include "Games/Common/MouGame.h"
#include "Games/Common/GameMetadata.h"
#include "NKMemory/NkUniquePtr.h"

namespace mou {

    class MouAssets;

    class GameFactory {
    public:
        static nkentseu::memory::NkUniquePtr<MouGame>
        CreateGame(GameId id, nkentseu::memory::NkAllocator* allocator = nullptr,
                   MouAssets* assets = nullptr) noexcept;

        static const GameInfo* GetGameInfo(GameId id) noexcept;
        static nkentseu::nk_uint32 GetGameCount() noexcept;
        static const GameInfo* GetAllGames() noexcept;
    };

}  // namespace mou

#endif // MOU_GAME_FACTORY_H
