// =============================================================================
// NetworkLobbyScene.cpp
// =============================================================================

#include "NetworkLobbyScene.h"
#include "GameplayScene.h"
#include "SelectMatchConfigScene.h"
#include "Pong/Net/NetworkSession.h"
#include "Pong/Net/NetProtocol.h"
#include "NKNetwork/Protocol/NkConnection.h"  // pour NkReceiveMsg
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>
#include <cstring>

namespace nkentseu
{
    namespace pong
    {

        static float EaseOutCubic(float t)
        {
            if (t <= 0.0f) return 0.0f;
            if (t >= 1.0f) return 1.0f;
            const float u = 1.0f - t;
            return 1.0f - u * u * u;
        }

        // ─────────────────────────────────────────────────────────────────────
        void NetworkLobbyScene::OnEnter(AppContext& /*ctx*/)
        {
            mTime = 0.0f;
            mEnterAnim = 0.0f;
            mView = 0;
            mActiveTouchId = -1;
            // Grace period : absorbe le release/touch-end "fantome" du clic
            // qui a pousse cette scene depuis SelectModeScene. Sans ca, si la
            // souris ou le doigt etait au-dessus de HEBERGER/REJOINDRE quand
            // on entre, l'action se declenche immediatement sans intention.
            mInputArmDelay = 0.20f;
            logger.Info("[NetLobby] OnEnter");
        }

        void NetworkLobbyScene::OnExit(AppContext& ctx)
        {
            // Si on quitte sans avoir lance le match, on coupe la session
            // pour ne pas laisser un socket ouvert dans le vide.
            if (ctx.network != nullptr
             && ctx.network->State() != NetworkState::Idle)
            {
                logger.Info("[NetLobby] OnExit -> shutdown session");
                ctx.network->Shutdown();
            }
        }

        void NetworkLobbyScene::OnUpdate(AppContext& ctx, float dt)
        {
            mTime += dt;
            mEnterAnim += dt / 0.3f;
            if (mEnterAnim > 1.0f) mEnterAnim = 1.0f;

            // Decrement de la grace period d'entree (evite l'auto-declenchement
            // d'un bouton si la souris/le doigt etait deja dessus a l'entree).
            if (mInputArmDelay > 0.0f)
            {
                mInputArmDelay -= dt;
                if (mInputArmDelay < 0.0f) mInputArmDelay = 0.0f;
            }

            // ── Drain reseau : ecoute kMsgStartMatch cote CLIENT ─────────────
            // Quand le HOST lance le match depuis SelectMatchConfigScene, il
            // envoie un kMsgStartMatch au CLIENT. Ce dernier est encore dans
            // NetworkLobbyScene (en attente) et doit pousser GameplayScene.
            // Cote HOST, NetworkLobbyScene n'est pas dans la stack au moment
            // ou il lance (il est passe par MatchConfig), donc ce code ne
            // s'execute que cote CLIENT.
            if (ctx.network != nullptr
             && ctx.network->State() == NetworkState::Connected
             && ctx.network->Role() == NetworkRole::Client)
            {
                NkVector<net::NkReceiveMsg> incoming;
                ctx.network->DrainReceived(incoming);
                for (const auto& msg : incoming)
                {
                    if (msg.size < 1) continue;
                    if (msg.data[0] != netproto::kMsgStartMatch) continue;
                    if (msg.size < sizeof(netproto::PktStartMatch)) continue;

                    netproto::PktStartMatch pkt;
                    std::memcpy(&pkt, msg.data, sizeof(pkt));
                    // Applique les settings critiques transmis par le HOST.
                    if (ctx.settings != nullptr)
                    {
                        ctx.settings->maxScore     = pkt.maxScore;
                        ctx.settings->timeLimit    = (float)pkt.timeLimitSec;
                        ctx.settings->ballSpeedMul = pkt.ballSpeedMul;
                        ctx.settings->winByTwo
                            = (pkt.flags & netproto::kStartFlagWinByTwo) != 0;
                        ctx.settings->powerUpsEnabled
                            = (pkt.flags & netproto::kStartFlagPowerUpsOn) != 0;
                        // Seed deterministe + toggles obstacles : memes
                        // obstacles aux memes positions cote HOST et CLIENT.
                        ctx.settings->obstacleSeed = pkt.obstacleSeed;
                        for (int i = 0; i < 8; ++i)
                        {
                            ctx.settings->obsActive[i] = (pkt.obsActive[i] != 0);
                        }
                    }
                    logger.Info("[NetLobby] CLIENT recoit kMsgStartMatch (seed={0}) -> Push Gameplay",
                                pkt.obstacleSeed);
                    // PUSH Gameplay : NetLobby reste en dessous dans la stack,
                    // identique au comportement HOST (qui passe par MatchConfig).
                    PushGameplay(ctx);
                    break;  // un seul start par session
                }
            }
        }

        bool NetworkLobbyScene::HitTest(float sx, float sy,
                                        float bx, float by, float bw, float bh) const
        {
            return sx >= bx && sx <= bx + bw
                && sy >= by && sy <= by + bh;
        }

        // ── Actions ──────────────────────────────────────────────────────────
        void NetworkLobbyScene::DoHost(AppContext& ctx)
        {
            if (ctx.network == nullptr) return;
            const bool ok = ctx.network->StartHost(kDefaultPort);
            logger.Info("[NetLobby] StartHost(port={0}) -> {1}",
                        kDefaultPort, ok ? "OK" : "FAIL");
        }
        void NetworkLobbyScene::DoJoin(AppContext& ctx)
        {
            if (ctx.network == nullptr) return;
            const bool ok = ctx.network->StartJoin(mIp, kDefaultPort);
            logger.Info("[NetLobby] StartJoin({0}:{1}) -> {2}",
                        mIp, kDefaultPort, ok ? "OK" : "FAIL");
        }
        void NetworkLobbyScene::DoCancel(AppContext& ctx)
        {
            if (ctx.network != nullptr) ctx.network->Shutdown();
            mView = 0;
            logger.Info("[NetLobby] Cancel");
        }
        void NetworkLobbyScene::DoLaunch(AppContext& ctx)
        {
            if (ctx.network == nullptr) return;
            if (ctx.network->State() != NetworkState::Connected) return;
            // Mode reseau : seul le HOST peut lancer (CLIENT attend kMsgStartMatch).
            // On passe par SelectMatchConfigScene pour que l'utilisateur configure
            // le match. SelectMatchConfigScene::Launch enverra kMsgStartMatch au
            // client AVANT de push GameplayScene.
            if (ctx.network->Role() != NetworkRole::Host)
            {
                // Cote CLIENT : ignore le clic (le bouton ne devrait pas etre
                // visible mais on garde la garde au cas ou).
                return;
            }
            logger.Info("[NetLobby] HOST push SelectMatchConfigScene");
            if (ctx.scenes != nullptr)
            {
                ctx.scenes->Push(new SelectMatchConfigScene());
            }
        }
        void NetworkLobbyScene::PushGameplay(AppContext& ctx)
        {
            // Pousse GameplayScene direct. Utilise :
            //   - Cote CLIENT quand on recoit kMsgStartMatch (drain dans OnUpdate)
            //   - Fallback mode local (sans reseau, plus utilise normalement)
            logger.Info("[NetLobby] Push Gameplay (network)");
            if (ctx.scenes != nullptr)
            {
                ctx.scenes->Push(new GameplayScene());
            }
        }

        // ── Edition IP ───────────────────────────────────────────────────────
        void NetworkLobbyScene::IpAppendDigit(char c)
        {
            if (mIpLen >= kMaxIpLen - 1) return;
            // Accepte 0-9 et '.'
            if (!((c >= '0' && c <= '9') || c == '.')) return;
            mIp[mIpLen++] = c;
            mIp[mIpLen] = '\0';
        }
        void NetworkLobbyScene::IpBackspace()
        {
            if (mIpLen > 0)
            {
                mIp[--mIpLen] = '\0';
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // OnRender — 2 vues : ecran de choix (mView=0) ou formulaire JOIN.
        // ─────────────────────────────────────────────────────────────────────
        void NetworkLobbyScene::OnRender(AppContext& ctx)
        {
            GLRenderer2D& r = *ctx.renderer;
            FontAtlas&    f = *ctx.font;
            const int W = ctx.viewportW;
            const int H = ctx.viewportH;
            const float scale = GetUIScale(W, H);
            const float safeX = (float)ctx.safe.LeftX();
            const float safeW = (float)ctx.safe.SafeW();
            const float enterA = EaseOutCubic(mEnterAnim);

            r.Clear(theme::Dark().r / 255.0f,
                    theme::Dark().g / 255.0f,
                    theme::Dark().b / 255.0f, 1.0f);
            r.Begin(W, H);

            // Header RETOUR + titre
            mBackW = 90.0f * scale;
            mBackH = 36.0f * scale;
            mBackX = (float)ctx.safe.LeftX() + 14.0f * scale;
            mBackY = (float)ctx.safe.TopY()  + 16.0f * scale;
            r.DrawQuad       (mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 30 });
            r.DrawQuadOutline(mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 200 }, 1.5f);
            f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                               mBackX + mBackW * 0.5f,
                               mBackY + mBackH * 0.18f,
                               "RETOUR", theme::Cyan());
            f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                               (float)W * 0.5f,
                               (float)ctx.safe.TopY() + 18.0f * scale,
                               "MULTIJOUEUR RESEAU",
                               theme::White());

            // ── Status global (centre) ────────────────────────────────────
            const NetworkState st = (ctx.network != nullptr)
                ? ctx.network->State() : NetworkState::Idle;

            const char* statusText = "CHOISIS UNE OPTION";
            math::NkColor statusCol = { 255, 255, 255, 200 };
            switch (st)
            {
            case NetworkState::Idle:
                statusText = "CHOISIS UNE OPTION CI-DESSOUS";
                break;
            case NetworkState::Hosting:
                statusText = "EN ATTENTE D'UN JOUEUR...";
                statusCol  = { 255, 215, 0, 240 };
                break;
            case NetworkState::Joining:
                statusText = "CONNEXION EN COURS...";
                statusCol  = { 255, 215, 0, 240 };
                break;
            case NetworkState::Connected:
                statusText = "PEER CONNECTE ! PRET A LANCER";
                statusCol  = { 80, 255, 100, 240 };
                break;
            case NetworkState::Disconnected:
                statusText = "DECONNECTE — RETENTE OU REJOIN";
                statusCol  = { 255, 100, 100, 240 };
                break;
            }

            const float gridLeft = safeX + 24.0f * scale;
            const float availW   = safeW - 48.0f * scale;
            float y = (float)ctx.safe.TopY() + 90.0f * scale;

            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                               (float)W * 0.5f, y, statusText, statusCol);
            y += 36.0f * scale;

            // Erreur (si presente)
            if (ctx.network != nullptr && ctx.network->LastError()[0] != '\0')
            {
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   (float)W * 0.5f, y,
                                   ctx.network->LastError(),
                                   { 255, 100, 100, 220 });
                y += 20.0f * scale;
            }

            // ── Vue 0 : ecran de choix ─────────────────────────────────────
            if (mView == 0 && st != NetworkState::Connected)
            {
                // Card HEBERGER
                mHostW = availW;
                mHostH = 90.0f * scale;
                mHostX = gridLeft;
                mHostY = y;
                {
                    math::NkColor bg = { 0, 245, 255, (uint8_t)(30 * enterA) };
                    math::NkColor bd = { 0, 245, 255, (uint8_t)(220 * enterA) };
                    r.DrawQuad       (mHostX, mHostY, mHostW, mHostH, bg);
                    r.DrawQuadOutline(mHostX, mHostY, mHostW, mHostH, bd, 1.5f);
                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 mHostX + 18.0f * scale,
                                 mHostY + 16.0f * scale,
                                 "HEBERGER", theme::Cyan());
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "PORT %u — DEMANDE A L'AUTRE JOUEUR DE TE REJOINDRE",
                                  (unsigned)kDefaultPort);
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 mHostX + 18.0f * scale,
                                 mHostY + 52.0f * scale,
                                 buf, { 255, 255, 255, 180 });
                }
                y += mHostH + 16.0f * scale;

                // Card REJOINDRE
                mJoinW = availW;
                mJoinH = 90.0f * scale;
                mJoinX = gridLeft;
                mJoinY = y;
                {
                    math::NkColor bg = { 255, 107, 0, (uint8_t)(30 * enterA) };
                    math::NkColor bd = { 255, 107, 0, (uint8_t)(220 * enterA) };
                    r.DrawQuad       (mJoinX, mJoinY, mJoinW, mJoinH, bg);
                    r.DrawQuadOutline(mJoinX, mJoinY, mJoinW, mJoinH, bd, 1.5f);
                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 mJoinX + 18.0f * scale,
                                 mJoinY + 16.0f * scale,
                                 "REJOINDRE", { 255, 107, 0, 240 });
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 mJoinX + 18.0f * scale,
                                 mJoinY + 52.0f * scale,
                                 "ENTRER L'IP DE L'HOTE", { 255, 255, 255, 180 });
                }
                y += mJoinH + 16.0f * scale;
            }

            // ── Vue 1 : formulaire IP (avec champ de saisie + CONNECTER) ───
            if (mView == 1 && st != NetworkState::Connected)
            {
                // Field background
                const float fieldW = availW;
                const float fieldH = 56.0f * scale;
                r.DrawQuad       (gridLeft, y, fieldW, fieldH, { 0, 245, 255, 18 });
                r.DrawQuadOutline(gridLeft, y, fieldW, fieldH, { 0, 245, 255, 200 }, 1.5f);
                f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                             gridLeft + 12.0f * scale,
                             y + 6.0f * scale,
                             "ADRESSE IP DE L'HOTE :",
                             { 255, 255, 255, 160 });
                // IP en gros au centre du field
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   gridLeft + fieldW * 0.5f,
                                   y + 28.0f * scale,
                                   mIp, theme::White());
                y += fieldH + 12.0f * scale;

                // Hint clavier (PC)
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   (float)W * 0.5f, y,
                                   "CLAVIER : 0-9 + . + BACKSPACE POUR EDITER",
                                   { 255, 255, 255, 140 });
                y += 22.0f * scale;

                // Boutons CONNECTER + ANNULER cote a cote
                mConnectW = (fieldW - 12.0f * scale) * 0.5f;
                mConnectH = 50.0f * scale;
                mConnectX = gridLeft;
                mConnectY = y;
                r.DrawQuad       (mConnectX, mConnectY, mConnectW, mConnectH, { 0, 245, 255, 40 });
                r.DrawQuadOutline(mConnectX, mConnectY, mConnectW, mConnectH, { 0, 245, 255, 230 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mConnectX + mConnectW * 0.5f,
                                   mConnectY + 12.0f * scale,
                                   "CONNECTER", theme::Cyan());

                mCancelW = mConnectW;
                mCancelH = mConnectH;
                mCancelX = gridLeft + mConnectW + 12.0f * scale;
                mCancelY = y;
                r.DrawQuad       (mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 30 });
                r.DrawQuadOutline(mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mCancelX + mCancelW * 0.5f,
                                   mCancelY + 12.0f * scale,
                                   "ANNULER", { 255, 100, 100, 240 });
            }

            // Vue HOSTING : bouton ANNULER
            if (st == NetworkState::Hosting || st == NetworkState::Joining)
            {
                mCancelW = availW * 0.5f;
                mCancelH = 50.0f * scale;
                mCancelX = gridLeft + (availW - mCancelW) * 0.5f;
                mCancelY = y;
                r.DrawQuad       (mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 30 });
                r.DrawQuadOutline(mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mCancelX + mCancelW * 0.5f,
                                   mCancelY + 12.0f * scale,
                                   "ANNULER", { 255, 100, 100, 240 });
            }

            // Vue CONNECTED : selon le role
            //   HOST   -> Card "CONFIGURER LE MATCH" + Card "ATTENDRE" (info)
            //   CLIENT -> Card d'attente "L'HOTE CONFIGURE LE MATCH..."
            if (st == NetworkState::Connected)
            {
                const bool isHost = ctx.network != nullptr
                                 && ctx.network->Role() == NetworkRole::Host;
                const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 3.0f);

                if (isHost)
                {
                    // Card "TU ES L'HOTE — CONFIGURE LA PARTIE"
                    y += 8.0f * scale;
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       (float)W * 0.5f, y,
                                       "TU ES L'HOTE", theme::Cyan());
                    y += 32.0f * scale;
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       (float)W * 0.5f, y,
                                       "CONFIGURE LE MATCH PUIS LANCE-LE.",
                                       { 255, 255, 255, 200 });
                    y += 18.0f * scale;
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       (float)W * 0.5f, y,
                                       "LA PARTIE DEMARRERA POUR TOI ET TON ADVERSAIRE.",
                                       { 255, 255, 255, 160 });
                    y += 28.0f * scale;

                    // Bouton CONFIGURER LE MATCH (push SelectMatchConfigScene)
                    mLaunchW = availW * 0.75f;
                    mLaunchH = 64.0f * scale;
                    mLaunchX = gridLeft + (availW - mLaunchW) * 0.5f;
                    mLaunchY = y;
                    math::NkColor bg = { 0, 245, 255, (uint8_t)((40 + 40 * pulse)) };
                    math::NkColor bd = { 0, 245, 255, 240 };
                    r.DrawQuad       (mLaunchX, mLaunchY, mLaunchW, mLaunchH, bg);
                    r.DrawQuadOutline(mLaunchX, mLaunchY, mLaunchW, mLaunchH, bd, 2.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                       mLaunchX + mLaunchW * 0.5f,
                                       mLaunchY + 16.0f * scale,
                                       "CONFIGURER LE MATCH", { 255, 255, 255, 250 });
                }
                else
                {
                    // CLIENT : ecran d'attente, pas de bouton.
                    // mLaunch* mis a 0 pour qu'aucun hit-test n'accroche.
                    mLaunchW = mLaunchH = mLaunchX = mLaunchY = 0.0f;
                    y += 16.0f * scale;
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       (float)W * 0.5f, y,
                                       "TU AS REJOINT LA PARTIE", { 255, 107, 0, 240 });
                    y += 36.0f * scale;

                    // Card d'attente animee
                    const float cardW = availW * 0.8f;
                    const float cardH = 100.0f * scale;
                    const float cardX = gridLeft + (availW - cardW) * 0.5f;
                    const float cardY = y;
                    math::NkColor bg = { 255, 215, 0, (uint8_t)(30 + 30 * pulse) };
                    math::NkColor bd = { 255, 215, 0, 200 };
                    r.DrawQuad       (cardX, cardY, cardW, cardH, bg);
                    r.DrawQuadOutline(cardX, cardY, cardW, cardH, bd, 1.5f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       cardX + cardW * 0.5f,
                                       cardY + 18.0f * scale,
                                       "EN ATTENTE DE L'HOTE...", { 255, 215, 0, 240 });
                    // 3 points animes
                    static const char* dots[] = { ".", "..", "..." };
                    const int dotIdx = ((int)(mTime * 2.0f)) % 3;
                    f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                       cardX + cardW * 0.5f,
                                       cardY + 50.0f * scale,
                                       dots[dotIdx], theme::Cyan());
                }
            }

            r.End();
        }

        // ─────────────────────────────────────────────────────────────────────
        void NetworkLobbyScene::OnEvent(AppContext& ctx, NkEvent& ev)
        {
            const NetworkState st = (ctx.network != nullptr)
                ? ctx.network->State() : NetworkState::Idle;

            // Clavier (Echap + saisie IP)
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                const NkKey key = k->GetKey();
                if (key == NkKey::NK_ESCAPE) { ctx.scenes->Pop(); return; }
                if (mView == 1)
                {
                    switch (key)
                    {
                    case NkKey::NK_NUM0: IpAppendDigit('0'); return;
                    case NkKey::NK_NUM1: IpAppendDigit('1'); return;
                    case NkKey::NK_NUM2: IpAppendDigit('2'); return;
                    case NkKey::NK_NUM3: IpAppendDigit('3'); return;
                    case NkKey::NK_NUM4: IpAppendDigit('4'); return;
                    case NkKey::NK_NUM5: IpAppendDigit('5'); return;
                    case NkKey::NK_NUM6: IpAppendDigit('6'); return;
                    case NkKey::NK_NUM7: IpAppendDigit('7'); return;
                    case NkKey::NK_NUM8: IpAppendDigit('8'); return;
                    case NkKey::NK_NUM9: IpAppendDigit('9'); return;
                    case NkKey::NK_PERIOD: IpAppendDigit('.'); return;
                    case NkKey::NK_BACK:   IpBackspace(); return;
                    case NkKey::NK_ENTER:  DoJoin(ctx); return;
                    default: break;
                    }
                }
                return;
            }

            // Souris : click = activate.
            // On gate sur mInputArmDelay > 0 pour ignorer le release fantome
            // qui suit le press effectue sur la scene precedente (SelectMode).
            if (auto* mr = ev.As<NkMouseButtonReleaseEvent>())
            {
                if (mInputArmDelay > 0.0f) return;
                if (mr->GetButton() == NkMouseButton::NK_MB_LEFT)
                {
                    const float px = (float)mr->GetX();
                    const float py = (float)mr->GetY();
                    if (HitTest(px, py, mBackX, mBackY, mBackW, mBackH))
                    {
                        ctx.scenes->Pop(); return;
                    }
                    if (st == NetworkState::Connected
                     && HitTest(px, py, mLaunchX, mLaunchY, mLaunchW, mLaunchH))
                    {
                        DoLaunch(ctx); return;
                    }
                    if ((st == NetworkState::Hosting || st == NetworkState::Joining
                      || mView == 1)
                     && HitTest(px, py, mCancelX, mCancelY, mCancelW, mCancelH))
                    {
                        DoCancel(ctx); return;
                    }
                    if (mView == 0 && st == NetworkState::Idle)
                    {
                        if (HitTest(px, py, mHostX, mHostY, mHostW, mHostH))
                        {
                            DoHost(ctx); return;
                        }
                        if (HitTest(px, py, mJoinX, mJoinY, mJoinW, mJoinH))
                        {
                            mView = 1; return;
                        }
                    }
                    if (mView == 1
                     && HitTest(px, py, mConnectX, mConnectY, mConnectW, mConnectH))
                    {
                        DoJoin(ctx); return;
                    }
                }
                return;
            }

            // Touch : begin/end avec id tracking.
            // Pendant la grace period, on n'arme pas mActiveTouchId : ainsi
            // le TouchEnd "fantome" du tap qui a pousse la scene depuis
            // SelectModeScene est ignore (id != actif).
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (mInputArmDelay > 0.0f) return;
                if (tb->GetNumTouches() > 0)
                    mActiveTouchId = (long long)tb->GetTouch(0).id;
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                if (mInputArmDelay > 0.0f) return;
                if (te->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = te->GetTouch(0);
                    if ((long long)tp.id != mActiveTouchId) { mActiveTouchId = -1; return; }
                    mActiveTouchId = -1;
                    if (HitTest(tp.clientX, tp.clientY, mBackX, mBackY, mBackW, mBackH))
                    {
                        ctx.scenes->Pop(); return;
                    }
                    if (st == NetworkState::Connected
                     && HitTest(tp.clientX, tp.clientY, mLaunchX, mLaunchY, mLaunchW, mLaunchH))
                    {
                        DoLaunch(ctx); return;
                    }
                    if ((st == NetworkState::Hosting || st == NetworkState::Joining
                      || mView == 1)
                     && HitTest(tp.clientX, tp.clientY, mCancelX, mCancelY, mCancelW, mCancelH))
                    {
                        DoCancel(ctx); return;
                    }
                    if (mView == 0 && st == NetworkState::Idle)
                    {
                        if (HitTest(tp.clientX, tp.clientY, mHostX, mHostY, mHostW, mHostH))
                        {
                            DoHost(ctx); return;
                        }
                        if (HitTest(tp.clientX, tp.clientY, mJoinX, mJoinY, mJoinW, mJoinH))
                        {
                            mView = 1; return;
                        }
                    }
                    if (mView == 1
                     && HitTest(tp.clientX, tp.clientY, mConnectX, mConnectY, mConnectW, mConnectH))
                    {
                        DoJoin(ctx); return;
                    }
                }
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
