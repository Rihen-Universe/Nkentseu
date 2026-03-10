#pragma once

// =============================================================================
// NkCustomEvents.h
// Événements personnalisés définis par l'application.
//
// Catégorie : NK_CAT_CUSTOM
//
// Classes :
//   NkCustomEvent         — événement générique à payload fixe (128 octets max)
//   NkCustomPtrEvent      — événement transportant un pointeur + données de taille libre
//   NkCustomStringEvent   — événement transportant une NkString arbitraire
//
// Usage recommandé :
//
//   // Définir un type d'événement applicatif
//   enum class MyAppEvent : NkU32 {
//       PLAYER_DIED    = 1,
//       LEVEL_COMPLETE = 2,
//       SCORE_UPDATED  = 3,
//   };
//
//   // Envoyer avec payload
//   struct ScorePayload { int score; int level; };
//   NkCustomEvent ev(static_cast<NkU32>(MyAppEvent::SCORE_UPDATED));
//   ev.SetPayload(ScorePayload{4200, 3});
//   dispatcher.Dispatch(ev);
//
//   // Recevoir
//   void OnEvent(NkEvent& e) {
//       if (auto* ce = e.As<NkCustomEvent>()) {
//           if (ce->GetCustomType() == static_cast<NkU32>(MyAppEvent::SCORE_UPDATED)) {
//               ScorePayload p{};
//               ce->GetPayload(p);
//               UpdateHud(p.score);
//           }
//       }
//   }
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkStringUtils.h"
#include <string>
#include <vector>
#include <cstring>
#include <memory>

namespace nkentseu {

    // =========================================================================
    // Constantes
    // =========================================================================

    /// @brief Taille maximale du payload inline de NkCustomEvent [octets]
    static constexpr NkU32 NK_CUSTOM_PAYLOAD_MAX = 128;

    // =========================================================================
    // NkCustomEvent — événement générique à payload fixe (128 octets max)
    // =========================================================================

    /**
     * @brief Événement personnalisé avec un payload inline de taille fixe.
     *
     * Convient pour transporter des structures de données simples (POD) jusqu'à
     * NK_CUSTOM_PAYLOAD_MAX octets sans allocation dynamique.
     *
     * customType : identifiant libre défini par l'application (ex: valeur
     *              d'une enum).
     * userPtr    : pointeur opaque optionnel (l'application gère la durée de
     *              vie ; éviter si l'événement peut être copié).
     *
     * @warning  T doit être trivially copyable (std::memcpy sûr).
     */
    class NkCustomEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_CUSTOM)
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_CUSTOM)

        /**
         * @param customType Identifiant applicatif de l'événement.
         * @param windowId   Identifiant de la fenêtre source.
         */
        explicit NkCustomEvent(NkU32 customType = 0,
                                NkU64 windowId   = 0) noexcept
            : NkEvent(windowId)
            , mCustomType(customType)
        {
            std::memset(mPayload, 0, sizeof(mPayload));
        }

        NkEvent*    Clone()    const override { return new NkCustomEvent(*this); }
        NkString ToString() const override {
            return "CustomEvent(type=" + string::NkToString(mCustomType)
                 + " dataSize=" + string::NkToString(mDataSize) + "B)";
        }

        // --- Accès au type applicatif ---

        NkU32  GetCustomType() const noexcept { return mCustomType; }
        void   SetCustomType(NkU32 t) noexcept { mCustomType = t; }

        // --- Accès au pointeur utilisateur ---

        void*  GetUserPtr() const noexcept { return mUserPtr; }
        void   SetUserPtr(void* p) noexcept { mUserPtr = p; }

        // --- Payload typé (template) ---

        /**
         * @brief Stocke une valeur T dans le payload inline.
         *
         * T doit être trivially copyable et sizeof(T) <= NK_CUSTOM_PAYLOAD_MAX.
         */
        template<typename T>
        bool SetPayload(const T& value) noexcept {
            static_assert(sizeof(T) <= NK_CUSTOM_PAYLOAD_MAX,
                          "NkCustomEvent: payload trop grand (max 128 octets)");
            std::memcpy(mPayload, &value, sizeof(T));
            mDataSize = static_cast<NkU32>(sizeof(T));
            return true;
        }

        /**
         * @brief Récupère la valeur T du payload.
         * @return false si le payload stocké est plus petit que sizeof(T).
         */
        template<typename T>
        bool GetPayload(T& out) const noexcept {
            if (mDataSize < sizeof(T)) return false;
            std::memcpy(&out, mPayload, sizeof(T));
            return true;
        }

        // --- Accès brut ---

        const NkU8* GetRawPayload() const noexcept { return mPayload; }
        NkU8*       GetRawPayload()       noexcept { return mPayload; }
        NkU32       GetDataSize()   const noexcept { return mDataSize; }

        bool        HasPayload()    const noexcept { return mDataSize > 0; }

    private:
        NkU32 mCustomType = 0;
        NkU32 mDataSize   = 0;
        void* mUserPtr    = nullptr;
        NkU8  mPayload[NK_CUSTOM_PAYLOAD_MAX] = {};
    };

    // =========================================================================
    // NkCustomPtrEvent — événement avec pointeur + données de taille libre
    // =========================================================================

    /**
     * @brief Événement personnalisé transportant un pointeur vers des données
     *        de taille arbitraire.
     *
     * Les données sont copiées dans un NkVector<NkU8> géré par l'événement.
     * Convient pour des payloads plus larges que NK_CUSTOM_PAYLOAD_MAX, ou
     * dont la taille n'est connue qu'au moment de l'exécution.
     *
     * userPtr : pointeur opaque optionnel NON géré par l'événement.
     */
    class NkCustomPtrEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_CUSTOM)
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_CUSTOM)

        /**
         * @param customType Identifiant applicatif.
         * @param data       Données à copier dans l'événement.
         * @param userPtr    Pointeur opaque (durée de vie gérée par l'appelant).
         * @param windowId   Identifiant de la fenêtre.
         */
        NkCustomPtrEvent(NkU32             customType,
                          NkVector<NkU8> data      = {},
                          void*             userPtr    = nullptr,
                          NkU64             windowId   = 0)
            : NkEvent(windowId)
            , mCustomType(customType)
            , mData(std::move(data))
            , mUserPtr(userPtr)
        {}

        NkEvent*    Clone()    const override { return new NkCustomPtrEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("CustomPtrEvent(type={0} size={1}B)", mCustomType, mData.size());
        }

        NkU32                    GetCustomType() const noexcept { return mCustomType; }
        void*                    GetUserPtr()    const noexcept { return mUserPtr; }
        void                     SetUserPtr(void* p) noexcept { mUserPtr = p; }
        const NkVector<NkU8>& GetData()       const noexcept { return mData; }
        NkVector<NkU8>&       GetData()             noexcept { return mData; }
        const NkU8*              GetBytes()      const noexcept { return mData.Data(); }
        NkU32                    GetSize()       const noexcept { return static_cast<NkU32>(mData.size()); }
        bool                     HasData()       const noexcept { return !mData.empty(); }

        /**
         * @brief Interprète les données comme une référence à T.
         * @return nullptr si les données sont insuffisantes.
         */
        template<typename T>
        const T* ViewAs() const noexcept {
            return (mData.size() >= sizeof(T))
                   ? reinterpret_cast<const T*>(mData.Data())
                   : nullptr;
        }

    private:
        NkU32             mCustomType = 0;
        NkVector<NkU8> mData;
        void*             mUserPtr    = nullptr;
    };

    // =========================================================================
    // NkCustomStringEvent — événement transportant un message texte
    // =========================================================================

    /**
     * @brief Événement personnalisé simple transportant un message texte UTF-8.
     *
     * Utile pour les notifications applicatives légères, les messages de
     * journalisation via le bus d'événements, ou les commandes scriptées.
     *
     * @code
     *   // Envoyer
     *   dispatcher.Dispatch(NkCustomStringEvent(100, "LEVEL_COMPLETE", "World 1-1"));
     *
     *   // Recevoir
     *   if (auto* e = evt.As<NkCustomStringEvent>()) {
     *       if (e->GetTag() == "LEVEL_COMPLETE") {
     *           LoadNextLevel(e->GetMessage());
     *       }
     *   }
     * @endcode
     */
    class NkCustomStringEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_CUSTOM)
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_CUSTOM)

        /**
         * @param customType Identifiant applicatif.
         * @param tag        Étiquette courte (ex: "LEVEL_COMPLETE", "SCORE").
         * @param message    Contenu textuel du message.
         * @param windowId   Identifiant de la fenêtre.
         */
        NkCustomStringEvent(NkU32       customType = 0,
                             NkString tag        = {},
                             NkString message    = {},
                             NkU64       windowId   = 0)
            : NkEvent(windowId)
            , mCustomType(customType)
            , mTag(std::move(tag))
            , mMessage(std::move(message))
        {}

        NkEvent*    Clone()    const override { return new NkCustomStringEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("CustomString(type={0} tag=\"{1}\" msg=\"{2}{3}\")",
                mCustomType, mTag, mMessage.SubStr(0, 32),
                (mMessage.Size() > 32 ? "..." : ""));
        }

        NkU32              GetCustomType() const noexcept { return mCustomType; }
        const NkString& GetTag()        const noexcept { return mTag; }
        const NkString& GetMessage()    const noexcept { return mMessage; }

        bool HasTag()     const noexcept { return !mTag.Empty(); }
        bool HasMessage() const noexcept { return !mMessage.Empty(); }

        /// @brief Compare l'étiquette avec la chaîne donnée
        bool IsTag(const char* tag) const noexcept {
            return mTag == tag;
        }

    private:
        NkU32       mCustomType = 0;
        NkString mTag;
        NkString mMessage;
    };

} // namespace nkentseu