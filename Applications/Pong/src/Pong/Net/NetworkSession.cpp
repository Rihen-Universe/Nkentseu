// =============================================================================
// NetworkSession.cpp
// =============================================================================

#include "NetworkSession.h"
// On inclut SEULEMENT les sous-modules necessaires (transport + protocole).
// L'aggregat NKNetwork.h reference Replication/NkNetWorld.h qui n'existe pas
// dans le module — donc on l'evite pour ne pas casser le build.
#include "NKNetwork/Core/NkNetDefines.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKNetwork/Protocol/NkConnection.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>

namespace nkentseu
{
    namespace pong
    {

        // ── Constantes ───────────────────────────────────────────────────────
        // Pong 1v1 : un seul peer attendu cote host. On limite a 1 pour ne pas
        // accepter de spectateurs / 3e joueur.
        static constexpr uint32 kMaxClients = 1;

        // ── Lifecycle plateforme ─────────────────────────────────────────────
        void NetworkSession::PlatformInit()
        {
            const auto r = net::NkSocket::PlatformInit();
            if (r != net::NkNetResult::NK_NET_OK)
            {
                logger.Error("[Net] PlatformInit failed: {0}",
                             (int)r);
            }
            else
            {
                logger.Info("[Net] PlatformInit OK");
            }
        }
        void NetworkSession::PlatformShutdown()
        {
            net::NkSocket::PlatformShutdown();
            logger.Info("[Net] PlatformShutdown");
        }

        // ── ctor / dtor ──────────────────────────────────────────────────────
        NetworkSession::NetworkSession()
        {
            mConnMgr = new net::NkConnectionManager();
        }
        NetworkSession::~NetworkSession()
        {
            Shutdown();
            delete mConnMgr;
            mConnMgr = nullptr;
        }

        // ── StartHost / StartJoin / Shutdown ─────────────────────────────────
        bool NetworkSession::StartHost(uint16 port)
        {
            Shutdown();
            if (mConnMgr == nullptr) return false;

            // Callbacks (appeles depuis le thread reseau — on garde des etats
            // atomiques simples et on fait le polling dans Tick()).
            mConnMgr->onPeerConnected = [this](net::NkPeerId /*peer*/)
            {
                const int n = mPeerCount.fetch_add(1) + 1;
                mState.store(NetworkState::Connected);
                logger.Info("[Net] Peer connected (host). Total peers: {0}", n);
            };
            mConnMgr->onPeerDisconnected = [this](net::NkPeerId /*peer*/, const char* reason)
            {
                int n = mPeerCount.load();
                if (n > 0) { mPeerCount.fetch_sub(1); n -= 1; }
                if (n == 0) mState.store(NetworkState::Hosting);
                logger.Info("[Net] Peer disconnected (host). Reason: {0}",
                            reason ? reason : "(unknown)");
            };

            logger.Info("[Net] StartHost -> calling NkConnectionManager::StartServer(port={0}, maxClients={1})",
                        port, kMaxClients);
            const auto r = mConnMgr->StartServer(port, kMaxClients);
            logger.Info("[Net] StartServer returned code={0} (NK_NET_OK=0)", (int)r);
            if (r != net::NkNetResult::NK_NET_OK)
            {
                std::snprintf(mLastError, sizeof(mLastError),
                              "StartServer echoue (code %d) sur port %u",
                              (int)r, (unsigned)port);
                mState.store(NetworkState::Idle);
                mRole  = NetworkRole::None;
                logger.Error("[Net] {0}", mLastError);
                return false;
            }
            mState.store(NetworkState::Hosting);
            mRole  = NetworkRole::Host;
            mPeerCount.store(0);
            logger.Info("[Net] State=Hosting (waiting for peer on port {0})", port);
            mLastError[0] = '\0';
            logger.Info("[Net] Hosting on port {0}", port);
            return true;
        }

        bool NetworkSession::StartJoin(const char* ip, uint16 port)
        {
            Shutdown();
            if (mConnMgr == nullptr || ip == nullptr || ip[0] == '\0') return false;

            mConnMgr->onPeerConnected = [this](net::NkPeerId /*peer*/)
            {
                const int n = mPeerCount.fetch_add(1) + 1;
                mState.store(NetworkState::Connected);
                logger.Info("[Net] Connected to host (client). Peer count: {0}", n);
            };
            mConnMgr->onPeerDisconnected = [this](net::NkPeerId /*peer*/, const char* reason)
            {
                if (mPeerCount.load() > 0) mPeerCount.fetch_sub(1);
                mState.store(NetworkState::Disconnected);
                logger.Info("[Net] Disconnected from host. Reason: {0}",
                            reason ? reason : "(unknown)");
            };

            logger.Info("[Net] StartJoin -> parsing addr ip='{0}' port={1}", ip, port);
            const net::NkAddress addr(ip, port);
            if (!addr.IsValid())
            {
                std::snprintf(mLastError, sizeof(mLastError),
                              "Adresse IP invalide : %s", ip);
                mState.store(NetworkState::Idle);
                mRole  = NetworkRole::None;
                logger.Error("[Net] {0}", mLastError);
                return false;
            }
            logger.Info("[Net] addr.IsValid=OK, calling NkConnectionManager::Connect(...)");
            const auto r = mConnMgr->Connect(addr);
            logger.Info("[Net] Connect returned code={0} (NK_NET_OK=0)", (int)r);
            if (r != net::NkNetResult::NK_NET_OK)
            {
                std::snprintf(mLastError, sizeof(mLastError),
                              "Connect echoue (code %d) vers %s:%u",
                              (int)r, ip, (unsigned)port);
                mState.store(NetworkState::Idle);
                mRole  = NetworkRole::None;
                logger.Error("[Net] {0}", mLastError);
                return false;
            }
            mState.store(NetworkState::Joining);
            mRole  = NetworkRole::Client;
            mPeerCount.store(0);
            mJoinElapsed = 0.0f;   // reset timeout counter
            mLastError[0] = '\0';
            logger.Info("[Net] Joining {0}:{1}", ip, port);
            return true;
        }

        void NetworkSession::Shutdown()
        {
            if (mConnMgr != nullptr
             && (mState.load() != NetworkState::Idle))
            {
                mConnMgr->Shutdown();
                logger.Info("[Net] Session shutdown");
            }
            mState.store(NetworkState::Idle);
            mRole  = NetworkRole::None;
            mPeerCount.store(0);
            mJoinElapsed  = 0.0f;
            mLastError[0] = '\0';
        }

        // ── Tick ─────────────────────────────────────────────────────────────
        void NetworkSession::Tick(float dt)
        {
            const NetworkState st = mState.load();

            // Heartbeat de debug : log toutes les ~1.5s quand on attend une
            // connection ou qu'on est en train de join. Permet de voir si
            // le state machine bouge ou si tout est bloque.
            mDebugLogElapsed += dt;
            if (mDebugLogElapsed >= 1.5f)
            {
                mDebugLogElapsed = 0.0f;
                if (st == NetworkState::Hosting)
                {
                    logger.Info("[Net][HB] HOSTING port=7777 peers={0} (en attente)",
                                mPeerCount.load());
                }
                else if (st == NetworkState::Joining)
                {
                    logger.Info("[Net][HB] JOINING (elapsed={0:.1}s / timeout {1}s)",
                                mJoinElapsed, (int)kJoinTimeoutSec);
                }
                else if (st == NetworkState::Connected)
                {
                    logger.Info("[Net][HB] CONNECTED peers={0}",
                                mPeerCount.load());
                }
            }

            // Timeout cote client : si on est Joining depuis > kJoinTimeoutSec,
            // on considere que l'host ne repond pas et on bascule en
            // Disconnected avec un message clair. Evite le spinner infini.
            if (st == NetworkState::Joining)
            {
                mJoinElapsed += dt;
                if (mJoinElapsed > kJoinTimeoutSec)
                {
                    std::snprintf(mLastError, sizeof(mLastError),
                                  "Timeout : aucun hote a repondu en %ds",
                                  (int)kJoinTimeoutSec);
                    if (mConnMgr != nullptr) mConnMgr->Shutdown();
                    mState.store(NetworkState::Disconnected);
                    mPeerCount.store(0);
                    logger.Info("[Net] Join timeout — host didn't respond in {0}s",
                                (int)kJoinTimeoutSec);
                }
            }
            else
            {
                mJoinElapsed = 0.0f;
            }
        }

        // ── Envoi / Reception ───────────────────────────────────────────────
        bool NetworkSession::Broadcast(const uint8* data, uint32 size,
                                       uint8 reliable)
        {
            if (mConnMgr == nullptr) return false;
            if (mState.load() != NetworkState::Connected) return false;
            const auto ch = reliable
                ? net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
                : net::NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;
            const auto r = mConnMgr->Broadcast(data, size, ch);
            return r == net::NkNetResult::NK_NET_OK;
        }

        void NetworkSession::DrainReceived(NkVector<net::NkReceiveMsg>& out)
        {
            if (mConnMgr != nullptr) mConnMgr->DrainAll(out);
        }

    } // namespace pong
} // namespace nkentseu
