#pragma once
// =============================================================================
// NKNetwork/NkNetDefines.h
// =============================================================================
// Types fondamentaux, macros et détection de plateforme pour NKNetwork.
//
// DÉPENDANCES :
//   NKCore/NkTypes.h      — types de base (uint8, uint32, float32...)
//   NKContainers/*        — NkVector, NkString, NkFunction
//   NKThreading/*         — NkMutex, NkThread, NkAtomic
//   NKLogger/NkLog.h      — logging
//
// PORTABILITÉ :
//   Windows  → Winsock2 (ws2_32)
//   Linux    → POSIX (sys/socket.h, netinet/in.h)
//   macOS    → POSIX (même que Linux)
//   Android  → POSIX (même que Linux)
//   iOS      → POSIX (même que macOS)
//   Wasm     → WebSocket via Emscripten (UDP non disponible)
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKCore/NkAtomic.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThread.h"
#include <cstring>
#include <cstdint>

// ── Détection de plateforme ───────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
#   define NK_NET_PLATFORM_WINDOWS 1
#   define WIN32_LEAN_AND_MEAN
#   define NOMINMAX
#   include <winsock2.h>
#   include <ws2tcpip.h>
    using NkSocketHandle = SOCKET;
    static constexpr NkSocketHandle kNkInvalidSocket = INVALID_SOCKET;
#   define NK_NET_CLOSE_SOCKET(s) closesocket(s)
#   define NK_NET_SOCKET_ERROR    SOCKET_ERROR
#   pragma comment(lib, "ws2_32.lib")
#elif defined(__EMSCRIPTEN__)
#   define NK_NET_PLATFORM_WEB 1
    // Wasm : pas de UDP brut, WebSocket uniquement via Emscripten
#   include <emscripten/websocket.h>
    using NkSocketHandle = int;
    static constexpr NkSocketHandle kNkInvalidSocket = -1;
#   define NK_NET_CLOSE_SOCKET(s) emscripten_websocket_close(s, 1000, "")
#else
    // Linux, macOS, Android, iOS
#   define NK_NET_PLATFORM_POSIX 1
#   include <sys/socket.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <netinet/tcp.h>
#   include <arpa/inet.h>
#   include <unistd.h>
#   include <fcntl.h>
#   include <netdb.h>
#   include <errno.h>
    using NkSocketHandle = int;
    static constexpr NkSocketHandle kNkInvalidSocket = -1;
#   define NK_NET_CLOSE_SOCKET(s) ::close(s)
#   define NK_NET_SOCKET_ERROR    -1
#endif

// ── Namespace & version ───────────────────────────────────────────────────────
#define NKNET_VERSION_MAJOR 1
#define NKNET_VERSION_MINOR 0
#define NKNET_VERSION_PATCH 0

// ── Limites du système ────────────────────────────────────────────────────────
static constexpr nkentseu::uint32 kNkMaxConnections     = 256u;
static constexpr nkentseu::uint32 kNkMaxPacketSize      = 1400u;  ///< MTU-safe UDP
static constexpr nkentseu::uint32 kNkMaxPayloadSize     = 1380u;  ///< Après headers
static constexpr nkentseu::uint32 kNkSendBufferSize     = 512u * 1024u;
static constexpr nkentseu::uint32 kNkRecvBufferSize     = 512u * 1024u;
static constexpr nkentseu::uint32 kNkMaxChannels        = 8u;
static constexpr nkentseu::uint32 kNkHeartbeatIntervalMs = 250u;
static constexpr nkentseu::uint32 kNkTimeoutMs          = 10000u;  ///< 10s sans heartbeat = déconnexion
static constexpr nkentseu::uint32 kNkMaxRetransmits     = 5u;
static constexpr nkentseu::uint32 kNkWindowSize         = 64u;     ///< Fenêtre RUDP (séquences)
static constexpr nkentseu::uint32 kNkMaxFragments       = 16u;
static constexpr nkentseu::uint32 kNkReplicationHistorySize = 60u; ///< Frames d'historique (rollback)

// ── Macros de log réseau ──────────────────────────────────────────────────────
#define NK_NET_LOG_INFO(fmt, ...)  logger.Infof("[NKNet] " fmt "\n", ##__VA_ARGS__)
#define NK_NET_LOG_WARN(fmt, ...)  logger.Warnf("[NKNet] " fmt "\n", ##__VA_ARGS__)
#define NK_NET_LOG_ERROR(fmt, ...) logger.Errorf("[NKNet] " fmt "\n", ##__VA_ARGS__)
#define NK_NET_LOG_DEBUG(fmt, ...) logger.Debugf("[NKNet] " fmt "\n", ##__VA_ARGS__)

#define NK_NET_ASSERT(cond, msg) \
    do { if (!(cond)) { NK_NET_LOG_ERROR("ASSERT FAILED: %s (%s:%d)", msg, __FILE__, __LINE__); } } while(0)

namespace nkentseu {
    namespace net {

        using uint8   = nkentseu::uint8;
        using uint16  = nkentseu::uint16;
        using uint32  = nkentseu::uint32;
        using uint64  = nkentseu::uint64;
        using int8    = nkentseu::int8;
        using int16   = nkentseu::int16;
        using int32   = nkentseu::int32;
        using int64   = nkentseu::int64;
        using float32 = nkentseu::float32;
        using float64 = nkentseu::float64;
        using usize   = nkentseu::usize;

        // =====================================================================
        // NkNetResult — code de retour uniforme
        // =====================================================================
        enum class NkNetResult : uint8 {
            OK = 0,
            InvalidArg,
            NotConnected,
            AlreadyConnected,
            ConnectionRefused,
            Timeout,
            PacketTooLarge,
            SocketError,
            BufferFull,
            NotInitialized,
            PlatformUnsupported,
            AuthFailed,
            Banned,
            Unknown,
        };

        [[nodiscard]] inline const char* NkNetResultStr(NkNetResult r) noexcept {
            switch (r) {
                case NkNetResult::OK:                  return "OK";
                case NkNetResult::InvalidArg:          return "Invalid argument";
                case NkNetResult::NotConnected:        return "Not connected";
                case NkNetResult::AlreadyConnected:    return "Already connected";
                case NkNetResult::ConnectionRefused:   return "Connection refused";
                case NkNetResult::Timeout:             return "Timeout";
                case NkNetResult::PacketTooLarge:      return "Packet too large";
                case NkNetResult::SocketError:         return "Socket error";
                case NkNetResult::BufferFull:          return "Buffer full";
                case NkNetResult::NotInitialized:      return "Not initialized";
                case NkNetResult::PlatformUnsupported: return "Platform unsupported";
                case NkNetResult::AuthFailed:          return "Auth failed";
                case NkNetResult::Banned:              return "Banned";
                default:                               return "Unknown";
            }
        }

        // =====================================================================
        // NkPeerId — identifiant de pair réseau (unique par session)
        // =====================================================================
        struct NkPeerId {
            uint64 value = 0;

            [[nodiscard]] bool IsValid()   const noexcept { return value != 0; }
            [[nodiscard]] bool IsServer()  const noexcept { return value == 1; }
            [[nodiscard]] static NkPeerId Invalid()   noexcept { return {0}; }
            [[nodiscard]] static NkPeerId Server()    noexcept { return {1}; }
            [[nodiscard]] static NkPeerId Generate()  noexcept;   // impl dans .cpp

            bool operator==(const NkPeerId& o) const noexcept { return value == o.value; }
            bool operator!=(const NkPeerId& o) const noexcept { return value != o.value; }
            bool operator< (const NkPeerId& o) const noexcept { return value <  o.value; }
        };

        // =====================================================================
        // NkNetId — identifiant réseau d'une entité répliquée
        // =====================================================================
        /**
         * @struct NkNetId
         * @brief Identifiant réseau stable d'une entité répliquée.
         *
         * DIFFÉRENCE avec NkEntityId (local) :
         *   NkEntityId = index local + génération, unique dans un monde ECS
         *   NkNetId    = identifiant global de session, même sur tous les pairs
         *
         * FORMAT : uint32 counter + uint8 owner (peerId simplifié)
         *   → 4 milliards d'entités par session, 255 propriétaires max
         */
        struct NkNetId {
            uint32 id    = 0;      ///< Compteur global d'entités réseau
            uint8  owner = 0;      ///< PeerId simplifié du propriétaire (0=serveur)

            [[nodiscard]] bool IsValid() const noexcept { return id != 0; }
            [[nodiscard]] static NkNetId Invalid() noexcept { return {0, 0}; }

            [[nodiscard]] uint64 Pack() const noexcept {
                return (static_cast<uint64>(owner) << 32u) | id;
            }
            [[nodiscard]] static NkNetId Unpack(uint64 v) noexcept {
                return { static_cast<uint32>(v & 0xFFFFFFFFu),
                         static_cast<uint8>(v >> 32u) };
            }

            bool operator==(const NkNetId& o) const noexcept {
                return id == o.id && owner == o.owner;
            }
            bool operator!=(const NkNetId& o) const noexcept { return !(*this == o); }
        };

        // =====================================================================
        // NkTimestamp — timestamp réseau précis (ms depuis epoch session)
        // =====================================================================
        using NkTimestampMs = uint64;

        [[nodiscard]] NkTimestampMs NkNetNowMs() noexcept;  // impl .cpp

        // =====================================================================
        // NkNetChannel — canaux de communication avec garanties différentes
        // =====================================================================
        /**
         * @enum NkNetChannel
         * @brief Canal de communication avec niveau de fiabilité configurable.
         *
         * Inspiré de l'approche ENet / GameNetworkingSockets :
         *   UNRELIABLE      → UDP pur, perte ok (positions, animations)
         *   RELIABLE_ORD    → garantit ordre + livraison (chat, événements)
         *   RELIABLE_UNORD  → garantit livraison sans ordre (fichiers)
         *   SEQUENCED       → séquencé, les plus anciens ignorés (état continu)
         *   SYSTEM          → canal interne (handshake, heartbeat, disconnect)
         */
        enum class NkNetChannel : uint8 {
            Unreliable     = 0,  ///< Position, input, animation — latence min
            ReliableOrdered,     ///< Chat, événements — fiable + ordonné
            ReliableUnordered,   ///< Gros transferts — fiable, ordre non garanti
            Sequenced,           ///< États continus — seul le plus récent compte
            System,              ///< Réservé moteur (handshake, heartbeat)
        };

        // =====================================================================
        // Byte order helpers
        // =====================================================================
        inline uint16 NkHToN16(uint16 v) noexcept { return htons(v); }
        inline uint16 NkNToH16(uint16 v) noexcept { return ntohs(v); }
        inline uint32 NkHToN32(uint32 v) noexcept { return htonl(v); }
        inline uint32 NkNToH32(uint32 v) noexcept { return ntohl(v); }
        inline uint64 NkHToN64(uint64 v) noexcept {
            return ((uint64)NkHToN32(v & 0xFFFFFFFF) << 32) | NkHToN32(v >> 32);
        }
        inline uint64 NkNToH64(uint64 v) noexcept { return NkHToN64(v); }

    } // namespace net
} // namespace nkentseu
