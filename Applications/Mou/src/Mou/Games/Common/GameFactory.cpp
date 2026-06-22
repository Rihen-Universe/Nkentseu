// =============================================================================
// Games/Common/GameFactory.cpp
// =============================================================================
#include "Games/Common/GameFactory.h"
#include "Core/MouConfig.h"
#include "Games/Specific/Couleurs/CouleursGame.h"
#include "Games/Specific/Compter/CompterGame.h"
#include "Games/Specific/Calcul/CalculGame.h"
#include "Games/Specific/Formes/FormesGame.h"
#include "Games/Specific/Animaux/AnimauxGame.h"
#include "Games/Specific/Memoire/MemoireGame.h"

namespace mou {

    using namespace nkentseu;

    // Table des jeux (1 carte de menu par entrée).
    static const GameInfo kGameInfoTable[] = {
        {
            GameId::Couleurs,
            "Les Couleurs",
            "Reconnais et trie les couleurs",
            "Glisse chaque ballon dans le panier de sa couleur. Pour les 3 ans et plus.",
            GameStatus::Prototype,
            true
        },
        {
            GameId::Compter,
            "Compter",
            "Compte de 1 a 10",
            "Compte les objets puis touche le bon chiffre. Pour les 4 ans et plus.",
            GameStatus::Prototype,
            true
        },
        {
            GameId::Calcul,
            "Calculs",
            "Petits calculs amusants",
            "Additions et soustractions illustrees jusqu'a 10. Pour les 5 ans et plus.",
            GameStatus::Prototype,
            true
        },
        {
            GameId::Formes,
            "Les Formes",
            "Range les formes",
            "Glisse chaque forme dans son trou. Pour les 3 ans et plus.",
            GameStatus::Prototype,
            true
        },
        {
            GameId::Animaux,
            "Les Animaux",
            "Touche le bon animal",
            "Ecoute la consigne et touche le bon animal. Pour les 3 ans et plus.",
            GameStatus::Prototype,
            true
        },
        {
            GameId::Memoire,
            "La Memoire",
            "Retrouve les paires",
            "Retourne les cartes et retrouve les paires de fruits. Pour les 4 ans et plus.",
            GameStatus::Prototype,
            true
        }
    };

    nkentseu::memory::NkUniquePtr<MouGame>
    GameFactory::CreateGame(GameId id, nkentseu::memory::NkAllocator* allocator,
                            MouAssets* assets) noexcept {
        using namespace nkentseu::memory;
        if (!allocator) allocator = &NkGetDefaultAllocator();

        MouGame* game = nullptr;
        switch (id) {
            case GameId::Couleurs:
                game = allocator->New<CouleursGame>();
                break;

            case GameId::Compter:
                game = allocator->New<CompterGame>();
                break;

            case GameId::Calcul:
                game = allocator->New<CalculGame>();
                break;

            case GameId::Formes:
                game = allocator->New<FormesGame>();
                break;

            case GameId::Animaux:
                game = allocator->New<AnimauxGame>();
                break;

            case GameId::Memoire:
                game = allocator->New<MemoireGame>();
                break;

            case GameId::Count:
            default:
                MOU_LOG_ERRORF("GameId invalide: %d", static_cast<int32>(id));
                return {};
        }

        if (game) game->SetAssets(assets);  // assets dispo AVANT Init()
        if (game && !game->Init()) {
            MOU_LOG_ERRORF("Echec Init pour jeu %d", static_cast<int32>(id));
            allocator->Delete(game);
            return {};
        }
        return NkUniquePtr<MouGame>(game, NkDefaultDelete<MouGame>(allocator));
    }

    const GameInfo* GameFactory::GetGameInfo(GameId id) noexcept {
        const int32 idx = static_cast<int32>(id);
        if (idx < 0 || idx >= static_cast<int32>(GameId::Count)) return nullptr;
        return &kGameInfoTable[idx];
    }

    nkentseu::nk_uint32 GameFactory::GetGameCount() noexcept {
        return static_cast<nkentseu::nk_uint32>(GameId::Count);
    }

    const GameInfo* GameFactory::GetAllGames() noexcept {
        return kGameInfoTable;
    }

}  // namespace mou
