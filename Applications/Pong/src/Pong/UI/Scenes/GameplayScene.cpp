// =============================================================================
// GameplayScene.cpp
// -----------------------------------------------------------------------------
// Implementation — reproduit 1:1 le comportement de docs/02_terrain_de_jeu_1v1.html
// (sans obstacles ni power-ups dans cette tranche minimale).
// =============================================================================

#include "GameplayScene.h"
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "Pong/Net/NetworkSession.h"
#include "Pong/Net/NetProtocol.h"
#include "NKNetwork/Protocol/NkConnection.h"  // pour NkReceiveMsg
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <cstring>

namespace nkentseu
{
    namespace pong
    {

        // ── Constantes HTML brutes (px/frame a 60 fps, dimensions a 1280x720) ─
        // Multipliees par mScale lors de l'usage pour rester visuellement
        // proportionnelles a n'importe quel viewport (cf. UIScale.h).
        static constexpr float kHUDTopHBase  = 56.0f;
        static constexpr float kHUDBotHBase  = 52.0f;
        // ── Vitesses balle : une SEULE constante de reference ───────────────
        // kBallBaseSpeed = norme de la vitesse INITIALE de la balle au coup
        // d'envoi (px/frame @60fps, avant scale UI). Tous les autres seuils
        // (min, cap, rebond...) sont exprimes comme multiplicateurs >= 1 de
        // cette reference. Pour rythmer ou ralentir GLOBALEMENT le jeu, il
        // suffit de toucher kBallBaseSpeed. Les multiplicateurs garantissent
        // visuellement (et conceptuellement) qu'aucune vitesse ne descend
        // sous la vitesse initiale.
        static constexpr float kBallBaseSpeed = 4.13f;      // = sqrt(3.5^2 + 2.2^2) ~ ancien init
        // Decomposition X/Y telle que ||(Vx,Vy)|| == kBallBaseSpeed (angle ~32°).
        static constexpr float kBallVxInit    = kBallBaseSpeed * 0.847f;   // 3.5
        static constexpr float kBallVyInit    = kBallBaseSpeed * 0.532f;   // 2.2
        // Multiplicateurs >= 1 (invariant : aucune vitesse < init).
        static constexpr float kMinSpeedMul   = 1.10f;
        static constexpr float kBounceVyMul   = 1.10f;
        static constexpr float kSpeedCapMul   = 2.20f;
        static constexpr float kBounceMul     = 1.04f;       // sans dimension (acceleration par hit)
        // Valeurs derivees (pour rester compatible avec le reste du code).
        static constexpr float kBounceVyBase  = kBallBaseSpeed * kBounceVyMul;
        static constexpr float kSpeedCapBase  = kBallBaseSpeed * kSpeedCapMul;
        // Vitesse minimum garantie (px/frame @60fps). Empeche la balle de
        // s'immobiliser dans un vortex Gravity ou de "tomber" en speed apres
        // une serie de collisions. Le jeu doit toujours rester rythme.
        static constexpr float kMinSpeed      = kBallBaseSpeed * kMinSpeedMul;
        // Invariant : chaque seuil >= vitesse init.
        static_assert(kMinSpeedMul >= 1.0f, "min speed must be >= init");
        static_assert(kBounceVyMul >= 1.0f, "bounce Vy must be >= init");
        static_assert(kSpeedCapMul >= 1.0f, "speed cap must be >= init");

        static constexpr float kGoalCooldown = 0.8f;        // secondes (pas de scale)
        static constexpr float kPaddleWBase  = 10.0f;
        static constexpr float kPaddleHBase  = 60.0f;
        static constexpr float kBallRBase    =  7.0f;
        static constexpr float kPaddleSpdBase=  4.0f;
        static constexpr float kPaddleXMargin= 18.0f;       // x=18 et x=W-28 (-10 paddle)

        // ── Couleurs HTML mesurees ───────────────────────────────────────────
        static math::NkColor ColP1()  { return {   0, 245, 255, 255 }; } // cyan
        static math::NkColor ColP2()  { return { 255, 107,   0, 255 }; } // orange
        static math::NkColor ColField(){return {   0, 245, 255,  30 }; } // border arene
        static math::NkColor ColDash() { return { 255, 255, 255,  20 }; } // ligne pointillee
        static math::NkColor ColRing() { return { 255, 255, 255,  18 }; } // cercle central

        // ── Random helper (range [-1, 1]) ────────────────────────────────────
        static float Rand11()
        {
            return ((float)std::rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        }

        // ─────────────────────────────────────────────────────────────────────
        // Lifecycle
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnEnter(AppContext& ctx)
        {
            mScoreL = 0;
            mScoreR = 0;
            mPaused = false;
            mGoalCooldown   = 0.0f;
            mGoalFlashAlpha = 0.0f;
            mGoalFlashSide  = 0;
            // Timer initial : depuis GameSettings (0 = chrono ascendant).
            mTimeLeft = (ctx.settings != nullptr && ctx.settings->timeLimit > 0.0f)
                        ? ctx.settings->timeLimit
                        : 0.0f;
            mTimeUp   = 0.0f;
            mGameOver = false;
            mWinner   = 0;
            mTrailHead = 0;
            mTrailCount = 0;
            mKeyW = mKeyS = mKeyUp = mKeyDown = false;

            // Scale calcule depuis le viewport courant (cf. UIScale.h).
            // Applique a toutes les dimensions et vitesses HTML brutes.
            mScale     = GetUIScale(ctx.viewportW, ctx.viewportH);
            mHUDTopH = math::NkMin(kHUDTopHBase * mScale,
                                   (float)ctx.viewportH * 0.12f);
            mHUDBotH = math::NkMin(kHUDBotHBase * mScale,
                                   (float)ctx.viewportH * 0.12f);
            mPaddleW   = kPaddleWBase  * mScale;
            mPaddleH   = kPaddleHBase  * mScale;
            mBallR     = kBallRBase    * mScale;
            mPaddleSpd = kPaddleSpdBase* mScale;

            // ── Detection mode reseau (Phase 2) ─────────────────────────────
            // Si une NetworkSession est connectee, on bascule en mode reseau :
            //   - HOST   : simulation autoritative (paddle L local, R via input
            //              client recu, ball/goals/scores calcules ici).
            //   - CLIENT : aucune simulation. Envoie son input local au host,
            //              applique tels quels les snapshots PktState recus.
            mIsNetwork = (ctx.network != nullptr
                       && ctx.network->State() == NetworkState::Connected);
            mIsHost    = mIsNetwork && (ctx.network->Role() == NetworkRole::Host);
            mNetSendStateTimer = 0.0f;
            mNetSendInputTimer = 0.0f;
            mNetRemotePaddleYN = 0.5f;
            mNetLocalPaddleYN  = 0.5f;
            mNetHasRemoteInput = false;
            mNetPeerLost       = false;
            mNetLostTimer      = 0.0f;

            // ── Configuration IA selon GameSettings.mode ────────────────────
            //   Local      : aucun paddle IA
            //   VsAI       : paddle DROITE pilote IA (gauche = joueur)
            //   AIvsAI     : 2 paddles pilotes IA
            //   NetworkLAN : pas d'IA, les 2 raquettes sont humaines
            mAILeftEnabled  = false;
            mAIRightEnabled = false;
            if (ctx.settings != nullptr && !mIsNetwork)
            {
                const GameMode m = ctx.settings->mode;
                if (m == GameMode::VsAI)
                {
                    mAIRightEnabled = true;
                    mAIRight.SetDifficulty(ctx.settings->difficulty);
                }
                else if (m == GameMode::AIvsAI)
                {
                    mAILeftEnabled  = true;
                    mAIRightEnabled = true;
                    // Difficulte par IA : gauche = settings->difficulty,
                    // droite = settings->difficultyP2. Permet de faire
                    // s'affronter 2 niveaux differents (ex: Beginner vs Legend)
                    // ou le meme niveau si l'user le choisit.
                    mAILeft .SetDifficulty(ctx.settings->difficulty);
                    mAIRight.SetDifficulty(ctx.settings->difficultyP2);
                }
            }
            if (mIsNetwork)
            {
                logger.Info("[Gameplay] Mode reseau actif role={0}",
                            mIsHost ? "HOST" : "CLIENT");
            }

            const float arenaW = (float)ctx.viewportW;
            const float arenaH = math::NkMax(1.0f, (float)ctx.viewportH - mHUDTopH - mHUDBotH);
            ResetPositions(arenaW, arenaH);

            // Spawn des obstacles selon settings.obsActive[8].
            // Phase 4 reseau : la seed deterministe (ctx.settings->obstacleSeed)
            // est utilisee cote HOST ET CLIENT pour spawn les MEMES obstacles
            // aux memes positions (cf. ObstacleSystem::SpawnFromSettings).
            // En mode local : obstacleSeed == 0, RNG natif (random_device).
            if (ctx.settings != nullptr)
            {
                mObstacles.SpawnFromSettings(*ctx.settings, arenaW, arenaH, mScale);
                logger.Info("[Gameplay] Obstacles spawn: {0} (seed={1})",
                            mObstacles.Count(), ctx.settings->obstacleSeed);
            }

            // Vitesse initiale = HTML * scale * multiplicateur joueur. La balle
            // traverse le terrain dans le meme temps qu'a 1280x720 (a mul=1),
            // peu importe le viewport. Le multiplicateur (>= 1) vient du menu
            // pre-match et permet d'accelerer globalement le jeu.
            const float mul = (ctx.settings != nullptr && ctx.settings->ballSpeedMul >= 1.0f)
                              ? ctx.settings->ballSpeedMul : 1.0f;
            mBallSpeedMul = mul;
            mBallVX = kBallVxInit * mScale * mul;
            mBallVY = kBallVyInit * mScale * mul;

            // Power-ups : reset l'etat (effets + drops) au demarrage du match.
            mPowerUps.Reset();
            mLastToucher = 0;
            // Particules : vide le pool au demarrage.
            mParticles.Reset();

            logger.Info("[Gameplay] OnEnter arena {0}x{1} scale={2}",
                        (int)arenaW, (int)arenaH, mScale);
        }

        void GameplayScene::RequestPauseToggle(AppContext& ctx)
        {
            // Mode local : toggle direct.
            if (!mIsNetwork)
            {
                mPaused = !mPaused;
                return;
            }
            // Mode reseau HOST : toggle local. Le snapshot suivant propagera
            // l'etat au client via PktState.flags & kFlagPaused.
            if (mIsHost)
            {
                mPaused = !mPaused;
                return;
            }
            // Mode reseau CLIENT : envoyer kMsgPauseToggle au host. Le HOST
            // togglera son mPaused et le snapshot suivant nous le renverra.
            // On ne touche PAS a notre mPaused local -- il sera mis a jour
            // au prochain snapshot.
            if (ctx.network != nullptr)
            {
                netproto::PktPauseToggle pkt;
                pkt.type = netproto::kMsgPauseToggle;
                // Reliable : message ponctuel, on veut etre sur qu'il arrive.
                ctx.network->Broadcast(reinterpret_cast<const uint8*>(&pkt),
                                       sizeof(pkt), 1 /*reliable*/);
                logger.Info("[Net][TX] CLIENT pause toggle request");
            }
        }

        void GameplayScene::RequestReplay(AppContext& ctx)
        {
            // Mode local OU HOST : on relance directement.
            if (!mIsNetwork || mIsHost)
            {
                StartNewMatch(ctx);
                return;
            }
            // Mode reseau CLIENT : envoyer kMsgReplayRequest au HOST. Le
            // HOST relancera (StartNewMatch) et le snapshot propagera
            // mGameOver=false + scores=0 au CLIENT. On NE touche PAS notre
            // mGameOver local -- il sera mis a jour par le prochain snapshot.
            if (ctx.network != nullptr)
            {
                netproto::PktReplayRequest pkt;
                pkt.type = netproto::kMsgReplayRequest;
                ctx.network->Broadcast(reinterpret_cast<const uint8*>(&pkt),
                                       sizeof(pkt), 1 /*reliable*/);
                logger.Info("[Net][TX] CLIENT replay request");
            }
        }

        void GameplayScene::OnPause(AppContext& /*ctx*/)
        {
            // En mode reseau : NE PAS auto-pauser sur perte de focus. Sinon
            // OnUpdate retourne tot et les messages reseau (snapshots du host,
            // inputs du client) ne sont plus drainees -> la session est gelee.
            // Cas typique : 2 instances sur le meme PC, alt-tab entre les 2
            // declenche FocusLost sur celle qu'on quitte et casse la partie.
            // On libere quand meme les touches tenues pour eviter qu'au retour
            // de focus, un paddle continue de bouger tout seul.
            if (mIsNetwork)
            {
                mKeyW = mKeyS = mKeyUp = mKeyDown = false;
                mMouseDownL = mMouseDownR = false;
                mTouchIdL = mTouchIdR = -1;
                return;
            }

            // Auto-pause systeme (mode local) : Hidden / FocusLost / background.
            // L'overlay pause s'affichera + le user verra "REPRENDRE" au
            // retour de l'app (NkWindowShownEvent -> OnResume, mais on laisse
            // la pause active pour qu'il valide manuellement la reprise).
            if (!mPaused)
            {
                mPaused = true;
                logger.Info("[Gameplay] Auto-pause (lifecycle)");
            }
            // Libere les inputs tenus pour eviter qu'au resume le paddle
            // continue de bouger tout seul.
            mKeyW = mKeyS = mKeyUp = mKeyDown = false;
            mMouseDownL = mMouseDownR = false;
            mTouchIdL = mTouchIdR = -1;
        }

        void GameplayScene::ResetPositions(float arenaW, float arenaH)
        {
            const float cx = arenaW * 0.5f;
            const float cy = arenaH * 0.5f;
            mBallX = cx;
            mBallY = cy;
            mTrailHead = 0;
            mTrailCount = 0;
            const float margin = kPaddleXMargin * mScale;
            mPaddleLX = margin;
            mPaddleLY = cy - mPaddleH * 0.5f;
            mPaddleRX = arenaW - margin - mPaddleW;
            mPaddleRY = cy - mPaddleH * 0.5f;
        }

        void GameplayScene::OnUpdate(AppContext& ctx, float dt)
        {
            // Note : on NE retourne PAS tot sur mPaused en mode reseau, pour
            // que la couche reseau (drain des messages, envoi de state) continue
            // a fonctionner pendant la pause. Sinon le client coince en pause
            // ne recevrait jamais le snapshot indiquant que l'host a depause.
            // Le check pause est fait APRES la section reseau, avant la simu.
            if (mPaused && !mIsNetwork) return;
            // Re-sync scale chaque frame au cas ou le viewport ait change
            // (resize desktop). Sur Android landscape locke c'est constant.
            const float newScale = GetUIScale(ctx.viewportW, ctx.viewportH);
            if (math::NkFabs(newScale - mScale) > 0.001f)
            {
                mScale     = newScale;
                mHUDTopH = math::NkMin(kHUDTopHBase * mScale,
                                       (float)ctx.viewportH * 0.12f);
                mHUDBotH = math::NkMin(kHUDBotHBase * mScale,
                                       (float)ctx.viewportH * 0.12f);
                mPaddleW   = kPaddleWBase  * mScale;
                mPaddleH   = kPaddleHBase  * mScale;
                mBallR     = kBallRBase    * mScale;
                mPaddleSpd = kPaddleSpdBase* mScale;
            }
            const float arenaW = (float)ctx.viewportW;
            const float arenaH = math::NkMax(1.0f, (float)ctx.viewportH - mHUDTopH - mHUDBotH);

            // ── Detection de perte du pair en mode reseau ───────────────────
            // Si l'autre joueur ferme son app, perd le wifi, ou pop sa scene,
            // sa NetworkSession passe en Disconnected (via callback ou timeout).
            // On le detecte ici et on affiche un overlay "ADVERSAIRE PARTI"
            // avec un bouton RETOUR MENU. La simulation HOST est gelee (plus
            // d'inputs CLIENT) et l'envoi state continue mais le CLIENT n'est
            // plus la pour le recevoir.
            if (mIsNetwork && ctx.network != nullptr && !mNetPeerLost)
            {
                const NetworkState netSt = ctx.network->State();
                if (netSt != NetworkState::Connected)
                {
                    mNetPeerLost  = true;
                    mNetLostTimer = 0.0f;
                    logger.Info("[Gameplay] Adversaire parti (state={0})",
                                (int)netSt);
                }
            }
            if (mNetPeerLost)
            {
                mNetLostTimer += dt;
            }

            // ── Reseau (Phase 2) : drain des messages + apply ────────────────
            // Le host recoit les PktInput du client et stocke mNetRemoteDir.
            // Le client recoit les PktState du host et applique tout tel quel.
            // L'envoi est fait plus bas dans la frame (timers a 30 Hz).
            if (mIsNetwork && ctx.network != nullptr)
            {
                NkVector<net::NkReceiveMsg> incoming;
                ctx.network->DrainReceived(incoming);
                // DIAG : compteur pour confirmer reception. Limite log a 5
                // premiers messages pour ne pas spammer.
                static int sRxDiagCount = 0;
                for (const auto& msg : incoming)
                {
                    if (msg.size < 1) continue;
                    const uint8 type = msg.data[0];
                    if (sRxDiagCount < 5)
                    {
                        logger.Info("[Net][RX] role={0} type={1} size={2}",
                                    mIsHost ? "HOST" : "CLIENT",
                                    (int)type, (int)msg.size);
                        sRxDiagCount++;
                    }

                    if (mIsHost && type == netproto::kMsgInput
                     && msg.size >= sizeof(netproto::PktInput))
                    {
                        netproto::PktInput in;
                        std::memcpy(&in, msg.data, sizeof(in));
                        // Clamp safety (le client envoie [0..1] en theorie)
                        float yN = in.paddleYN;
                        if (yN < 0.0f) yN = 0.0f;
                        if (yN > 1.0f) yN = 1.0f;
                        mNetRemotePaddleYN = yN;
                        mNetHasRemoteInput = true;
                    }
                    else if (mIsHost && type == netproto::kMsgPauseToggle
                          && msg.size >= sizeof(netproto::PktPauseToggle))
                    {
                        // Le client demande de pauser/depauser. Le HOST est
                        // autoritatif : on toggle ici et la nouvelle valeur sera
                        // propagee au client via PktState.flags & kFlagPaused
                        // au prochain envoi state.
                        mPaused = !mPaused;
                        logger.Info("[Net][RX] HOST pause toggle -> {0}",
                                    mPaused ? "paused" : "resumed");
                    }
                    else if (mIsHost && type == netproto::kMsgReplayRequest
                          && msg.size >= sizeof(netproto::PktReplayRequest))
                    {
                        // Le client demande de rejouer. On accepte uniquement
                        // si on est en game over (sinon ignore : la demande
                        // n'a pas de sens en cours de match). Au prochain
                        // snapshot, mGameOver=false sera propage au CLIENT
                        // qui sortira de son ecran game over en meme temps.
                        if (mGameOver)
                        {
                            logger.Info("[Net][RX] HOST replay request -> StartNewMatch");
                            StartNewMatch(ctx);
                        }
                    }
                    else if (!mIsHost && type == netproto::kMsgState
                          && msg.size >= sizeof(netproto::PktState))
                    {
                        netproto::PktState st;
                        std::memcpy(&st, msg.data, sizeof(st));
                        // Denormalisation : les coordonnees sont [0..1]
                        // relatives a l'arene HOST -> on multiplie par notre
                        // propre arene CLIENT pour rester visuellement coherent
                        // meme si les viewports different.
                        const float aW = (float)ctx.viewportW;
                        const float aH = math::NkMax(1.0f,
                            (float)ctx.viewportH - mHUDTopH - mHUDBotH);
                        mPaddleLY = st.paddleLYN * aH;
                        // mPaddleRY : on NE l'ECRASE PAS depuis le snapshot.
                        // Le CLIENT garde sa position locale immediate (souris/
                        // touch/clavier). Le snapshot HOST a la meme position
                        // car HOST applique mNetRemotePaddleYN tel quel. Sans
                        // ce skip, le paddle saccadait entre la position locale
                        // (avancee de la souris) et la position recue (en retard
                        // de ~50ms) -> impression "bouge a peine".
                        // mPaddleRY = st.paddleRYN * aH;  // SKIP
                        mBallX    = st.ballXN    * aW;
                        mBallY    = st.ballYN    * aH;
                        mBallVX   = st.ballVXN   * aW;
                        mBallVY   = st.ballVYN   * aH;
                        mScoreL   = (int)st.scoreL;
                        mScoreR   = (int)st.scoreR;
                        const bool wasGameOver = mGameOver;
                        mGameOver = (st.flags & netproto::kFlagGameOver) != 0;
                        mPaused   = (st.flags & netproto::kFlagPaused)   != 0;
                        mWinner   = (int)st.winner;
                        // Detection transition gameOver true -> false : le HOST
                        // a relance une nouvelle manche. On respawn nos obstacles
                        // localement (en gardant la meme seed, donc les memes
                        // positions) + reset notre etat cosmetique.
                        if (wasGameOver && !mGameOver)
                        {
                            logger.Info("[Net][RX] CLIENT detection replay -> StartNewMatch local");
                            StartNewMatch(ctx);
                        }
                        // ── Sync power-ups (Phase 5) ─────────────────────────
                        // Multiplicateurs taille paddle : dequantification 64q = 1.0
                        mNetPaddleHMulL = (float)st.paddleHMulL_q / 64.0f;
                        mNetPaddleHMulR = (float)st.paddleHMulR_q / 64.0f;
                        mNetEffFlagsL   = st.effFlagsL;
                        mNetEffFlagsR   = st.effFlagsR;
                        // Drops : reconstruit la liste interne de mPowerUps
                        // depuis le snapshot HOST. Le CLIENT ne simule pas
                        // les collisions paddle/balle-drop (le HOST decide).
                        PowerUpDrop tmpDrops[netproto::kMaxDropsSync];
                        const int n = math::NkMin((int)st.numDrops,
                                          (int)netproto::kMaxDropsSync);
                        for (int k = 0; k < n; ++k)
                        {
                            const netproto::PktDropEntry& e = st.drops[k];
                            PowerUpDrop& d = tmpDrops[k];
                            d.isBonus = (e.isBonusKind & 0x80) != 0;
                            d.kind    = (uint8)(e.isBonusKind & 0x7F);
                            d.x       = e.xN * aW;
                            d.y       = e.yN * aH;
                            d.r       = 16.0f;
                            d.vx      = 0.0f;
                            d.vy      = 0.0f;
                            d.alive   = true;
                            d.pulse   = 0.0f;
                        }
                        mPowerUps.SetNetDrops(tmpDrops, n);
                        // Le flash de but cote client : on declenche localement
                        // si le snapshot dit qu'un but vient d'arriver.
                        if ((st.flags & netproto::kFlagGoalFlash) != 0
                         && mGoalFlashAlpha <= 0.0f)
                        {
                            mGoalFlashAlpha = 1.0f;
                        }
                    }
                }
            }
            // Le paddle droit doit toujours coller au bord droit du viewport
            // meme si la fenetre est resized en cours de partie. Le paddle
            // gauche reste a sa marge. On clamp aussi Y pour ne pas sortir
            // si arenaH a diminue.
            const float marginX = kPaddleXMargin * mScale;
            mPaddleLX = marginX;
            mPaddleRX = arenaW - marginX - mPaddleW;
            if (mPaddleLY > arenaH - mPaddleH) mPaddleLY = arenaH - mPaddleH;
            if (mPaddleRY > arenaH - mPaddleH) mPaddleRY = arenaH - mPaddleH;
            if (mPaddleLY < 0.0f) mPaddleLY = 0.0f;
            if (mPaddleRY < 0.0f) mPaddleRY = 0.0f;

            // Conversion px/frame@60fps -> distance reelle ce frame.
            const float dt60 = dt * 60.0f;

            // Goal cooldown : on attend avant que la balle reprenne ses
            // collisions normales pour eviter les re-buts en boucle.
            if (mGoalCooldown > 0.0f)
            {
                mGoalCooldown -= dt;
                if (mGoalCooldown < 0.0f) mGoalCooldown = 0.0f;
            }
            // Flash de but : decroit en ~350 ms.
            if (mGoalFlashAlpha > 0.0f)
            {
                mGoalFlashAlpha -= dt / 0.35f;
                if (mGoalFlashAlpha < 0.0f) mGoalFlashAlpha = 0.0f;
            }

            // Timer : si timeLimit > 0 -> descendant, sinon ascendant (chrono)
            if (ctx.settings != nullptr && ctx.settings->timeLimit > 0.0f)
            {
                if (mTimeLeft > 0.0f) mTimeLeft -= dt;
                if (mTimeLeft < 0.0f) mTimeLeft = 0.0f;
                // Time-based victory : a temps 0, le joueur en tete gagne
                if (!mGameOver && mTimeLeft <= 0.0f && mGoalCooldown <= 0.0f)
                {
                    CheckWinConditions(*ctx.settings);
                }
            }
            else
            {
                mTimeUp += dt;
            }

            // ── Reseau CLIENT : prediction balle + trail + obstacles + particules ─
            // Phase 3 : entre 2 snapshots HOST (60 Hz = 16 ms), on extrapole
            // la position de la balle avec sa velocite. Phase 4 : on update
            // aussi les obstacles localement (anims pulse/rotate/blink/warp)
            // car ils ont ete spawnes avec la meme seed que le HOST.
            if (mIsNetwork && !mIsHost && !mPaused && !mGameOver)
            {
                // Prediction : avance la balle avec sa velocite actuelle.
                mBallX += mBallVX * dt60;
                mBallY += mBallVY * dt60;

                // Trail balle : push la position actuelle (predite) chaque frame.
                mTrailX[mTrailHead] = mBallX;
                mTrailY[mTrailHead] = mBallY;
                mTrailHead = (mTrailHead + 1) % kTrailMax;
                if (mTrailCount < kTrailMax) mTrailCount++;
                // Anims obstacles + particules : tournent localement.
                mObstacles.Update(dt, arenaW, arenaH);
                mParticles.Update(dt);
            }

            // ── Reseau : calcul + envoi (TOUJOURS, meme en pause/gameOver) ───
            // On fait l'envoi reseau ICI, AVANT les returns sur mGameOver et
            // mPaused, pour que la communication ne se gele jamais. Sinon le
            // CLIENT ne saurait pas que le HOST a pause/depause ou gameover.
            if (mIsNetwork && !mIsHost)
            {
                // Calcul de mNetLocalPaddleYN cote CLIENT a partir de tous les
                // inputs locaux (souris/touch ont deja modifie mPaddleRY dans
                // OnEvent, clavier on l'applique ici).
                if (arenaH > 0.0f) mNetLocalPaddleYN = mPaddleRY / arenaH;
                const float spdN = (mPaddleSpd * dt60) / math::NkMax(1.0f, arenaH);
                if (mKeyW || mKeyUp)   mNetLocalPaddleYN -= spdN;
                if (mKeyS || mKeyDown) mNetLocalPaddleYN += spdN;
                const float pHN = mPaddleH / math::NkMax(1.0f, arenaH);
                if (mNetLocalPaddleYN < 0.0f)       mNetLocalPaddleYN = 0.0f;
                if (mNetLocalPaddleYN > 1.0f - pHN) mNetLocalPaddleYN = 1.0f - pHN;
                // Reflet local immediat (sera ecrase par le prochain snapshot,
                // mais procure la reactivite visible immediate au user).
                mPaddleRY = mNetLocalPaddleYN * arenaH;

                // Envoi a kInputSendHz
                mNetSendInputTimer += dt;
                const float kInputDt = 1.0f / netproto::kInputSendHz;
                if (mNetSendInputTimer >= kInputDt && ctx.network != nullptr)
                {
                    mNetSendInputTimer = 0.0f;
                    netproto::PktInput in;
                    in.type     = netproto::kMsgInput;
                    in.paddleYN = mNetLocalPaddleYN;
                    const bool ok = ctx.network->Broadcast(
                        reinterpret_cast<const uint8*>(&in),
                        sizeof(in), 0 /*unreliable*/);
                    static float sLastTxYN = -1.0f;
                    if (math::NkFabs(sLastTxYN - in.paddleYN) > 0.02f)
                    {
                        logger.Info("[Net][TX] CLIENT input paddleYN={0} ok={1}",
                                    in.paddleYN, ok ? "true" : "false");
                        sLastTxYN = in.paddleYN;
                    }
                }
                return;  // CLIENT : aucune simulation, l'etat vient du HOST.
            }
            if (mIsNetwork && mIsHost && ctx.network != nullptr)
            {
                // Envoi state a kStateSendHz, meme en pause ou gameover
                mNetSendStateTimer += dt;
                const float kStateDt = 1.0f / netproto::kStateSendHz;
                if (mNetSendStateTimer >= kStateDt)
                {
                    mNetSendStateTimer = 0.0f;
                    netproto::PktState st;
                    st.type     = netproto::kMsgState;
                    const float invW = (arenaW > 0.0f) ? (1.0f / arenaW) : 0.0f;
                    const float invH = (arenaH > 0.0f) ? (1.0f / arenaH) : 0.0f;
                    st.paddleLYN = mPaddleLY * invH;
                    st.paddleRYN = mPaddleRY * invH;
                    st.ballXN    = mBallX    * invW;
                    st.ballYN    = mBallY    * invH;
                    st.ballVXN   = mBallVX   * invW;
                    st.ballVYN   = mBallVY   * invH;
                    st.scoreL    = (uint16)math::NkMax(0, mScoreL);
                    st.scoreR    = (uint16)math::NkMax(0, mScoreR);
                    uint8 flags = 0;
                    if (mPaused)                flags |= netproto::kFlagPaused;
                    if (mGameOver)              flags |= netproto::kFlagGameOver;
                    if (mGoalFlashAlpha > 0.5f) flags |= netproto::kFlagGoalFlash;
                    st.flags  = flags;
                    st.winner = (int8)mWinner;
                    // ── Sync power-ups (Phase 5) =================================
                    // Quantification paddleHMul : 64 q = 1.0 (range 0..4 sur 8 bits)
                    auto quantHMul = [](float v) -> uint8
                    {
                        const float q = v * 64.0f;
                        if (q < 0.0f)   return 0;
                        if (q > 255.0f) return 255;
                        return (uint8)q;
                    };
                    st.paddleHMulL_q = quantHMul(mPowerUps.GetPaddleHeightMul(-1));
                    st.paddleHMulR_q = quantHMul(mPowerUps.GetPaddleHeightMul(+1));
                    // Flags effets visuels par cote.
                    uint8 effL = 0, effR = 0;
                    if (mPowerUps.IsFrozen(-1))           effL |= netproto::kEffFrozen;
                    if (mPowerUps.IsBlind(-1))            effL |= netproto::kEffBlind;
                    if (mPowerUps.HasInvertedControls(-1)) effL |= netproto::kEffInverted;
                    if (mPowerUps.IsFrozen(+1))           effR |= netproto::kEffFrozen;
                    if (mPowerUps.IsBlind(+1))            effR |= netproto::kEffBlind;
                    if (mPowerUps.HasInvertedControls(+1)) effR |= netproto::kEffInverted;
                    st.effFlagsL = effL;
                    st.effFlagsR = effR;
                    // Drops (jusqu'a kMaxDropsSync = 4)
                    const int nDrops = math::NkMin(
                        (int)netproto::kMaxDropsSync, mPowerUps.DropCount());
                    st.numDrops = (uint8)nDrops;
                    for (int k = 0; k < nDrops; ++k)
                    {
                        const PowerUpDrop& d = mPowerUps.GetDrop(k);
                        netproto::PktDropEntry& e = st.drops[k];
                        e.isBonusKind = (uint8)((d.isBonus ? 0x80 : 0x00)
                                              | (d.kind & 0x7F));
                        e.xN = d.x * invW;
                        e.yN = d.y * invH;
                    }
                    for (int k = nDrops; k < (int)netproto::kMaxDropsSync; ++k)
                    {
                        st.drops[k].isBonusKind = 0;
                        st.drops[k].xN = 0.0f;
                        st.drops[k].yN = 0.0f;
                    }
                    ctx.network->Broadcast(reinterpret_cast<const uint8*>(&st),
                                           sizeof(st), 0 /*unreliable*/);
                }
            }

            // Si pause (en mode reseau le HOST peut pauser apres l'envoi) ou
            // game over : on n'avance plus la simulation. Le user doit
            // cliquer REJOUER ou MENU. On laisse juste les flash/cooldown finir.
            if (mPaused)   return;
            if (mGameOver) return;

            // Avance les anims des obstacles (pulse, rotation, blink, warp).
            // arenaW/H requis pour le warp (repositionnement aleatoire).
            mObstacles.Update(dt, arenaW, arenaH);

            // Avance les power-ups (effets timer, drops qui tombent).
            // Phase 5 reseau : le HOST simule (autoritatif) et envoie l'etat
            // visuel via PktState (drops + paddleHMul + effFlags). Le CLIENT
            // skip cet Update (sa simu est ecrasee chaque snapshot). Mode
            // local : Update normal.
            if (!mIsNetwork || mIsHost)
            {
                mPowerUps.Update(dt, arenaW, arenaH, mScale);
            }
            // Avance les particules (fade + position).
            mParticles.Update(dt);

            // TeleportPaddle : si l'effet est en attente sur un cote, snap la
            // raquette a la fraction Y demandee (1 seul tick, le flag se
            // consomme tout seul dans TryTeleport).
            float fracY = 0.5f;
            const float pHL = mPaddleH * EffectivePaddleHMul(-1);
            const float pHR = mPaddleH * EffectivePaddleHMul(+1);
            if (mPowerUps.TryTeleport(-1, fracY)) mPaddleLY = (arenaH - pHL) * fracY;
            if (mPowerUps.TryTeleport(+1, fracY)) mPaddleRY = (arenaH - pHR) * fracY;

            // Drops vs raquettes : on teste si un orbe touche la raquette
            // gauche ou droite. Si oui : on emet un burst au point d'impact
            // avec la couleur de l'effet.
            float catchX = 0.0f, catchY = 0.0f;
            math::NkColor catchCol;
            if (mPowerUps.CheckPaddleCollision(-1, mPaddleLX, mPaddleLY,
                                               mPaddleW, pHL,
                                               &catchX, &catchY, &catchCol))
            {
                mParticles.EmitBurst(catchX, catchY, 18, catchCol,
                                     60.0f, 220.0f, 0.4f, 0.8f, 2.0f, 4.0f);
            }
            if (mPowerUps.CheckPaddleCollision(+1, mPaddleRX, mPaddleRY,
                                               mPaddleW, pHR,
                                               &catchX, &catchY, &catchCol))
            {
                mParticles.EmitBurst(catchX, catchY, 18, catchCol,
                                     60.0f, 220.0f, 0.4f, 0.8f, 2.0f, 4.0f);
            }

            // HOST (ou mode local) : simulation normale.
            StepPaddles(arenaH, dt60);
            StepBall   (ctx, arenaW, arenaH, dt60);
        }

        // ─────────────────────────────────────────────────────────────────────
        // StepPaddles — W/S et fleches, vitesse 4 px/frame a 60fps. Clamp aux
        // bords de l'arene (mg=0 dans le HTML).
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::StepPaddles(float arenaH, float dt60)
        {
            // Multiplicateurs power-up (taille raquette + vitesse, par cote)
            const float pHL = mPaddleH * EffectivePaddleHMul(-1);
            const float pHR = mPaddleH * EffectivePaddleHMul(+1);
            const float spdL = mPaddleSpd * mPowerUps.GetPaddleSpeedMul(-1) * dt60;
            const float spdR = mPaddleSpd * mPowerUps.GetPaddleSpeedMul(+1) * dt60;
            const bool frozenL = mPowerUps.IsFrozen(-1);
            const bool frozenR = mPowerUps.IsFrozen(+1);
            const bool invL    = mPowerUps.HasInvertedControls(-1);
            const bool invR    = mPowerUps.HasInvertedControls(+1);

            // Paddle gauche : IA si configuree, sinon input humain
            if (frozenL)
            {
                // Raquette gelee : aucun deplacement (ni IA ni input).
            }
            else if (mAILeftEnabled)
            {
                const float dt = dt60 / 60.0f;
                mPaddleLY += mAILeft.Update(dt, mBallX, mBallY, mBallVX, mBallVY,
                                            mPaddleLX, mPaddleLY, pHL,
                                            (float)0.0f, arenaH,
                                            mPaddleSpd * mPowerUps.GetPaddleSpeedMul(-1));
            }
            else
            {
                // Inversion controles : si malus actif, W -> bas, S -> haut.
                const bool wantUp   = invL ? mKeyS : mKeyW;
                const bool wantDown = invL ? mKeyW : mKeyS;
                if (wantUp)   mPaddleLY -= spdL;
                if (wantDown) mPaddleLY += spdL;
            }

            // Paddle droite : idem
            if (frozenR)
            {
                // Idem cote droit.
            }
            else if (mAIRightEnabled)
            {
                const float dt = dt60 / 60.0f;
                mPaddleRY += mAIRight.Update(dt, mBallX, mBallY, mBallVX, mBallVY,
                                             mPaddleRX, mPaddleRY, pHR,
                                             (float)0.0f, arenaH,
                                             mPaddleSpd * mPowerUps.GetPaddleSpeedMul(+1));
            }
            else if (mIsNetwork && mIsHost)
            {
                // En reseau, le paddle droit suit DIRECTEMENT la position
                // envoyee par le client (mNetRemotePaddleYN), sans cap de
                // vitesse. Sinon le HOST traine d'1-2 frames derriere ce que
                // le user voit cote CLIENT (snapshot ramene la position
                // intermediaire -> impression "bouge a peine"). Phase 3
                // ajoutera anti-cheat (validation de la delta entre 2 inputs).
                if (mNetHasRemoteInput)
                {
                    mPaddleRY = mNetRemotePaddleYN * arenaH;
                }
            }
            else
            {
                const bool wantUp   = invR ? mKeyDown : mKeyUp;
                const bool wantDown = invR ? mKeyUp   : mKeyDown;
                if (wantUp)   mPaddleRY -= spdR;
                if (wantDown) mPaddleRY += spdR;
            }

            // Clamp en utilisant les hauteurs effectives (peuvent etre x2 ou /2).
            if (mPaddleLY < 0.0f)         mPaddleLY = 0.0f;
            if (mPaddleLY > arenaH - pHL) mPaddleLY = arenaH - pHL;
            if (mPaddleRY < 0.0f)         mPaddleRY = 0.0f;
            if (mPaddleRY > arenaH - pHR) mPaddleRY = arenaH - pHR;
        }

        // ─────────────────────────────────────────────────────────────────────
        // StepBall — meme logique que moveBall() du HTML, en dt60.
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::StepBall(AppContext& ctx, float arenaW, float arenaH, float dt60)
        {
            // Trail : push la position actuelle AVANT de bouger (comme le HTML).
            mTrailX[mTrailHead] = mBallX;
            mTrailY[mTrailHead] = mBallY;
            mTrailHead = (mTrailHead + 1) % kTrailMax;
            if (mTrailCount < kTrailMax) mTrailCount++;

            // Vitesse balle modulee par les power-ups (SlowBall x0.5, FastBall x3).
            const float bMul = mPowerUps.GetBallSpeedMul();
            mBallX += mBallVX * dt60 * bMul;
            mBallY += mBallVY * dt60 * bMul;

            // Rebond haut / bas
            if (mBallY - mBallR < 0.0f)
            {
                mBallY = mBallR;
                mBallVY = math::NkFabs(mBallVY);
            }
            if (mBallY + mBallR > arenaH)
            {
                mBallY = arenaH - mBallR;
                mBallVY = -math::NkFabs(mBallVY);
            }

            // Sortie laterale = but (avec cooldown)
            if (mGoalCooldown <= 0.0f)
            {
                // Convention TriggerGoal : side = camp DU JOUEUR QUI MARQUE.
                // Ball sortie a GAUCHE = passe la raquette de P1 = P2 (droite, -1) marque.
                // Ball sortie a DROITE = passe la raquette de P2 = P1 (gauche, +1) marque.
                if (mBallX - mBallR < 0.0f)        { TriggerGoal(ctx, -1, arenaW, arenaH); return; }
                if (mBallX + mBallR > arenaW)      { TriggerGoal(ctx, +1, arenaW, arenaH); return; }
            }

            // Collision paddles (left puis right). Hauteurs effectives prennent
            // en compte GiantPaddle (x2) ou MiniPaddle (x0.5).
            const float pHL_c = mPaddleH * EffectivePaddleHMul(-1);
            const float pHR_c = mPaddleH * EffectivePaddleHMul(+1);
            for (int i = 0; i < 2; ++i)
            {
                const float px = (i == 0) ? mPaddleLX : mPaddleRX;
                const float py = (i == 0) ? mPaddleLY : mPaddleRY;
                const float pw = mPaddleW;
                const float ph = (i == 0) ? pHL_c     : pHR_c;
                if (mBallX + mBallR > px && mBallX - mBallR < px + pw
                 && mBallY + mBallR > py && mBallY - mBallR < py + ph)
                {
                    // Memorise le dernier joueur a avoir touche la balle.
                    // Sert a attribuer le bonus quand une BonusStar est collectee.
                    mLastToucher = (i == 0) ? -1 : +1;
                    // Direction selon position sur le terrain (HTML : b.x<W/2 ? 1 : -1)
                    const float dir = (mBallX < arenaW * 0.5f) ? 1.0f : -1.0f;
                    mBallVX = math::NkFabs(mBallVX) * dir;
                    const float rel = (mBallY - (py + ph * 0.5f)) / (ph * 0.5f);
                    mBallVY = rel * kBounceVyBase * mScale;
                    // Acceleration speed = min(sqrt(...) * 1.04, cap).
                    // Le cap est scale par mBallSpeedMul pour que les modes
                    // x5/x10 restent rapides apres les rebonds (sinon le cap
                    // ramenerait la balle a la vitesse de base).
                    float spd2 = mBallVX * mBallVX + mBallVY * mBallVY;
                    float spd  = math::NkSqrt(spd2) * kBounceMul;
                    const float speedCap = kSpeedCapBase * mScale * mBallSpeedMul;
                    if (spd > speedCap) spd = speedCap;
                    const float ang = math::NkAtan2(mBallVY, mBallVX);
                    mBallVX = math::NkCos(ang) * spd;
                    mBallVY = math::NkSin(ang) * spd;
                    // Burst de particules au point de contact, oriente dans
                    // la direction de rebond. Couleur = celle du joueur qui
                    // a renvoye la balle.
                    const math::NkColor col = (i == 0) ? ColP1() : ColP2();
                    mParticles.EmitDirectional(mBallX, mBallY, 14, col,
                                               ang, 0.6f,
                                               80.0f, 260.0f,
                                               0.25f, 0.6f,
                                               1.8f, 3.5f);
                    break;  // une seule collision paddle par frame
                }
            }

            // Garantit une vitesse minimum : la balle ne doit jamais ralentir
            // sous kMinSpeed * mScale * mBallSpeedMul. Le mul utilisateur est
            // pris en compte pour que les modes rapides (x5, x10) ne soient
            // pas annules par le plancher (sinon plancher ~4 alors que init = 40).
            {
                const float minSpd = kMinSpeed * mScale * mBallSpeedMul;
                const float spd = math::NkSqrt(mBallVX * mBallVX
                                             + mBallVY * mBallVY);
                if (spd < minSpd)
                {
                    if (spd > 0.001f)
                    {
                        const float k = minSpd / spd;
                        mBallVX *= k;
                        mBallVY *= k;
                    }
                    else
                    {
                        // Vitesse quasi-nulle : redonne une direction aleatoire
                        const float ang = Rand11() * 3.14159f;
                        mBallVX = math::NkCos(ang) * minSpd;
                        mBallVY = math::NkSin(ang) * minSpd;
                    }
                }
            }

            // Collision balle <-> drops power-up : la balle propulse les
            // drops en diagonale (transfert de momentum). La balle elle-meme
            // n'est pas modifiee (les drops sont passifs).
            mPowerUps.CheckBallCollision(mBallX, mBallY, mBallR, mBallVX, mBallVY);

            // Collision balle-obstacles. Applique les effets selon le type.
            const ObstacleHit oh = mObstacles.CheckCollision(
                mBallX, mBallY, mBallR, mBallVX, mBallVY, dt60);
            // BonusStar collectee : applique un bonus random au dernier joueur
            // ayant touche la balle. Cote HOST (mode local OU reseau) on
            // applique localement ; l'effet visuel (paddleHMul + flags + drops)
            // sera propage au CLIENT via PktState au prochain snapshot.
            // Cote CLIENT : skip car StepBall n'est pas appele (return early).
            if (oh.bonusStarCollected && mLastToucher != 0)
            {
                if (!mIsNetwork || mIsHost) mPowerUps.ApplyRandomBonus(mLastToucher);
                mParticles.EmitBurst(mBallX, mBallY, 24,
                                     oh.particleColor.a > 0
                                       ? oh.particleColor
                                       : math::NkColor{255, 215, 0, 255},
                                     80.0f, 280.0f, 0.5f, 1.0f, 2.0f, 4.0f);
            }
            // Burst generique pour les autres types d'obstacles (Wall, Mine,
            // Portal, Magnet...) selon le particleCount fourni par l'obstacle.
            if (oh.hit && oh.particleCount > 0)
            {
                mParticles.EmitBurst(mBallX, mBallY, oh.particleCount,
                                     oh.particleColor,
                                     60.0f, 200.0f, 0.3f, 0.7f, 1.5f, 3.0f);
            }
            // Chain reaction Mine : emit particles a chaque centre liste par
            // l'ObstacleSystem (Mines voisines qui ont detone par sympathie).
            for (int c = 0; c < oh.chainCount; ++c)
            {
                mParticles.EmitBurst(oh.chainX[c], oh.chainY[c], 18,
                                     oh.particleColor,
                                     50.0f, 220.0f, 0.4f, 0.8f, 1.8f, 3.5f);
            }
            if (oh.hit)
            {
                if (oh.reflectX) mBallVX = -mBallVX;
                if (oh.reflectY) mBallVY = -mBallVY;
                if (oh.setVel)
                {
                    mBallVX = oh.setVX;
                    mBallVY = oh.setVY;
                }
                if (oh.setPos)
                {
                    mBallX = oh.setX;
                    mBallY = oh.setY;
                }
                // Cap vitesse pour eviter que les boost s'accumulent. Le cap
                // est scale par mBallSpeedMul (cf. paddle bounce) pour respecter
                // le mode rapide choisi par l'user.
                const float spd2 = mBallVX * mBallVX + mBallVY * mBallVY;
                const float speedCap = kSpeedCapBase * 1.3f * mScale * mBallSpeedMul;
                if (spd2 > speedCap * speedCap)
                {
                    const float ang = math::NkAtan2(mBallVY, mBallVX);
                    mBallVX = math::NkCos(ang) * speedCap;
                    mBallVY = math::NkSin(ang) * speedCap;
                }
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // TriggerGoal — side : +1 = c'est P1 (gauche) qui marque (balle sortie
        // a droite), -1 = c'est P2 (droite) qui marque.
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::TriggerGoal(AppContext& ctx, int side, float arenaW, float arenaH)
        {
            // side : +1 = P1 (gauche) marque, balle sortie a droite (cote +1
            // recoit le but). -1 = P2 marque, balle sortie a gauche (cote -1
            // recoit le but). On Consume Shield sur le cote QUI ENCAISSE.
            const int scoredOnSide = -side;
            if (mPowerUps.ConsumeShield(scoredOnSide))
            {
                // Shield absorbe : le but n'est pas comptabilise. On reset
                // juste la balle au centre + petit cooldown court (pas de flash).
                mGoalCooldown = 0.4f;
                mBallX = arenaW * 0.5f;
                mBallY = arenaH * 0.5f;
                mTrailHead = 0;
                mTrailCount = 0;
                mBallVX = (side > 0 ? 1.0f : -1.0f) * kBallVxInit * mScale * mBallSpeedMul;
                mBallVY = Rand11() * 2.0f * mScale * mBallSpeedMul;
                logger.Info("[Gameplay] Shield absorb goal P{0}",
                            scoredOnSide > 0 ? 2 : 1);
                return;
            }
            mGoalCooldown   = kGoalCooldown;
            mGoalFlashAlpha = 1.0f;
            mGoalFlashSide  = side;
            // Explosion sur la ligne de but adverse (celle qu'on a franchi).
            // side > 0 (P1 marque) -> balle sortie a DROITE -> mur droit (x=arenaW).
            // side < 0 (P2 marque) -> balle sortie a GAUCHE  -> mur gauche (x=0).
            const float wallX = (side > 0) ? arenaW : 0.0f;
            const math::NkColor goalCol = (side > 0) ? ColP1() : ColP2();
            mParticles.EmitGoal(wallX, arenaH, goalCol);
            // DoublePoint : si le joueur qui marque a l'effet, le but compte
            // double. Le flag est consume.
            const int goalValue = mPowerUps.ConsumeDoublePoint(side) ? 2 : 1;
            if (side > 0) mScoreL += goalValue;
            else          mScoreR += goalValue;
            logger.Info("[Gameplay] GOAL P{0} (+{1}) ! score {2} - {3}",
                        side > 0 ? 1 : 2, goalValue, mScoreL, mScoreR);

            // Reset balle au centre. Direction : on relance vers le perdant.
            mBallX = arenaW * 0.5f;
            mBallY = arenaH * 0.5f;
            mTrailHead = 0;
            mTrailCount = 0;
            mBallVX = (side > 0 ? 1.0f : -1.0f) * kBallVxInit * mScale * mBallSpeedMul;
            mBallVY = Rand11() * 2.0f * mScale * mBallSpeedMul;

            // Conditions de victoire (score-based) apres chaque goal.
            if (ctx.settings != nullptr)
            {
                CheckWinConditions(*ctx.settings);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // CheckWinConditions — flexible : maxScore + winByTwo + timeLimit.
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::CheckWinConditions(const GameSettings& s)
        {
            if (mGameOver) return;

            const int diff = mScoreL - mScoreR;
            const int absDiff = (diff >= 0) ? diff : -diff;
            const int maxScore = s.maxScore;

            // 1. Victoire par score (premier a maxScore, avec winByTwo eventuel)
            if (maxScore > 0 && (mScoreL >= maxScore || mScoreR >= maxScore))
            {
                if (!s.winByTwo || absDiff >= 2)
                {
                    mGameOver = true;
                    mWinner   = (diff > 0) ? +1 : (diff < 0 ? -1 : 0);
                    logger.Info("[Gameplay] GAME OVER (score) winner=P{0}",
                                mWinner > 0 ? 1 : (mWinner < 0 ? 2 : 0));
                    return;
                }
            }

            // 2. Victoire au temps : si timeLimit defini ET timer expire
            if (s.timeLimit > 0.0f && mTimeLeft <= 0.0f)
            {
                mGameOver = true;
                mWinner   = (diff > 0) ? +1 : (diff < 0 ? -1 : 0);
                logger.Info("[Gameplay] GAME OVER (time) winner=P{0}",
                            mWinner > 0 ? 1 : (mWinner < 0 ? 2 : 0));
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // StartNewMatch — reset complet (utilise par REJOUER).
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::StartNewMatch(AppContext& ctx)
        {
            mScoreL = 0;
            mScoreR = 0;
            mGameOver = false;
            mWinner = 0;
            mPaused = false;
            mGoalCooldown = 0.0f;
            mGoalFlashAlpha = 0.0f;
            mTimeUp = 0.0f;
            if (ctx.settings != nullptr && ctx.settings->timeLimit > 0.0f)
            {
                mTimeLeft = ctx.settings->timeLimit;
            }
            const float arenaW = (float)ctx.viewportW;
            const float arenaH = math::NkMax(1.0f,
                                  (float)ctx.viewportH - mHUDTopH - mHUDBotH);
            ResetPositions(arenaW, arenaH);
            // Re-applique le multiplicateur de vitesse choisi par le joueur.
            const float mul = (ctx.settings != nullptr && ctx.settings->ballSpeedMul >= 1.0f)
                              ? ctx.settings->ballSpeedMul : 1.0f;
            mBallSpeedMul = mul;
            mBallVX = kBallVxInit * mScale * mul;
            mBallVY = kBallVyInit * mScale * mul;
            // Reset power-ups + dernier toucheur pour le nouveau match.
            mPowerUps.Reset();
            mLastToucher = 0;
            mParticles.Reset();
            // Respawn obstacles (reset des "collected" sur BonusStar)
            if (ctx.settings != nullptr)
            {
                mObstacles.SpawnFromSettings(*ctx.settings, arenaW, arenaH, mScale);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Rendu
        // ─────────────────────────────────────────────────────────────────────
        static void DrawArenaField(GLRenderer2D& r,
                                   float ax, float ay, float aw, float ah,
                                   float scale)
        {
            // Grille fine cyan : step et epaisseur scaled
            const float gridStep = 32.0f * scale;
            const float lineThick = math::NkMax(1.0f, scale);
            const math::NkColor gridCol = { 0, 245, 255, 10 };
            for (float x = ax; x < ax + aw; x += gridStep)
            {
                r.DrawQuad(x, ay, lineThick, ah, gridCol);
            }
            for (float y = ay; y < ay + ah; y += gridStep)
            {
                r.DrawQuad(ax, y, aw, lineThick, gridCol);
            }
            // Bordure cyan
            r.DrawQuadOutline(ax, ay, aw, ah, ColField(), lineThick);
            // Ligne centrale pointillee (dash 8 / gap 8 scales)
            const float midX = ax + aw * 0.5f;
            const float dashLen = 8.0f * scale;
            const float dashStep = 16.0f * scale;
            for (float yy = ay + 4.0f * scale; yy < ay + ah - 4.0f * scale; yy += dashStep)
            {
                r.DrawQuad(midX - lineThick * 0.5f, yy, lineThick, dashLen, ColDash());
            }
            // Cercle central r=50 scaled
            r.DrawCircleOutline(midX, ay + ah * 0.5f, 50.0f * scale,
                                ColRing(), lineThick, 48);
            // Goal glows lateraux (4 bandes degrade) scales
            const float bandW = 10.0f * scale;
            for (int i = 0; i < 4; ++i)
            {
                const float a = 1.0f - (float)i / 4.0f;
                math::NkColor cl = ColP1(); cl.a = static_cast<uint8_t>(45 * a);
                math::NkColor cr = ColP2(); cr.a = static_cast<uint8_t>(45 * a);
                r.DrawQuad(ax + i * bandW,           ay, bandW, ah, cl);
                r.DrawQuad(ax + aw - (i + 1) * bandW, ay, bandW, ah, cr);
            }
        }

        // Strategie texte responsive :
        //   - On garde un slot de base raisonnable (Body 18px par defaut)
        //   - On scale la rasterisation via DrawStringScaled(scale = mScale)
        //   - Resultat : texte 18 * 2.25 = 40px sur S22+ landscape, lisible.
        // Note : l'atlas raster est en GL_LINEAR donc l'upscale jusqu'a 3x
        // reste correct visuellement.

        static void DrawHUDTop(GLRenderer2D& r, FontAtlas& f,
                               int W, float hudH, float scale,
                               int scoreL, int scoreR, float timeShown,
                               int maxScore, bool countdown)
        {
            r.DrawQuad(0, 0, (float)W, hudH, { 5, 10, 20, 235 });
            r.DrawQuad(0, hudH - 1, (float)W, 1.0f, { 0, 245, 255, 26 });

            // Slots de base : on garde des slots raisonnables et on applique
            // mScale via DrawStringScaled. Sur mobile le texte sera donc
            // automatiquement scale (~2.5x sur S22+).
            const FontAtlas::SizeSlot slotLabel = FontAtlas::SmallSlot;   // 14
            const FontAtlas::SizeSlot slotScore = FontAtlas::SubtitleSlot;// 28
            const FontAtlas::SizeSlot slotCenter= FontAtlas::BodySlot;    // 18
            const FontAtlas::SizeSlot slotMatch = FontAtlas::SmallSlot;   // 14

            const float marginX = 20.0f * scale;
            const float avatarR = 14.0f * scale;
            const float avatarR2= 12.0f * scale;
            const float gx = marginX;
            const float cyH = hudH * 0.5f;
            r.DrawCircleOutline(gx + avatarR, cyH, avatarR,
                                ColP1(), 2.0f * scale, 32);
            r.DrawCircle(gx + avatarR, cyH, avatarR2,
                         { 0, 245, 255, 38 }, 32);
            f.DrawStringCenteredScaled(r, slotLabel, scale,
                               gx + avatarR, cyH - 7.0f * scale,
                               "P1", ColP1());
            const float textOff = (avatarR * 2.0f) + 10.0f * scale;
            f.DrawStringScaled(r, slotLabel, scale,
                         gx + textOff, 10.0f * scale, "JOUEUR 1", ColP1());
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", scoreL);
            f.DrawStringShadowScaled(r, slotScore, scale,
                             gx + textOff, 22.0f * scale, buf,
                             { 255, 255, 255, 255 }, ColP1(), 2);

            const int m = (int)(timeShown) / 60;
            const int s = (int)(timeShown) % 60;
            char tbuf[16];
            std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d", m, s);
            f.DrawStringCenteredScaled(r, slotCenter, scale,
                               (float)W * 0.5f, 6.0f * scale, tbuf,
                               { 255, 255, 255, 240 });
            // Label dynamique selon maxScore + mode timer.
            char labelBuf[48];
            if (maxScore > 0)
            {
                std::snprintf(labelBuf, sizeof(labelBuf), "1 VS 1 - %d PTS%s",
                              maxScore, countdown ? "" : "  (CHRONO)");
            }
            else
            {
                std::snprintf(labelBuf, sizeof(labelBuf),
                              "1 VS 1 - %s",
                              countdown ? "TEMPS LIMITE" : "CHRONO");
            }
            f.DrawStringCenteredScaled(r, slotMatch, scale,
                               (float)W * 0.5f, 30.0f * scale,
                               labelBuf,
                               { 255, 255, 255, 80 });

            const float dx = (float)W - marginX;
            r.DrawCircleOutline(dx - avatarR, cyH, avatarR,
                                ColP2(), 2.0f * scale, 32);
            r.DrawCircle(dx - avatarR, cyH, avatarR2,
                         { 255, 107, 0, 38 }, 32);
            f.DrawStringCenteredScaled(r, slotLabel, scale,
                               dx - avatarR, cyH - 7.0f * scale,
                               "P2", ColP2());
            std::snprintf(buf, sizeof(buf), "%d", scoreR);
            const float scoreW = f.MeasureWidthScaled(slotScore, scale, buf);
            const float nameW  = f.MeasureWidthScaled(slotLabel, scale, "JOUEUR 2");
            f.DrawStringScaled(r, slotLabel, scale,
                         dx - textOff - nameW, 10.0f * scale, "JOUEUR 2", ColP2());
            f.DrawStringShadowScaled(r, slotScore, scale,
                             dx - textOff - scoreW, 22.0f * scale, buf,
                             { 255, 255, 255, 255 }, ColP2(), 2);
        }

        static void DrawHUDBottom(GLRenderer2D& r, FontAtlas& f,
                                  int W, int H, float hudH, float scale,
                                  bool paused)
        {
            const float by = (float)H - hudH;
            r.DrawQuad(0, by, (float)W, hudH, { 5, 10, 20, 235 });
            r.DrawQuad(0, by, (float)W, 1.0f, { 0, 245, 255, 26 });

            const FontAtlas::SizeSlot slotHint = FontAtlas::SmallSlot;

            f.DrawStringCenteredScaled(r, slotHint, scale,
                               (float)W * 0.5f, by + hudH * 0.20f,
                               "P1 : W / S    OU GLISSER GAUCHE          P2 : FLECHES    OU GLISSER DROITE",
                               { 255, 255, 255, 100 });
            f.DrawStringCenteredScaled(r, slotHint, scale,
                               (float)W * 0.5f, by + hudH * 0.55f,
                               paused ? "ESPACE / TAP : REPRENDRE     ECHAP : MENU"
                                      : "ESPACE / TAP : PAUSE     ECHAP : MENU",
                               { 255, 255, 255, 70 });
        }

        // ─────────────────────────────────────────────────────────────────────
        // Trail + balle avec effet de lumiere (halo blanc + glow cyan).
        // Reproduit l'effet shadowBlur=18 du HTML (couches concentriques).
        // ─────────────────────────────────────────────────────────────────────
        static void DrawBallAndTrail(GLRenderer2D& r,
                                     float ax, float ay,
                                     float bx, float by, float br,
                                     const float* trX, const float* trY,
                                     int trHead, int trCount, int trMax)
        {
            // Trail : iterer du plus ancien au plus recent, taille croissante.
            // Chaque point a un halo cyan + un cercle blanc.
            const int oldest = (trHead - trCount + trMax) % trMax;
            for (int i = 0; i < trCount; ++i)
            {
                const int idx = (oldest + i) % trMax;
                const float t = (float)(i + 1) / (float)trCount;  // 0..1
                const float rad = br * t * 0.85f;
                // Halo cyan large (shadowBlur=8 du HTML)
                math::NkColor glow = { 0, 245, 255, static_cast<uint8_t>(110.0f * t) };
                r.DrawCircle(ax + trX[idx], ay + trY[idx], rad * 1.8f, glow, 16);
                // Cercle blanc translucide
                math::NkColor c = { 255, 255, 255, static_cast<uint8_t>(180.0f * t) };
                r.DrawCircle(ax + trX[idx], ay + trY[idx], rad, c, 16);
            }

            // Balle : halo 5 couches decroissantes (shadowBlur=18 du HTML)
            // De l'exterieur vers l'interieur, alpha croissant.
            for (int i = 5; i >= 1; --i)
            {
                const float radius = br + (float)i * 3.5f;
                // Mix cyan -> blanc en se rapprochant du centre
                const float t = 1.0f - (float)i / 5.0f;  // 0 a l'exterieur, 1 au centre
                math::NkColor halo;
                halo.r = static_cast<uint8_t>(  0 + 255.0f * t);
                halo.g = static_cast<uint8_t>(245 +  10.0f * t);
                halo.b = static_cast<uint8_t>(255);
                halo.a = static_cast<uint8_t>(30 + 40 * t);
                r.DrawCircle(ax + bx, ay + by, radius, halo, 24);
            }
            // Cercle blanc plein au centre
            r.DrawCircle(ax + bx, ay + by, br, { 255, 255, 255, 255 }, 24);
        }

        // ─────────────────────────────────────────────────────────────────────
        // Paddle avec halo (shadowBlur=20 du HTML). Couleur du halo = couleur
        // du paddle. 6 couches concentriques pour simuler le blur.
        // ─────────────────────────────────────────────────────────────────────
        static void DrawPaddle(GLRenderer2D& r,
                               float x, float y, float w, float h,
                               math::NkColor c)
        {
            // 6 quads remplis de plus en plus larges, alpha decroissant.
            // Couches "soft" empilees : effet de gradient autour du paddle.
            for (int i = 6; i >= 1; --i)
            {
                const float pad = (float)i * 2.5f;
                math::NkColor glow = c;
                // alpha decroit avec la distance (30 -> 5)
                glow.a = static_cast<uint8_t>(34 - (i - 1) * 5);
                r.DrawQuad(x - pad, y - pad,
                           w + pad * 2.0f, h + pad * 2.0f, glow);
            }
            // Outline lumineux (rebord net)
            math::NkColor edge = c; edge.a = 220;
            r.DrawQuadOutline(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, edge, 1.0f);
            // Paddle plein
            r.DrawQuad(x, y, w, h, c);
            // Highlight central blanc tres leger (effet "verre" du neon HTML)
            r.DrawQuad(x + 1.0f, y + 1.0f, w - 2.0f, h * 0.35f,
                       { 255, 255, 255, 50 });
        }

        void GameplayScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // Arene : entre HUD top et HUD bot (scales)
            const float ax = 0.0f;
            const float ay = mHUDTopH;
            const float aw = (float)W;
            const float ah = (float)H - mHUDTopH - mHUDBotH;
            if (ah > 0.0f)
            {
                DrawArenaField(r, ax, ay, aw, ah, mScale);

                // Obstacles (sous les paddles et la balle pour que ces
                // derniers passent visuellement par-dessus).
                mObstacles.Render(r, ax, ay, mScale);

                // Power-up drops (orbes qui descendent du haut). Render avant
                // les paddles pour qu'un orbe au contact passe derriere.
                mPowerUps.Render(r, f, ax, ay, mScale);

                // Paddles (hauteur effective : GiantPaddle x2 ou MiniPaddle x0.5)
                const float pHL_r = mPaddleH * EffectivePaddleHMul(-1);
                const float pHR_r = mPaddleH * EffectivePaddleHMul(+1);
                DrawPaddle(r, ax + mPaddleLX, ay + mPaddleLY,
                           mPaddleW, pHL_r, ColP1());
                DrawPaddle(r, ax + mPaddleRX, ay + mPaddleRY,
                           mPaddleW, pHR_r, ColP2());

                // Trail + balle (par-dessus tout)
                DrawBallAndTrail(r, ax, ay, mBallX, mBallY, mBallR,
                                 mTrailX, mTrailY, mTrailHead, mTrailCount, kTrailMax);

                // Particules (juice) au-dessus des paddles/balle pour bien
                // souligner l'evenement.
                mParticles.Render(r, ax, ay);

                // Blind overlay : si malus Blind actif sur un cote, on
                // assombrit la MOITIE DE TERRAIN du cote affecte (l'effet
                // simule la perte de visibilite du joueur affecte).
                if (mPowerUps.IsBlind(-1))
                {
                    r.DrawQuad(ax,            ay, aw * 0.5f, ah,
                               { 0, 0, 0, 180 });
                }
                if (mPowerUps.IsBlind(+1))
                {
                    r.DrawQuad(ax + aw * 0.5f, ay, aw * 0.5f, ah,
                               { 0, 0, 0, 180 });
                }

                // Goal flash sur l'arene
                if (mGoalFlashAlpha > 0.0f)
                {
                    math::NkColor c = (mGoalFlashSide > 0) ? ColP1() : ColP2();
                    c.a = static_cast<uint8_t>(50 * mGoalFlashAlpha);
                    r.DrawQuad(ax, ay, aw, ah, c);
                }

                // ── HUD effets actifs : badges nom + barre de temps ─────
                // Pour chaque effet actif, on dessine un petit bandeau dans
                // la zone HUD top (au-dessus de l'arene) : pastille couleur,
                // nom du bonus/malus, et une barre de progression sous le
                // texte indiquant le temps restant. Les badges du joueur
                // gauche sont alignes a gauche, ceux du joueur droit a droite.
                {
                    const float badgeW = 100.0f * mScale;
                    const float badgeH = 22.0f  * mScale;
                    const float pad    = 4.0f   * mScale;
                    int countL = 0, countR = 0;
                    for (int i = 0; i < mPowerUps.EffectCount(); ++i)
                    {
                        const ActiveEffect& e = mPowerUps.GetEffect(i);
                        int& cnt = (e.side < 0) ? countL : countR;
                        const float bx = (e.side < 0)
                            ? (8.0f * mScale)
                            : ((float)W - badgeW - 8.0f * mScale);
                        const float by = mHUDTopH + 6.0f * mScale
                                       + cnt * (badgeH + pad);
                        // Fond + border colore selon le type.
                        math::NkColor fg = mPowerUps.GetEffectColor(i);
                        math::NkColor bg = fg; bg.a = 30;
                        math::NkColor bd = fg; bd.a = 200;
                        r.DrawQuad       (bx, by, badgeW, badgeH, bg);
                        r.DrawQuadOutline(bx, by, badgeW, badgeH, bd, 1.5f);
                        // Pastille a gauche du texte
                        const float dotR = badgeH * 0.30f;
                        r.DrawCircle(bx + 8.0f * mScale + dotR,
                                     by + badgeH * 0.5f, dotR, fg, 12);
                        // Texte nom (court)
                        f.DrawStringScaled(r, FontAtlas::SmallSlot, mScale,
                                     bx + 8.0f * mScale + dotR * 2.0f + 4.0f * mScale,
                                     by + badgeH * 0.20f,
                                     mPowerUps.GetEffectName(i),
                                     { 255, 255, 255, 230 });
                        // Barre de temps en bas du badge.
                        const float barH = 2.0f * mScale;
                        const float barW = badgeW - 4.0f * mScale;
                        const float frac = (e.duration > 0.001f)
                            ? math::NkClamp(e.timeLeft / e.duration, 0.0f, 1.0f)
                            : 1.0f;
                        r.DrawQuad(bx + 2.0f * mScale, by + badgeH - barH - 2.0f,
                                   barW, barH, { 255, 255, 255, 40 });
                        r.DrawQuad(bx + 2.0f * mScale, by + badgeH - barH - 2.0f,
                                   barW * frac, barH, bd);
                        ++cnt;
                    }
                }

                // ── Toast notifications (popup d'application) ──────────────
                // Affiche un bandeau central par cote pendant kNotifDuration
                // secondes. Anim simple : slide-in vertical sur 0.15s, hold,
                // fade-out sur 0.3s.
                for (int side = -1; side <= 1; side += 2)
                {
                    const auto& n = mPowerUps.GetNotification(side);
                    if (!n.active) continue;
                    const float elapsed = n.duration - n.timeLeft;
                    const float slideIn = math::NkClamp(elapsed / 0.15f, 0.0f, 1.0f);
                    const float fadeOut = math::NkClamp(n.timeLeft / 0.30f, 0.0f, 1.0f);
                    const float alpha   = slideIn * fadeOut;

                    const float toastW = 220.0f * mScale;
                    const float toastH = 44.0f  * mScale;
                    const float tx     = (side < 0)
                        ? ((float)W * 0.25f - toastW * 0.5f)
                        : ((float)W * 0.75f - toastW * 0.5f);
                    // Slide-in depuis le haut : decale verticalement.
                    const float tyFinal = ay + 30.0f * mScale;
                    const float ty      = tyFinal - (1.0f - slideIn) * 20.0f * mScale;

                    math::NkColor c = mPowerUps.NotifColor(n);
                    math::NkColor bg = c; bg.a = static_cast<uint8_t>(160 * alpha);
                    math::NkColor bd = c; bd.a = static_cast<uint8_t>(240 * alpha);
                    r.DrawQuad       (tx, ty, toastW, toastH, bg);
                    r.DrawQuadOutline(tx, ty, toastW, toastH, bd, 2.0f);

                    // Prefixe + nom : "+ BOUCLIER" ou "- AVEUGLE"
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s %s",
                                  n.isBonus ? "+" : "-",
                                  mPowerUps.NotifName(n));
                    math::NkColor txtCol = { 255, 255, 255, 255 };
                    txtCol.a = static_cast<uint8_t>(255 * alpha);
                    f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                       tx + toastW * 0.5f,
                                       ty + toastH * 0.20f,
                                       buf, txtCol);
                }
            }

            // HUDs : choisir mode timer selon GameSettings
            const bool countdown = (ctx.settings != nullptr
                                    && ctx.settings->timeLimit > 0.0f);
            const float timeShown = countdown ? mTimeLeft : mTimeUp;
            const int maxScore = (ctx.settings != nullptr)
                                 ? ctx.settings->maxScore : 11;
            DrawHUDTop(r, f, W, mHUDTopH, mScale,
                       mScoreL, mScoreR, timeShown, maxScore, countdown);
            DrawHUDBottom(r, f, W, H, mHUDBotH, mScale, mPaused);

            // ── Bouton PAUSE flottant (HORS arene, hors zone timer) ─────────
            // Place dans la barre HUD top, juste sous le bord superieur, a
            // droite du timer central. Ne chevauche ni le timer (centre) ni
            // les avatars/scores P1/P2 (extremites).
            {
                const float btnSize = mHUDTopH * 0.65f;
                mPauseBtnW = btnSize;
                mPauseBtnH = btnSize;
                // Position : ~70% de la largeur (entre le centre et l'avatar P2)
                mPauseBtnX = (float)W * 0.70f - btnSize * 0.5f;
                mPauseBtnY = (mHUDTopH - btnSize) * 0.5f;

                // Fond + border cyan
                math::NkColor bg = ColP1(); bg.a = 30;
                math::NkColor bd = ColP1(); bd.a = 180;
                r.DrawQuad       (mPauseBtnX, mPauseBtnY, mPauseBtnW, mPauseBtnH, bg);
                r.DrawQuadOutline(mPauseBtnX, mPauseBtnY, mPauseBtnW, mPauseBtnH, bd, 1.5f * mScale);

                // Icone : 2 barres verticales si non-paused, triangle play si paused
                const float ix = mPauseBtnX + mPauseBtnW * 0.5f;
                const float iy = mPauseBtnY + mPauseBtnH * 0.5f;
                const float ih = mPauseBtnH * 0.4f;
                if (mPaused)
                {
                    // Triangle play (3 lignes vers le sommet droit)
                    const float tw = mPauseBtnW * 0.3f;
                    r.DrawTriangle(ix - tw * 0.5f, iy - ih,
                                   ix - tw * 0.5f, iy + ih,
                                   ix + tw * 0.5f, iy,
                                   ColP1());
                }
                else
                {
                    // 2 barres pause
                    const float bw = mPauseBtnW * 0.10f;
                    const float gap = mPauseBtnW * 0.08f;
                    r.DrawQuad(ix - gap - bw, iy - ih, bw, ih * 2.0f, ColP1());
                    r.DrawQuad(ix + gap,      iy - ih, bw, ih * 2.0f, ColP1());
                }
            }

            // ── Overlay GAME OVER (prioritaire sur PAUSE) ────────────────────
            if (mGameOver)
            {
                r.DrawQuad(0, 0, (float)W, (float)H, { 5, 10, 20, 230 });
                const float cx = (float)W * 0.5f;
                const float cy = (float)H * 0.5f;

                // Titre dynamique selon vainqueur
                const char* title = (mWinner > 0) ? "VICTOIRE  P1"
                                  : (mWinner < 0) ? "VICTOIRE  P2"
                                                  : "EGALITE";
                math::NkColor titleC = (mWinner > 0) ? ColP1()
                                    : (mWinner < 0) ? ColP2()
                                                    : math::NkColor{ 255, 215, 0, 255 };
                f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, mScale,
                                         cx, cy - 130.0f * mScale, title,
                                         { 255, 255, 255, 255 }, titleC, 3);

                // Score final
                char scoreBuf[24];
                std::snprintf(scoreBuf, sizeof(scoreBuf), "%d  -  %d",
                              mScoreL, mScoreR);
                f.DrawStringCenteredScaled(r, FontAtlas::DisplaySlot, mScale,
                                   cx, cy - 60.0f * mScale, scoreBuf,
                                   { 255, 255, 255, 230 });

                // Boutons REJOUER / RETOUR MENU
                const float btnW = 280.0f * mScale;
                const float btnH = 56.0f  * mScale;
                const float btnGap = 16.0f * mScale;

                mGOReplayBtnW = btnW; mGOReplayBtnH = btnH;
                mGOReplayBtnX = cx - btnW * 0.5f;
                mGOReplayBtnY = cy + 24.0f * mScale;
                math::NkColor pbg = ColP1(); pbg.a = 32;
                math::NkColor pbd = ColP1(); pbd.a = 200;
                r.DrawQuad       (mGOReplayBtnX, mGOReplayBtnY, btnW, btnH, pbg);
                r.DrawQuadOutline(mGOReplayBtnX, mGOReplayBtnY, btnW, btnH, pbd, 2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mGOReplayBtnY + btnH * 0.30f,
                                   "REJOUER  (ENTREE)", ColP1());

                mGOMenuBtnW = btnW; mGOMenuBtnH = btnH;
                mGOMenuBtnX = cx - btnW * 0.5f;
                mGOMenuBtnY = mGOReplayBtnY + btnH + btnGap;
                math::NkColor mbg = { 255, 64, 64, 32 };
                math::NkColor mbd = { 255, 64, 64, 200 };
                r.DrawQuad       (mGOMenuBtnX, mGOMenuBtnY, btnW, btnH, mbg);
                r.DrawQuadOutline(mGOMenuBtnX, mGOMenuBtnY, btnW, btnH, mbd, 2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mGOMenuBtnY + btnH * 0.30f,
                                   "RETOUR MENU  (ECHAP)",
                                   { 255, 64, 64, 230 });
            }
            // ── Overlay PAUSE avec boutons cliquables ────────────────────────
            else if (mPaused)
            {
                r.DrawQuad(0, 0, (float)W, (float)H, { 5, 10, 20, 220 });
                const float cx = (float)W * 0.5f;
                const float cy = (float)H * 0.5f;

                // Titre PAUSE
                f.DrawStringShadowCenteredScaled(r, FontAtlas::HeadlineSlot, mScale,
                                         cx, cy - 90.0f * mScale, "PAUSE",
                                         { 255, 255, 255, 255 }, ColP1(), 3);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, cy - 30.0f * mScale,
                                   "LE MATCH EST EN ATTENTE",
                                   { 255, 255, 255, 120 });

                // Boutons REPRENDRE et RETOUR MENU centres
                const float btnW = 280.0f * mScale;
                const float btnH = 56.0f  * mScale;
                const float btnGap = 16.0f * mScale;

                // REPRENDRE (cyan)
                mResumeBtnW = btnW; mResumeBtnH = btnH;
                mResumeBtnX = cx - btnW * 0.5f;
                mResumeBtnY = cy + 16.0f * mScale;
                math::NkColor rbg = ColP1(); rbg.a = 32;
                math::NkColor rbd = ColP1(); rbd.a = 200;
                r.DrawQuad       (mResumeBtnX, mResumeBtnY, mResumeBtnW, mResumeBtnH, rbg);
                r.DrawQuadOutline(mResumeBtnX, mResumeBtnY, mResumeBtnW, mResumeBtnH, rbd, 2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mResumeBtnY + btnH * 0.30f,
                                   "REPRENDRE  (ESPACE / TAP)", ColP1());

                // RETOUR MENU (rouge)
                mMenuBtnW = btnW; mMenuBtnH = btnH;
                mMenuBtnX = cx - btnW * 0.5f;
                mMenuBtnY = mResumeBtnY + btnH + btnGap;
                math::NkColor mbg = { 255, 64, 64, 32 };
                math::NkColor mbd = { 255, 64, 64, 200 };
                r.DrawQuad       (mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH, mbg);
                r.DrawQuadOutline(mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH, mbd, 2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mMenuBtnY + btnH * 0.30f,
                                   "RETOUR MENU  (ECHAP)",
                                   { 255, 64, 64, 230 });
            }

            // ── Overlay "ADVERSAIRE PARTI" (mode reseau, perte du pair) ─────
            // Affiche un fade noir + un panneau central avec message + bouton
            // RETOUR MENU. Le bouton fait PopToRoot pour ramener au MainMenu
            // (la NetworkLobbyScene::OnExit fera Shutdown de la session).
            if (mNetPeerLost)
            {
                const float W = (float)ctx.viewportW;
                const float H = (float)ctx.viewportH;
                const float cx = W * 0.5f;
                const float cy = H * 0.5f;
                // Fade noir progressif (apparait en ~0.3s)
                const float fade = math::NkMin(1.0f, mNetLostTimer / 0.3f);
                r.DrawQuad(0.0f, 0.0f, W, H,
                           { 0, 0, 0, (uint8)(180 * fade) });
                // Panneau central
                const float panelW = math::NkMin(560.0f * mScale, W * 0.85f);
                const float panelH = 220.0f * mScale;
                const float px = cx - panelW * 0.5f;
                const float py = cy - panelH * 0.5f;
                r.DrawQuad       (px, py, panelW, panelH,
                                  { 24, 28, 36, (uint8)(230 * fade) });
                r.DrawQuadOutline(px, py, panelW, panelH,
                                  { 255, 80, 80, (uint8)(220 * fade) },
                                  2.0f * mScale);
                // Titre
                f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, mScale,
                                   cx, py + 22.0f * mScale,
                                   "ADVERSAIRE PARTI",
                                   { 255, 80, 80, (uint8)(250 * fade) });
                // Sous-titre
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, py + 70.0f * mScale,
                                   "LE MATCH EST TERMINE.",
                                   { 255, 255, 255, (uint8)(220 * fade) });
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, mScale,
                                   cx, py + 100.0f * mScale,
                                   "TON ADVERSAIRE A QUITTE OU PERDU LA CONNEXION.",
                                   { 255, 255, 255, (uint8)(180 * fade) });
                // Bouton RETOUR MENU
                mLostMenuBtnW = 240.0f * mScale;
                mLostMenuBtnH = 50.0f  * mScale;
                mLostMenuBtnX = cx - mLostMenuBtnW * 0.5f;
                mLostMenuBtnY = py + panelH - mLostMenuBtnH - 18.0f * mScale;
                r.DrawQuad       (mLostMenuBtnX, mLostMenuBtnY,
                                  mLostMenuBtnW, mLostMenuBtnH,
                                  { 255, 64, 64, (uint8)(60 * fade) });
                r.DrawQuadOutline(mLostMenuBtnX, mLostMenuBtnY,
                                  mLostMenuBtnW, mLostMenuBtnH,
                                  { 255, 64, 64, (uint8)(220 * fade) },
                                  2.0f * mScale);
                f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, mScale,
                                   cx, mLostMenuBtnY + 14.0f * mScale,
                                   "RETOUR MENU  (ECHAP)",
                                   { 255, 200, 200, (uint8)(240 * fade) });
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnEvent — clavier + touch (drag vertical) + souris
        // ─────────────────────────────────────────────────────────────────────
        // Touch/mouse model :
        //   - L'arene est divisee en 2 moities (X < W/2 = P1 gauche, sinon P2)
        //   - Sur TouchBegin / MouseDown : on associe le touch ID a la moitie
        //   - Sur TouchMove / MouseMove tant que le bouton est tenu : on
        //     applique deltaY a mPaddleLY/RY
        //   - Sur TouchEnd / MouseUp : on libere
        // ─────────────────────────────────────────────────────────────────────
        void GameplayScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            // ── Clavier ─────────────────────────────────────────────────────
            if (auto* kp = ev.As<NkKeyPressEvent>())
            {
                const NkKey k = kp->GetKey();
                // Si game over : Entree = rejouer, Echap = menu
                if (mGameOver)
                {
                    if (k == NkKey::NK_ENTER || k == NkKey::NK_NUMPAD_ENTER
                     || k == NkKey::NK_SPACE)
                    {
                        RequestReplay(ctx);
                    }
                    else if (k == NkKey::NK_ESCAPE)
                    {
                        ctx.scenes->Pop();
                    }
                    return;
                }
                // Si peer parti en mode reseau : Echap/Enter/Espace -> menu
                if (mNetPeerLost)
                {
                    if (k == NkKey::NK_ESCAPE || k == NkKey::NK_ENTER
                     || k == NkKey::NK_NUMPAD_ENTER || k == NkKey::NK_SPACE)
                    {
                        // Retour menu : PopToRoot ramene a MainMenu et le
                        // shutdown de NetworkSession se fait via OnExit des
                        // scenes intermediaires.
                        ctx.scenes->PopToRoot();
                    }
                    return;
                }
                // DIAG : log les premiers key press pour confirmer que les
                // events arrivent bien jusqu'a GameplayScene (utile en reseau
                // pour distinguer "client pas focus" de "bug code").
                {
                    static int sKeyDiag = 0;
                    if (sKeyDiag < 8)
                    {
                        logger.Info("[Gameplay][KP] role={0} key={1}",
                                    mIsNetwork ? (mIsHost ? "HOST" : "CLIENT")
                                               : "LOCAL",
                                    (int)k);
                        sKeyDiag++;
                    }
                }
                switch (k)
                {
                case NkKey::NK_W:     mKeyW = true;    break;
                case NkKey::NK_S:     mKeyS = true;    break;
                case NkKey::NK_UP:    mKeyUp = true;   break;
                case NkKey::NK_DOWN:  mKeyDown = true; break;
                case NkKey::NK_SPACE: RequestPauseToggle(ctx); break;
                case NkKey::NK_ESCAPE:
                    if (mPaused) ctx.scenes->Pop();
                    else         RequestPauseToggle(ctx);
                    break;
                default: break;
                }
                return;
            }
            if (auto* kr = ev.As<NkKeyReleaseEvent>())
            {
                const NkKey k = kr->GetKey();
                switch (k)
                {
                case NkKey::NK_W:    mKeyW = false;    break;
                case NkKey::NK_S:    mKeyS = false;    break;
                case NkKey::NK_UP:   mKeyUp = false;   break;
                case NkKey::NK_DOWN: mKeyDown = false; break;
                default: break;
                }
                return;
            }

            const float halfW = (float)ctx.viewportW * 0.5f;

            // Helper local : teste si le point (x,y) tombe sur un des boutons
            // UI prioritaires (PAUSE / REPRENDRE / RETOUR MENU). Retourne
            // true si l'event a ete consomme par un bouton (= ne pas le
            // propager au drag paddle).
            auto handleUIButtons = [&](float x, float y) -> bool
            {
                // PEER PARTI : un seul bouton RETOUR MENU
                if (mNetPeerLost)
                {
                    if (PointInRect(x, y, mLostMenuBtnX, mLostMenuBtnY,
                                          mLostMenuBtnW, mLostMenuBtnH))
                    {
                        ctx.scenes->PopToRoot();
                        return true;
                    }
                    return true;  // tout autre tap absorbe pendant l'overlay
                }
                // GAME OVER : boutons REJOUER / RETOUR MENU
                if (mGameOver)
                {
                    if (PointInRect(x, y, mGOReplayBtnX, mGOReplayBtnY,
                                          mGOReplayBtnW, mGOReplayBtnH))
                    {
                        RequestReplay(ctx);
                        return true;
                    }
                    if (PointInRect(x, y, mGOMenuBtnX, mGOMenuBtnY,
                                          mGOMenuBtnW, mGOMenuBtnH))
                    {
                        // RETOUR MENU = sauter direct au menu principal, pas
                        // un Pop simple (sinon on retomberait sur MatchConfig).
                        ctx.scenes->PopToRoot();
                        return true;
                    }
                    return true;  // tout autre tap absorbe pendant game over
                }
                // Bouton PAUSE flottant (toujours visible)
                if (PointInRect(x, y, mPauseBtnX, mPauseBtnY, mPauseBtnW, mPauseBtnH))
                {
                    RequestPauseToggle(ctx);
                    return true;
                }
                if (mPaused)
                {
                    if (PointInRect(x, y, mResumeBtnX, mResumeBtnY, mResumeBtnW, mResumeBtnH))
                    {
                        RequestPauseToggle(ctx);
                        return true;
                    }
                    if (PointInRect(x, y, mMenuBtnX, mMenuBtnY, mMenuBtnW, mMenuBtnH))
                    {
                        // RETOUR MENU depuis le panneau Pause : meme logique
                        // que pour le GameOver — saute direct au menu.
                        ctx.scenes->PopToRoot();
                        return true;
                    }
                    return true;
                }
                return false;
            };

            // ── Souris (desktop) : drag bouton gauche tenu ──────────────────
            if (auto* mp = ev.As<NkMouseButtonPressEvent>())
            {
                if (mp->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float mx = (float)mp->GetX();
                    const float my = (float)mp->GetY();
                    if (handleUIButtons(mx, my)) return;
                    // Ne PAS prendre le controle d'un paddle pilote par l'IA.
                    if (mx < halfW)
                    {
                        if (!mAILeftEnabled)  { mMouseDownL = true;  mLastMouseY = my; }
                    }
                    else
                    {
                        if (!mAIRightEnabled) { mMouseDownR = true;  mLastMouseY = my; }
                    }
                }
                return;
            }
            if (auto* mr = ev.As<NkMouseButtonReleaseEvent>())
            {
                if (mr->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    mMouseDownL = false;
                    mMouseDownR = false;
                }
                return;
            }
            if (auto* mm = ev.As<NkMouseMoveEvent>())
            {
                const float my = (float)mm->GetY();
                const float dy = my - mLastMouseY;
                if (mMouseDownL) mPaddleLY += dy;
                if (mMouseDownR) mPaddleRY += dy;
                mLastMouseY = my;
                return;
            }

            // ── Touch (mobile) : drag par moitie ────────────────────────────
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                for (uint32 i = 0; i < tb->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = tb->GetTouch(i);
                    // Test bouton PAUSE / overlay AVANT le drag paddle
                    if (handleUIButtons(tp.clientX, tp.clientY)) continue;
                    // Ne PAS prendre le controle d'un paddle pilote par l'IA.
                    if (tp.clientX < halfW)
                    {
                        if (mAILeftEnabled) continue;
                        mTouchIdL    = (long long)tp.id;
                        mLastTouchYL = tp.clientY;
                    }
                    else
                    {
                        if (mAIRightEnabled) continue;
                        mTouchIdR    = (long long)tp.id;
                        mLastTouchYR = tp.clientY;
                    }
                }
                return;
            }
            if (auto* tm = ev.As<NkTouchMoveEvent>())
            {
                for (uint32 i = 0; i < tm->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = tm->GetTouch(i);
                    const long long id = (long long)tp.id;
                    if (id == mTouchIdL)
                    {
                        const float dy = tp.clientY - mLastTouchYL;
                        mPaddleLY += dy;
                        mLastTouchYL = tp.clientY;
                    }
                    else if (id == mTouchIdR)
                    {
                        const float dy = tp.clientY - mLastTouchYR;
                        mPaddleRY += dy;
                        mLastTouchYR = tp.clientY;
                    }
                }
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                for (uint32 i = 0; i < te->GetNumTouches(); ++i)
                {
                    const NkTouchPoint& tp = te->GetTouch(i);
                    const long long id = (long long)tp.id;
                    if (id == mTouchIdL) mTouchIdL = -1;
                    if (id == mTouchIdR) mTouchIdR = -1;
                }
                return;
            }
            if (auto* tc = ev.As<NkTouchCancelEvent>())
            {
                (void)tc;
                mTouchIdL = -1;
                mTouchIdR = -1;
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
