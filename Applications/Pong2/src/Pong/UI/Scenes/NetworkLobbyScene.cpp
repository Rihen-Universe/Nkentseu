// =============================================================================
// NetworkLobbyScene.cpp
// =============================================================================

#include "NetworkLobbyScene.h"
#include "GameplayScene.h"
#include "SelectMatchConfigScene.h"
#include "Pong/Net/NetworkSession.h"
#include "Pong/Net/NetworkDiscovery.h"
#include "Pong/Net/NetProtocol.h"
#include "NKNetwork/Protocol/NkConnection.h"  // pour NkReceiveMsg
#include "Pong/Render/GLRenderer2D.h"
#include "Pong/Render/FontAtlas.h"
#include "Pong/UI/Theme.h"
#include "Pong/UI/SceneManager.h"
#include "Pong/UI/UIScale.h"
#include "Pong/UI/ResponsiveLayout.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkMouseEvent.h"  // pour NkMouseWheelVerticalEvent
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
            // Stoppe la decouverte LAN (beacon + scan) pour ne pas spammer
            // le reseau quand on n'est plus dans le lobby.
            if (ctx.discovery != nullptr)
            {
                ctx.discovery->StopBeacon();
                ctx.discovery->StopScan();
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
            // Demarre le beacon LAN pour que les clients du WiFi puissent
            // nous detecter par nom (Phase C). Idempotent. Le pseudo provient
            // de ctx.my* (genere une fois par PongApp::Init).
            if (ok && ctx.discovery != nullptr)
            {
                ctx.discovery->StartBeacon(ctx.myCountry, ctx.myCity,
                                           ctx.myCode, kDefaultPort);
            }
        }
        void NetworkLobbyScene::DoOpenJoin(AppContext& ctx)
        {
            // Bascule en vue "Rejoindre" + demarre le scan UDP pour decouvrir
            // les hosts du LAN. Le bouton CONNECTER + saisie IP est supprime :
            // on liste les hosts par nom et l'user tap sur un nom (cf
            // [[pong_multijoueur_lobby_pays_ville]] decision 2026-05-20).
            mView = 1;
            if (ctx.discovery != nullptr)
            {
                ctx.discovery->StartScan();
            }
            logger.Info("[NetLobby] DoOpenJoin -> scan LAN demarre");
        }
        void NetworkLobbyScene::DoJoinSlot(AppContext& ctx, int slotIdx)
        {
            if (slotIdx < 0 || slotIdx >= mHostBtnCount) return;
            if (ctx.network == nullptr) return;
            const char* ip   = mHostBtnIp[slotIdx];
            const uint16 port = mHostBtnPort[slotIdx];
            const bool ok = ctx.network->StartJoin(ip, port);
            logger.Info("[NetLobby] DoJoinSlot[{0}] {1}:{2} -> {3}",
                        slotIdx, ip, (unsigned)port, ok ? "OK" : "FAIL");
            // Une fois la connexion lancee, le scan n'est plus utile :
            // on est en train de joindre un host specifique.
            if (ok && ctx.discovery != nullptr)
            {
                ctx.discovery->StopScan();
            }
        }
        void NetworkLobbyScene::DoAcceptChallenger(AppContext& ctx, int slotIdx)
        {
            if (slotIdx < 0 || slotIdx >= mChalBtnCount) return;
            if (ctx.network == nullptr) return;
            const uint64 peerId = mChalBtnPeerId[slotIdx];
            ctx.network->AcceptChallenger(peerId);
            // Une fois accepte, on n'a plus besoin du beacon (l'host est en
            // match avec ce challenger valide ; les autres sont rejetes).
            if (ctx.discovery != nullptr)
            {
                ctx.discovery->StopBeacon();
            }
            logger.Info("[NetLobby] AcceptChallenger[{0}] peerId={1}",
                        slotIdx, (unsigned long long)peerId);
        }
        void NetworkLobbyScene::DoCancel(AppContext& ctx)
        {
            if (ctx.network != nullptr) ctx.network->Shutdown();
            // Stoppe aussi beacon + scan : on retourne au choix initial.
            if (ctx.discovery != nullptr)
            {
                ctx.discovery->StopBeacon();
                ctx.discovery->StopScan();
            }
            mView = 0;
            mScrollJoin = 0.0f;
            mScrollHost = 0.0f;
            logger.Info("[NetLobby] Cancel");
        }
        void NetworkLobbyScene::DoReturnAfterReject(AppContext& ctx)
        {
            // Apres un PktReject recu cote client, on shutdown la session
            // et on retourne au scan (vue Rejoindre) pour que l'user puisse
            // tenter un autre host.
            if (ctx.network != nullptr) ctx.network->Shutdown();
            if (ctx.discovery != nullptr) ctx.discovery->StartScan();
            mView = 0;
            mScrollJoin = 0.0f;
            logger.Info("[NetLobby] Return after reject");
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

        // ─────────────────────────────────────────────────────────────────────
        // OnRender — 2 vues : ecran de choix (mView=0) ou liste hosts (mView=1).
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

            // Layout responsive : dimensions en % viewport (2026-05-19).
            // On exprime tout en fractions de W/H avec clamps doux, pour que
            // l'UI soit lisible et bien centree sur TOUTES les resolutions
            // (mobile portrait, mobile landscape, PC 720p/1080p/4K, ultrawide).
            const float Wf = (float)W;

            // Header RETOUR + titre (responsive)
            mBackW = Pct::W(W, 0.10f,  70.0f, 130.0f);
            mBackH = Pct::H(H, 0.050f, 30.0f,  56.0f);
            mBackX = (float)ctx.safe.LeftX() + Pct::W(W, 0.015f,  8.0f, 24.0f);
            mBackY = (float)ctx.safe.TopY()  + Pct::H(H, 0.020f, 10.0f, 28.0f);
            r.DrawQuad       (mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 30 });
            r.DrawQuadOutline(mBackX, mBackY, mBackW, mBackH, { 0, 245, 255, 200 }, 1.5f);
            f.DrawStringCenteredScaled(r, FontAtlas::BodySlot, scale,
                               mBackX + mBackW * 0.5f,
                               mBackY + mBackH * 0.18f,
                               "RETOUR", theme::Cyan());
            f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                               Wf * 0.5f,
                               (float)ctx.safe.TopY() + Pct::H(H, 0.022f, 12.0f, 32.0f),
                               "MULTIJOUEUR RESEAU",
                               theme::White());

            // ── Bande identite Pays/Ville (toujours visible) ──────────────
            // Affiche notre identifiant en grand (carte de visite reseau).
            // Quand on est Connected, affiche aussi celui du pair en dessous.
            // L'identifiant est genere une fois au demarrage par PongApp.
            {
                const float idY = (float)ctx.safe.TopY()
                                + Pct::H(H, 0.072f, 38.0f, 90.0f);
                char myBuf[96];
                std::snprintf(myBuf, sizeof(myBuf), "TOI : %s/%s-%s",
                              ctx.myCountry, ctx.myCity, ctx.myCode);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   Wf * 0.5f, idY,
                                   myBuf, { 0, 245, 255, 230 });
                // Si on a un pair identifie, l'affiche sous le notre.
                if (ctx.network != nullptr && ctx.network->HasPeerIdentity())
                {
                    char peerBuf[96];
                    std::snprintf(peerBuf, sizeof(peerBuf), "VS  %s/%s-%s",
                                  ctx.network->PeerCountry(),
                                  ctx.network->PeerCity(),
                                  ctx.network->PeerCode());
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       Wf * 0.5f,
                                       idY + Pct::H(H, 0.038f, 20.0f, 44.0f),
                                       peerBuf, { 255, 107, 0, 230 });
                }
            }

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

            // Bande centrale : on cape la largeur a ~70% du viewport sur grand
            // ecran, et on garde une marge en % sur petit ecran (no pixels fixes).
            const float sideMargin = Pct::W(W, 0.030f, 14.0f, 40.0f);
            const float maxBand    = Pct::W(W, 0.70f, 360.0f, 900.0f);
            const float bandW      = math::NkMin(safeW - 2.0f * sideMargin, maxBand);
            const float gridLeft   = safeX + (safeW - bandW) * 0.5f;
            const float availW     = bandW;
            // Decale en dessous de la bande identite (TOI + VS).
            // Si pair connu : +38px ; sinon : juste la ligne TOI.
            const float idBlockExtra =
                (ctx.network != nullptr && ctx.network->HasPeerIdentity())
                ? Pct::H(H, 0.038f, 20.0f, 44.0f) : 0.0f;
            float y = (float)ctx.safe.TopY()
                    + Pct::H(H, 0.165f, 88.0f, 200.0f)
                    + idBlockExtra;

            f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                               Wf * 0.5f, y, statusText, statusCol);
            y += Pct::H(H, 0.045f, 24.0f, 52.0f);

            // Erreur (si presente)
            if (ctx.network != nullptr && ctx.network->LastError()[0] != '\0')
            {
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   Wf * 0.5f, y,
                                   ctx.network->LastError(),
                                   { 255, 100, 100, 220 });
                y += Pct::H(H, 0.026f, 14.0f, 30.0f);
            }

            // ── Vue 0 : ecran de choix ─────────────────────────────────────
            // Cards responsive : hauteur en % de H (entre 70 et 130 px), pad
            // interne en % de W. La largeur reste availW (deja capee plus haut).
            // FIX 2026-05-19 : on n'affiche les vues 0/1 QUE en etat Idle. En
            // Hosting/Joining, seule la vue ANNULER centree doit etre visible.
            if (mView == 0 && st == NetworkState::Idle)
            {
                const float cardH   = Pct::H(H, 0.115f, 70.0f, 130.0f);
                const float padX    = Pct::W(W, 0.025f, 10.0f, 28.0f);
                const float titleY  = Pct::H(H, 0.020f, 10.0f, 22.0f);
                const float subY    = Pct::H(H, 0.065f, 36.0f, 76.0f);
                const float cardGap = Pct::H(H, 0.022f, 10.0f, 24.0f);

                // Card HEBERGER
                mHostW = availW;
                mHostH = cardH;
                mHostX = gridLeft;
                mHostY = y;
                {
                    math::NkColor bg = { 0, 245, 255, (uint8_t)(30 * enterA) };
                    math::NkColor bd = { 0, 245, 255, (uint8_t)(220 * enterA) };
                    r.DrawQuad       (mHostX, mHostY, mHostW, mHostH, bg);
                    r.DrawQuadOutline(mHostX, mHostY, mHostW, mHostH, bd, 1.5f);
                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 mHostX + padX,
                                 mHostY + titleY,
                                 "HEBERGER", theme::Cyan());
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "PORT %u — DEMANDE A L'AUTRE JOUEUR DE TE REJOINDRE",
                                  (unsigned)kDefaultPort);
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 mHostX + padX,
                                 mHostY + subY,
                                 buf, { 255, 255, 255, 180 });
                }
                y += mHostH + cardGap;

                // Card REJOINDRE
                mJoinW = availW;
                mJoinH = cardH;
                mJoinX = gridLeft;
                mJoinY = y;
                {
                    math::NkColor bg = { 255, 107, 0, (uint8_t)(30 * enterA) };
                    math::NkColor bd = { 255, 107, 0, (uint8_t)(220 * enterA) };
                    r.DrawQuad       (mJoinX, mJoinY, mJoinW, mJoinH, bg);
                    r.DrawQuadOutline(mJoinX, mJoinY, mJoinW, mJoinH, bd, 1.5f);
                    f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                 mJoinX + padX,
                                 mJoinY + titleY,
                                 "REJOINDRE", { 255, 107, 0, 240 });
                    f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                 mJoinX + padX,
                                 mJoinY + subY,
                                 "ENTRER L'IP DE L'HOTE", { 255, 255, 255, 180 });
                }
                y += mJoinH + cardGap;
            }

            // ── Vue 1 : liste des hosts detectes (scan UDP LAN) ───────────
            // Plus de saisie IP : on affiche la liste des hosts visibles
            // identifies par leur Pays/Ville-Code. Tap = se connecter par
            // l'IP du beacon (cachee a l'user). Cf phase C des changes
            // 2026-05-20 ([[pong_multijoueur_internet_strategy]]).
            mHostBtnCount = 0;
            if (mView == 1 && st == NetworkState::Idle)
            {
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   Wf * 0.5f, y,
                                   "PARTIES VISIBLES SUR TON RESEAU LOCAL",
                                   { 255, 255, 255, 200 });
                y += Pct::H(H, 0.034f, 18.0f, 38.0f);

                NkVector<HostInfo> hosts;
                if (ctx.discovery != nullptr) ctx.discovery->GetHosts(hosts);

                if (hosts.Size() == 0)
                {
                    // Animation 3 points pour signaler "scan en cours".
                    const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 3.0f);
                    math::NkColor col = { 255, 215, 0,
                        (uint8_t)(160 + 80 * pulse) };
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       Wf * 0.5f, y,
                                       "RECHERCHE EN COURS...", col);
                    y += Pct::H(H, 0.040f, 22.0f, 48.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       Wf * 0.5f, y,
                                       "VERIFIE QUE TON ADVERSAIRE EST SUR LE MEME WIFI",
                                       { 255, 255, 255, 140 });
                    y += Pct::H(H, 0.040f, 22.0f, 48.0f);
                }
                else
                {
                    // Render des slots host avec scroll vertical. mScrollJoin
                    // est l'offset (>= 0) applique sur Y. On affiche tous les
                    // slots mais les hits hors zone visible sont implicitement
                    // ignores (y trop bas ne sera pas cliquable car les autres
                    // boutons recouvrent).
                    const int n = (int)hosts.Size();
                    const int nShow = (n < kMaxListSlots) ? n : kMaxListSlots;
                    const float btnH    = Pct::H(H, 0.085f, 48.0f, 92.0f);
                    const float btnGap  = Pct::H(H, 0.012f,  6.0f, 16.0f);
                    const float padX    = Pct::W(W, 0.020f, 10.0f, 22.0f);
                    const float titleDY = Pct::H(H, 0.020f, 10.0f, 22.0f);
                    const float subDY   = Pct::H(H, 0.052f, 28.0f, 60.0f);
                    // Hauteur totale de la liste (pour clamp scroll).
                    mContentHJoin = (float)nShow * (btnH + btnGap);
                    // Hauteur viewport de scroll : on reserve ~50% de H pour
                    // les cards visibles. Au-dela on scroll.
                    mViewportHJoin = Pct::H(H, 0.50f, 200.0f, 500.0f);
                    // Clamp mScrollJoin entre 0 et max overflow.
                    const float maxScrollJ =
                        (mContentHJoin > mViewportHJoin)
                        ? (mContentHJoin - mViewportHJoin) : 0.0f;
                    if (mScrollJoin < 0.0f) mScrollJoin = 0.0f;
                    if (mScrollJoin > maxScrollJ) mScrollJoin = maxScrollJ;

                    const float scrollAreaTop = y;
                    for (int i = 0; i < nShow; ++i)
                    {
                        const HostInfo& h = hosts[(uint32)i];
                        const float bx = gridLeft;
                        const float by = y - mScrollJoin;
                        const float bw = availW;
                        const float bh = btnH;
                        // Skip render si entierement hors viewport visible.
                        const float visTop = scrollAreaTop;
                        const float visBot = scrollAreaTop + mViewportHJoin;
                        if (by + bh < visTop || by > visBot)
                        {
                            mHostBtnX[i] = mHostBtnY[i] = mHostBtnW[i] = mHostBtnH[i] = 0.0f;
                            y += bh + btnGap;
                            continue;
                        }
                        math::NkColor bg = { 255, 107, 0, (uint8_t)(30 * enterA) };
                        math::NkColor bd = { 255, 107, 0, (uint8_t)(220 * enterA) };
                        r.DrawQuad       (bx, by, bw, bh, bg);
                        r.DrawQuadOutline(bx, by, bw, bh, bd, 1.5f);
                        char nameBuf[96];
                        std::snprintf(nameBuf, sizeof(nameBuf), "%s/%s-%s",
                                      h.country, h.city, h.code);
                        f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                     bx + padX, by + titleDY,
                                     nameBuf, { 255, 107, 0, 240 });
                        f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                     bx + padX, by + subDY,
                                     "TAPE POUR REJOINDRE", { 255, 255, 255, 180 });
                        // Memoise le slot pour OnEvent.
                        mHostBtnX[i] = bx;
                        mHostBtnY[i] = by;
                        mHostBtnW[i] = bw;
                        mHostBtnH[i] = bh;
                        std::strncpy(mHostBtnIp[i], h.ip, sizeof(mHostBtnIp[i]) - 1);
                        mHostBtnIp[i][sizeof(mHostBtnIp[i]) - 1] = '\0';
                        mHostBtnPort[i] = h.gamePort;
                        y += bh + btnGap;
                    }
                    mHostBtnCount = nShow;
                    // Apres la liste, on saute jusqu'a la fin de la zone visible
                    // pour que le bouton ANNULER soit toujours en bas, peu
                    // importe le scroll.
                    if (mContentHJoin > mViewportHJoin)
                    {
                        y = scrollAreaTop + mViewportHJoin;
                        // Indicateur scroll : "FAIS DEFILER POUR VOIR PLUS".
                        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                           Wf * 0.5f, y,
                                           "FAIS DEFILER (MOLETTE OU GLISSE) POUR VOIR LE RESTE",
                                           { 255, 255, 255, 130 });
                        y += Pct::H(H, 0.028f, 14.0f, 32.0f);
                    }
                }

                // Bouton ANNULER (retour vue choix).
                const float cancelBtnH = Pct::H(H, 0.075f, 38.0f, 70.0f);
                const float cancelPadT = Pct::H(H, 0.017f,  8.0f, 18.0f);
                mCancelW = availW * 0.5f;
                mCancelH = cancelBtnH;
                mCancelX = gridLeft + (availW - mCancelW) * 0.5f;
                mCancelY = y + Pct::H(H, 0.015f, 8.0f, 18.0f);
                r.DrawQuad       (mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 30 });
                r.DrawQuadOutline(mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mCancelX + mCancelW * 0.5f,
                                   mCancelY + cancelPadT,
                                   "ANNULER", { 255, 100, 100, 240 });
            }

            // ── Vue HOSTING : liste challengers + Accepter ─────────────────
            // Cote HOST en attente : on affiche tous les challengers
            // connectes au transport avec leur identite Pays/Ville-Code (recue
            // via PktHello). L'host tape sur celui qu'il veut affronter.
            // Liste scrollable au cas ou il y en a beaucoup.
            mChalBtnCount = 0;
            if (st == NetworkState::Hosting)
            {
                NkVector<ChallengerInfo> chals;
                if (ctx.network != nullptr) ctx.network->GetChallengers(chals);
                if (chals.Size() == 0)
                {
                    // Aucun challenger pour l'instant.
                    const float pulse = 0.5f + 0.5f * math::NkSin(mTime * 3.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       Wf * 0.5f, y,
                                       "AUCUN JOUEUR POUR LE MOMENT...",
                                       { 255, 215, 0,
                                         (uint8_t)(160 + 80 * pulse) });
                    y += Pct::H(H, 0.040f, 22.0f, 48.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       Wf * 0.5f, y,
                                       "TON IDENTIFIANT EST DEJA VISIBLE SUR LE RESEAU",
                                       { 255, 255, 255, 140 });
                    y += Pct::H(H, 0.038f, 20.0f, 44.0f);
                }
                else
                {
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       Wf * 0.5f, y,
                                       "CHOISIS UN CHALLENGER A AFFRONTER",
                                       { 255, 255, 255, 220 });
                    y += Pct::H(H, 0.034f, 18.0f, 38.0f);

                    const int n = (int)chals.Size();
                    const int nShow = (n < kMaxListSlots) ? n : kMaxListSlots;
                    const float btnH    = Pct::H(H, 0.095f, 56.0f, 100.0f);
                    const float btnGap  = Pct::H(H, 0.012f,  6.0f, 16.0f);
                    const float padX    = Pct::W(W, 0.020f, 10.0f, 22.0f);
                    const float titleDY = Pct::H(H, 0.018f,  8.0f, 20.0f);
                    const float subDY   = Pct::H(H, 0.058f, 32.0f, 64.0f);
                    mContentHHost = (float)nShow * (btnH + btnGap);
                    mViewportHHost = Pct::H(H, 0.45f, 180.0f, 460.0f);
                    const float maxScrollH =
                        (mContentHHost > mViewportHHost)
                        ? (mContentHHost - mViewportHHost) : 0.0f;
                    if (mScrollHost < 0.0f) mScrollHost = 0.0f;
                    if (mScrollHost > maxScrollH) mScrollHost = maxScrollH;
                    const float scrollAreaTop = y;
                    for (int i = 0; i < nShow; ++i)
                    {
                        const ChallengerInfo& c = chals[(uint32)i];
                        const float bx = gridLeft;
                        const float by = y - mScrollHost;
                        const float bw = availW;
                        const float bh = btnH;
                        const float visTop = scrollAreaTop;
                        const float visBot = scrollAreaTop + mViewportHHost;
                        if (by + bh < visTop || by > visBot)
                        {
                            mChalBtnX[i] = mChalBtnY[i] = mChalBtnW[i] = mChalBtnH[i] = 0.0f;
                            y += bh + btnGap;
                            continue;
                        }
                        math::NkColor bg = { 0, 245, 255, (uint8_t)(30 * enterA) };
                        math::NkColor bd = { 0, 245, 255, (uint8_t)(220 * enterA) };
                        r.DrawQuad       (bx, by, bw, bh, bg);
                        r.DrawQuadOutline(bx, by, bw, bh, bd, 1.5f);
                        char nameBuf[96];
                        if (c.hasIdentity)
                        {
                            std::snprintf(nameBuf, sizeof(nameBuf), "%s/%s-%s",
                                          c.country, c.city, c.code);
                        }
                        else
                        {
                            std::snprintf(nameBuf, sizeof(nameBuf),
                                          "JOUEUR (HANDSHAKE EN COURS...)");
                        }
                        f.DrawStringScaled(r, FontAtlas::SubtitleSlot, scale,
                                     bx + padX, by + titleDY,
                                     nameBuf, theme::Cyan());
                        f.DrawStringScaled(r, FontAtlas::SmallSlot, scale,
                                     bx + padX, by + subDY,
                                     "TAPE POUR ACCEPTER CE JOUEUR",
                                     { 255, 255, 255, 180 });
                        mChalBtnX[i] = bx;
                        mChalBtnY[i] = by;
                        mChalBtnW[i] = bw;
                        mChalBtnH[i] = bh;
                        mChalBtnPeerId[i] = c.peerId;
                        y += bh + btnGap;
                    }
                    mChalBtnCount = nShow;
                    if (mContentHHost > mViewportHHost)
                    {
                        y = scrollAreaTop + mViewportHHost;
                        f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                           Wf * 0.5f, y,
                                           "FAIS DEFILER POUR VOIR LES AUTRES JOUEURS",
                                           { 255, 255, 255, 130 });
                        y += Pct::H(H, 0.028f, 14.0f, 32.0f);
                    }
                }
                // Bouton ANNULER (retour mView=0).
                const float cancelBtnH = Pct::H(H, 0.075f, 38.0f, 70.0f);
                const float cancelPadT = Pct::H(H, 0.017f,  8.0f, 18.0f);
                mCancelW = availW * 0.5f;
                mCancelH = cancelBtnH;
                mCancelX = gridLeft + (availW - mCancelW) * 0.5f;
                mCancelY = y + Pct::H(H, 0.015f, 8.0f, 18.0f);
                r.DrawQuad       (mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 30 });
                r.DrawQuadOutline(mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mCancelX + mCancelW * 0.5f,
                                   mCancelY + cancelPadT,
                                   "ANNULER", { 255, 100, 100, 240 });
            }

            // ── Vue JOINING : connexion en cours (cote CLIENT) ─────────────
            if (st == NetworkState::Joining)
            {
                const float cancelBtnH = Pct::H(H, 0.075f, 38.0f, 70.0f);
                const float cancelPadT = Pct::H(H, 0.017f,  8.0f, 18.0f);
                mCancelW = availW * 0.5f;
                mCancelH = cancelBtnH;
                mCancelX = gridLeft + (availW - mCancelW) * 0.5f;
                mCancelY = y;
                r.DrawQuad       (mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 30 });
                r.DrawQuadOutline(mCancelX, mCancelY, mCancelW, mCancelH, { 255, 100, 100, 200 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mCancelX + mCancelW * 0.5f,
                                   mCancelY + cancelPadT,
                                   "ANNULER", { 255, 100, 100, 240 });
            }

            // ── Vue Rejected : cote CLIENT, l'host nous a refuse ──────────
            // Affiche un message + bouton RETOUR pour relancer le scan.
            if (ctx.network != nullptr && ctx.network->WasRejectedByHost())
            {
                y += Pct::H(H, 0.022f, 10.0f, 24.0f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   Wf * 0.5f, y,
                                   "TU AS ETE REFUSE PAR L'HOTE",
                                   { 255, 100, 100, 240 });
                y += Pct::H(H, 0.038f, 20.0f, 44.0f);
                f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                   Wf * 0.5f, y,
                                   "L'HOTE A CHOISI UN AUTRE JOUEUR. ESSAIE UN AUTRE HOTE.",
                                   { 255, 255, 255, 200 });
                y += Pct::H(H, 0.036f, 20.0f, 40.0f);
                // Bouton RETOUR (reuse mCancel slot pour hit-test).
                const float cancelBtnH = Pct::H(H, 0.085f, 44.0f, 80.0f);
                const float cancelPadT = Pct::H(H, 0.020f,  8.0f, 22.0f);
                mCancelW = availW * 0.6f;
                mCancelH = cancelBtnH;
                mCancelX = gridLeft + (availW - mCancelW) * 0.5f;
                mCancelY = y;
                r.DrawQuad       (mCancelX, mCancelY, mCancelW, mCancelH, { 0, 245, 255, 40 });
                r.DrawQuadOutline(mCancelX, mCancelY, mCancelW, mCancelH, { 0, 245, 255, 240 }, 1.5f);
                f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                   mCancelX + mCancelW * 0.5f,
                                   mCancelY + cancelPadT,
                                   "CHERCHER UN AUTRE HOTE", theme::Cyan());
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
                    // Espacements verticaux en % H (clamps doux).
                    y += Pct::H(H, 0.014f,  6.0f, 16.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       Wf * 0.5f, y,
                                       "TU ES L'HOTE", theme::Cyan());
                    y += Pct::H(H, 0.042f, 22.0f, 50.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       Wf * 0.5f, y,
                                       "CONFIGURE LE MATCH PUIS LANCE-LE.",
                                       { 255, 255, 255, 200 });
                    y += Pct::H(H, 0.024f, 12.0f, 28.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SmallSlot, scale,
                                       Wf * 0.5f, y,
                                       "LA PARTIE DEMARRERA POUR TOI ET TON ADVERSAIRE.",
                                       { 255, 255, 255, 160 });
                    y += Pct::H(H, 0.036f, 18.0f, 40.0f);

                    // Bouton CONFIGURER LE MATCH (push SelectMatchConfigScene)
                    // Largeur en % bande (75%), hauteur en % H avec clamp.
                    mLaunchW = math::NkMin(availW * 0.75f,
                                           Pct::W(W, 0.45f, 240.0f, 560.0f));
                    mLaunchH = Pct::H(H, 0.090f, 48.0f, 92.0f);
                    mLaunchX = gridLeft + (availW - mLaunchW) * 0.5f;
                    mLaunchY = y;
                    math::NkColor bg = { 0, 245, 255, (uint8_t)((40 + 40 * pulse)) };
                    math::NkColor bd = { 0, 245, 255, 240 };
                    r.DrawQuad       (mLaunchX, mLaunchY, mLaunchW, mLaunchH, bg);
                    r.DrawQuadOutline(mLaunchX, mLaunchY, mLaunchW, mLaunchH, bd, 2.0f);
                    f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                       mLaunchX + mLaunchW * 0.5f,
                                       mLaunchY + mLaunchH * 0.25f,
                                       "CONFIGURER LE MATCH", { 255, 255, 255, 250 });
                }
                else
                {
                    // CLIENT : 2 sous-etats.
                    //   - !validated : "EN ATTENTE DE VALIDATION DE L'HOTE..."
                    //   - validated  : "TU AS REJOINT LA PARTIE" + attente match
                    mLaunchW = mLaunchH = mLaunchX = mLaunchY = 0.0f;
                    const bool validated = ctx.network->IsValidatedByHost();
                    y += Pct::H(H, 0.022f, 10.0f, 24.0f);
                    if (!validated)
                    {
                        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                           Wf * 0.5f, y,
                                           "EN ATTENTE DE VALIDATION DE L'HOTE...",
                                           { 255, 215, 0, 240 });
                    }
                    else
                    {
                        f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                           Wf * 0.5f, y,
                                           "TU AS REJOINT LA PARTIE",
                                           { 255, 107, 0, 240 });
                    }
                    y += Pct::H(H, 0.048f, 24.0f, 54.0f);

                    // Card d'attente animee (largeur en % bande, hauteur en % H).
                    const float cardW = math::NkMin(availW * 0.8f,
                                                    Pct::W(W, 0.55f, 300.0f, 720.0f));
                    const float cardH = Pct::H(H, 0.135f, 80.0f, 150.0f);
                    const float cardX = gridLeft + (availW - cardW) * 0.5f;
                    const float cardY = y;
                    math::NkColor bg = { 255, 215, 0, (uint8_t)(30 + 30 * pulse) };
                    math::NkColor bd = { 255, 215, 0, 200 };
                    r.DrawQuad       (cardX, cardY, cardW, cardH, bg);
                    r.DrawQuadOutline(cardX, cardY, cardW, cardH, bd, 1.5f);
                    f.DrawStringCenteredScaled(r, FontAtlas::SubtitleSlot, scale,
                                       cardX + cardW * 0.5f,
                                       cardY + cardH * 0.20f,
                                       "EN ATTENTE DE L'HOTE...", { 255, 215, 0, 240 });
                    // 3 points animes
                    static const char* dots[] = { ".", "..", "..." };
                    const int dotIdx = ((int)(mTime * 2.0f)) % 3;
                    f.DrawStringCenteredScaled(r, FontAtlas::HeadlineSlot, scale,
                                       cardX + cardW * 0.5f,
                                       cardY + cardH * 0.55f,
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

            // Clavier : seulement Echap (retour). Plus de saisie IP (Phase C).
            if (auto* k = ev.As<NkKeyPressEvent>())
            {
                if (k->GetKey() == NkKey::NK_ESCAPE)
                {
                    ctx.scenes->Pop();
                }
                return;
            }

            // Mouse wheel : scroll vertical des listes. ~40 px par tick clavier.
            if (auto* w = ev.As<NkMouseWheelVerticalEvent>())
            {
                const float dy = (float)w->GetDeltaY();
                const float step = 40.0f;
                if (mView == 1 && st == NetworkState::Idle)
                {
                    mScrollJoin -= dy * step;
                }
                else if (st == NetworkState::Hosting)
                {
                    mScrollHost -= dy * step;
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
                    // Cas REJECTED prioritaire : bouton "CHERCHER UN AUTRE HOTE"
                    // utilise le slot mCancel. Test avant DoCancel pour eviter
                    // qu'un Cancel n'avale ce clic.
                    if (ctx.network != nullptr
                     && ctx.network->WasRejectedByHost()
                     && HitTest(px, py, mCancelX, mCancelY, mCancelW, mCancelH))
                    {
                        DoReturnAfterReject(ctx); return;
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
                            DoOpenJoin(ctx); return;
                        }
                    }
                    // Vue Rejoindre : tap sur un host de la liste = StartJoin.
                    if (mView == 1 && st == NetworkState::Idle)
                    {
                        for (int i = 0; i < mHostBtnCount; ++i)
                        {
                            if (HitTest(px, py,
                                        mHostBtnX[i], mHostBtnY[i],
                                        mHostBtnW[i], mHostBtnH[i]))
                            {
                                DoJoinSlot(ctx, i);
                                return;
                            }
                        }
                    }
                    // Vue Hosting : tap sur un challenger = AcceptChallenger.
                    if (st == NetworkState::Hosting)
                    {
                        for (int i = 0; i < mChalBtnCount; ++i)
                        {
                            if (HitTest(px, py,
                                        mChalBtnX[i], mChalBtnY[i],
                                        mChalBtnW[i], mChalBtnH[i]))
                            {
                                DoAcceptChallenger(ctx, i);
                                return;
                            }
                        }
                    }
                }
                return;
            }

            // Touch : begin / move / end avec id tracking + scroll drag.
            // Distinction tap vs drag : si mTouchTotalDeltaY > kTapMaxDeltaPx
            // au moment du End, c'est un drag (le scroll a deja ete applique
            // pendant le move) et on n'execute PAS le tap.
            if (auto* tb = ev.As<NkTouchBeginEvent>())
            {
                if (mInputArmDelay > 0.0f) return;
                if (tb->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = tb->GetTouch(0);
                    mActiveTouchId    = (long long)tp.id;
                    mTouchStartX      = tp.clientX;
                    mTouchStartY      = tp.clientY;
                    mTouchLastY       = tp.clientY;
                    mTouchTotalDeltaY = 0.0f;
                    mTouchIsDragging  = false;
                }
                return;
            }
            if (auto* tm = ev.As<NkTouchMoveEvent>())
            {
                if (tm->GetNumTouches() == 0) return;
                const NkTouchPoint& tp = tm->GetTouch(0);
                if ((long long)tp.id != mActiveTouchId) return;
                const float dyFrame = tp.clientY - mTouchLastY;
                mTouchTotalDeltaY += dyFrame;
                mTouchLastY = tp.clientY;
                // Au-dela de kTapMaxDeltaPx (vertical absolu), on bascule en
                // mode drag : le tap ne sera plus declenche au End.
                if (math::NkAbs(mTouchTotalDeltaY) > kTapMaxDeltaPx)
                {
                    mTouchIsDragging = true;
                }
                // Applique le drag au scroll de la vue active.
                if (mView == 1 && st == NetworkState::Idle)
                {
                    mScrollJoin -= dyFrame;
                }
                else if (st == NetworkState::Hosting)
                {
                    mScrollHost -= dyFrame;
                }
                return;
            }
            if (auto* te = ev.As<NkTouchEndEvent>())
            {
                if (mInputArmDelay > 0.0f) return;
                if (te->GetNumTouches() > 0)
                {
                    const NkTouchPoint& tp = te->GetTouch(0);
                    if ((long long)tp.id != mActiveTouchId) { mActiveTouchId = -1; return; }
                    const bool wasDrag = mTouchIsDragging;
                    mActiveTouchId   = -1;
                    mTouchIsDragging = false;
                    // Drag = pas de tap (l'utilisateur scrollait).
                    if (wasDrag) return;

                    if (HitTest(tp.clientX, tp.clientY, mBackX, mBackY, mBackW, mBackH))
                    {
                        ctx.scenes->Pop(); return;
                    }
                    if (ctx.network != nullptr
                     && ctx.network->WasRejectedByHost()
                     && HitTest(tp.clientX, tp.clientY, mCancelX, mCancelY, mCancelW, mCancelH))
                    {
                        DoReturnAfterReject(ctx); return;
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
                            DoOpenJoin(ctx); return;
                        }
                    }
                    if (mView == 1 && st == NetworkState::Idle)
                    {
                        for (int i = 0; i < mHostBtnCount; ++i)
                        {
                            if (HitTest(tp.clientX, tp.clientY,
                                        mHostBtnX[i], mHostBtnY[i],
                                        mHostBtnW[i], mHostBtnH[i]))
                            {
                                DoJoinSlot(ctx, i);
                                return;
                            }
                        }
                    }
                    // Vue Hosting : tap challenger = accept.
                    if (st == NetworkState::Hosting)
                    {
                        for (int i = 0; i < mChalBtnCount; ++i)
                        {
                            if (HitTest(tp.clientX, tp.clientY,
                                        mChalBtnX[i], mChalBtnY[i],
                                        mChalBtnW[i], mChalBtnH[i]))
                            {
                                DoAcceptChallenger(ctx, i);
                                return;
                            }
                        }
                    }
                }
                return;
            }
        }

    } // namespace pong
} // namespace nkentseu
