#pragma once
// =============================================================================
// NKNetwork/Transport/NkAddress.h
// =============================================================================
// Adresse réseau (IP + port) et socket brut UDP/TCP cross-platform.
// =============================================================================

#include "Core/NkNetDefines.h"

namespace nkentseu {
    namespace net {

        // =====================================================================
        // NkAddress — adresse réseau IPv4 ou IPv6
        // =====================================================================
        class NkAddress {
        public:
            enum class Family : uint8 { IPv4 = 0, IPv6 };

            NkAddress() noexcept = default;

            // ── Constructeurs ────────────────────────────────────────────────
            /**
             * @brief Construit depuis une adresse et un port.
             * @param host "192.168.1.1" ou "::1" ou "localhost"
             * @param port Numéro de port (1-65535)
             */
            NkAddress(const char* host, uint16 port) noexcept;

            /**
             * @brief Construit depuis les bytes IPv4 (a.b.c.d).
             */
            NkAddress(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port) noexcept;

            /**
             * @brief Construit depuis une sockaddr_in (Winsock/POSIX).
             */
#if defined(NK_NET_PLATFORM_WINDOWS) || defined(NK_NET_PLATFORM_POSIX)
            explicit NkAddress(const sockaddr_in& addr) noexcept;
            explicit NkAddress(const sockaddr_in6& addr) noexcept;
            void ToSockAddr(sockaddr_storage& out, socklen_t& len) const noexcept;
#endif

            // ── Factory ──────────────────────────────────────────────────────
            [[nodiscard]] static NkAddress Loopback(uint16 port,
                                                     Family f = Family::IPv4) noexcept;
            [[nodiscard]] static NkAddress Any     (uint16 port,
                                                     Family f = Family::IPv4) noexcept;
            [[nodiscard]] static NkAddress Broadcast(uint16 port) noexcept;

            /**
             * @brief Résolution DNS (bloquant).
             * @return Liste d'adresses résolues (peut en retourner plusieurs).
             */
            [[nodiscard]] static NkVector<NkAddress>
            Resolve(const char* host, uint16 port,
                    Family preferred = Family::IPv4) noexcept;

            // ── Accesseurs ───────────────────────────────────────────────────
            [[nodiscard]] uint16     Port()   const noexcept { return mPort; }
            [[nodiscard]] Family     GetFamily() const noexcept { return mFamily; }
            [[nodiscard]] bool       IsIPv4() const noexcept { return mFamily == Family::IPv4; }
            [[nodiscard]] bool       IsIPv6() const noexcept { return mFamily == Family::IPv6; }
            [[nodiscard]] bool       IsValid() const noexcept { return mPort != 0 && mValid; }
            [[nodiscard]] bool       IsLoopback() const noexcept;
            [[nodiscard]] bool       IsBroadcast() const noexcept;
            [[nodiscard]] bool       IsMulticast() const noexcept;

            /**
             * @brief Retourne la chaîne "host:port" (ex: "192.168.1.1:7777").
             */
            [[nodiscard]] NkString ToString() const noexcept;

            /**
             * @brief Retourne uniquement la partie hôte.
             */
            [[nodiscard]] NkString HostString() const noexcept;

            bool operator==(const NkAddress& o) const noexcept;
            bool operator!=(const NkAddress& o) const noexcept { return !(*this == o); }
            bool operator< (const NkAddress& o) const noexcept;

        private:
            union {
                uint8  ipv4[4]  = {};
                uint8  ipv6[16];
            } mIP;
            uint16  mPort   = 0;
            Family  mFamily = Family::IPv4;
            bool    mValid  = false;
        };

        // =====================================================================
        // NkPacket — paquet réseau (données + adresse source)
        // =====================================================================
        /**
         * @struct NkPacket
         * @brief Paquet réseau prêt à envoyer ou reçu.
         *
         * Taille max : kNkMaxPacketSize (1400 bytes, MTU-safe).
         * Le buffer est inline pour éviter toute allocation heap.
         */
        struct NkPacket {
            uint8    data[kNkMaxPacketSize] = {};
            uint32   size    = 0;       ///< Octets valides dans data
            NkAddress from;             ///< Adresse source (pour les reçus)
            uint32   seqNum  = 0;       ///< Numéro de séquence (RUDP)
            uint32   ackMask = 0;       ///< Masque ACK des 32 derniers paquets
            NkNetChannel channel = NkNetChannel::Unreliable;

            [[nodiscard]] bool IsEmpty() const noexcept { return size == 0; }
            void Clear() noexcept { size = 0; seqNum = 0; ackMask = 0; }
        };

        // =====================================================================
        // NkSocket — socket UDP ou TCP cross-platform
        // =====================================================================
        class NkSocket {
        public:
            enum class Type : uint8 { UDP, TCP };

            NkSocket()  noexcept = default;
            ~NkSocket() noexcept { Close(); }

            NkSocket(const NkSocket&) = delete;
            NkSocket& operator=(const NkSocket&) = delete;
            NkSocket(NkSocket&&) noexcept;
            NkSocket& operator=(NkSocket&&) noexcept;

            // ── Cycle de vie ─────────────────────────────────────────────────

            /**
             * @brief Crée et lie un socket sur une adresse locale.
             * @param localAddr Adresse locale. Port 0 = OS choisit.
             * @param type      UDP (jeux) ou TCP (lobby/HTTP).
             */
            [[nodiscard]] NkNetResult Create(const NkAddress& localAddr,
                                              Type type = Type::UDP) noexcept;

            void Close() noexcept;

            // ── Configuration ────────────────────────────────────────────────

            /**
             * @brief Non-bloquant : Send/Recv retournent immédiatement.
             * Toujours activer pour la game loop.
             */
            [[nodiscard]] NkNetResult SetNonBlocking(bool v) noexcept;

            /**
             * @brief Socket de broadcast (pour découverte LAN).
             */
            [[nodiscard]] NkNetResult SetBroadcast(bool v) noexcept;

            /**
             * @brief Désactive l'algorithme de Nagle (TCP seulement).
             * Indispensable pour la latence minimale en jeu.
             */
            [[nodiscard]] NkNetResult SetNoDelay(bool v) noexcept;

            [[nodiscard]] NkNetResult SetSendBufferSize(uint32 bytes) noexcept;
            [[nodiscard]] NkNetResult SetRecvBufferSize(uint32 bytes) noexcept;

            /**
             * @brief Reuse address (pour relancer rapidement un serveur).
             */
            [[nodiscard]] NkNetResult SetReuseAddr(bool v) noexcept;

            // ── UDP ──────────────────────────────────────────────────────────

            /**
             * @brief Envoie des données UDP vers une adresse distante.
             * @return NkNetResult::OK ou code d'erreur.
             */
            [[nodiscard]] NkNetResult SendTo(const void* data, uint32 size,
                                              const NkAddress& to) noexcept;

            /**
             * @brief Reçoit un datagramme UDP.
             * @param buf       Buffer de réception.
             * @param bufSize   Taille max du buffer.
             * @param outSize   Octets reçus.
             * @param outFrom   Adresse source.
             * @return OK, ou SocketError si rien à lire (non-bloquant).
             */
            [[nodiscard]] NkNetResult RecvFrom(void* buf, uint32 bufSize,
                                                uint32& outSize,
                                                NkAddress& outFrom) noexcept;

            // ── TCP ──────────────────────────────────────────────────────────
            [[nodiscard]] NkNetResult Listen(uint32 backlog = 16) noexcept;
            [[nodiscard]] NkNetResult Connect(const NkAddress& remote) noexcept;
            [[nodiscard]] NkNetResult Accept(NkSocket& outClient,
                                              NkAddress& outAddr) noexcept;
            [[nodiscard]] NkNetResult Send(const void* data, uint32 size) noexcept;
            [[nodiscard]] NkNetResult Recv(void* buf, uint32 bufSize,
                                            uint32& outSize) noexcept;

            // ── État ─────────────────────────────────────────────────────────
            [[nodiscard]] bool IsValid()    const noexcept { return mHandle != kNkInvalidSocket; }
            [[nodiscard]] Type GetType()    const noexcept { return mType; }
            [[nodiscard]] const NkAddress& GetLocalAddr() const noexcept { return mLocalAddr; }
            [[nodiscard]] int32 GetLastError() const noexcept;

            // ── Utilitaires statiques ─────────────────────────────────────────

            /**
             * @brief Initialise Winsock (Windows uniquement, no-op sur autres OS).
             * Appeler une seule fois au démarrage.
             */
            static NkNetResult PlatformInit() noexcept;
            static void        PlatformShutdown() noexcept;

            /**
             * @brief Multiplex avec select() — attend données sur N sockets.
             * @param readSockets   Sockets à surveiller en lecture.
             * @param timeoutMs     Timeout (0=retour immédiat, UINT32_MAX=infini).
             * @param outReady      Index des sockets prêts.
             */
            static NkNetResult Select(NkSpan<NkSocket*> readSockets,
                                       uint32 timeoutMs,
                                       NkVector<uint32>& outReady) noexcept;

        private:
            NkSocketHandle mHandle    = kNkInvalidSocket;
            Type           mType      = Type::UDP;
            NkAddress      mLocalAddr;
            bool           mNonBlocking = false;
        };

    } // namespace net
} // namespace nkentseu
