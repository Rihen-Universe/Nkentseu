#pragma once

// =============================================================================
// NkTransferEvent.h
// Hiérarchie des événements de transfert de données et de fichiers.
//
// Catégorie : NK_CAT_TRANSFER
//
// Enumerations :
//   NkTransferDirection  — sens du transfert (envoi / réception)
//   NkTransferProtocol   — protocole ou canal de transfert
//   NkTransferStatus     — résultat final d'un transfert
//
// Classes :
//   NkTransferEvent               — base abstraite (catégorie TRANSFER)
//     NkTransferBeginEvent        — début d'un transfert identifié
//     NkTransferProgressEvent     — progression (bytes / pourcentage)
//     NkTransferCompleteEvent     — transfert terminé avec succès
//     NkTransferErrorEvent        — transfert interrompu par erreur
//     NkTransferCancelledEvent    — transfert annulé par l'utilisateur
//     NkTransferDataEvent         — paquet de données brut reçu en ligne
//
// Usage type :
//   Un transfert est identifié par un NkU64 transferId unique.
//   La séquence d'événements normale est :
//     NkTransferBeginEvent -> N * NkTransferProgressEvent
//     -> NkTransferCompleteEvent  (succès)
//      | NkTransferErrorEvent     (erreur)
//      | NkTransferCancelledEvent (annulation)
// =============================================================================

#include "NkEvent.h"
#include <string>
#include <vector>

namespace nkentseu {

    // =========================================================================
    // NkTransferDirection — sens du transfert
    // =========================================================================

    enum class NkTransferDirection : NkU32 {
        NK_TRANSFER_SEND    = 0, ///< Envoi (upload, write)
        NK_TRANSFER_RECEIVE = 1  ///< Réception (download, read)
    };

    // =========================================================================
    // NkTransferProtocol — canal de transfert
    // =========================================================================

    enum class NkTransferProtocol : NkU32 {
        NK_TRANSFER_PROTO_UNKNOWN  = 0,
        NK_TRANSFER_PROTO_FILE,        ///< Fichier local (copie, déplacement)
        NK_TRANSFER_PROTO_HTTP,        ///< HTTP / HTTPS
        NK_TRANSFER_PROTO_FTP,         ///< FTP
        NK_TRANSFER_PROTO_BLUETOOTH,   ///< Bluetooth
        NK_TRANSFER_PROTO_USB,         ///< USB (mass storage, MTP...)
        NK_TRANSFER_PROTO_IPC,         ///< IPC (pipe, socket locale)
        NK_TRANSFER_PROTO_CUSTOM,      ///< Canal défini par l'application
        NK_TRANSFER_PROTO_MAX
    };

    inline const char* NkTransferProtocolToString(NkTransferProtocol p) noexcept {
        switch (p) {
            case NkTransferProtocol::NK_TRANSFER_PROTO_FILE:      return "FILE";
            case NkTransferProtocol::NK_TRANSFER_PROTO_HTTP:      return "HTTP";
            case NkTransferProtocol::NK_TRANSFER_PROTO_FTP:       return "FTP";
            case NkTransferProtocol::NK_TRANSFER_PROTO_BLUETOOTH: return "BT";
            case NkTransferProtocol::NK_TRANSFER_PROTO_USB:       return "USB";
            case NkTransferProtocol::NK_TRANSFER_PROTO_IPC:       return "IPC";
            case NkTransferProtocol::NK_TRANSFER_PROTO_CUSTOM:    return "CUSTOM";
            default:                                                return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkTransferStatus — résultat final
    // =========================================================================

    enum class NkTransferStatus : NkU32 {
        NK_TRANSFER_STATUS_SUCCESS   = 0,
        NK_TRANSFER_STATUS_ERROR,        ///< Erreur réseau / I/O
        NK_TRANSFER_STATUS_TIMEOUT,      ///< Délai dépassé
        NK_TRANSFER_STATUS_DENIED,       ///< Accès refusé
        NK_TRANSFER_STATUS_CORRUPTED,    ///< Données corrompues (checksum)
        NK_TRANSFER_STATUS_CANCELLED,    ///< Annulation volontaire
        NK_TRANSFER_STATUS_MAX
    };

    inline const char* NkTransferStatusToString(NkTransferStatus s) noexcept {
        switch (s) {
            case NkTransferStatus::NK_TRANSFER_STATUS_SUCCESS:   return "SUCCESS";
            case NkTransferStatus::NK_TRANSFER_STATUS_ERROR:     return "ERROR";
            case NkTransferStatus::NK_TRANSFER_STATUS_TIMEOUT:   return "TIMEOUT";
            case NkTransferStatus::NK_TRANSFER_STATUS_DENIED:    return "DENIED";
            case NkTransferStatus::NK_TRANSFER_STATUS_CORRUPTED: return "CORRUPTED";
            case NkTransferStatus::NK_TRANSFER_STATUS_CANCELLED: return "CANCELLED";
            default:                                               return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkTransferEvent — base abstraite pour tous les événements de transfert
    // =========================================================================

    /**
     * @brief Classe de base pour les événements de transfert de données.
     *
     * Catégorie : NK_CAT_TRANSFER
     *
     * Chaque transfert est identifié par un mTransferId unique permettant de
     * corréler tous les événements d'un même transfert.
     */
    class NkTransferEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_TRANSFER)

        /// @brief Identifiant unique du transfert
        NkU64 GetTransferId() const noexcept { return mTransferId; }

    protected:
        NkTransferEvent(NkU64 transferId, NkU64 windowId = 0) noexcept
            : NkEvent(windowId)
            , mTransferId(transferId)
        {}

        NkU64 mTransferId = 0;
    };

    // =========================================================================
    // NkTransferBeginEvent — début d'un transfert
    // =========================================================================

    /**
     * @brief Émis au démarrage d'un transfert de données.
     *
     * totalBytes == 0 signifie que la taille totale est inconnue (streaming,
     * connexion réseau en chunked transfer...).
     */
    class NkTransferBeginEvent final : public NkTransferEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

        /**
         * @param transferId  Identifiant unique du transfert.
         * @param name        Nom lisible (nom de fichier, URI...).
         * @param totalBytes  Taille totale [octets] (0 = inconnue).
         * @param direction   Sens du transfert.
         * @param protocol    Canal utilisé.
         * @param windowId    Identifiant de la fenêtre.
         */
        NkTransferBeginEvent(NkU64 transferId,
                              std::string           name,
                              NkU64                 totalBytes = 0,
                              NkTransferDirection   direction  = NkTransferDirection::NK_TRANSFER_RECEIVE,
                              NkTransferProtocol    protocol   = NkTransferProtocol::NK_TRANSFER_PROTO_UNKNOWN,
                              NkU64                 windowId   = 0)
            : NkTransferEvent(transferId, windowId)
            , mName(std::move(name))
            , mTotalBytes(totalBytes)
            , mDirection(direction)
            , mProtocol(protocol)
        {}

        NkEvent*    Clone()    const override { return new NkTransferBeginEvent(*this); }
        std::string ToString() const override {
            return "TransferBegin(id=" + std::to_string(mTransferId)
                 + " \"" + mName + "\""
                 + " size=" + (mTotalBytes > 0 ? std::to_string(mTotalBytes) : "?") + "B"
                 + " " + NkTransferProtocolToString(mProtocol) + ")";
        }

        const std::string&    GetNames()      const noexcept { return mName; }
        NkU64                 GetTotalBytes()const noexcept { return mTotalBytes; }
        NkTransferDirection   GetDirection() const noexcept { return mDirection; }
        NkTransferProtocol    GetProtocol()  const noexcept { return mProtocol; }
        bool                  IsSend()       const noexcept { return mDirection == NkTransferDirection::NK_TRANSFER_SEND; }
        bool                  IsReceive()    const noexcept { return mDirection == NkTransferDirection::NK_TRANSFER_RECEIVE; }
        bool                  IsSizeKnown() const noexcept { return mTotalBytes > 0; }

    private:
        std::string         mName;
        NkU64               mTotalBytes = 0;
        NkTransferDirection mDirection  = NkTransferDirection::NK_TRANSFER_RECEIVE;
        NkTransferProtocol  mProtocol   = NkTransferProtocol::NK_TRANSFER_PROTO_UNKNOWN;
    };

    // =========================================================================
    // NkTransferProgressEvent — progression d'un transfert
    // =========================================================================

    /**
     * @brief Émis périodiquement pendant un transfert pour signaler la progression.
     *
     * bytesTransferred : octets transférés depuis le début.
     * totalBytes       : taille totale (0 si inconnue).
     * speedBytesPerSec : débit instantané [octets/s] (0 si inconnu).
     */
    class NkTransferProgressEvent final : public NkTransferEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

        NkTransferProgressEvent(NkU64 transferId,
                                  NkU64 bytesTransferred,
                                  NkU64 totalBytes        = 0,
                                  NkU64 speedBytesPerSec  = 0,
                                  NkU64 windowId          = 0) noexcept
            : NkTransferEvent(transferId, windowId)
            , mBytesTransferred(bytesTransferred)
            , mTotalBytes(totalBytes)
            , mSpeedBytesPerSec(speedBytesPerSec)
        {}

        NkEvent*    Clone()    const override { return new NkTransferProgressEvent(*this); }
        std::string ToString() const override {
            return "TransferProgress(id=" + std::to_string(mTransferId)
                 + " " + std::to_string(mBytesTransferred) + "/"
                 + (mTotalBytes > 0 ? std::to_string(mTotalBytes) : "?") + "B"
                 + " " + std::to_string(static_cast<int>(GetProgressPercent())) + "%)";
        }

        NkU64 GetBytesTransferred()  const noexcept { return mBytesTransferred; }
        NkU64 GetTotalBytes()        const noexcept { return mTotalBytes; }
        NkU64 GetSpeedBytesPerSec()  const noexcept { return mSpeedBytesPerSec; }

        /// @brief Pourcentage de progression [0.0, 100.0] ou -1 si taille inconnue
        NkF64 GetProgressPercent() const noexcept {
            if (mTotalBytes == 0) return -1.0;
            return 100.0 * static_cast<NkF64>(mBytesTransferred) / static_cast<NkF64>(mTotalBytes);
        }

        /// @brief Vitesse de transfert en Ko/s (arrondi)
        NkU64 GetSpeedKBps() const noexcept { return mSpeedBytesPerSec / 1024ULL; }

    private:
        NkU64 mBytesTransferred = 0;
        NkU64 mTotalBytes       = 0;
        NkU64 mSpeedBytesPerSec = 0;
    };

    // =========================================================================
    // NkTransferCompleteEvent — transfert terminé avec succès
    // =========================================================================

    /**
     * @brief Émis lorsqu'un transfert se termine avec succès.
     */
    class NkTransferCompleteEvent final : public NkTransferEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

        /**
         * @param transferId   Identifiant du transfert.
         * @param totalBytes   Nombre total d'octets transférés.
         * @param durationMs   Durée totale du transfert [ms].
         * @param outputPath   Chemin de destination (si applicable).
         * @param windowId     Identifiant de la fenêtre.
         */
        NkTransferCompleteEvent(NkU64 transferId,
                                  NkU64 totalBytes  = 0,
                                  NkU64 durationMs  = 0,
                                  std::string outputPath = {},
                                  NkU64 windowId    = 0)
            : NkTransferEvent(transferId, windowId)
            , mTotalBytes(totalBytes)
            , mDurationMs(durationMs)
            , mOutputPath(std::move(outputPath))
        {}

        NkEvent*    Clone()    const override { return new NkTransferCompleteEvent(*this); }
        std::string ToString() const override {
            return "TransferComplete(id=" + std::to_string(mTransferId)
                 + " " + std::to_string(mTotalBytes) + "B"
                 + " " + std::to_string(mDurationMs) + "ms)";
        }

        NkU64              GetTotalBytes()  const noexcept { return mTotalBytes; }
        NkU64              GetDurationMs()  const noexcept { return mDurationMs; }
        const std::string& GetOutputPath()  const noexcept { return mOutputPath; }

        /// @brief Débit moyen [octets/s] (0 si durée nulle)
        NkU64 GetAverageSpeedBytesPerSec() const noexcept {
            if (mDurationMs == 0) return 0;
            return (mTotalBytes * 1000ULL) / mDurationMs;
        }

    private:
        NkU64       mTotalBytes = 0;
        NkU64       mDurationMs = 0;
        std::string mOutputPath;
    };

    // =========================================================================
    // NkTransferErrorEvent — transfert interrompu par erreur
    // =========================================================================

    /**
     * @brief Émis lorsqu'un transfert échoue.
     *
     * bytesTransferred indique combien d'octets ont été transférés avant l'erreur.
     */
    class NkTransferErrorEvent final : public NkTransferEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

        /**
         * @param transferId       Identifiant du transfert.
         * @param status           Code d'erreur.
         * @param message          Description textuelle de l'erreur.
         * @param bytesTransferred Octets transférés avant l'erreur.
         * @param windowId         Identifiant de la fenêtre.
         */
        NkTransferErrorEvent(NkU64 transferId,
                               NkTransferStatus status,
                               std::string      message          = {},
                               NkU64            bytesTransferred = 0,
                               NkU64            windowId         = 0)
            : NkTransferEvent(transferId, windowId)
            , mStatus(status)
            , mMessage(std::move(message))
            , mBytesTransferred(bytesTransferred)
        {}

        NkEvent*    Clone()    const override { return new NkTransferErrorEvent(*this); }
        std::string ToString() const override {
            return "TransferError(id=" + std::to_string(mTransferId)
                 + " " + NkTransferStatusToString(mStatus)
                 + (mMessage.empty() ? "" : " \"" + mMessage + "\"") + ")";
        }

        NkTransferStatus   GetStatus()           const noexcept { return mStatus; }
        const std::string& GetMessage()          const noexcept { return mMessage; }
        NkU64              GetBytesTransferred() const noexcept { return mBytesTransferred; }

    private:
        NkTransferStatus mStatus           = NkTransferStatus::NK_TRANSFER_STATUS_ERROR;
        std::string      mMessage;
        NkU64            mBytesTransferred = 0;
    };

    // =========================================================================
    // NkTransferCancelledEvent — transfert annulé
    // =========================================================================

    /**
     * @brief Émis lorsqu'un transfert est annulé volontairement.
     */
    class NkTransferCancelledEvent final : public NkTransferEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

        NkTransferCancelledEvent(NkU64 transferId,
                                   NkU64 bytesTransferred = 0,
                                   NkU64 windowId         = 0) noexcept
            : NkTransferEvent(transferId, windowId)
            , mBytesTransferred(bytesTransferred)
        {}

        NkEvent*    Clone()    const override { return new NkTransferCancelledEvent(*this); }
        std::string ToString() const override {
            return "TransferCancelled(id=" + std::to_string(mTransferId)
                 + " after " + std::to_string(mBytesTransferred) + "B)";
        }

        NkU64 GetBytesTransferred() const noexcept { return mBytesTransferred; }

    private:
        NkU64 mBytesTransferred = 0;
    };

    // =========================================================================
    // NkTransferDataEvent — paquet de données brut (streaming en ligne)
    // =========================================================================

    /**
     * @brief Émis lorsqu'un paquet de données est disponible pendant un transfert.
     *
     * Utilisé pour les transferts en streaming où les données doivent être
     * traitées au fur et à mesure (audio, vidéo, réseau temps-réel...).
     *
     * @note  Les données sont passées par valeur (std::vector<NkU8>) pour
     *        éviter les problèmes de durée de vie. Pour les gros volumes, il
     *        est recommandé d'utiliser un système de buffers partagés en dehors
     *        du système d'événements.
     */
    class NkTransferDataEvent final : public NkTransferEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TRANSFER)

        /**
         * @param transferId  Identifiant du transfert.
         * @param data        Données du paquet (copiées dans l'événement).
         * @param offset      Offset depuis le début du flux [octets].
         * @param windowId    Identifiant de la fenêtre.
         */
        NkTransferDataEvent(NkU64             transferId,
                              std::vector<NkU8> data,
                              NkU64             offset    = 0,
                              NkU64             windowId  = 0)
            : NkTransferEvent(transferId, windowId)
            , mData(std::move(data))
            , mOffset(offset)
        {}

        NkEvent*    Clone()    const override { return new NkTransferDataEvent(*this); }
        std::string ToString() const override {
            return "TransferData(id=" + std::to_string(mTransferId)
                 + " size=" + std::to_string(mData.size()) + "B"
                 + " offset=" + std::to_string(mOffset) + ")";
        }

        const std::vector<NkU8>& GetData()   const noexcept { return mData; }
        NkU64                    GetOffset()  const noexcept { return mOffset; }
        NkU32                    GetSize()    const noexcept { return static_cast<NkU32>(mData.size()); }
        const NkU8*              GetBytes()   const noexcept { return mData.data(); }

    private:
        std::vector<NkU8> mData;
        NkU64             mOffset = 0;
    };

} // namespace nkentseu