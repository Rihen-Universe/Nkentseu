// =============================================================================
// Games/Common/GameFactory.cpp
// Implémentation factory.
// =============================================================================
#include "Games/Common/GameFactory.h"
#include "Core/NkoungConfig.h"
#include "Games/Specific/LaserPuzzle/LaserPuzzleGame.h"
#include "Games/Specific/Labyrinth/LabyrinthGame.h"

namespace nkoung {

    using namespace nkentseu;   // int32, etc.

    // Table des infos de jeux. À compléter au fur et à mesure.
    static const GameInfo kGameInfoTable[] = {
        {
            GameId::LaserPuzzle,
            "Laser Puzzle",
            "Puzzle avec laser et miroirs",
            "Oriente des miroirs, prismes et portes pour guider un rayon lumineux vers une cible.",
            GameStatus::AlphaBuild,
            true
        },
        {
            GameId::Territories,
            "Territoires",
            "Strategie au tour par tour",
            "Capture des cases et bloque l'adversaire pour contrôler la carte.",
            GameStatus::NotStarted,
            false
        },
        {
            GameId::Labyrinth,
            "Gardien du Labyrinthe",
            "Aventure puzzle top-down",
            "Explore des salles courtes, evite les murs, ouvre la route et atteins la sortie.",
            GameStatus::AlphaBuild,
            true
        },
        {
            GameId::Bridges,
            "Ponts et Chemins",
            "Puzzle de tuiles",
            "Place des routes et des ponts pour connecter plusieurs villages.",
            GameStatus::NotStarted,
            false
        },
        {
            GameId::Flow,
            "Flux",
            "Puzzle de canalisation",
            "Canalise le flux entre sources et puits sans croisements.",
            GameStatus::NotStarted,
            false
        },
        {
            GameId::Tactics,
            "Tactique",
            "Strategie avancee",
            "Stratégie avec unités, carte dynamique et conditions de victoire variées.",
            GameStatus::NotStarted,
            false
        }
    };

    // =========================================================================
    // Implémentation GameFactory
    // =========================================================================

    nkentseu::memory::NkUniquePtr<NkoungGame>
    GameFactory::CreateGame(GameId id, nkentseu::memory::NkAllocator* allocator) noexcept {
        using namespace nkentseu::memory;

        // Toujours un allocateur valide (défaut NKMemory si non fourni).
        if (!allocator) {
            allocator = &NkGetDefaultAllocator();
        }

        NkoungGame* game = nullptr;

        switch (id) {
            case GameId::LaserPuzzle:
                game = allocator->New<LaserPuzzleGame>();   // alloue + construit via NKMemory
                break;

            case GameId::Labyrinth:
                game = allocator->New<LabyrinthGame>();
                break;

            case GameId::Territories:
            case GameId::Bridges:
            case GameId::Flow:
            case GameId::Tactics:
                NKOUNG_LOG_WARNF("Jeu %d non implémenté", static_cast<int32>(id));
                return {};

            case GameId::Count:
            default:
                NKOUNG_LOG_ERRORF("GameId invalide: %d", static_cast<int32>(id));
                return {};
        }

        if (game && !game->Init()) {
            NKOUNG_LOG_ERRORF("Échec Init pour jeu %d", static_cast<int32>(id));
            allocator->Delete(game);   // détruit + libère (symétrique de New)
            return {};
        }

        // Le deleter libère via le MÊME allocateur que l'allocation (symétrie alloc/free).
        return NkUniquePtr<NkoungGame>(game, NkDefaultDelete<NkoungGame>(allocator));
    }

    const GameInfo* GameFactory::GetGameInfo(GameId id) noexcept {
        const int32 idx = static_cast<int32>(id);
        if (idx < 0 || idx >= static_cast<int32>(GameId::Count)) {
            return nullptr;
        }
        return &kGameInfoTable[idx];
    }

    nkentseu::nk_uint32 GameFactory::GetGameCount() noexcept {
        return static_cast<nkentseu::nk_uint32>(GameId::Count);
    }

    const GameInfo* GameFactory::GetAllGames() noexcept {
        return kGameInfoTable;
    }

}  // namespace nkoung
