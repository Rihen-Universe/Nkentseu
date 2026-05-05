// =============================================================================
// NKNetwork/Transport/NkSocket.cpp
// =============================================================================
// DESCRIPTION :
//   Implémentation des méthodes de NkAddress et NkSocket.
//   Gestion cross-platform des sockets UDP/TCP et des adresses réseau.
//
// NOTES D'IMPLÉMENTATION :
//   - Toutes les méthodes publiques retournent NkNetResult pour cohérence API
//   - Gestion d'erreur unifiée via macros NK_NET_LOG_* et codes plateforme
//   - Aucune allocation heap dans les chemins critiques (performance)
//   - Respect strict du RAII pour gestion automatique des ressources
//
// DÉPENDANCES INTERNES :
//   - NkSocket.h : Déclarations des classes implémentées
//   - NkNetDefines.h : Constantes et types fondamentaux réseau
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

// -------------------------------------------------------------------------
// SECTION 1 : INCLUSIONS (ordre strict requis)
// -------------------------------------------------------------------------
// 1. Precompiled header en premier (obligatoire pour MSVC/Clang avec PCH)
// 2. Header correspondant au fichier .cpp (vérification de cohérence)
// 3. Headers du projet NKEntseu
// 4. Headers système conditionnels selon la plateforme cible

#include "pch.h"
#include "NKNetwork/Transport/NkSocket.h"

// En-têtes standards C/C++ pour opérations bas-niveau
#include <cstring>
#include <cstdio>

// En-têtes système pour détection d'erreur et gestion socket
#if defined(NKENTSEU_PLATFORM_WINDOWS)

    // Headers Winsock2 déjà inclus via NkSocket.h — ré-inclusion guardée par pragma once
    // Fonctions utilitaires pour gestion d'erreur Windows

    static int NkGetLastSocketError() noexcept
    {
        return ::WSAGetLastError();
    }

    static bool NkIsWouldBlockError(int errorCode) noexcept
    {
        return errorCode == WSAEWOULDBLOCK;
    }

#elif defined(NKENTSEU_PLATFORM_POSIX)

    // Headers POSIX déjà inclus via NkSocket.h
    // Fonctions utilitaires pour gestion d'erreur POSIX

    static int NkGetLastSocketError() noexcept
    {
        return errno;
    }

    static bool NkIsWouldBlockError(int errorCode) noexcept
    {
        return errorCode == EAGAIN || errorCode == EWOULDBLOCK;
    }

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

    // WebAssembly — gestion d'erreur simplifiée
    static int NkGetLastSocketError() noexcept
    {
        return -1;  // Pas de code d'erreur standardisé pour Emscripten WebSocket
    }

    static bool NkIsWouldBlockError(int) noexcept
    {
        return false;  // Modèle async Emscripten — pas de concept "would block"
    }

#endif // Fin de la détection de plateforme

// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes dans le namespace nkentseu::net.

namespace nkentseu {

    namespace net {

        // =====================================================================
        // IMPLÉMENTATION : NkAddress — Constructeurs
        // =====================================================================

        NkAddress::NkAddress(const char* host, uint16 port) noexcept
            : mPort(port)
            , mFamily(Family::NK_IP_V4)
            , mValid(false)
        {
            // Validation du paramètre d'entrée
            if (host == nullptr)
            {
                return;
            }

            // Cas 1 : Adresse "any" — écoute sur toutes les interfaces
            if (std::strcmp(host, "0.0.0.0") == 0 || std::strcmp(host, "any") == 0)
            {
                mIP.ipv4[0] = 0;
                mIP.ipv4[1] = 0;
                mIP.ipv4[2] = 0;
                mIP.ipv4[3] = 0;
                mFamily = Family::NK_IP_V4;
                mValid = true;
                return;
            }

            // Cas 2 : Loopback localhost
            if (std::strcmp(host, "localhost") == 0 || std::strcmp(host, "127.0.0.1") == 0)
            {
                mIP.ipv4[0] = 127;
                mIP.ipv4[1] = 0;
                mIP.ipv4[2] = 0;
                mIP.ipv4[3] = 1;
                mFamily = Family::NK_IP_V4;
                mValid = true;
                return;
            }

            // Cas 3 : Adresse IPv4 en notation dotted-decimal (a.b.c.d)
            uint32 a = 0;
            uint32 b = 0;
            uint32 c = 0;
            uint32 d = 0;
            const int parsed = std::sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d);

            if (parsed == 4)
            {
                // Validation des plages d'octets (0-255)
                if (a <= 255 && b <= 255 && c <= 255 && d <= 255)
                {
                    mIP.ipv4[0] = static_cast<uint8>(a);
                    mIP.ipv4[1] = static_cast<uint8>(b);
                    mIP.ipv4[2] = static_cast<uint8>(c);
                    mIP.ipv4[3] = static_cast<uint8>(d);
                    mFamily = Family::NK_IP_V4;
                    mValid = true;
                    return;
                }
            }

            // Cas 4 : IPv6 — TODO : implémentation future avec inet_pton
            // Pour l'instant, marquer comme invalide si non reconnu

            // Échec de parsing — adresse invalide
            mValid = false;
        }

        NkAddress::NkAddress(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port) noexcept
            : mPort(port)
            , mFamily(Family::NK_IP_V4)
            , mValid(true)
        {
            mIP.ipv4[0] = a;
            mIP.ipv4[1] = b;
            mIP.ipv4[2] = c;
            mIP.ipv4[3] = d;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkAddress — Constructeurs plateforme-specific
        // =====================================================================
        #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_POSIX)

            NkAddress::NkAddress(const sockaddr_in& addr) noexcept
                : mFamily(Family::NK_IP_V4)
                , mValid(true)
            {
                // Extraction du port (conversion network → host byte order)
                mPort = ntohs(addr.sin_port);

                // Extraction de l'adresse IPv4 (4 octets)
                const uint8* ipBytes = reinterpret_cast<const uint8*>(&addr.sin_addr.s_addr);
                mIP.ipv4[0] = ipBytes[0];
                mIP.ipv4[1] = ipBytes[1];
                mIP.ipv4[2] = ipBytes[2];
                mIP.ipv4[3] = ipBytes[3];
            }

            NkAddress::NkAddress(const sockaddr_in6& addr) noexcept
                : mFamily(Family::NK_IP_V6)
                , mValid(true)
            {
                // Extraction du port (conversion network → host byte order)
                mPort = ntohs(addr.sin6_port);

                // Copie des 16 octets de l'adresse IPv6
                std::memcpy(mIP.ipv6, addr.sin6_addr.s6_addr, 16);
            }

            void NkAddress::ToSockAddr(sockaddr_storage& out, socklen_t& len) const noexcept
            {
                // Initialisation à zéro pour éviter les données non-initialisées
                std::memset(&out, 0, sizeof(sockaddr_storage));

                if (mFamily == Family::NK_IP_V4)
                {
                    // Cast vers sockaddr_in pour remplissage IPv4
                    auto* ipv4Struct = reinterpret_cast<sockaddr_in*>(&out);

                    ipv4Struct->sin_family = AF_INET;
                    ipv4Struct->sin_port = htons(mPort);

                    std::memcpy(&ipv4Struct->sin_addr.s_addr, mIP.ipv4, 4);

                    len = sizeof(sockaddr_in);
                }
                else if (mFamily == Family::NK_IP_V6)
                {
                    // Cast vers sockaddr_in6 pour remplissage IPv6
                    auto* ipv6Struct = reinterpret_cast<sockaddr_in6*>(&out);

                    ipv6Struct->sin6_family = AF_INET6;
                    ipv6Struct->sin6_port = htons(mPort);

                    std::memcpy(ipv6Struct->sin6_addr.s6_addr, mIP.ipv6, 16);

                    len = sizeof(sockaddr_in6);
                }
                else
                {
                    // Famille inconnue — retourner structure vide
                    len = 0;
                }
            }

        #endif // NKENTSEU_PLATFORM_WINDOWS || NKENTSEU_PLATFORM_POSIX

        // =====================================================================
        // IMPLÉMENTATION : NkAddress — Méthodes factory statiques
        // =====================================================================

        NkAddress NkAddress::Loopback(uint16 port, Family f) noexcept
        {
            if (f == Family::NK_IP_V4)
            {
                return NkAddress(127, 0, 0, 1, port);
            }
            else
            {
                // IPv6 loopback ::1 — construction manuelle des 16 octets
                NkAddress addr;
                addr.mPort = port;
                addr.mFamily = Family::NK_IP_V6;
                addr.mValid = true;

                // ::1 = 15 octets à 0, dernier octet à 1
                std::memset(addr.mIP.ipv6, 0, 16);
                addr.mIP.ipv6[15] = 1;

                return addr;
            }
        }

        NkAddress NkAddress::Any(uint16 port, Family f) noexcept
        {
            if (f == Family::NK_IP_V4)
            {
                return NkAddress(0, 0, 0, 0, port);
            }
            else
            {
                // IPv6 any :: — tous les octets à 0
                NkAddress addr;
                addr.mPort = port;
                addr.mFamily = Family::NK_IP_V6;
                addr.mValid = true;

                std::memset(addr.mIP.ipv6, 0, 16);

                return addr;
            }
        }

        NkAddress NkAddress::Broadcast(uint16 port) noexcept
        {
            // Broadcast IPv4 : 255.255.255.255
            return NkAddress(255, 255, 255, 255, port);
        }

        NkVector<NkAddress> NkAddress::Resolve(
            const char* host,
            uint16 port,
            Family preferred
        ) noexcept
        {
            NkVector<NkAddress> results;

            // Validation des paramètres
            if (host == nullptr)
            {
                return results;
            }

            // Optimisation : si host est déjà une adresse IP, pas besoin de résolution
            NkAddress direct(host, port);
            if (direct.IsValid())
            {
                results.PushBack(direct);
                return results;
            }

            // Résolution DNS via getaddrinfo (POSIX/Windows moderne)
            #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_POSIX)

                addrinfo hints = {};
                hints.ai_family = (preferred == Family::NK_IP_V4) ? AF_INET : AF_INET6;
                hints.ai_socktype = SOCK_DGRAM;  // UDP par défaut
                hints.ai_protocol = IPPROTO_UDP;

                addrinfo* resultInfo = nullptr;
                const int status = getaddrinfo(host, nullptr, &hints, &resultInfo);

                if (status == 0 && resultInfo != nullptr)
                {
                    // Parcours de la liste chaînée des résultats
                    for (addrinfo* ptr = resultInfo; ptr != nullptr; ptr = ptr->ai_next)
                    {
                        if (ptr->ai_family == AF_INET && ptr->ai_addr != nullptr)
                        {
                            // Conversion sockaddr_in → NkAddress
                            const auto* ipv4Addr = reinterpret_cast<const sockaddr_in*>(ptr->ai_addr);
                            NkAddress addr(*ipv4Addr);
                            addr.mPort = port;  // Appliquer le port demandé
                            results.PushBack(addr);
                        }
                        else if (ptr->ai_family == AF_INET6 && ptr->ai_addr != nullptr)
                        {
                            // Conversion sockaddr_in6 → NkAddress
                            const auto* ipv6Addr = reinterpret_cast<const sockaddr_in6*>(ptr->ai_addr);
                            NkAddress addr(*ipv6Addr);
                            addr.mPort = port;  // Appliquer le port demandé
                            results.PushBack(addr);
                        }
                    }

                    freeaddrinfo(resultInfo);
                }

            #endif // NKENTSEU_PLATFORM_WINDOWS || NKENTSEU_PLATFORM_POSIX

            // Fallback : si aucune résolution, retourner vecteur vide
            return results;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkAddress — Accesseurs
        // =====================================================================

        uint16 NkAddress::Port() const noexcept
        {
            return mPort;
        }

        NkAddress::Family NkAddress::GetFamily() const noexcept
        {
            return mFamily;
        }

        bool NkAddress::IsIPv4() const noexcept
        {
            return mFamily == Family::NK_IP_V4;
        }

        bool NkAddress::IsIPv6() const noexcept
        {
            return mFamily == Family::NK_IP_V6;
        }

        bool NkAddress::IsValid() const noexcept
        {
            return mPort != 0 && mValid;
        }

        bool NkAddress::IsLoopback() const noexcept
        {
            if (!mValid)
            {
                return false;
            }

            if (mFamily == Family::NK_IP_V4)
            {
                // 127.0.0.0/8 est réservé pour loopback en IPv4
                return mIP.ipv4[0] == 127;
            }
            else
            {
                // ::1 est le loopback IPv6
                // Les 15 premiers octets doivent être 0, le dernier 1
                for (int i = 0; i < 15; ++i)
                {
                    if (mIP.ipv6[i] != 0)
                    {
                        return false;
                    }
                }
                return mIP.ipv6[15] == 1;
            }
        }

        bool NkAddress::IsBroadcast() const noexcept
        {
            if (!mValid || mFamily != Family::NK_IP_V4)
            {
                return false;
            }

            // Broadcast IPv4 : tous les octets à 255
            return mIP.ipv4[0] == 255 &&
                   mIP.ipv4[1] == 255 &&
                   mIP.ipv4[2] == 255 &&
                   mIP.ipv4[3] == 255;
        }

        bool NkAddress::IsMulticast() const noexcept
        {
            if (!mValid)
            {
                return false;
            }

            if (mFamily == Family::NK_IP_V4)
            {
                // IPv4 multicast : 224.0.0.0 à 239.255.255.255
                return mIP.ipv4[0] >= 224 && mIP.ipv4[0] <= 239;
            }
            else
            {
                // IPv6 multicast : préfixe ff00::/8
                return mIP.ipv6[0] == 0xFF;
            }
        }

        NkString NkAddress::ToString() const noexcept
        {
            char buffer[64] = {};

            if (mFamily == Family::NK_IP_V4)
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "%u.%u.%u.%u:%u",
                    mIP.ipv4[0],
                    mIP.ipv4[1],
                    mIP.ipv4[2],
                    mIP.ipv4[3],
                    mPort
                );
            }
            else
            {
                // Format IPv6 : [::1]:port pour éviter ambiguïté avec le séparateur de port
                std::snprintf(buffer, sizeof(buffer), "[ipv6]:%u", mPort);
                // TODO : Implémenter conversion réelle IPv6 → chaîne avec inet_ntop
            }

            return NkString(buffer);
        }

        NkString NkAddress::HostString() const noexcept
        {
            char buffer[64] = {};

            if (mFamily == Family::NK_IP_V4)
            {
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "%u.%u.%u.%u",
                    mIP.ipv4[0],
                    mIP.ipv4[1],
                    mIP.ipv4[2],
                    mIP.ipv4[3]
                );
            }
            else
            {
                std::snprintf(buffer, sizeof(buffer), "ipv6");
                // TODO : Implémenter conversion réelle IPv6 → chaîne
            }

            return NkString(buffer);
        }

        bool NkAddress::operator==(const NkAddress& other) const noexcept
        {
            // Comparaison rapide des champs scalaires
            if (mFamily != other.mFamily)
            {
                return false;
            }

            if (mPort != other.mPort)
            {
                return false;
            }

            // Comparaison des données IP selon la famille
            if (mFamily == Family::NK_IP_V4)
            {
                return std::memcmp(mIP.ipv4, other.mIP.ipv4, 4) == 0;
            }
            else
            {
                return std::memcmp(mIP.ipv6, other.mIP.ipv6, 16) == 0;
            }
        }

        bool NkAddress::operator!=(const NkAddress& other) const noexcept
        {
            return !(*this == other);
        }

        bool NkAddress::operator<(const NkAddress& other) const noexcept
        {
            // Ordre lexicographique : famille → IP → port

            if (mFamily != other.mFamily)
            {
                return static_cast<uint8>(mFamily) < static_cast<uint8>(other.mFamily);
            }

            if (mFamily == Family::NK_IP_V4)
            {
                const int ipCmp = std::memcmp(mIP.ipv4, other.mIP.ipv4, 4);
                if (ipCmp != 0)
                {
                    return ipCmp < 0;
                }
            }
            else
            {
                const int ipCmp = std::memcmp(mIP.ipv6, other.mIP.ipv6, 16);
                if (ipCmp != 0)
                {
                    return ipCmp < 0;
                }
            }

            return mPort < other.mPort;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkPacket — Méthodes utilitaires
        // =====================================================================

        bool NkPacket::IsEmpty() const noexcept
        {
            return size == 0;
        }

        void NkPacket::Clear() noexcept
        {
            size = 0;
            seqNum = 0;
            ackMask = 0;
            // Note : on ne zero-remplit pas data[] pour performance
            // Le contenu sera écrasé au prochain remplissage
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Constructeurs/Destructeur
        // =====================================================================

        NkSocket::~NkSocket() noexcept
        {
            // RAII : fermeture automatique si socket encore ouvert
            Close();
        }

        NkSocket::NkSocket(NkSocket&& other) noexcept
            : mHandle(other.mHandle)
            , mType(other.mType)
            , mLocalAddr(other.mLocalAddr)
            , mNonBlocking(other.mNonBlocking)
        {
            // Transfert de propriété : invalider la source
            other.mHandle = kNkNativeInvalidSocket;
        }

        NkSocket& NkSocket::operator=(NkSocket&& other) noexcept
        {
            // Protection contre auto-affectation
            if (this != &other)
            {
                // Libération des ressources actuelles
                Close();

                // Transfert des membres
                mHandle = other.mHandle;
                mType = other.mType;
                mLocalAddr = other.mLocalAddr;
                mNonBlocking = other.mNonBlocking;

                // Invalidation de la source
                other.mHandle = kNkNativeInvalidSocket;
            }

            return *this;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Cycle de vie
        // =====================================================================

        NkNetResult NkSocket::Create(const NkAddress& localAddr, Type type) noexcept
        {
            // Fermeture d'un socket précédemment ouvert (idempotent)
            Close();

            // Mémorisation des paramètres de création
            mType = type;
            mLocalAddr = localAddr;

            // Sélection des paramètres socket selon la famille d'adresse
            const int addressFamily = AF_INET;  // IPv4 uniquement pour l'instant
            const int socketType = (type == Type::NK_UDP) ? SOCK_DGRAM : SOCK_STREAM;
            const int protocol = (type == Type::NK_UDP) ? IPPROTO_UDP : IPPROTO_TCP;

            // Création du socket natif
            mHandle = ::socket(addressFamily, socketType, protocol);

            if (mHandle == kNkNativeInvalidSocket)
            {
                const int errorCode = NkGetLastSocketError();
                NK_NET_LOG_ERROR("socket() failed: code %d", errorCode);
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Préparation de l'adresse pour bind()
            sockaddr_storage addressStorage = {};
            socklen_t addressLength = 0;
            localAddr.ToSockAddr(addressStorage, addressLength);

            // Liaison du socket à l'adresse locale
            const int bindResult = ::bind(
                mHandle,
                reinterpret_cast<sockaddr*>(&addressStorage),
                addressLength
            );

            if (bindResult != 0)
            {
                const int errorCode = NkGetLastSocketError();
                NK_NET_LOG_ERROR("bind() failed: code %d", errorCode);
                Close();  // Nettoyage en cas d'échec
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Configuration par défaut des buffers pour performance
            SetSendBufferSize(kNkSendBufferSize);
            SetRecvBufferSize(kNkRecvBufferSize);

            return NkNetResult::NK_NET_OK;
        }

        void NkSocket::Close() noexcept
        {
            if (mHandle != kNkNativeInvalidSocket)
            {
                // Fermeture plateforme-specific via macro
                NK_NET_CLOSE_SOCKET_IMPL(mHandle);
                mHandle = kNkNativeInvalidSocket;
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Configuration
        // =====================================================================

        NkNetResult NkSocket::SetNonBlocking(bool enabled) noexcept
        {
            if (!IsValid())
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS)

                u_long mode = enabled ? 1 : 0;
                const int result = ::ioctlsocket(mHandle, FIONBIO, &mode);
                if (result != 0)
                {
                    return NkNetResult::NK_NET_SOCKET_ERROR;
                }

            #elif defined(NKENTSEU_PLATFORM_POSIX)

                int flags = ::fcntl(mHandle, F_GETFL, 0);
                if (flags < 0)
                {
                    return NkNetResult::NK_NET_SOCKET_ERROR;
                }

                if (enabled)
                {
                    flags |= O_NONBLOCK;
                }
                else
                {
                    flags &= ~O_NONBLOCK;
                }

                if (::fcntl(mHandle, F_SETFL, flags) < 0)
                {
                    return NkNetResult::NK_NET_SOCKET_ERROR;
                }

            #endif // Plateforme

            mNonBlocking = enabled;
            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::SetBroadcast(bool enabled) noexcept
        {
            if (!IsValid())
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            int optionValue = enabled ? 1 : 0;
            const int result = ::setsockopt(
                mHandle,
                SOL_SOCKET,
                SO_BROADCAST,
                reinterpret_cast<const char*>(&optionValue),
                sizeof(optionValue)
            );

            if (result != 0)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::SetNoDelay(bool enabled) noexcept
        {
            if (!IsValid() || mType != Type::NK_TCP)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            int optionValue = enabled ? 1 : 0;
            const int result = ::setsockopt(
                mHandle,
                IPPROTO_TCP,
                TCP_NODELAY,
                reinterpret_cast<const char*>(&optionValue),
                sizeof(optionValue)
            );

            if (result != 0)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::SetReuseAddr(bool enabled) noexcept
        {
            if (!IsValid())
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            int optionValue = enabled ? 1 : 0;
            const int result = ::setsockopt(
                mHandle,
                SOL_SOCKET,
                SO_REUSEADDR,
                reinterpret_cast<const char*>(&optionValue),
                sizeof(optionValue)
            );

            if (result != 0)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::SetSendBufferSize(uint32 bytes) noexcept
        {
            if (!IsValid())
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            int bufferSize = static_cast<int>(bytes);
            ::setsockopt(
                mHandle,
                SOL_SOCKET,
                SO_SNDBUF,
                reinterpret_cast<const char*>(&bufferSize),
                sizeof(bufferSize)
            );
            // Note : on ignore l'échec — l'OS ajuste selon ses limites

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::SetRecvBufferSize(uint32 bytes) noexcept
        {
            if (!IsValid())
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            int bufferSize = static_cast<int>(bytes);
            ::setsockopt(
                mHandle,
                SOL_SOCKET,
                SO_RCVBUF,
                reinterpret_cast<const char*>(&bufferSize),
                sizeof(bufferSize)
            );
            // Note : on ignore l'échec — l'OS ajuste selon ses limites

            return NkNetResult::NK_NET_OK;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Opérations UDP
        // =====================================================================

        NkNetResult NkSocket::SendTo(
            const void* data,
            uint32 size,
            const NkAddress& target
        ) noexcept
        {
            // Validation des paramètres
            if (!IsValid() || data == nullptr || size == 0)
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Vérification de la taille maximale MTU-safe
            if (size > kNkMaxPacketSize)
            {
                return NkNetResult::NK_NET_PACKET_TOO_LARGE;
            }

            // Préparation de l'adresse de destination
            sockaddr_storage addressStorage = {};
            socklen_t addressLength = 0;
            target.ToSockAddr(addressStorage, addressLength);

            // Envoi du datagramme
            const int sentBytes = ::sendto(
                mHandle,
                reinterpret_cast<const char*>(data),
                static_cast<int>(size),
                0,  // Flags
                reinterpret_cast<sockaddr*>(&addressStorage),
                addressLength
            );

            if (sentBytes == NK_NET_SOCKET_ERROR_CODE)
            {
                const int errorCode = NkGetLastSocketError();

                // Gestion du cas "would block" en mode non-bloquant
                if (mNonBlocking && NkIsWouldBlockError(errorCode))
                {
                    return NkNetResult::NK_NET_BUFFER_FULL;
                }

                NK_NET_LOG_ERROR("sendto() failed: code %d", errorCode);
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::RecvFrom(
            void* buffer,
            uint32 bufferSize,
            uint32& outSize,
            NkAddress& outFrom
        ) noexcept
        {
            // Validation des paramètres
            if (!IsValid() || buffer == nullptr)
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Préparation pour réception de l'adresse source
            sockaddr_storage addressStorage = {};
            socklen_t addressLength = sizeof(sockaddr_storage);

            // Réception du datagramme
            const int receivedBytes = ::recvfrom(
                mHandle,
                reinterpret_cast<char*>(buffer),
                static_cast<int>(bufferSize),
                0,  // Flags
                reinterpret_cast<sockaddr*>(&addressStorage),
                &addressLength
            );

            if (receivedBytes == NK_NET_SOCKET_ERROR_CODE)
            {
                const int errorCode = NkGetLastSocketError();

                // En mode non-bloquant, "rien à lire" n'est pas une erreur
                if (mNonBlocking && NkIsWouldBlockError(errorCode))
                {
                    outSize = 0;
                    return NkNetResult::NK_NET_OK;
                }

                NK_NET_LOG_ERROR("recvfrom() failed: code %d", errorCode);
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Remplissage des paramètres de sortie
            outSize = static_cast<uint32>(receivedBytes);

            // Extraction de l'adresse source si disponible
            if (addressStorage.ss_family == AF_INET && addressLength >= sizeof(sockaddr_in))
            {
                const auto& ipv4Addr = reinterpret_cast<const sockaddr_in&>(addressStorage);
                outFrom = NkAddress(ipv4Addr);
            }
            else if (addressStorage.ss_family == AF_INET6 && addressLength >= sizeof(sockaddr_in6))
            {
                const auto& ipv6Addr = reinterpret_cast<const sockaddr_in6&>(addressStorage);
                outFrom = NkAddress(ipv6Addr);
            }

            return NkNetResult::NK_NET_OK;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Opérations TCP
        // =====================================================================

        NkNetResult NkSocket::Listen(uint32 backlog) noexcept
        {
            if (!IsValid() || mType != Type::NK_TCP)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            const int result = ::listen(mHandle, static_cast<int>(backlog));
            if (result != 0)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::Connect(const NkAddress& remote) noexcept
        {
            if (!IsValid())
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            sockaddr_storage addressStorage = {};
            socklen_t addressLength = 0;
            remote.ToSockAddr(addressStorage, addressLength);

            const int result = ::connect(
                mHandle,
                reinterpret_cast<sockaddr*>(&addressStorage),
                addressLength
            );

            if (result != 0)
            {
                const int errorCode = NkGetLastSocketError();

                // En mode non-bloquant, EINPROGRESS/WSAEWOULDBLOCK est attendu
                #if defined(NKENTSEU_PLATFORM_WINDOWS)
                    if (errorCode != WSAEWOULDBLOCK)
                #else
                    if (errorCode != EINPROGRESS)
                #endif
                {
                    return NkNetResult::NK_NET_SOCKET_ERROR;
                }
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::Accept(NkSocket& outClient, NkAddress& outAddr) noexcept
        {
            if (!IsValid() || mType != Type::NK_TCP)
            {
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            sockaddr_storage addressStorage = {};
            socklen_t addressLength = sizeof(sockaddr_storage);

            const NkNativeSocketHandle clientHandle = ::accept(
                mHandle,
                reinterpret_cast<sockaddr*>(&addressStorage),
                &addressLength
            );

            if (clientHandle == kNkNativeInvalidSocket)
            {
                const int errorCode = NkGetLastSocketError();

                // En mode non-bloquant, aucune connexion en attente n'est normal
                if (mNonBlocking && NkIsWouldBlockError(errorCode))
                {
                    return NkNetResult::NK_NET_OK;
                }

                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Transfert du handle vers le socket client
            outClient.Close();  // Nettoyage préventif
            outClient.mHandle = clientHandle;
            outClient.mType = Type::NK_TCP;
            outClient.mNonBlocking = mNonBlocking;  // Hériter du mode

            // Extraction de l'adresse du client
            if (addressStorage.ss_family == AF_INET)
            {
                const auto& ipv4Addr = reinterpret_cast<const sockaddr_in&>(addressStorage);
                outAddr = NkAddress(ipv4Addr);
            }
            else if (addressStorage.ss_family == AF_INET6)
            {
                const auto& ipv6Addr = reinterpret_cast<const sockaddr_in6&>(addressStorage);
                outAddr = NkAddress(ipv6Addr);
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::Send(const void* data, uint32 size) noexcept
        {
            if (!IsValid() || data == nullptr || size == 0)
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            const int sentBytes = ::send(
                mHandle,
                reinterpret_cast<const char*>(data),
                static_cast<int>(size),
                0  // Flags
            );

            if (sentBytes == NK_NET_SOCKET_ERROR_CODE)
            {
                const int errorCode = NkGetLastSocketError();

                if (mNonBlocking && NkIsWouldBlockError(errorCode))
                {
                    return NkNetResult::NK_NET_BUFFER_FULL;
                }

                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkSocket::Recv(void* buffer, uint32 bufferSize, uint32& outSize) noexcept
        {
            if (!IsValid() || buffer == nullptr)
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            const int receivedBytes = ::recv(
                mHandle,
                reinterpret_cast<char*>(buffer),
                static_cast<int>(bufferSize),
                0  // Flags
            );

            if (receivedBytes == NK_NET_SOCKET_ERROR_CODE)
            {
                const int errorCode = NkGetLastSocketError();

                if (mNonBlocking && NkIsWouldBlockError(errorCode))
                {
                    outSize = 0;
                    return NkNetResult::NK_NET_OK;
                }

                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            outSize = static_cast<uint32>(receivedBytes);
            return NkNetResult::NK_NET_OK;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Inspection
        // =====================================================================

        bool NkSocket::IsValid() const noexcept
        {
            return mHandle != kNkNativeInvalidSocket;
        }

        NkSocket::Type NkSocket::GetType() const noexcept
        {
            return mType;
        }

        const NkAddress& NkSocket::GetLocalAddr() const noexcept
        {
            return mLocalAddr;
        }

        int32 NkSocket::GetLastError() const noexcept
        {
            return static_cast<int32>(NkGetLastSocketError());
        }

        // =====================================================================
        // IMPLÉMENTATION : NkSocket — Utilitaires statiques
        // =====================================================================

        NkNetResult NkSocket::PlatformInit() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)

                WSADATA wsaData = {};
                const int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);

                if (result != 0)
                {
                    NK_NET_LOG_ERROR("WSAStartup failed: code %d", result);
                    return NkNetResult::NK_NET_SOCKET_ERROR;
                }

                // Vérification de la version Winsock demandée
                if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
                {
                    NK_NET_LOG_ERROR("Winsock 2.2 not supported");
                    ::WSACleanup();
                    return NkNetResult::NK_NET_PLATFORM_UNSUPPORTED;
                }

            #endif // NKENTSEU_PLATFORM_WINDOWS

            // POSIX et Emscripten : pas d'initialisation requise
            return NkNetResult::NK_NET_OK;
        }

        void NkSocket::PlatformShutdown() noexcept
        {
            #if defined(NKENTSEU_PLATFORM_WINDOWS)

                ::WSACleanup();

            #endif // NKENTSEU_PLATFORM_WINDOWS

            // POSIX et Emscripten : pas de nettoyage requis
        }

        NkNetResult NkSocket::Select(
            NkSpan<NkSocket*> socketsToWatch,
            uint32 timeoutMs,
            NkVector<uint32>& outReady
        ) noexcept
        {
            // Validation des paramètres
            if (socketsToWatch.Empty())
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_POSIX)

                // Préparation du fd_set pour select()
                fd_set readSet = {};
                FD_ZERO(&readSet);

                // Mapping indice → handle pour reconstruction du résultat
                NkVector<NkNativeSocketHandle> handleMap;
                handleMap.Reserve(socketsToWatch.Size());

                NkNativeSocketHandle maxHandle = 0;

                for (uint32 i = 0; i < socketsToWatch.Size(); ++i)
                {
                    NkSocket* sock = socketsToWatch[i];
                    if (sock != nullptr && sock->IsValid())
                    {
                        FD_SET(sock->mHandle, &readSet);
                        handleMap.PushBack(sock->mHandle);

                        #if defined(NKENTSEU_PLATFORM_WINDOWS)
                            // Windows : maxHandle non requis pour select()
                        #else
                            // POSIX : select() nécessite le max descriptor + 1
                            if (sock->mHandle > maxHandle)
                            {
                                maxHandle = sock->mHandle;
                            }
                        #endif
                    }
                }

                if (handleMap.IsEmpty())
                {
                    return NkNetResult::NK_NET_INVALID_ARG;
                }

                // Configuration du timeout
                timeval timeout = {};
                timeval* timeoutPtr = nullptr;

                if (timeoutMs == UINT32_MAX)
                {
                    // Timeout infini : nullptr pour bloquer indéfiniment
                    timeoutPtr = nullptr;
                }
                else
                {
                    timeout.tv_sec = static_cast<long>(timeoutMs / 1000);
                    timeout.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);
                    timeoutPtr = &timeout;
                }

                // Appel à select()
                const int result = ::select(
                    #if defined(NKENTSEU_PLATFORM_WINDOWS)
                        0,  // Premier paramètre ignoré sous Windows
                    #else
                        static_cast<int>(maxHandle + 1),
                    #endif
                    &readSet,
                    nullptr,  // writeSet non utilisé
                    nullptr,  // exceptSet non utilisé
                    timeoutPtr
                );

                if (result < 0)
                {
                    const int errorCode = NkGetLastSocketError();
                    NK_NET_LOG_ERROR("select() failed: code %d", errorCode);
                    return NkNetResult::NK_NET_SOCKET_ERROR;
                }

                if (result == 0)
                {
                    // Timeout écoulé sans activité
                    return NkNetResult::NK_NET_TIMEOUT;
                }

                // Collecte des sockets prêts
                outReady.Clear();
                outReady.Reserve(static_cast<size_t>(result));

                for (uint32 i = 0; i < handleMap.Size(); ++i)
                {
                    if (FD_ISSET(handleMap[i], &readSet))
                    {
                        outReady.PushBack(i);
                    }
                }

                return NkNetResult::NK_NET_OK;

            #else

                // Emscripten : select() non disponible pour WebSocket
                // Fallback : polling simple (non-optimal)
                outReady.Clear();
                for (uint32 i = 0; i < socketsToWatch.Size(); ++i)
                {
                    // TODO : Implémenter détection d'activité Emscripten-specific
                    // Pour l'instant, retourner vide
                }
                return NkNetResult::NK_NET_OK;

            #endif // Plateforme
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
    Gestion des erreurs plateforme :
    -------------------------------
    - Windows utilise WSAGetLastError(), POSIX utilise errno
    - Les macros NkGetLastSocketError() et NkIsWouldBlockError() unifient la gestion
    - Toujours logger les erreurs avec le code pour diagnostic facilité

    Mode non-bloquant :
    ------------------
    - En mode non-bloquant, "aucune donnée disponible" n'est pas une erreur
    - Recv/RecvFrom retournent OK avec outSize=0 dans ce cas
    - Le code appelant doit gérer ce cas explicitement

    Gestion des buffers :
    --------------------
    - Les buffers système (SO_SNDBUF/SO_RCVBUF) sont indicatifs
    - L'OS peut ajuster les valeurs selon ses limites et politiques
    - Toujours vérifier la taille réelle via getsockopt() si critique

    Performance :
    ------------
    - Éviter les allocations heap dans les chemins critiques (Send/Recv)
    - Utiliser des buffers stack ou pré-alloués pour les opérations fréquentes
    - Désactiver Nagle (TCP_NODELAY) pour les applications temps réel

    Thread-safety :
    --------------
    - Les sockets ne sont PAS thread-safe par conception
    - Si un socket est partagé entre threads, protéger avec NkMutex
    - Alternative préférée : un thread par socket avec communication via queues

    Extensions futures :
    -------------------
    - Support complet IPv6 (inet_pton, inet_ntop pour conversion chaîne)
    - Options socket avancées : keepalive, linger, tcp_keepidle, etc.
    - Support SSL/TLS via wrapper NkSecureSocket
    - Intégration avec I/O completions ports (Windows) / epoll (Linux) pour scaling

    Compatibilité Emscripten :
    -------------------------
    - WebSocket API ≠ sockets POSIX — limitations importantes
    - UDP natif non disponible dans les navigateurs
    - Modèle async Emscripten incompatible avec select() bloquant
    - Envisager une abstraction Web-specific pour le WebAssembly
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================