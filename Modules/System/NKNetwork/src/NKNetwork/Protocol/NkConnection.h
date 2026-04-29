// =============================================================================
// NKNetwork/Protocol/NkConnection.h
// =============================================================================
// DESCRIPTION :
//   Gestion des connexions réseau avec machine à états complète pour
//   l'établissement, le maintien et la fermeture gracieuse des sessions.
//
// MACHINE À ÉTATS DE CONNEXION :
//   • NK_CONNECTION_DISCONNECTED  : État initial / après fermeture
//   • NK_CONNECTION_SYN_SENT      : Client : SYN envoyé, attente SYN-ACK
//   • NK_CONNECTION_SYN_RECEIVED  : Serveur : SYN reçu, SYN-ACK envoyé
//   • NK_CONNECTION_ESTABLISHED   : Connexion active, échange de données
//   • NK_CONNECTION_DISCONNECTING : Déconnexion gracieuse en cours (FIN)
//   • NK_CONNECTION_TIMED_OUT     : Délai d'inactivité dépassé
//
// PROTOCOLE DE HANDSHAKE (3-way simplifié) :
//   1. Client → Serveur : SYN (challenge + version + PeerId local)
//   2. Serveur → Client : SYN-ACK (challenge réponse + token session)
//   3. Client → Serveur : ACK (confirmation finale)
//   → Connexion établie des deux côtés, état NK_CONNECTION_ESTABLISHED
//
// MÉCANISME DE HEARTBEAT :
//   • Envoi périodique de PING toutes les kNkHeartbeatIntervalMs (250ms)
//   • Détection de déconnexion si aucune réponse en kNkTimeoutMs (10s)
//   • Mise à jour automatique de mLastActivityAt à chaque paquet reçu
//
// MODÈLE DE THREADING :
//   • NkConnectionManager : thread-safe via NkMutex sur la map des connexions
//   • NkConnection individuelle : utilisée depuis un seul thread (réseau)
//   • Callbacks (OnReceive, OnConnect, OnDisconnect) : appelés depuis thread réseau
//   • Utiliser NkGameplayEventBus pour différer vers le thread jeu si nécessaire
//
// DÉPENDANCES :
//   • NkReliableUDP.h    : Couche RUDP pour fiabilité du transport
//   • NKThreading/NkMutex : Protection des accès concurrents aux structures partagées
//   • NkNetDefines.h     : Types fondamentaux et constantes réseau
//
// RÈGLES D'UTILISATION :
//   • Initialiser via InitAsServer() ou InitAsClient() avant tout usage
//   • Appeler Update(dt) chaque frame depuis le thread réseau
//   • Vérifier IsConnected() avant tout envoi de données applicatives
//   • Utiliser DrainReceived() pour récupérer les messages prêts à traitement
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKCONNECTION_H
#define NKENTSEU_NETWORK_NKCONNECTION_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Transport/NkReliableUDP.h"
    #include "NKNetwork/Protocol/NkBitStream.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKThreading/NkMutex.h"
    #include "NKThreading/NkThread.h"
    #include "NKCore/NkAtomic.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // =================================================================
            // ÉNUMÉRATION : NkConnectionState — Machine à états de connexion
            // =================================================================
            /**
             * @enum NkConnectionState
             * @brief États possibles d'une connexion réseau dans son cycle de vie.
             *
             * Cette énumération définit la machine à états complète pour gérer :
             *   • L'établissement de connexion via handshake en 3 étapes
             *   • Le maintien de la connexion via heartbeats périodiques
             *   • La fermeture gracieuse ou forcée selon les conditions
             *
             * TRANSITIONS D'ÉTAT TYPIQUES :
             *   Disconnected → SynSent (client envoie SYN)
             *   SynSent → Established (réception SYN-ACK + envoi ACK)
             *   Established → Disconnecting (appel à Disconnect())
             *   Disconnecting → Disconnected (réception FIN ou timeout)
             *   Any → TimedOut (aucune activité pendant kNkTimeoutMs)
             *
             * @note Utiliser NkConnStateStr() pour conversion en chaîne lisible
             * @see NkConnStateStr() Pour logging et debugging
             */
            enum class NkConnectionState : uint8
            {
                /// État initial : aucune connexion active.
                /// Transition : vers SynSent (Connect()) ou SynReceived (SYN reçu).
                NK_CONNECTION_DISCONNECTED = 0,

                /// Client : SYN envoyé, en attente de SYN-ACK du serveur.
                /// Timeout : retour à Disconnected si pas de réponse en kNkTimeoutMs.
                NK_CONNECTION_SYN_SENT,

                /// Serveur : SYN reçu d'un client, SYN-ACK envoyé, attente ACK final.
                /// Timeout : retour à Disconnected si pas d'ACK en kNkTimeoutMs.
                NK_CONNECTION_SYN_RECEIVED,

                /// Connexion établie : handshake terminé, échange de données possible.
                /// État stable : maintien via heartbeats, fermeture via Disconnect().
                NK_CONNECTION_ESTABLISHED,

                /// Déconnexion gracieuse en cours : envoi de FIN, attente d'acquittement.
                /// Transition automatique vers Disconnected après confirmation ou timeout.
                NK_CONNECTION_DISCONNECTING,

                /// Délai d'inactivité dépassé : aucune réponse aux heartbeats.
                /// État terminal : nécessite ré-initialisation pour nouvelle connexion.
                NK_CONNECTION_TIMED_OUT
            };

            /**
             * @brief Convertit un état de connexion en chaîne lisible pour logging.
             * @param s État à convertir.
             * @return Chaîne statique décrivant l'état (ne pas libérer).
             * @note Fonction constexpr-friendly pour affichage et debugging.
             */
            [[nodiscard]] inline const char* NkConnStateStr(NkConnectionState s) noexcept
            {
                switch (s)
                {
                    case NkConnectionState::NK_CONNECTION_DISCONNECTED:
                        return "Disconnected";
                    case NkConnectionState::NK_CONNECTION_SYN_SENT:
                        return "SynSent";
                    case NkConnectionState::NK_CONNECTION_SYN_RECEIVED:
                        return "SynReceived";
                    case NkConnectionState::NK_CONNECTION_ESTABLISHED:
                        return "Established";
                    case NkConnectionState::NK_CONNECTION_DISCONNECTING:
                        return "Disconnecting";
                    case NkConnectionState::NK_CONNECTION_TIMED_OUT:
                        return "TimedOut";
                    default:
                        return "Unknown";
                }
            }

            // =================================================================
            // STRUCTURE : NkConnectionStats — Statistiques de connexion
            // =================================================================
            /**
             * @struct NkConnectionStats
             * @brief Métriques de performance et qualité pour une connexion active.
             *
             * Ces statistiques sont mises à jour en temps réel et accessibles
             * en lecture seule pour monitoring, debugging et adaptation dynamique :
             *   • Volume de données : bytes/packets sent/received
             *   • Fiabilité : retransmissions, perte de paquets estimée
             *   • Latence : RTT moyen, jitter (variation), ping arrondi
             *   • Durée : timestamp de connexion pour sessions longues
             *
             * @note Toutes les valeurs sont lissées pour éviter les fluctuations brutales
             * @note Lecture thread-safe après mise à jour (pas de protection interne)
             * @see NkConnection::Stats() Pour accès aux métriques d'une connexion
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkConnectionStats
            {
                // -------------------------------------------------------------
                // Métriques de volume — trafic total depuis l'établissement
                // -------------------------------------------------------------

                /// Nombre total d'octets envoyés sur cette connexion.
                uint64 bytesSent = 0;

                /// Nombre total d'octets reçus sur cette connexion.
                uint64 bytesReceived = 0;

                /// Nombre total de paquets envoyés (inclut retransmissions).
                uint64 packetsSent = 0;

                /// Nombre total de paquets reçus et validés.
                uint64 packetsReceived = 0;

                /// Nombre total de retransmissions effectuées (indicateur de perte).
                uint64 retransmits = 0;

                // -------------------------------------------------------------
                // Métriques de qualité — estimation en temps réel
                // -------------------------------------------------------------

                /// RTT moyen estimé en millisecondes (lissage exponentiel).
                float32 rttMs = 0.f;

                /// Jitter estimé en millisecondes (variation du RTT).
                float32 jitterMs = 0.f;

                /// Taux de perte estimé [0.0 à 1.0] (moyenne glissante).
                float32 packetLoss = 0.f;

                // -------------------------------------------------------------
                // Métriques temporelles — durée et activité
                // -------------------------------------------------------------

                /// Timestamp d'établissement de la connexion (session start).
                NkTimestampMs connectedSince = 0;
            };

            // =================================================================
            // STRUCTURE : NkReceiveMsg — Message reçu prêt à consommation
            // =================================================================
            /**
             * @struct NkReceiveMsg
             * @brief Conteneur de message réseau reçu et validé, prêt pour l'application.
             *
             * Cette structure est produite par NkConnection::DrainReceived() et
             * NkConnectionManager::DrainAll() pour livraison au code applicatif :
             *   • Buffer inline : pas d'allocation heap pour performance
             *   • Métadonnées complètes : source, canal, timestamp pour routing
             *   • Taille utile : nombre d'octets valides dans le buffer data[]
             *
             * @note Taille maximale : kNkMaxPayloadSize (1380 bytes) après retrait de l'en-tête RUDP
             * @note Les messages non-fiables peuvent arriver dans le désordre — gérer la déduplication si nécessaire
             * @see NkConnection::DrainReceived() Pour extraction depuis une connexion spécifique
             * @see NkConnectionManager::DrainAll() Pour extraction globale côté serveur
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkReceiveMsg
            {
                // -------------------------------------------------------------
                // Données du message — payload applicatif
                // -------------------------------------------------------------

                /// Buffer inline contenant les données reçues (sans en-têtes réseau).
                uint8 data[kNkMaxPayloadSize] = {};

                /// Nombre d'octets valides dans le buffer (0 à kNkMaxPayloadSize).
                uint32 size = 0;

                // -------------------------------------------------------------
                // Métadonnées de routage et de timing
                // -------------------------------------------------------------

                /// Identifiant du pair ayant envoyé ce message.
                NkPeerId from;

                /// Canal logique utilisé pour la réception (garanties de livraison).
                NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;

                /// Timestamp de réception en millisecondes depuis début de session.
                NkTimestampMs receivedAt = 0;
            };

            // =================================================================
            // CLASSE : NkConnection — Gestion d'une connexion peer-to-peer
            // =================================================================
            /**
             * @class NkConnection
             * @brief Représente une connexion réseau établie avec un pair distant.
             *
             * Cette classe encapsule l'état complet d'une connexion :
             *   • Machine à états pour gestion du cycle de vie (handshake → data → close)
             *   • Couche RUDP intégrée pour fiabilité configurable par canal
             *   • File d'attente des messages entrants pour découplage thread réseau/jeu
             *   • Statistiques en temps réel pour monitoring et adaptation dynamique
             *   • Callbacks pour notification d'événements (connect/disconnect)
             *
             * CYCLE DE VIE TYPIQUE (côté client) :
             * @code
             * // 1. Création et initialisation
             * NkConnection conn;
             * conn.InitAsClient(&socket, serverAddr, localPeerId);
             *
             * // 2. Démarrage du handshake
             * if (conn.Connect() == NkNetResult::NK_NET_OK) {
             *     // Handshake en cours...
             * }
             *
             * // 3. Boucle principale (thread réseau)
             * void NetworkThread() {
             *     while (conn.GetState() != NK_CONNECTION_DISCONNECTED) {
             *         // a) Mise à jour des timers et retransmissions
             *         conn.Update(dt);
             *
             *         // b) Envoi de données applicatives
             *         if (conn.IsConnected()) {
             *             conn.Send(playerInput, sizeof(playerInput),
             *                      NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
             *         }
             *
             *         // c) Réception des messages entrants
             *         NkVector<NkReceiveMsg> received;
             *         conn.DrainReceived(received);
             *         for (const auto& msg : received) {
             *             ProcessGameMessage(msg.from, msg.data, msg.size);
             *         }
             *     }
             * }
             *
             * // 4. Fermeture (gracieuse ou forcée)
             * conn.Disconnect("User requested");  // ou conn.ForceDisconnect();
             * @endcode
             *
             * @note Non thread-safe pour les méthodes publiques — appeler depuis le thread réseau dédié
             * @note Les callbacks sont invoqués depuis le thread réseau — utiliser un event bus pour le thread jeu
             * @see NkConnectionManager Pour la gestion centralisée de multiples connexions
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkConnection
            {
            public:

                // -------------------------------------------------------------
                // Constructeur, destructeur et règles de copie
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise une connexion inactive.
                 * @note GetState() retournera NK_CONNECTION_DISCONNECTED jusqu'à InitAs*().
                 */
                NkConnection() noexcept = default;

                /**
                 * @brief Destructeur — appelle ForceDisconnect() si encore connecté.
                 * @note Garantit la libération des ressources réseau même en cas d'oubli.
                 */
                ~NkConnection() noexcept;

                // Suppression de la copie — une connexion ne peut être dupliquée.
                NkConnection(const NkConnection&) = delete;
                NkConnection& operator=(const NkConnection&) = delete;

                // -------------------------------------------------------------
                // Initialisation — configuration avant handshake
                // -------------------------------------------------------------

                /**
                 * @brief Initialise la connexion pour un rôle serveur (après accept).
                 * @param socket Pointeur vers le socket UDP partagé (non transféré).
                 * @param remote Adresse du client distant pour l'envoi des paquets.
                 * @param localId Identifiant du serveur dans cette session.
                 * @param remoteId Identifiant attribué au client pour cette connexion.
                 * @note Appelé par NkConnectionManager après accept() réussi.
                 * @note La connexion démarre en état NK_CONNECTION_SYN_RECEIVED.
                 */
                void InitAsServer(
                    NkSocket* socket,
                    const NkAddress& remote,
                    NkPeerId localId,
                    NkPeerId remoteId
                ) noexcept;

                /**
                 * @brief Initialise la connexion pour un rôle client (avant connexion).
                 * @param socket Pointeur vers le socket UDP partagé (non transféré).
                 * @param remote Adresse du serveur cible pour l'envoi du SYN.
                 * @param localId Identifiant du client dans cette session.
                 * @note Appelé par NkConnectionManager::Connect() côté client.
                 * @note La connexion démarre en état NK_CONNECTION_DISCONNECTED, prête pour Connect().
                 */
                void InitAsClient(
                    NkSocket* socket,
                    const NkAddress& remote,
                    NkPeerId localId
                ) noexcept;

                // -------------------------------------------------------------
                // Cycle de vie — établissement et fermeture de connexion
                // -------------------------------------------------------------

                /**
                 * @brief Démarre le handshake client : envoie SYN et passe à SynSent.
                 * @return NkNetResult::NK_NET_OK si SYN envoyé, code d'erreur sinon.
                 * @note Uniquement valide côté client après InitAsClient().
                 * @note La connexion passe à NK_CONNECTION_SYN_SENT en cas de succès.
                 */
                [[nodiscard]] NkNetResult Connect() noexcept;

                /**
                 * @brief Initie une déconnexion gracieuse : envoie FIN et attend acquittement.
                 * @param reason Message optionnel expliquant la raison de la déconnexion.
                 * @return NkNetResult::NK_NET_OK si FIN envoyé, code d'erreur sinon.
                 * @note La connexion passe à NK_CONNECTION_DISCONNECTING.
                 * @note Timeout automatique vers Disconnected si pas de réponse.
                 */
                [[nodiscard]] NkNetResult Disconnect(const char* reason = nullptr) noexcept;

                /**
                 * @brief Force la déconnexion immédiate sans échange de paquets.
                 * @note Utiliser en cas d'erreur critique ou fermeture d'urgence.
                 * @note La connexion passe directement à NK_CONNECTION_DISCONNECTED.
                 * @note Aucun paquet de fin n'est envoyé — le pair détectera par timeout.
                 */
                void ForceDisconnect() noexcept;

                // -------------------------------------------------------------
                // Mise à jour — traitement périodique des timers et événements
                // -------------------------------------------------------------

                /**
                 * @brief Traite les retransmissions, heartbeats et timeouts.
                 * @param dt Delta-time depuis le dernier appel en secondes.
                 * @note À appeler chaque frame depuis le thread réseau dédié.
                 * @note Envoie automatiquement les PING si heartbeat timer écoulé.
                 * @note Détecte les timeouts d'inactivité et déclenche déconnexion.
                 */
                void Update(float32 dt) noexcept;

                // -------------------------------------------------------------
                // Interface d'envoi — transmission de données applicatives
                // -------------------------------------------------------------

                /**
                 * @brief Envoie des données brutes sur le canal logique spécifié.
                 * @param data Pointeur vers le buffer contenant les données à envoyer.
                 * @param size Taille des données en bytes (max : kNkMaxPayloadSize × kNkMaxFragments).
                 * @param channel Canal logique avec garanties de livraison souhaitées.
                 * @return NkNetResult::NK_NET_OK en cas d'envoi réussi, code d'erreur sinon.
                 * @note Si size > kNkMaxPayloadSize : fragmentation automatique via RUDP.
                 * @note Pour canaux fiables : stockage en attente d'ACK avant envoi réel.
                 * @note Thread-safe : protection interne si compilé avec NKNET_THREAD_SAFE.
                 */
                [[nodiscard]] NkNetResult Send(
                    const uint8* data,
                    uint32 size,
                    NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
                ) noexcept;

                /**
                 * @brief Envoie un objet sérialisable via NkBitStream.
                 * @tparam T Type de l'objet à envoyer (doit implémenter NetSerialize()).
                 * @param obj Référence constante vers l'objet à sérialiser et envoyer.
                 * @param ch Canal logique à utiliser pour l'envoi.
                 * @return NkNetResult::NK_NET_OK en cas de succès, code d'erreur sinon.
                 * @note Utilise un buffer stack temporaire de taille kNkMaxPayloadSize.
                 * @note L'objet doit sérialiser < kNkMaxPayloadSize bytes pour éviter l'échec.
                 * @example
                 * @code
                 * struct PlayerMove { uint32 id; float32 x, y, z; };
                 * // Avec méthode : void NetSerialize(NkBitWriter& w) const;
                 * conn.SendObject(playerMove, NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
                 * @endcode
                 */
                template<typename T>
                [[nodiscard]] NkNetResult SendObject(
                    const T& obj,
                    NkNetChannel ch = NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
                ) noexcept;

                // -------------------------------------------------------------
                // Interface de réception — traitement des données entrantes
                // -------------------------------------------------------------

                /**
                 * @brief Traite un datagramme UDP brut reçu du réseau.
                 * @param data Pointeur vers le buffer contenant le datagramme complet.
                 * @param size Taille totale du datagramme en bytes.
                 * @note Valide et traite les paquets système (SYN, ACK, PING, FIN) en interne.
                 * @note Les paquets de données applicatives sont enqueue dans mIncomingQueue.
                 * @note Appelée automatiquement par NkConnectionManager::DispatchPacket().
                 */
                void OnRawReceived(const uint8* data, uint32 size) noexcept;

                /**
                 * @brief Extrait les messages reçus prêts à consommation applicative.
                 * @param out Vecteur de sortie recevant les messages livrables.
                 * @note Vide la file interne après extraction pour éviter les doublons.
                 * @note Les messages sont dans l'ordre de réception (pour canaux Ordered).
                 * @note À appeler après Update() pour traiter les nouveaux messages.
                 */
                void DrainReceived(NkVector<NkReceiveMsg>& out) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — inspection de l'état et des métriques
                // -------------------------------------------------------------

                /**
                 * @brief Retourne l'état courant de la machine à états.
                 * @return Valeur de l'énumération NkConnectionState.
                 */
                [[nodiscard]] NkConnectionState GetState() const noexcept;

                /**
                 * @brief Teste si la connexion est établie et prête à l'échange de données.
                 * @return true si GetState() == NK_CONNECTION_ESTABLISHED.
                 */
                [[nodiscard]] bool IsConnected() const noexcept;

                /**
                 * @brief Retourne l'identifiant du pair distant dans cette connexion.
                 * @return NkPeerId du client (côté serveur) ou du serveur (côté client).
                 */
                [[nodiscard]] NkPeerId GetRemotePeerId() const noexcept;

                /**
                 * @brief Retourne l'identifiant local dans cette connexion.
                 * @return NkPeerId du serveur (côté serveur) ou du client (côté client).
                 */
                [[nodiscard]] NkPeerId GetLocalPeerId() const noexcept;

                /**
                 * @brief Retourne l'adresse réseau du pair distant.
                 * @return Référence constante vers l'adresse configurée.
                 */
                [[nodiscard]] const NkAddress& GetRemoteAddr() const noexcept;

                /**
                 * @brief Retourne les statistiques de performance de la connexion.
                 * @return Référence constante vers la structure NkConnectionStats.
                 * @note Lecture seule — les valeurs sont mises à jour en interne.
                 */
                [[nodiscard]] const NkConnectionStats& Stats() const noexcept;

                /**
                 * @brief Retourne l'estimation courante du RTT vers le pair distant.
                 * @return RTT moyen en millisecondes (délégation vers NkReliableUDP).
                 */
                [[nodiscard]] float32 GetRTT() const noexcept;

                /**
                 * @brief Retourne le ping estimé en millisecondes (arrondi de GetRTT()).
                 * @return Ping entier en ms pour affichage UI ou logging.
                 */
                [[nodiscard]] uint32 GetPingMs() const noexcept;

                /**
                 * @brief Retourne le timestamp de la dernière activité réseau.
                 * @return Timestamp en millisecondes depuis début de session.
                 * @note Utiliser pour détection de déconnexion par timeout d'inactivité.
                 */
                [[nodiscard]] NkTimestampMs LastActivityAt() const noexcept;

                // -------------------------------------------------------------
                // Callbacks — notification d'événements de connexion
                // -------------------------------------------------------------

                /**
                 * @typedef ConnectCb
                 * @brief Signature des callbacks de connexion établie.
                 * @param peer Identifiant du pair avec qui la connexion vient d'être établie.
                 * @note Appelée une fois quand la connexion passe à NK_CONNECTION_ESTABLISHED.
                 */
                using ConnectCb = NkFunction<void(NkPeerId peer)>;

                /**
                 * @typedef DisconnectCb
                 * @brief Signature des callbacks de déconnexion.
                 * @param peer Identifiant du pair qui vient de se déconnecter.
                 * @param reason Message optionnel expliquant la raison de la déconnexion.
                 * @note Appelée quand la connexion passe à NK_CONNECTION_DISCONNECTED.
                 */
                using DisconnectCb = NkFunction<void(NkPeerId peer, const char* reason)>;

                /// Callback invoqué quand la connexion est établie avec succès.
                ConnectCb onConnected;

                /// Callback invoqué quand la connexion est fermée (gracieuse ou erreur).
                DisconnectCb onDisconnected;

            private:

                // -------------------------------------------------------------
                // Méthodes privées — gestion du handshake en 3 étapes
                // -------------------------------------------------------------

                /// Envoie un paquet SYN pour initier le handshake (côté client).
                /// @return NkNetResult::NK_NET_OK si envoyé, code d'erreur sinon.
                [[nodiscard]] NkNetResult SendSYN() noexcept;

                /// Envoie un paquet SYN-ACK en réponse à un SYN reçu (côté serveur).
                /// @return NkNetResult::NK_NET_OK si envoyé, code d'erreur sinon.
                [[nodiscard]] NkNetResult SendSYNACK() noexcept;

                /// Envoie un paquet ACK final pour compléter le handshake (côté client).
                /// @return NkNetResult::NK_NET_OK si envoyé, code d'erreur sinon.
                [[nodiscard]] NkNetResult SendACK() noexcept;

                /// Traite un paquet SYN reçu : valide et répond par SYN-ACK.
                /// @param data Buffer contenant le paquet SYN brut.
                /// @param size Taille du paquet en bytes.
                void ProcessSYN(const uint8* data, uint32 size) noexcept;

                /// Traite un paquet SYN-ACK reçu : valide et complète le handshake.
                /// @param data Buffer contenant le paquet SYN-ACK brut.
                /// @param size Taille du paquet en bytes.
                void ProcessSYNACK(const uint8* data, uint32 size) noexcept;

                // -------------------------------------------------------------
                // Méthodes privées — gestion des heartbeats et keep-alive
                // -------------------------------------------------------------

                /// Envoie un paquet PING pour mesurer le RTT et maintenir la connexion.
                void SendPing() noexcept;

                /// Traite un paquet PING reçu : répond par PONG avec timestamp.
                /// @param data Buffer contenant le paquet PING brut.
                /// @param size Taille du paquet en bytes.
                void ProcessPing(const uint8* data, uint32 size) noexcept;

                /// Traite un paquet PONG reçu : met à jour l'estimateur RTT.
                /// @param data Buffer contenant le paquet PONG brut.
                /// @param size Taille du paquet en bytes.
                void ProcessPong(const uint8* data, uint32 size) noexcept;

                // -------------------------------------------------------------
                // Membres privés — état interne de la connexion
                // -------------------------------------------------------------

                /// État courant de la machine à états de connexion.
                NkConnectionState mState = NkConnectionState::NK_CONNECTION_DISCONNECTED;

                /// Identifiant du pair local dans cette session.
                NkPeerId mLocalPeerId;

                /// Identifiant du pair distant dans cette session.
                NkPeerId mRemotePeerId;

                /// Adresse réseau du pair distant pour l'envoi des paquets.
                NkAddress mRemoteAddr;

                /// Couche RUDP gérant la fiabilité du transport pour cette connexion.
                NkReliableUDP mRUDP;

                /// Statistiques de performance mises à jour en temps réel.
                NkConnectionStats mStats;

                /// Timestamp de la dernière activité réseau (pour détection de timeout).
                NkTimestampMs mLastActivityAt = 0;

                /// Timestamp de la tentative de connexion initiale (pour timeout handshake).
                NkTimestampMs mConnectAttemptAt = 0;

                /// Timer accumulé pour envoi périodique de heartbeats (en secondes).
                float32 mHeartbeatTimer = 0.f;

                /// Timer accumulé pour détection de timeout d'inactivité (en secondes).
                float32 mTimeoutTimer = 0.f;

                /// File d'attente des messages entrants prêts à livraison applicative.
                NkVector<NkReceiveMsg> mIncomingQueue;

                /// Mutex protégeant l'accès concurrent à mIncomingQueue.
                threading::NkMutex mQueueMutex;

                /// Challenge local généré pour le handshake (anti-spoofing).
                uint32 mLocalChallenge = 0;

                /// Challenge reçu du pair distant pour validation croisée.
                uint32 mRemoteChallenge = 0;

                /// Indicateur de rôle : true = serveur, false = client.
                bool mIsServer = false;
            };

            struct NkNetworkMetrics {
                /// Connexions actives / maximales
                uint32 activeConnections = 0;
                uint32 peakConnections = 0;
                
                /// Trafic réseau (octets)
                uint64 bytesSent = 0;
                uint64 bytesReceived = 0;
                
                /// Performance
                float32 avgLatencyMs = 0.f;
                uint32 packetLossPercent = 0;  // 0-100
                
                /// Erreurs
                uint32 connectionErrors = 0;
                uint32 timeoutErrors = 0;
                
                /// Timestamp de dernière mise à jour
                NkTimestampMs lastUpdate = 0;
            };

            // =================================================================
            // CLASSE : NkConnectionManager — Gestion centralisée des connexions
            // =================================================================
            /**
             * @class NkConnectionManager
             * @brief Orchestre l'ensemble des connexions pour un serveur ou un client.
             *
             * MODE SERVEUR :
             *   • StartServer(port) : écoute sur le port et accepte les connexions entrantes
             *   • Chaque SYN reçu : crée une NkConnection et démarre le handshake
             *   • Gestion automatique des déconnexions, timeouts et nettoyage
             *
             * MODE CLIENT :
             *   • Connect(serverAddr) : crée une connexion et initie le handshake
             *   • Gestion de la reconnexion automatique en cas d'échec (optionnel)
             *   • Délégation des envois/réceptions vers la connexion unique
             *
             * MODÈLE DE THREADING :
             *   • Thread I/O dédié (mNetThread) : boucle continue de polling socket
             *     - Réception des datagrammes UDP via recvfrom()
             *     - Dispatch vers la NkConnection correspondante par adresse source
             *     - Appel de NkConnection::Update() pour chaque connexion active
             *   • Thread jeu : accès thread-safe via SendTo(), Broadcast(), DrainAll()
             *     - Protection par mutex sur les structures partagées
             *     - Copy-on-read pour éviter les blocages prolongés
             *
             * USAGE TYPIQUE (serveur) :
             * @code
             * // 1. Initialisation au démarrage
             * NkConnectionManager connMgr;
             * connMgr.StartServer(7777, 64);  // Port 7777, max 64 clients
             *
             * // 2. Boucle réseau dédiée (thread séparé)
             * void NetworkThread() {
             *     while (connMgr.IsRunning()) {
             *         // Polling et traitement automatique dans NetThreadLoop()
             *         // Appelé en continu par le thread interne
             *     }
             * }
             *
             * // 3. Thread jeu : envoi et réception thread-safe
             * void GameLoop() {
             *     // Envoi à un client spécifique
             *     connMgr.SendTo(clientId, gameStateData, dataSize);
             *
             *     // Broadcast à tous les clients (sauf l'expéditeur)
             *     connMgr.Broadcast(playerUpdate, updateSize,
             *                      NkNetChannel::NK_NET_CHANNEL_UNRELIABLE,
             *                      senderId);  // exclude sender
             *
             *     // Réception des messages entrants
             *     NkVector<NkReceiveMsg> received;
             *     connMgr.DrainAll(received);
             *     for (const auto& msg : received) {
             *         HandleClientMessage(msg.from, msg.data, msg.size);
             *     }
             * }
             *
             * // 4. Arrêt propre à la fermeture
             * connMgr.DisconnectAll("Server shutting down");
             * connMgr.Shutdown();
             * @endcode
             *
             * @note Thread-safe pour les méthodes publiques : SendTo, Broadcast, DrainAll, etc.
             * @note Non thread-safe pour les méthodes internes : réservées au thread réseau
             * @see NkConnection Pour la gestion d'une connexion individuelle
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkConnectionManager
            {
            public:

                // -------------------------------------------------------------
                // Constructeur, destructeur et règles de copie
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise un gestionnaire inactif.
                 * @note Appeler StartServer() ou Connect() pour démarrer l'activité réseau.
                 */
                NkConnectionManager() noexcept = default;

                /**
                 * @brief Destructeur — arrête proprement le thread réseau et libère les connexions.
                 * @note Appel automatique à Shutdown() si encore running.
                 */
                ~NkConnectionManager() noexcept;

                // Suppression de la copie — un gestionnaire ne peut être dupliqué.
                NkConnectionManager(const NkConnectionManager&) = delete;
                NkConnectionManager& operator=(const NkConnectionManager&) = delete;

                // -------------------------------------------------------------
                // Initialisation — démarrage en mode serveur ou client
                // -------------------------------------------------------------

                /**
                 * @brief Démarre un serveur écoutant sur le port spécifié.
                 * @param port Numéro de port d'écoute (ex: 7777 pour jeu, 8080 pour web).
                 * @param maxClients Nombre maximal de connexions simultanées acceptées.
                 * @return NkNetResult::NK_NET_OK si écoute démarrée, code d'erreur sinon.
                 * @note Crée un socket UDP et le lie à l'adresse Any(port).
                 * @note Démarre le thread réseau interne pour polling automatique.
                 */
                [[nodiscard]] NkNetResult StartServer(
                    uint16 port,
                    uint32 maxClients = 64
                ) noexcept;

                /**
                 * @brief Démarre un client et initie la connexion vers un serveur.
                 * @param serverAddr Adresse du serveur cible (IP + port).
                 * @param localPort Port local pour le socket client (0 = auto-assigné par l'OS).
                 * @return NkNetResult::NK_NET_OK si connexion initiée, code d'erreur sinon.
                 * @note Crée une NkConnection interne et démarre le handshake.
                 * @note Démarre le thread réseau interne pour polling automatique.
                 */
                [[nodiscard]] NkNetResult Connect(
                    const NkAddress& serverAddr,
                    uint16 localPort = 0
                ) noexcept;

                // -------------------------------------------------------------
                // Arrêt — fermeture propre des ressources réseau
                // -------------------------------------------------------------

                /**
                 * @brief Arrête le gestionnaire et ferme toutes les connexions actives.
                 * @note Envoie une déconnexion gracieuse à tous les pairs connectés.
                 * @note Attend la fin du thread réseau avant retour (join).
                 * @note Libère les ressources socket et mémoire associées.
                 */
                void Shutdown() noexcept;

                // -------------------------------------------------------------
                // Interface d'envoi — transmission vers un ou plusieurs pairs
                // -------------------------------------------------------------

                /**
                 * @brief Envoie des données à un pair spécifique par son identifiant.
                 * @param peer Identifiant du destinataire (NkPeerId valide).
                 * @param data Pointeur vers le buffer contenant les données à envoyer.
                 * @param size Taille des données en bytes.
                 * @param ch Canal logique avec garanties de livraison souhaitées.
                 * @return NkNetResult::NK_NET_OK en cas d'envoi réussi, code d'erreur sinon.
                 * @note Thread-safe : acquisition du mutex de connexion pour lookup.
                 * @note Retourne NK_NET_NOT_CONNECTED si le peer n'est pas trouvé ou non-connecté.
                 */
                [[nodiscard]] NkNetResult SendTo(
                    NkPeerId peer,
                    const uint8* data,
                    uint32 size,
                    NkNetChannel ch = NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
                ) noexcept;

                /**
                 * @brief Broadcast des données à tous les pairs connectés.
                 * @param data Pointeur vers le buffer contenant les données à broadcaster.
                 * @param size Taille des données en bytes.
                 * @param ch Canal logique avec garanties de livraison souhaitées.
                 * @param exclude Identifiant optionnel d'un pair à exclure du broadcast.
                 * @return NkNetResult::NK_NET_OK si tous les envois ont réussi, premier code d'erreur sinon.
                 * @note Thread-safe : itération protégée sur la table des connexions.
                 * @note Utile pour : mises à jour d'état global, événements broadcast, sync temps réel.
                 */
                [[nodiscard]] NkNetResult Broadcast(
                    const uint8* data,
                    uint32 size,
                    NkNetChannel ch = NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED,
                    NkPeerId exclude = NkPeerId::Invalid()
                ) noexcept;

                // -------------------------------------------------------------
                // Interface de réception — extraction des messages entrants
                // -------------------------------------------------------------

                /**
                 * @brief Extrait tous les messages reçus depuis la dernière frame.
                 * @param out Vecteur de sortie recevant l'ensemble des messages livrables.
                 * @note Thread-safe : copie protégée depuis la file globale vers out.
                 * @note Vide la file interne après extraction pour éviter les doublons.
                 * @note À appeler depuis le thread jeu après chaque frame réseau.
                 */
                void DrainAll(NkVector<NkReceiveMsg>& out) noexcept;

                // -------------------------------------------------------------
                // Déconnexion — fermeture ciblée ou globale des connexions
                // -------------------------------------------------------------

                /**
                 * @brief Déconnecte un pair spécifique avec message de raison optionnel.
                 * @param peer Identifiant du pair à déconnecter.
                 * @param reason Message optionnel expliquant la raison de la déconnexion.
                 * @note Envoie un paquet FIN si la connexion est encore active.
                 * @note Libère les ressources de la connexion après confirmation ou timeout.
                 */
                void Disconnect(NkPeerId peer, const char* reason = nullptr) noexcept;

                /**
                 * @brief Déconnecte tous les pairs connectés avec message de raison commun.
                 * @param reason Message optionnel expliquant la raison de la déconnexion globale.
                 * @note Utiliser avant Shutdown() pour fermeture gracieuse du serveur.
                 * @note Itération protégée sur la table des connexions pour thread-safety.
                 */
                void DisconnectAll(const char* reason = nullptr) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — inspection de l'état du gestionnaire
                // -------------------------------------------------------------

                /**
                 * @brief Teste si le gestionnaire fonctionne en mode serveur.
                 * @return true si StartServer() a été appelé avec succès.
                 */
                [[nodiscard]] bool IsServer() const noexcept;

                /**
                 * @brief Teste si le thread réseau est en cours d'exécution.
                 * @return true si IsRunning() == true et thread actif.
                 */
                [[nodiscard]] bool IsRunning() const noexcept;

                /**
                 * @brief Retourne le nombre de pairs actuellement connectés.
                 * @return Compteur de connexions en état NK_CONNECTION_ESTABLISHED.
                 * @note Thread-safe : lecture protégée du compteur interne.
                 */
                [[nodiscard]] uint32 ConnectedPeerCount() const noexcept;

                /**
                 * @brief Retourne l'identifiant local dans cette session réseau.
                 * @return NkPeerId du serveur (mode serveur) ou du client (mode client).
                 */
                [[nodiscard]] NkPeerId GetLocalPeerId() const noexcept;

                /**
                 * @brief Retourne un pointeur vers une connexion par identifiant de pair.
                 * @param peer Identifiant du pair dont on veut obtenir la connexion.
                 * @return Pointeur constant vers la NkConnection si trouvée, nullptr sinon.
                 * @warning Le pointeur peut devenir invalide dès le retour si le thread réseau
                 *          déconnecte ce pair. Utiliser GetConnectionStats() pour un accès safe.
                 * @note Thread-safe pour le lookup — pas de garantie sur la durée de vie du pointeur.
                 */
                [[nodiscard]] const NkConnection* GetConnection(NkPeerId peer) const noexcept;

                /**
                 * @brief Copie thread-safe des statistiques d'une connexion par identifiant de pair.
                 * @param peer     Identifiant du pair dont on veut les métriques.
                 * @param outStats Structure de sortie recevant la copie des statistiques.
                 * @return true si le pair a été trouvé et les stats copiées, false sinon.
                 * @note Préférer cette méthode à GetConnection() pour éviter les use-after-free.
                 * @example
                 * @code
                 * NkConnectionStats stats;
                 * if (connMgr.GetConnectionStats(clientId, stats))
                 *     ShowPing(static_cast<uint32>(stats.rttMs));
                 * @endcode
                 */
                [[nodiscard]] bool GetConnectionStats(
                    NkPeerId peer,
                    NkConnectionStats& outStats
                ) const noexcept;

                // -------------------------------------------------------------
                // Callbacks — notification d'événements globaux de connexion
                // -------------------------------------------------------------

                /// Callback invoqué quand un nouveau pair se connecte au serveur.
                NkConnection::ConnectCb onPeerConnected;

                /// Callback invoqué quand un pair se déconnecte (gracieux ou erreur).
                NkConnection::DisconnectCb onPeerDisconnected;

                // -------------------------------------------------------------
                // Configuration — paramètres ajustables du gestionnaire
                // -------------------------------------------------------------

                /// Nombre maximal de connexions simultanées autorisées.
                /// @note Doit être ≤ kNkMaxConnections (256) défini dans NkNetDefines.h.
                uint32 maxConnections = kNkMaxConnections;

                /// Retourne les métriques réseau courantes (thread-safe, copie)
                // [[nodiscard]] NkNetworkMetrics GetMetrics() const noexcept;
                
                // /// Réinitialise les compteurs de métriques
                // void ResetMetrics() noexcept;

            private:

                mutable threading::NkMutex mMetricsMutex;
                NkNetworkMetrics mMetrics = {};
                
                // Helper pour mise à jour thread-safe
                // void UpdateMetric(uint64 NkConnectionManager::*metric, uint64 delta) noexcept;

                // -------------------------------------------------------------
                // Méthodes privées — boucle réseau et polling socket
                // -------------------------------------------------------------

                /// Boucle principale du thread réseau : polling, dispatch, update.
                /// @note Appelée en continu par mNetThread jusqu'à arrêt via Shutdown().
                void NetThreadLoop() noexcept;

                /// Polling du socket UDP pour réception de datagrammes entrants.
                /// @note Appelle DispatchPacket() pour chaque paquet reçu valide.
                void PollSocket() noexcept;

                /// Mise à jour périodique de toutes les connexions actives.
                /// @param dt Delta-time depuis le dernier appel en secondes.
                /// @note Appelle NkConnection::Update() pour chaque connexion.
                void UpdateConnections(float32 dt) noexcept;

                /// Dispatch d'un paquet brut vers la connexion correspondante.
                /// @param data Buffer contenant le paquet reçu.
                /// @param size Taille du paquet en bytes.
                /// @param from Adresse source du paquet pour lookup de connexion.
                /// @note Crée une nouvelle connexion si SYN reçu et slot disponible.
                void DispatchPacket(
                    const uint8* data,
                    uint32 size,
                    const NkAddress& from
                ) noexcept;

                // -------------------------------------------------------------
                // Méthodes privées — lookup et gestion de la table des connexions
                // -------------------------------------------------------------

                /// Trouve ou crée une connexion pour une adresse source donnée.
                /// @param from       Adresse du pair distant pour identification.
                /// @param outIsNew   Mis à true si la connexion vient d'être créée (callback à déclencher).
                /// @return Pointeur vers la connexion existante ou nouvellement créée.
                /// @note Retourne nullptr si table pleine ou adresse invalide.
                /// @note Thread-safe : création atomique sous mConnMutex (pas de TOCTOU).
                NkConnection* FindOrCreateConnection(const NkAddress& from, bool& outIsNew) noexcept;

                /// Trouve une connexion par identifiant de pair.
                /// @param peer Identifiant du pair à rechercher.
                /// @return Pointeur vers la connexion si trouvée, nullptr sinon.
                NkConnection* FindConnection(NkPeerId peer) noexcept;

                /// Trouve une connexion par adresse réseau.
                /// @param addr Adresse du pair distant pour lookup.
                /// @return Pointeur vers la connexion si trouvée, nullptr sinon.
                NkConnection* FindConnectionByAddr(const NkAddress& addr) noexcept;

                // -------------------------------------------------------------
                // Membres privés — état interne du gestionnaire
                // -------------------------------------------------------------

                /// Socket UDP partagé pour toutes les connexions (serveur ou client).
                NkSocket mSocket;

                /// Identifiant du pair local dans cette session réseau.
                NkPeerId mLocalPeerId;

                /// Indicateur de mode : true = serveur, false = client.
                bool mIsServer = false;

                /// Nombre maximal de connexions gérables (copie de maxConnections).
                static constexpr uint32 kMaxConn = kNkMaxConnections;

                /// Table statique des pointeurs vers connexions actives.
                NkConnection* mConnections[kMaxConn] = {};

                /// Table parallèle des identifiants de pair pour lookup rapide.
                NkPeerId mConnPeerIds[kMaxConn] = {};

                /// Compteur du nombre de connexions actuellement actives.
                uint32 mConnCount = 0;

                /// Mutex protégeant l'accès concurrent à la table des connexions.
                mutable threading::NkMutex mConnMutex;

                /// Thread dédié au polling réseau et traitement des paquets.
                threading::NkThread mNetThread;

                /// Flag atomique de contrôle du thread réseau (start/stop).
                NkAtomic<bool> mRunning{false};

                /// Buffer global consolidé des messages entrants pour DrainAll().
                NkVector<NkReceiveMsg> mGlobalIncoming;

                /// Mutex protégeant l'accès concurrent à mGlobalIncoming.
                threading::NkMutex mIncomingMutex;

                /// Timestamp de la dernière mise à jour complète du gestionnaire.
                NkTimestampMs mLastUpdateAt = 0;
            };

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKCONNECTION_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCONNECTION.H
// =============================================================================
// Ce fichier fournit la gestion des connexions réseau avec machine à états.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Serveur multi-clients avec gestion d'événements
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkConnection.h"
    #include "NKCore/Logger/NkLogger.h"

    class GameServer
    {
    public:
        bool Start(uint16 port, uint32 maxPlayers = 32)
        {
            using namespace nkentseu::net;

            // Configuration des callbacks pour notification d'événements
            mConnMgr.onPeerConnected = [this](NkPeerId peer) {
                NK_LOG_INFO("Joueur connecté : {}", peer.value);
                SendWelcomeMessage(peer);
            };

            mConnMgr.onPeerDisconnected = [this](NkPeerId peer, const char* reason) {
                NK_LOG_INFO("Joueur déconnecté : {} ({})", peer.value, reason ? reason : "sans raison");
                CleanupPlayer(peer);
            };

            // Démarrage du serveur
            const auto result = mConnMgr.StartServer(port, maxPlayers);
            if (result != NkNetResult::NK_NET_OK)
            {
                NK_LOG_ERROR("Échec de démarrage du serveur : {}", NkNetResultStr(result));
                return false;
            }

            NK_LOG_INFO("Serveur démarré sur port {}", port);
            return true;
        }

        void Update(float32 dt)
        {
            // Le thread réseau gère automatiquement polling et updates
            // Ce méthode est appelée depuis le thread jeu pour logique métier

            // Traitement des messages entrants
            NkVector<NkReceiveMsg> received;
            mConnMgr.DrainAll(received);

            for (const auto& msg : received)
            {
                HandleClientMessage(msg.from, msg.data, msg.size);
            }

            // Broadcast de l'état du jeu (ex: positions des joueurs)
            BroadcastGameState();
        }

        void Shutdown()
        {
            mConnMgr.DisconnectAll("Server shutting down");
            mConnMgr.Shutdown();
        }

    private:
        void SendWelcomeMessage(nkentseu::net::NkPeerId peer)
        {
            using namespace nkentseu::net;

            // Envoi d'un message de bienvenue fiable et ordonné
            const char* welcome = "Welcome to the game!";
            mConnMgr.SendTo(
                peer,
                reinterpret_cast<const uint8*>(welcome),
                static_cast<uint32>(std::strlen(welcome)),
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
            );
        }

        void BroadcastGameState()
        {
            using namespace nkentseu::net;

            // Sérialisation de l'état du jeu (exemple simplifié)
            GameState state = GetCurrentGameState();
            uint8 buffer[4096];
            NkBitWriter writer(buffer, sizeof(buffer));
            state.NetSerialize(writer);

            // Broadcast à tous les clients (sauf éventuellement l'expéditeur)
            mConnMgr.Broadcast(
                buffer,
                writer.BytesWritten(),
                NkNetChannel::NK_NET_CHANNEL_UNRELIABLE  // Positions : perte acceptable
            );
        }

        NkConnectionManager mConnMgr;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Client avec reconnexion automatique
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkConnection.h"
    #include "NKCore/Logger/NkLogger.h"

    class GameClient
    {
    public:
        bool ConnectToServer(const char* serverHost, uint16 serverPort)
        {
            using namespace nkentseu::net;

            // Résolution de l'adresse du serveur (bloquant — à faire en thread dédié)
            auto addresses = NkAddress::Resolve(serverHost, serverPort);
            if (addresses.IsEmpty())
            {
                NK_LOG_ERROR("Impossible de résoudre l'adresse : {}", serverHost);
                return false;
            }

            // Tentative de connexion sur la première adresse résolue
            const auto result = mConnMgr.Connect(addresses[0]);
            if (result != NkNetResult::NK_NET_OK)
            {
                NK_LOG_ERROR("Échec de connexion au serveur : {}", NkNetResultStr(result));
                return false;
            }

            // Configuration des callbacks
            mConnMgr.onPeerConnected = [this](NkPeerId) {
                NK_LOG_INFO("Connecté au serveur !");
                mIsConnected = true;
                RequestInitialGameState();
            };

            mConnMgr.onPeerDisconnected = [this](NkPeerId, const char* reason) {
                NK_LOG_WARN("Déconnecté du serveur : {}", reason ? reason : "inconnue");
                mIsConnected = false;
                ScheduleReconnect();
            };

            return true;
        }

        void Update(float32 dt)
        {
            if (!mIsConnected)
            {
                return;  // En attente de connexion ou reconnexion
            }

            // Envoi des inputs joueur (unreliable pour latence minimale)
            SendPlayerInput();

            // Réception des messages serveur
            NkVector<NkReceiveMsg> received;
            mConnMgr.DrainAll(received);

            for (const auto& msg : received)
            {
                HandleServerMessage(msg.data, msg.size);
            }
        }

        void Disconnect()
        {
            mConnMgr.DisconnectAll("Client requested disconnect");
            mConnMgr.Shutdown();
            mIsConnected = false;
        }

    private:
        void ScheduleReconnect()
        {
            // Stratégie de reconnexion avec backoff exponentiel
            static uint32 sReconnectAttempts = 0;
            const uint32 delayMs = std::min(1000u * (1u << sReconnectAttempts), 30000u);

            NK_LOG_INFO("Tentative de reconnexion dans {}ms...", delayMs);

            // Planifier la reconnexion (pseudo-code — adapter à votre système de timers)
            Timer::Schedule(delayMs, [this]() {
                ++sReconnectAttempts;
                ConnectToServer(mServerHost.c_str(), mServerPort);
            });
        }

        bool mIsConnected = false;
        std::string mServerHost;
        uint16 mServerPort = 0;
        NkConnectionManager mConnMgr;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Monitoring et statistiques de connexion
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Protocol/NkConnection.h"
    #include "NKCore/Logger/NkLogger.h"

    void LogConnectionStats(nkentseu::net::NkConnectionManager& connMgr)
    {
        using namespace nkentseu::net;

        NK_LOG_INFO("=== Statistiques de connexion ===");
        NK_LOG_INFO("Mode : {}", connMgr.IsServer() ? "Serveur" : "Client");
        NK_LOG_INFO("Connexions actives : {}/{}",
            connMgr.ConnectedPeerCount(),
            connMgr.maxConnections);

        // Statistiques détaillées par connexion (serveur uniquement)
        if (connMgr.IsServer())
        {
            // Note : accès direct aux connexions nécessite protection mutex
            // Cet exemple est illustratif — adapter selon votre architecture
            for (uint32 i = 0; i < connMgr.maxConnections; ++i)
            {
                const auto* conn = connMgr.GetConnection(NkPeerId{i + 1});  // Exemple
                if (conn && conn->IsConnected())
                {
                    const auto& stats = conn->Stats();
                    NK_LOG_INFO("Peer {}: RTT={:.0f}ms, Perte={:.1f}%, Octets={}/{}",
                        conn->GetRemotePeerId().value,
                        stats.rttMs,
                        stats.packetLoss * 100.f,
                        stats.bytesReceived,
                        stats.bytesSent);
                }
            }
        }
        else
        {
            // Client : statistiques de la connexion unique
            // (implémentation dépend de l'accès à la connexion interne)
        }

        NK_LOG_INFO("==============================");
    }

    // Appel périodique (ex: toutes les 10 secondes)
    void PeriodicStatsUpdate(nkentseu::net::NkConnectionManager& connMgr)
    {
        static NkTimestampMs sLastLog = 0;
        const NkTimestampMs now = nkentseu::net::NkNetNowMs();

        if (now - sLastLog >= 10000)  // 10 secondes
        {
            LogConnectionStats(connMgr);
            sLastLog = now;
        }
    }
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Gestion du cycle de vie :
    ------------------------
    - Toujours initialiser via InitAsServer() ou InitAsClient() avant usage
    - Appeler Update(dt) chaque frame depuis le thread réseau dédié
    - Vérifier IsConnected() avant tout envoi de données applicatives
    - Utiliser Disconnect() pour fermeture gracieuse, ForceDisconnect() pour urgence

    Thread-safety :
    --------------
    - NkConnectionManager est thread-safe pour SendTo/Broadcast/DrainAll
    - NkConnection individuelle : utiliser depuis le thread réseau uniquement
    - Callbacks invoqués depuis thread réseau : différer vers thread jeu via event bus
    - Protéger l'accès aux connexions avec mConnMutex si accès direct requis

    Gestion des erreurs :
    --------------------
    - Toujours vérifier les codes de retour NkNetResult des opérations réseau
    - Implémenter une logique de reconnexion avec backoff exponentiel côté client
    - Logger les déconnexions avec contexte pour diagnostic serveur

    Performance :
    ------------
    - Privilégier NK_NET_CHANNEL_UNRELIABLE pour données fréquentes non-critiques
    - Regrouper les petits messages en un seul paquet si possible (batching)
    - Configurer maxConnections selon la capacité serveur attendue

    Heartbeat et timeout :
    ---------------------
    - kNkHeartbeatIntervalMs = 250ms : équilibre entre réactivité et overhead
    - kNkTimeoutMs = 10000ms : tolérance aux pertes temporaires sans déconnexion prématurée
    - Ajuster selon le type d'application (jeu temps réel vs application métier)

    Sécurité :
    ---------
    - Valider systématiquement les données reçues côté serveur avant traitement
    - Utiliser les challenges du handshake pour anti-spoofing basique
    - Envisager chiffrement TLS/DTLS pour les applications sensibles

    Debugging :
    ----------
    - Activer le logging des états de connexion en développement
    - Utiliser Stats() pour monitoring en temps réel des performances réseau
    - Tester les scénarios de perte/ping élevé avec outils de simulation réseau
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================