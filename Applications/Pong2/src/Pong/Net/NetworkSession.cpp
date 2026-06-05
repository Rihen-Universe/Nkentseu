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
#include "Pong/Net/NetProtocol.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>

namespace nkentseu
{
    namespace pong
    {

        // ── Constantes ───────────────────────────────────────────────────────
        // Phase B : on accepte N challengers au niveau transport pour que
        // l'host puisse en choisir UN seul via AcceptChallenger. Les autres
        // sont disconnectes par PktReject + Disconnect. 32 est large pour
        // un evenement / salle de classe.
        static constexpr uint32 kMaxClients = 32;

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

            // Callbacks (appeles depuis le thread reseau). Phase B : on ne
            // passe PAS en Connected des qu'un peer arrive — l'host doit
            // explicitement valider UN challenger via AcceptChallenger.
            // Tant qu'aucun choix n'est fait, on reste en Hosting.
            mConnMgr->onPeerConnected = [this](net::NkPeerId peer)
            {
                mPeerCount.fetch_add(1);
                AddChallenger(peer.value);
                logger.Info("[Net] Challenger connected (host). peerId={0} total={1}",
                            (unsigned long long)peer.value, mPeerCount.load());
            };
            mConnMgr->onPeerDisconnected = [this](net::NkPeerId peer, const char* reason)
            {
                if (mPeerCount.load() > 0) mPeerCount.fetch_sub(1);
                RemoveChallenger(peer.value);
                // Si le peer accepte se deconnecte, retour en Hosting (les
                // autres challengers eventuels restent visibles).
                if (mAcceptedPeerId == peer.value)
                {
                    mAcceptedPeerId = 0;
                    mState.store(NetworkState::Hosting);
                }
                logger.Info("[Net] Challenger disconnected (host). peerId={0} reason={1}",
                            (unsigned long long)peer.value,
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
            // Reset identite Pays/Ville-Code distante : la session reseau
            // peut etre relancee plusieurs fois sans recreer la
            // NetworkSession, l'identite du pair precedent ne doit pas fuiter.
            mPeerCountry[0] = '\0';
            mPeerCity[0]    = '\0';
            mPeerCode[0]    = '\0';
            mHelloSent      = false;
            // Reset etat Phase B : liste challengers + validation.
            {
                std::lock_guard<std::mutex> lk(mChallengersMutex);
                mChallengers.Clear();
            }
            mAcceptedPeerId  = 0;
            mValidatedByHost = false;
            mRejectedByHost  = false;
            mRejectReason[0] = '\0';
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

            // Drain interne (consomme Hello/Accept/Reject + file pour caller).
            // Fait CHAQUE FRAME independamment du state pour eviter que les
            // messages internes restent en attente. Bug fixe 2026-05-20 :
            // l'host en Hosting ne drainait pas et perdait les Hello des
            // challengers, du coup ils apparaissaient comme "HANDSHAKE EN COURS".
            DrainInternal();

            // Echange Hello : des qu'on est Connected, on envoie notre pseudo
            // Pays/Ville-Code au pair. Idempotent via mHelloSent. SendHello()
            // est un no-op tant que mLocalCountry n'a pas ete renseigne par
            // PongApp::Init -> SetLocalIdentity(). Cote HOST, SendHello sera
            // appele apres AcceptChallenger (passage en Connected).
            if (st == NetworkState::Connected)
            {
                SendHello();
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

        void NetworkSession::DrainInternal()
        {
            if (mConnMgr == nullptr) return;
            // Drain brut depuis NkConnectionManager.
            NkVector<net::NkReceiveMsg> all;
            mConnMgr->DrainAll(all);
            for (uint32 i = 0; i < all.Size(); ++i)
            {
                const auto& msg = all[i];
                if (msg.size >= sizeof(netproto::PktHello)
                 && msg.data != nullptr
                 && msg.data[0] == netproto::kMsgHello)
                {
                    netproto::PktHello pkt;
                    std::memcpy(&pkt, msg.data, sizeof(pkt));
                    // memset preventif puis copie : safe meme si l'emetteur
                    // n'a pas null-termine.
                    std::memset(mPeerCountry, 0, sizeof(mPeerCountry));
                    std::memset(mPeerCity,    0, sizeof(mPeerCity));
                    std::memset(mPeerCode,    0, sizeof(mPeerCode));
                    std::memcpy(mPeerCountry, pkt.country, sizeof(mPeerCountry) - 1);
                    std::memcpy(mPeerCity,    pkt.city,    sizeof(mPeerCity)    - 1);
                    std::memcpy(mPeerCode,    pkt.code,    sizeof(mPeerCode)    - 1);
                    logger.Info("[Net] PktHello recu du pair : {0}/{1}-{2}",
                                mPeerCountry, mPeerCity, mPeerCode);
                    // Cote HOST : associe ce Hello au peerId qui l'a envoye.
                    // UpdateChallengerIdentity est tolerant si le challenger
                    // n'est pas encore dans la liste (race condition entre
                    // callback onPeerConnected et reception PktHello).
                    if (mRole == NetworkRole::Host)
                    {
                        UpdateChallengerIdentity(msg.from.value,
                                                 pkt.country, pkt.city, pkt.code);
                    }
                    continue;
                }
                if (msg.size >= 1 && msg.data != nullptr
                 && msg.data[0] == netproto::kMsgAccept)
                {
                    mValidatedByHost = true;
                    logger.Info("[Net] PktAccept recu : valide par l'host");
                    continue;
                }
                if (msg.size >= 1 && msg.data != nullptr
                 && msg.data[0] == netproto::kMsgReject)
                {
                    mRejectedByHost = true;
                    std::snprintf(mRejectReason, sizeof(mRejectReason),
                                  "L'hote a choisi un autre joueur");
                    logger.Info("[Net] PktReject recu : refuse par l'host");
                    continue;
                }
                // Message non-interne : reserve pour le caller via DrainReceived.
                mPendingForUser.PushBack(msg);
            }
        }

        void NetworkSession::DrainReceived(NkVector<net::NkReceiveMsg>& out)
        {
            // Le drain interne est deja fait chaque frame dans Tick(). Ici on
            // se contente de transferer la file accumulee au caller et la vider.
            out.Clear();
            out.Reserve(mPendingForUser.Size());
            for (uint32 i = 0; i < mPendingForUser.Size(); ++i)
            {
                out.PushBack(mPendingForUser[i]);
            }
            mPendingForUser.Clear();
        }

        // ── Identite Pays/Ville-Code (Phase A multijoueur) ──────────────────
        void NetworkSession::SetLocalIdentity(const char* country, const char* city,
                                              const char* code) noexcept
        {
            std::memset(mLocalCountry, 0, sizeof(mLocalCountry));
            std::memset(mLocalCity,    0, sizeof(mLocalCity));
            std::memset(mLocalCode,    0, sizeof(mLocalCode));
            if (country != nullptr)
            {
                std::strncpy(mLocalCountry, country, sizeof(mLocalCountry) - 1);
            }
            if (city != nullptr)
            {
                std::strncpy(mLocalCity, city, sizeof(mLocalCity) - 1);
            }
            if (code != nullptr)
            {
                std::strncpy(mLocalCode, code, sizeof(mLocalCode) - 1);
            }
        }

        void NetworkSession::SendHello()
        {
            // Echange symetrique Phase A : HOST et CLIENT envoient leur Hello
            // des Connected. Idempotent via mHelloSent (reset au Shutdown).
            if (mHelloSent) return;
            if (mState.load() != NetworkState::Connected) return;
            if (mLocalCountry[0] == '\0') return;   // pas encore identite local

            netproto::PktHello pkt;
            std::memset(&pkt, 0, sizeof(pkt));
            pkt.type = netproto::kMsgHello;
            std::strncpy(pkt.country, mLocalCountry, sizeof(pkt.country) - 1);
            std::strncpy(pkt.city,    mLocalCity,    sizeof(pkt.city)    - 1);
            std::strncpy(pkt.code,    mLocalCode,    sizeof(pkt.code)    - 1);
            const bool ok = Broadcast(reinterpret_cast<const uint8*>(&pkt),
                                      (uint32)sizeof(pkt), /*reliable*/1);
            if (ok)
            {
                mHelloSent = true;
                logger.Info("[Net] PktHello envoye : {0}/{1}-{2}",
                            mLocalCountry, mLocalCity, mLocalCode);
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Phase B : liste challengers (cote HOST)
        // ─────────────────────────────────────────────────────────────────────
        void NetworkSession::AddChallenger(uint64 peerId) noexcept
        {
            std::lock_guard<std::mutex> lk(mChallengersMutex);
            // Idempotent : on n'ajoute pas si peerId deja present.
            for (uint32 i = 0; i < mChallengers.Size(); ++i)
            {
                if (mChallengers[i].peerId == peerId) return;
            }
            ChallengerInfo info;
            info.peerId      = peerId;
            info.hasIdentity = false;
            mChallengers.PushBack(info);
        }

        void NetworkSession::RemoveChallenger(uint64 peerId) noexcept
        {
            std::lock_guard<std::mutex> lk(mChallengersMutex);
            for (uint32 i = 0; i < mChallengers.Size(); ++i)
            {
                if (mChallengers[i].peerId == peerId)
                {
                    // Compact en place : on remplace par le dernier puis on
                    // PopBack (ordre non garanti mais c'est OK pour la UI).
                    const uint32 last = mChallengers.Size() - 1;
                    if (i != last) mChallengers[i] = mChallengers[last];
                    mChallengers.PopBack();
                    return;
                }
            }
        }

        void NetworkSession::UpdateChallengerIdentity(uint64 peerId,
                                                     const char* country,
                                                     const char* city,
                                                     const char* code) noexcept
        {
            std::lock_guard<std::mutex> lk(mChallengersMutex);
            for (uint32 i = 0; i < mChallengers.Size(); ++i)
            {
                if (mChallengers[i].peerId == peerId)
                {
                    std::memset(mChallengers[i].country, 0, sizeof(mChallengers[i].country));
                    std::memset(mChallengers[i].city,    0, sizeof(mChallengers[i].city));
                    std::memset(mChallengers[i].code,    0, sizeof(mChallengers[i].code));
                    if (country) std::strncpy(mChallengers[i].country, country, sizeof(mChallengers[i].country) - 1);
                    if (city)    std::strncpy(mChallengers[i].city,    city,    sizeof(mChallengers[i].city)    - 1);
                    if (code)    std::strncpy(mChallengers[i].code,    code,    sizeof(mChallengers[i].code)    - 1);
                    mChallengers[i].hasIdentity = true;
                    return;
                }
            }
            // Race condition possible : PktHello recu avant que le callback
            // onPeerConnected ait pousse l'entry. On cree l'entry avec
            // l'identite des maintenant ; AddChallenger sera idempotent
            // quand il sera enfin appele.
            ChallengerInfo info;
            info.peerId      = peerId;
            info.hasIdentity = true;
            if (country) std::strncpy(info.country, country, sizeof(info.country) - 1);
            if (city)    std::strncpy(info.city,    city,    sizeof(info.city)    - 1);
            if (code)    std::strncpy(info.code,    code,    sizeof(info.code)    - 1);
            mChallengers.PushBack(info);
            logger.Info("[Net] UpdateChallengerIdentity : entry creee avant onPeerConnected pour peerId={0}",
                        (unsigned long long)peerId);
        }

        void NetworkSession::GetChallengers(NkVector<ChallengerInfo>& out) const noexcept
        {
            std::lock_guard<std::mutex> lk(mChallengersMutex);
            out.Clear();
            out.Reserve(mChallengers.Size());
            for (uint32 i = 0; i < mChallengers.Size(); ++i)
            {
                out.PushBack(mChallengers[i]);
            }
        }

        int NetworkSession::ChallengerCount() const noexcept
        {
            std::lock_guard<std::mutex> lk(mChallengersMutex);
            return (int)mChallengers.Size();
        }

        void NetworkSession::AcceptChallenger(uint64 peerId) noexcept
        {
            if (mRole != NetworkRole::Host) return;
            if (mConnMgr == nullptr) return;
            // Verifie que ce peer existe dans la liste.
            bool found = false;
            {
                std::lock_guard<std::mutex> lk(mChallengersMutex);
                for (uint32 i = 0; i < mChallengers.Size(); ++i)
                {
                    if (mChallengers[i].peerId == peerId) { found = true; break; }
                }
            }
            if (!found)
            {
                logger.Warn("[Net] AcceptChallenger : peerId {0} introuvable",
                            (unsigned long long)peerId);
                return;
            }
            // 1) Envoie PktAccept au choisi.
            netproto::PktAccept ack{ netproto::kMsgAccept };
            const auto ar = mConnMgr->SendTo(net::NkPeerId{ peerId },
                                             reinterpret_cast<const uint8*>(&ack),
                                             (uint32)sizeof(ack),
                                             net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);
            logger.Info("[Net] AcceptChallenger peerId={0} -> SendTo PktAccept code={1}",
                        (unsigned long long)peerId, (int)ar);
            // 2) Envoie PktReject + Disconnect a tous les autres.
            netproto::PktReject rej{ netproto::kMsgReject };
            NkVector<uint64> toDisconnect;
            {
                std::lock_guard<std::mutex> lk(mChallengersMutex);
                for (uint32 i = 0; i < mChallengers.Size(); ++i)
                {
                    if (mChallengers[i].peerId != peerId)
                    {
                        toDisconnect.PushBack(mChallengers[i].peerId);
                    }
                }
            }
            for (uint32 i = 0; i < toDisconnect.Size(); ++i)
            {
                (void)mConnMgr->SendTo(net::NkPeerId{ toDisconnect[i] },
                                       reinterpret_cast<const uint8*>(&rej),
                                       (uint32)sizeof(rej),
                                       net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);
                mConnMgr->Disconnect(net::NkPeerId{ toDisconnect[i] },
                                     "Host a choisi un autre challenger");
            }
            // 3) Bascule l'etat en Connected (pour ce peer seulement).
            mAcceptedPeerId = peerId;
            mState.store(NetworkState::Connected);
            logger.Info("[Net] State -> Connected (challenger valide)");
        }

        void NetworkSession::RejectChallenger(uint64 peerId) noexcept
        {
            if (mRole != NetworkRole::Host) return;
            if (mConnMgr == nullptr) return;
            netproto::PktReject rej{ netproto::kMsgReject };
            (void)mConnMgr->SendTo(net::NkPeerId{ peerId },
                                   reinterpret_cast<const uint8*>(&rej),
                                   (uint32)sizeof(rej),
                                   net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);
            mConnMgr->Disconnect(net::NkPeerId{ peerId },
                                 "Refuse par l'hote");
            // Le callback onPeerDisconnected enleve l'entry de la liste.
            logger.Info("[Net] RejectChallenger peerId={0}", (unsigned long long)peerId);
        }

    } // namespace pong
} // namespace nkentseu
