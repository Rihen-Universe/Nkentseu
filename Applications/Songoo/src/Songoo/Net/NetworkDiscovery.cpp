// =============================================================================
// NetworkDiscovery.cpp
// -----------------------------------------------------------------------------
// Implementation broadcast UDP + scan LAN. Voir le header pour l'archi.
// =============================================================================

#include "NetworkDiscovery.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu
{
    namespace songoo
    {

        NetworkDiscovery::NetworkDiscovery()
        {
            std::memset(mHosts, 0, sizeof(mHosts));
        }

        NetworkDiscovery::~NetworkDiscovery()
        {
            StopBeacon();
            StopScan();
        }

        // ─────────────────────────────────────────────────────────────────────
        // BEACON (cote HOST)
        // ─────────────────────────────────────────────────────────────────────
        bool NetworkDiscovery::StartBeacon(const char* country, const char* city,
                                           const char* code, uint16 gamePort) noexcept
        {
            // Idempotent : si deja actif on update juste la charge utile.
            std::memset(&mBeaconPkt, 0, sizeof(mBeaconPkt));
            std::memcpy(mBeaconPkt.magic, kBeaconMagic, 4);
            mBeaconPkt.version  = kBeaconVersion;
            mBeaconPkt.gamePort = gamePort;
            if (country) std::strncpy(mBeaconPkt.country, country, sizeof(mBeaconPkt.country) - 1);
            if (city)    std::strncpy(mBeaconPkt.city,    city,    sizeof(mBeaconPkt.city)    - 1);
            if (code)    std::strncpy(mBeaconPkt.code,    code,    sizeof(mBeaconPkt.code)    - 1);

            if (mBeaconSock != nullptr) return true;   // deja en cours

            mBeaconSock = new net::NkSocket();
            // Bind sur une socket UDP IPv4 quelconque (port 0 = laisse l'OS choisir),
            // on n'a besoin que d'envoyer. SetBroadcast active SO_BROADCAST
            // necessaire pour pouvoir SendTo vers 255.255.255.255.
            const auto rc = mBeaconSock->Create(
                net::NkAddress::Any(0), net::NkSocket::Type::NK_UDP);
            if (rc != net::NkNetResult::NK_NET_OK)
            {
                logger.Error("[Discovery] StartBeacon : Create socket FAILED ({0})", (int)rc);
                delete mBeaconSock;
                mBeaconSock = nullptr;
                return false;
            }
            (void)mBeaconSock->SetNonBlocking(true);
            const auto rb = mBeaconSock->SetBroadcast(true);
            if (rb != net::NkNetResult::NK_NET_OK)
            {
                logger.Error("[Discovery] StartBeacon : SetBroadcast FAILED ({0})", (int)rb);
                delete mBeaconSock;
                mBeaconSock = nullptr;
                return false;
            }
            mBeaconTimer = 0.0f;
            logger.Info("[Discovery] Beacon started : {0}/{1}-{2} on port {3}",
                        mBeaconPkt.country, mBeaconPkt.city, mBeaconPkt.code,
                        (unsigned)gamePort);
            return true;
        }

        void NetworkDiscovery::StopBeacon() noexcept
        {
            if (mBeaconSock != nullptr)
            {
                delete mBeaconSock;   // dtor fait Close
                mBeaconSock = nullptr;
                logger.Info("[Discovery] Beacon stopped");
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // SCAN (cote CLIENT)
        // ─────────────────────────────────────────────────────────────────────
        bool NetworkDiscovery::StartScan() noexcept
        {
            if (mScanSock != nullptr) return true;

            mScanSock = new net::NkSocket();
            // Bind sur kBeaconPort, INADDR_ANY : on reçoit les broadcasts
            // emis vers 255.255.255.255:kBeaconPort par d'autres machines du LAN.
            const auto rc = mScanSock->Create(
                net::NkAddress::Any(kBeaconPort), net::NkSocket::Type::NK_UDP);
            if (rc != net::NkNetResult::NK_NET_OK)
            {
                logger.Error("[Discovery] StartScan : Create socket FAILED ({0})", (int)rc);
                delete mScanSock;
                mScanSock = nullptr;
                return false;
            }
            (void)mScanSock->SetNonBlocking(true);
            // SetBroadcast n'est pas obligatoire pour recevoir, mais ne nuit pas.
            (void)mScanSock->SetBroadcast(true);
            mHostCount = 0;
            std::memset(mHosts, 0, sizeof(mHosts));
            logger.Info("[Discovery] Scan started on port {0}", (unsigned)kBeaconPort);
            return true;
        }

        void NetworkDiscovery::StopScan() noexcept
        {
            if (mScanSock != nullptr)
            {
                delete mScanSock;
                mScanSock = nullptr;
                mHostCount = 0;
                logger.Info("[Discovery] Scan stopped");
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // TICK : emission periodique du beacon + drain non-bloquant du scan
        // ─────────────────────────────────────────────────────────────────────
        void NetworkDiscovery::Tick(float dt) noexcept
        {
            // ── Emission beacon ──────────────────────────────────────────────
            if (mBeaconSock != nullptr)
            {
                mBeaconTimer -= dt;
                if (mBeaconTimer <= 0.0f)
                {
                    const auto dst = net::NkAddress::Broadcast(kBeaconPort);
                    const auto rs = mBeaconSock->SendTo(&mBeaconPkt,
                                                       sizeof(mBeaconPkt),
                                                       dst);
                    if (rs != net::NkNetResult::NK_NET_OK)
                    {
                        // On log occasionnellement, pas a chaque echec : un
                        // network down est commun (laptop ferme, wifi off).
                        static int errCount = 0;
                        if ((++errCount % 30) == 1)
                        {
                            logger.Warn("[Discovery] Beacon SendTo FAILED ({0}, occurrence #{1})",
                                        (int)rs, errCount);
                        }
                    }
                    mBeaconTimer = kBeaconIntervalSec;
                }
            }

            // ── Drain scan ───────────────────────────────────────────────────
            if (mScanSock != nullptr)
            {
                // Boucle : on lit tant qu'il y a des datagrammes pendants.
                // En non-bloquant, RecvFrom OK avec outSize=0 = rien a lire.
                constexpr int kMaxPerTick = 32;
                for (int i = 0; i < kMaxPerTick; ++i)
                {
                    uint8 buf[256];
                    uint32 received = 0;
                    net::NkAddress from;
                    const auto rr = mScanSock->RecvFrom(buf, sizeof(buf),
                                                       received, from);
                    if (rr != net::NkNetResult::NK_NET_OK) break;
                    if (received == 0) break;
                    // Filtre : taille + magic exact + version.
                    if (received < sizeof(PktBeacon)) continue;
                    PktBeacon pkt;
                    std::memcpy(&pkt, buf, sizeof(pkt));
                    if (std::memcmp(pkt.magic, kBeaconMagic, 4) != 0) continue;
                    if (pkt.version != kBeaconVersion) continue;
                    UpdateHost(pkt, from);
                }

                // Vieillit les hosts existants + GC les expires.
                for (int i = 0; i < mHostCount; ++i)
                {
                    mHosts[i].ageSec += dt;
                }
                GarbageCollectHosts();
            }
        }

        // ─────────────────────────────────────────────────────────────────────
        // Helpers internes
        // ─────────────────────────────────────────────────────────────────────
        void NetworkDiscovery::UpdateHost(const PktBeacon& pkt,
                                          const net::NkAddress& from) noexcept
        {
            // Cle de unicite : (ip, code). Le code 9 chiffres distingue deux
            // hosts qui auraient meme IP (rare en LAN domestique).
            const NkString fromStr = from.ToString();
            // ToString() rend "1.2.3.4:5678" — on garde juste l'IP pour ip[].
            char ipBuf[48] = { 0 };
            std::strncpy(ipBuf, fromStr.CStr(), sizeof(ipBuf) - 1);
            // Coupe au ':' pour ne garder que l'IP.
            for (int i = 0; i < (int)sizeof(ipBuf); ++i)
            {
                if (ipBuf[i] == ':') { ipBuf[i] = 0; break; }
                if (ipBuf[i] == 0)   break;
            }

            // Cherche un host existant avec meme (ip, code).
            for (int i = 0; i < mHostCount; ++i)
            {
                if (std::strcmp(mHosts[i].ip,   ipBuf)    == 0
                 && std::strcmp(mHosts[i].code, pkt.code) == 0)
                {
                    // Refresh : on remet age a 0 et on update les champs au
                    // cas ou le host aurait change de pseudo (peu probable
                    // dans une meme session, mais robuste).
                    mHosts[i].ageSec = 0.0f;
                    std::strncpy(mHosts[i].country, pkt.country, sizeof(mHosts[i].country) - 1);
                    std::strncpy(mHosts[i].city,    pkt.city,    sizeof(mHosts[i].city)    - 1);
                    mHosts[i].gamePort = pkt.gamePort;
                    return;
                }
            }
            // Nouveau host : ajoute si on a de la place.
            if (mHostCount >= kMaxHosts) return;
            HostInfo& h = mHosts[mHostCount++];
            std::memset(&h, 0, sizeof(h));
            std::strncpy(h.country, pkt.country, sizeof(h.country) - 1);
            std::strncpy(h.city,    pkt.city,    sizeof(h.city)    - 1);
            std::strncpy(h.code,    pkt.code,    sizeof(h.code)    - 1);
            std::strncpy(h.ip,      ipBuf,       sizeof(h.ip)      - 1);
            h.gamePort = pkt.gamePort;
            h.ageSec   = 0.0f;
            logger.Info("[Discovery] Nouveau host : {0}/{1}-{2} @ {3}:{4}",
                        h.country, h.city, h.code, h.ip, (unsigned)h.gamePort);
        }

        void NetworkDiscovery::GarbageCollectHosts() noexcept
        {
            // Compact en place : on retire les hosts dont age > TTL.
            int dst = 0;
            for (int src = 0; src < mHostCount; ++src)
            {
                if (mHosts[src].ageSec <= kHostTtlSec)
                {
                    if (dst != src) mHosts[dst] = mHosts[src];
                    ++dst;
                }
                else
                {
                    logger.Info("[Discovery] Host expire : {0}/{1}-{2}",
                                mHosts[src].country, mHosts[src].city,
                                mHosts[src].code);
                }
            }
            mHostCount = dst;
        }

        void NetworkDiscovery::GetHosts(NkVector<HostInfo>& out) noexcept
        {
            out.Clear();
            out.Reserve((uint32)mHostCount);
            for (int i = 0; i < mHostCount; ++i)
            {
                out.PushBack(mHosts[i]);
            }
        }

    } // namespace songoo
} // namespace nkentseu
