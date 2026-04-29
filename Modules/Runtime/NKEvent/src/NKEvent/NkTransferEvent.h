// =============================================================================
// NKEvent/NkTransferEvent.h
// Hiérarchie des événements de transfert de données et de fichiers.
//
// Catégorie : NK_CAT_TRANSFER
//
// Architecture :
//   - NkTransferDirection  : sens du transfert (envoi vs réception)
//   - NkTransferProtocol   : canal/protocole utilisé pour le transfert
//   - NkTransferStatus     : résultat final d'un transfert (succès/erreur)
//   - NkTransferEvent      : classe de base abstraite pour tous les transferts
//   - Classes dérivées     : événements concrets du cycle de vie d'un transfert
//
// Cycle de vie typique d'un transfert :
//   1. NkTransferBeginEvent      : démarrage, métadonnées initiales
//   2. N × NkTransferProgressEvent : mises à jour périodiques de progression
//   3. Terminal : l'un des trois suivants :
//      - NkTransferCompleteEvent   : succès, données disponibles
//      - NkTransferErrorEvent      : échec avec code d'erreur
//      - NkTransferCancelledEvent  : annulation volontaire par l'utilisateur
//
// Streaming en temps réel :
//   - NkTransferDataEvent : paquets de données brutes pour traitement immédiat
//   - Utilisé pour audio, vidéo, réseau temps-réel où le buffering n'est pas souhaité
//
// Usage type :
//   // Démarrer un transfert
//   uint64 transferId = TransferManager::StartDownload(url, destination);
//
//   // Suivre la progression
//   eventMgr.Subscribe<NkTransferProgressEvent>(
//       [](NkTransferProgressEvent& evt) {
//           float percent = evt.GetProgressPercent();
//           if (percent >= 0) {
//               ProgressBar::Update(percent);
//           }
//       }
//   );
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKTRANSFEREVENT_H
#define NKENTSEU_EVENT_NKTRANSFEREVENT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString
    #include "NKCore/NkTraits.h"                        // Utilitaires de traits (NkMove, etc.)

    namespace nkentseu {

        // =====================================================================
        // NkTransferDirection — Énumération du sens de transfert
        // =====================================================================

        /// @brief Représente la direction d'un transfert de données
        /// @note Utilisé pour distinguer upload (envoi) et download (réception)
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkTransferDirection : uint32 {
            NK_TRANSFER_SEND    = 0,    ///< Envoi de données (upload, write, export)
            NK_TRANSFER_RECEIVE = 1     ///< Réception de données (download, read, import)
        };

        // =====================================================================
        // NkTransferProtocol — Énumération des protocoles/canaux de transfert
        // =====================================================================

        /// @brief Représente le protocole ou canal utilisé pour un transfert
        /// @note Permet d'adapter le comportement selon le type de connexion
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkTransferProtocol : uint32 {
            NK_TRANSFER_PROTO_UNKNOWN  = 0,   ///< Protocole non identifié ou non supporté
            NK_TRANSFER_PROTO_FILE,           ///< Fichier local (copie, déplacement, I/O direct)
            NK_TRANSFER_PROTO_HTTP,           ///< HTTP/HTTPS (web, API REST, téléchargement)
            NK_TRANSFER_PROTO_FTP,            ///< FTP/FTPS (transfert de fichiers legacy)
            NK_TRANSFER_PROTO_BLUETOOTH,      ///< Bluetooth (pairing, transfert sans fil courte portée)
            NK_TRANSFER_PROTO_USB,            ///< USB (mass storage, MTP, ADB, connexion filaire)
            NK_TRANSFER_PROTO_IPC,            ///< IPC (pipe, socket locale, communication inter-processus)
            NK_TRANSFER_PROTO_CUSTOM,         ///< Canal défini par l'application (extension personnalisée)
            NK_TRANSFER_PROTO_MAX             ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        /// @brief Convertit une valeur NkTransferProtocol en chaîne lisible pour le débogage
        /// @param p La valeur de protocole à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkTransferProtocolToString(NkTransferProtocol p) noexcept {
            switch (p) {
                case NkTransferProtocol::NK_TRANSFER_PROTO_FILE:
                    return "FILE";
                case NkTransferProtocol::NK_TRANSFER_PROTO_HTTP:
                    return "HTTP";
                case NkTransferProtocol::NK_TRANSFER_PROTO_FTP:
                    return "FTP";
                case NkTransferProtocol::NK_TRANSFER_PROTO_BLUETOOTH:
                    return "BT";
                case NkTransferProtocol::NK_TRANSFER_PROTO_USB:
                    return "USB";
                case NkTransferProtocol::NK_TRANSFER_PROTO_IPC:
                    return "IPC";
                case NkTransferProtocol::NK_TRANSFER_PROTO_CUSTOM:
                    return "CUSTOM";
                case NkTransferProtocol::NK_TRANSFER_PROTO_UNKNOWN:
                default:
                    return "UNKNOWN";
            }
        }

        // =====================================================================
        // NkTransferStatus — Énumération des résultats de transfert
        // =====================================================================

        /// @brief Représente le résultat final d'un transfert de données
        /// @note Utilisé par NkTransferErrorEvent pour catégoriser les échecs
        enum class NKENTSEU_EVENT_CLASS_EXPORT NkTransferStatus : uint32 {
            NK_TRANSFER_STATUS_SUCCESS   = 0,   ///< Transfert terminé avec succès
            NK_TRANSFER_STATUS_ERROR,           ///< Erreur générique (réseau, I/O, système)
            NK_TRANSFER_STATUS_TIMEOUT,         ///< Délai d'attente dépassé (timeout)
            NK_TRANSFER_STATUS_DENIED,          ///< Accès refusé (permissions, authentification)
            NK_TRANSFER_STATUS_CORRUPTED,       ///< Données corrompues (checksum/CRC invalide)
            NK_TRANSFER_STATUS_CANCELLED,       ///< Annulation volontaire par l'utilisateur ou l'application
            NK_TRANSFER_STATUS_MAX              ///< Sentinelle : nombre de valeurs valides (pour validation)
        };

        /// @brief Convertit une valeur NkTransferStatus en chaîne lisible pour le débogage
        /// @param s La valeur de statut à convertir
        /// @return Pointeur vers une chaîne littérale statique (ne pas libérer)
        /// @note Fonction noexcept pour usage dans les contextes critiques
        inline const char* NkTransferStatusToString(NkTransferStatus s) noexcept {
            switch (s) {
                case NkTransferStatus::NK_TRANSFER_STATUS_SUCCESS:
                    return "SUCCESS";
                case NkTransferStatus::NK_TRANSFER_STATUS_ERROR:
                    return "ERROR";
                case NkTransferStatus::NK_TRANSFER_STATUS_TIMEOUT:
                    return "TIMEOUT";
                case NkTransferStatus::NK_TRANSFER_STATUS_DENIED:
                    return "DENIED";
                case NkTransferStatus::NK_TRANSFER_STATUS_CORRUPTED:
                    return "CORRUPTED";
                case NkTransferStatus::NK_TRANSFER_STATUS_CANCELLED:
                    return "CANCELLED";
                case NkTransferStatus::NK_TRANSFER_STATUS_MAX:
                default:
                    return "UNKNOWN";
            }
        }

        // =====================================================================
        // NkTransferEvent — Classe de base abstraite pour événements de transfert
        // =====================================================================

        /**
         * @brief Classe de base polymorphe pour tous les événements de transfert de données
         *
         * Catégorie associée : NK_CAT_TRANSFER
         *
         * Cette classe sert de point d'ancrage commun pour tous les événements
         * liés au cycle de vie d'un transfert : début, progression, complétion, erreur.
         *
         * @note Chaque transfert est identifié par un mTransferId unique (uint64)
         *       permettant de corréler tous les événements d'un même transfert,
         *       même en cas de transferts multiples concurrents.
         *
         * @note Les classes dérivées DOIVENT utiliser les macros NK_EVENT_TYPE_FLAGS
         *       et NK_EVENT_CATEGORY_FLAGS pour éviter la duplication de code.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferEvent : public NkEvent {
            public:
                /// @brief Implémente le filtrage par catégorie TRANSFER pour tous les dérivés
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_TRANSFER)

                /// @brief Retourne l'identifiant unique du transfert associé
                /// @return uint64 représentant l'ID du transfert (généré par TransferManager)
                /// @note Cet ID permet de corréler Begin/Progress/Complete/Error pour un même transfert
                NKENTSEU_EVENT_API_INLINE uint64 GetTransferId() const noexcept {
                    return mTransferId;
                }

            protected:
                /// @brief Constructeur protégé — réservé aux classes dérivées
                /// @param transferId Identifiant unique du transfert à associer
                /// @param windowId Identifiant de la fenêtre source (0 = événement global)
                NKENTSEU_EVENT_API NkTransferEvent(uint64 transferId, uint64 windowId = 0) noexcept
                    : NkEvent(windowId)
                    , mTransferId(transferId) {
                }

                uint64 mTransferId;  ///< Identifiant unique du transfert (corrélation événements)
        };

        // =====================================================================
        // NkTransferBeginEvent — Démarrage d'un transfert
        // =====================================================================

        /**
         * @brief Émis au démarrage d'un transfert de données
         *
         * Cet événement initialise le suivi d'un transfert et transporte les métadonnées :
         *   - Nom lisible (nom de fichier, URI, description)
         *   - Taille totale attendue (0 si inconnue, ex: streaming, chunked transfer)
         *   - Direction (envoi vs réception)
         *   - Protocole utilisé (HTTP, FTP, fichier local, etc.)
         *
         * @note totalBytes == 0 signifie que la taille totale est inconnue.
         *       Dans ce cas, GetProgressPercent() retournera -1.0 pendant le transfert.
         *
         * @note Cet événement marque le début du cycle de vie : les handlers
         *       peuvent l'utiliser pour initialiser des UI de progression,
         *       allouer des buffers, ou logger le démarrage du transfert.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferBeginEvent final : public NkTransferEvent {
            public:
                /// @brief Associe le type d'événement NK_TRANSFER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

                /**
                 * @brief Constructeur avec métadonnées complètes du transfert
                 * @param transferId Identifiant unique du transfert (généré par TransferManager)
                 * @param name Nom lisible (nom de fichier, URI, description utilisateur)
                 * @param totalBytes Taille totale attendue en octets (0 = inconnue/streaming)
                 * @param direction Sens du transfert (SEND = upload, RECEIVE = download)
                 * @param protocol Protocole/canal utilisé pour le transfert
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTransferBeginEvent(
                    uint64 transferId,
                    NkString name,
                    uint64 totalBytes = 0,
                    NkTransferDirection direction = NkTransferDirection::NK_TRANSFER_RECEIVE,
                    NkTransferProtocol protocol = NkTransferProtocol::NK_TRANSFER_PROTO_UNKNOWN,
                    uint64 windowId = 0
                )
                    : NkTransferEvent(transferId, windowId)
                    , mName(traits::NkMove(name))
                    , mTotalBytes(totalBytes)
                    , mDirection(direction)
                    , mProtocol(protocol) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTransferBeginEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'ID, le nom, la taille et le protocole
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "TransferBegin(id={0} \"{1}\" size={2}B {3})",
                        mTransferId,
                        mName,
                        (mTotalBytes > 0 ? NkString::Fmt("{0}", mTotalBytes) : "?"),
                        NkTransferProtocolToString(mProtocol)
                    );
                }

                /// @brief Retourne le nom lisible du transfert
                /// @return const NkString& référence vers le nom (lecture seule)
                NKENTSEU_EVENT_API_INLINE const NkString& GetName() const noexcept {
                    return mName;
                }

                /// @brief Retourne la taille totale attendue du transfert
                /// @return uint64 représentant le nombre d'octets (0 = inconnue)
                NKENTSEU_EVENT_API_INLINE uint64 GetTotalBytes() const noexcept {
                    return mTotalBytes;
                }

                /// @brief Retourne la direction du transfert
                /// @return NkTransferDirection indiquant SEND ou RECEIVE
                NKENTSEU_EVENT_API_INLINE NkTransferDirection GetDirection() const noexcept {
                    return mDirection;
                }

                /// @brief Retourne le protocole utilisé pour le transfert
                /// @return NkTransferProtocol identifiant le canal de communication
                NKENTSEU_EVENT_API_INLINE NkTransferProtocol GetProtocol() const noexcept {
                    return mProtocol;
                }

                /// @brief Indique si le transfert est un envoi (upload)
                /// @return true si GetDirection() == NK_TRANSFER_SEND
                NKENTSEU_EVENT_API_INLINE bool IsSend() const noexcept {
                    return mDirection == NkTransferDirection::NK_TRANSFER_SEND;
                }

                /// @brief Indique si le transfert est une réception (download)
                /// @return true si GetDirection() == NK_TRANSFER_RECEIVE
                NKENTSEU_EVENT_API_INLINE bool IsReceive() const noexcept {
                    return mDirection == NkTransferDirection::NK_TRANSFER_RECEIVE;
                }

                /// @brief Indique si la taille totale du transfert est connue
                /// @return true si GetTotalBytes() > 0
                /// @note Si false, GetProgressPercent() retournera -1.0
                NKENTSEU_EVENT_API_INLINE bool IsSizeKnown() const noexcept {
                    return mTotalBytes > 0;
                }

            private:
                NkString mName;              ///< Nom lisible du transfert (fichier, URI...)
                uint64 mTotalBytes;          ///< Taille totale attendue [octets] (0 = inconnue)
                NkTransferDirection mDirection;  ///< Sens du transfert (SEND/RECEIVE)
                NkTransferProtocol mProtocol;    ///< Protocole/canal utilisé
        };

        // =====================================================================
        // NkTransferProgressEvent — Mise à jour de progression
        // =====================================================================

        /**
         * @brief Émis périodiquement pendant un transfert pour signaler la progression
         *
         * Cet événement permet de mettre à jour les interfaces utilisateur et
         * de monitorer les performances du transfert en temps réel :
         *   - Barres de progression (% complété)
         *   - Estimations de temps restant (ETA)
         *   - Débit instantané pour diagnostic de performance réseau
         *
         * @note La fréquence d'émission de cet événement est contrôlée par
         *       le TransferManager pour équilibrer réactivité UI et overhead CPU.
         *
         * @note bytesTransferred est cumulatif depuis le début du transfert.
         *       Pour calculer le débit sur une fenêtre glissante, conserver
         *       les valeurs précédentes et diviser la différence par le delta temps.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferProgressEvent final : public NkTransferEvent {
            public:
                /// @brief Associe le type d'événement NK_TRANSFER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

                /**
                 * @brief Constructeur avec données de progression
                 * @param transferId Identifiant du transfert concerné
                 * @param bytesTransferred Nombre d'octets transférés depuis le début (cumulatif)
                 * @param totalBytes Taille totale attendue (0 si inconnue)
                 * @param speedBytesPerSec Débit instantané estimé [octets/seconde] (0 si inconnu)
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTransferProgressEvent(
                    uint64 transferId,
                    uint64 bytesTransferred,
                    uint64 totalBytes = 0,
                    uint64 speedBytesPerSec = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkTransferEvent(transferId, windowId)
                    , mBytesTransferred(bytesTransferred)
                    , mTotalBytes(totalBytes)
                    , mSpeedBytesPerSec(speedBytesPerSec) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTransferProgressEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant la progression en octets et pourcentage
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "TransferProgress(id={0} {1}/{2}B {3}%)",
                        mTransferId,
                        mBytesTransferred,
                        (mTotalBytes > 0 ? NkString::Fmt("{0}", mTotalBytes) : "?"),
                        static_cast<int>(GetProgressPercent())
                    );
                }

                /// @brief Retourne le nombre d'octets transférés depuis le début
                /// @return uint64 représentant le compteur cumulatif [octets]
                NKENTSEU_EVENT_API_INLINE uint64 GetBytesTransferred() const noexcept {
                    return mBytesTransferred;
                }

                /// @brief Retourne la taille totale attendue du transfert
                /// @return uint64 représentant le total [octets] (0 = inconnue)
                NKENTSEU_EVENT_API_INLINE uint64 GetTotalBytes() const noexcept {
                    return mTotalBytes;
                }

                /// @brief Retourne le débit instantané estimé
                /// @return uint64 représentant les octets par seconde (0 = inconnu)
                NKENTSEU_EVENT_API_INLINE uint64 GetSpeedBytesPerSec() const noexcept {
                    return mSpeedBytesPerSec;
                }

                /// @brief Calcule le pourcentage de progression [0.0, 100.0]
                /// @return float64 représentant le % complété, ou -1.0 si taille inconnue
                /// @note Formule : (bytesTransferred / totalBytes) * 100.0
                NKENTSEU_EVENT_API_INLINE float64 GetProgressPercent() const noexcept {
                    if (mTotalBytes == 0) {
                        return -1.0;
                    }
                    return 100.0
                         * static_cast<float64>(mBytesTransferred)
                         / static_cast<float64>(mTotalBytes);
                }

                /// @brief Convertit le débit en kilo-octets par seconde (arrondi)
                /// @return uint64 représentant les Ko/s (division par 1024)
                /// @note Utile pour affichage humain : "256 Ko/s" au lieu de "262144 octets/s"
                NKENTSEU_EVENT_API_INLINE uint64 GetSpeedKBps() const noexcept {
                    return mSpeedBytesPerSec / 1024ULL;
                }

            private:
                uint64 mBytesTransferred;  ///< Octets transférés depuis le début [cumulatif]
                uint64 mTotalBytes;        ///< Taille totale attendue [octets] (0 = inconnue)
                uint64 mSpeedBytesPerSec;  ///< Débit instantané estimé [octets/seconde]
        };

        // =====================================================================
        // NkTransferCompleteEvent — Transfert terminé avec succès
        // =====================================================================

        /**
         * @brief Émis lorsqu'un transfert se termine avec succès
         *
         * Cet événement marque la fin heureuse d'un transfert et fournit
         * les métriques finales pour logging et statistiques :
         *   - Nombre total d'octets effectivement transférés
         *   - Durée totale du transfert (pour calcul du débit moyen)
         *   - Chemin de destination (pour les transferts vers fichier local)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Notifier l'utilisateur de la complétion
         *       - Déclencher le traitement des données reçues
         *       - Mettre à jour l'historique des transferts
         *       - Libérer les ressources temporaires allouées au début
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferCompleteEvent final : public NkTransferEvent {
            public:
                /// @brief Associe le type d'événement NK_TRANSFER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

                /**
                 * @brief Constructeur avec métriques de complétion
                 * @param transferId Identifiant du transfert concerné
                 * @param totalBytes Nombre total d'octets effectivement transférés
                 * @param durationMs Durée totale du transfert en millisecondes
                 * @param outputPath Chemin de destination (si applicable, ex: fichier local)
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTransferCompleteEvent(
                    uint64 transferId,
                    uint64 totalBytes = 0,
                    uint64 durationMs = 0,
                    NkString outputPath = {},
                    uint64 windowId = 0
                )
                    : NkTransferEvent(transferId, windowId)
                    , mTotalBytes(totalBytes)
                    , mDurationMs(durationMs)
                    , mOutputPath(traits::NkMove(outputPath)) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTransferCompleteEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'ID, la taille et la durée du transfert
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "TransferComplete(id={0} {1}B {2}ms)",
                        mTransferId, mTotalBytes, mDurationMs
                    );
                }

                /// @brief Retourne le nombre total d'octets transférés
                /// @return uint64 représentant le volume final [octets]
                NKENTSEU_EVENT_API_INLINE uint64 GetTotalBytes() const noexcept {
                    return mTotalBytes;
                }

                /// @brief Retourne la durée totale du transfert
                /// @return uint64 représentant le temps écoulé [millisecondes]
                NKENTSEU_EVENT_API_INLINE uint64 GetDurationMs() const noexcept {
                    return mDurationMs;
                }

                /// @brief Retourne le chemin de destination du transfert
                /// @return const NkString& référence vers le chemin (lecture seule)
                /// @note Vide si le transfert n'a pas de destination fichier (ex: mémoire, socket)
                NKENTSEU_EVENT_API_INLINE const NkString& GetOutputPath() const noexcept {
                    return mOutputPath;
                }

                /// @brief Calcule le débit moyen du transfert [octets/seconde]
                /// @return uint64 représentant le débit moyen (0 si durationMs == 0)
                /// @note Formule : (totalBytes * 1000) / durationMs
                NKENTSEU_EVENT_API_INLINE uint64 GetAverageSpeedBytesPerSec() const noexcept {
                    if (mDurationMs == 0) {
                        return 0;
                    }
                    return (mTotalBytes * 1000ULL) / mDurationMs;
                }

            private:
                uint64 mTotalBytes;    ///< Nombre total d'octets effectivement transférés
                uint64 mDurationMs;    ///< Durée totale du transfert [millisecondes]
                NkString mOutputPath;  ///< Chemin de destination (si applicable)
        };

        // =====================================================================
        // NkTransferErrorEvent — Transfert interrompu par erreur
        // =====================================================================

        /**
         * @brief Émis lorsqu'un transfert échoue avant complétion
         *
         * Cet événement notifie l'échec d'un transfert et fournit
         * les informations de diagnostic pour gestion d'erreur :
         *   - Code de statut pour catégorisation programmatique
         *   - Message textuel pour affichage utilisateur ou logging
         *   - Nombre d'octets transférés avant l'erreur (pour reprise possible)
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Afficher un message d'erreur adapté au statut
         *       - Proposer une reprise (retry) si l'erreur est transitoire
         *       - Nettoyer les ressources partielles allouées
         *       - Logger l'erreur pour analyse post-mortem
         *
         * @note bytesTransferred peut être utilisé pour implémenter
         *       un mécanisme de reprise (resume) si le protocole le supporte.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferErrorEvent final : public NkTransferEvent {
            public:
                /// @brief Associe le type d'événement NK_TRANSFER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

                /**
                 * @brief Constructeur avec informations d'erreur
                 * @param transferId Identifiant du transfert concerné
                 * @param status Code de statut catégorisant le type d'erreur
                 * @param message Description textuelle de l'erreur (optionnelle)
                 * @param bytesTransferred Octets transférés avant l'échec (pour reprise)
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTransferErrorEvent(
                    uint64 transferId,
                    NkTransferStatus status,
                    NkString message = {},
                    uint64 bytesTransferred = 0,
                    uint64 windowId = 0
                )
                    : NkTransferEvent(transferId, windowId)
                    , mStatus(status)
                    , mMessage(traits::NkMove(message))
                    , mBytesTransferred(bytesTransferred) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTransferErrorEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'ID, le statut et le message d'erreur
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "TransferError(id={0} {1}{2})",
                        mTransferId,
                        NkTransferStatusToString(mStatus),
                        (mMessage.Empty() ? "" : " \"" + mMessage + "\"")
                    );
                }

                /// @brief Retourne le code de statut de l'erreur
                /// @return NkTransferStatus catégorisant le type d'échec
                NKENTSEU_EVENT_API_INLINE NkTransferStatus GetStatus() const noexcept {
                    return mStatus;
                }

                /// @brief Retourne le message descriptif de l'erreur
                /// @return const NkString& référence vers le message (lecture seule)
                /// @note Peut être vide si aucun détail supplémentaire n'est disponible
                NKENTSEU_EVENT_API_INLINE const NkString& GetMessage() const noexcept {
                    return mMessage;
                }

                /// @brief Retourne le nombre d'octets transférés avant l'échec
                /// @return uint64 représentant le progrès partiel [octets]
                /// @note Utile pour implémenter un mécanisme de reprise (resume)
                NKENTSEU_EVENT_API_INLINE uint64 GetBytesTransferred() const noexcept {
                    return mBytesTransferred;
                }

            private:
                NkTransferStatus mStatus;        ///< Code de statut catégorisant l'erreur
                NkString mMessage;               ///< Message descriptif optionnel
                uint64 mBytesTransferred;        ///< Octets transférés avant l'échec
        };

        // =====================================================================
        // NkTransferCancelledEvent — Transfert annulé volontairement
        // =====================================================================

        /**
         * @brief Émis lorsqu'un transfert est annulé par l'utilisateur ou l'application
         *
         * Cet événement distingue l'annulation volontaire d'un échec technique :
         *   - L'utilisateur a cliqué sur "Annuler" dans l'UI
         *   - L'application a décidé d'arrêter le transfert (changement de contexte)
         *   - Un timeout utilisateur a expiré (pas un timeout réseau)
         *
         * @note Contrairement à NkTransferErrorEvent, l'annulation n'est pas
         *       une condition d'erreur : c'est un flux de contrôle attendu.
         *
         * @note Les handlers peuvent utiliser cet événement pour :
         *       - Nettoyer l'UI de progression sans afficher d'erreur
         *       - Libérer les ressources temporaires allouées
         *       - Optionnellement sauvegarder l'état partiel pour reprise future
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferCancelledEvent final : public NkTransferEvent {
            public:
                /// @brief Associe le type d'événement NK_TRANSFER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

                /**
                 * @brief Constructeur avec progrès au moment de l'annulation
                 * @param transferId Identifiant du transfert concerné
                 * @param bytesTransferred Octets transférés avant annulation
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkTransferCancelledEvent(
                    uint64 transferId,
                    uint64 bytesTransferred = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkTransferEvent(transferId, windowId)
                    , mBytesTransferred(bytesTransferred) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTransferCancelledEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'ID et le progrès au moment de l'annulation
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "TransferCancelled(id={0} after {1}B)",
                        mTransferId, mBytesTransferred
                    );
                }

                /// @brief Retourne le nombre d'octets transférés avant annulation
                /// @return uint64 représentant le progrès partiel [octets]
                NKENTSEU_EVENT_API_INLINE uint64 GetBytesTransferred() const noexcept {
                    return mBytesTransferred;
                }

            private:
                uint64 mBytesTransferred;  ///< Octets transférés avant annulation
        };

        // =====================================================================
        // NkTransferDataEvent — Paquet de données brut pour streaming
        // =====================================================================

        /**
         * @brief Émis lorsqu'un paquet de données est disponible pendant un transfert en streaming
         *
         * Cet événement est conçu pour les transferts où les données doivent être
         * traitées au fur et à mesure de leur réception, sans attendre la complétion :
         *   - Streaming audio/vidéo : décodage et lecture en temps réel
         *   - Réseau temps-réel : traitement immédiat des paquets reçus
         *   - Logs distants : affichage incrémental des messages
         *
         * @note Les données sont copiées dans un NkVector<uint8> interne à l'événement.
         *       Cela garantit que les données restent valides pendant le traitement
         *       du handler, même si la source originale est libérée.
         *
         * @warning Pour les très gros volumes de données, cette copie peut avoir
         *          un coût mémoire et performance. Dans ce cas, envisager :
         *          - Un système de buffers partagés en dehors du système d'événements
         *          - Un traitement par chunks plus petits pour limiter l'allocation
         *          - Un mécanisme de zero-copy si le framework le supporte
         *
         * @note L'offset permet de reconstituer le flux complet en concaténant
         *       les paquets dans l'ordre de réception.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkTransferDataEvent final : public NkTransferEvent {
            public:
                /// @brief Associe le type d'événement NK_TRANSFER à cette classe
                NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

                /**
                 * @brief Constructeur avec paquet de données et métadonnées
                 * @param transferId Identifiant du transfert concerné
                 * @param data Vecteur de données du paquet (copiées dans l'événement)
                 * @param offset Offset depuis le début du flux [octets] pour reconstitution
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 *
                 * @note Le vecteur data est déplacé (move) si possible pour performance.
                 *       L'événement prend ownership des données : pas de risque de dangling.
                 */
                NKENTSEU_EVENT_API NkTransferDataEvent(
                    uint64 transferId,
                    NkVector<uint8> data,
                    uint64 offset = 0,
                    uint64 windowId = 0
                )
                    : NkTransferEvent(transferId, windowId)
                    , mData(traits::NkMove(data))
                    , mOffset(offset) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                /// @note Copie profonde du vecteur mData via copy-ctor de NkVector
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkTransferDataEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant l'ID, la taille du paquet et l'offset
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "TransferData(id={0} size={1}B offset={2})",
                        mTransferId, mData.Size(), mOffset
                    );
                }

                /// @brief Retourne une référence constante vers les données du paquet
                /// @return const NkVector<uint8>& pour lecture seule des données
                NKENTSEU_EVENT_API_INLINE const NkVector<uint8>& GetData() const noexcept {
                    return mData;
                }

                /// @brief Retourne l'offset du paquet depuis le début du flux
                /// @return uint64 représentant la position [octets] dans le flux complet
                /// @note Utile pour reconstituer le flux en concaténant les paquets dans l'ordre
                NKENTSEU_EVENT_API_INLINE uint64 GetOffset() const noexcept {
                    return mOffset;
                }

                /// @brief Retourne la taille du paquet en octets
                /// @return uint32 représentant le nombre d'octets dans ce paquet
                NKENTSEU_EVENT_API_INLINE uint32 GetSize() const noexcept {
                    return static_cast<uint32>(mData.Size());
                }

                /// @brief Retourne un pointeur en lecture seule vers le buffer interne
                /// @return const uint8* pointant vers les données brutes (ou nullptr si vide)
                NKENTSEU_EVENT_API_INLINE const uint8* GetBytes() const noexcept {
                    return mData.Data();
                }

            private:
                NkVector<uint8> mData;  ///< Vecteur propriétaire des données du paquet
                uint64 mOffset;         ///< Offset depuis le début du flux [octets]
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKTRANSFEREVENT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKTRANSFEREVENT.H
// =============================================================================
// Ce fichier définit la hiérarchie des événements de transfert pour le module NKEvent.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Suivi de progression avec barre UI et estimation de temps
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTransferEvent.h"

    class TransferUI {
    public:
        void OnTransferBegin(const nkentseu::NkTransferBeginEvent& event) {
            using namespace nkentseu;

            // Initialisation de l'UI de progression
            if (event.IsSizeKnown()) {
                mProgressBar->SetMode(ProgressBar::Mode::Deterministic);
                mProgressBar->SetTotal(event.GetTotalBytes());
            }
            else {
                mProgressBar->SetMode(ProgressBar::Mode::Indeterminate);
            }

            mStatusLabel->SetText(
                NkString::Fmt("Transferring: {}", event.GetName())
            );

            mStartTime = NkChrono::Now().milliseconds;
        }

        void OnTransferProgress(const nkentseu::NkTransferProgressEvent& event) {
            using namespace nkentseu;

            // Mise à jour de la barre de progression
            float percent = event.GetProgressPercent();
            if (percent >= 0.0) {
                mProgressBar->SetValue(event.GetBytesTransferred());
            }

            // Calcul et affichage de l'ETA (Estimated Time of Arrival)
            if (event.GetSpeedBytesPerSec() > 0 && event.IsSizeKnown()) {
                uint64 remaining = event.GetTotalBytes() - event.GetBytesTransferred();
                uint64 etaSeconds = remaining / event.GetSpeedBytesPerSec();
                mTimeLabel->SetText(
                    NkString::Fmt("{0} KB/s - ETA: {1}s",
                        event.GetSpeedKBps(), etaSeconds)
                );
            }
        }

        void OnTransferComplete(const nkentseu::NkTransferCompleteEvent& event) {
            using namespace nkentseu;

            // Affichage du succès avec statistiques
            float durationSec = static_cast<float>(event.GetDurationMs()) / 1000.0f;
            float avgSpeedKBps = static_cast<float>(event.GetAverageSpeedBytesPerSec()) / 1024.0f;

            mStatusLabel->SetText(
                NkString::Fmt("✓ Complete: {} in {:.1f}s ({:.1f} KB/s)",
                    event.GetOutputPath(), durationSec, avgSpeedKBps)
            );

            // Nettoyage de l'UI après délai
            Timer::Schedule([this]() { HideTransferUI(); }, 3000);
        }

    private:
        ProgressBar* mProgressBar = nullptr;
        Label* mStatusLabel = nullptr;
        Label* mTimeLabel = nullptr;
        uint64 mStartTime = 0;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion d'erreur avec reprise automatique (retry)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTransferEvent.h"

    class ResilientTransferHandler {
    public:
        void OnTransferError(nkentseu::NkTransferErrorEvent& event) {
            using namespace nkentseu;

            // Stratégie de reprise selon le type d'erreur
            switch (event.GetStatus()) {
                case NkTransferStatus::NK_TRANSFER_STATUS_TIMEOUT:
                case NkTransferStatus::NK_TRANSFER_STATUS_ERROR:
                    // Erreurs transitoires : tentative de reprise
                    if (mRetryCount < MAX_RETRIES) {
                        mRetryCount++;
                        NK_FOUNDATION_LOG_INFO("Transfer error, retry {}/{}",
                            mRetryCount, MAX_RETRIES);
                        RetryTransfer(event.GetTransferId(), event.GetBytesTransferred());
                    }
                    else {
                        ShowError("Transfer failed after {} retries", MAX_RETRIES);
                    }
                    break;

                case NkTransferStatus::NK_TRANSFER_STATUS_DENIED:
                    // Erreur d'authentification : demander à l'utilisateur
                    ShowAuthDialog([this, &event]() {
                        RetryTransfer(event.GetTransferId(), 0);  // Reprendre depuis le début
                    });
                    break;

                case NkTransferStatus::NK_TRANSFER_STATUS_CORRUPTED:
                    // Données corrompues : impossible de reprendre, échec définitif
                    ShowError("Data corruption detected. Transfer aborted.");
                    CleanupPartialData(event.GetTransferId());
                    break;

                default:
                    // Autres erreurs : logging et échec
                    NK_FOUNDATION_LOG_ERROR("Transfer error {}: {}",
                        NkTransferStatusToString(event.GetStatus()),
                        event.GetMessage());
                    break;
            }
        }

    private:
        static constexpr int MAX_RETRIES = 3;
        int mRetryCount = 0;

        void RetryTransfer(uint64 transferId, uint64 resumeFrom);
        void ShowAuthDialog(std::function<void()> onAuthSuccess);
        void ShowError(const char* fmt, ...);
        void CleanupPartialData(uint64 transferId);
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Streaming audio avec traitement temps-réel des paquets
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTransferEvent.h"
    #include "NKAudio/NkAudioStream.h"

    class AudioStreamPlayer {
    public:
        void OnTransferData(const nkentseu::NkTransferDataEvent& event) {
            using namespace nkentseu;

            // Vérification que les données sont pour ce flux audio
            if (event.GetTransferId() != mAudioTransferId) {
                return;
            }

            // Enqueue des données brutes dans le decoder audio
            const uint8* bytes = event.GetBytes();
            uint32 size = event.GetSize();

            if (bytes && size > 0) {
                // Décodeur traite les données de façon asynchrone
                mAudioDecoder->EnqueueSamples(bytes, size);

                // Logging occasionnel pour monitoring
                if (mPacketCount % 100 == 0) {
                    NK_FOUNDATION_LOG_DEBUG("Audio packet #{}: {}B @ offset {}",
                        mPacketCount, size, event.GetOffset());
                }
                mPacketCount++;
            }
        }

        void StartStreaming(const NkString& streamUrl) {
            using namespace nkentseu;

            // Démarrage du transfert avec callback pour les données
            mAudioTransferId = TransferManager::StartDownload(
                streamUrl,
                TransferFlags::Streaming  // Flag indiquant le mode streaming
            );

            // Enregistrement du handler pour les paquets de données
            EventManager::GetInstance().Subscribe<NkTransferDataEvent>(
                NK_EVENT_BIND_HANDLER(OnTransferData)
            );
        }

    private:
        uint64 mAudioTransferId = 0;
        uint64 mPacketCount = 0;
        AudioDecoder* mAudioDecoder = nullptr;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestionnaire de transferts multiples avec suivi par ID
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTransferEvent.h"
    #include <unordered_map>

    class TransferManagerUI {
    public:
        // Démarrage d'un nouveau transfert avec suivi UI
        uint64 StartTrackedTransfer(const TransferConfig& config) {
            using namespace nkentseu;

            // Démarrage effectif du transfert
            uint64 transferId = TransferManager::Start(config);

            // Création d'un widget de suivi pour ce transfert
            auto& tracker = mTrackers[transferId];
            tracker.progressBar = new ProgressBar();
            tracker.statusLabel = new Label();
            tracker.cancelButton = new Button("Cancel", [this, transferId]() {
                TransferManager::Cancel(transferId);
            });

            // Ajout à l'UI principale
            mTransferList->AddItem(tracker.CreateWidget());

            return transferId;
        }

        // Mise à jour centralisée des trackers
        void OnTransferProgress(const nkentseu::NkTransferProgressEvent& event) {
            using namespace nkentseu;

            auto it = mTrackers.find(event.GetTransferId());
            if (it != mTrackers.end()) {
                auto& tracker = it->second;
                float percent = event.GetProgressPercent();

                if (percent >= 0.0) {
                    tracker.progressBar->SetValue(percent);
                    tracker.statusLabel->SetText(
                        NkString::Fmt("{:.1f}%", percent)
                    );
                }
            }
        }

        // Nettoyage lors de la complétion ou échec
        void OnTransferTerminal(const nkentseu::NkTransferEvent& event) {
            using namespace nkentseu;

            auto it = mTrackers.find(event.GetTransferId());
            if (it != mTrackers.end()) {
                // Animation de fin puis suppression du widget
                auto& tracker = it->second;
                tracker.CreateWidget()->FadeOut(200, [this, transferId = event.GetTransferId()]() {
                    mTransferList->RemoveItem(transferId);
                    mTrackers.erase(transferId);
                });
            }
        }

    private:
        struct TransferTracker {
            ProgressBar* progressBar = nullptr;
            Label* statusLabel = nullptr;
            Button* cancelButton = nullptr;

            Widget* CreateWidget() { /\* ... *\/ }
        };

        std::unordered_map<uint64, TransferTracker> mTrackers;
        TransferList* mTransferList = nullptr;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Logging et statistiques des transferts pour analyse post-mortem
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkTransferEvent.h"
    #include "NKSerialization/NkJson.h"  // Supposé exister

    class TransferAnalytics {
    public:
        void OnTransferBegin(const nkentseu::NkTransferBeginEvent& event) {
            using namespace nkentseu;

            // Enregistrement du début pour calcul de durée
            mStartTimes[event.GetTransferId()] = NkChrono::Now().milliseconds;

            // Logging structuré pour ingestion dans un système d'analytics
            NkJson logEntry;
            logEntry["event"] = "transfer_begin";
            logEntry["transfer_id"] = event.GetTransferId();
            logEntry["protocol"] = NkTransferProtocolToString(event.GetProtocol());
            logEntry["direction"] = event.IsSend() ? "send" : "receive";
            logEntry["size_bytes"] = event.GetTotalBytes();
            logEntry["timestamp"] = event.GetTimestamp();

            Analytics::Log(logEntry);
        }

        void OnTransferComplete(const nkentseu::NkTransferCompleteEvent& event) {
            using namespace nkentseu;

            // Calcul de la durée réelle
            auto it = mStartTimes.find(event.GetTransferId());
            uint64 durationMs = (it != mStartTimes.end())
                ? event.GetTimestamp() - it->second
                : event.GetDurationMs();

            // Logging des métriques de performance
            NkJson logEntry;
            logEntry["event"] = "transfer_complete";
            logEntry["transfer_id"] = event.GetTransferId();
            logEntry["duration_ms"] = durationMs;
            logEntry["bytes_transferred"] = event.GetTotalBytes();
            logEntry["avg_speed_kbps"] = event.GetAverageSpeedBytesPerSec() / 1024;
            logEntry["success"] = true;

            Analytics::Log(logEntry);

            // Nettoyage du tracking
            mStartTimes.erase(event.GetTransferId());
        }

        void OnTransferError(const nkentseu::NkTransferErrorEvent& event) {
            using namespace nkentseu;

            // Logging d'erreur avec contexte pour debugging
            NkJson logEntry;
            logEntry["event"] = "transfer_error";
            logEntry["transfer_id"] = event.GetTransferId();
            logEntry["status"] = NkTransferStatusToString(event.GetStatus());
            logEntry["message"] = event.GetMessage();
            logEntry["bytes_before_error"] = event.GetBytesTransferred();
            logEntry["success"] = false;

            Analytics::Log(logEntry);

            // Alerting si erreur critique
            if (event.GetStatus() == NkTransferStatus::NK_TRANSFER_STATUS_CORRUPTED) {
                Monitoring::Alert("Data corruption in transfer {}", event.GetTransferId());
            }
        }

    private:
        std::unordered_map<uint64, uint64> mStartTimes;  // transferId -> start timestamp
    };
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. GESTION DES TRANSFERTS CONCURRENTS :
       - Chaque transfert a un transferId unique (uint64) pour corrélation
       - Les handlers doivent toujours vérifier GetTransferId() avant traitement
       - Utiliser un std::unordered_map<uint64, TransferState> pour le suivi

    2. PERFORMANCE ET FRÉQUENCE DES ÉVÉNEMENTS PROGRESS :
       - NkTransferProgressEvent peut être émis très fréquemment (ex: toutes les 100ms)
       - Éviter les allocations ou calculs lourds dans les handlers de progression
       - Pour l'UI, throttler les mises à jour visuelles (ex: max 30 FPS) même si
         les événements arrivent plus vite

    3. STREAMING VS BUFFERING :
       - NkTransferDataEvent est pour le traitement temps-réel (pas d'attente de fin)
       - Pour les gros fichiers, préférer NkTransferProgressEvent + lecture post-complétion
       - Documenter clairement quel mode est utilisé pour chaque type de transfert

    4. REPRISE APRÈS ERREUR (RESUME) :
       - NkTransferErrorEvent::GetBytesTransferred() permet de connaître le progrès
       - Si le protocole supporte les Range headers (HTTP) ou seek (fichier),
         reprendre depuis cet offset au lieu de recommencer depuis 0
       - Toujours vérifier que les données partielles sont valides avant reprise

    5. SÉCURITÉ DES DONNÉES :
       - NkTransferDataEvent copie les données : pas de risque de dangling pointer
       - Pour les données sensibles, envisager un chiffrement au niveau application
       - Ne pas logger le contenu des paquets dans ToString() ou les logs de debug

    6. EXTENSIBILITÉ :
       - Pour ajouter un nouvel événement de transfert :
         a) Dériver de NkTransferEvent (hérite automatiquement de NK_CAT_TRANSFER)
         b) Utiliser NK_EVENT_TYPE_FLAGS avec un nouveau type dans NkEventType
         c) Implémenter Clone() et ToString() pour cohérence avec l'infrastructure
         d) Documenter le cas d'usage et l'ordre d'émission attendu dans le cycle de vie
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================