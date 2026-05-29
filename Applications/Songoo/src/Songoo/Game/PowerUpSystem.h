#pragma once
// =============================================================================
// PowerUpSystem.h
// -----------------------------------------------------------------------------
// Systeme de power-ups (bonus) et power-downs (malus) selon GDD §2.3.
//
// Deux mecaniques de declenchement :
//   1. Collecte d'une BonusStar (obstacle existant) : applique un bonus
//      aleatoire au DERNIER joueur ayant touche la balle.
//   2. Drop periodique : un orbe descend du haut du terrain, le joueur qui
//      le percute avec sa raquette declenche son effet.
//        - orbe BONUS  -> bonus applique au joueur qui a touche l'orbe
//        - orbe MALUS  -> malus applique a l'ADVERSAIRE du joueur (cf. GDD :
//                         "malus = infliges a l'adversaire")
//
// 12 effets implementes (cf. BonusType / MalusType dans GameTypes.h) :
//   Bonus : GiantPaddle, PaddleSpeed, DoublePoint, Shield, SlowBall, RandomStar
//   Malus : Blind, MiniPaddle, InvertControls, Freeze, FastBall, TeleportPaddle
//
// La scene Gameplay interroge ce systeme chaque frame pour moduler la taille
// de raquette, sa vitesse, la vitesse de balle, etc. Quand un goal est marque,
// elle consulte ConsumeShield / ConsumeDoublePoint avant de l'enregistrer.
// =============================================================================

#include "Songoo/Game/GameTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkColor.h"
#include "NKCore/NkTypes.h"

namespace nkentseu
{
    namespace songoo
    {

        class GLRenderer2D;
        class FontAtlas;

        // ── Effet actif sur un joueur ────────────────────────────────────────
        // Stocke a la fois bonus et malus dans une meme liste (le flag isBonus
        // distingue). `kind` : index dans BonusType ou MalusType selon isBonus.
        // ─────────────────────────────────────────────────────────────────────
        struct ActiveEffect
        {
            bool   isBonus;        ///< true = BonusType, false = MalusType
            uint8  kind;           ///< cast en BonusType ou MalusType
            int    side;           ///< -1 = P1 (gauche), +1 = P2 (droite)
            float  duration;       ///< duree initiale (pour le rendu de la jauge HUD)
            float  timeLeft;       ///< temps restant (s)
            int    charges;        ///< Shield/DoublePoint : utilisations restantes
        };

        // ── Orbe drop qui derive du haut vers un paddle ─────────────────────
        // Le drop spawn au-dessus du centre puis derive en DIAGONALE vers un
        // paddle aleatoire (gauche ou droit). Sans cette derive, les drops
        // tombaient verticalement entre les paddles -> impossible a attraper.
        struct PowerUpDrop
        {
            bool   isBonus;
            uint8  kind;           ///< si isBonus: BonusType, sinon MalusType
            float  x, y;           ///< centre
            float  r;              ///< rayon
            float  vx;             ///< vitesse horizontale (px/sec) -> derive vers paddle
            float  vy;             ///< vitesse de chute (px/sec)
            bool   alive;
            float  pulse;          ///< accumulateur d'anim
        };

        class PowerUpSystem
        {
        public:
            void Reset();
            void Update(float dt, float arenaW, float arenaH, float scale);
            void Render(GLRenderer2D& r, FontAtlas& f,
                        float arenaOX, float arenaOY, float scale) const;

            // ── Application d'un effet ──────────────────────────────────────
            /// Bonus -> applique au COLLECTEUR. Si l'effet est deja actif, la
            /// duree est reset (pas de cumul).
            void ApplyBonus(BonusType b, int collectorSide);
            /// Malus -> applique au TARGET (= adversaire du collecteur).
            void ApplyMalus(MalusType m, int targetSide);
            /// Bonus aleatoire (tirage uniforme parmi les BonusType).
            void ApplyRandomBonus(int collectorSide);
            /// Malus aleatoire (tirage uniforme parmi les MalusType).
            void ApplyRandomMalus(int targetSide);

            // ── Drops periodiques ───────────────────────────────────────────
            /// Spawn force un orbe random au top de l'arene.
            void SpawnRandomDrop(float arenaW);
            /// Teste collision drops/paddle. Si touche : applique l'effet,
            /// detruit le drop, retourne true. Le caller passe le cote du
            /// paddle (-1 ou +1) et son AABB ECRAN.
            /// Si @p outX/outY non null, ils recoivent la position monde du
            /// drop touche (utile pour emettre des particules au point d'impact).
            /// Si @p outColor non null, il recoit la couleur du drop touche.
            bool CheckPaddleCollision(int side,
                                      float paddleX, float paddleY,
                                      float paddleW, float paddleH,
                                      float* outX = nullptr,
                                      float* outY = nullptr,
                                      math::NkColor* outColor = nullptr);

            // ── Query pour GameplayScene ────────────────────────────────────
            float GetPaddleHeightMul(int side) const;
            float GetPaddleSpeedMul (int side) const;
            float GetBallSpeedMul   () const;
            bool  IsBlind           (int side) const;
            bool  IsFrozen          (int side) const;
            bool  HasInvertedControls(int side) const;
            /// Consomme un charge Shield sur @p side. Retourne true si shield
            /// etait actif (le caller doit alors annuler le goal).
            bool  ConsumeShield     (int side);
            /// Consomme une charge DoublePoint. Retourne true si actif (goal
            /// compte double).
            bool  ConsumeDoublePoint(int side);
            /// Si TeleportPaddle est en attente sur @p side, met @p outFracY
            /// (0..1, position cible dans l'arene) et retourne true. Le flag
            /// est consomme.
            bool  TryTeleport       (int side, float& outFracY);

            // ── Acces HUD ──────────────────────────────────────────────────
            int                  EffectCount() const noexcept { return (int)mEffects.Size(); }
            const ActiveEffect&  GetEffect(int i) const       { return mEffects[i]; }
            int                  DropCount  () const noexcept { return (int)mDrops.Size(); }
            const PowerUpDrop&   GetDrop  (int i) const       { return mDrops[i]; }
            /// Couleur de l'effet @p i (pour le rendu HUD).
            math::NkColor        GetEffectColor(int i) const;
            /// Nom court affichable de l'effet @p i (ex: "BOUCLIER", "GEL").
            const char*          GetEffectName (int i) const;

            // ── Sync reseau (Phase 5) ──────────────────────────────────────
            /// Remplace la liste interne des drops avec ceux fournis. Utilise
            /// cote CLIENT pour reconstruire l'etat visuel depuis le snapshot
            /// HOST. NE TOUCHE PAS aux effets actifs ni au timer de spawn (le
            /// HOST autoritatif gere ces aspects ; le CLIENT ne fait que rendu).
            void SetNetDrops(const PowerUpDrop* drops, int count);

            /// Cote CLIENT reseau : injecte une notification recue du HOST.
            /// @p timeLeft : duree restante en secondes (0 = clear la notif).
            void SetNetNotification(int side, bool isBonus, uint8 kind, float timeLeft);

            // ── Collision balle <-> drops ──────────────────────────────────
            /// Teste collision cercle-cercle entre la balle (bx, by, br) et
            /// chaque drop actif. A la collision, transfere une fraction du
            /// momentum de la balle au drop : il part en diagonale (typiquement
            /// vers le paddle adverse selon la direction de la balle).
            /// La balle n'est PAS modifiee (le drop est passif).
            /// @param ballVX/VY Velocite balle en px/frame@60fps (convertie en
            ///                  px/sec interne via *60). Compatible avec
            ///                  mBallVX/VY de GameplayScene.
            void CheckBallCollision(float bx, float by, float br,
                                    float ballVX, float ballVY);

            // ── Notifications toast ────────────────────────────────────────
            // Quand un effet est applique, on remplit la slot du cote
            // concerne. Le caller (GameplayScene) dessine un bandeau popup
            // pendant kNotifDuration secondes puis le slot s'eteint.
            struct Notification
            {
                bool   active   = false;
                bool   isBonus  = false;
                uint8  kind     = 0;
                int    side     = 0;
                float  timeLeft = 0.0f;
                float  duration = 1.5f;
            };
            static constexpr float kNotifDuration = 1.5f;
            const Notification&  GetNotification(int side) const
            { return mNotifs[SideIdx(side)]; }
            math::NkColor        NotifColor(const Notification& n) const;
            const char*          NotifName (const Notification& n) const;

        private:
            NkVector<ActiveEffect> mEffects;
            NkVector<PowerUpDrop>  mDrops;
            float                  mDropTimer       = 12.0f;   ///< delay avant 1er spawn
            float                  mDropPeriodMin   = 8.0f;
            float                  mDropPeriodMax   = 15.0f;
            // TeleportPaddle est "instantane" : on memorise le side cible +
            // la position Y. Le GameplayScene appelle TryTeleport pour appliquer.
            bool                   mTeleportPending[2] = { false, false };
            float                  mTeleportFracY  [2] = { 0.5f, 0.5f };
            // Slot de notification par cote (index 0 = side -1, 1 = side +1).
            // Une nouvelle application overwrite la slot en cours.
            Notification           mNotifs[2];

            // ── Helpers internes ──────────────────────────────────────────
            int    SideIdx(int side) const { return (side > 0) ? 1 : 0; }
            float  RandPeriod();
            void   ScheduleNextDrop();
            void   AddEffect(bool isBonus, uint8 kind, int side,
                             float duration, int charges = 0);
            /// Supprime tout effet (isBonus, kind, side) deja present (refresh).
            void   RemoveEffectsByKind(bool isBonus, uint8 kind, int side);
            /// True si effet correspondant actif sur side.
            bool   HasEffect(bool isBonus, uint8 kind, int side) const;
        };

    } // namespace songoo
} // namespace nkentseu
