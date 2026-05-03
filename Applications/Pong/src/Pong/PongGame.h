#pragma once
// =============================================================================
// PongGame.h
// =============================================================================
// Description : Gestionnaire principal du jeu Pong avec :
//   - Interface utilisateur multi-ecrans (menu, selection difficulte, obstacles)
//   - Systeme de difficulte IA calibre (4 niveaux)
//   - 6 presets de niveaux d\'obstacles avec positions variees
//   - Effets visuels (lueurs, particules, flash, ecran-secousse)
//   - Double-buffering via NkRenderer
//
// Hierarchie des composants :
//   PongGame      -> Classe principale, orchestre tout
//   Ball          -> Balle avec trail et etats de phase
//   Paddle        -> Raquette joueur / IA avec parametres de difficulte
//   Obstacle      -> Bloc interactif avec 8 effets possibles
//   Particle      -> Point-masse pour effets visuels
//   GameSettings  -> Configuration courante (difficulte, preset, score max)
//
// Cycle de vie du jeu :
//   MainMenu -> SelectDifficulty / SelectObstacles -> Playing
//   Playing  -> Paused <-> Playing
//   Playing  -> GoalFlash -> Playing / GameOver
//   GameOver -> MainMenu / Playing (rejouer)
//
// Usage simplifie :
//   PongGame game(renderer);
//   game.Init();
//   while (running) {
//       game.Update(dt, up, down, enter, esc, left, right);
//       game.Render();
//       renderer.Present();
//   }
// =============================================================================

#include "Renderer/NkRasterizer.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <random>
#include <cstdio>

namespace nkentseu {

    // =============================================================================
    // Espace de noms : pongmath::
    // Description   : Fonctions mathematiques utilitaires pour la physique du jeu.
    //                 Toutes les fonctions sont inline et noexcept pour performances.
    // =============================================================================
    namespace pongmath {
        /// @brief Produit scalaire de deux vecteurs 2D.
        /// @param ax Composante X du premier vecteur.
        /// @param ay Composante Y du premier vecteur.
        /// @param bx Composante X du second vecteur.
        /// @param by Composante Y du second vecteur.
        /// @return Valeur scalaire ax*bx + ay*by.
        inline float Dot(float ax, float ay, float bx, float by) noexcept {
            return ax * bx + ay * by;
        }

        /// @brief Norme euclidienne d\'un vecteur 2D.
        /// @param x Composante X.
        /// @param y Composante Y.
        /// @return sqrt(x*x + y*y).
        inline float Length(float x, float y) noexcept {
            return math::NkSqrt(x * x + y * y);
        }

        /// @brief Normalise un vecteur 2D en place (longueur = 1).
        ///        Ne fait rien si la longueur est quasi-nulle (evite division par zero).
        /// @param x Composante X, modifiee en place.
        /// @param y Composante Y, modifiee en place.
        inline void Normalize(float& x, float& y) noexcept {
            float l = Length(x, y);
            if (l > 0.0001f)
            {
                x /= l;
                y /= l;
            }
        }

        /// @brief Contraint une valeur dans l\'intervalle [lo, hi].
        /// @param v  Valeur a contraindre.
        /// @param lo Borne inferieure.
        /// @param hi Borne superieure.
        /// @return Valeur clampee.
        inline float Clamp(float v, float lo, float hi) noexcept {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        /// @brief Interpolation lineaire entre a et b selon le facteur t.
        /// @param a Valeur de depart (t = 0).
        /// @param b Valeur d\'arrivee (t = 1).
        /// @param t Facteur d\'interpolation [0..1].
        /// @return a + (b - a) * t.
        inline float Lerp(float a, float b, float t) noexcept {
            return a + (b - a) * t;
        }

        /// @brief Genere un flottant aleatoire dans [lo, hi].
        /// @param rng Generateur Mersenne Twister.
        /// @param lo  Borne inferieure.
        /// @param hi  Borne superieure.
        /// @return Valeur aleatoire uniformement distribuee.
        inline float RandF(std::mt19937& rng, float lo, float hi) noexcept {
            std::uniform_real_distribution<float> d(lo, hi);
            return d(rng);
        }

    } // namespace pongmath::


    // =============================================================================
    // Espace de noms : gamecolors
    // Description   : Palette de couleurs predefinies pour tous les elements du jeu.
    //                 Chaque couleur est une fonction inline retournant un NkColor.
    //                 Les couleurs d\'obstacle correspondent a leur type d\'effet.
    // =============================================================================
    namespace gamecolors {
        /// @brief Fond de l\'ecran : bleu nuit profond.
        inline math::NkColor BG() {
            return { 6, 10, 24, 255 };
        }

        /// @brief Grille decorative de fond : bleu-gris tres discret.
        inline math::NkColor Grid() {
            return { 18, 28, 55, 255 };
        }

        /// @brief Raquette du joueur : cyan electrique.
        inline math::NkColor Player() {
            return { 80, 220, 255, 255 };
        }

        /// @brief Raquette de l\'IA : rose-rouge.
        inline math::NkColor AI() {
            return { 255, 100, 120, 255 };
        }

        /// @brief Balle : jaune dore.
        inline math::NkColor BallCol() {
            return { 255, 230, 60, 255 };
        }

        /// @brief Trail de la balle : orange semi-transparent.
        inline math::NkColor TrailCol() {
            return { 255, 160, 20, 120 };
        }

        /// @brief Blanc pur pour textes et elements d\'interface.
        inline math::NkColor White() {
            return { 255, 255, 255, 255 };
        }

        /// @brief Ligne centrale du terrain : bleu-marine semi-transparent.
        inline math::NkColor CenterLine() {
            return { 50, 70, 120, 200 };
        }

        /// @brief Fond des panneaux UI : bleu nuit tres sombre, semi-transparent.
        inline math::NkColor PanelBG() {
            return { 12, 18, 42, 220 };
        }

        /// @brief Bordure des panneaux UI : bleu moyen.
        inline math::NkColor PanelBorder() {
            return { 60, 90, 180, 255 };
        }

        /// @brief Bordure des elements UI selectionnes : cyan vif.
        inline math::NkColor SelBorder() {
            return { 100, 200, 255, 255 };
        }

        /// @brief Or : titres et elements importants.
        inline math::NkColor Gold() {
            return { 255, 200, 0, 255 };
        }

        /// @brief Vert vif : actions positives (Jouer, Reprendre).
        inline math::NkColor Green() {
            return { 80, 255, 120, 255 };
        }

        /// @brief Rouge vif : actions destructives (Quitter).
        inline math::NkColor Red() {
            return { 255, 80, 80, 255 };
        }

        // ── Couleurs des obstacles (chacune identifie un type d\'effet) ────────────

        /// @brief SpeedBoost : vert neon (acceleration).
        inline math::NkColor ObsBoost() {
            return { 50, 255, 100, 255 };
        }

        /// @brief SpeedReduce : rouge terne (deceleration).
        inline math::NkColor ObsReduce() {
            return { 200, 80, 80, 255 };
        }

        /// @brief Redirect : violet (deviation angulaire fixe).
        inline math::NkColor ObsRedirect() {
            return { 180, 100, 255, 255 };
        }

        /// @brief RandomDeflect : orange vif (deviation aleatoire).
        inline math::NkColor ObsRandom() {
            return { 255, 180, 0, 255 };
        }

        /// @brief Magnetic : cyan electrique (attraction/repulsion).
        inline math::NkColor ObsMagnetic() {
            return { 0, 200, 255, 255 };
        }

        /// @brief Portal : magenta (teleportation).
        inline math::NkColor ObsPortal() {
            return { 255, 60, 200, 255 };
        }

        /// @brief Phase : vert d\'eau (traversee temporaire).
        inline math::NkColor ObsPhase() {
            return { 120, 255, 200, 255 };
        }

    } // namespace gamecolors


    // =============================================================================
    // Enumeration : AIDifficulty
    // Description : Niveaux de difficulte de l\'intelligence artificielle.
    //               Chaque niveau ajuste la reaction, l\'erreur et la prediction.
    //
    //  Facile  : reaction 28%, erreur +-72 px, pas de prediction -> IA tres battable
    //  Moyen   : reaction 52%, erreur +-38 px, pas de prediction -> IA equilibree
    //  Difficile: reaction 73%, erreur +-16 px, prediction 0.09 s -> IA reactive
    //  Expert  : reaction 88%, erreur +-5  px, prediction 0.16 s -> tres difficile
    // =============================================================================
    enum class AIDifficulty {
        Easy,    ///< IA lente, grosses erreurs, sans prediction de trajectoire.
        Medium,  ///< IA equilibree, erreurs moderees, sans prediction.
        Hard,    ///< IA rapide, petites erreurs, prediction courte.
        Expert   ///< IA tres rapide, quasi-parfaite, prediction longue.
    };

    /// @brief Retourne le nom affichable d\'un niveau de difficulte.
    /// @param d Niveau de difficulte.
    /// @return Chaine C statique (ex: "FACILE").
    inline const char* AIDifficultyName(AIDifficulty d) {
        switch (d) {
            case AIDifficulty::Easy:   return "FACILE";
            case AIDifficulty::Medium: return "MOYEN";
            case AIDifficulty::Hard:   return "DIFFICILE";
            case AIDifficulty::Expert: return "EXPERT";
            default:                   return "?";
        }
    }

    /// @brief Retourne la description courte d\'un niveau de difficulte.
    /// @param d Niveau de difficulte.
    /// @return Chaine C statique decrivant le comportement de l\'IA.
    inline const char* AIDifficultyDesc(AIDifficulty d) {
        switch (d) {
            case AIDifficulty::Easy:
                return "IA lente, grosses erreurs. Parfait pour les debutants.";
            case AIDifficulty::Medium:
                return "IA equilibree, bonne pour la plupart des joueurs.";
            case AIDifficulty::Hard:
                return "IA rapide et precise. Requiert de la concentration.";
            case AIDifficulty::Expert:
                return "IA maximale. Seuls les meilleurs peuvent gagner!";
            default:
                return "";
        }
    }


    // =============================================================================
    // Enumeration : ObstaclePreset
    // Description : Presets de niveaux d\'obstacles.
    //               Chaque preset definit un agencement unique avec des positions
    //               variees (pas forcement au centre) et des effets differents.
    //
    //  None     : Aucun obstacle -> Pong classique pur
    //  Classic  : 6 obstacles symetriques autour du centre
    //  Chaos    : 12 obstacles disperses aleatoirement (seed fixe)
    //  Gauntlet : Rangees d\'obstacles formant des couloirs a traverser
    //  Portal   : 4 paires de portails de teleportation disperses
    //  Boss     : Grand aimant central + anneau de 6 obstacles en orbite
    // =============================================================================
    enum class ObstaclePreset {
        None,      ///< Aucun obstacle, Pong pur.
        Classic,   ///< 6 obstacles symetriques, layout equilibre.
        Chaos,     ///< 12 obstacles aleatoires, tres impredictible.
        Gauntlet,  ///< Rangees formant des corridors.
        Portal,    ///< Portails de teleportation.
        Boss       ///< Aimant geant + obstacles en orbite.
    };

    /// @brief Retourne le nom affichable d\'un preset d\'obstacles.
    /// @param p Preset d\'obstacles.
    /// @return Chaine C statique (ex: "CLASSIQUE").
    inline const char* ObstaclePresetName(ObstaclePreset p) {
        switch (p) {
            case ObstaclePreset::None:     return "AUCUN";
            case ObstaclePreset::Classic:  return "CLASSIQUE";
            case ObstaclePreset::Chaos:    return "CHAOS";
            case ObstaclePreset::Gauntlet: return "GAUNTLET";
            case ObstaclePreset::Portal:   return "PORTAILS";
            case ObstaclePreset::Boss:     return "BOSS";
            default:                       return "?";
        }
    }

    /// @brief Retourne la description courte d\'un preset d\'obstacles.
    /// @param p Preset d\'obstacles.
    /// @return Chaine C statique.
    inline const char* ObstaclePresetDesc(ObstaclePreset p) {
        switch (p) {
            case ObstaclePreset::None:
                return "Pong pur. Aucun obstacle, que du skill!";
            case ObstaclePreset::Classic:
                return "6 obstacles equilibres. Boost, ralenti, deflexion.";
            case ObstaclePreset::Chaos:
                return "12 obstacles disperses! Tres impredictible.";
            case ObstaclePreset::Gauntlet:
                return "Rangees d\'obstacles. Trouvez les passages!";
            case ObstaclePreset::Portal:
                return "Portails de teleportation partout. Surprise!";
            case ObstaclePreset::Boss:
                return "Grand aimant + obstacles en orbite. Chaos total!";
            default:
                return "";
        }
    }


    // =============================================================================
    // Structure : GameSettings
    // Description : Regroupe tous les parametres configurables d\'une partie.
    //               Instanciee avec des valeurs par defaut jouables immediatement.
    // =============================================================================
    struct GameSettings {
        /// @brief Niveau de difficulte de l\'IA (par defaut : Medium).
        AIDifficulty   difficulty     = AIDifficulty::Medium;

        /// @brief Preset d\'obstacles du niveau (par defaut : Classic).
        ObstaclePreset obstaclePreset = ObstaclePreset::Classic;

        /// @brief Nombre de points requis pour gagner la partie (par defaut : 7).
        int            maxScore       = 7;
    };

    // =============================================================================
    // Enumeration : ObstacleEffect
    // Description : Type d\'effet applique par un obstacle lors d\'un contact.
    // =============================================================================
    enum class ObstacleEffect {
        None,          ///< Rebond standard, aucun effet supplementaire.
        SpeedBoost,    ///< Multiplie la vitesse de la balle par speedMult.
        SpeedReduce,   ///< Divise la vitesse de la balle par speedMult.
        Redirect,      ///< Applique une rotation de fixedAngle a la velocite.
        RandomDeflect, ///< Ajoute un bruit aleatoire [-angleNoise, +angleNoise].
        Magnetic,      ///< Force d\'attraction (>0) ou repulsion (<0) permanente.
        Portal,        ///< Teleporte la balle vers l\'obstacle d\'index pairedId.
        Phase          ///< Cycle ACTIF/TRAVERSABLE controlee par phaseTimer.
    };

    // =============================================================================
    // Structure : Obstacle
    // Description : Bloc interactif du terrain avec geometrie, deplacement,
    //               effet special et cycle de phase.
    //
    // Cycle de vie :
    //   Si hasPhase == true :
    //     phaseTimer decrement chaque frame.
    //     Quand phaseTimer <= 0 : isPhasing bascule, phaseTimer se recharge.
    //     ACTIF    -> attend phaseCooldown secondes -> PHASE
    //     PHASE    -> attend phaseDuration secondes -> ACTIF
    //
    // Deplacement :
    //   L\'obstacle se deplace selon (vx, vy) et rebondit sur les bords du terrain.
    //   vx = vy = 0 pour un obstacle fixe.
    // =============================================================================
    struct Obstacle {
        // ── Geometrie ─────────────────────────────────────────────────────────────

        /// @brief Coordonnee X du coin superieur gauche (pixels).
        float x = 0.0f;

        /// @brief Coordonnee Y du coin superieur gauche (pixels).
        float y = 0.0f;

        /// @brief Largeur de l\'obstacle (pixels).
        float w = 0.0f;

        /// @brief Hauteur de l\'obstacle (pixels).
        float h = 0.0f;

        /// @brief Velocite horizontale pour les obstacles mobiles (pixels/s).
        float vx = 0.0f;

        /// @brief Velocite verticale pour les obstacles mobiles (pixels/s).
        float vy = 0.0f;

        // ── Effet special ─────────────────────────────────────────────────────────

        /// @brief Type d\'effet applique au contact de la balle.
        ObstacleEffect effect = ObstacleEffect::None;

        /// @brief Facteur de vitesse pour SpeedBoost / SpeedReduce.
        float speedMult = 1.0f;

        /// @brief Angle de rotation supplementaire pour Redirect (radians).
        float fixedAngle = 0.0f;

        /// @brief Amplitude maximale du bruit angulaire pour RandomDeflect (rad).
        float angleNoise = 0.4f;

        /// @brief Intensite de la force magnetique (>0 = attraction, <0 = repulsion).
        float magnetForce = 0.0f;

        /// @brief Index de l\'obstacle cible pour l\'effet Portal (-1 = non apparie).
        int pairedId = -1;

        // ── Cycle de phase ────────────────────────────────────────────────────────

        /// @brief Active le mecanisme de phase (traversee periodique).
        bool hasPhase = false;

        /// @brief Duree en secondes de l\'etat traversable (isPhasing == true).
        float phaseDuration = 1.2f;

        /// @brief Duree en secondes de l\'etat solide entre deux phases.
        float phaseCooldown = 3.5f;

        /// @brief Minuteur interne du cycle de phase (decremente chaque frame).
        float phaseTimer = 0.0f;

        /// @brief Etat courant : true = la balle peut traverser cet obstacle.
        bool isPhasing = false;

        // ── Apparence visuelle ────────────────────────────────────────────────────

        /// @brief Couleur de remplissage de l\'obstacle.
        math::NkColor color = { 200, 200, 200, 255 };

        /// @brief Couleur de l\'effet de lueur autour de l\'obstacle.
        math::NkColor glowColor = { 255, 255, 255, 100 };

        /// @brief Minuteur d\'animation de la pulsation (incremente chaque frame).
        float pulseTimer = 0.0f;

        /// @brief Frequence de la pulsation visuelle (Hz).
        float pulseSpeed = 2.0f;

        /// @brief Intensite du flash d\'impact apres un rebond [0..1], decroissance auto.
        float hitFlash = 0.0f;

        // ── Tests de collision ────────────────────────────────────────────────────

        /// @brief Verifie si le point (px, py) est contenu dans l\'AABB de l\'obstacle.
        /// @return true si le point est dans [x, x+w] x [y, y+h].
        bool Contains(float px, float py) const noexcept
        {
            return px >= x && px <= x + w && py >= y && py <= y + h;
        }

        // ── Mise a jour ───────────────────────────────────────────────────────────

        /// @brief Met a jour la position, le cycle de phase et les effets visuels.
        /// @param dt    Delta-time en secondes.
        /// @param fieldW Largeur du terrain en pixels.
        /// @param fieldH Hauteur du terrain en pixels.
        void Update(float dt, float fieldW, float fieldH) noexcept
        {
            // Deplacement selon la velocite courante
            x += vx * dt;
            y += vy * dt;

            // Rebond sur le bord gauche
            if (x < 0.0f)
            {
                x  = 0.0f;
                vx = -vx;
            }

            // Rebond sur le bord droit
            if (x + w > fieldW)
            {
                x  = fieldW - w;
                vx = -vx;
            }

            // Rebond sur le bord haut
            if (y < 0.0f)
            {
                y  = 0.0f;
                vy = -vy;
            }

            // Rebond sur le bord bas
            if (y + h > fieldH)
            {
                y  = fieldH - h;
                vy = -vy;
            }

            // Gestion du cycle de phase
            if (hasPhase)
            {
                phaseTimer -= dt;

                if (phaseTimer <= 0.0f)
                {
                    // Basculer entre etat traversable et etat solide
                    isPhasing  = !isPhasing;
                    phaseTimer = isPhasing ? phaseDuration : phaseCooldown;
                }
            }

            // Animation de pulsation visuelle
            pulseTimer += dt * pulseSpeed;

            // Decroissance du flash d\'impact
            if (hitFlash > 0.0f)
            {
                hitFlash -= dt * 4.0f;
            }
        }
    };


    // =============================================================================
    // Structure : Ball
    // Description : Balle du jeu avec physique, trail visuel et etat de phase.
    //
    // Vitesse :
    //   Demarre a BASE_SPEED, augmente a chaque contact avec une raquette (+6%).
    //   Plafonnee a MAX_SPEED.
    //   ClampSpeed() normalise le vecteur velocite apres chaque modification.
    //
    // Trail :
    //   18 positions historiques enregistrees a ~60 Hz via trailTimer.
    //   Utilisees par le rendu pour dessiner une trainee de lueur.
    //
    // Phase :
    //   Quand isPhasing == true, la balle traverse les obstacles PhaseActif.
    //   La phase s\'active via BallVsObstacle et expire apres phaseTimer secondes.
    //
    // Charge visuelle :
    //   chargeLevel = (speed - BASE_SPEED) / (MAX_SPEED - BASE_SPEED) dans [0..1].
    //   La couleur passe de jaune (0) a rouge (1) selon ce niveau.
    // =============================================================================
    struct Ball {
        // ── Constantes de configuration ───────────────────────────────────────────

        /// @brief Nombre de points dans le trail visuel.
        static constexpr int   TRAIL_LEN  = 18;

        /// @brief Vitesse de depart de la balle (pixels/seconde).
        static constexpr float BASE_SPEED = 270.0f;

        /// @brief Vitesse maximale autorisee (pixels/seconde).
        static constexpr float MAX_SPEED  = 680.0f;

        // ── Etat physique ─────────────────────────────────────────────────────────

        /// @brief Position horizontale du centre de la balle (pixels).
        float x = 0.0f;

        /// @brief Position verticale du centre de la balle (pixels).
        float y = 0.0f;

        /// @brief Composante X de la velocite (pixels/seconde).
        float vx = 0.0f;

        /// @brief Composante Y de la velocite (pixels/seconde).
        float vy = 0.0f;

        /// @brief Rayon de la balle pour la detection de collision (pixels).
        float radius = 8.0f;

        /// @brief Norme du vecteur velocite (pixels/seconde).
        float speed = BASE_SPEED;

        // ── Trail visuel ──────────────────────────────────────────────────────────

        /// @brief Buffer circulaire des positions X precedentes.
        float trailX[TRAIL_LEN] = {};

        /// @brief Buffer circulaire des positions Y precedentes.
        float trailY[TRAIL_LEN] = {};

        /// @brief Tete du buffer circulaire (position la plus recente).
        int trailHead = 0;

        /// @brief Accumulateur de temps pour l\'echantillonnage du trail.
        float trailTimer = 0.0f;

        // ── Etat de phase ─────────────────────────────────────────────────────────

        /// @brief true = la balle traverse actuellement les obstacles phasing.
        bool isPhasing = false;

        /// @brief Duree restante de l\'etat de phase (secondes).
        float phaseTimer = 0.0f;

        // ── Feedback visuel ───────────────────────────────────────────────────────

        /// @brief Niveau de charge [0..1] base sur la vitesse courante.
        float chargeLevel = 0.0f;

        // ── Methodes ──────────────────────────────────────────────────────────────

        /// @brief Initialise la balle a la position (cx, cy) avec direction (dirX, dirY).
        ///        La direction est normalisee automatiquement.
        ///        La vitesse est appliquee depuis le membre speed courant.
        /// @param cx   Position X initiale.
        /// @param cy   Position Y initiale.
        /// @param dirX Composante X de la direction (non normalisee).
        /// @param dirY Composante Y de la direction (non normalisee).
        void Init(float cx, float cy, float dirX, float dirY) noexcept
        {
            x = cx;
            y = cy;
            pongmath::Normalize(dirX, dirY);
            vx        = dirX * speed;
            vy        = dirY * speed;
            isPhasing = false;
            phaseTimer = 0.0f;
            for (int i = 0; i < TRAIL_LEN; ++i)
            {
                trailX[i] = cx;
                trailY[i] = cy;
            }
            trailHead = 0;
        }

        /// @brief Avance la physique d\'un delta-time dt.
        ///        Met a jour : position, chargeLevel, phaseTimer, trail.
        /// @param dt Delta-time en secondes.
        void Update(float dt) noexcept
        {
            // Deplacement
            x += vx * dt;
            y += vy * dt;

            // Calcul du niveau de charge visuel
            chargeLevel = pongmath::Clamp(
                (speed - BASE_SPEED) / (MAX_SPEED - BASE_SPEED),
                0.0f,
                1.0f
            );

            // Decompte de la phase
            if (isPhasing)
            {
                phaseTimer -= dt;
                if (phaseTimer <= 0.0f)
                {
                    isPhasing = false;
                }
            }

            // Enregistrement du trail (~60 Hz)
            trailTimer += dt;
            if (trailTimer >= 0.016f)
            {
                trailTimer             = 0.0f;
                trailHead              = (trailHead + 1) % TRAIL_LEN;
                trailX[trailHead] = x;
                trailY[trailHead] = y;
            }
        }

        /// @brief Contraint la vitesse dans [BASE_SPEED*0.5, MAX_SPEED].
        ///        Renormalise le vecteur velocite pour conserver la direction.
        void ClampSpeed() noexcept
        {
            speed = pongmath::Clamp(speed, BASE_SPEED * 0.5f, MAX_SPEED);
            float len = pongmath::Length(vx, vy);
            if (len > 0.001f)
            {
                vx = (vx / len) * speed;
                vy = (vy / len) * speed;
            }
        }

        /// @brief Calcule la couleur dynamique de la balle selon le niveau de charge.
        ///        Interpolation jaune (charge=0) -> rouge (charge=1).
        /// @return Couleur RGBA de la balle.
        math::NkColor CurrentColor() const noexcept
        {
            uint8_t r = 255;
            uint8_t g = static_cast<uint8_t>(230.0f * (1.0f - chargeLevel * 0.8f));
            uint8_t b = static_cast<uint8_t>(60.0f  * (1.0f - chargeLevel));
            return { r, g, b, 255 };
        }
    };


    // =============================================================================
    // Structure : Paddle
    // Description : Raquette du joueur ou de l\'IA.
    //
    // Joueur (isLeft == true)  : deplacee via MoveUp/MoveDown selon les inputs.
    // IA    (isLeft == false)  : deplacee par UpdatePaddles selon les parametres
    //                            aiReaction, aiSpeed et aiError.
    //
    // Parametres IA :
    //   aiReaction : fraction de l\'erreur verticale corrigee par frame [0..1].
    //                Faible = IA lente, eleve = IA quasi-parfaite.
    //   aiSpeed    : vitesse maximale de deplacement de l\'IA (pixels/s).
    //   aiError    : decalage aleatoire applique a la cible verticale de l\'IA.
    //                Simule l\'imprécision humaine. Rafraichi periodiquement.
    // =============================================================================
    struct Paddle {
        // ── Geometrie ─────────────────────────────────────────────────────────────

        /// @brief Position X du coin superieur gauche (pixels).
        float x = 0.0f;

        /// @brief Position Y du coin superieur gauche (pixels).
        float y = 0.0f;

        /// @brief Largeur de la raquette (pixels).
        float w = 12.0f;

        /// @brief Hauteur de la raquette (pixels).
        float h = 90.0f;

        /// @brief Vitesse de deplacement du joueur (pixels/s).
        float speed = 400.0f;

        // ── Apparence et score ────────────────────────────────────────────────────

        /// @brief Couleur de la raquette.
        math::NkColor color = { 200, 200, 200, 255 };

        /// @brief Nombre de points marques par ce cote.
        int score = 0;

        // ── Identification ────────────────────────────────────────────────────────

        /// @brief true = raquette gauche (joueur), false = droite (IA).
        bool isLeft = true;

        // ── Parametres IA ─────────────────────────────────────────────────────────

        /// @brief Facteur de reaction [0..1] : part de l\'ecart corrigee par frame.
        float aiReaction = 1.0f;

        /// @brief Offset aleatoire applique a la cible verticale de l\'IA.
        float aiError = 0.0f;

        /// @brief Vitesse maximale de deplacement de l\'IA (pixels/s).
        float aiSpeed = 400.0f;

        // ── Methodes ──────────────────────────────────────────────────────────────

        /// @brief Retourne la coordonnee Y du centre vertical de la raquette.
        float CenterY() const noexcept
        {
            return y + h * 0.5f;
        }

        /// @brief Deplace la raquette vers le haut d\'une distance speed*dt.
        /// @param dt Delta-time en secondes.
        void MoveUp(float dt) noexcept
        {
            y -= speed * dt;
        }

        /// @brief Deplace la raquette vers le bas d\'une distance speed*dt.
        /// @param dt Delta-time en secondes.
        void MoveDown(float dt) noexcept
        {
            y += speed * dt;
        }

        /// @brief Contraint la raquette dans les limites verticales du terrain.
        /// @param fieldH Hauteur du terrain en pixels.
        void ClampToField(float fieldH) noexcept
        {
            if (y < 0.0f)
            {
                y = 0.0f;
            }
            if (y + h > fieldH)
            {
                y = fieldH - h;
            }
        }
    };


    // =============================================================================
    // Structure : Particle
    // Description : Particule elementaire pour effets visuels (impacts, buts, etc.)
    //               Chaque particule a une duree de vie, une position, une velocite
    //               et une couleur. La taille decroit automatiquement.
    //               Le mode additif cree un effet de lueur lumineuse.
    // =============================================================================
    struct Particle {
        /// @brief Position horizontale courante (pixels).
        float x = 0.0f;

        /// @brief Position verticale courante (pixels).
        float y = 0.0f;

        /// @brief Velocite horizontale (pixels/s).
        float vx = 0.0f;

        /// @brief Velocite verticale (pixels/s). Soumise a la gravite (+120/s^2).
        float vy = 0.0f;

        /// @brief Duree de vie restante (secondes). Supprimee quand <= 0.
        float life = 0.0f;

        /// @brief Duree de vie initiale (secondes). Utilisee pour l\'alpha decroissant.
        float maxLife = 0.0f;

        /// @brief Rayon visuel de la particule (pixels). Decroit chaque frame.
        float size = 0.0f;

        /// @brief Couleur de la particule (alpha utilise pour le fondu).
        math::NkColor color = { 255, 255, 255, 255 };

        /// @brief true = fusion additive (lueur), false = fusion alpha standard.
        bool additive = false;
    };


    // =============================================================================
    // Enumeration : GameState
    // Description : Etats possibles du cycle de vie du jeu.
    //               Controle quelles methodes Update et Render sont actives.
    //
    //  MainMenu         : Ecran titre avec options Jouer, Difficulte, Obstacles
    //  SelectDifficulty : Ecran de selection du niveau de l\'IA
    //  SelectObstacles  : Ecran de selection du preset d\'obstacles + preview
    //  Playing          : Partie en cours, toute la logique est active
    //  Paused           : Jeu suspendu, overlay de pause affiche
    //  GoalFlash        : Flash visuel bref apres un but (avant reprise)
    //  GameOver         : Fin de partie, affichage du vainqueur
    // =============================================================================
    enum class GameState
    {
        SplashScreen,     ///< Ecran de demarrage : entreprise Rihen + moteur Noge.
        MainMenu,         ///< Menu principal.
        SelectDifficulty, ///< Selection de la difficulte IA.
        SelectObstacles,  ///< Selection du preset d\'obstacles.
        Playing,          ///< Partie en cours.
        Paused,           ///< Jeu en pause (touche P ou Echap).
        GoalFlash,        ///< Flash apres un but.
        GameOver          ///< Fin de partie.
    };


    // =============================================================================
    // Classe : PongGame
    // Description : Gestionnaire principal du jeu Pong.
    //               Orchestre la logique, le rendu et la navigation UI.
    //
    // Interface publique :
    //   Init()    -> Reinitialise le jeu (raquettes, balle, obstacles, etat).
    //   OnResize() -> Adapte les positions au nouveau viewport sans changer l'etat.
    //   Update()  -> Avance la logique d\'un frame (physique, IA, menus).
    //   Render()  -> Dessine toutes les couches visuelles sur le back-buffer.
    //
    // Parametres de Update :
    //   dt           : Delta-time en secondes (clamp a 33ms max).
    //   upHeld       : Touche haut maintenue (deplacement raquette).
    //   downHeld     : Touche bas maintenue (deplacement raquette).
    //   enterPressed : Touche Entree/Espace sur le front montant ce frame.
    //   escapePressed: Touche Echap sur le front montant ce frame.
    //   leftPressed  : Touche gauche sur le front montant (navigation future).
    //   rightPressed : Touche droite sur le front montant (navigation future).
    //
    // Dependances :
    //   - NkRenderer (double-buffered software renderer)
    //   - NkRasterizer (primitives 2D : lignes, cercles, rectangles, texte)
    // =============================================================================
    class PongGame {
        public:
            // ── Constructeur ──────────────────────────────────────────────────────────

            /// @brief Construit le gestionnaire de jeu en le liant au renderer.
            ///        Le jeu n\'est pas proprietaire du renderer.
            ///        Appeler Init() avant tout Update/Render.
            /// @param renderer Reference au renderer double-buffere.
            explicit PongGame(NkRenderer& renderer) noexcept
                : mRenderer(renderer)
                , mRasterizer(renderer)
                , mRng(42u)
            {
            }

            // ── Interface publique ────────────────────────────────────────────────────

            /// @brief Initialise ou reinitialise tous les composants du jeu.
            ///        Repose les raquettes, regenere les obstacles, remet les scores a
            ///        zero et place le jeu sur MainMenu.
            ///        Doit etre appele apres Init() du renderer.
            void Init();

            /// @brief Adapte la position des raquettes et regenere les obstacles
            ///        apres un redimensionnement de la fenetre, sans changer l'etat du jeu.
            ///        Conserve le GameState courant, les scores et la selection des menus.
            void OnResize();

            /// @brief Met a jour la logique du jeu d\'un frame.
            ///        Gestion des menus, physique, IA, collisions, particules.
            /// @param dt           Delta-time en secondes.
            /// @param upHeld       Touche haut maintenue.
            /// @param downHeld     Touche bas maintenue.
            /// @param enterPressed Touche Entree (flanc montant ce frame).
            /// @param escapePressed Touche Echap (flanc montant ce frame).
            /// @param leftPressed  Touche gauche (flanc montant ce frame).
            /// @param rightPressed Touche droite (flanc montant ce frame).
            void Update(float dt,
                        bool  upHeld,
                        bool  downHeld,
                        bool  enterPressed,
                        bool  escapePressed,
                        bool  leftPressed,
                        bool  rightPressed,
                        int   mouseX     = -1,
                        int   mouseY     = -1,
                        bool  mouseClick = false);

            /// @brief Dessine l\'integralite de l\'etat courant du jeu sur le back-buffer.
            ///        Couches : fond -> obstacles -> raquettes -> balle -> particules
            ///                  -> HUD -> overlays (menus, pause, flash, game over).
            void Render();

            /// @brief Retourne l\'etat courant du jeu (utile pour Apps.cpp).
            /// @return Valeur de l\'enumeration GameState.
            GameState GetState() const noexcept
            {
                return mState;
            }

            // ── Interface touch (Android) ─────────────────────────────────────────────

            /// @brief Active ou desactive le rendu des boutons tactiles a l\'ecran.
            void SetShowTouchButtons(bool show) noexcept
            {
                mShowTouchButtons = show;
            }

            /// @brief Rectangles des boutons tactiles pour le hit-testing.
            struct TouchButtonRects {
                // Bouton VALIDATION/ENTER (haut a gauche)
                int enterX, enterY, enterW, enterH;
                // Bouton BACK/ESCAPE (haut au centre)
                int escapeX, escapeY, escapeW, escapeH;
                // Bouton PAUSE (haut a droite)
                int pauseX, pauseY, pauseW, pauseH;
                // Bouton UP (milieu bas, a gauche du centre)
                int upX, upY, upW, upH;
                // Bouton DOWN (milieu bas, a droite du centre)
                int dnX, dnY, dnW, dnH;
            };

            /// @brief Calcule et retourne les coordonnees des boutons tactiles.
            TouchButtonRects GetTouchButtonRects() const noexcept;

        private:
            // ── Gestion de la partie ──────────────────────────────────────────────────

            /// @brief Remet la balle au centre et lui donne une direction aleatoire.
            /// @param leftServe true = la balle part vers la gauche (joueur sert).
            void ResetBall(bool leftServe);

            /// @brief Applique les settings courants et passe en etat Playing.
            ///        Configure les parametres IA selon la difficulte selectionnee.
            void StartGame();

            /// @brief Dispatche vers la methode SpawnObstacles_* selon le preset courant.
            void SpawnObstacles();

            // ── Presets d\'obstacles (un par valeur de ObstaclePreset) ─────────────────

            /// @brief Aucun obstacle. Terrain vide pour Pong classique.
            void SpawnObstacles_None();

            /// @brief 6 obstacles symetriques autour du centre. Layout equilibre.
            ///        Boost haut, Reduce bas, 2 Redirect, RandomDeflect, Magnetic.
            void SpawnObstacles_Classic();

            /// @brief 12 obstacles disperses aleatoirement (seed = 777, reproductible).
            ///        Types, tailles, vitesses et phases varies. Evite les zones raquette.
            void SpawnObstacles_Chaos();

            /// @brief 3 rangees horizontales d\'obstacles formant des corridors.
            ///        Alternance Boost/Redirect/Reduce avec decalages pour les passages.
            void SpawnObstacles_Gauntlet();

            /// @brief 4 paires de portails de teleportation disperses sur le terrain.
            ///        Plus quelques obstacles de deviation pour complexifier les trajectoires.
            void SpawnObstacles_Portal();

            /// @brief 1 grand aimant central mobile + anneau de 6 obstacles en orbite.
            ///        1 paire de portails aux coins. Combinaison de tous les effets.
            void SpawnObstacles_Boss();

            // ── Systeme de particules ─────────────────────────────────────────────────

            /// @brief Cree n particules a (px, py) avec couleur c et vitesse max spd.
            /// @param px      Position X de l\'emission.
            /// @param py      Position Y de l\'emission.
            /// @param c       Couleur de base des particules.
            /// @param n       Nombre de particules a creer.
            /// @param spd     Vitesse maximale (vitesse reelle = [0.3*spd, spd]).
            /// @param add     true = fusion additive (lueur), false = fusion alpha.
            void SpawnParticles(float px, float py, math::NkColor c,
                                int n, float spd, bool add = false);

            /// @brief Met a jour toutes les particules (deplacement, gravite, vie).
            ///        Supprime automatiquement les particules expirees.
            /// @param dt Delta-time en secondes.
            void UpdateParticles(float dt);

            /// @brief Met a jour la position et l\'etat de tous les obstacles actifs.
            /// @param dt Delta-time en secondes.
            void UpdateObstacles(float dt);

            /// @brief Gere la physique de la balle : deplacement, murs, buts, aimants,
            ///        collisions avec raquettes et obstacles.
            /// @param dt Delta-time en secondes.
            void UpdateBall(float dt);

            /// @brief Gere les deplacements des raquettes : input joueur et logique IA.
            ///        La difficulte affecte la vitesse, la reaction et l\'erreur de l\'IA.
            /// @param dt       Delta-time en secondes.
            /// @param upHeld   Touche haut maintenue.
            /// @param downHeld Touche bas maintenue.
            void UpdatePaddles(float dt, bool upHeld, bool downHeld);

            // ── Collisions ────────────────────────────────────────────────────────────

            /// @brief Teste et resout la collision balle-raquette.
            ///        Calcule l\'angle de rebond selon le point d\'impact sur la raquette.
            /// @param p Reference a la raquette a tester.
            /// @return true si collision detectee et resolue.
            bool BallVsPaddle(Paddle& p);

            /// @brief Teste et resout la collision balle-obstacle (index idx).
            ///        Gere la phase, la reflexion et appelle ApplyObstacleEffect.
            /// @param idx Index de l\'obstacle dans mObstacles.
            /// @return true si collision detectee et resolue.
            bool BallVsObstacle(int idx);

            /// @brief Applique l\'effet special de l\'obstacle idx apres une collision.
            ///        Dispatch selon ObstacleEffect : boost, reduce, redirect, etc.
            /// @param idx Index de l\'obstacle dans mObstacles.
            void ApplyObstacleEffect(int idx);

            // ── Navigation dans les menus ─────────────────────────────────────────────

            /// @brief Fait avancer le splach screen et bascule vers MainMenu quand termine.
            void UpdateSplash(float dt, bool anyKey);

            /// @brief Dessine l\'ecran de demarrage (Rihen + Noge + titre Pong).
            void RenderSplash();

            /// @brief Dessine les boutons tactiles UP / DOWN pour Android.
            void RenderTouchButtons();

            /// @brief Gere la navigation et les actions du menu principal.
            /// @param up    Navigation vers le haut (flanc).
            /// @param down  Navigation vers le bas (flanc).
            /// @param enter Confirmation (flanc).
            /// @param esc   Retour / annulation (flanc).
            void UpdateMainMenu(bool up, bool down, bool enter, bool esc);

            /// @brief Gere la selection de la difficulte IA.
            /// @param up    Navigation haut.
            /// @param down  Navigation bas.
            /// @param enter Confirme et retourne au menu principal.
            /// @param esc   Retourne au menu principal sans changer.
            void UpdateDiffSelect(bool up, bool down, bool enter, bool esc);

            /// @brief Gere la selection du preset d\'obstacles.
            /// @param up    Navigation haut.
            /// @param down  Navigation bas.
            /// @param enter Confirme et retourne au menu principal.
            /// @param esc   Retourne au menu principal sans changer.
            /// @param left  Reserve (navigation future).
            /// @param right Reserve (navigation future).
            void UpdateObsSelect(bool up, bool down, bool enter, bool esc,
                                bool left, bool right);

            /// @brief Gere le menu de pause (Reprendre / Menu principal).
            /// @param enter   Confirme l\'action selectionnee.
            /// @param esc     Reprend directement la partie.
            /// @param navUp   Navigation vers le haut (flanc).
            /// @param navDown Navigation vers le bas (flanc).
            void UpdatePauseMenu(bool enter, bool esc, bool navUp, bool navDown);

            // ── Rendu par couche ──────────────────────────────────────────────────────

            /// @brief Dessine le fond : couleur de base, etoiles, grille, ligne centrale.
            void RenderBackground();

            /// @brief Dessine les deux raquettes avec lueur et contour.
            void RenderPaddles();

            /// @brief Dessine la balle avec trail, lueur et feedback de charge.
            void RenderBall();

            /// @brief Dessine tous les obstacles actifs avec icones d\'effet et lueurs.
            void RenderObstacles();

            /// @brief Dessine toutes les particules vivantes.
            void RenderParticles();

            /// @brief Dessine le HUD en jeu : scores, vitesse, difficulte, touche pause.
            void RenderHUD();

            /// @brief Dessine l\'ecran de menu principal.
            void RenderMainMenu();

            /// @brief Dessine l\'ecran de selection de difficulte.
            void RenderDiffSelect();

            /// @brief Dessine l\'ecran de selection d\'obstacles avec preview miniature.
            void RenderObsSelect();

            /// @brief Dessine l\'overlay de pause sur la scene courante.
            void RenderPauseOverlay();

            /// @brief Dessine le flash visuel apres un but.
            void RenderGoalFlash();

            /// @brief Dessine l\'ecran de fin de partie.
            void RenderGameOver();

            // ── Helpers UI ────────────────────────────────────────────────────────────

            /// @brief Dessine un panneau rectangulaire avec fond semi-transparent et bordure.
            /// @param x       Position X coin superieur gauche.
            /// @param y       Position Y coin superieur gauche.
            /// @param w       Largeur.
            /// @param h       Hauteur.
            /// @param fill    Couleur de fond (avec alpha pour semi-transparence).
            /// @param border  Couleur de la bordure.
            /// @param bw      Epaisseur de la bordure en pixels.
            void DrawPanel(int x, int y, int w, int h,  math::NkColor fill, math::NkColor border, int bw = 2);

            /// @brief Dessine un bouton interactif avec texte blanc centre.
            ///        L\'etat selectionne ajoute une lueur et une bordure plus visible.
            /// @param x    Position X.
            /// @param y    Position Y.
            /// @param w    Largeur.
            /// @param h    Hauteur.
            /// @param text Texte a afficher (toujours blanc pour lisibilite maximale).
            /// @param sel  true = bouton selectionne (mise en evidence).
            /// @param col  Couleur d\'accent pour fond et lueur.
            /// @param ts   Echelle du texte (defaut 2).
            void DrawButton(int x, int y, int w, int h, const char* text, bool sel, math::NkColor col, int ts = 2);

            /// @brief Dessine un champ d\'etoiles scintillantes en fond d\'ecran.
            ///        Positions fixes (seed 0xABCD), luminosite modulee par le temps.
            /// @param time Temps global du jeu (secondes).
            void DrawStars(float time);

            /// @brief Dessine une mini-representation du terrain avec les obstacles
            ///        du preset indique, dans un rectangle de (pw x ph) pixels a (px, py).
            /// @param px     Position X de la preview.
            /// @param py     Position Y de la preview.
            /// @param pw     Largeur de la preview.
            /// @param ph     Hauteur de la preview.
            /// @param preset Preset dont les obstacles sont a afficher.
            void DrawObstaclePreview(int px, int py, int pw, int ph, ObstaclePreset preset);

            float UIScale() const noexcept;

            // ── Donnees membres ───────────────────────────────────────────────────────

            /// @brief Renderer double-buffere cible (non possede par PongGame).
            NkRenderer&  mRenderer;

            /// @brief Rasterizer 2D lie au renderer.
            NkRasterizer mRasterizer;

            /// @brief Generateur de nombres aleatoires Mersenne Twister (seed fixe = 42).
            std::mt19937 mRng;

            // ── Entites de jeu ────────────────────────────────────────────────────────

            /// @brief Etat et physique de la balle.
            Ball   mBall;

            /// @brief Raquette du joueur (cote gauche).
            Paddle mPlayer;

            /// @brief Raquette de l\'IA (cote droit).
            Paddle mAI;

            /// @brief Liste dynamique des obstacles actifs.
            std::vector<Obstacle> mObstacles;

            /// @brief Liste dynamique des particules visuelles.
            std::vector<Particle> mParticles;

            // ── Configuration et etat du jeu ──────────────────────────────────────────

            /// @brief Parametres courants de la partie (difficulte, preset, score max).
            GameSettings mSettings;

            /// @brief Etat courant du cycle de vie du jeu.
            GameState mState = GameState::MainMenu;

            // ── Navigation UI ─────────────────────────────────────────────────────────

            /// @brief Item selectionne dans le menu principal (0=Jouer..3=Quitter).
            int mMainMenuSel = 0;

            /// @brief Indice de difficulte selectionne dans SelectDifficulty (0..3).
            int mDiffSel = 1;

            /// @brief Indice de preset selectionne dans SelectObstacles (0..5).
            int mObsSel = 1;

            /// @brief Anti-repetition pour la navigation clavier (secondes).
            ///        Navigation bloquee tant que > 0. Decremente chaque frame.
            float mInputRepeat = 0.0f;

            // ── Gestion des buts ──────────────────────────────────────────────────────

            /// @brief Duree restante de l\'effet GoalFlash (secondes).
            float mGoalTimer = 0.0f;

            /// @brief Identifiant du dernier marqueur (0 = joueur, 1 = IA).
            int mLastScorer = 0;

            // ── Effets d\'ecran ────────────────────────────────────────────────────────

            /// @brief Intensite courante de l\'ecran-secousse (decroissance automatique).
            float mShake = 0.0f;

            /// @brief Opacite courante du flash d\'ecran global [0..1].
            float mFlashAlpha = 0.0f;

            /// @brief Couleur du flash d\'ecran global.
            math::NkColor mFlashColor = { 255, 255, 255, 255 };

            // ── Timers ────────────────────────────────────────────────────────────────

            /// @brief Temps total ecoule depuis le debut de la session (secondes).
            float mTime = 0.0f;

            /// @brief Minuteur pour le rafraichissement de l\'erreur IA.
            float mAiErrTimer = 0.0f;

            /// @brief Item selectionne dans le menu de pause (0=Reprendre, 1=Menu).
            int mPauseSel = 0;

            // ── Splash screen ─────────────────────────────────────────────────────────

            /// @brief Duree restante du splash screen (secondes). Auto-avance a 0.
            float mSplashTimer = 3.0f;

            // ── Touch (Android) ───────────────────────────────────────────────────────

            /// @brief true = afficher les boutons tactiles UP/DOWN a l\'ecran.
            bool mShowTouchButtons = false;

            // ── Pointeur (souris / tactile) pour navigation menus ─────────────────────

            /// @brief Position X du pointeur en coordonnees client (-1 = inconnu).
            int  mMouseX     = -1;

            /// @brief Position Y du pointeur en coordonnees client (-1 = inconnu).
            int  mMouseY     = -1;

            /// @brief true = clic gauche (ou tap) detecte ce frame (flanc montant).
            bool mMouseClick = false;

    }; // class PongGame

} // namespace nkentseu


// =============================================================================
// EXEMPLES D\'UTILISATION
// =============================================================================
/*

// ── Exemple 1 : Boucle de jeu minimale ────────────────────────────────────────
//
// Cree une fenetre, un renderer et un jeu. Lance la boucle principale.
// Le jeu demarre automatiquement sur le menu principal.
//
//   #include "PongGame.h"
//   using namespace nkentseu;
//
//   int nkmain(const NkEntryState& state)
//   {
//       (void)state;
//
//       // Creer et configurer la fenetre
//       NkWindowConfig cfg;
//       cfg.title  = "Mon Pong";
//       cfg.width  = 1280;
//       cfg.height = 720;
//       NkWindow window;
//       window.Create(cfg);
//
//       // Initialiser le renderer double-buffere
//       NkRenderer renderer;
//       renderer.Init(window);
//
//       // Creer et initialiser le jeu
//       PongGame game(renderer);
//       game.Init();
//
//       // Variables d\'input
//       bool up = false, down = false;
//       bool enter = false, escape = false;
//       bool left = false, right = false;
//       bool prevEnter = false, prevEsc = false;
//
//       bool running = true;
//       NkChrono chrono;
//
//       while (running)
//       {
//           float dt = chrono.Reset().milliseconds / 1000.0f;
//           dt = std::min(dt, 0.033f); // Clamp a 30 FPS minimum
//
//           // Traiter les evenements systeme
//           while (NkEvent* ev = NkEvents().PollEvent())
//           {
//               if (ev->As<NkWindowCloseEvent>()) running = false;
//               // ... gestion clavier ...
//           }
//
//           // Calculer les flancs montants (enter, escape)
//           enter  = enterHeld  && !prevEnter;
//           escape = escapeHeld && !prevEsc;
//           prevEnter = enterHeld;
//           prevEsc   = escapeHeld;
//
//           // Mettre a jour et dessiner
//           game.Update(dt, up, down, enter, escape, left, right);
//           game.Render();
//           renderer.Present();
//
//           // Limiter a 60 FPS
//           if (chrono.Elapsed().milliseconds < 16)
//               NkChrono::Sleep(16 - chrono.Elapsed().milliseconds);
//       }
//
//       window.Close();
//       return 0;
//   }


// ── Exemple 2 : Demarrer une partie en Expert avec obstacles Boss ─────────────
//
// Depuis le menu principal, le joueur navigue vers SelectDifficulty,
// choisit Expert, puis SelectObstacles et choisit Boss.
// Les indices correspondent aux valeurs des enumerations :
//   AIDifficulty::Expert  -> index 3
//   ObstaclePreset::Boss  -> index 5
//
// Cela peut aussi etre fait programmatiquement avant Init() si on expose
// les membres ou ajoute des setters :
//
//   // Methode recommandee : navigation UI classique
//   // L\'utilisateur navigue dans les menus avec les fleches et Entree.
//
//   // Alternative : definir les parametres directement avant StartGame()
//   // (necessite un setter ou un acces ami)
//   // game.SetSettings({ AIDifficulty::Expert, ObstaclePreset::Boss, 7 });
//   // game.StartGame();


// ── Exemple 3 : Redimensionnement de la fenetre ────────────────────────────────
//
// Quand la fenetre est redimensionnee, utiliser OnResize() au lieu de Init()
// pour conserver l'etat courant du jeu :
//
//   if (needResize)
//   {
//       renderer.Resize(window, newWidth, newHeight);
//       game.OnResize();  // Repositionne sans changer l'etat du jeu
//       needResize = false;
//   }


// ── Exemple 4 : Detecter la fin de partie depuis Apps.cpp ────────────────────
//
// PongGame::GetState() expose l\'etat courant. Utile pour declencher
// des evenements externes (son, enregistrement de score, etc.) :
//
//   if (game.GetState() == GameState::GameOver)
//   {
//       // Enregistrer le score, jouer un son de victoire, etc.
//   }


// ── Exemple 5 : Description des niveaux de difficulte IA ─────────────────────
//
//   AIDifficulty::Easy   : reaction=28%, erreur=+-72px, rafraichit toutes 0.8s
//                          L\'IA rate souvent des balles simples.
//
//   AIDifficulty::Medium : reaction=52%, erreur=+-38px, rafraichit toutes 1.2s
//                          IA competitive mais battable avec une bonne strategie.
//
//   AIDifficulty::Hard   : reaction=73%, erreur=+-16px, prediction=0.09s,
//                          rafraichit toutes 1.8s
//                          IA reactive, necessite precision et anticipation.
//
//   AIDifficulty::Expert : reaction=88%, erreur=+-5px, prediction=0.16s,
//                          rafraichit toutes 2.5s
//                          Tres difficile mais battable en exploitant les angles.


// ── Exemple 6 : Description des presets d\'obstacles ─────────────────────────
//
//   ObstaclePreset::None     : Terrain vide. Pong pur, depends uniquement des
//                              raquettes et des murs.
//
//   ObstaclePreset::Classic  : Layout symetrique au centre.
//                              1 SpeedBoost (haut, mobile), 1 SpeedReduce (bas),
//                              2 Redirect (flancs), 1 RandomDeflect, 1 Magnetic.
//
//   ObstaclePreset::Chaos    : 12 obstacles disperses aleatoirement.
//                              Positions reproductibles (seed=777).
//                              Types varies, certains mobiles, certains avec phase.
//
//   ObstaclePreset::Gauntlet : Rangees d\'obstacles formant des corridors.
//                              Rangee 1 (haut) : 2 SpeedBoost
//                              Rangee 2 (centre) : 3 Redirect avec phase
//                              Rangee 3 (bas) : 2 SpeedReduce mobiles
//
//   ObstaclePreset::Portal   : 4 paires de portails disperses.
//                              Positions variers (haut-gauche <-> bas-droite, etc.)
//                              Plus 2 obstacles de deviation.
//
//   ObstaclePreset::Boss     : Grand aimant central mobile.
//                              Anneau de 6 obstacles en orbite (vitesse orbitale).
//                              1 paire de portails aux coins.

*/
// =============================================================================
// FIN DES EXEMPLES
// =============================================================================