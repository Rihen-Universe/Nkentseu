// =============================================================================
// FICHIER  : Modules/System/NKNetwork/src/NKNetwork/NkNetDefines.h
// MODULE   : NKNetwork
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Types fondamentaux, macros et détection de plateforme pour NKNetwork.
//   Abstraction cross-platform des sockets, identifiants réseau, et canaux
//   de communication avec garanties configurables.
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKNETDEFINES_H
#define NKENTSEU_NETWORK_NKNETDEFINES_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis par NKNetwork dans l'ordre de dépendance.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKPlatform/NkPlatformDetect.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/NkAtomic.h"
    #include "NKLogger/NkLog.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKThreading/NkMutex.h"
    #include "NKThreading/NkThread.h"

    // En-têtes standards C/C++ pour opérations bas-niveau
    #include <cstring>
    #include <cstdint>

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉTECTION ET CONFIGURATION DE PLATEFORME
    // -------------------------------------------------------------------------
    // Définition des macros de plateforme via NKPlatformDetect.h et inclusion
    // des headers système spécifiques.

    // =====================================================================
    // SOUS-SECTION 2.1 : Windows — Winsock2 API
    // =====================================================================
    #if defined(NKENTSEU_PLATFORM_WINDOWS)

        // Réduction de l'empreinte des headers Windows pour compilation plus rapide
        #define WIN32_LEAN_AND_MEAN

        // Headers Winsock2 pour sockets Windows
        #include <winsock2.h>
        #include <ws2tcpip.h>

        /// Type opaque pour handle de socket Windows.
        using NkSocketHandle = SOCKET;

        /// Valeur invalide pour socket Windows.
        static constexpr NkSocketHandle kNkInvalidSocket = INVALID_SOCKET;

        /// Macro de fermeture de socket pour Windows.
        #define NK_NET_CLOSE_SOCKET(s) closesocket(s)

        /// Code d'erreur socket pour Windows.
        #define NK_NET_SOCKET_ERRORS    SOCKET_ERROR

        // Liaison automatique avec la bibliothèque Winsock2
        #pragma comment(lib, "ws2_32.lib")

    // =====================================================================
    // SOUS-SECTION 2.2 : WebAssembly — Emscripten WebSocket API
    // =====================================================================
    #elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

        // Header Emscripten pour API WebSocket
        // Note : UDP natif non disponible dans les navigateurs
        #include <emscripten/websocket.h>

        /// Type opaque pour handle de socket Web (WebSocket ID).
        using NkSocketHandle = int;

        /// Valeur invalide pour socket Web.
        static constexpr NkSocketHandle kNkInvalidSocket = -1;

        /// Macro de fermeture de socket pour Web (code 1000 = normal closure).
        #define NK_NET_CLOSE_SOCKET(s) emscripten_websocket_close(s, 1000, "")

    // =====================================================================
    // SOUS-SECTION 2.3 : POSIX — Linux/macOS/Android/iOS
    // =====================================================================
    #else

        // Headers POSIX pour sockets Unix-like
        #include <sys/socket.h>
        #include <sys/types.h>
        #include <netinet/in.h>
        #include <netinet/tcp.h>
        #include <arpa/inet.h>
        #include <unistd.h>
        #include <fcntl.h>
        #include <netdb.h>
        #include <errno.h>

        /// Type opaque pour handle de socket POSIX (descripteur de fichier).
        using NkSocketHandle = int;

        /// Valeur invalide pour socket POSIX.
        static constexpr NkSocketHandle kNkInvalidSocket = -1;

        /// Macro de fermeture de socket pour POSIX.
        #define NK_NET_CLOSE_SOCKET(s) ::close(s)

        /// Code d'erreur socket pour POSIX.
        #define NK_NET_SOCKET_ERRORS    -1

    #endif // Platform detection

    // -------------------------------------------------------------------------
    // SECTION 3 : VERSION DU MODULE NKNETWORK
    // -------------------------------------------------------------------------
    // Numérotation sémantique pour gestion des compatibilités API.

    /// Version majeure : changements incompatibles avec les versions précédentes.
    #define NKNET_VERSION_MAJOR 1

    /// Version mineure : nouvelles fonctionnalités rétro-compatibles.
    #define NKNET_VERSION_MINOR 0

    /// Version patch : corrections de bugs rétro-compatibles.
    #define NKNET_VERSION_PATCH 0

    // -------------------------------------------------------------------------
    // SECTION 4 : CONSTANTES ET LIMITES DU SYSTÈME RÉSEAU
    // -------------------------------------------------------------------------
    // Valeurs configurables pour tuning des performances et contraintes réseau.

    /// Nombre maximal de connexions simultanées supportées par instance.
    static constexpr nkentseu::uint32 kNkMaxConnections = 256u;

    /// Taille maximale d'un paquet UDP (MTU-safe pour éviter fragmentation IP).
    static constexpr nkentseu::uint32 kNkMaxPacketSize = 1400u;

    /// Taille maximale de payload utile après soustraction des headers réseau.
    static constexpr nkentseu::uint32 kNkMaxPayloadSize = 1380u;

    /// Taille du buffer d'envoi en octets (tuning pour throughput élevé).
    static constexpr nkentseu::uint32 kNkSendBufferSize = 512u * 1024u;

    /// Taille du buffer de réception en octets (tuning pour burst traffic).
    static constexpr nkentseu::uint32 kNkRecvBufferSize = 512u * 1024u;

    /// Nombre maximal de canaux de communication logiques par connexion.
    static constexpr nkentseu::uint32 kNkMaxChannels = 8u;

    /// Intervalle entre deux heartbeats en millisecondes (détection de déconnexion).
    static constexpr nkentseu::uint32 kNkHeartbeatIntervalMs = 250u;

    /// Délai de timeout sans heartbeat avant déconnexion forcée en millisecondes.
    static constexpr nkentseu::uint32 kNkTimeoutMs = 10000u;

    /// Nombre maximal de tentatives de retransmission pour paquets fiables.
    static constexpr nkentseu::uint32 kNkMaxRetransmits = 5u;

    /// Taille de la fenêtre de séquences pour protocole RUDP (contrôle de flux).
    static constexpr nkentseu::uint32 kNkWindowSize = 64u;

    /// Nombre maximal de fragments pour reassembly de gros paquets.
    static constexpr nkentseu::uint32 kNkMaxFragments = 16u;

    /// Taille de l'historique de réplication pour rollback/netcode prédictif.
    static constexpr nkentseu::uint32 kNkReplicationHistorySize = 60u;

    // -------------------------------------------------------------------------
    // SECTION 5 : MACROS DE LOGGING ET ASSERTIONS RÉSEAU
    // -------------------------------------------------------------------------
    // Wrappers autour du logger pour tagging cohérent des messages NKNetwork.

    /// Macro de logging niveau INFO pour NKNetwork.
    #define NK_NET_LOG_INFO(fmt, ...)  logger.Infof("[NKNet] " fmt "\n", ##__VA_ARGS__)

    /// Macro de logging niveau WARNING pour NKNetwork.
    #define NK_NET_LOG_WARN(fmt, ...)  logger.Warnf("[NKNet] " fmt "\n", ##__VA_ARGS__)

    /// Macro de logging niveau ERROR pour NKNetwork.
    #define NK_NET_LOG_ERROR(fmt, ...) logger.Errorf("[NKNet] " fmt "\n", ##__VA_ARGS__)

    /// Macro de logging niveau DEBUG pour NKNetwork (compilée uniquement en debug).
    #define NK_NET_LOG_DEBUG(fmt, ...) logger.Debugf("[NKNet] " fmt "\n", ##__VA_ARGS__)

    /// Macro d'assertion réseau avec logging d'erreur détaillé.
    /// @param cond Condition à vérifier
    /// @param msg Message descriptif en cas d'échec
    #define NK_NET_ASSERT(cond, msg) \
        do { \
            if (!(cond)) { \
                NK_NET_LOG_ERROR("ASSERT FAILED: %s (%s:%d)", msg, __FILE__, __LINE__); \
            } \
        } while(0)

    // -------------------------------------------------------------------------
    // SECTION 6 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module NKNetwork résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {
            struct NkNetworkConfig {
                /// Nombre maximal de connexions simultanées (défaut: 256)
                /// @note Ajuster selon la mémoire disponible et le cas d'usage
                uint32 maxConnections = 256;
                
                /// Taille des buffers socket en octets (défaut: 512KB)
                uint32 socketBufferSize = 512u * 1024u;
                
                /// Timeout de connexion en millisecondes (défaut: 10s)
                uint32 connectionTimeoutMs = 10000u;
                
                /// Intervalle de heartbeat en millisecondes (défaut: 250ms)
                uint32 heartbeatIntervalMs = 250u;
            };
    
            class NkNetworkConfigManager {
                public:
                    /// Configure le module réseau - à appeler AVANT toute initialisation
                    static void Configure(const NkNetworkConfig& cfg) noexcept;
                    
                    /// Retourne la configuration courante
                    [[nodiscard]] static const NkNetworkConfig& Get() noexcept;
                    
                    /// Réinitialise aux valeurs par défaut
                    static void ResetToDefaults() noexcept;
                    
                private:
                    static NkNetworkConfig sConfig;
                    static threading::NkMutex sConfigMutex;
            };

            // =================================================================
            // Énumération : NkNetResult — Codes de retour unifiés
            // =================================================================
            /**
             * @enum NkNetResult
             * @brief Codes de retour standardisés pour toutes les opérations réseau.
             *
             * Cette énumération fournit un vocabulaire commun pour signaler
             * le résultat des opérations réseau, facilitant la gestion d'erreur
             * cohérente à travers le moteur.
             *
             * @note Utiliser NkNetResultStr() pour conversion en chaîne lisible.
             * @see NkNetResultStr()
             */
            enum class NkNetResult : uint8
            {
                NK_NET_OK = 0,                        ///< Opération réussie.
                NK_NET_INVALID_ARG,                   ///< Argument invalide ou null.
                NK_NET_NOT_CONNECTED,                 ///< Opération requiert une connexion active.
                NK_NET_ALREADY_CONNECTED,             ///< Tentative de connexion redondante.
                NK_NET_CONNECTION_REFUSED,            ///< Serveur a rejeté la connexion.
                NK_NET_TIMEOUT,                       ///< Délai d'attente dépassé.
                NK_NET_PACKET_TOO_LARGE,              ///< Paquet dépasse kNkMaxPacketSize.
                NK_NET_SOCKET_ERROR,                  ///< Erreur système au niveau socket.
                NK_NET_BUFFER_FULL,                   ///< Buffer d'envoi/réception saturé.
                NK_NET_NOT_INITIALIZED,               ///< Module réseau non initialisé.
                NK_NET_PLATFORM_UNSUPPORTED,          ///< Fonctionnalité non supportée sur plateforme.
                NK_NET_AUTH_FAILED,                   ///< Échec d'authentification.
                NK_NET_BANNED,                        ///< Pair banni par le serveur.
                NK_NET_UNKNOWN                        ///< Erreur non catégorisée.
            };

            /**
             * @brief Convertit un code NkNetResult en chaîne lisible.
             * @param r Code de résultat à convertir.
             * @return Chaîne statique décrivant le résultat (ne pas libérer).
             * @note Fonction constexpr-friendly pour logging et debugging.
             */
            [[nodiscard]] inline const char* NkNetResultStr(NkNetResult r) noexcept
            {
                switch (r)
                {
                    case NkNetResult::NK_NET_OK:
                        return "OK";
                    case NkNetResult::NK_NET_INVALID_ARG:
                        return "Invalid argument";
                    case NkNetResult::NK_NET_NOT_CONNECTED:
                        return "Not connected";
                    case NkNetResult::NK_NET_ALREADY_CONNECTED:
                        return "Already connected";
                    case NkNetResult::NK_NET_CONNECTION_REFUSED:
                        return "Connection refused";
                    case NkNetResult::NK_NET_TIMEOUT:
                        return "Timeout";
                    case NkNetResult::NK_NET_PACKET_TOO_LARGE:
                        return "Packet too large";
                    case NkNetResult::NK_NET_SOCKET_ERROR:
                        return "Socket error";
                    case NkNetResult::NK_NET_BUFFER_FULL:
                        return "Buffer full";
                    case NkNetResult::NK_NET_NOT_INITIALIZED:
                        return "Not initialized";
                    case NkNetResult::NK_NET_PLATFORM_UNSUPPORTED:
                        return "Platform unsupported";
                    case NkNetResult::NK_NET_AUTH_FAILED:
                        return "Auth failed";
                    case NkNetResult::NK_NET_BANNED:
                        return "Banned";
                    default:
                        return "Unknown";
                }
            }

            // =================================================================
            // Structure : NkPeerId — Identifiant unique de pair réseau
            // =================================================================
            /**
             * @struct NkPeerId
             * @brief Identifiant opaque et unique d'un pair dans une session réseau.
             *
             * Chaque client/serveur connecté reçoit un NkPeerId unique généré
             * par le serveur pour la durée de la session. Cet identifiant est
             * utilisé pour le routage des messages et la gestion des connexions.
             *
             * @note Valeur 0 = invalide, Valeur 1 = serveur (convention)
             * @threadsafe Lecture seule thread-safe après génération
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkPeerId
            {
                // -------------------------------------------------------------
                // Membres publics
                // -------------------------------------------------------------

                /// Valeur numérique opaque de l'identifiant.
                uint64 value = 0;

                // -------------------------------------------------------------
                // Méthodes d'inspection
                // -------------------------------------------------------------

                /// Vérifie si l'identifiant est valide (non-null).
                /// @return true si value != 0, false sinon.
                [[nodiscard]] bool IsValid() const noexcept;

                /// Vérifie si cet identifiant correspond au serveur.
                /// @return true si value == 1 (convention serveur), false sinon.
                [[nodiscard]] bool IsServer() const noexcept;

                // -------------------------------------------------------------
                // Fabriques statiques
                // -------------------------------------------------------------

                /// Retourne un identifiant invalide (value = 0).
                /// @return NkPeerId avec value = 0.
                [[nodiscard]] static NkPeerId Invalid() noexcept;

                /// Retourne l'identifiant réservé au serveur (value = 1).
                /// @return NkPeerId avec value = 1.
                [[nodiscard]] static NkPeerId Server() noexcept;

                /// Génère un nouvel identifiant unique pour un pair client.
                /// @return NkPeerId avec valeur aléatoire/non-collisante.
                /// @note Implémentation dans NkNetDefines.cpp pour contrôle centralisé.
                [[nodiscard]] static NkPeerId Generate() noexcept;

                // -------------------------------------------------------------
                // Opérateurs de comparaison
                // -------------------------------------------------------------

                /// Égalité : comparaison des valeurs numériques.
                /// @param o Autre NkPeerId à comparer.
                /// @return true si les valeurs sont identiques.
                bool operator==(const NkPeerId& o) const noexcept;

                /// Inégalité : inverse de operator==.
                /// @param o Autre NkPeerId à comparer.
                /// @return true si les valeurs diffèrent.
                bool operator!=(const NkPeerId& o) const noexcept;

                /// Ordre partiel : utile pour conteneurs triés (std::map, etc.).
                /// @param o Autre NkPeerId à comparer.
                /// @return true si this->value < o.value.
                bool operator<(const NkPeerId& o) const noexcept;
            };

            // =================================================================
            // Structure : NkNetId — Identifiant d'entité répliquée
            // =================================================================
            /**
             * @struct NkNetId
             * @brief Identifiant réseau stable d'une entité répliquée entre pairs.
             *
             * DIFFÉRENCE CRITIQUE avec NkEntityId (ECS local) :
             *   • NkEntityId = index local + génération, unique dans un monde ECS
             *   • NkNetId    = identifiant global de session, cohérent sur tous les pairs
             *
             * FORMAT INTERNE :
             *   • Bits 0-31  : Compteur global d'entités (4 milliards max par session)
             *   • Bits 32-39 : Owner simplifié (peerId & 0xFF, 255 propriétaires max)
             *   • Bits 40-63 : Réservés pour extensions futures
             *
             * @note Cet identifiant persiste même si l'entité change de NkEntityId local.
             * @see NkEntityId Pour identifiants ECS locaux
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkNetId
            {
                // -------------------------------------------------------------
                // Membres publics
                // -------------------------------------------------------------

                /// Compteur global d'entités réseau (unique par session).
                uint32 id = 0;

                /// Identifiant simplifié du propriétaire (0 = serveur).
                uint8 owner = 0;

                // -------------------------------------------------------------
                // Méthodes d'inspection
                // -------------------------------------------------------------

                /// Vérifie si l'identifiant est valide (id != 0).
                /// @return true si id != 0, false sinon.
                [[nodiscard]] bool IsValid() const noexcept;

                /// Retourne un identifiant invalide (id = 0, owner = 0).
                /// @return NkNetId avec tous les champs à zéro.
                [[nodiscard]] static NkNetId Invalid() noexcept;

                // -------------------------------------------------------------
                // Sérialisation / Désérialisation
                // -------------------------------------------------------------

                /// Packe l'identifiant en uint64 pour transmission réseau.
                /// @return Valeur 64-bit : (owner << 32) | id.
                /// @note Format little-endian pour cohérence cross-platform.
                [[nodiscard]] uint64 Pack() const noexcept;

                /// Dépacke un uint64 reçu en NkNetId.
                /// @param v Valeur 64-bit reçue du réseau.
                /// @return NkNetId reconstruit.
                /// @see Pack() Pour format de sérialisation.
                [[nodiscard]] static NkNetId Unpack(uint64 v) noexcept;

                // -------------------------------------------------------------
                // Opérateurs de comparaison
                // -------------------------------------------------------------

                /// Égalité : comparaison des deux champs (id + owner).
                /// @param o Autre NkNetId à comparer.
                /// @return true si id et owner sont identiques.
                bool operator==(const NkNetId& o) const noexcept;

                /// Inégalité : inverse de operator==.
                /// @param o Autre NkNetId à comparer.
                /// @return true si id ou owner diffèrent.
                bool operator!=(const NkNetId& o) const noexcept;
            };

            // =================================================================
            // Type alias : NkTimestampMs — Timestamp réseau en millisecondes
            // =================================================================
            /**
             * @typedef NkTimestampMs
             * @brief Timestamp réseau précis en millisecondes depuis epoch de session.
             *
             * Contrairement à std::chrono ou time_t, ce timestamp est relatif
             * au démarrage de la session réseau pour éviter les problèmes de
             * synchronisation d'horloge entre clients et serveur.
             *
             * @note Utiliser NkNetNowMs() pour obtenir le timestamp courant.
             * @see NkNetNowMs()
             */
            using NkTimestampMs = uint64;

            /**
             * @brief Obtient le timestamp réseau courant en millisecondes.
             * @return Nombre de millisecondes depuis le début de la session.
             * @note Implémentation plateforme-specific dans NkNetDefines.cpp.
             * @threadsafe Appel thread-safe (lecture d'horloge monotone).
             */
            [[nodiscard]] NkTimestampMs NkNetNowMs() noexcept;

            // =================================================================
            // Énumération : NkNetChannel — Canaux de communication
            // =================================================================
            /**
             * @enum NkNetChannel
             * @brief Canal logique de communication avec garanties de livraison configurables.
             *
             * Inspiré des approches ENet / GameNetworkingSockets / RakNet :
             * chaque canal offre un compromis différent entre latence, fiabilité
             * et ordre de livraison pour adapter le transport au type de donnée.
             *
             * @note Le canal System est réservé au moteur : ne pas l'utiliser en gameplay.
             * @see NkNetResult Pour codes de retour des opérations par canal
             */
            enum class NkNetChannel : uint8
            {
                /// UDP pur : aucune garantie, latence minimale.
                /// Usage : positions, inputs, animations — données éphémères.
                NK_NET_CHANNEL_UNRELIABLE = 0,

                /// TCP-like : garantit livraison + ordre d'arrivée.
                /// Usage : chat, événements gameplay, commandes critiques.
                NK_NET_CHANNEL_RELIABLE_ORDERED,

                /// Garantit livraison mais pas l'ordre.
                /// Usage : transferts de fichiers, chunks de données indépendants.
                NK_NET_CHANNEL_RELIABLE_UNORDERED,

                /// Séquencé : seul le paquet le plus récent est livré.
                /// Usage : états continus (health, mana) — les anciens sont obsolètes.
                NK_NET_CHANNEL_SEQUENCED,

                /// Réservé au moteur : handshake, heartbeat, disconnect.
                /// Usage interne uniquement — comportement non défini si utilisé.
                NK_NET_CHANNEL_SYSTEM
            };

            // =================================================================
            // Helpers : Byte Order — Conversion host/network endian
            // =================================================================
            /**
             * @defgroup ByteOrderHelpers Conversion Endian Host ↔ Network
             * @brief Fonctions inline pour conversion de byte-order réseau.
             *
             * Le réseau utilise le big-endian (network byte order) tandis que
             * la plupart des CPU modernes utilisent le little-endian.
             * Ces helpers assurent la portabilité des données binaires.
             *
             * @note Sur plateformes big-endian natives, ces fonctions sont no-op.
             * @see htons, ntohs, htonl, ntohl (POSIX)
             */
            /// @{

            /**
             * @brief Convertit uint16 de host-order à network-order.
             * @param v Valeur 16-bit en host byte order.
             * @return Valeur convertie en network byte order (big-endian).
             */
            inline uint16 NkHToN16(uint16 v) noexcept;

            /**
             * @brief Convertit uint16 de network-order à host-order.
             * @param v Valeur 16-bit en network byte order.
             * @return Valeur convertie en host byte order.
             */
            inline uint16 NkNToH16(uint16 v) noexcept;

            /**
             * @brief Convertit uint32 de host-order à network-order.
             * @param v Valeur 32-bit en host byte order.
             * @return Valeur convertie en network byte order (big-endian).
             */
            inline uint32 NkHToN32(uint32 v) noexcept;

            /**
             * @brief Convertit uint32 de network-order à host-order.
             * @param v Valeur 32-bit en network byte order.
             * @return Valeur convertie en host byte order.
             */
            inline uint32 NkNToH32(uint32 v) noexcept;

            /**
             * @brief Convertit uint64 de host-order à network-order.
             * @param v Valeur 64-bit en host byte order.
             * @return Valeur convertie en network byte order (big-endian).
             * @note Implémentation logicielle : pas de htonl64 standard POSIX.
             */
            inline uint64 NkHToN64(uint64 v) noexcept;

            /**
             * @brief Convertit uint64 de network-order à host-order.
             * @param v Valeur 64-bit en network byte order.
             * @return Valeur convertie en host byte order.
             * @note Symétrique de NkHToN64().
             */
            inline uint64 NkNToH64(uint64 v) noexcept;

            /// @} End of ByteOrderHelpers group

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKNETDEFINES_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKNETDEFINES.H
// =============================================================================
// Ce fichier fournit les types fondamentaux et macros pour le module NKNetwork.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Gestion des codes de retour réseau
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Core/NkNetDefines.h"
    #include "NKCore/Logger/NkLogger.h"

    void HandleNetworkOperation(nkentseu::net::NkNetResult result)
    {
        using namespace nkentseu::net;

        switch (result)
        {
            case NkNetResult::NK_NET_OK:
                NK_LOG_INFO("Opération réseau réussie");
                break;

            case NkNetResult::NK_NET_TIMEOUT:
                NK_LOG_WARN("Timeout détecté : {}", NkNetResultStr(result));
                // Logique de reconnexion...
                break;

            case NkNetResult::NK_NET_CONNECTION_REFUSED:
                NK_LOG_ERROR("Connexion refusée par le serveur");
                break;

            default:
                NK_LOG_ERROR("Erreur réseau : {}", NkNetResultStr(result));
                break;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des identifiants de pair
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Core/NkNetDefines.h"

    void PeerManagementExample()
    {
        using namespace nkentseu::net;

        // Création d'identifiants
        NkPeerId serverId = NkPeerId::Server();      // value = 1
        NkPeerId invalidId = NkPeerId::Invalid();    // value = 0
        NkPeerId clientId = NkPeerId::Generate();    // valeur unique aléatoire

        // Vérifications
        if (serverId.IsServer())
        {
            // Logique spécifique serveur
        }

        if (clientId.IsValid())
        {
            // Utilisation de l'identifiant client
        }

        // Comparaison dans des conteneurs
        std::map<NkPeerId, UserData> peerData;
        peerData[clientId] = userData;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Identifiants d'entités répliquées
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Core/NkNetDefines.h"

    void ReplicationExample()
    {
        using namespace nkentseu::net;

        // Création d'un identifiant d'entité réseau
        NkNetId entityId;
        entityId.id = 42;
        entityId.owner = 1;  // Appartient au serveur

        // Sérialisation pour envoi réseau
        uint64 packed = entityId.Pack();
        // Envoi de 'packed' sur le réseau...

        // Désérialisation à la réception
        NkNetId received = NkNetId::Unpack(packed);

        if (received.IsValid() && received.owner == NkPeerId::Server().value)
        {
            // Entité appartenant au serveur — traitement spécifique
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Canaux de communication avec garanties différentes
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Core/NkNetDefines.h"

    void ChannelUsageExample()
    {
        using namespace nkentseu::net;

        // Position du joueur : canal Unreliable (latence minimale)
        SendPacket(playerPosition, NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);

        // Message de chat : canal ReliableOrdered (garanti + ordonné)
        SendPacket(chatMessage, NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);

        // Transfert de fichier : canal ReliableUnordered (garanti, ordre non critique)
        SendPacket(fileChunk, NkNetChannel::NK_NET_CHANNEL_RELIABLE_UNORDERED);

        // État de santé : canal Sequenced (seul le plus récent compte)
        SendPacket(healthUpdate, NkNetChannel::NK_NET_CHANNEL_SEQUENCED);

        // Note : NkNetChannel::NK_NET_CHANNEL_SYSTEM est réservé au moteur interne
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Conversion de byte-order pour portabilité réseau
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Core/NkNetDefines.h"
    #include <cstring>

    void SerializeNetworkPacket(void* buffer, uint16_t port, uint32_t sequence)
    {
        using namespace nkentseu::net;

        uint8_t* ptr = static_cast<uint8_t*>(buffer);

        // Conversion host → network byte order avant envoi
        uint16_t portNet = NkHToN16(port);
        uint32_t seqNet = NkHToN32(sequence);

        // Copie dans le buffer réseau
        std::memcpy(ptr, &portNet, sizeof(portNet));
        ptr += sizeof(portNet);
        std::memcpy(ptr, &seqNet, sizeof(seqNet));
    }

    void DeserializeNetworkPacket(const void* buffer, uint16_t& port, uint32_t& sequence)
    {
        using namespace nkentseu::net;

        const uint8_t* ptr = static_cast<const uint8_t*>(buffer);

        // Lecture depuis le buffer
        uint16_t portNet, seqNet;
        std::memcpy(&portNet, ptr, sizeof(portNet));
        ptr += sizeof(portNet);
        std::memcpy(&seqNet, ptr, sizeof(seqNet));

        // Conversion network → host byte order après réception
        port = NkNToH16(portNet);
        sequence = NkNToH32(seqNet);
    }
*/

// -----------------------------------------------------------------------------
// Notes d'utilisation et bonnes pratiques
// -----------------------------------------------------------------------------
/*
    Gestion des erreurs réseau :
    ---------------------------
    - Toujours vérifier le retour NkNetResult des opérations réseau
    - Utiliser NkNetResultStr() pour logging lisible des erreurs
    - Implémenter une logique de retry pour Timeout/SocketError

    Identifiants de pair :
    --------------------
    - NkPeerId::Server() retourne toujours value = 1 (convention)
    - NkPeerId::Generate() doit être appelé côté serveur uniquement
    - Les clients ne doivent jamais générer leurs propres PeerId

    Identifiants d'entités :
    -----------------------
    - NkNetId est stable pendant toute la session réseau
    - Ne pas confondre avec NkEntityId (ECS local, peut changer)
    - Le champ 'owner' permet de router les messages vers le bon pair

    Canaux de communication :
    ------------------------
    - Choisir le canal en fonction du compromis latence/fiabilité requis
    - Unreliable : données éphémères où la perte est acceptable
    - ReliableOrdered : données critiques où l'ordre importe
    - Sequenced : états continus où seul le dernier compte

    Byte-order et portabilité :
    --------------------------
    - Toujours convertir vers network byte order avant envoi
    - Toujours convertir vers host byte order après réception
    - Les fonctions NkHToN*\/NkNToH* sont no-op sur big-endian natif

    Thread-safety :
    --------------
    - Les types de ce header sont thread-safe en lecture seule
    - NkPeerId::Generate() et NkNetNowMs() peuvent avoir un état interne
    - Protéger l'accès concurrent avec NkMutex si nécessaire
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================