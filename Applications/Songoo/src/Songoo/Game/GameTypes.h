#pragma once
// =============================================================================
// GameTypes.h — Types, enums et configuration Songo'o
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu { namespace songoo {

    // ── Mode de jeu ──────────────────────────────────────────────────────────
    enum class GameMode {
        LocalPvP,    ///< Joueur 1 vs Joueur 2 (même machine)
        VsAI,        ///< Joueur vs IA
    };

    // ── État du plateau (capture) ─────────────────────────────────────────────
    enum class PriseStatus { NOT_PRISE = 0, PRISE = 1 };

    // ── Scènes du jeu ─────────────────────────────────────────────────────────
    enum class SceneId {
        RihenIntro,
        MainMenu,
        Story,
        Gameplay,
        GameOver,
        Options,
        Credits,
        Rules,
    };

    // ── Configuration utilisateur persistée ───────────────────────────────────
    struct GameSettings {
        GameMode mode         = GameMode::LocalPvP;
        bool  soundEnabled    = true;
        bool  musicEnabled    = true;
        bool  drumEnabled     = true;
        float musicVolume     = 0.10f;   // [0..1]
        float sfxVolume       = 1.00f;   // [0..1]
    };

    // ── Sons Songo'o ──────────────────────────────────────────────────────────
    enum class SoundId : uint8 {
        Pickup   = 0,  ///< Ramassage d'une graine
        Deposit  = 1,  ///< Dépôt dans un trou
        Drum     = 2,  ///< Tambour (changement de joueur)
        Score    = 3,  ///< Score / capture
        COUNT
    };

}} // namespace nkentseu::songoo
