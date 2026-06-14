#pragma once
// =============================================================================
// NetworkDiscovery.h
// -----------------------------------------------------------------------------
// Decouverte LAN par broadcast UDP. Permet a un client Pong de lister les
// hosts visibles sur le meme WiFi sans saisie d'adresse IP. Cf
// [[pong_multijoueur_lobby_pays_ville]] et [[pong_multijoueur_internet_strategy]].
//
// Architecture :
//   - HOST : ouvre un socket UDP en broadcast sur kBeaconPort et emet
//     toutes les kBeaconIntervalSec un PktBeacon contenant son identite
//     Pays/Ville-Code + son gamePort d'ecoute (par defaut 7777).
//   - CLIENT (scan) : ouvre un socket UDP bind sur kBeaconPort en non-bloquant,
//     reçoit les beacons, dedoublonne par (ip, code) et expose la liste des
//     hosts visibles avec age (TTL).
//
// Cette decouverte ne marche QUE sur le meme reseau local. Pour de l'Internet
// entre 2 villes il faudra ajouter un matchmaker centralise (cf Phase D dans
// [[pong_multijoueur_internet_strategy]]).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu { namespace net {
    class NkSocket;
    struct NkAddress;
}}

namespace nkentseu
{
    namespace pong
    {

        /// Port UDP utilise pour les beacons de decouverte. Different du
        /// port game (7777) pour ne pas se marcher dessus avec le trafic
        /// gameplay.
        constexpr uint16 kBeaconPort       = 7778;
        /// Intervalle d'emission cote host (1.0s = 1 Hz).
        constexpr float  kBeaconIntervalSec = 1.0f;
        /// Au-dela de ce delai sans beacon, un host est considere offline.
        constexpr float  kHostTtlSec        = 3.5f;
        /// Magic 4 octets en debut de paquet pour ignorer les UDP non-Pong.
        constexpr uint8  kBeaconMagic[4]    = { 'P', 'O', 'N', 'G' };
        /// Version du protocole beacon (incrementer si layout change).
        constexpr uint16 kBeaconVersion     = 1;

        // ─────────────────────────────────────────────────────────────────────
        // PktBeacon — datagramme broadcast UDP emis par les hosts.
        // ─────────────────────────────────────────────────────────────────────
        #pragma pack(push, 1)
        struct PktBeacon
        {
            uint8  magic[4];     ///< Doit valoir kBeaconMagic
            uint16 version;      ///< Doit valoir kBeaconVersion
            uint16 gamePort;     ///< Port d'ecoute TCP/UDP du game (typiquement 7777)
            char   country[32];  ///< Identite host (null-term)
            char   city[32];
            char   code[16];
        };
        #pragma pack(pop)
        static_assert(sizeof(PktBeacon) == 4 + 2 + 2 + 32 + 32 + 16,
                      "PktBeacon layout cassee");

        // ─────────────────────────────────────────────────────────────────────
        // HostInfo — host detecte par le scan. Expose au UI pour affichage.
        // ─────────────────────────────────────────────────────────────────────
        struct HostInfo
        {
            char     country[32]   = { 0 };
            char     city[32]      = { 0 };
            char     code[16]      = { 0 };
            char     ip[48]        = { 0 };   ///< IP source du beacon
            uint16   gamePort      = 0;
            float    ageSec        = 0.0f;    ///< Temps depuis dernier beacon
        };

        // ─────────────────────────────────────────────────────────────────────
        // NetworkDiscovery — service standalone, possede par PongApp.
        // ─────────────────────────────────────────────────────────────────────
        class NetworkDiscovery
        {
        public:
            NetworkDiscovery();
            ~NetworkDiscovery();

            // ── Beacon (cote HOST) ───────────────────────────────────────────
            /// Demarre l'emission periodique du beacon. country/city/code
            /// identifient ce host, gamePort est annonce dans le beacon pour
            /// que les clients sachent quel port joindre. Idempotent.
            bool StartBeacon(const char* country, const char* city,
                             const char* code, uint16 gamePort) noexcept;
            void StopBeacon() noexcept;

            // ── Scan (cote CLIENT) ───────────────────────────────────────────
            /// Demarre l'ecoute des beacons. Une fois actif, GetHosts() liste
            /// les hosts detectes. Idempotent.
            bool StartScan() noexcept;
            void StopScan() noexcept;

            /// Doit etre appele chaque frame depuis PongApp::Update. Gere
            /// l'emission timer-based des beacons (host) et le drain
            /// non-bloquant des datagrammes recus (scan).
            void Tick(float dt) noexcept;

            /// Liste des hosts visibles actuellement (filtres par TTL).
            /// Re-cree a chaque appel : la copie est intentionnelle car le
            /// caller (UI) ne doit pas tenir un pointeur sur l'etat interne.
            void GetHosts(NkVector<HostInfo>& out) noexcept;

            /// Etat
            bool IsBeaconActive() const noexcept { return mBeaconSock != nullptr; }
            bool IsScanActive()   const noexcept { return mScanSock   != nullptr; }

        private:
            // ── Beacon side ──────────────────────────────────────────────────
            net::NkSocket* mBeaconSock     = nullptr;
            float          mBeaconTimer    = 0.0f;
            // Payload pre-rempli, juste re-envoye periodiquement.
            PktBeacon      mBeaconPkt      = {};

            // ── Scan side ────────────────────────────────────────────────────
            net::NkSocket* mScanSock       = nullptr;
            // Liste interne des hosts vus + leur timestamp. ageSec recalcule
            // dans Tick. On limite a 32 hosts max pour eviter une liste qui
            // explose (peu probable en LAN domestique).
            static constexpr int kMaxHosts = 32;
            HostInfo       mHosts[kMaxHosts];
            int            mHostCount = 0;

            // Ajoute ou met a jour un host dans la liste. Match par (ip, code).
            void UpdateHost(const PktBeacon& pkt, const net::NkAddress& from) noexcept;
            // Retire les hosts dont ageSec > kHostTtlSec.
            void GarbageCollectHosts() noexcept;
        };

    } // namespace pong
} // namespace nkentseu
