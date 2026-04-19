#pragma once
// =============================================================================
// NKNetwork/Transport/NkReliableUDP.h
// =============================================================================
// Couche de fiabilité sur UDP (RUDP — Reliable UDP).
//
// PROBLÈMES UDP RÉSOLUS :
//   • Perte de paquets     → retransmission + ACK
//   • Duplication          → déduplication par numéro de séquence
//   • Réordonnancement     → file d'attente ordonnée (canaux fiables)
//   • Fragmentation        → découpage + réassemblage des gros messages
//
// ALGORITHME (inspiré de ENet / GameNetworkingSockets) :
//   Chaque paquet porte :
//     seqNum   — numéro de séquence monotone du canal
//     ackNum   — numéro du dernier paquet reçu de l'autre côté
//     ackMask  — masque 32 bits des 32 paquets précédents (ACK selectif)
//
//   Envoi fiable :
//     1. Assigne un seqNum
//     2. Stocke dans NkSendWindow (buffer circulaire)
//     3. Marque le timestamp d'envoi
//     4. Si pas d'ACK en RTT × 1.5 → retransmet
//
//   Réception :
//     1. Extrait seqNum, ackNum, ackMask
//     2. Applique les ACKs → libère les buffers confirmés
//     3. Pour ORDERED : livre en ordre strict (attend les trous)
//     4. Pour UNORDERED : livre dès réception
//
// CANAUX SUPPORTÉS :
//   Unreliable   → pas de séquençage ni retransmission
//   Reliable     → ACK + retransmission, ordonné
//   Sequenced    → seul le plus récent compte, les vieux ignorés
// =============================================================================

#include "NkSocket.h"
#include "NKThreading/NkMutex.h"

namespace nkentseu {
    namespace net {

        // =====================================================================
        // NkRUDPHeader — en-tête de tout paquet RUDP
        // =====================================================================
        /**
         * @struct NkRUDPHeader
         * @brief En-tête de 16 bytes préfixant chaque datagramme UDP.
         *
         * LAYOUT (16 bytes total) :
         *   [0]    magic      uint8   — 0xNK (sanity check)
         *   [1]    flags      uint8   — NkRUDPFlags
         *   [2]    channel    uint8   — NkNetChannel
         *   [3]    fragIdx    uint8   — index du fragment (0 si non fragmenté)
         *   [4]    fragCount  uint8   — nombre de fragments (1 si non fragmenté)
         *   [5]    reserved   uint8
         *   [6-7]  dataSize   uint16  — taille du payload en bytes
         *   [8-11] seqNum     uint32  — numéro de séquence
         *   [12-15] ackNum    uint32  — ACK du pair (dernier seq reçu)
         *   [16-19] ackMask   uint32  — masque ACK sélectif (32 bits)
         *
         * Total en-tête : 20 bytes → payload max = 1400 - 20 = 1380 bytes
         */
        struct NkRUDPHeader {
            static constexpr uint8  kMagic  = 0x4E;  // 'N'
            static constexpr uint32 kSize   = 20u;

            uint8  magic     = kMagic;
            uint8  flags     = 0;
            uint8  channel   = 0;
            uint8  fragIdx   = 0;
            uint8  fragCount = 1;
            uint8  reserved  = 0;
            uint16 dataSize  = 0;
            uint32 seqNum    = 0;
            uint32 ackNum    = 0;
            uint32 ackMask   = 0;

            void Serialize  (uint8* buf) const noexcept;
            bool Deserialize(const uint8* buf, uint32 bufSize) noexcept;
        };

        enum NkRUDPFlags : uint8 {
            kFlagReliable   = 1 << 0,  ///< Paquet fiable (ACK requis)
            kFlagOrdered    = 1 << 1,  ///< Paquet ordonné
            kFlagAck        = 1 << 2,  ///< Paquet ACK pur (pas de payload)
            kFlagHandshake  = 1 << 3,  ///< Paquet de connexion
            kFlagDisconnect = 1 << 4,  ///< Paquet de déconnexion
            kFlagPing       = 1 << 5,  ///< Heartbeat
            kFlagFragment   = 1 << 6,  ///< Paquet fragmenté
            kFlagEncrypted  = 1 << 7,  ///< Payload chiffré
        };

        // =====================================================================
        // NkSendEntry — paquet en attente d'ACK
        // =====================================================================
        struct NkSendEntry {
            uint8           data[kNkMaxPacketSize] = {};
            uint32          size       = 0;
            uint32          seqNum     = 0;
            NkTimestampMs   sentAt     = 0;
            uint32          retransmits = 0;
            bool            acked      = false;
            NkNetChannel    channel    = NkNetChannel::Unreliable;
        };

        // =====================================================================
        // NkRecvEntry — paquet reçu en attente de livraison ordonnée
        // =====================================================================
        struct NkRecvEntry {
            uint8        data[kNkMaxPacketSize] = {};
            uint32       size    = 0;
            uint32       seqNum  = 0;
            bool         valid   = false;
        };

        // =====================================================================
        // NkRTTEstimator — estimation Round-Trip Time
        // =====================================================================
        class NkRTTEstimator {
        public:
            void Update(float32 sampleMs) noexcept;
            [[nodiscard]] float32 GetRTT()    const noexcept { return mRTT; }
            [[nodiscard]] float32 GetJitter() const noexcept { return mJitter; }
            [[nodiscard]] float32 GetRTO()    const noexcept;  ///< Retransmission timeout

        private:
            float32 mRTT    = 100.f;   ///< Estimation RTT (ms)
            float32 mJitter = 10.f;    ///< Variation RTT (ms)
            static constexpr float32 kAlpha = 0.125f;
            static constexpr float32 kBeta  = 0.25f;
        };

        // =====================================================================
        // NkReliableChannel — un canal RUDP par direction
        // =====================================================================
        class NkReliableChannel {
        public:
            NkReliableChannel() noexcept = default;

            // ── Envoi ────────────────────────────────────────────────────────
            /**
             * @brief Prépare un paquet fiable à envoyer.
             * Assigne un seqNum et stocke dans la fenêtre d'envoi.
             * @return seqNum assigné, ou UINT32_MAX si fenêtre pleine.
             */
            uint32 PrepareReliable(const uint8* data, uint32 size,
                                    NkNetChannel ch) noexcept;

            /**
             * @brief Retourne les paquets à (re)transmettre.
             * Appelé chaque frame — rempli mToSend avec les paquets prêts.
             */
            void GatherPendingSends(NkTimestampMs now, float32 rto,
                                     NkVector<const NkSendEntry*>& out) noexcept;

            // ── Réception ────────────────────────────────────────────────────
            /**
             * @brief Traite un ACK reçu.
             * Libère les entrées confirmées de la fenêtre d'envoi.
             */
            void ProcessACK(uint32 ackNum, uint32 ackMask) noexcept;

            /**
             * @brief Insère un paquet reçu dans la file ordonnée.
             * @return true si le paquet est nouveau (pas un doublon).
             */
            bool InsertReceived(uint32 seqNum, const uint8* data, uint32 size) noexcept;

            /**
             * @brief Retire les paquets prêts à livrer (ordonnés).
             */
            void DrainDeliverable(NkVector<NkRecvEntry>& out) noexcept;

            // ── ACK à envoyer ────────────────────────────────────────────────
            [[nodiscard]] uint32 GetOutgoingAckNum()  const noexcept { return mLastReceivedSeq; }
            [[nodiscard]] uint32 GetOutgoingAckMask() const noexcept { return mReceivedMask; }

            // ── Stats ────────────────────────────────────────────────────────
            [[nodiscard]] uint32 GetPendingCount() const noexcept;
            [[nodiscard]] float32 GetPacketLoss()  const noexcept { return mPacketLoss; }

        private:
            // Fenêtre d'envoi (buffer circulaire)
            NkSendEntry  mSendWindow[kNkWindowSize] = {};
            uint32       mSendHead      = 0;   ///< Prochain slot à écrire
            uint32       mSendTail      = 0;   ///< Plus ancien non-ACKé
            uint32       mNextSeqNum    = 1;   ///< Prochain seqNum à assigner

            // Fenêtre de réception
            NkRecvEntry  mRecvBuffer[kNkWindowSize] = {};
            uint32       mExpectedSeq   = 1;   ///< Prochain seqNum attendu
            uint32       mLastReceivedSeq = 0; ///< Dernier reçu
            uint32       mReceivedMask  = 0;   ///< Masque ACK sélectif

            // Stats
            uint32       mTotalSent     = 0;
            uint32       mTotalAcked    = 0;
            uint32       mRetransmits   = 0;
            float32      mPacketLoss    = 0.f;
        };

        // =====================================================================
        // NkReliableUDP — gestion complète RUDP pour une paire de pairs
        // =====================================================================
        /**
         * @class NkReliableUDP
         * @brief Couche RUDP gérant la fiabilité pour une connexion peer-to-peer.
         *
         * Usage typique dans NkConnection :
         * @code
         * NkReliableUDP rudp;
         * rudp.Init(&socket, remoteAddr);
         *
         * // Chaque frame :
         * rudp.Update(dt);   // Retransmet si nécessaire, envoie ACKs
         *
         * // Envoyer :
         * rudp.Send(data, size, NkNetChannel::ReliableOrdered);
         *
         * // Recevoir (depuis NkConnectionManager) :
         * rudp.OnReceived(rawData, rawSize);  // Feed des données brutes
         *
         * // Consommer les paquets livrés :
         * NkVector<NkRecvEntry> delivered;
         * rudp.Drain(delivered);
         * for (auto& pkt : delivered) { /* process */ }
         * @endcode
         */
        class NkReliableUDP {
        public:
            NkReliableUDP()  noexcept = default;
            ~NkReliableUDP() noexcept = default;

            NkReliableUDP(const NkReliableUDP&) = delete;
            NkReliableUDP& operator=(const NkReliableUDP&) = delete;

            // ── Init ─────────────────────────────────────────────────────────
            void Init(NkSocket* socket, const NkAddress& remote) noexcept {
                mSocket = socket;
                mRemote = remote;
            }

            // ── Frame update ─────────────────────────────────────────────────
            /**
             * @brief Traitement des retransmissions et ACKs.
             * À appeler chaque frame depuis NkConnection::Update().
             */
            void Update(float32 dt) noexcept;

            // ── Envoi ─────────────────────────────────────────────────────────
            /**
             * @brief Envoie un message sur le canal spécifié.
             * Si le message dépasse kNkMaxPayloadSize → fragmentation automatique.
             */
            [[nodiscard]] NkNetResult Send(const uint8* data, uint32 size,
                                            NkNetChannel channel) noexcept;

            /**
             * @brief Envoi d'un ACK pur (sans payload).
             * Appelé automatiquement par Update().
             */
            NkNetResult SendACK() noexcept;

            // ── Réception ────────────────────────────────────────────────────
            /**
             * @brief Traite un datagramme UDP brut reçu.
             * Extrait l'en-tête, met à jour les ACKs, insère dans le buffer.
             */
            void OnReceived(const uint8* rawData, uint32 rawSize) noexcept;

            /**
             * @brief Retire les messages livrés et prêts à consommer.
             */
            void Drain(NkVector<NkRecvEntry>& out) noexcept;

            // ── Stats & diagnostic ───────────────────────────────────────────
            [[nodiscard]] float32 GetRTT()       const noexcept { return mRTT.GetRTT(); }
            [[nodiscard]] float32 GetJitter()    const noexcept { return mRTT.GetJitter(); }
            [[nodiscard]] float32 GetPacketLoss()const noexcept;
            [[nodiscard]] uint32  GetPingMs()    const noexcept { return (uint32)mRTT.GetRTT(); }

            [[nodiscard]] const NkAddress& GetRemote() const noexcept { return mRemote; }
            [[nodiscard]] NkTimestampMs    GetLastRecv() const noexcept { return mLastRecvAt; }

            // ── Fragmentation ────────────────────────────────────────────────
        private:
            NkNetResult SendFragmented(const uint8* data, uint32 size,
                                        NkNetChannel ch) noexcept;
            bool TryReassemble(uint8 fragIdx, uint8 fragCount,
                                uint32 seqBase, const uint8* data,
                                uint32 size) noexcept;

            // ── Données membres ──────────────────────────────────────────────
            NkSocket*          mSocket  = nullptr;
            NkAddress          mRemote;
            NkTimestampMs      mLastRecvAt = 0;
            NkTimestampMs      mLastAckAt  = 0;
            float32            mAckTimer   = 0.f;

            NkReliableChannel  mRelOrd;    ///< Canal ReliableOrdered
            NkReliableChannel  mRelUnord;  ///< Canal ReliableUnordered
            NkRTTEstimator     mRTT;

            // Ping round-trip
            NkTimestampMs mPingSentAt  = 0;
            uint32        mPingSeqNum  = 0;

            // Buffer de réassemblage de fragments
            struct FragBuffer {
                uint8  data[kNkMaxPacketSize * kNkMaxFragments] = {};
                uint8  received[kNkMaxFragments] = {};
                uint8  total    = 0;
                uint8  count    = 0;
                uint32 seqBase  = 0;
                bool   active   = false;
            } mFragBuffer;

            // Buffer de paquets non-fiables reçus (livrés directement)
            NkVector<NkRecvEntry> mUnreliableQueue;

            // Séquençage non-fiable (pour ignorer les vieux)
            uint32 mSeqLastUnrel = 0;
        };

    } // namespace net
} // namespace nkentseu
