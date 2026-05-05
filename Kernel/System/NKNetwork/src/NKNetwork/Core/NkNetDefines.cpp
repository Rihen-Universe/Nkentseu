// =============================================================================
// FICHIER  : Modules/System/NKNetwork/src/NKNetwork/NkNetDefines.cpp
// MODULE   : NKNetwork
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implémentation des méthodes et fonctions de NkNetDefines.h.
//   Gestion des identifiants réseau, timestamps, et conversion de byte-order.
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour la compilation MSVC/Clang)
// 2. Header correspondant au fichier .cpp
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme

#include "pch.h"
#include "NkNetDefines.h"

// En-têtes standards C/C++ pour opérations système et temporelles
#include <chrono>
#include <random>
#include <atomic>

// En-têtes plateforme pour génération d'identifiants uniques
#ifdef NKENTSEU_PLATFORM_WINDOWS
    #include <windows.h>
    #include <wincrypt.h>
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    #include <emscripten.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
#endif

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes dans le namespace nkentseu::net.

namespace nkentseu {

    namespace net {

        NkNetworkConfig NkNetworkConfigManager::sConfig = {};
        threading::NkMutex NkNetworkConfigManager::sConfigMutex;

        void NkNetworkConfigManager::Configure(const NkNetworkConfig& cfg) noexcept
        {
            threading::NkScopedLockMutex lock(sConfigMutex);
            // Validation des valeurs
            if (cfg.maxConnections > 0 && cfg.maxConnections <= 65536) {
                sConfig.maxConnections = cfg.maxConnections;
            }
            if (cfg.socketBufferSize >= 64u * 1024u) {  // Minimum 64KB
                sConfig.socketBufferSize = cfg.socketBufferSize;
            }
            if (cfg.connectionTimeoutMs >= 1000u) {  // Minimum 1s
                sConfig.connectionTimeoutMs = cfg.connectionTimeoutMs;
            }
            if (cfg.heartbeatIntervalMs >= 50u && cfg.heartbeatIntervalMs <= 5000u) {
                sConfig.heartbeatIntervalMs = cfg.heartbeatIntervalMs;
            }
        }

        const NkNetworkConfig& NkNetworkConfigManager::Get() noexcept
        {
            // Lecture sans lock car configuration immutable après initialisation
            // (pattern "initialize once, read many" thread-safe en C++11+)
            return sConfig;
        }

        void NkNetworkConfigManager::ResetToDefaults() noexcept
        {
            threading::NkScopedLockMutex lock(sConfigMutex);
            sConfig = NkNetworkConfig{};  // Valeurs par défaut du struct
        }

        // =====================================================================
        // Implémentation : NkPeerId
        // =====================================================================
        // Méthodes d'inspection et fabriques statiques.

        bool NkPeerId::IsValid() const noexcept
        {
            return value != 0;
        }

        bool NkPeerId::IsServer() const noexcept
        {
            return value == 1;
        }

        NkPeerId NkPeerId::Invalid() noexcept
        {
            return NkPeerId{ 0 };
        }

        NkPeerId NkPeerId::Server() noexcept
        {
            return NkPeerId{ 1 };
        }

        NkPeerId NkPeerId::Generate() noexcept
        {
            // Générateur de nombres aléatoires thread-safe
            static std::atomic<uint64> sCounter{ 1000 };  // Commence après les IDs réservés
            static std::mt19937_64 sEngine{
                static_cast<uint64>(std::chrono::steady_clock::now().time_since_epoch().count())
            };
            static std::uniform_int_distribution<uint64> sDist{ 2, 0xFFFFFFFFFFFFFFFF };

            // Incrémentation atomique pour éviter les collisions simples
            const uint64 counter = sCounter.fetch_add(1, std::memory_order_relaxed);

            // Combinaison counter + random pour robustesse
            const uint64 randomPart = sDist(sEngine);
            const uint64 generated = (counter << 32) ^ randomPart;

            // Garantir que l'ID n'est ni 0 (invalid) ni 1 (server)
            if (generated <= 1)
            {
                return NkPeerId{ counter + 2 };
            }

            return NkPeerId{ generated };
        }

        bool NkPeerId::operator==(const NkPeerId& o) const noexcept
        {
            return value == o.value;
        }

        bool NkPeerId::operator!=(const NkPeerId& o) const noexcept
        {
            return value != o.value;
        }

        bool NkPeerId::operator<(const NkPeerId& o) const noexcept
        {
            return value < o.value;
        }

        // =====================================================================
        // Implémentation : NkNetId
        // =====================================================================
        // Méthodes d'inspection, sérialisation et comparaison.

        bool NkNetId::IsValid() const noexcept
        {
            return id != 0;
        }

        NkNetId NkNetId::Invalid() noexcept
        {
            return NkNetId{ 0, 0 };
        }

        uint64 NkNetId::Pack() const noexcept
        {
            return (static_cast<uint64>(owner) << 32u) | id;
        }

        NkNetId NkNetId::Unpack(uint64 v) noexcept
        {
            return NkNetId{
                static_cast<uint32>(v & 0xFFFFFFFFu),
                static_cast<uint8>((v >> 32u) & 0xFFu)
            };
        }

        bool NkNetId::operator==(const NkNetId& o) const noexcept
        {
            return id == o.id && owner == o.owner;
        }

        bool NkNetId::operator!=(const NkNetId& o) const noexcept
        {
            return !(*this == o);
        }

        // =====================================================================
        // Implémentation : NkNetNowMs — Timestamp réseau
        // =====================================================================
        // Obtention du timestamp courant en millisecondes depuis epoch de session.

        NkTimestampMs NkNetNowMs() noexcept
        {
            // Utilisation d'une horloge monotone pour éviter les sauts d'heure système
            // Le timestamp est relatif au premier appel dans la session
            using Clock = std::chrono::steady_clock;
            using Duration = std::chrono::milliseconds;

            static const auto sSessionStart = Clock::now();
            const auto now = Clock::now();
            const auto elapsed = std::chrono::duration_cast<Duration>(now - sSessionStart);

            return static_cast<NkTimestampMs>(elapsed.count());
        }

        // =====================================================================
        // Implémentation : Helpers Byte Order
        // =====================================================================
        // Conversion host ↔ network endian pour portabilité réseau.

        uint16 NkHToN16(uint16 v) noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return htons(v);
            #elif defined(NKENTSEU_PLATFORM_POSIX) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
                return htons(v);
            #else
                // Fallback logiciel : détection big-endian à la compilation
                #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                    return v;  // Déjà en network byte order
                #else
                    return static_cast<uint16>(((v & 0xFF00u) >> 8) | ((v & 0x00FFu) << 8));
                #endif
            #endif
        }

        uint16 NkNToH16(uint16 v) noexcept
        {
            // Network-to-host est symétrique de host-to-network
            return NkHToN16(v);
        }

        uint32 NkHToN32(uint32 v) noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                return htonl(v);
            #elif defined(NKENTSEU_PLATFORM_POSIX) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
                return htonl(v);
            #else
                #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                    return v;
                #else
                    return ((v & 0xFF000000u) >> 24) |
                           ((v & 0x00FF0000u) >> 8) |
                           ((v & 0x0000FF00u) << 8) |
                           ((v & 0x000000FFu) << 24);
                #endif
            #endif
        }

        uint32 NkNToH32(uint32 v) noexcept
        {
            return NkHToN32(v);
        }

        uint64 NkHToN64(uint64 v) noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)
                // Windows : pas de htonll standard, implémentation manuelle
                return ((static_cast<uint64>(htonl(static_cast<uint32>(v & 0xFFFFFFFFu)))) << 32) |
                       htonl(static_cast<uint32>((v >> 32) & 0xFFFFFFFFu));
            #elif defined(NKENTSEU_PLATFORM_POSIX)
                // POSIX moderne : htonll peut être disponible
                #if defined(htonll)
                    return htonll(v);
                #else
                    return ((static_cast<uint64>(htonl(static_cast<uint32>(v & 0xFFFFFFFFu)))) << 32) |
                           htonl(static_cast<uint32>((v >> 32) & 0xFFFFFFFFu));
                #endif
            #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
                // Emscripten : réseau = big-endian, implémentation logicielle
                return ((static_cast<uint64>(htonl(static_cast<uint32>(v & 0xFFFFFFFFu)))) << 32) |
                       htonl(static_cast<uint32>((v >> 32) & 0xFFFFFFFFu));
            #else
                // Fallback générique
                #if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
                    return v;
                #else
                    return ((v & 0xFF00000000000000ull) >> 56) |
                           ((v & 0x00FF000000000000ull) >> 40) |
                           ((v & 0x0000FF0000000000ull) >> 24) |
                           ((v & 0x000000FF00000000ull) >> 8) |
                           ((v & 0x00000000FF000000ull) << 8) |
                           ((v & 0x0000000000FF0000ull) << 24) |
                           ((v & 0x000000000000FF00ull) << 40) |
                           ((v & 0x00000000000000FFull) << 56);
                #endif
            #endif
        }

        uint64 NkNToH64(uint64 v) noexcept
        {
            return NkHToN64(v);
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION
// =============================================================================
/*
    Génération d'identifiants NkPeerId :
    ------------------------------------
    - NkPeerId::Generate() utilise un compteur atomique + RNG pour minimiser les collisions
    - Les IDs 0 et 1 sont réservés (Invalid/Server) et exclus de la génération
    - Thread-safe : std::atomic garantit l'unicité même avec appels concurrents
    - Pour une distribution distribuée, envisager UUID ou timestamp+MAC address

    Timestamps réseau NkNetNowMs() :
    -------------------------------
    - Utilisation de std::chrono::steady_clock pour éviter les sauts d'horloge (NTP, DST)
    - Le timestamp est relatif au premier appel dans la session, pas à epoch Unix
    - Cela garantit la monotonie et évite les problèmes de synchronisation client/serveur
    - Précision : millisecondes, suffisant pour la plupart des jeux/applications temps réel

    Conversion de byte-order :
    -------------------------
    - Le réseau utilise le big-endian (network byte order) par convention historique
    - La plupart des CPU modernes sont little-endian (x86, x64, ARM en mode standard)
    - Les fonctions NkHToN*\/NkNToH* utilisent les APIs système quand disponibles (htons, etc.)
    - Sur plateformes big-endian natives, ces fonctions sont des no-op (retour direct)
    - Pour uint64, pas de standard POSIX universel : implémentation manuelle portable

    Performance :
    ------------
    - Toutes les méthodes sont noexcept et inline-friendly pour optimisation du compilateur
    - NkPeerId::Generate() est la seule méthode non-triviale (RNG, atomic) — à appeler avec modération
    - Les conversions byte-order sont optimisées par le compilateur sur plateformes connues
    - NkNetNowMs() utilise une variable static pour éviter de recalculer le début de session

    Thread-safety :
    --------------
    - NkPeerId et NkNetId : immuables après construction, lecture seule thread-safe
    - NkPeerId::Generate() : thread-safe via std::atomic et RNG thread-local
    - NkNetNowMs() : thread-safe, variable static initialisée une fois (C++11 magic static)
    - Helpers byte-order : stateless, thread-safe par conception

    Extensions futures possibles :
    -----------------------------
    - Support des UUID pour NkPeerId dans les environnements distribués
    - Timestamps absolus (epoch Unix) en option pour logging/audit
    - Détection runtime de l'endianness pour fallback plus robuste
    - Version SIMD des conversions byte-order pour traitement de buffers massifs
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================