#pragma once
// =============================================================================
// GameTypes.h
// -----------------------------------------------------------------------------
// Types et enums conformes au Game Design Document v1.1
//   docs/GDD_PONG_ULTRA_ARENA_v1.1.docx
//
// Ce fichier centralise les enumerations metier (modes, IA, obstacles, bonus,
// malus, etats de jeu) ainsi que la struct GameSettings utilisee par toute
// l'application pour porter la configuration utilisateur.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace pong
    {

        // ─────────────────────────────────────────────────────────────────────
        // GameMode — mode de partie selectionne au menu principal.
        // GDD §2 « 2 joueurs s'affrontent en 1v1 ».
        // ─────────────────────────────────────────────────────────────────────
        enum class GameMode
        {
            Local,          ///< 1v1 même clavier (P1 = W/S, P2 = ↑/↓)
            VsAI,           ///< Joueur unique contre IA
            AIvsAI,         ///< Démo : 2 IA s'affrontent
            NetworkLAN,     ///< 1v1 LAN (futur — NKNetwork)
            NetworkOnline   ///< 1v1 Internet (futur — NKNetwork)
        };

        // ─────────────────────────────────────────────────────────────────────
        // AIDifficulty — 6 niveaux selon GDD §3.1.
        // Chaque niveau ajuste vitesse, precision et anticipation de l'IA.
        // ─────────────────────────────────────────────────────────────────────
        enum class AIDifficulty
        {
            Beginner,    ///< 01 Débutant   — vitesse 20%, précision 40%, aucune anticipation
            Apprentice,  ///< 02 Apprenti   — vitesse 40%, précision 60%, trajectoire simple
            Competitor,  ///< 03 Compétiteur— vitesse 65%, précision 78%, 1 rebond
            Expert,      ///< 04 Expert     — vitesse 85%, précision 92%, angles complexes + obstacles
            Legend,      ///< 05 Légende    — vitesse 100%, précision 99%, tous rebonds + obstacles
            Chaos        ///< 06 Chaos      — variable, totalement aléatoire
        };

        // ─────────────────────────────────────────────────────────────────────
        // ObstacleType — 8 types selon GDD §2.2.
        // Chacun applique un effet physique distinct à la balle.
        // ─────────────────────────────────────────────────────────────────────
        enum class ObstacleType
        {
            Wall,         ///< Mur Solide       — rebond 90 deg
            Portal,       ///< Portail          — téléport + ×1.5 vitesse
            Gravity,      ///< Zone Gravité     — déviation continue
            Magnet,       ///< Aimant           — propulse ×2 au contact
            Mine,         ///< Mine             — explosion + aveuglement 2s
            GhostMirror,  ///< Miroir Fantôme   — clone temporaire 3s
            AirCurrent,   ///< Courant d'Air    — pousse direction fixe +20%
            BonusStar     ///< Étoile Bonus     — bonus aléatoire au dernier toucheur
        };

        // ─────────────────────────────────────────────────────────────────────
        // BonusType — 6 bonus selon GDD §2.3.1, attribués au joueur qui collecte.
        // ─────────────────────────────────────────────────────────────────────
        enum class BonusType
        {
            GiantPaddle,  ///< Raquette Géante ×2 — 8s
            PaddleSpeed,  ///< Boost vitesse raquette ×2 — 6s
            DoublePoint,  ///< Prochain but ×2 — 1 utilisation
            Shield,       ///< Bouclier absorbe 1 but — 12s
            SlowBall,     ///< Vitesse balle /2 — 5s (les 2 joueurs)
            RandomStar    ///< Effet bonus surprise (tirage uniforme)
        };

        // ─────────────────────────────────────────────────────────────────────
        // MalusType — 6 malus selon GDD §2.3.2, infligés à l'adversaire.
        // ─────────────────────────────────────────────────────────────────────
        enum class MalusType
        {
            Blind,           ///< Aveuglement -70% vision — 2s
            MiniPaddle,      ///< Raquette /2 — 6s
            InvertControls,  ///< Inversion contrôles — 4s
            Freeze,          ///< Immobilisation raquette — 1.5s
            FastBall,        ///< Vitesse balle ×3 — 10s
            TeleportPaddle   ///< Raquette téléportée aléatoirement
        };

        // ─────────────────────────────────────────────────────────────────────
        // GameState — etats du cycle de vie selon GDD §7.1.
        // Le SceneManager pilote les transitions entre ces etats.
        // ─────────────────────────────────────────────────────────────────────
        enum class GameState
        {
            SplashScreen,     ///< Écran titre animé (auto-advance après ~4s)
            MainMenu,         ///< Hub de navigation principal
            SelectMode,       ///< Choix Local / VsAI / IAvsAI / Réseau
            SelectDifficulty, ///< Sélection niveau IA (6 cards)
            SelectObstacles,  ///< Toggle des 8 obstacles
            Options,          ///< Audio / Graphiques / Gameplay / Contrôles / Réseau
            Competition,      ///< Tournois / prize pool / spectateur / paris
            Leaderboard,      ///< Classement global
            Playing,          ///< Match en cours
            Paused,           ///< Pause overlay (sortie focus = pause auto)
            GoalFlash,        ///< Flash visuel après un but
            GameOver          ///< Résultats fin de manche
        };

        // ─────────────────────────────────────────────────────────────────────
        // BallSpeedPreset — preset de vitesse initiale (GDD §6).
        // ─────────────────────────────────────────────────────────────────────
        enum class BallSpeedPreset
        {
            Slow,
            Normal,
            Fast
        };

        // ─────────────────────────────────────────────────────────────────────
        // VisualQuality — preset graphique (GDD §6).
        // Influe sur particules, glow GPU, scanlines.
        // ─────────────────────────────────────────────────────────────────────
        enum class VisualQuality
        {
            Low,
            Medium,
            High,
            Ultra
        };

        // ─────────────────────────────────────────────────────────────────────
        // DaltonianMode — mode daltonien optionnel (GDD §6).
        // ─────────────────────────────────────────────────────────────────────
        enum class DaltonianMode
        {
            Normal,
            Deuteranopia,
            Protanopia
        };

        // ─────────────────────────────────────────────────────────────────────
        // NetworkRegion — choix region serveur (GDD §6).
        // ─────────────────────────────────────────────────────────────────────
        enum class NetworkRegion
        {
            Auto,
            EU,
            US,
            AF
        };

        // ─────────────────────────────────────────────────────────────────────
        // GameSettings — configuration utilisateur globale.
        // Persistee entre sessions par le SettingsStore (futur).
        // ─────────────────────────────────────────────────────────────────────
        struct GameSettings
        {
            // Mode & IA
            GameMode      mode             = GameMode::VsAI;
            // difficulty   : IA principale (raquette DROITE en VsAI, raquette
            //                GAUCHE en AIvsAI). Toujours utilisee.
            // difficultyP2 : IA secondaire (raquette DROITE en AIvsAI seulement).
            //                Ignoree dans les autres modes.
            AIDifficulty  difficulty       = AIDifficulty::Competitor;
            AIDifficulty  difficultyP2     = AIDifficulty::Competitor;

            // Obstacles actifs : un toggle par type (index = enum ObstacleType)
            bool          obsActive[8]     = { true, true, true, true, true, true, true, true };
            // Parametres personnalises par type d'obstacle (modifiable par
            // le joueur via le panneau dans SelectMatchConfigScene).
            //   count       : 0 = random selon type, sinon nombre exact
            //   powerLevel  : 1=Low, 2=Normal (defaut), 3=High
            //   chaotic     : true = motion random selon profil, false = static
            int           obsCount     [8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            int           obsPowerLevel[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
            bool          obsChaotic   [8] = { true, true, true, true,
                                               true, true, true, true };
            bool          powerUpsEnabled  = true;

            // Gameplay — conditions de victoire flexibles
            int             maxScore         = 11;     ///< Points pour gagner (3/5/7/11/15/21). 0 = pas de limite score.
            int             numRounds        = 1;      ///< Nombre de manches (1/3/5)
            float           timeLimit        = 0.0f;   ///< Limite de temps en secondes (0 = pas de limite). A timeLeft<=0, le joueur en tete gagne.
            bool            winByTwo         = false;  ///< Si true, il faut 2 pts d'ecart pour gagner (regle tennis).
            BallSpeedPreset ballSpeed        = BallSpeedPreset::Normal;
            bool            ballAcceleration = true;
            /// Multiplicateur global applique a la vitesse de la balle a chaque
            /// engagement (>= 1.0 garanti par le menu). 1.0 = vitesse de base
            /// historique, 1.5 = +50%, etc. Permet au joueur de rythmer la
            /// partie sans toucher au code.
            float           ballSpeedMul     = 1.0f;

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

            // Seed RNG pour spawn deterministe (obstacles + power-ups).
            // - 0 = utilise random_device (mode local classique).
            // - != 0 = srand(seed) avant spawn pour reproductibilite.
            // Utilise en mode reseau : le HOST genere la seed et la transmet
            // au CLIENT via PktStartMatch pour que les 2 cotes spawnent les
            // mêmes obstacles. Transient, non persiste.
            uint32        obstacleSeed     = 0;
        };

    } // namespace pong
} // namespace nkentseu
