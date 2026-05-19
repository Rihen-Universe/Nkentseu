#pragma once
// =============================================================================
// GameplayScene.h
// -----------------------------------------------------------------------------
// Boucle de jeu 1v1 local — reproduction fidele du mockup HTML
// (docs/02_terrain_de_jeu_1v1.html).
//
// Specs HTML respectees a l'identique (valeurs en pixels par frame a 60 fps,
// converties en pixels/seconde via dt*60 cote C++) :
//   - Balle : vx=3.5, vy=2.2 (init), rayon 7, trail 14 positions
//   - Paddles : 10 x 60 px, vitesse 4
//   - Bounce paddle : vy = rel * 4.5 ; speed ×1.04 capped a 9
//   - Goal : cooldown 800 ms, reset vx=±3.5, vy=(rnd-0.5)*4
//   - Layout : HUD top 56 px, HUD bottom 52 px, terrain entre les deux
//   - Couleurs : P1 cyan #00F5FF, P2 orange #FF6B00
//
// Controles (clavier) :
//   - W / S         : paddle gauche (P1) haut / bas
//   - Fleche haut/bas : paddle droite (P2)
//   - Espace        : pause / reprise
//   - Echap         : si paused -> retour menu (Pop), sinon -> pause
//
// PAS d'obstacles / power-ups / particules dans cette tranche minimale.
// =============================================================================

#include "Pong/UI/Scene.h"
#include "Pong/Game/GameTypes.h"
#include "Pong/Game/AIController.h"
#include "Pong/Game/ObstacleSystem.h"
#include "Pong/Game/PowerUpSystem.h"
#include "Pong/Game/ParticleSystem.h"
#include "NKMath/NkColor.h"

namespace nkentseu
{
    namespace pong
    {

        class GameplayScene : public Scene
        {
        public:
            GameplayScene()  = default;
            ~GameplayScene() override = default;

            const char* Name() const noexcept override { return "Gameplay"; }

            void OnEnter (AppContext& ctx) override;
            void OnUpdate(AppContext& ctx, float dt) override;
            void OnRender(AppContext& ctx) override;
            void OnEvent (AppContext& ctx, NkEvent& ev) override;
            void OnPause (AppContext& ctx) override;

        private:
            // ── Scale UI/gameplay ───────────────────────────────────────────
            // Calcule a chaque frame depuis le viewport via UIScale.h. Toutes
            // les dimensions HTML (paddle, balle, vitesses, HUD) sont
            // multipliees par mScale pour rester visuellement coherent sur
            // n'importe quel ecran (desktop ou mobile haute densite).
            float mScale  = 1.0f;
            float mHUDTopH = 56.0f;  // = kHUDTopH * mScale, recalcule
            float mHUDBotH = 52.0f;

            // ── Etat balle ──────────────────────────────────────────────────
            // Vitesses stockees en "px par frame a 60 fps" * mScale (= valeurs
            // HTML mises a l'echelle). On applique pos += vel * (dt * 60) pour
            // conserver le comportement exact du mockup peu importe le
            // framerate.
            float mBallX = 0.0f, mBallY = 0.0f;
            float mBallVX = 0.0f, mBallVY = 0.0f;
            float mBallR  = 7.0f;  // = 7 * mScale
            // Multiplicateur de vitesse global pour ce match (>= 1.0). Lu
            // depuis GameSettings.ballSpeedMul en OnEnter et reapplique a
            // chaque respawn de la balle.
            float mBallSpeedMul = 1.0f;
            // Trail circulaire — 14 positions (taille HTML).
            static constexpr int kTrailMax = 14;
            float mTrailX[kTrailMax] = {0};
            float mTrailY[kTrailMax] = {0};
            int   mTrailHead   = 0;   ///< prochain index a ecrire
            int   mTrailCount  = 0;   ///< nombre de positions valides

            // ── Paddles ─────────────────────────────────────────────────────
            // x,y = coin haut-gauche. w,h = taille (deja multipliee par mScale).
            float mPaddleLX = 0.0f, mPaddleLY = 0.0f;
            float mPaddleRX = 0.0f, mPaddleRY = 0.0f;
            float mPaddleW  = 10.0f, mPaddleH = 60.0f;  // * mScale
            float mPaddleSpd = 4.0f;                    // * mScale

            // ── Score / etat match ──────────────────────────────────────────
            int   mScoreL = 0;
            int   mScoreR = 0;
            bool  mPaused = false;
            float mGoalCooldown = 0.0f;  // secondes restantes
            float mGoalFlashAlpha = 0.0f;
            int   mGoalFlashSide  = 0;   // -1=left scored, +1=right scored

            // Timer (descendant si timeLimit > 0, ascendant si timeLimit = 0)
            float mTimeLeft = 0.0f;      // si timeLimit > 0
            float mTimeUp   = 0.0f;      // si timeLimit = 0 (chrono ascendant)

            // ── Fin de partie ────────────────────────────────────────────────
            bool  mGameOver = false;
            int   mWinner   = 0;         // +1 = P1 gagne, -1 = P2 gagne, 0 = egalite

            // Geometrie boutons GameOver
            float mGOReplayBtnX = 0.0f, mGOReplayBtnY = 0.0f;
            float mGOReplayBtnW = 0.0f, mGOReplayBtnH = 0.0f;
            float mGOMenuBtnX   = 0.0f, mGOMenuBtnY   = 0.0f;
            float mGOMenuBtnW   = 0.0f, mGOMenuBtnH   = 0.0f;

            // ── Etat input (touches tenues) ─────────────────────────────────
            bool mKeyW = false, mKeyS = false;
            bool mKeyUp = false, mKeyDown = false;

            // ── IA pour les modes vs IA / IA vs IA ──────────────────────────
            AIController mAILeft;
            AIController mAIRight;
            bool         mAILeftEnabled  = false;
            bool         mAIRightEnabled = false;

            // ── Obstacles in-game (8 types, configurable via settings) ──────
            ObstacleSystem mObstacles;

            // ── Power-ups / Power-downs (6 bonus + 6 malus selon GDD §2.3) ──
            // Etat des effets actifs, drops periodiques et collision avec
            // les raquettes. Module la taille raquette, sa vitesse, la vitesse
            // de balle, et intercepte les goals (Shield / DoublePoint).
            PowerUpSystem mPowerUps;
            /// Cote du DERNIER joueur ayant touche la balle (-1 = P1 gauche,
            /// +1 = P2 droite, 0 = personne encore). Utilise pour attribuer
            /// le bonus quand une BonusStar est collectee.
            int mLastToucher = 0;

            // ── Particules (juice visuel) ──────────────────────────────────
            // Bursts emis aux evenements gameplay : rebond paddle, hit
            // obstacle, collecte bonus, drop catched, goal. Purement
            // cosmetique — n'influe sur aucun gameplay.
            ParticleSystem mParticles;

            // ── Touch / souris (drag vertical par moitie) ────────────────────
            long long mTouchIdL    = -1;
            long long mTouchIdR    = -1;
            bool      mMouseDownL  = false;
            bool      mMouseDownR  = false;
            float     mLastTouchYL = 0.0f;
            float     mLastTouchYR = 0.0f;
            float     mLastMouseY  = 0.0f;

            // ── Geometrie des boutons cliquables (sync chaque frame) ─────────
            // Pour le bouton PAUSE dans le HUD + boutons de l'overlay pause.
            // Permet le hit-test dans OnEvent sans recalculer le layout.
            float mPauseBtnX  = 0.0f, mPauseBtnY  = 0.0f;
            float mPauseBtnW  = 0.0f, mPauseBtnH  = 0.0f;
            float mResumeBtnX = 0.0f, mResumeBtnY = 0.0f;
            float mResumeBtnW = 0.0f, mResumeBtnH = 0.0f;
            float mMenuBtnX   = 0.0f, mMenuBtnY   = 0.0f;
            float mMenuBtnW   = 0.0f, mMenuBtnH   = 0.0f;

            // ── Reseau (Phase 2) ────────────────────────────────────────────
            // Active si ctx.network est Connected a l'entree. Le host fait la
            // simulation autoritative, le client se contente d'envoyer son
            // input et d'appliquer les snapshots recus tel quel.
            bool  mIsNetwork = false;
            bool  mIsHost    = false;
            // Timers d'envoi (accumulent dt jusqu'a depasser 1/Hz).
            float mNetSendStateTimer = 0.0f;  ///< Host : envoi du snapshot
            float mNetSendInputTimer = 0.0f;  ///< Client : envoi de l'input
            // Position Y normalisee [0..1] desiree par le client pour son
            // propre paddle (cote droit chez le host). Supporte clavier ET
            // souris ET touch en envoyant une position absolue (pas un dir).
            // - mNetRemotePaddleYN : ce que le HOST a recu en dernier
            // - mNetLocalPaddleYN  : ce que le CLIENT va envoyer
            float mNetRemotePaddleYN = 0.5f;
            float mNetLocalPaddleYN  = 0.5f;
            // Flag : true si le client a deja envoye au moins un input. Sinon
            // le HOST utilise sa position locale par defaut (centre arene).
            bool  mNetHasRemoteInput = false;

            // Detection du depart de l'adversaire (peer disconnect en cours
            // de match). Si l'autre joueur ferme son app, perd la connexion,
            // ou appuie sur RETOUR MENU, on bascule en mNetPeerLost et on
            // affiche un overlay "ADVERSAIRE PARTI" + bouton RETOUR MENU.
            bool  mNetPeerLost  = false;
            float mNetLostTimer = 0.0f;  ///< Temps depuis la perte (pour anim)
            // Coords du bouton RETOUR MENU dans l'overlay de deconnexion.
            float mLostMenuBtnX = 0.0f, mLostMenuBtnY = 0.0f;
            float mLostMenuBtnW = 0.0f, mLostMenuBtnH = 0.0f;

            // ── Sync power-ups (Phase 5) ────────────────────────────────────
            // Cote CLIENT, le HOST envoie ces valeurs dans PktState a chaque
            // snapshot 60Hz. Le CLIENT les utilise pour rendre les paddles a
            // la bonne taille et appliquer les effets visuels (gel/aveuglement
            // de la zone, etc.). En mode local, ces valeurs sont ignorees ;
            // on utilise mPowerUps.GetPaddleHeightMul directement.
            float mNetPaddleHMulL = 1.0f;
            float mNetPaddleHMulR = 1.0f;
            uint8 mNetEffFlagsL   = 0;
            uint8 mNetEffFlagsR   = 0;

            // Helper : hit-test rectangle.
            static bool PointInRect(float px, float py,
                                    float rx, float ry, float rw, float rh)
            {
                return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
            }

            // ── Helpers ─────────────────────────────────────────────────────
            void ResetPositions(float arenaW, float arenaH);
            void TriggerGoal(AppContext& ctx, int side, float arenaW, float arenaH);
            void StepBall   (AppContext& ctx, float arenaW, float arenaH, float dt60);
            /// Pause network-aware. Mode local : toggle classique. Mode reseau
            /// CLIENT : envoie kMsgPauseToggle au host (qui togglera et
            /// repropagera via snapshot). Mode reseau HOST : toggle local
            /// (snapshot propage au client).
            void RequestPauseToggle(AppContext& ctx);
            /// Replay network-aware. Mode local OU HOST : StartNewMatch direct.
            /// Mode reseau CLIENT : envoie kMsgReplayRequest au HOST (qui
            /// fera StartNewMatch et propagera via snapshot).
            void RequestReplay(AppContext& ctx);
            /// Multiplicateur de hauteur paddle effectif par cote :
            /// - mode local OU HOST : mPowerUps.GetPaddleHeightMul(side)
            /// - mode reseau CLIENT : mNetPaddleHMulL/R recu du snapshot
            ///   (mPowerUps cote client n'a pas les effets, seulement les drops).
            float EffectivePaddleHMul(int side) const
            {
                if (mIsNetwork && !mIsHost)
                {
                    return (side < 0) ? mNetPaddleHMulL : mNetPaddleHMulR;
                }
                return mPowerUps.GetPaddleHeightMul(side);
            }
            void StepPaddles(float arenaH, float dt60);
            /// Verifie les conditions de victoire (score >= maxScore avec/sans
            /// winByTwo, ou timer ecoule). Met mGameOver+mWinner.
            void CheckWinConditions(const GameSettings& s);
            /// Reset complet pour rejouer (scores 0, ball au centre).
            void StartNewMatch(AppContext& ctx);
        };

    } // namespace pong
} // namespace nkentseu
