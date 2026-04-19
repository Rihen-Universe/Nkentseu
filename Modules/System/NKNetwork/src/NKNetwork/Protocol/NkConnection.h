#pragma once
// =============================================================================
// NKNetwork/Protocol/NkConnection.h
// =============================================================================
// Connexion réseau avec machine à états (Handshake → Connected → Disconnected).
//
// ÉTATS D'UNE CONNEXION :
//   Disconnected → SynSent (client) / SynReceived (serveur)
//   → Established → Disconnecting → Disconnected
//
// HANDSHAKE (3-way style, simplifié) :
//   Client  → SYN (challenge + version + PeerId)
//   Serveur → SYN-ACK (challenge réponse + token de session)
//   Client  → ACK (confirme)
//   → Connexion établie sur les deux côtés
//
// HEARTBEAT :
//   Tous les kNkHeartbeatIntervalMs = PING envoyé
//   Si pas de réponse en kNkTimeoutMs → déconnexion
//
// THREAD SAFETY :
//   NkConnectionManager est thread-safe (NkMutex sur la map des connexions).
//   NkConnection individuelle = utilisée depuis un seul thread (recv thread).
//   Les callbacks (OnReceive, OnConnect, OnDisconnect) sont appelés depuis
//   le thread réseau → utiliser NkGameplayEventBus (deferred) pour le thread jeu.
// =============================================================================

#include "Transport/NkReliableUDP.h"
#include "NKThreading/NkMutex.h"

namespace nkentseu {
    namespace net {

        // =====================================================================
        // NkConnectionState — machine à états d'une connexion
        // =====================================================================
        enum class NkConnectionState : uint8 {
            Disconnected = 0,
            SynSent,         ///< Client : SYN envoyé, attente SYN-ACK
            SynReceived,     ///< Serveur : SYN reçu, SYN-ACK envoyé
            Established,     ///< Connecté et prêt à envoyer des données
            Disconnecting,   ///< Déconnexion gracieuse en cours
            TimedOut,        ///< Délai d'attente dépassé
        };

        [[nodiscard]] inline const char* NkConnStateStr(NkConnectionState s) noexcept {
            switch (s) {
                case NkConnectionState::Disconnected:  return "Disconnected";
                case NkConnectionState::SynSent:       return "SynSent";
                case NkConnectionState::SynReceived:   return "SynReceived";
                case NkConnectionState::Established:   return "Established";
                case NkConnectionState::Disconnecting: return "Disconnecting";
                case NkConnectionState::TimedOut:      return "TimedOut";
                default:                               return "Unknown";
            }
        }

        // =====================================================================
        // NkConnectionStats — statistiques d'une connexion
        // =====================================================================
        struct NkConnectionStats {
            uint64  bytesSent       = 0;
            uint64  bytesReceived   = 0;
            uint64  packetsSent     = 0;
            uint64  packetsReceived = 0;
            uint64  retransmits     = 0;
            float32 rttMs           = 0.f;
            float32 jitterMs        = 0.f;
            float32 packetLoss      = 0.f;  ///< [0..1]
            NkTimestampMs connectedSince = 0;
        };

        // =====================================================================
        // NkReceiveMsg — message reçu d'une connexion
        // =====================================================================
        struct NkReceiveMsg {
            uint8        data[kNkMaxPayloadSize] = {};
            uint32       size    = 0;
            NkPeerId     from;
            NkNetChannel channel = NkNetChannel::Unreliable;
            NkTimestampMs receivedAt = 0;
        };

        // =====================================================================
        // NkConnection — une connexion réseau établie avec un pair
        // =====================================================================
        class NkConnection {
        public:
            NkConnection()  noexcept = default;
            ~NkConnection() noexcept = default;
            NkConnection(const NkConnection&) = delete;
            NkConnection& operator=(const NkConnection&) = delete;

            // ── Cycle de vie ─────────────────────────────────────────────────

            /**
             * @brief Initialise la connexion (côté serveur, après accept).
             */
            void InitAsServer(NkSocket* socket,
                               const NkAddress& remote,
                               NkPeerId localId,
                               NkPeerId remoteId) noexcept;

            /**
             * @brief Initialise la connexion (côté client, avant connexion).
             */
            void InitAsClient(NkSocket* socket,
                               const NkAddress& remote,
                               NkPeerId localId) noexcept;

            /**
             * @brief Démarre le handshake client → envoie SYN.
             */
            NkNetResult Connect() noexcept;

            /**
             * @brief Déconnexion gracieuse → envoie FIN.
             */
            NkNetResult Disconnect(const char* reason = nullptr) noexcept;

            /**
             * @brief Déconnexion immédiate (sans envoi).
             */
            void ForceDisconnect() noexcept;

            // ── Frame update ─────────────────────────────────────────────────
            /**
             * @brief Traitement des timers, retransmissions, heartbeat.
             * À appeler depuis le thread réseau.
             */
            void Update(float32 dt) noexcept;

            // ── Envoi ─────────────────────────────────────────────────────────
            /**
             * @brief Envoie des données sur un canal.
             * @param data    Buffer de données.
             * @param size    Taille (max kNkMaxPayloadSize × kNkMaxFragments).
             * @param channel Canal de communication.
             */
            [[nodiscard]] NkNetResult Send(const uint8* data, uint32 size,
                                            NkNetChannel channel = NkNetChannel::ReliableOrdered) noexcept;

            /**
             * @brief Envoie un objet sérialisé via NkBitStream.
             */
            template<typename T>
            [[nodiscard]] NkNetResult SendObject(const T& obj,
                                                  NkNetChannel ch = NkNetChannel::ReliableOrdered) noexcept {
                uint8 buf[kNkMaxPayloadSize];
                NkBitWriter w(buf, sizeof(buf));
                obj.NetSerialize(w);
                return Send(buf, w.BytesWritten(), ch);
            }

            // ── Réception (appelée par NkConnectionManager) ───────────────────
            /**
             * @brief Feed d'un datagramme brut reçu depuis le socket.
             * Traite les paquets système (SYN, ACK, PING) en interne.
             * Les paquets données sont mis en file dans mIncomingQueue.
             */
            void OnRawReceived(const uint8* data, uint32 size) noexcept;

            /**
             * @brief Retire les messages reçus prêts à consommer.
             */
            void DrainReceived(NkVector<NkReceiveMsg>& out) noexcept;

            // ── Accesseurs ────────────────────────────────────────────────────
            [[nodiscard]] NkConnectionState GetState()    const noexcept { return mState; }
            [[nodiscard]] bool IsConnected()              const noexcept { return mState == NkConnectionState::Established; }
            [[nodiscard]] NkPeerId GetRemotePeerId()      const noexcept { return mRemotePeerId; }
            [[nodiscard]] NkPeerId GetLocalPeerId()       const noexcept { return mLocalPeerId; }
            [[nodiscard]] const NkAddress& GetRemoteAddr()const noexcept { return mRemoteAddr; }
            [[nodiscard]] const NkConnectionStats& Stats()const noexcept { return mStats; }
            [[nodiscard]] float32 GetRTT()                const noexcept { return mRUDP.GetRTT(); }
            [[nodiscard]] uint32  GetPingMs()             const noexcept { return mRUDP.GetPingMs(); }
            [[nodiscard]] NkTimestampMs LastActivityAt()  const noexcept { return mLastActivityAt; }

            // ── Callbacks ─────────────────────────────────────────────────────
            using ConnectCb    = NkFunction<void(NkPeerId peer)>;
            using DisconnectCb = NkFunction<void(NkPeerId peer, const char* reason)>;

            ConnectCb    onConnected;
            DisconnectCb onDisconnected;

        private:
            // ── Handshake ─────────────────────────────────────────────────────
            NkNetResult SendSYN()    noexcept;
            NkNetResult SendSYNACK() noexcept;
            NkNetResult SendACK()    noexcept;
            void        ProcessSYN   (const uint8* data, uint32 size) noexcept;
            void        ProcessSYNACK(const uint8* data, uint32 size) noexcept;

            // ── Heartbeat ─────────────────────────────────────────────────────
            void SendPing() noexcept;
            void ProcessPing(const uint8* data, uint32 size) noexcept;
            void ProcessPong(const uint8* data, uint32 size) noexcept;

            // ── État ──────────────────────────────────────────────────────────
            NkConnectionState   mState      = NkConnectionState::Disconnected;
            NkPeerId            mLocalPeerId;
            NkPeerId            mRemotePeerId;
            NkAddress           mRemoteAddr;
            NkReliableUDP       mRUDP;
            NkConnectionStats   mStats;
            NkTimestampMs       mLastActivityAt = 0;
            NkTimestampMs       mConnectAttemptAt = 0;
            float32             mHeartbeatTimer = 0.f;
            float32             mTimeoutTimer   = 0.f;

            // File des messages entrants
            NkVector<NkReceiveMsg> mIncomingQueue;
            threading::NkMutex     mQueueMutex;

            // Challenge pour le handshake (anti-spoofing)
            uint32  mLocalChallenge  = 0;
            uint32  mRemoteChallenge = 0;
            bool    mIsServer        = false;
        };

        // =====================================================================
        // NkConnectionManager — gestionnaire de toutes les connexions
        // =====================================================================
        /**
         * @class NkConnectionManager
         * @brief Gère l'ensemble des connexions (serveur ou client).
         *
         * SERVEUR :
         *   StartServer(port) → écoute + accepte les connexions entrantes
         *   Chaque nouveau SYN → crée une NkConnection + démarre handshake
         *
         * CLIENT :
         *   Connect(address) → crée une NkConnection + envoie SYN
         *
         * THREAD MODEL :
         *   Un thread I/O dédié (mNetThread) tourne en continu :
         *     - Polling socket UDP (recvfrom)
         *     - Dispatch des paquets aux NkConnection correspondantes
         *     - Appel de NkConnection::Update()
         *   Thread jeu : accède via Send() et DrainAll() (thread-safe)
         */
        class NkConnectionManager {
        public:
            NkConnectionManager()  noexcept = default;
            ~NkConnectionManager() noexcept { Shutdown(); }

            NkConnectionManager(const NkConnectionManager&) = delete;
            NkConnectionManager& operator=(const NkConnectionManager&) = delete;

            // ── Serveur ───────────────────────────────────────────────────────
            /**
             * @brief Démarre un serveur sur le port donné.
             * @param port Port d'écoute (ex: 7777).
             * @param maxClients Nombre max de clients simultanés.
             */
            [[nodiscard]] NkNetResult StartServer(uint16 port,
                                                   uint32 maxClients = 64) noexcept;

            // ── Client ────────────────────────────────────────────────────────
            /**
             * @brief Démarre le client et connecte à un serveur.
             * @param serverAddr Adresse du serveur.
             * @param localPort  Port local (0 = OS choisit).
             */
            [[nodiscard]] NkNetResult Connect(const NkAddress& serverAddr,
                                               uint16 localPort = 0) noexcept;

            // ── Arrêt ─────────────────────────────────────────────────────────
            void Shutdown() noexcept;

            // ── Envoi ─────────────────────────────────────────────────────────
            /**
             * @brief Envoie à un peer spécifique.
             */
            [[nodiscard]] NkNetResult SendTo(NkPeerId peer,
                                              const uint8* data, uint32 size,
                                              NkNetChannel ch = NkNetChannel::ReliableOrdered) noexcept;

            /**
             * @brief Broadcast à tous les peers connectés.
             */
            [[nodiscard]] NkNetResult Broadcast(const uint8* data, uint32 size,
                                                  NkNetChannel ch = NkNetChannel::ReliableOrdered,
                                                  NkPeerId exclude = NkPeerId::Invalid()) noexcept;

            // ── Réception ────────────────────────────────────────────────────
            /**
             * @brief Retire tous les messages reçus depuis la dernière frame.
             * Thread-safe. Appelé depuis le thread jeu.
             */
            void DrainAll(NkVector<NkReceiveMsg>& out) noexcept;

            // ── Déconnexion ───────────────────────────────────────────────────
            void Disconnect(NkPeerId peer, const char* reason = nullptr) noexcept;
            void DisconnectAll(const char* reason = nullptr) noexcept;

            // ── État ──────────────────────────────────────────────────────────
            [[nodiscard]] bool IsServer()  const noexcept { return mIsServer; }
            [[nodiscard]] bool IsRunning() const noexcept;
            [[nodiscard]] uint32 ConnectedPeerCount() const noexcept;
            [[nodiscard]] NkPeerId GetLocalPeerId()  const noexcept { return mLocalPeerId; }

            [[nodiscard]] const NkConnection* GetConnection(NkPeerId peer) const noexcept;

            // ── Callbacks ─────────────────────────────────────────────────────
            NkConnection::ConnectCb    onPeerConnected;
            NkConnection::DisconnectCb onPeerDisconnected;

            // ── Config ────────────────────────────────────────────────────────
            uint32 maxConnections = kNkMaxConnections;

        private:
            // ── Thread réseau ─────────────────────────────────────────────────
            void NetThreadLoop() noexcept;
            void PollSocket() noexcept;
            void UpdateConnections(float32 dt) noexcept;
            void DispatchPacket(const uint8* data, uint32 size,
                                 const NkAddress& from) noexcept;

            NkConnection* FindOrCreateConnection(const NkAddress& from) noexcept;
            NkConnection* FindConnection(NkPeerId peer) noexcept;
            NkConnection* FindConnectionByAddr(const NkAddress& addr) noexcept;

            // ── Données membres ──────────────────────────────────────────────
            NkSocket                  mSocket;
            NkPeerId                  mLocalPeerId;
            bool                      mIsServer  = false;

            // Map PeerId → NkConnection (owned)
            static constexpr uint32   kMaxConn = kNkMaxConnections;
            NkConnection*             mConnections[kMaxConn] = {};
            NkPeerId                  mConnPeerIds[kMaxConn] = {};
            uint32                    mConnCount = 0;
            mutable threading::NkMutex mConnMutex;

            // Thread réseau
            threading::NkThread       mNetThread;
            threading::NkAtomic<bool> mRunning{false};

            // Buffer de paquets entrants consolidés
            NkVector<NkReceiveMsg>    mGlobalIncoming;
            threading::NkMutex        mIncomingMutex;

            NkTimestampMs             mLastUpdateAt = 0;
        };

    } // namespace net
} // namespace nkentseu
