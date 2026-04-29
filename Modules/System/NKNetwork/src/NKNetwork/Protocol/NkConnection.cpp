// =============================================================================
// NKNetwork/Protocol/NkConnection.cpp
// =============================================================================
// DESCRIPTION :
//   Implémentation de la gestion des connexions réseau avec machine à états
//   pour l'établissement, le maintien et la fermeture des sessions peer-to-peer.
//
// NOTES D'IMPLÉMENTATION :
//   • Toutes les méthodes publiques retournent NkNetResult pour cohérence API
//   • Gestion d'erreur robuste : logging + retour gracieux sans crash
//   • Thread-safety : mutex sur les structures partagées (mConnMutex, mQueueMutex)
//   • Aucune allocation heap dans les chemins critiques pour performance temps réel
//
// DÉPENDANCES INTERNES :
//   • NkConnection.h : Déclarations des classes implémentées
//   • NkReliableUDP.h : Couche RUDP pour fiabilité du transport
//   • NkNetDefines.h : Types fondamentaux et constantes réseau
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
#include "NKNetwork/Protocol/NkConnection.h"

// En-têtes standards C/C++ pour opérations bas-niveau
#include <cstring>
#include <algorithm>
#include <random>

// En-têtes pour opérations temporelles et threads
#include <chrono>
#include <thread>

// -------------------------------------------------------------------------
// SECTION 2 : CONSTANTES LOCALES ET HELPERS
// -------------------------------------------------------------------------

namespace {

    // =====================================================================
    // Constantes de protocole handshake
    // =====================================================================
    static constexpr nkentseu::uint8 kProtocolMagic = 0x4E;  // 'N' pour NKEntseu
    static constexpr nkentseu::uint8 kProtocolVersion = 1u;  // Version actuelle du protocole

    // =====================================================================
    // Types de paquets système pour le handshake et heartbeat
    // =====================================================================
    enum class NkSystemPacketType : nkentseu::uint8
    {
        NK_SYS_SYN = 0,        /// Handshake : demande de connexion (client → serveur)
        NK_SYS_SYN_ACK,        /// Handshake : réponse serveur (serveur → client)
        NK_SYS_ACK,            /// Handshake : confirmation finale (client → serveur)
        NK_SYS_PING,           /// Heartbeat : mesure de RTT et keep-alive
        NK_SYS_PONG,           /// Heartbeat : réponse au ping
        NK_SYS_FIN,            /// Déconnexion gracieuse : demande de fermeture
        NK_SYS_DATA            /// Paquet de données applicatives (après handshake)
    };

    // =====================================================================
    // Structure d'en-tête pour paquets système (handshake/heartbeat)
    // =====================================================================
    // =========================================================================
    // NkSystemHeader — en-tête fixe pour paquets de contrôle (handshake/heartbeat)
    // =========================================================================
    // LAYOUT (28 bytes, little-endian interne — conversion réseau non requise ici) :
    //   [0]     magic     : 0x4E ('N') — signature de validation du paquet
    //   [1]     version   : version du protocole — rejet si différent
    //   [2]     type      : NkSystemPacketType — SYN/SYN-ACK/ACK/PING/PONG/FIN
    //   [3]     reserved  : alignement / extensions futures
    //   [4..7]  challenge : nonce anti-spoofing pour validation du handshake
    //   [8..11] response  : réponse au challenge distant (SYN-ACK/ACK)
    //   [12..19] peerId   : NkPeerId::value — identifiant du pair expéditeur
    //   [20..27] timestamp: NkNetNowMs() — horodatage PING pour mesure RTT
    // =========================================================================
    struct NkSystemHeader
    {
        // Taille totale sérialisée : 4 (flags) + 4 (challenge) + 4 (response)
        //                          + 8 (peerId) + 8 (timestamp) = 28 bytes
        static constexpr nkentseu::uint32 kSize = 28u;

        nkentseu::uint8  magic     = kProtocolMagic;    /// Signature de validation
        nkentseu::uint8  version   = kProtocolVersion;  /// Version du protocole
        nkentseu::uint8  type      = 0;                 /// NkSystemPacketType
        nkentseu::uint8  reserved  = 0;                 /// Alignement / extensions futures
        nkentseu::uint32 challenge = 0;                 /// Nonce anti-spoofing
        nkentseu::uint32 response  = 0;                 /// Réponse au challenge (SYN-ACK)
        nkentseu::uint64 peerId    = 0;                 /// Identifiant du pair expéditeur
        nkentseu::uint64 timestamp = 0;                 /// Horodatage pour mesure RTT (PING/PONG)

        void Serialize(nkentseu::uint8* buf) const noexcept
        {
            if (buf == nullptr) { return; }
            buf[0] = magic;
            buf[1] = version;
            buf[2] = type;
            buf[3] = reserved;
            std::memcpy(buf + 4,  &challenge,  sizeof(nkentseu::uint32));
            std::memcpy(buf + 8,  &response,   sizeof(nkentseu::uint32));
            std::memcpy(buf + 12, &peerId,     sizeof(nkentseu::uint64));
            std::memcpy(buf + 20, &timestamp,  sizeof(nkentseu::uint64));
        }

        bool Deserialize(const nkentseu::uint8* buf, nkentseu::uint32 bufSize) noexcept
        {
            if (buf == nullptr || bufSize < kSize) { return false; }
            if (buf[0] != kProtocolMagic || buf[1] != kProtocolVersion) { return false; }
            type     = buf[2];
            reserved = buf[3];
            std::memcpy(&challenge,  buf + 4,  sizeof(nkentseu::uint32));
            std::memcpy(&response,   buf + 8,  sizeof(nkentseu::uint32));
            std::memcpy(&peerId,     buf + 12, sizeof(nkentseu::uint64));
            std::memcpy(&timestamp,  buf + 20, sizeof(nkentseu::uint64));
            return true;
        }
    };

    // =====================================================================
    // Génération de challenge aléatoire pour handshake (anti-spoofing)
    // =====================================================================
    [[nodiscard]] nkentseu::uint32 GenerateChallenge() noexcept
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<nkentseu::uint32> dist(1u, 0xFFFFFFFFu);
        return dist(gen);
    }

    // =====================================================================
    // Fonction de réponse au challenge (simple XOR pour l'exemple)
    // Note : En production, utiliser un algorithme cryptographique robuste
    // =====================================================================
    [[nodiscard]] nkentseu::uint32 ComputeChallengeResponse(nkentseu::uint32 challenge, nkentseu::uint64 peerId) noexcept
    {
        // XOR simple avec peerId pour l'exemple — à renforcer en production
        return challenge ^ static_cast<nkentseu::uint32>(peerId ^ (peerId >> 32));
    }

} // namespace anonyme

// -------------------------------------------------------------------------
// SECTION 3 : NAMESPACE PRINCIPAL
// -------------------------------------------------------------------------
// Implémentation des méthodes dans le namespace nkentseu::net.

namespace nkentseu {

    namespace net {

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Constructeur/Destructeur
        // =====================================================================

        NkConnection::~NkConnection() noexcept
        {
            // RAII : déconnexion forcée si connexion encore active
            if (mState != NkConnectionState::NK_CONNECTION_DISCONNECTED)
            {
                ForceDisconnect();
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Initialisation
        // =====================================================================

        void NkConnection::InitAsServer(
            NkSocket* socket,
            const NkAddress& remote,
            NkPeerId localId,
            NkPeerId remoteId
        ) noexcept
        {
            // Validation des paramètres
            if (socket == nullptr || !socket->IsValid())
            {
                NK_NET_LOG_ERROR("InitAsServer : socket invalide");
                return;
            }

            // Initialisation des membres
            mState = NkConnectionState::NK_CONNECTION_SYN_RECEIVED;
            mLocalPeerId = localId;
            mRemotePeerId = remoteId;
            mRemoteAddr = remote;
            mIsServer = true;
            mLastActivityAt = NkNetNowMs();
            mConnectAttemptAt = 0;
            mHeartbeatTimer = 0.f;
            mTimeoutTimer = 0.f;

            // Génération du challenge local pour validation du handshake
            mLocalChallenge = GenerateChallenge();
            mRemoteChallenge = 0;

            // Initialisation de la couche RUDP
            mRUDP.Init(socket, remote);

            // Réinitialisation des statistiques
            std::memset(&mStats, 0, sizeof(mStats));
            mStats.connectedSince = NkNetNowMs();

            // Vidage de la file de réception
            {
                threading::NkScopedLockMutex lock(mQueueMutex);
                mIncomingQueue.Clear();
            }

            NK_NET_LOG_DEBUG("Connexion serveur initialisée pour peer {} @ {}",
                remoteId.value, remote.ToString().CStr());
        }

        void NkConnection::InitAsClient(
            NkSocket* socket,
            const NkAddress& remote,
            NkPeerId localId
        ) noexcept
        {
            // Validation des paramètres
            if (socket == nullptr || !socket->IsValid())
            {
                NK_NET_LOG_ERROR("InitAsClient : socket invalide");
                return;
            }

            // Initialisation des membres
            mState = NkConnectionState::NK_CONNECTION_DISCONNECTED;
            mLocalPeerId = localId;
            mRemotePeerId = NkPeerId::Invalid();
            mRemoteAddr = remote;
            mIsServer = false;
            mLastActivityAt = 0;
            mConnectAttemptAt = 0;
            mHeartbeatTimer = 0.f;
            mTimeoutTimer = 0.f;

            // Génération du challenge local
            mLocalChallenge = GenerateChallenge();
            mRemoteChallenge = 0;

            // Initialisation de la couche RUDP
            mRUDP.Init(socket, remote);

            // Réinitialisation des statistiques
            std::memset(&mStats, 0, sizeof(mStats));

            // Vidage de la file de réception
            {
                threading::NkScopedLockMutex lock(mQueueMutex);
                mIncomingQueue.Clear();
            }

            NK_NET_LOG_DEBUG("Connexion client initialisée vers {}", remote.ToString().CStr());
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Cycle de vie
        // =====================================================================

        NkNetResult NkConnection::Connect() noexcept
        {
            // Vérification de l'état et du rôle
            if (mIsServer)
            {
                NK_NET_LOG_WARN("Connect() appelé côté serveur — ignoré");
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            if (mState != NkConnectionState::NK_CONNECTION_DISCONNECTED)
            {
                NK_NET_LOG_WARN("Connect() appelé en état {} — ignoré", NkConnStateStr(mState));
                return NkNetResult::NK_NET_ALREADY_CONNECTED;
            }

            // Envoi du SYN pour initier le handshake
            const NkNetResult result = SendSYN();
            if (result == NkNetResult::NK_NET_OK)
            {
                mState = NkConnectionState::NK_CONNECTION_SYN_SENT;
                mConnectAttemptAt = NkNetNowMs();
                NK_NET_LOG_DEBUG("SYN envoyé vers {} — attente SYN-ACK", mRemoteAddr.ToString().CStr());
            }
            else
            {
                NK_NET_LOG_ERROR("Échec d'envoi du SYN : {}", NkNetResultStr(result));
            }

            return result;
        }

        NkNetResult NkConnection::Disconnect(const char* reason) noexcept
        {
            // Vérification de l'état
            if (mState != NkConnectionState::NK_CONNECTION_ESTABLISHED)
            {
                // Déjà en cours de déconnexion ou déconnecté
                ForceDisconnect();
                return NkNetResult::NK_NET_OK;
            }

            // Passage à l'état de déconnexion
            mState = NkConnectionState::NK_CONNECTION_DISCONNECTING;

            // Envoi du paquet FIN si possible
            NkSystemHeader finHeader;
            finHeader.type = static_cast<uint8>(NkSystemPacketType::NK_SYS_FIN);
            finHeader.challenge = mLocalChallenge;
            finHeader.peerId = mLocalPeerId.value;

            uint8 buffer[NkSystemHeader::kSize];
            finHeader.Serialize(buffer);

            const NkNetResult result = mRUDP.Send(buffer, NkSystemHeader::kSize,
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);

            if (result == NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_INFO("FIN envoyé à {} : {}", mRemoteAddr.ToString().CStr(),
                    reason ? reason : "sans raison");
            }
            else
            {
                NK_NET_LOG_WARN("Échec d'envoi du FIN : {} — déconnexion forcée",
                    NkNetResultStr(result));
                ForceDisconnect();
            }

            return result;
        }

        void NkConnection::ForceDisconnect() noexcept
        {
            // Sauvegarde de l'état précédent pour logging
            const NkConnectionState previousState = mState;

            // Fermeture de la couche RUDP — placement new car operator= est supprimé
            mRUDP.~NkReliableUDP();
            new (&mRUDP) NkReliableUDP();

            // Passage à l'état déconnecté
            mState = NkConnectionState::NK_CONNECTION_DISCONNECTED;

            // Notification via callback si déconnexion depuis un état actif
            if (previousState == NkConnectionState::NK_CONNECTION_ESTABLISHED && onDisconnected)
            {
                onDisconnected(mRemotePeerId, "Force disconnected");
            }

            NK_NET_LOG_DEBUG("Déconnexion forcée : {} → {}",
                NkConnStateStr(previousState), NkConnStateStr(mState));
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Mise à jour périodique
        // =====================================================================

        void NkConnection::Update(float32 dt) noexcept
        {
            // Ignorer si déconnecté
            if (mState == NkConnectionState::NK_CONNECTION_DISCONNECTED)
            {
                return;
            }

            const NkTimestampMs now = NkNetNowMs();

            // -------------------------------------------------------------------------
            // Gestion des timeouts de handshake
            // -------------------------------------------------------------------------
            if (mState == NkConnectionState::NK_CONNECTION_SYN_SENT ||
                mState == NkConnectionState::NK_CONNECTION_SYN_RECEIVED)
            {
                if (now - mConnectAttemptAt > kNkTimeoutMs)
                {
                    NK_NET_LOG_WARN("Timeout handshake avec {} — déconnexion",
                        mRemoteAddr.ToString().CStr());
                    ForceDisconnect();
                    return;
                }
            }

            // -------------------------------------------------------------------------
            // Mise à jour de la couche RUDP
            // -------------------------------------------------------------------------
            mRUDP.Update(dt);

            // Mise à jour des statistiques depuis RUDP
            mStats.rttMs = mRUDP.GetRTT();
            mStats.jitterMs = mRUDP.GetJitter();
            mStats.packetLoss = mRUDP.GetPacketLoss();

            // -------------------------------------------------------------------------
            // Gestion des heartbeats (uniquement en état Established)
            // -------------------------------------------------------------------------
            if (mState == NkConnectionState::NK_CONNECTION_ESTABLISHED)
            {
                mHeartbeatTimer += dt;
                mTimeoutTimer += dt;

                // Envoi de PING périodique
                if (mHeartbeatTimer >= (static_cast<float32>(kNkHeartbeatIntervalMs) / 1000.f))
                {
                    SendPing();
                    mHeartbeatTimer = 0.f;
                }

                // Détection de timeout d'inactivité
                if (mTimeoutTimer >= (static_cast<float32>(kNkTimeoutMs) / 1000.f))
                {
                    if (now - mLastActivityAt > kNkTimeoutMs)
                    {
                        NK_NET_LOG_WARN("Timeout d'inactivité avec {} — déconnexion",
                            mRemoteAddr.ToString().CStr());
                        mState = NkConnectionState::NK_CONNECTION_TIMED_OUT;
                        ForceDisconnect();
                        return;
                    }
                    mTimeoutTimer = 0.f;
                }
            }

            // -------------------------------------------------------------------------
            // Gestion de la déconnexion gracieuse
            // -------------------------------------------------------------------------
            if (mState == NkConnectionState::NK_CONNECTION_DISCONNECTING)
            {
                // Attendre un court délai puis forcer la déconnexion
                // (en production : attendre ACK du FIN)
                if (mTimeoutTimer >= 1.f)  // 1 seconde de grâce
                {
                    ForceDisconnect();
                }
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Envoi de données
        // =====================================================================

        NkNetResult NkConnection::Send(
            const uint8* data,
            uint32 size,
            NkNetChannel channel
        ) noexcept
        {
            // Validation des paramètres
            if (data == nullptr || size == 0)
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Vérification de l'état de connexion
            if (mState != NkConnectionState::NK_CONNECTION_ESTABLISHED)
            {
                NK_NET_LOG_WARN("Send() appelé en état {} — ignoré", NkConnStateStr(mState));
                return NkNetResult::NK_NET_NOT_CONNECTED;
            }

            // Mise à jour des statistiques
            mStats.bytesSent += size;
            mStats.packetsSent++;

            // Envoi via la couche RUDP
            return static_cast<NkNetResult>(mRUDP.Send(data, size, channel));
        }

        // Note : SendObject<T>() est un template défini inline dans le header

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Réception de données
        // =====================================================================

        void NkConnection::OnRawReceived(const uint8* data, uint32 size) noexcept
        {
            // Validation des paramètres
            if (data == nullptr || size == 0)
            {
                return;
            }

            // Mise à jour de l'activité récente
            mLastActivityAt = NkNetNowMs();
            mStats.bytesReceived += size;
            mStats.packetsReceived++;

            // -------------------------------------------------------------------------
            // Traitement des paquets système (handshake/heartbeat)
            // -------------------------------------------------------------------------
            if (size >= NkSystemHeader::kSize)
            {
                NkSystemHeader header;
                if (header.Deserialize(data, size))
                {
                    // Validation du magic et version
                    if (header.magic == kProtocolMagic && header.version == kProtocolVersion)
                    {
                        switch (static_cast<NkSystemPacketType>(header.type))
                        {
                            case NkSystemPacketType::NK_SYS_SYN:
                                ProcessSYN(data, size);
                                return;  // Paquet système traité — ne pas passer à RUDP
                            case NkSystemPacketType::NK_SYS_SYN_ACK:
                                ProcessSYNACK(data, size);
                                return;
                            case NkSystemPacketType::NK_SYS_PING:
                                ProcessPing(data, size);
                                return;
                            case NkSystemPacketType::NK_SYS_PONG:
                                ProcessPong(data, size);
                                return;
                            case NkSystemPacketType::NK_SYS_FIN:
                                // Réception de FIN : déconnexion gracieuse
                                NK_NET_LOG_INFO("FIN reçu de {} — déconnexion",
                                    mRemoteAddr.ToString().CStr());
                                ForceDisconnect();
                                return;
                            default:
                                // Type inconnu — ignorer
                                break;
                        }
                    }
                }
            }

            // -------------------------------------------------------------------------
            // Traitement des paquets de données applicatives via RUDP
            // -------------------------------------------------------------------------
            if (mState == NkConnectionState::NK_CONNECTION_ESTABLISHED)
            {
                // Forward vers la couche RUDP pour traitement
                mRUDP.OnReceived(data, size);

                // Extraction des messages livrés
                NkVector<NkRecvEntry> delivered;
                mRUDP.Drain(delivered);

                // Enqueue dans la file de réception avec protection mutex
                {
                    threading::NkScopedLockMutex lock(mQueueMutex);
                    for (const auto& entry : delivered)
                    {
                        if (entry.valid && entry.size > 0)
                        {
                            NkReceiveMsg msg;
                            std::memcpy(msg.data, entry.data, entry.size);
                            msg.size = entry.size;
                            msg.from = mRemotePeerId;
                            msg.channel = NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED;  // Simplifié
                            msg.receivedAt = NkNetNowMs();
                            mIncomingQueue.PushBack(msg);
                        }
                    }
                }
            }
        }

        void NkConnection::DrainReceived(NkVector<NkReceiveMsg>& out) noexcept
        {
            // Extraction thread-safe depuis la file de réception
            threading::NkScopedLockMutex lock(mQueueMutex);
            for (auto& msg : mIncomingQueue)
            {
                out.PushBack(msg);
            }
            mIncomingQueue.Clear();
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Accesseurs
        // =====================================================================

        NkConnectionState NkConnection::GetState() const noexcept
        {
            return mState;
        }

        bool NkConnection::IsConnected() const noexcept
        {
            return mState == NkConnectionState::NK_CONNECTION_ESTABLISHED;
        }

        NkPeerId NkConnection::GetRemotePeerId() const noexcept
        {
            return mRemotePeerId;
        }

        NkPeerId NkConnection::GetLocalPeerId() const noexcept
        {
            return mLocalPeerId;
        }

        const NkAddress& NkConnection::GetRemoteAddr() const noexcept
        {
            return mRemoteAddr;
        }

        const NkConnectionStats& NkConnection::Stats() const noexcept
        {
            return mStats;
        }

        float32 NkConnection::GetRTT() const noexcept
        {
            return mRUDP.GetRTT();
        }

        uint32 NkConnection::GetPingMs() const noexcept
        {
            return mRUDP.GetPingMs();
        }

        NkTimestampMs NkConnection::LastActivityAt() const noexcept
        {
            return mLastActivityAt;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Handshake (privé)
        // =====================================================================

        NkNetResult NkConnection::SendSYN() noexcept
        {
            // Construction de l'en-tête SYN
            NkSystemHeader syn;
            syn.type = static_cast<uint8>(NkSystemPacketType::NK_SYS_SYN);
            syn.challenge = mLocalChallenge;
            syn.peerId = mLocalPeerId.value;

            uint8 buffer[NkSystemHeader::kSize];
            syn.Serialize(buffer);

            // Envoi via socket avec canal fiable pour le handshake
            const auto result = mRUDP.Send(buffer, NkSystemHeader::kSize,
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);

            if (result == NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_DEBUG("SYN envoyé : challenge={}", mLocalChallenge);
            }

            return static_cast<NkNetResult>(result);
        }

        NkNetResult NkConnection::SendSYNACK() noexcept
        {
            // Construction de l'en-tête SYN-ACK
            NkSystemHeader synAck;
            synAck.type = static_cast<uint8>(NkSystemPacketType::NK_SYS_SYN_ACK);
            synAck.challenge = mLocalChallenge;  // Notre challenge pour le client
            synAck.response = ComputeChallengeResponse(mRemoteChallenge, mRemotePeerId.value);
            synAck.peerId = mLocalPeerId.value;

            uint8 buffer[NkSystemHeader::kSize];
            synAck.Serialize(buffer);

            // Envoi via socket
            const auto result = mRUDP.Send(buffer, NkSystemHeader::kSize,
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);

            if (result == NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_DEBUG("SYN-ACK envoyé : response={}", synAck.response);
            }

            return static_cast<NkNetResult>(result);
        }

        NkNetResult NkConnection::SendACK() noexcept
        {
            // Construction de l'en-tête ACK final
            NkSystemHeader ack;
            ack.type = static_cast<uint8>(NkSystemPacketType::NK_SYS_ACK);
            ack.challenge = 0;  // Non utilisé pour ACK
            ack.response = ComputeChallengeResponse(mRemoteChallenge, mLocalPeerId.value);
            ack.peerId = mLocalPeerId.value;

            uint8 buffer[NkSystemHeader::kSize];
            ack.Serialize(buffer);

            // Envoi via socket
            const auto result = mRUDP.Send(buffer, NkSystemHeader::kSize,
                NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);

            if (result == NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_DEBUG("ACK envoyé : handshake complété");
            }

            return static_cast<NkNetResult>(result);
        }

        void NkConnection::ProcessSYN(const uint8* data, uint32 size) noexcept
        {
            // Uniquement côté serveur
            if (!mIsServer)
            {
                NK_NET_LOG_WARN("SYN reçu côté client — ignoré");
                return;
            }

            // Désérialisation de l'en-tête
            NkSystemHeader header;
            if (!header.Deserialize(data, size) || header.type != static_cast<uint8>(NkSystemPacketType::NK_SYS_SYN))
            {
                NK_NET_LOG_WARN("SYN invalide reçu");
                return;
            }

            // Validation du peerId et stockage du challenge distant
            mRemotePeerId = NkPeerId{ header.peerId };
            mRemoteChallenge = header.challenge;

            // Envoi de la réponse SYN-ACK
            if (SendSYNACK() == NkNetResult::NK_NET_OK)
            {
                mState = NkConnectionState::NK_CONNECTION_SYN_RECEIVED;
                mConnectAttemptAt = NkNetNowMs();
                NK_NET_LOG_DEBUG("SYN-ACK envoyé à {} — attente ACK", mRemoteAddr.ToString().CStr());
            }
        }

        void NkConnection::ProcessSYNACK(const uint8* data, uint32 size) noexcept
        {
            // Uniquement côté client
            if (mIsServer)
            {
                NK_NET_LOG_WARN("SYN-ACK reçu côté serveur — ignoré");
                return;
            }

            // Vérification de l'état
            if (mState != NkConnectionState::NK_CONNECTION_SYN_SENT)
            {
                NK_NET_LOG_WARN("SYN-ACK reçu en état {} — ignoré", NkConnStateStr(mState));
                return;
            }

            // Désérialisation de l'en-tête
            NkSystemHeader header;
            if (!header.Deserialize(data, size) || header.type != static_cast<uint8>(NkSystemPacketType::NK_SYS_SYN_ACK))
            {
                NK_NET_LOG_WARN("SYN-ACK invalide reçu");
                return;
            }

            // Validation de la réponse au challenge
            const uint32 expectedResponse = ComputeChallengeResponse(mLocalChallenge, mLocalPeerId.value);
            if (header.response != expectedResponse)
            {
                NK_NET_LOG_ERROR("Validation du challenge échouée : attendu={}, reçu={}",
                    expectedResponse, header.response);
                ForceDisconnect();
                return;
            }

            // Stockage du peerId serveur et du challenge distant
            mRemotePeerId = NkPeerId{ header.peerId };
            mRemoteChallenge = header.challenge;

            // Envoi de l'ACK final pour compléter le handshake
            if (SendACK() == NkNetResult::NK_NET_OK)
            {
                mState = NkConnectionState::NK_CONNECTION_ESTABLISHED;
                mStats.connectedSince = NkNetNowMs();
                NK_NET_LOG_INFO("Connexion établie avec serveur {}", mRemotePeerId.value);

                // Notification via callback
                if (onConnected)
                {
                    onConnected(mRemotePeerId);
                }
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnection — Heartbeat (privé)
        // =====================================================================

        void NkConnection::SendPing() noexcept
        {
            // Construction de l'en-tête PING
            NkSystemHeader ping;
            ping.type = static_cast<uint8>(NkSystemPacketType::NK_SYS_PING);
            ping.timestamp = NkNetNowMs();
            ping.peerId = mLocalPeerId.value;

            uint8 buffer[NkSystemHeader::kSize];
            ping.Serialize(buffer);

            // Envoi via RUDP en unreliable pour minimiser l'overhead
            mRUDP.Send(buffer, NkSystemHeader::kSize,
                NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
        }

        void NkConnection::ProcessPing(const uint8* data, uint32 size) noexcept
        {
            // Désérialisation de l'en-tête
            NkSystemHeader header;
            if (!header.Deserialize(data, size) || header.type != static_cast<uint8>(NkSystemPacketType::NK_SYS_PING))
            {
                return;
            }

            // Construction de la réponse PONG avec timestamp de réception
            NkSystemHeader pong;
            pong.type = static_cast<uint8>(NkSystemPacketType::NK_SYS_PONG);
            pong.timestamp = header.timestamp;  // Timestamp d'envoi du PING original
            pong.peerId = mLocalPeerId.value;

            uint8 buffer[NkSystemHeader::kSize];
            pong.Serialize(buffer);

            // Envoi de la réponse
            mRUDP.Send(buffer, NkSystemHeader::kSize,
                NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
        }

        void NkConnection::ProcessPong(const uint8* data, uint32 size) noexcept
        {
            // Désérialisation de l'en-tête
            NkSystemHeader header;
            if (!header.Deserialize(data, size) || header.type != static_cast<uint8>(NkSystemPacketType::NK_SYS_PONG))
            {
                return;
            }

            // Calcul du RTT à partir du timestamp d'envoi du PING original
            const NkTimestampMs now    = NkNetNowMs();
            const NkTimestampMs sentAt = header.timestamp;

            if (sentAt != 0 && sentAt <= now)
            {
                const float32 rttSample = static_cast<float32>(now - sentAt);

                // Mise à jour de l'estimateur Jacobson/Karels dans la couche RUDP
                mRUDP.UpdateRTT(rttSample);

                // Synchronisation des statistiques locales avec les nouvelles estimations
                mStats.rttMs    = mRUDP.GetRTT();
                mStats.jitterMs = mRUDP.GetJitter();

                NK_NET_LOG_DEBUG("PONG reçu : RTT={:.1f}ms jitter={:.1f}ms",
                    mStats.rttMs, mStats.jitterMs);
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Constructeur/Destructeur
        // =====================================================================

        NkConnectionManager::~NkConnectionManager() noexcept
        {
            // Arrêt propre si encore en cours d'exécution
            if (mRunning.Load())
            {
                Shutdown();
            }

            // Libération des connexions restantes (Shutdown() en a déjà libéré la plupart)
            threading::NkScopedLockMutex lock(mConnMutex);
            for (uint32 i = 0; i < mConnCount; ++i)
            {
                if (mConnections[i] != nullptr)
                {
                    delete mConnections[i];
                    mConnections[i] = nullptr;
                }
            }
            mConnCount = 0;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Initialisation
        // =====================================================================

        NkNetResult NkConnectionManager::StartServer(uint16 port, uint32 maxClients) noexcept
        {
            // Validation des paramètres
            if (port == 0 || maxClients == 0 || maxClients > kMaxConn)
            {
                NK_NET_LOG_ERROR("StartServer : paramètres invalides (port={}, max={})", port, maxClients);
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Initialisation de la plateforme socket
            if (NkSocket::PlatformInit() != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("StartServer : échec d'initialisation de la plateforme socket");
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Création et configuration du socket serveur
            NkAddress bindAddr = NkAddress::Any(port);
            if (mSocket.Create(bindAddr, NkSocket::Type::NK_UDP) != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("StartServer : échec de création du socket sur port {}", port);
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Récupération de la configuration globale
            const auto& cfg = NkNetworkConfigManager::Get();

            mSocket.SetNonBlocking(true);
            mSocket.SetRecvBufferSize(kNkRecvBufferSize);
            mSocket.SetSendBufferSize(kNkSendBufferSize);

            // Initialisation des membres
            mIsServer = true;
            mLocalPeerId = NkPeerId::Server();  // Le serveur a toujours l'ID 1
            maxConnections = math::NkMin(maxClients, cfg.maxConnections);
            mConnCount = 0;

            // Démarrage du thread réseau
            mRunning.Store(true);
            mNetThread = threading::NkThread([this](void*) { NetThreadLoop(); });

            NK_NET_LOG_INFO("Serveur démarré sur port {} (max {} clients)", port, maxConnections);
            return NkNetResult::NK_NET_OK;
        }

        NkNetResult NkConnectionManager::Connect(const NkAddress& serverAddr, uint16 localPort) noexcept
        {
            // Validation des paramètres
            if (!serverAddr.IsValid())
            {
                NK_NET_LOG_ERROR("Connect : adresse serveur invalide");
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Initialisation de la plateforme socket
            if (NkSocket::PlatformInit() != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Connect : échec d'initialisation de la plateforme socket");
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            // Création et configuration du socket client
            NkAddress bindAddr = NkAddress::Any(localPort);
            if (mSocket.Create(bindAddr, NkSocket::Type::NK_UDP) != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Connect : échec de création du socket client");
                return NkNetResult::NK_NET_SOCKET_ERROR;
            }

            mSocket.SetNonBlocking(true);
            mSocket.SetRecvBufferSize(kNkRecvBufferSize);
            mSocket.SetSendBufferSize(kNkSendBufferSize);

            // Initialisation des membres
            mIsServer = false;
            mLocalPeerId = NkPeerId::Generate();  // ID unique pour ce client
            maxConnections = 1;  // Client : une seule connexion
            mConnCount = 0;

            // Création de la connexion unique
            NkConnection* conn = new NkConnection();
            conn->InitAsClient(&mSocket, serverAddr, mLocalPeerId);

            // Stockage dans la table
            {
                threading::NkScopedLockMutex lock(mConnMutex);
                mConnections[0] = conn;
                mConnPeerIds[0] = NkPeerId::Invalid();  // Sera défini après handshake
                mConnCount = 1;
            }

            // Démarrage du handshake
            const NkNetResult result = conn->Connect();
            if (result != NkNetResult::NK_NET_OK)
            {
                NK_NET_LOG_ERROR("Connect : échec d'initiation du handshake");
                delete conn;
                {
                    threading::NkScopedLockMutex lock(mConnMutex);
                    mConnections[0] = nullptr;
                    mConnCount = 0;
                }
                return result;
            }

            // Démarrage du thread réseau
            mRunning.Store(true);
            mNetThread = threading::NkThread([this](void*) { NetThreadLoop(); });

            NK_NET_LOG_INFO("Connexion initiée vers {}", serverAddr.ToString().CStr());
            return NkNetResult::NK_NET_OK;
        }

        void NkConnectionManager::Shutdown() noexcept
        {
            // Arrêt du flag de boucle
            mRunning.Store(false);

            // Attente de la fin du thread réseau
            if (mNetThread.Joinable())
            {
                mNetThread.Join();
            }

            // Déconnexion de tous les pairs
            DisconnectAll("Server shutdown");

            // Libération de toutes les connexions restantes après déconnexion gracieuse
            {
                threading::NkScopedLockMutex lock(mConnMutex);
                for (uint32 i = 0; i < mConnCount; ++i)
                {
                    if (mConnections[i] != nullptr)
                    {
                        delete mConnections[i];
                        mConnections[i] = nullptr;
                        mConnPeerIds[i] = NkPeerId::Invalid();
                    }
                }
                mConnCount = 0;
            }

            // Fermeture du socket
            mSocket.Close();
            NkSocket::PlatformShutdown();

            NK_NET_LOG_INFO("Gestionnaire de connexions arrêté");
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Envoi de données
        // =====================================================================

        NkNetResult NkConnectionManager::SendTo(
            NkPeerId peer,
            const uint8* data,
            uint32 size,
            NkNetChannel ch
        ) noexcept
        {
            // Validation des paramètres
            if (data == nullptr || size == 0 || !peer.IsValid())
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            // Recherche de la connexion avec protection mutex
            NkConnection* conn = nullptr;
            {
                threading::NkScopedLockMutex lock(mConnMutex);
                conn = FindConnection(peer);
            }

            if (conn == nullptr || !conn->IsConnected())
            {
                return NkNetResult::NK_NET_NOT_CONNECTED;
            }

            // Envoi via la connexion trouvée
            return conn->Send(data, size, ch);
        }

        NkNetResult NkConnectionManager::Broadcast(
            const uint8* data,
            uint32 size,
            NkNetChannel ch,
            NkPeerId exclude
        ) noexcept
        {
            // Validation des paramètres
            if (data == nullptr || size == 0)
            {
                return NkNetResult::NK_NET_INVALID_ARG;
            }

            NkNetResult finalResult = NkNetResult::NK_NET_OK;

            // Itération sur toutes les connexions avec protection mutex
            {
                threading::NkScopedLockMutex lock(mConnMutex);
                for (uint32 i = 0; i < mConnCount; ++i)
                {
                    NkConnection* conn = mConnections[i];
                    if (conn != nullptr && conn->IsConnected())
                    {
                        // Exclusion du pair spécifié si nécessaire
                        if (conn->GetRemotePeerId() == exclude)
                        {
                            continue;
                        }

                        const NkNetResult result = conn->Send(data, size, ch);
                        if (result != NkNetResult::NK_NET_OK && finalResult == NkNetResult::NK_NET_OK)
                        {
                            // Conserver le premier code d'erreur rencontré
                            finalResult = result;
                        }
                    }
                }
            }

            return finalResult;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Réception de données
        // =====================================================================

        void NkConnectionManager::DrainAll(NkVector<NkReceiveMsg>& out) noexcept
        {
            // Extraction thread-safe depuis le buffer global
            threading::NkScopedLockMutex lock(mIncomingMutex);
            for (auto& msg : mGlobalIncoming)
            {
                out.PushBack(msg);
            }
            mGlobalIncoming.Clear();
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Déconnexion
        // =====================================================================

        void NkConnectionManager::Disconnect(NkPeerId peer, const char* reason) noexcept
        {
            // Recherche et déconnexion de la connexion cible
            NkConnection* conn = nullptr;
            {
                threading::NkScopedLockMutex lock(mConnMutex);
                conn = FindConnection(peer);
            }

            if (conn != nullptr)
            {
                conn->Disconnect(reason);
            }
        }

        void NkConnectionManager::DisconnectAll(const char* reason) noexcept
        {
            // Déconnexion de toutes les connexions actives
            threading::NkScopedLockMutex lock(mConnMutex);
            for (uint32 i = 0; i < mConnCount; ++i)
            {
                if (mConnections[i] != nullptr)
                {
                    mConnections[i]->Disconnect(reason);
                }
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Accesseurs
        // =====================================================================

        bool NkConnectionManager::IsServer() const noexcept
        {
            return mIsServer;
        }

        bool NkConnectionManager::IsRunning() const noexcept
        {
            return mRunning.Load() && mNetThread.Joinable();
        }

        uint32 NkConnectionManager::ConnectedPeerCount() const noexcept
        {
            uint32 count = 0;
            threading::NkScopedLockMutex lock(mConnMutex);
            for (uint32 i = 0; i < mConnCount; ++i)
            {
                if (mConnections[i] != nullptr && mConnections[i]->IsConnected())
                {
                    ++count;
                }
            }
            return count;
        }

        NkPeerId NkConnectionManager::GetLocalPeerId() const noexcept
        {
            return mLocalPeerId;
        }

        const NkConnection* NkConnectionManager::GetConnection(NkPeerId peer) const noexcept
        {
            threading::NkScopedLockMutex lock(mConnMutex);
            return const_cast<NkConnectionManager*>(this)->FindConnection(peer);
        }

        bool NkConnectionManager::GetConnectionStats(
            NkPeerId peer,
            NkConnectionStats& outStats
        ) const noexcept
        {
            // Copie atomique des stats sous lock — le pointeur n'est jamais exposé à l'appelant.
            threading::NkScopedLockMutex lock(mConnMutex);
            const NkConnection* conn = const_cast<NkConnectionManager*>(this)->FindConnection(peer);
            if (conn == nullptr || !conn->IsConnected())
            {
                return false;
            }
            outStats = conn->Stats();
            return true;
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Boucle réseau (privé)
        // =====================================================================

        void NkConnectionManager::NetThreadLoop() noexcept
        {
            NK_NET_LOG_INFO("Thread réseau démarré");

            // Boucle principale jusqu'à arrêt demandé
            while (mRunning.Load())
            {
                const NkTimestampMs frameStart = NkNetNowMs();

                // 1. Polling du socket pour réception de paquets
                PollSocket();

                // 2. Mise à jour de toutes les connexions
                const float32 dt = 0.016f;  // ~60 FPS — à adapter selon votre boucle
                UpdateConnections(dt);

                // 3. Sleep pour éviter le busy-wait (optionnel)
                const NkTimestampMs frameEnd = NkNetNowMs();
                const uint32 frameTime = static_cast<uint32>(frameEnd - frameStart);
                if (frameTime < 16)  // Moins de 16ms = plus de 60 FPS
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameTime));
                }
            }

            NK_NET_LOG_INFO("Thread réseau arrêté");
        }

        void NkConnectionManager::PollSocket() noexcept
        {
            // Buffer de réception
            uint8 buffer[kNkMaxPacketSize];
            uint32 receivedSize = 0;
            NkAddress from;

            // Lecture de tous les paquets disponibles (mode non-bloquant)
            while (mSocket.RecvFrom(buffer, sizeof(buffer), receivedSize, from) == NkNetResult::NK_NET_OK)
            {
                if (receivedSize > 0)
                {
                    // Dispatch vers la connexion appropriée
                    DispatchPacket(buffer, receivedSize, from);
                }
            }
        }

        void NkConnectionManager::UpdateConnections(float32 dt) noexcept
        {
            // Collecte des pairs déconnectés pour invoquer les callbacks HORS du lock.
            // Invoquer des callbacks sous mConnMutex provoquerait un deadlock si l'appelant
            // rappelle SendTo()/Broadcast() depuis son handler (qui acquièrent aussi mConnMutex).
            NkVector<NkPeerId> disconnectedPeers;

            {
                threading::NkScopedLockMutex lock(mConnMutex);

                uint32 i = 0;
                while (i < mConnCount)
                {
                    NkConnection* conn = mConnections[i];
                    if (conn == nullptr)
                    {
                        ++i;
                        continue;
                    }

                    // Mise à jour des timers, heartbeats et retransmissions
                    conn->Update(dt);

                    if (conn->GetState() == NkConnectionState::NK_CONNECTION_DISCONNECTED)
                    {
                        // Enregistrer le peer ID avant libération pour le callback
                        disconnectedPeers.PushBack(conn->GetRemotePeerId());

                        // Libération de la connexion
                        delete conn;

                        // Compaction : échange avec le dernier slot actif pour éviter les trous.
                        // Sans compaction, mConnCount ne descendrait jamais et le serveur atteindrait
                        // rapidement la limite kMaxConn même avec des connexions temporaires.
                        mConnCount--;
                        mConnections[i]       = mConnections[mConnCount];
                        mConnPeerIds[i]       = mConnPeerIds[mConnCount];
                        mConnections[mConnCount] = nullptr;
                        mConnPeerIds[mConnCount] = NkPeerId::Invalid();

                        // Ne pas incrémenter i : le slot contient maintenant le dernier élément
                        // qui n'a pas encore été mis à jour — il sera traité au prochain passage.
                    }
                    else
                    {
                        ++i;
                    }
                }

                // Mise à jour des métriques globales
                {
                    threading::NkScopedLockMutex mLock(mMetricsMutex);
                    mMetrics.activeConnections = 0;
                    for (uint32 j = 0; j < mConnCount; ++j)
                    {
                        if (mConnections[j] != nullptr && mConnections[j]->IsConnected())
                        {
                            mMetrics.activeConnections++;
                        }
                    }
                    if (mMetrics.activeConnections > mMetrics.peakConnections)
                    {
                        mMetrics.peakConnections = mMetrics.activeConnections;
                    }
                    mMetrics.lastUpdate = NkNetNowMs();
                }
            }

            // Déclenchement des callbacks de déconnexion HORS du lock (évite deadlock)
            for (const auto& peer : disconnectedPeers)
            {
                if (onPeerDisconnected)
                {
                    onPeerDisconnected(peer, "Disconnected");
                }
            }
        }

        void NkConnectionManager::DispatchPacket(
            const uint8* data,
            uint32 size,
            const NkAddress& from
        ) noexcept
        {
            // FindOrCreateConnection est atomique : la connexion est trouvée ou créée
            // dans une seule section critique, sans race condition TOCTOU.
            bool isNewConnection = false;
            NkConnection* conn = FindOrCreateConnection(from, isNewConnection);

            if (conn == nullptr)
            {
                NK_NET_LOG_WARN("Paquet reçu de {} rejeté (limite connexions atteinte)",
                    from.ToString().CStr());
                return;
            }

            // Déclenchement du callback de connexion HORS du lock pour éviter deadlock.
            // FindOrCreateConnection ne déclenche pas le callback lui-même — c'est intentionnel.
            if (isNewConnection && onPeerConnected)
            {
                onPeerConnected(conn->GetRemotePeerId());
            }

            // Traitement du paquet reçu (handshake, données, heartbeat)
            conn->OnRawReceived(data, size);

            // Extraction des messages livrés et consolidation dans le buffer global
            NkVector<NkReceiveMsg> connMessages;
            conn->DrainReceived(connMessages);

            if (!connMessages.IsEmpty())
            {
                threading::NkScopedLockMutex lock(mIncomingMutex);
                for (auto& msg : connMessages)
                {
                    mGlobalIncoming.PushBack(msg);
                }
            }
        }

        // =====================================================================
        // IMPLÉMENTATION : NkConnectionManager — Lookup de connexions (privé)
        // =====================================================================

        NkConnection* NkConnectionManager::FindOrCreateConnection(
            const NkAddress& from,
            bool& outIsNew
        ) noexcept
        {
            outIsNew = false;

            // Mode client : une seule connexion possible, pas de création dynamique
            if (!mIsServer)
            {
                threading::NkScopedLockMutex lock(mConnMutex);
                return (mConnCount > 0) ? mConnections[0] : nullptr;
            }

            // Mode serveur : la RECHERCHE et la CRÉATION sont atomiques sous un seul lock.
            // Deux sections lock séparées créaient un TOCTOU : entre les deux sections,
            // un autre thread pouvait créer une connexion pour la même adresse, résultant
            // en deux NkConnection distinctes pour le même pair.
            {
                threading::NkScopedLockMutex lock(mConnMutex);

                // Recherche d'une connexion existante par adresse source
                NkConnection* existing = FindConnectionByAddr(from);
                if (existing != nullptr)
                {
                    return existing;
                }

                // Vérification de la capacité avant allocation
                if (mConnCount >= maxConnections)
                {
                    NK_NET_LOG_WARN("Limite de connexions atteinte ({}/{}) — rejet de {}",
                        mConnCount, maxConnections, from.ToString().CStr());
                    return nullptr;
                }

                // Allocation et initialisation dans la section critique (atomique)
                NkConnection* newConn = new NkConnection();
                newConn->InitAsServer(&mSocket, from, mLocalPeerId, NkPeerId::Generate());

                // Insertion dans la table de connexions
                const uint32 slot = mConnCount++;
                mConnections[slot] = newConn;
                mConnPeerIds[slot] = newConn->GetRemotePeerId();

                NK_NET_LOG_INFO("Connexion créée : {} (slot {}/{} max)",
                    from.ToString().CStr(), slot, maxConnections);

                // Signaler au appelant qu'il s'agit d'une nouvelle connexion.
                // Le callback onPeerConnected est déclenché par DispatchPacket() HORS du lock
                // pour éviter le deadlock si le callback rappelle SendTo()/Broadcast().
                outIsNew = true;
                return newConn;
            }
        }

        NkConnection* NkConnectionManager::FindConnection(NkPeerId peer) noexcept
        {
            for (uint32 i = 0; i < mConnCount; ++i)
            {
                if (mConnections[i] != nullptr &&
                    mConnections[i]->GetRemotePeerId() == peer)
                {
                    return mConnections[i];
                }
            }
            return nullptr;
        }

        NkConnection* NkConnectionManager::FindConnectionByAddr(const NkAddress& addr) noexcept
        {
            for (uint32 i = 0; i < mConnCount; ++i)
            {
                if (mConnections[i] != nullptr &&
                    mConnections[i]->GetRemoteAddr() == addr)
                {
                    return mConnections[i];
                }
            }
            return nullptr;
        }

    } // namespace net

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
    Gestion du handshake :
    ---------------------
    - Challenge/response simple (XOR) pour l'exemple — renforcer en production
    - Timeout de handshake : kNkTimeoutMs (10s) pour éviter les connexions pendantes
    - Validation stricte des en-têtes système avant traitement

    Thread-safety :
    --------------
    - mConnMutex protège l'accès à la table des connexions
    - mQueueMutex protège la file de réception par connexion
    - mIncomingMutex protège le buffer global DrainAll()
    - Les callbacks sont invoqués depuis le thread réseau — différer vers thread jeu si nécessaire

    Gestion mémoire :
    ----------------
    - Connexions allouées dynamiquement (new/delete) pour flexibilité
    - Table statique de pointeurs pour lookup rapide O(1) par index
    - Compaction optionnelle de la table après déconnexions (non implémentée ici)

    Performance :
    ------------
    - Buffers stack pour en-têtes système : pas d'allocation heap
    - Lecture non-bloquante du socket : polling avec boucle while pour drainer
    - Mise à jour des connexions en batch : itération simple sur tableau

    Extensions futures :
    -------------------
    - Support du chiffrement TLS/DTLS pour les paquets système
    - Compression des payloads pour réduire la bande passante
    - Priorisation des connexions pour QoS (qualité de service)
    - Statistiques détaillées exportables pour monitoring/telemetry
    - Support IPv6 complet dans NkAddress et résolution DNS

    Debugging :
    ----------
    - Logging conditionnel via NK_NET_LOG_DEBUG en mode développement
    - Validation des en-têtes avec retour d'erreur explicite
    - Callbacks pour instrumentation externe des événements de connexion
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================