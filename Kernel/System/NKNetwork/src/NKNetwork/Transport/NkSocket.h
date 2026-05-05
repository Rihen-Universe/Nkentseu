// =============================================================================
// NKNetwork/Transport/NkSocket.h
// =============================================================================
// DESCRIPTION :
//   Adresse réseau (IP + port) et socket brut UDP/TCP cross-platform.
//
// FONCTIONNALITÉS :
//   - Abstraction unifiée des sockets pour Windows, POSIX et WebAssembly
//   - Gestion des adresses IPv4/IPv6 avec résolution DNS
//   - Support UDP (datagrammes) et TCP (flux) avec configuration avancée
//   - API non-bloquante optimisée pour les boucles de jeu temps réel
//
// DÉPENDANCES :
//   - NKCore/NkNetDefines.h : Types fondamentaux et macros réseau
//   - NKPlatform : Détection de plateforme et headers système
//
// RÈGLES D'UTILISATION :
//   - Toujours appeler NkSocket::PlatformInit() avant toute utilisation
//   - Utiliser SetNonBlocking(true) pour les applications interactives
//   - Vérifier les codes de retour NkNetResult pour chaque opération
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKSOCKET_H
#define NKENTSEU_NETWORK_NKSOCKET_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Core/NkNetDefines.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Views/NkSpan.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION PLATEFORME ET HEADERS SYSTÈME
    // -------------------------------------------------------------------------
    // Définition des macros et inclusion des headers système spécifiques.

    // =====================================================================
    // SOUS-SECTION 2.1 : Windows — Winsock2 API
    // =====================================================================
    #if defined(NKENTSEU_PLATFORM_WINDOWS)

        // Réduction de l'empreinte des headers Windows pour compilation optimisée.
        #define WIN32_LEAN_AND_MEAN

        // Headers Winsock2 pour la gestion des sockets Windows.
        #include <winsock2.h>
        #include <ws2tcpip.h>

        // Alias de type pour le handle de socket Windows.
        using NkNativeSocketHandle = SOCKET;

        // Valeur constante représentant un socket invalide sous Windows.
        static constexpr NkNativeSocketHandle kNkNativeInvalidSocket = INVALID_SOCKET;

        // Macro de fermeture de socket spécifique à Windows.
        #define NK_NET_CLOSE_SOCKET_IMPL(s) ::closesocket(s)

        // Code d'erreur standard pour les opérations socket sous Windows.
        #define NK_NET_SOCKET_ERROR_CODE    SOCKET_ERROR

    // =====================================================================
    // SOUS-SECTION 2.2 : POSIX — Linux/macOS/Android/iOS
    // =====================================================================
    #elif defined(NKENTSEU_PLATFORM_POSIX)

        // Headers POSIX standard pour la gestion des sockets Unix-like.
        #include <sys/socket.h>
        #include <sys/types.h>
        #include <netinet/in.h>
        #include <netinet/tcp.h>
        #include <arpa/inet.h>
        #include <unistd.h>
        #include <fcntl.h>
        #include <netdb.h>
        #include <errno.h>

        // Alias de type pour le handle de socket POSIX (descripteur de fichier).
        using NkNativeSocketHandle = int;

        // Valeur constante représentant un socket invalide sous POSIX.
        static constexpr NkNativeSocketHandle kNkNativeInvalidSocket = -1;

        // Macro de fermeture de socket spécifique à POSIX.
        #define NK_NET_CLOSE_SOCKET_IMPL(s) ::close(s)

        // Code d'erreur standard pour les opérations socket sous POSIX.
        #define NK_NET_SOCKET_ERROR_CODE    -1

    // =====================================================================
    // SOUS-SECTION 2.3 : WebAssembly — Emscripten (support limité)
    // =====================================================================
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

        // Header Emscripten pour l'API WebSocket (UDP natif non disponible).
        #include <emscripten/websocket.h>

        // Alias de type pour le handle de socket Web (WebSocket ID).
        using NkNativeSocketHandle = int;

        // Valeur constante représentant un socket invalide pour le Web.
        static constexpr NkNativeSocketHandle kNkNativeInvalidSocket = -1;

        // Macro de fermeture de socket pour Web (code 1000 = fermeture normale).
        #define NK_NET_CLOSE_SOCKET_IMPL(s) emscripten_websocket_close(s, 1000, "")

    #endif // Fin de la détection de plateforme

    // -------------------------------------------------------------------------
    // SECTION 3 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // =================================================================
            // CLASSE : NkAddress — Abstraction d'adresse réseau
            // =================================================================
            /**
             * @class NkAddress
             * @brief Représente une adresse réseau complète (IP + port) IPv4 ou IPv6.
             *
             * Cette classe fournit une interface unifiée pour manipuler des adresses
             * réseau indépendamment de la plateforme sous-jacente. Elle supporte :
             *   - Construction depuis chaîne, composants IPv4, ou structures système
             *   - Résolution DNS (bloquante) pour les noms d'hôte
             *   - Méthodes de prédicat pour tester les types d'adresses
             *   - Conversion vers structures sockaddr pour appels système
             *
             * @threadsafe Lecture seule thread-safe après construction
             * @note Les opérations de résolution DNS sont bloquantes — à éviter en game loop
             *
             * @example
             * @code
             * // Construction depuis une chaîne
             * NkAddress addr1("192.168.1.100", 7777);
             * NkAddress addr2("localhost", 8080);
             *
             * // Construction depuis composants IPv4
             * NkAddress addr3(10, 0, 0, 1, 12345);
             *
             * // Adresses spéciales
             * NkAddress loopback = NkAddress::Loopback(9000);
             * NkAddress any      = NkAddress::Any(0);  // Port 0 = auto-assigné
             *
             * // Résolution DNS (attention : bloquant)
             * auto resolved = NkAddress::Resolve("example.com", 443);
             * for (const auto& a : resolved) {
             *     NK_NET_LOG_INFO("Résolu : {}", a.ToString());
             * }
             * @endcode
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkAddress
            {
            public:

                // -------------------------------------------------------------
                // Énumération : Family — Famille d'adresse IP
                // -------------------------------------------------------------
                /**
                 * @enum Family
                 * @brief Identifie la famille de protocole IP utilisée.
                 */
                enum class Family : uint8
                {
                    NK_IP_V4 = 0,  ///< Adresse IPv4 (32 bits, notation dotted-decimal)
                    NK_IP_V6       ///< Adresse IPv6 (128 bits, notation hexadécimale)
                };

                // -------------------------------------------------------------
                // Constructeurs et destructeur
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise une adresse invalide.
                 * @note L'adresse résultante aura IsValid() == false.
                 */
                NkAddress() noexcept = default;

                /**
                 * @brief Construit une adresse depuis une chaîne hôte et un port.
                 * @param host Chaîne représentant l'hôte :
                 *             - IPv4 : "192.168.1.1"
                 *             - IPv6 : "::1" (non implémenté actuellement)
                 *             - Nom : "localhost", "example.com" (résolution DNS)
                 * @param port Numéro de port (1-65535), 0 pour auto-assignation.
                 * @note La résolution DNS est bloquante et peut échouer.
                 */
                NkAddress(const char* host, uint16 port) noexcept;

                /**
                 * @brief Construit une adresse IPv4 depuis ses quatre octets.
                 * @param a Premier octet de l'adresse (0-255).
                 * @param b Deuxième octet de l'adresse (0-255).
                 * @param c Troisième octet de l'adresse (0-255).
                 * @param d Quatrième octet de l'adresse (0-255).
                 * @param port Numéro de port associé (1-65535).
                 * @example NkAddress addr(192, 168, 1, 100, 7777);
                 */
                NkAddress(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port) noexcept;

                // -------------------------------------------------------------
                // Constructeurs spécifiques plateforme (headers système)
                // -------------------------------------------------------------
                #if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_POSIX)

                    /**
                     * @brief Construit depuis une structure sockaddr_in (IPv4).
                     * @param addr Structure système contenant l'adresse IPv4.
                     * @note Conversion automatique du byte-order (network → host).
                     */
                    explicit NkAddress(const sockaddr_in& addr) noexcept;

                    /**
                     * @brief Construit depuis une structure sockaddr_in6 (IPv6).
                     * @param addr Structure système contenant l'adresse IPv6.
                     * @note IPv6 partiellement supporté — vérifier IsIPv6() après construction.
                     */
                    explicit NkAddress(const sockaddr_in6& addr) noexcept;

                    /**
                     * @brief Convertit cette adresse vers une structure sockaddr_storage.
                     * @param out Structure de sortie à remplir.
                     * @param len Longueur effective de la structure remplie.
                     * @note Utilisé pour les appels système bind/connect/sendto.
                     */
                    void ToSockAddr(sockaddr_storage& out, socklen_t& len) const noexcept;

                #endif // NKENTSEU_PLATFORM_WINDOWS || NKENTSEU_PLATFORM_POSIX

                // -------------------------------------------------------------
                // Méthodes factory statiques
                // -------------------------------------------------------------

                /**
                 * @brief Crée une adresse de loopback (localhost).
                 * @param port Numéro de port à utiliser.
                 * @param f Famille d'adresse (IPv4 par défaut, IPv6 partiellement supporté).
                 * @return NkAddress configurée pour 127.0.0.1 (ou ::1 pour IPv6).
                 * @example auto addr = NkAddress::Loopback(8080);
                 */
                [[nodiscard]] static NkAddress Loopback(uint16 port, Family f = Family::NK_IP_V4) noexcept;

                /**
                 * @brief Crée une adresse "any" (écoute sur toutes les interfaces).
                 * @param port Numéro de port à utiliser.
                 * @param f Famille d'adresse (IPv4 par défaut).
                 * @return NkAddress configurée pour 0.0.0.0 (ou :: pour IPv6).
                 * @example auto addr = NkAddress::Any(0);  // Port auto-assigné
                 */
                [[nodiscard]] static NkAddress Any(uint16 port, Family f = Family::NK_IP_V4) noexcept;

                /**
                 * @brief Crée une adresse de broadcast IPv4.
                 * @param port Numéro de port à utiliser.
                 * @return NkAddress configurée pour 255.255.255.255.
                 * @note Broadcast uniquement supporté en IPv4 avec socket configuré via SetBroadcast().
                 */
                [[nodiscard]] static NkAddress Broadcast(uint16 port) noexcept;

                /**
                 * @brief Résout un nom d'hôte en une ou plusieurs adresses IP.
                 * @param host Nom d'hôte ou adresse IP en chaîne ("example.com", "192.168.1.1").
                 * @param port Numéro de port à associer aux adresses résolues.
                 * @param preferred Famille préférée en cas de résolution duale (IPv4 par défaut).
                 * @return Vecteur d'adresses résolues (peut être vide en cas d'échec).
                 * @warning Opération bloquante — ne pas appeler dans la game loop.
                 * @example
                 * @code
                 * auto addresses = NkAddress::Resolve("game.example.com", 7777);
                 * for (const auto& addr : addresses) {
                 *     if (addr.IsValid()) { /\* Tenter connexion *\/ }
                 * }
                 * @endcode
                 */
                [[nodiscard]] static NkVector<NkAddress> Resolve(
                    const char* host,
                    uint16 port,
                    Family preferred = Family::NK_IP_V4
                ) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — inspection de l'adresse
                // -------------------------------------------------------------

                /**
                 * @brief Retourne le numéro de port de cette adresse.
                 * @return Port en host byte order (1-65535, ou 0 si non assigné).
                 */
                [[nodiscard]] uint16 Port() const noexcept;

                /**
                 * @brief Retourne la famille d'adresse (IPv4 ou IPv6).
                 * @return Valeur de l'énumération Family.
                 */
                [[nodiscard]] Family GetFamily() const noexcept;

                /**
                 * @brief Teste si cette adresse est de famille IPv4.
                 * @return true si GetFamily() == Family::IPv4.
                 */
                [[nodiscard]] bool IsIPv4() const noexcept;

                /**
                 * @brief Teste si cette adresse est de famille IPv6.
                 * @return true si GetFamily() == Family::IPv6.
                 */
                [[nodiscard]] bool IsIPv6() const noexcept;

                /**
                 * @brief Teste si cette adresse est valide et utilisable.
                 * @return true si le port est non-nul ET les données IP sont valides.
                 */
                [[nodiscard]] bool IsValid() const noexcept;

                /**
                 * @brief Teste si cette adresse pointe vers localhost/loopback.
                 * @return true pour 127.0.0.1 (IPv4) ou ::1 (IPv6).
                 */
                [[nodiscard]] bool IsLoopback() const noexcept;

                /**
                 * @brief Teste si cette adresse est l'adresse de broadcast IPv4.
                 * @return true pour 255.255.255.255.
                 * @note Toujours false pour IPv6 (le broadcast n'existe pas en IPv6).
                 */
                [[nodiscard]] bool IsBroadcast() const noexcept;

                /**
                 * @brief Teste si cette adresse appartient à une plage multicast.
                 * @return true pour IPv4: 224.0.0.0-239.255.255.255 ou IPv6: ff00::/8.
                 */
                [[nodiscard]] bool IsMulticast() const noexcept;

                /**
                 * @brief Retourne une représentation lisible "host:port".
                 * @return NkString au format "192.168.1.1:7777" ou "[::1]:8080".
                 * @note Format adapté pour logging et affichage utilisateur.
                 */
                [[nodiscard]] NkString ToString() const noexcept;

                /**
                 * @brief Retourne uniquement la partie hôte (sans le port).
                 * @return NkString contenant l'adresse IP ou le nom résolu.
                 */
                [[nodiscard]] NkString HostString() const noexcept;

                // -------------------------------------------------------------
                // Opérateurs de comparaison
                // -------------------------------------------------------------

                /**
                 * @brief Compare l'égalité de deux adresses.
                 * @param o Autre adresse à comparer.
                 * @return true si famille, IP et port sont identiques.
                 */
                bool operator==(const NkAddress& o) const noexcept;

                /**
                 * @brief Compare la différence de deux adresses.
                 * @param o Autre adresse à comparer.
                 * @return true si au moins un champ diffère.
                 */
                bool operator!=(const NkAddress& o) const noexcept;

                /**
                 * @brief Compare l'ordre pour utilisation dans conteneurs triés.
                 * @param o Autre adresse à comparer.
                 * @return true si cette adresse est "inférieure" à l'autre.
                 * @note Ordre basé sur: famille → IP (byte-à-byte) → port.
                 */
                bool operator<(const NkAddress& o) const noexcept;

            private:

                // -------------------------------------------------------------
                // Membres privés — stockage interne de l'adresse
                // -------------------------------------------------------------

                // Union pour stockage compact IPv4 (4 octets) ou IPv6 (16 octets).
                union
                {
                    uint8 ipv4[4] = {};  ///< Octets pour adresse IPv4 (a.b.c.d)
                    uint8 ipv6[16];      ///< Octets pour adresse IPv6 (128 bits)
                } mIP;

                uint16  mPort;           ///< Numéro de port en host byte order
                Family  mFamily;         ///< Famille d'adresse (IPv4 ou IPv6)
                bool    mValid;          ///< Indicateur de validité après construction
            };

            // =================================================================
            // STRUCTURE : NkPacket — Paquet réseau prêt à l'envoi/réception
            // =================================================================
            /**
             * @struct NkPacket
             * @brief Conteneur de données réseau avec métadonnées de transport.
             *
             * Cette structure représente un paquet réseau complet, incluant :
             *   - Buffer de données inline (pas d'allocation heap)
             *   - Adresse source pour les paquets reçus
             *   - Numéros de séquence et masques ACK pour protocole fiable
             *   - Canal logique pour gestion des garanties de livraison
             *
             * @note Taille maximale : kNkMaxPacketSize (1400 bytes, MTU-safe)
             * @see NkNetChannel Pour les garanties de livraison par canal
             * @see NkSocket::SendTo / RecvFrom Pour l'envoi/réception de paquets
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkPacket
            {
                // -------------------------------------------------------------
                // Données du paquet
                // -------------------------------------------------------------

                /// Buffer de données inline — évite toute allocation dynamique.
                uint8 data[kNkMaxPacketSize] = {};

                /// Nombre d'octets valides dans le buffer (0 à kNkMaxPacketSize).
                uint32 size = 0;

                // -------------------------------------------------------------
                // Métadonnées de transport
                // -------------------------------------------------------------

                /// Adresse source du paquet (remplie automatiquement à la réception).
                NkAddress from;

                /// Numéro de séquence pour protocole RUDP (gestion des pertes/duplications).
                uint32 seqNum = 0;

                /// Masque de bits ACK pour acquitter les 32 derniers paquets reçus.
                uint32 ackMask = 0;

                /// Canal logique utilisé pour ce paquet (garanties de livraison).
                NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;

                // -------------------------------------------------------------
                // Méthodes utilitaires
                // -------------------------------------------------------------

                /**
                 * @brief Teste si le paquet est vide (aucune donnée utile).
                 * @return true si size == 0.
                 */
                [[nodiscard]] bool IsEmpty() const noexcept;

                /**
                 * @brief Réinitialise le paquet à son état par défaut.
                 * @note Efface size, seqNum et ackMask mais pas le buffer data.
                 */
                void Clear() noexcept;
            };

            // =================================================================
            // CLASSE : NkSocket — Socket UDP/TCP cross-platform
            // =================================================================
            /**
             * @class NkSocket
             * @brief Abstraction unifiée des sockets réseau pour UDP et TCP.
             *
             * Cette classe fournit une interface RAII pour la gestion des sockets :
             *   - Création, configuration et fermeture automatique des ressources
             *   - Support UDP (datagrammes non-connectés) et TCP (flux connectés)
             *   - Options de configuration : non-bloquant, broadcast, Nagle, buffers
             *   - Méthodes d'E/O avec gestion d'erreur unifiée via NkNetResult
             *   - Multiplexing via Select() pour surveillance de multiples sockets
             *
             * @threadsafe Non thread-safe par défaut — protéger avec NkMutex si accès concurrent
             * @note Toujours appeler PlatformInit() une fois au démarrage de l'application
             *
             * @example
             * @code
             * // Initialisation plateforme (une seule fois)
             * NkSocket::PlatformInit();
             *
             * // Création d'un socket UDP serveur
             * NkSocket serverSocket;
             * NkAddress bindAddr = NkAddress::Any(7777);
             * if (serverSocket.Create(bindAddr, NkSocket::Type::UDP) == NkNetResult::OK)
             * {
             *     serverSocket.SetNonBlocking(true);
             *     // Boucle de réception...
             * }
             *
             * // Envoi UDP vers un client
             * NkAddress clientAddr("192.168.1.100", 7777);
             * const char* msg = "Hello";
             * serverSocket.SendTo(msg, 5, clientAddr);
             *
             * // Nettoyage automatique via destructeur
             * // (ou appel explicite à Close() si nécessaire)
             * @endcode
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkSocket
            {
            public:

                // -------------------------------------------------------------
                // Énumération : Type — Protocole de transport
                // -------------------------------------------------------------
                /**
                 * @enum Type
                 * @brief Identifie le protocole de transport utilisé par le socket.
                 */
                enum class Type : uint8
                {
                    NK_UDP = 0,  ///< Datagrammes non-connectés — latence minimale, perte possible
                    NK_TCP       ///< Flux connectés — livraison garantie, ordre préservé
                };

                // -------------------------------------------------------------
                // Constructeurs, destructeur et règles de copie/déplacement
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — socket non-initialisé.
                 * @note IsValid() retournera false jusqu'à appel de Create().
                 */
                NkSocket() noexcept = default;

                /**
                 * @brief Destructeur — ferme automatiquement le socket si ouvert.
                 * @note Garantit la libération des ressources système (RAII).
                 */
                ~NkSocket() noexcept;

                // Suppression de la copie — un socket ne peut être dupliqué implicitement.
                NkSocket(const NkSocket&) = delete;
                NkSocket& operator=(const NkSocket&) = delete;

                /**
                 * @brief Constructeur de déplacement — transfère la propriété du handle.
                 * @param o Socket source dont les ressources sont transférées.
                 * @note Le socket source devient invalide après le déplacement.
                 */
                NkSocket(NkSocket&& o) noexcept;

                /**
                 * @brief Opérateur d'affectation par déplacement.
                 * @param o Socket source dont les ressources sont transférées.
                 * @return Référence vers this pour chaînage.
                 * @note Ferme le socket courant avant d'acquérir les nouvelles ressources.
                 */
                NkSocket& operator=(NkSocket&& o) noexcept;

                // -------------------------------------------------------------
                // Cycle de vie — création et fermeture
                // -------------------------------------------------------------

                /**
                 * @brief Crée et lie un socket sur une adresse locale.
                 * @param localAddr Adresse locale pour le bind (port 0 = auto-assigné par l'OS).
                 * @param type Protocole de transport : UDP (jeux) ou TCP (lobby/HTTP).
                 * @return NkNetResult::OK en cas de succès, code d'erreur sinon.
                 * @note Ferme automatiquement un socket déjà ouvert avant création.
                 * @example
                 * @code
                 * NkSocket sock;
                 * auto result = sock.Create(NkAddress::Any(0), NkSocket::Type::UDP);
                 * if (result == NkNetResult::OK) { /\* Socket prêt *\/ }
                 * @endcode
                 */
                [[nodiscard]] NkNetResult Create(const NkAddress& localAddr, Type type = Type::NK_UDP) noexcept;

                /**
                 * @brief Ferme le socket et libère les ressources système associées.
                 * @note Idempotent — peut être appelé multiples fois sans effet secondaire.
                 * @note Appelée automatiquement par le destructeur.
                 */
                void Close() noexcept;

                // -------------------------------------------------------------
                // Configuration — options du socket
                // -------------------------------------------------------------

                /**
                 * @brief Active/désactive le mode non-bloquant.
                 * @param v true pour mode non-bloquant (recommandé pour game loop).
                 * @return NkNetResult::OK en cas de succès.
                 * @note En mode non-bloquant, Send/Recv retournent immédiatement.
                 * @warning Toujours activer pour les applications interactives/jeux.
                 */
                [[nodiscard]] NkNetResult SetNonBlocking(bool v) noexcept;

                /**
                 * @brief Active/désactive l'envoi de paquets broadcast.
                 * @param v true pour autoriser le broadcast (découverte LAN).
                 * @return NkNetResult::OK en cas de succès.
                 * @note Uniquement applicable aux sockets UDP IPv4.
                 */
                [[nodiscard]] NkNetResult SetBroadcast(bool v) noexcept;

                /**
                 * @brief Active/désactive l'algorithme de Nagle (TCP uniquement).
                 * @param v true pour désactiver Nagle (latence minimale).
                 * @return NkNetResult::OK en cas de succès.
                 * @note Indispensable pour les jeux — évite la mise en buffer des petits paquets.
                 */
                [[nodiscard]] NkNetResult SetNoDelay(bool v) noexcept;

                /**
                 * @brief Configure la taille du buffer d'envoi du système.
                 * @param bytes Taille souhaitée en octets (recommandé: 512KB+).
                 * @return NkNetResult::OK en cas de succès.
                 * @note Valeur indicative — l'OS peut ajuster selon ses limites.
                 */
                [[nodiscard]] NkNetResult SetSendBufferSize(uint32 bytes) noexcept;

                /**
                 * @brief Configure la taille du buffer de réception du système.
                 * @param bytes Taille souhaitée en octets (recommandé: 512KB+).
                 * @return NkNetResult::OK en cas de succès.
                 * @note Important pour absorber les bursts de trafic réseau.
                 */
                [[nodiscard]] NkNetResult SetRecvBufferSize(uint32 bytes) noexcept;

                /**
                 * @brief Active/désactive l'option SO_REUSEADDR.
                 * @param v true pour permettre la réutilisation rapide de l'adresse.
                 * @return NkNetResult::OK en cas de succès.
                 * @note Utile pour redémarrer un serveur rapidement après crash.
                 */
                [[nodiscard]] NkNetResult SetReuseAddr(bool v) noexcept;

                // -------------------------------------------------------------
                // Opérations UDP — envoi/réception de datagrammes
                // -------------------------------------------------------------

                /**
                 * @brief Envoie un datagramme UDP vers une adresse distante.
                 * @param data Pointeur vers les données à envoyer.
                 * @param size Nombre d'octets à envoyer (max: kNkMaxPacketSize).
                 * @param to Adresse de destination du paquet.
                 * @return NkNetResult::OK en cas de succès, code d'erreur sinon.
                 * @note En mode non-bloquant, peut retourner BufferFull si socket saturé.
                 */
                [[nodiscard]] NkNetResult SendTo(
                    const void* data,
                    uint32 size,
                    const NkAddress& to
                ) noexcept;

                /**
                 * @brief Reçoit un datagramme UDP entrant.
                 * @param buf Buffer de réception pour les données.
                 * @param bufSize Taille maximale du buffer en octets.
                 * @param outSize [OUT] Nombre d'octets réellement reçus.
                 * @param outFrom [OUT] Adresse source du paquet reçu.
                 * @return NkNetResult::OK (même si aucun paquet disponible en non-bloquant).
                 * @note En mode non-bloquant, retourne OK avec outSize=0 si rien à lire.
                 */
                [[nodiscard]] NkNetResult RecvFrom(
                    void* buf,
                    uint32 bufSize,
                    uint32& outSize,
                    NkAddress& outFrom
                ) noexcept;

                // -------------------------------------------------------------
                // Opérations TCP — flux connectés
                // -------------------------------------------------------------

                /**
                 * @brief Met le socket en mode écoute pour connexions entrantes.
                 * @param backlog Nombre maximal de connexions en attente dans la file.
                 * @return NkNetResult::OK en cas de succès.
                 * @note Uniquement pour sockets TCP — après Create() et avant Accept().
                 */
                [[nodiscard]] NkNetResult Listen(uint32 backlog = 16) noexcept;

                /**
                 * @brief Initie une connexion TCP vers une adresse distante.
                 * @param remote Adresse du serveur cible.
                 * @return NkNetResult::OK (connexion en cours en non-bloquant).
                 * @note En mode non-bloquant, vérifier la connexion via Select() ou GetLastError().
                 */
                [[nodiscard]] NkNetResult Connect(const NkAddress& remote) noexcept;

                /**
                 * @brief Accepte une connexion entrante sur un socket serveur TCP.
                 * @param outClient [OUT] Socket nouvellement créé pour le client.
                 * @param outAddr [OUT] Adresse du client connecté.
                 * @return NkNetResult::OK si connexion acceptée, OK avec client invalide si aucune en attente (non-bloquant).
                 * @note Le socket retour doit être géré séparément (thread ou Select).
                 */
                [[nodiscard]] NkNetResult Accept(NkSocket& outClient, NkAddress& outAddr) noexcept;

                /**
                 * @brief Envoie des données sur un socket TCP connecté.
                 * @param data Pointeur vers les données à envoyer.
                 * @param size Nombre d'octets à envoyer.
                 * @return NkNetResult::OK en cas de succès.
                 * @note Peut envoyer partiellement — vérifier la valeur de retour pour envoi complet.
                 */
                [[nodiscard]] NkNetResult Send(const void* data, uint32 size) noexcept;

                /**
                 * @brief Reçoit des données sur un socket TCP connecté.
                 * @param buf Buffer de réception pour les données.
                 * @param bufSize Taille maximale du buffer en octets.
                 * @param outSize [OUT] Nombre d'octets réellement reçus.
                 * @return NkNetResult::OK (même si aucun dato disponible en non-bloquant).
                 * @note Peut recevoir partiellement — gérer l'assemblage des fragments si nécessaire.
                 */
                [[nodiscard]] NkNetResult Recv(void* buf, uint32 bufSize, uint32& outSize) noexcept;

                // -------------------------------------------------------------
                // Inspection — état du socket
                // -------------------------------------------------------------

                /**
                 * @brief Teste si le socket est valide et opérationnel.
                 * @return true si le handle est différent de kNkInvalidSocket.
                 */
                [[nodiscard]] bool IsValid() const noexcept;

                /**
                 * @brief Retourne le type de protocole du socket.
                 * @return Type::UDP ou Type::TCP selon la création.
                 */
                [[nodiscard]] Type GetType() const noexcept;

                /**
                 * @brief Retourne l'adresse locale sur laquelle le socket est lié.
                 * @return Référence constante vers l'adresse de bind.
                 */
                [[nodiscard]] const NkAddress& GetLocalAddr() const noexcept;

                /**
                 * @brief Retourne le dernier code d'erreur système pour ce socket.
                 * @return Code d'erreur plateforme-specific (WSAGetLastError/errno).
                 * @note Utiliser pour debugging — préférer NkNetResult pour la logique métier.
                 */
                [[nodiscard]] int32 GetLastError() const noexcept;

                // -------------------------------------------------------------
                // Utilitaires statiques — gestion plateforme et multiplexing
                // -------------------------------------------------------------

                /**
                 * @brief Initialise le sous-système réseau de la plateforme.
                 * @return NkNetResult::OK en cas de succès.
                 * @note Windows : appelle WSAStartup — autres plateformes : no-op.
                 * @warning Appeler exactement une fois au démarrage de l'application.
                 */
                static NkNetResult PlatformInit() noexcept;

                /**
                 * @brief Libère les ressources du sous-système réseau plateforme.
                 * @note Windows : appelle WSACleanup — autres plateformes : no-op.
                 * @warning Appeler exactement une fois à l'arrêt de l'application.
                 */
                static void PlatformShutdown() noexcept;

                /**
                 * @brief Attend des données sur un ensemble de sockets (multiplexing).
                 * @param readSockets Span de pointeurs vers sockets à surveiller en lecture.
                 * @param timeoutMs Délai d'attente en millisecondes (0=immédiat, UINT32_MAX=infini).
                 * @param outReady [OUT] Vecteur recevant les indices des sockets prêts.
                 * @return NkNetResult::OK en cas de succès, TIMEOUT si délai écoulé sans activité.
                 * @note Alternative légère à epoll/kqueue/IOCP pour portabilité maximale.
                 * @example
                 * @code
                 * NkVector<NkSocket*> sockets = {&sock1, &sock2, &sock3};
                 * NkVector<uint32> ready;
                 * if (NkSocket::Select(sockets, 16, ready) == NkNetResult::OK)
                 * {
                 *     for (uint32 idx : ready) {
                 *         /\* Traiter sockets[idx] *\/
                 *     }
                 * }
                 * @endcode
                 */
                static NkNetResult Select(
                    NkSpan<NkSocket*> readSockets,
                    uint32 timeoutMs,
                    NkVector<uint32>& outReady
                ) noexcept;

            private:

                // -------------------------------------------------------------
                // Membres privés — état interne du socket
                // -------------------------------------------------------------

                /// Handle natif du socket (SOCKET sous Windows, int sous POSIX).
                NkNativeSocketHandle mHandle = kNkNativeInvalidSocket;

                /// Type de protocole : UDP ou TCP.
                Type mType = Type::NK_UDP;

                /// Adresse locale sur laquelle le socket est lié.
                NkAddress mLocalAddr;

                /// Indicateur du mode non-bloquant pour gestion d'erreur adaptée.
                bool mNonBlocking = false;
            };

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKSOCKET_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKSOCKET.H
// =============================================================================
// Ce fichier fournit l'abstraction socket cross-platform pour NKNetwork.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Initialisation et création d'un socket UDP serveur
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkSocket.h"
    #include "NKCore/Logger/NkLogger.h"

    void StartUdpServer()
    {
        using namespace nkentseu::net;

        // Initialisation plateforme (une seule fois au démarrage)
        if (NkSocket::PlatformInit() != NkNetResult::OK)
        {
            NK_LOG_ERROR("Échec d'initialisation du sous-système réseau");
            return;
        }

        // Création du socket serveur UDP
        NkSocket serverSocket;
        NkAddress bindAddr = NkAddress::Any(7777);  // Écoute sur toutes les interfaces

        if (serverSocket.Create(bindAddr, NkSocket::Type::UDP) != NkNetResult::OK)
        {
            NK_LOG_ERROR("Échec de création du socket serveur");
            return;
        }

        // Configuration recommandée pour jeu temps réel
        serverSocket.SetNonBlocking(true);           // Mode non-bloquant
        serverSocket.SetSendBufferSize(1024 * 1024); // Buffer 1MB pour envoi
        serverSocket.SetRecvBufferSize(1024 * 1024); // Buffer 1MB pour réception

        NK_LOG_INFO("Serveur UDP démarré sur port 7777");

        // Boucle principale de réception (simplifiée)
        uint8 buffer[kNkMaxPacketSize];
        NkAddress clientAddr;
        uint32 receivedSize = 0;

        while (true) // Remplacer par condition d'arrêt réelle
        {
            NkNetResult result = serverSocket.RecvFrom(
                buffer,
                sizeof(buffer),
                receivedSize,
                clientAddr
            );

            if (result == NkNetResult::OK && receivedSize > 0)
            {
                // Traitement du paquet reçu
                NK_LOG_INFO("Reçu {} octets depuis {}", receivedSize, clientAddr.ToString());

                // Écho vers l'expéditeur (exemple)
                serverSocket.SendTo(buffer, receivedSize, clientAddr);
            }
            else if (result != NkNetResult::OK)
            {
                NK_LOG_WARN("Erreur de réception : {}", NkNetResultStr(result));
            }

            // Pause courte pour éviter le busy-wait (à remplacer par Select en prod)
            PlatformThread::Sleep(1);
        }

        // Nettoyage automatique via destructeur, ou appel explicite :
        // serverSocket.Close();
        // NkSocket::PlatformShutdown(); // À l'arrêt de l'application
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Client UDP avec envoi vers serveur distant
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkSocket.h"

    void UdpClientExample()
    {
        using namespace nkentseu::net;

        NkSocket::PlatformInit();

        // Création socket client UDP (port auto-assigné)
        NkSocket client;
        if (client.Create(NkAddress::Any(0), NkSocket::Type::UDP) != NkNetResult::OK)
        {
            return; // Gestion d'erreur...
        }

        client.SetNonBlocking(true);

        // Adresse du serveur cible
        NkAddress serverAddr("192.168.1.100", 7777);

        // Envoi d'un message
        const char* message = "PING";
        if (client.SendTo(message, 4, serverAddr) == NkNetResult::OK)
        {
            NK_LOG_INFO("Message envoyé au serveur");
        }

        // Attente de réponse (simplifiée — utiliser Select en prod)
        uint8 response[kNkMaxPacketSize];
        uint32 responseSize = 0;
        NkAddress responder;

        if (client.RecvFrom(response, sizeof(response), responseSize, responder) == NkNetResult::OK)
        {
            if (responseSize > 0)
            {
                NK_LOG_INFO("Réponse reçue : {}", NkString(reinterpret_cast<char*>(response), responseSize));
            }
        }

        client.Close();
        NkSocket::PlatformShutdown();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Serveur TCP avec acceptation de multiples clients
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkSocket.h"
    #include "NKContainers/Sequential/NkVector.h"

    void TcpServerExample()
    {
        using namespace nkentseu::net;

        NkSocket::PlatformInit();

        // Socket serveur TCP
        NkSocket server;
        if (server.Create(NkAddress::Any(8080), NkSocket::Type::TCP) != NkNetResult::OK)
        {
            return;
        }

        server.SetNonBlocking(true);
        server.SetReuseAddr(true);  // Permet redémarrage rapide
        server.Listen(32);          // File d'attente de 32 connexions

        // Liste des clients connectés
        NkVector<NkSocket> clients;

        // Boucle principale
        while (true)
        {
            // Acceptation des nouvelles connexions
            NkSocket newClient;
            NkAddress clientAddr;

            if (server.Accept(newClient, clientAddr) == NkNetResult::OK && newClient.IsValid())
            {
                newClient.SetNonBlocking(true);
                newClient.SetNoDelay(true);  // Désactive Nagle pour latence minimale

                NK_LOG_INFO("Nouveau client connecté : {}", clientAddr.ToString());
                clients.PushBack(std::move(newClient));
            }

            // Traitement des clients existants (simplifié)
            for (auto& client : clients)
            {
                uint8 buffer[4096];
                uint32 received = 0;

                if (client.Recv(buffer, sizeof(buffer), received) == NkNetResult::OK && received > 0)
                {
                    // Écho des données reçues
                    client.Send(buffer, received);
                }
            }

            PlatformThread::Sleep(1);
        }

        // Nettoyage
        for (auto& client : clients) { client.Close(); }
        server.Close();
        NkSocket::PlatformShutdown();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de Select() pour multiplexing efficace
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkSocket.h"
    #include "NKContainers/Views/NkSpan.h"

    void SelectExample()
    {
        using namespace nkentseu::net;

        NkSocket::PlatformInit();

        // Création de plusieurs sockets à surveiller
        NkSocket sock1, sock2, sock3;
        sock1.Create(NkAddress::Any(9001), NkSocket::Type::UDP);
        sock2.Create(NkAddress::Any(9002), NkSocket::Type::UDP);
        sock3.Create(NkAddress::Any(9003), NkSocket::Type::UDP);

        sock1.SetNonBlocking(true);
        sock2.SetNonBlocking(true);
        sock3.SetNonBlocking(true);

        // Tableau de sockets à surveiller
        NkVector<NkSocket*> socketsToWatch = { &sock1, &sock2, &sock3 };
        NkVector<uint32> readyIndices;

        // Boucle de multiplexing
        while (true)
        {
            readyIndices.Clear();

            NkNetResult result = NkSocket::Select(
                NkSpan<NkSocket*>(socketsToWatch.Data(), socketsToWatch.Size()),
                16,  // Timeout 16ms
                readyIndices
            );

            if (result == NkNetResult::OK)
            {
                for (uint32 idx : readyIndices)
                {
                    NkSocket* sock = socketsToWatch[idx];
                    uint8 buffer[kNkMaxPacketSize];
                    uint32 size = 0;
                    NkAddress from;

                    if (sock->RecvFrom(buffer, sizeof(buffer), size, from) == NkNetResult::OK && size > 0)
                    {
                        NK_LOG_INFO("Données reçues sur socket {}: {} octets", idx, size);
                    }
                }
            }
        }

        // Nettoyage
        sock1.Close(); sock2.Close(); sock3.Close();
        NkSocket::PlatformShutdown();
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Gestion des adresses et résolution DNS
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkSocket.h"

    void AddressExample()
    {
        using namespace nkentseu::net;

        // Construction directe
        NkAddress addr1("192.168.1.50", 12345);
        NkAddress addr2(10, 0, 0, 1, 80);  // IPv4 par composants

        // Adresses spéciales
        NkAddress loopback = NkAddress::Loopback(7777);      // 127.0.0.1:7777
        NkAddress any      = NkAddress::Any(0);              // 0.0.0.0:0 (auto)
        NkAddress broadcast = NkAddress::Broadcast(9999);    // 255.255.255.255:9999

        // Inspection
        if (addr1.IsValid())
        {
            NK_LOG_INFO("Adresse : {}", addr1.ToString());           // "192.168.1.50:12345"
            NK_LOG_INFO("Hôte seul : {}", addr1.HostString());       // "192.168.1.50"
            NK_LOG_INFO("IPv4 : {}, Port : {}", addr1.IsIPv4(), addr1.Port());
        }

        // Résolution DNS (attention : bloquant — à faire en thread dédié)
        auto resolved = NkAddress::Resolve("google.com", 443);
        for (const auto& addr : resolved)
        {
            if (addr.IsValid())
            {
                NK_LOG_INFO("Résolu : {}", addr.ToString());
                // Tenter connexion avec addr...
            }
        }

        // Comparaison et utilisation dans conteneurs
        std::map<NkAddress, UserData> peerData;
        peerData[addr1] = userData;

        if (addr1 == addr2) { /\* Même adresse *\/ }
        if (addr1 < addr2)  { /\* Pour tri *\/ }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Gestion robuste des erreurs réseau
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkSocket.h"
    #include "NKCore/Logger/NkLogger.h"

    NkNetResult SendWithRetry(
        NkSocket& socket,
        const void* data,
        uint32 size,
        const NkAddress& target,
        uint32 maxAttempts = 3
    )
    {
        using namespace nkentseu::net;

        if (!socket.IsValid() || !data || size == 0)
        {
            NK_LOG_ERROR("Paramètres invalides pour SendWithRetry");
            return NkNetResult::NK_NET_INVALID_ARG;
        }

        for (uint32 attempt = 0; attempt < maxAttempts; ++attempt)
        {
            NkNetResult result = socket.SendTo(data, size, target);

            if (result == NkNetResult::NK_NET_OK)
            {
                return NkNetResult::NK_NET_OK;
            }

            if (result == NkNetResult::NK_NET_BUFFER_FULL)
            {
                // Buffer saturé — attendre un peu et réessayer
                NK_LOG_WARN("Buffer saturé, tentative {}/{}", attempt + 1, maxAttempts);
                PlatformThread::Sleep(10 * (attempt + 1));  // Backoff simple
                continue;
            }

            // Erreur non-récupérable
            NK_LOG_ERROR("Échec d'envoi : {} (tentative {}/{})",
                NkNetResultStr(result), attempt + 1, maxAttempts);
            return result;
        }

        NK_LOG_ERROR("Échec après {} tentatives", maxAttempts);
        return NkNetResult::NK_NET_TIMEOUT;
    }
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Initialisation/Nettoyage :
    -------------------------
    - Appeler NkSocket::PlatformInit() UNE FOIS au démarrage (avant tout socket)
    - Appeler NkSocket::PlatformShutdown() UNE FOIS à l'arrêt (après fermeture des sockets)
    - Sous Windows, PlatformInit() appelle WSAStartup — obligatoire

    Mode non-bloquant :
    ------------------
    - Toujours utiliser SetNonBlocking(true) pour les applications interactives
    - En mode non-bloquant, Recv/RecvFrom retournent OK avec size=0 si rien à lire
    - Vérifier toujours la valeur de retour, ne pas supposer que les données sont disponibles

    Gestion des buffers :
    --------------------
    - Utiliser des buffers de taille kNkMaxPacketSize (1400 bytes) pour UDP
    - Configurer SendBufferSize/RecvBufferSize selon le throughput attendu (512KB+)
    - En TCP, gérer l'assemblage des fragments — Send/Recv peuvent être partiels

    Performance :
    ------------
    - Désactiver Nagle avec SetNoDelay(true) pour TCP en temps réel
    - Utiliser Select() plutôt que polling actif pour multiplexing efficace
    - Réutiliser les sockets quand possible plutôt que créer/fermer fréquemment

    Thread-safety :
    --------------
    - NkSocket n'est PAS thread-safe par défaut
    - Protéger l'accès concurrent avec NkMutex si un socket est partagé entre threads
    - Alternative : un thread par socket avec communication via queues thread-safe

    Résolution DNS :
    ---------------
    - NkAddress::Resolve() est BLOQUANTE — ne jamais appeler dans la game loop
    - Exécuter la résolution dans un thread dédié ou au chargement
    - Mettre en cache les résultats pour éviter des résolutions répétées

    Nettoyage RAII :
    ---------------
    - Le destructeur ~NkSocket() appelle automatiquement Close()
    - Préférer la gestion RAII aux appels manuels de Close()
    - En cas de déplacement (move), le socket source devient invalide

    Codes d'erreur :
    ---------------
    - Privilégier NkNetResult pour la logique métier (portable, lisible)
    - Utiliser GetLastError() uniquement pour debugging plateforme-specific
    - Toujours logger les erreurs avec contexte pour facilité de diagnostic
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================