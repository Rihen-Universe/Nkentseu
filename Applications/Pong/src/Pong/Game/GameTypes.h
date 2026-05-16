#pragma once
// =============================================================================
// GameTypes.h — Types et enums conformes au GDD v1.1
//   docs/GDD_PONG_ULTRA_ARENA_v1.1.docx
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu { namespace pong {

    // ── Modes de jeu (GDD §2) ────────────────────────────────────────────────
    enum class GameMode {
        Local,      // 1v1 même clavier
        VsAI,       // Joueur vs IA
        AIvsAI,     // Démo IA contre IA
        NetworkLAN, // Multijoueur réseau LAN (à implémenter via NKNetwork)
        NetworkOnline
    };

    // ── 6 niveaux d'IA (GDD §3.1) ────────────────────────────────────────────
    enum class AIDifficulty {
        Beginner,    // 01 Débutant   — vitesse 20%, précision 40%, aucune anticipation
        Apprentice,  // 02 Apprenti   — vitesse 40%, précision 60%, trajectoire simple
        Competitor,  // 03 Compétiteur— vitesse 65%, précision 78%, 1 rebond
        Expert,      // 04 Expert     — vitesse 85%, précision 92%, angles complexes + obstacles
        Legend,      // 05 Légende    — vitesse 100%, précision 99%, tous rebonds + obstacles
        Chaos        // 06 Chaos      — variable, totalement aléatoire
    };

    // ── 8 types d'obstacles (GDD §2.2) ───────────────────────────────────────
    enum class ObstacleType {
        Wall,         // Mur Solide — rebond 90 deg
        Portal,       // Portail Électrique — téléport + ×1.5 vitesse
        Gravity,      // Zone Gravitationnelle — déviation continue
        Magnet,       // Aimant — ralentit puis propulse ×2
        Mine,         // Mine — explosion + aveuglement 2s
        GhostMirror,  // Miroir Fantôme — clone temporaire 3s
        AirCurrent,   // Courant d'Air — pousse direction fixe +20%
        BonusStar     // Étoile Bonus — power-up aléatoire au toucheur
    };

    // ── 6 bonus (GDD §2.3.1) ─────────────────────────────────────────────────
    enum class BonusType {
        GiantPaddle,  // Raquette Géante ×2 — 8s
        PaddleSpeed,  // Boost vitesse raquette ×2 — 6s
        DoublePoint,  // Prochain but ×2 — 1 utilisation
        Shield,       // Bouclier absorbe 1 but — 12s
        SlowBall,     // Vitesse balle /2 — 5s (les 2 joueurs)
        RandomStar    // Effet bonus surprise
    };

    // ── 6 malus (GDD §2.3.2) ─────────────────────────────────────────────────
    enum class MalusType {
        Blind,           // Aveuglement -70% vision — 2s (déclenché par Mine)
        MiniPaddle,      // Raquette /2 — 6s (obstacles)
        InvertControls,  // Inversion contrôles — 4s (Zone Gravité)
        Freeze,          // Immobilisation raquette — 1.5s (Power-up ennemi)
        FastBall,        // Vitesse balle ×3 les 2 — 10s (Portail)
        TeleportPaddle   // Raquette téléportée aléatoirement (Power-up ennemi)
    };

    // ── États du cycle de vie (GDD §7.1) ─────────────────────────────────────
    enum class GameState {
        SplashScreen,    // Écran titre
        MainMenu,        // Hub navigation
        SelectMode,      // Local / Réseau / vs IA / IA vs IA
        SelectDifficulty,// 6 niveaux IA
        SelectObstacles, // 8 toggles obstacles
        Options,         // Audio / Graphiques / Gameplay / Contrôles / Réseau
        Competition,     // Tournois / prize pool / spectateur / paris
        Leaderboard,     // Classement global
        Playing,         // Match en cours
        Paused,          // Pause overlay
        GoalFlash,       // Flash après but
        GameOver         // Résultat fin de manche
    };

    // ── Vitesse de balle (GDD §6) ────────────────────────────────────────────
    enum class BallSpeedPreset {
        Slow,   // Lente
        Normal, // Normale (par défaut)
        Fast    // Rapide
    };

    // ── Qualité visuelle (GDD §6) ────────────────────────────────────────────
    enum class VisualQuality {
        Low,    // Basse
        Medium, // Moyenne
        High,   // Haute (défaut)
        Ultra   // Ultra
    };

    // ── Mode daltonien (GDD §6) ──────────────────────────────────────────────
    enum class DaltonianMode {
        Normal,
        Deuteranopia,
        Protanopia
    };

    // ── Région serveur réseau (GDD §6) ───────────────────────────────────────
    enum class NetworkRegion {
        Auto, EU, US, AF
    };

    // ── Configuration de partie (GDD §6) ─────────────────────────────────────
    struct GameSettings {
        // Mode & IA
        GameMode      mode             = GameMode::VsAI;
        AIDifficulty  difficulty       = AIDifficulty::Competitor;

        // Obstacles : un toggle par type (index = ObstacleType)
        bool          obsActive[8]     = { true, true, true, true, true, true, true, true };
        bool          powerUpsEnabled  = true;

        // Gameplay
        int           maxScore         = 11;     // 3/5/7/11/15/21
        int           numRounds        = 1;      // 1/3/5
        BallSpeedPreset ballSpeed      = BallSpeedPreset::Normal;
        bool          ballAcceleration = true;

        // Graphique
        VisualQuality visualQuality    = VisualQuality::High;
        bool          neonGlow         = true;
        bool          particles        = true;
        bool          scanlines        = false;
        DaltonianMode daltonian        = DaltonianMode::Normal;

        // Audio
        int           musicVolume      = 75;
        int           sfxVolume        = 90;
        bool          announcer        = true;

        // Contrôles
        bool          vibration        = true;

        // Réseau
        NetworkRegion region           = NetworkRegion::Auto;
        bool          showPing         = true;
    };

}} // namespace nkentseu::pong
