// =============================================================================
// NKNetwork/Transport/NkReliableUDP.h
// =============================================================================
// DESCRIPTION :
//   Couche de fiabilité sur UDP (RUDP — Reliable UDP) pour communications
//   temps réel avec garanties configurables de livraison et d'ordre.
//
// PROBLÈMES UDP RÉSOLUS :
//   • Perte de paquets     → retransmission + accusés de réception (ACK)
//   • Duplication          → déduplication par numéro de séquence unique
//   • Réordonnancement     → file d'attente ordonnée pour canaux fiables
//   • Fragmentation        → découpage et réassemblage des gros messages
//
// ALGORITHME (inspiré de ENet / GameNetworkingSockets / RakNet) :
//   Chaque paquet transporte un en-tête RUDP contenant :
//     • seqNum   : numéro de séquence monotone incrémenté par canal
//     • ackNum   : dernier numéro de séquence reçu de l'autre pair
//     • ackMask  : masque de 32 bits pour ACK sélectif des paquets récents
//
//   Mécanisme d'envoi fiable :
//     1. Attribution d'un seqNum unique au paquet
//     2. Stockage dans NkSendWindow (buffer circulaire de taille fixe)
//     3. Enregistrement du timestamp d'envoi pour calcul de timeout
//     4. Retransmission si aucun ACK reçu après RTT × facteur de sécurité
//
//   Mécanisme de réception :
//     1. Extraction et validation de l'en-tête RUDP
//     2. Mise à jour des ACKs → libération des buffers confirmés
//     3. Pour canaux ORDERED : livraison strictement dans l'ordre des seqNum
//     4. Pour canaux UNORDERED : livraison immédiate dès réception valide
//
// CANAUX SUPPORTÉS (via NkNetChannel) :
//   • Unreliable          : UDP pur, aucune garantie, latence minimale
//   • ReliableOrdered     : ACK + retransmission + livraison dans l'ordre
//   • ReliableUnordered   : ACK + retransmission, ordre non garanti
//   • Sequenced           : Seul le paquet le plus récent est livré (état continu)
//
// DÉPENDANCES :
//   • NkSocket.h          : Abstraction socket UDP pour transport bas-niveau
//   • NkNetDefines.h      : Types fondamentaux et constantes réseau
//   • NKThreading/NkMutex : Protection thread-safe optionnelle pour accès concurrents
//
// RÈGLES D'UTILISATION :
//   • Une instance NkReliableUDP par connexion peer-to-peer
//   • Appeler Update(dt) chaque frame pour gérer retransmissions et ACKs
//   • Vérifier les codes de retour NkNetResult pour chaque opération Send()
//   • Utiliser Drain() pour récupérer les paquets livrés prêts à traitement
//
// AUTEUR   : Rihen
// DATE     : 2026-02-10
// VERSION  : 1.0.0
// LICENCE  : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKRELIABLEUDP_H
#define NKENTSEU_NETWORK_NKRELIABLEUDP_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules requis dans l'ordre de dépendance hiérarchique.

    #include "NKNetwork/NkNetworkApi.h"
    #include "NKNetwork/Transport/NkSocket.h"
    #include "NKNetwork/Core/NkNetDefines.h"
    #include "NKCore/NkTypes.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKThreading/NkMutex.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : NAMESPACE PRINCIPAL ET DÉCLARATIONS
    // -------------------------------------------------------------------------
    // Toutes les déclarations du module résident dans nkentseu::net.

    namespace nkentseu {

        namespace net {

            // =================================================================
            // STRUCTURE : NkRUDPHeader — En-tête de protocole RUDP
            // =================================================================
            /**
             * @struct NkRUDPHeader
             * @brief En-tête fixe de 20 bytes préfixant chaque datagramme UDP fiable.
             *
             * LAYOUT MÉMOIRE (20 bytes, big-endian pour réseau) :
             *   Offset  Taille  Champ       Description
             *   ------  ------  -----       -----------
             *   0       1       magic       Signature 0x4E ('N') pour validation
             *   1       1       flags       Bits de contrôle (NkRUDPFlags)
             *   2       1       channel     Identifiant du canal logique (NkNetChannel)
             *   3       1       fragIdx     Index du fragment courant (0 si unique)
             *   4       1       fragCount   Nombre total de fragments (1 si unique)
             *   5       1       reserved    Réservé pour extensions futures (alignement)
             *   6-7     2       dataSize    Taille utile du payload en bytes (network order)
             *   8-11    4       seqNum      Numéro de séquence du paquet (network order)
             *   12-15   4       ackNum      ACK : dernier seqNum reçu du pair distant
             *   16-19   4       ackMask     ACK sélectif : masque 32 bits des paquets récents
             *
             * CONSÉQUENCES :
             *   • Payload max = kNkMaxPacketSize (1400) - 20 = 1380 bytes
             *   • Messages > 1380 bytes nécessitent fragmentation automatique
             *   • Tous les champs multi-octets sont en network byte order (big-endian)
             *
             * @note Utiliser Serialize()/Deserialize() pour conversion binaire portable
             * @see NkRUDPFlags Pour la sémantique des bits de contrôle
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkRUDPHeader
            {
                // -------------------------------------------------------------
                // Constantes de protocole
                // -------------------------------------------------------------

                /// Signature magique pour validation rapide des paquets entrants.
                static constexpr uint8 kMagic = 0x4E;  // Caractère ASCII 'N'

                /// Taille fixe de l'en-tête en bytes (sans payload).
                static constexpr uint32 kSize = 20u;

                // -------------------------------------------------------------
                // Champs de l'en-tête (ordre mémoire = ordre réseau)
                // -------------------------------------------------------------

                /// Signature de validation : doit valoir kMagic à la réception.
                uint8 magic = kMagic;

                /// Bits de contrôle combinant plusieurs flags NkRUDPFlags.
                uint8 flags = 0;

                /// Identifiant du canal logique utilisé pour ce paquet.
                uint8 channel = 0;

                /// Index du fragment dans un message fragmenté (0-based, 0 si non fragmenté).
                uint8 fragIdx = 0;

                /// Nombre total de fragments pour ce message (1 si non fragmenté).
                uint8 fragCount = 1;

                /// Octet réservé pour alignement ou extensions futures du protocole.
                uint8 reserved = 0;

                /// Taille utile des données suivant l'en-tête, en bytes (network order).
                uint16 dataSize = 0;

                /// Numéro de séquence unique pour ce paquet dans son canal (network order).
                uint32 seqNum = 0;

                /// Numéro de séquence du dernier paquet reçu du pair distant (ACK cumulatif).
                uint32 ackNum = 0;

                /// Masque de 32 bits pour ACK sélectif des paquets précédant ackNum.
                uint32 ackMask = 0;

                // -------------------------------------------------------------
                // Sérialisation / Désérialisation binaire
                // -------------------------------------------------------------

                /**
                 * @brief Sérialise l'en-tête dans un buffer binaire pour envoi réseau.
                 * @param buf Pointeur vers le buffer de destination (doit avoir ≥ kSize bytes libres).
                 * @note Convertit les champs multi-octets en network byte order (big-endian).
                 * @note N'écrit PAS le payload — celui-ci doit être copié après l'en-tête.
                 */
                void Serialize(uint8* buf) const noexcept;

                /**
                 * @brief Désérialise un en-tête depuis un buffer binaire reçu.
                 * @param buf Pointeur vers le buffer source contenant l'en-tête.
                 * @param bufSize Taille disponible dans le buffer (doit être ≥ kSize).
                 * @return true si la désérialisation a réussi et magic est valide.
                 * @note Convertit les champs multi-octets de network → host byte order.
                 * @note Ne lit PAS le payload — celui-ci commence à buf + kSize.
                 */
                [[nodiscard]] bool Deserialize(const uint8* buf, uint32 bufSize) noexcept;
            };

            // =================================================================
            // ÉNUMÉRATION : NkRUDPFlags — Bits de contrôle de paquet
            // =================================================================
            /**
             * @enum NkRUDPFlags
             * @brief Flags binaires combinables dans NkRUDPHeader::flags.
             *
             * Chaque bit contrôle un aspect du traitement du paquet :
             *   • Fiabilité (ACK requis ou non)
             *   • Ordonnancement (livraison dans l'ordre ou non)
             *   • Type de paquet (données, ACK pur, contrôle)
             *   • État du payload (fragmenté, chiffré, etc.)
             *
             * @note Plusieurs flags peuvent être combinés par OU binaire (|)
             * @see NkRUDPHeader Pour l'utilisation dans l'en-tête
             */
            enum NkRUDPFlags : uint8
            {
                /// Paquet fiable : nécessite un accusé de réception (ACK) du destinataire.
                kFlagReliable = 1u << 0u,

                /// Paquet ordonné : doit être livré dans l'ordre strict des seqNum.
                kFlagOrdered = 1u << 1u,

                /// Paquet ACK pur : ne contient pas de payload, seulement des accusés.
                kFlagAck = 1u << 2u,

                /// Paquet de handshake : utilisé pour l'établissement de connexion.
                kFlagHandshake = 1u << 3u,

                /// Paquet de déconnexion : signal de fin de session gracieuse.
                kFlagDisconnect = 1u << 4u,

                /// Paquet heartbeat/ping : utilisé pour mesurer le RTT et détecter les déconnexions.
                kFlagPing = 1u << 5u,

                /// Paquet fragmenté : fait partie d'un message découpé en plusieurs datagrammes.
                kFlagFragment = 1u << 6u,

                /// Payload chiffré : les données suivant l'en-tête sont chiffrées.
                kFlagEncrypted = 1u << 7u
            };

            // =================================================================
            // STRUCTURE : NkSendEntry — Paquet en attente d'accusé de réception
            // =================================================================
            /**
             * @struct NkSendEntry
             * @brief Représente un paquet fiable envoyé mais pas encore acquitté.
             *
             * Stocké dans la fenêtre d'envoi circulaire (NkReliableChannel::mSendWindow).
             * Chaque entrée est conservée jusqu'à réception d'un ACK ou expiration du timeout.
             *
             * @note Taille fixe : kNkMaxPacketSize pour éviter allocations dynamiques
             * @threadsafe Lecture/écriture protégée par le mutex de NkReliableUDP si accès concurrent
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkSendEntry
            {
                // -------------------------------------------------------------
                // Données du paquet
                // -------------------------------------------------------------

                /// Buffer inline contenant le paquet complet (en-tête + payload).
                uint8 data[kNkMaxPacketSize] = {};

                /// Taille totale du paquet en bytes (en-tête RUDP inclus).
                uint32 size = 0;

                // -------------------------------------------------------------
                // Métadonnées de suivi
                // -------------------------------------------------------------

                /// Numéro de séquence assigné à ce paquet (pour matching avec ACK).
                uint32 seqNum = 0;

                /// Timestamp d'envoi initial en millisecondes (pour calcul de timeout).
                NkTimestampMs sentAt = 0;

                /// Nombre de tentatives de retransmission déjà effectuées.
                uint32 retransmits = 0;

                /// Indicateur de réception d'ACK : true = paquet confirmé, peut être libéré.
                bool acked = false;

                /// Canal logique associé (pour gestion par canal dans NkReliableChannel).
                NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;
            };

            // =================================================================
            // STRUCTURE : NkRecvEntry — Paquet reçu en attente de livraison
            // =================================================================
            /**
             * @struct NkRecvEntry
             * @brief Représente un paquet reçu et validé, en attente de livraison à l'application.
             *
             * Pour les canaux ORDERED : stocké dans mRecvBuffer jusqu'à ce que tous
             * les paquets précédents soient également reçus (pas de "trous" dans la séquence).
             *
             * Pour les canaux UNORDERED/UNRELIABLE : livré immédiatement via Drain().
             *
             * @note Taille fixe : kNkMaxPacketSize pour éviter allocations dynamiques
             * @see NkReliableChannel::DrainDeliverable() Pour extraction des paquets prêts
             */
            struct NKENTSEU_NETWORK_CLASS_EXPORT NkRecvEntry
            {
                // -------------------------------------------------------------
                // Données du paquet
                // -------------------------------------------------------------

                /// Buffer inline contenant le payload utile (sans en-tête RUDP).
                uint8 data[kNkMaxPacketSize] = {};

                /// Taille du payload utile en bytes (exclut l'en-tête RUDP de 20 bytes).
                uint32 size = 0;

                // -------------------------------------------------------------
                // Métadonnées de séquençage
                // -------------------------------------------------------------

                /// Numéro de séquence du paquet (pour ordonnancement et déduplication).
                uint32 seqNum = 0;

                /// Indicateur de validité : true = entrée active, false = slot libre.
                bool valid = false;
            };

            // =================================================================
            // CLASSE : NkRTTEstimator — Estimation du Round-Trip Time
            // =================================================================
            /**
             * @class NkRTTEstimator
             * @brief Estimeur de RTT (Round-Trip Time) et jitter utilisant l'algorithme de Jacobson/Karels.
             *
             * Cet estimateur calcule en temps réel :
             *   • RTT moyen lissé : pour adapter les timeouts de retransmission
             *   • Jitter (variation) : pour ajouter une marge de sécurité aux timeouts
             *   • RTO (Retransmission Timeout) : RTT + 4×jitter (formule standard TCP)
             *
             * ALGORITHME (Jacobson/Karels, RFC 6298) :
             *   Pour chaque échantillon de RTT (sampleMs) :
             *     RTT ← (1 - α) × RTT + α × sample    avec α = 0.125
             *     DEV ← (1 - β) × DEV + β × |RTT - sample|  avec β = 0.25
             *     RTO ← RTT + 4 × DEV
             *
             * @note Initialisé avec RTT=100ms, jitter=10ms pour démarrage prudent
             * @threadsafe Non thread-safe — protéger avec mutex si appelé depuis plusieurs threads
             *
             * @example
             * @code
             * NkRTTEstimator rttEst;
             *
             * // À chaque ACK reçu, mesurer le temps aller-retour
             * float32 sample = NkNetNowMs() - packetSentTime;
             * rttEst.Update(sample);
             *
             * // Utiliser le RTO pour décider quand retransmettre
             * if (NkNetNowMs() - lastSendTime > rttEst.GetRTO()) {
             *     RetransmitPacket();
             * }
             * @endcode
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkRTTEstimator
            {
            public:

                // -------------------------------------------------------------
                // Mise à jour de l'estimation
                // -------------------------------------------------------------

                /**
                 * @brief Met à jour les estimations RTT/jitter avec un nouvel échantillon.
                 * @param sampleMs Durée mesurée d'un aller-retour en millisecondes.
                 * @note Ignore les échantillons aberrants (> 10× RTT actuel) pour robustesse.
                 * @note Thread-safe en lecture seule après mise à jour (pas de protection interne).
                 */
                void Update(float32 sampleMs) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — valeurs estimées
                // -------------------------------------------------------------

                /**
                 * @brief Retourne l'estimation courante du RTT moyen.
                 * @return RTT lissé en millisecondes (valeur typique : 50-300ms).
                 */
                [[nodiscard]] float32 GetRTT() const noexcept;

                /**
                 * @brief Retourne l'estimation courante du jitter (variation du RTT).
                 * @return Jitter en millisecondes (valeur typique : 5-50ms).
                 */
                [[nodiscard]] float32 GetJitter() const noexcept;

                /**
                 * @brief Calcule le timeout de retransmission recommandé.
                 * @return RTO = RTT + 4×jitter en millisecondes.
                 * @note Utiliser cette valeur pour décider quand retransmettre un paquet non-ACKé.
                 */
                [[nodiscard]] float32 GetRTO() const noexcept;

            private:

                // -------------------------------------------------------------
                // État interne de l'estimateur
                // -------------------------------------------------------------

                /// Estimation lissée du RTT moyen en millisecondes.
                float32 mRTT = 100.f;

                /// Estimation lissée de la déviation (jitter) en millisecondes.
                float32 mJitter = 10.f;

                /// Coefficient de lissage pour RTT (α = 0.125 = 1/8).
                static constexpr float32 kAlpha = 0.125f;

                /// Coefficient de lissage pour jitter (β = 0.25 = 1/4).
                static constexpr float32 kBeta = 0.25f;
            };

            // =================================================================
            // CLASSE : NkReliableChannel — Gestion d'un canal RUDP unidirectionnel
            // =================================================================
            /**
             * @class NkReliableChannel
             * @brief Gère la fiabilité, l'ordonnancement et les ACKs pour un canal logique.
             *
             * Une instance est créée par canal (ReliableOrdered, ReliableUnordered).
             * Chaque canal maintient :
             *   • Une fenêtre d'envoi circulaire pour les paquets en attente d'ACK
             *   • Une fenêtre de réception pour réordonnancement des paquets entrants
             *   • Des compteurs de statistiques pour monitoring et debugging
             *
             * ALGORITHME DE FENÊTRE GLISSANTE :
             *   • Taille fixe : kNkWindowSize (64 paquets max en vol)
             *   • Head : prochain slot libre pour insertion d'un nouveau paquet
             *   • Tail : plus ancien paquet non-ACKé (bloquant l'avancée de la fenêtre)
             *   • La fenêtre avance quand Tail est ACKé et libéré
             *
             * @note Non thread-safe — synchronisation externe requise pour accès concurrents
             * @see NkReliableUDP Pour l'orchestration multi-canaux
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkReliableChannel
            {
            public:

                // -------------------------------------------------------------
                // Constructeur
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise un canal vide.
                 * @note Les fenêtres d'envoi/réception sont pré-allouées en stack.
                 */
                NkReliableChannel() noexcept = default;

                // -------------------------------------------------------------
                // Interface d'envoi — préparation et transmission
                // -------------------------------------------------------------

                /**
                 * @brief Prépare un paquet fiable pour envoi : assigne seqNum et stocke.
                 * @param data Pointeur vers les données à envoyer (payload utile).
                 * @param size Taille des données en bytes (doit être ≤ kNkMaxPayloadSize).
                 * @param ch Canal logique associé (pour gestion des garanties).
                 * @return Numéro de séquence assigné, ou UINT32_MAX si fenêtre d'envoi pleine.
                 * @note Ne transmet PAS le paquet — appeler GatherPendingSends() ensuite.
                 * @note Thread-safe en lecture seule après appel (pas de modification ultérieure).
                 */
                [[nodiscard]] uint32 PrepareReliable(
                    const uint8* data,
                    uint32 size,
                    NkNetChannel ch
                ) noexcept;

                /**
                 * @brief Collecte les paquets prêts à être (re)transmis.
                 * @param now Timestamp courant en millisecondes (pour calcul de timeout).
                 * @param rto Timeout de retransmission en millisecondes (via NkRTTEstimator).
                 * @param out Vecteur de sortie recevant les pointeurs vers entrées à envoyer.
                 * @note Remplit out avec : nouveaux paquets + paquets non-ACKés après timeout.
                 * @note Les entrées retournées restent valides jusqu'au prochain appel à Update().
                 */
                void GatherPendingSends(
                    NkTimestampMs now,
                    float32 rto,
                    NkVector<const NkSendEntry*>& out
                ) noexcept;

                // -------------------------------------------------------------
                // Interface de réception — traitement des ACKs et livraison
                // -------------------------------------------------------------

                /**
                 * @brief Traite un accusé de réception reçu du pair distant.
                 * @param ackNum Numéro de séquence ACKé de façon cumulative.
                 * @param ackMask Masque de 32 bits pour ACK sélectif des paquets précédents.
                 * @note Libère les entrées confirmées de la fenêtre d'envoi.
                 * @note Met à jour les statistiques de perte/paquets acquittés.
                 */
                void ProcessACK(uint32 ackNum, uint32 ackMask) noexcept;

                /**
                 * @brief Insère un paquet reçu dans la fenêtre de réception.
                 * @param seqNum Numéro de séquence du paquet reçu.
                 * @param data Pointeur vers le payload utile (sans en-tête RUDP).
                 * @param size Taille du payload en bytes.
                 * @return true si le paquet est nouveau et accepté, false si doublon ou hors fenêtre.
                 * @note Pour canaux ORDERED : stocke en attente d'ordonnancement.
                 * @note Pour canaux UNORDERED : marque comme prêt à livraison immédiate.
                 */
                [[nodiscard]] bool InsertReceived(
                    uint32 seqNum,
                    const uint8* data,
                    uint32 size
                ) noexcept;

                /**
                 * @brief Extrait les paquets prêts à être livrés à l'application.
                 * @param out Vecteur de sortie recevant les entrées livrables.
                 * @note Pour canaux ORDERED : ne livre que les paquets consécutifs depuis mExpectedSeq.
                 * @note Pour canaux UNORDERED : livre tous les paquets valides immédiatement.
                 * @note Les entrées livrées sont marquées invalides et réutilisables.
                 */
                void DrainDeliverable(NkVector<NkRecvEntry>& out) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — état et statistiques du canal
                // -------------------------------------------------------------

                /**
                 * @brief Retourne le prochain numéro ACK à envoyer au pair distant.
                 * @return Numéro de séquence du dernier paquet reçu en ordre.
                 * @note Utiliser dans l'en-tête des paquets sortants pour ACK cumulatif.
                 */
                [[nodiscard]] uint32 GetOutgoingAckNum() const noexcept;

                /**
                 * @brief Retourne le masque ACK sélectif à envoyer au pair distant.
                 * @return Masque de 32 bits indiquant les paquets reçus après ackNum.
                 * @note Utiliser dans l'en-tête des paquets sortants pour ACK sélectif.
                 */
                [[nodiscard]] uint32 GetOutgoingAckMask() const noexcept;

                /**
                 * @brief Retourne le nombre de paquets en attente d'ACK dans la fenêtre d'envoi.
                 * @return Compteur de paquets "in flight" (0 à kNkWindowSize).
                 * @note Utile pour flow control : éviter d'envoyer si fenêtre saturée.
                 */
                [[nodiscard]] uint32 GetPendingCount() const noexcept;

                /**
                 * @brief Estime le taux de perte de paquets sur ce canal.
                 * @return Pourcentage de perte (0.0 à 1.0), calculé sur les 128 derniers paquets.
                 * @note Valeur lissée pour éviter les fluctuations brutales.
                 */
                [[nodiscard]] float32 GetPacketLoss() const noexcept;

            private:

                // -------------------------------------------------------------
                // Fenêtre d'envoi — paquets en attente d'ACK
                // -------------------------------------------------------------

                /// Buffer circulaire des paquets envoyés mais pas encore acquittés.
                NkSendEntry mSendWindow[kNkWindowSize] = {};

                /// Index du prochain slot libre pour insertion (écriture).
                uint32 mSendHead = 0;

                /// Index du plus ancien paquet non-ACKé (lecture, bloquant l'avancée).
                uint32 mSendTail = 0;

                /// Prochain numéro de séquence à assigner (monotone, commence à 1).
                uint32 mNextSeqNum = 1;

                // -------------------------------------------------------------
                // Fenêtre de réception — paquets reçus en attente de livraison
                // -------------------------------------------------------------

                /// Buffer circulaire des paquets reçus mais pas encore livrés.
                NkRecvEntry mRecvBuffer[kNkWindowSize] = {};

                /// Prochain numéro de séquence attendu pour livraison ordonnée.
                uint32 mExpectedSeq = 1;

                /// Dernier numéro de séquence reçu (pour ACK cumulatif).
                uint32 mLastReceivedSeq = 0;

                /// Masque de bits pour ACK sélectif des 32 paquets précédant mLastReceivedSeq.
                uint32 mReceivedMask = 0;

                // -------------------------------------------------------------
                // Statistiques — monitoring et debugging
                // -------------------------------------------------------------

                /// Compteur total de paquets envoyés (pour calcul de perte).
                uint32 mTotalSent = 0;

                /// Compteur total de paquets acquittés (pour calcul de perte).
                uint32 mTotalAcked = 0;

                /// Compteur total de retransmissions effectuées (indicateur de congestion).
                uint32 mRetransmits = 0;

                /// Taux de perte estimé (0.0 à 1.0), mis à jour périodiquement.
                float32 mPacketLoss = 0.f;
            };

            // =================================================================
            // CLASSE : NkReliableUDP — Orchestration complète RUDP
            // =================================================================
            /**
             * @class NkReliableUDP
             * @brief Couche RUDP complète gérant la fiabilité pour une connexion peer-to-peer.
             *
             * Cette classe orchestre :
             *   • Multiplexage de plusieurs canaux logiques (Ordered, Unordered, Sequenced)
             *   • Estimation adaptative du RTT et gestion des timeouts de retransmission
             *   • Fragmentation automatique des messages dépassant la MTU
             *   • Réassemblage des fragments reçus en messages complets
             *   • Envoi périodique d'ACKs et de heartbeats pour maintenir la connexion
             *
             * CYCLE DE VIE TYPIQUE :
             * @code
             * // 1. Initialisation
             * NkReliableUDP rudp;
             * rudp.Init(&socket, remoteAddress);
             *
             * // 2. Boucle principale (chaque frame)
             * void GameLoop(float32 dt) {
             *     // a) Traitement des retransmissions et ACKs
             *     rudp.Update(dt);
             *
             *     // b) Envoi de données applicatives
             *     rudp.Send(playerInput, sizeof(playerInput), NkNetChannel::Unreliable);
             *     rudp.Send(chatMessage, msgSize, NkNetChannel::ReliableOrdered);
             *
             *     // c) Réception des données réseau (appelé depuis NkConnectionManager)
             *     //    Quand un datagramme UDP est reçu :
             *     rudp.OnReceived(rawBuffer, rawSize);
             *
             *     // d) Consommation des paquets livrés
             *     NkVector<NkRecvEntry> delivered;
             *     rudp.Drain(delivered);
             *     for (const auto& pkt : delivered) {
             *         ProcessGameMessage(pkt.data, pkt.size);
             *     }
             * }
             *
             * // 3. Nettoyage (optionnel, RAII gère automatiquement)
             * // rudp.~NkReliableUDP(); // Fermeture gracieuse si nécessaire
             * @endcode
             *
             * @note Une instance par connexion peer-to-peer — ne pas partager entre pairs
             * @note Non thread-safe — protéger avec NkMutex si accès depuis plusieurs threads
             * @see NkConnection Pour l'intégration dans le système de connexion haut-niveau
             */
            class NKENTSEU_NETWORK_CLASS_EXPORT NkReliableUDP
            {
            public:

                // -------------------------------------------------------------
                // Constructeur et règles de copie
                // -------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut — initialise un état vide.
                 * @note Appeler Init() avant toute utilisation pour configurer socket et adresse.
                 */
                NkReliableUDP() noexcept = default;

                /**
                 * @brief Destructeur par défaut — libération automatique des ressources.
                 * @note Aucune allocation heap dans cette classe — destruction triviale.
                 */
                ~NkReliableUDP() noexcept = default;

                // Suppression de la copie — une connexion RUDP ne peut être dupliquée.
                NkReliableUDP(const NkReliableUDP&) = delete;
                NkReliableUDP& operator=(const NkReliableUDP&) = delete;

                // -------------------------------------------------------------
                // Initialisation
                // -------------------------------------------------------------

                /**
                 * @brief Initialise l'instance avec un socket et une adresse distante.
                 * @param socket Pointeur vers le socket UDP sous-jacent (non transféré).
                 * @param remote Adresse du pair distant pour l'envoi des paquets.
                 * @note La socket doit être valide et configurée en mode non-bloquant.
                 * @note Thread-safe : peut être appelée avant tout accès concurrent.
                 */
                void Init(NkSocket* socket, const NkAddress& remote) noexcept;

                // -------------------------------------------------------------
                // Mise à jour — traitement périodique
                // -------------------------------------------------------------

                /**
                 * @brief Traite les retransmissions, l'envoi d'ACKs et les heartbeats.
                 * @param dt Delta-time depuis le dernier appel en secondes (pour lissage).
                 * @note À appeler chaque frame depuis NkConnection::Update().
                 * @note Envoie automatiquement les ACKs en attente si timer écoulé.
                 * @note Déclenche les retransmissions pour paquets non-ACKés après timeout.
                 */
                void Update(float32 dt) noexcept;

                // -------------------------------------------------------------
                // Interface d'envoi — transmission de messages applicatifs
                // -------------------------------------------------------------

                /**
                 * @brief Envoie un message sur le canal logique spécifié.
                 * @param data Pointeur vers les données à envoyer (payload applicatif).
                 * @param size Taille des données en bytes.
                 * @param channel Canal logique avec garanties de livraison souhaitées.
                 * @return NkNetResult::OK en cas de succès, code d'erreur sinon.
                 * @note Si size > kNkMaxPayloadSize : fragmentation automatique en multiples datagrammes.
                 * @note Pour canaux fiables : le paquet est stocké en attente d'ACK avant envoi réel.
                 * @note Thread-safe : protection interne via mutex si compilé avec NKNET_THREAD_SAFE.
                 */
                [[nodiscard]] NkNetResult Send(
                    const uint8* data,
                    uint32 size,
                    NkNetChannel channel
                ) noexcept;

                /**
                 * @brief Envoie un accusé de réception pur (sans payload applicatif).
                 * @return NkNetResult::OK en cas de succès, code d'erreur sinon.
                 * @note Appelé automatiquement par Update() quand des ACKs sont en attente.
                 * @note Peut être appelé manuellement pour forcer un ACK immédiat (optimisation).
                 */
                [[nodiscard]] NkNetResult SendACK() noexcept;

                // -------------------------------------------------------------
                // Interface de réception — traitement des datagrammes entrants
                // -------------------------------------------------------------

                /**
                 * @brief Traite un datagramme UDP brut reçu du réseau.
                 * @param rawData Pointeur vers le buffer contenant le datagramme complet.
                 * @param rawSize Taille totale du datagramme en bytes (en-tête RUDP inclus).
                 * @note Valide l'en-tête RUDP (magic, taille, checksum si activé).
                 * @note Extrait et traite les ACKs pour libérer les paquets envoyés.
                 * @note Insère le payload dans le canal approprié pour livraison ultérieure.
                 * @note Met à jour l'estimateur RTT si le paquet contient un ping/ACK.
                 */
                void OnReceived(const uint8* rawData, uint32 rawSize) noexcept;

                /**
                 * @brief Extrait les messages livrés et prêts à consommation applicative.
                 * @param out Vecteur de sortie recevant les paquets livrés.
                 * @note Vide les buffers internes des canaux après extraction.
                 * @note Les entrées retournées sont valides jusqu'au prochain appel à Drain().
                 * @note À appeler après OnReceived() pour traiter les nouveaux messages.
                 */
                void Drain(NkVector<NkRecvEntry>& out) noexcept;

                // -------------------------------------------------------------
                // Accesseurs — métriques de connexion et diagnostic
                // -------------------------------------------------------------

                /**
                 * @brief Retourne l'estimation courante du RTT vers le pair distant.
                 * @return RTT moyen en millisecondes (valeur typique : 50-300ms).
                 */
                [[nodiscard]] float32 GetRTT() const noexcept;

                /**
                 * @brief Retourne l'estimation courante du jitter (variation du RTT).
                 * @return Jitter en millisecondes (valeur typique : 5-50ms).
                 */
                [[nodiscard]] float32 GetJitter() const noexcept;

                /**
                 * @brief Estime le taux de perte de paquets sur la connexion.
                 * @return Pourcentage de perte (0.0 à 1.0), moyenne sur tous les canaux fiables.
                 * @note Utile pour adaptation dynamique du débit ou détection de congestion.
                 */
                [[nodiscard]] float32 GetPacketLoss() const noexcept;

                /**
                 * @brief Retourne le ping estimé en millisecondes (arrondi de GetRTT()).
                 * @return Ping entier en ms pour affichage UI ou logging.
                 */
                [[nodiscard]] uint32 GetPingMs() const noexcept;

                /**
                 * @brief Retourne l'adresse du pair distant associé à cette connexion.
                 * @return Référence constante vers l'adresse configurée à l'initialisation.
                 */
                [[nodiscard]] const NkAddress& GetRemote() const noexcept;

                /**
                 * @brief Met à jour l'estimateur RTT avec un échantillon mesuré via PING/PONG.
                 * @param sampleMs Durée aller-retour mesurée en millisecondes.
                 * @note Complémente la mesure interne par ACKs RUDP avec la mesure applicative.
                 * @note Utilise l'algorithme de Jacobson/Karels (RFC 6298) pour lissage adaptatif.
                 * @example rtt = NkNetNowMs() - pingSentAt; rudp.UpdateRTT(rtt);
                 */
                void UpdateRTT(float32 sampleMs) noexcept;

                /**
                 * @brief Retourne le timestamp du dernier paquet reçu.
                 * @return Timestamp en millisecondes depuis le début de session.
                 * @note Utiliser pour détection de déconnexion par timeout d'inactivité.
                 */
                [[nodiscard]] NkTimestampMs GetLastRecv() const noexcept;

            private:

                // -------------------------------------------------------------
                // Méthodes privées — fragmentation et réassemblage
                // -------------------------------------------------------------

                /**
                 * @brief Envoie un message fragmenté en multiples datagrammes UDP.
                 * @param data Pointeur vers les données complètes à fragmenter.
                 * @param size Taille totale du message en bytes.
                 * @param ch Canal logique à utiliser pour tous les fragments.
                 * @return NkNetResult::OK si tous les fragments ont été envoyés, erreur sinon.
                 * @note Chaque fragment porte un en-tête RUDP avec fragIdx/fragCount.
                 * @note Tous les fragments utilisent le même seqNum de base pour réassemblage.
                 */
                [[nodiscard]] NkNetResult SendFragmented(
                    const uint8* data,
                    uint32 size,
                    NkNetChannel ch
                ) noexcept;

                /**
                 * @brief Tente de réassembler un message à partir de fragments reçus.
                 * @param fragIdx Index du fragment reçu (0-based).
                 * @param fragCount Nombre total de fragments attendus.
                 * @param seqBase Numéro de séquence de base pour ce message fragmenté.
                 * @param data Payload du fragment reçu (sans en-tête RUDP).
                 * @param size Taille du payload du fragment en bytes.
                 * @return true si le message est complet et livré, false si en attente de fragments.
                 * @note Stocke les fragments dans mFragBuffer jusqu'à complétude.
                 * @note Libère le buffer après livraison ou timeout de réassemblage.
                 */
                [[nodiscard]] bool TryReassemble(
                    uint8 fragIdx,
                    uint8 fragCount,
                    uint32 seqBase,
                    const uint8* data,
                    uint32 size
                ) noexcept;

                // -------------------------------------------------------------
                // Membres privés — état de la connexion RUDP
                // -------------------------------------------------------------

                /// Pointeur vers le socket UDP sous-jacent (non transféré, non possédé).
                NkSocket* mSocket = nullptr;

                /// Adresse du pair distant pour l'envoi des paquets sortants.
                NkAddress mRemote;

                /// Timestamp du dernier paquet reçu (pour détection de déconnexion).
                NkTimestampMs mLastRecvAt = 0;

                /// Timestamp du dernier envoi d'ACK (pour throttling des ACKs).
                NkTimestampMs mLastAckAt = 0;

                /// Timer accumulé pour envoi périodique d'ACKs (en secondes).
                float32 mAckTimer = 0.f;

                /// Canal pour messages fiables avec livraison ordonnée.
                NkReliableChannel mRelOrd;

                /// Canal pour messages fiables sans garantie d'ordre.
                NkReliableChannel mRelUnord;

                /// Estimateur de RTT/jitter pour adaptation dynamique des timeouts.
                NkRTTEstimator mRTT;

                /// Timestamp d'envoi du dernier ping (pour calcul de RTT).
                NkTimestampMs mPingSentAt = 0;

                /// Numéro de séquence du dernier ping envoyé (pour matching avec réponse).
                uint32 mPingSeqNum = 0;

                // -------------------------------------------------------------
                // Buffer de réassemblage de fragments
                // -------------------------------------------------------------
                struct FragBuffer
                {
                    /// Buffer linéaire pour stockage des fragments reçus.
                    uint8 data[kNkMaxPacketSize * kNkMaxFragments] = {};

                    /// Tableau de flags indiquant quels fragments ont été reçus.
                    uint8 received[kNkMaxFragments] = {};

                    /// Nombre total de fragments attendus pour ce message.
                    uint8 total = 0;

                    /// Nombre de fragments déjà reçus et validés.
                    uint8 count = 0;

                    /// Numéro de séquence de base pour ce message fragmenté.
                    uint32 seqBase = 0;

                    /// Indicateur d'activité : true = réassemblage en cours.
                    bool active = false;
                } mFragBuffer;

                /// File d'attente pour paquets non-fiables reçus (livraison immédiate).
                NkVector<NkRecvEntry> mUnreliableQueue;

                /// Dernier numéro de séquence reçu sur le canal non-fiable (pour déduplication).
                uint32 mSeqLastUnrel = 0;
            };

        } // namespace net

    } // namespace nkentseu

#endif // NKENTSEU_NETWORK_NKRELIABLEUDP_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKRELIABLEUDP.H
// =============================================================================
// Ce fichier fournit la couche de fiabilité RUDP pour communications UDP temps réel.
// Les exemples suivants illustrent les cas d'usage courants.

// -----------------------------------------------------------------------------
// Exemple 1 : Intégration basique dans une boucle de jeu
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkReliableUDP.h"
    #include "NKCore/Logger/NkLogger.h"

    class GameNetwork
    {
    public:
        void Initialize()
        {
            using namespace nkentseu::net;

            // Création et configuration du socket UDP
            mSocket.Create(NkAddress::Any(0), NkSocket::Type::UDP);
            mSocket.SetNonBlocking(true);

            // Initialisation de la couche RUDP
            NkAddress serverAddr("game.example.com", 7777);
            mRUDP.Init(&mSocket, serverAddr);

            NK_LOG_INFO("Connexion RUDP initialisée vers {0}", serverAddr.ToString());
        }

        void Update(float32 dt)
        {
            using namespace nkentseu::net;

            // 1. Traitement des retransmissions et ACKs
            mRUDP.Update(dt);

            // 2. Envoi des données de jeu
            SendPlayerInput();
            SendHeartbeatIfNeeded();

            // 3. Réception des paquets entrants
            ReceivePackets();

            // 4. Traitement des messages livrés
            ProcessDeliveredMessages();
        }

    private:
        void SendPlayerInput()
        {
            using namespace nkentseu::net;

            // Input joueur : canal Unreliable (perte acceptable, latence critique)
            PlayerInput input = GetCurrentInput();
            mRUDP.Send(
                reinterpret_cast<const uint8*>(&input),
                sizeof(input),
                NkNetChannel::NK_NET_CHANNEL_UNRELIABLE
            );
        }

        void ReceivePackets()
        {
            using namespace nkentseu::net;

            uint8 buffer[kNkMaxPacketSize];
            uint32 receivedSize = 0;
            NkAddress from;

            // Lecture de tous les paquets disponibles (mode non-bloquant)
            while (mSocket.RecvFrom(buffer, sizeof(buffer), receivedSize, from) == NkNetResult::NK_NET_OK)
            {
                if (receivedSize > 0)
                {
                    // Traitement par la couche RUDP
                    mRUDP.OnReceived(buffer, receivedSize);
                }
            }
        }

        void ProcessDeliveredMessages()
        {
            using namespace nkentseu::net;

            NkVector<NkRecvEntry> delivered;
            mRUDP.Drain(delivered);

            for (const auto& pkt : delivered)
            {
                // Dispatch selon le type de message (premier byte = message ID)
                if (pkt.size > 0)
                {
                    uint8 messageId = pkt.data[0];
                    HandleGameMessage(messageId, pkt.data + 1, pkt.size - 1);
                }
            }
        }

        NkSocket mSocket;
        NkReliableUDP mRUDP;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion des canaux avec garanties différentes
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkReliableUDP.h"

    void ChannelUsageExample(nkentseu::net::NkReliableUDP& rudp)
    {
        using namespace nkentseu::net;

        // Position du joueur : Unreliable (perte acceptable, latence minimale)
        PlayerPosition pos = GetCurrentPosition();
        rudp.Send(
            reinterpret_cast<const uint8*>(&pos),
            sizeof(pos),
            NkNetChannel::NK_NET_CHANNEL_UNRELIABLE
        );

        // Message de chat : ReliableOrdered (garanti + dans l'ordre)
        NkString chatMsg = "Hello from client!";
        rudp.Send(
            reinterpret_cast<const uint8*>(chatMsg.CStr()),
            static_cast<uint32>(chatMsg.Length()),
            NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
        );

        // Mise à jour d'inventaire : ReliableUnordered (garanti, ordre non critique)
        InventoryUpdate invUpdate = GetInventoryDelta();
        rudp.Send(
            reinterpret_cast<const uint8*>(&invUpdate),
            sizeof(invUpdate),
            NkNetChannel::NK_NET_CHANNEL_RELIABLE_UNORDERED
        );

        // État de santé : Sequenced (seul le plus récent compte)
        HealthUpdate health = GetCurrentHealth();
        rudp.Send(
            reinterpret_cast<const uint8*>(&health),
            sizeof(health),
            NkNetChannel::NK_NET_CHANNEL_SEQUENCED
        );
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Monitoring et adaptation dynamique
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/Transport/NkReliableUDP.h"
    #include "NKCore/Logger/NkLogger.h"

    void AdaptiveSendExample(nkentseu::net::NkReliableUDP& rudp, const void* data, uint32 size)
    {
        using namespace nkentseu::net;

        // Récupération des métriques de connexion
        float32 rtt = rudp.GetRTT();
        float32 jitter = rudp.GetJitter();
        float32 loss = rudp.GetPacketLoss();

        // Adaptation du canal selon les conditions réseau
        NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED;

        if (loss > 0.1f)  // Plus de 10% de perte
        {
            // Réduire la fréquence d'envoi ou utiliser un canal plus tolérant
            NK_LOG_WARN("Perte élevée détectée : {:.1f}%", loss * 100.f);
            // Option : passer à Unreliable pour données non-critiques
        }

        if (rtt > 300.f)  // Latence > 300ms
        {
            // Prédiction côté client ou interpolation pour masquer la latence
            NK_LOG_WARN("Latence élevée : {:.0f}ms", rtt);
        }

        // Envoi avec le canal sélectionné
        rudp.Send(static_cast<const uint8*>(data), size, channel);

        // Logging périodique des métriques (toutes les 5 secondes)
        static NkTimestampMs sLastLog = 0;
        if (NkNetNowMs() - sLastLog > 5000)
        {
            NK_LOG_INFO("Connexion : RTT={:.0f}ms, Jitter={:.0f}ms, Perte={:.1f}%",
                rtt, jitter, loss * 100.f);
            sLastLog = NkNetNowMs();
        }
    }
*/

// -----------------------------------------------------------------------------
// Bonnes pratiques et notes d'utilisation
// -----------------------------------------------------------------------------
/*
    Gestion des canaux :
    -------------------
    - Choisir le canal en fonction du compromis latence/fiabilité requis
    - Unreliable : positions, inputs, animations — données éphémères
    - ReliableOrdered : chat, événements gameplay, commandes critiques
    - ReliableUnordered : transferts de fichiers, chunks indépendants
    - Sequenced : états continus (health, mana) — les anciens sont obsolètes

    Fragmentation :
    --------------
    - Payload max par datagramme : kNkMaxPayloadSize (1380 bytes)
    - Messages plus gros sont fragmentés automatiquement
    - Tous les fragments partagent le même seqNum de base pour réassemblage
    - Timeout de réassemblage : libère le buffer si fragments manquants après délai

    Retransmission :
    ---------------
    - Timeout adaptatif : RTO = RTT + 4×jitter (algorithme Jacobson/Karels)
    - Nombre max de retransmissions : kNkMaxRetransmits (5 par défaut)
    - Après échec : notifier l'application via code de retour ou callback

    Flow control :
    -------------
    - Fenêtre d'envoi limitée à kNkWindowSize (64 paquets)
    - Vérifier GetPendingCount() avant d'envoyer pour éviter saturation
    - En cas de fenêtre pleine : bufferiser côté application ou rejeter

    Détection de déconnexion :
    -------------------------
    - Utiliser GetLastRecv() pour mesurer l'inactivité
    - Timeout typique : 10-30 secondes sans paquet reçu
    - Envoyer des heartbeats périodiques pour maintenir la connexion

    Thread-safety :
    --------------
    - NkReliableUDP n'est PAS thread-safe par défaut
    - Protéger l'accès concurrent avec NkMutex si nécessaire
    - Alternative : un thread dédié par connexion avec queue de messages

    Performance :
    ------------
    - Buffers inline : aucune allocation heap dans le chemin critique
    - Sérialisation/désérialisation optimisée (pas de copies inutiles)
    - Estimation RTT en O(1) avec formules de lissage simples

    Debugging :
    ----------
    - Utiliser GetPacketLoss() et GetJitter() pour diagnostic réseau
    - Logger les retransmissions excessives (indicateur de congestion)
    - Vérifier la cohérence des seqNum pour détecter les bugs de protocole
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================