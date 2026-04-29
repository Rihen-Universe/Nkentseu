// =============================================================================
// NKEvent/NkCustomEvents.h
// Événements personnalisés définis par l'application.
//
// Catégorie : NK_CAT_CUSTOM
//
// Architecture :
//   - NkCustomEvent        : payload inline fixe (128 octets max) pour POD simples
//   - NkCustomPtrEvent     : payload dynamique via NkVector<uint8> pour données arbitraires
//   - NkCustomStringEvent  : message texte UTF-8 avec tag pour routing sémantique
//
// Design :
//   - Templates type-safe pour SetPayload/GetPayload avec static_assert de sécurité
//   - Copie mémoire via memory::NkMemCopy pour performance et portabilité
//   - Pointeurs utilisateur opaques (void*) pour métadonnées externes
//   - Méthodes ViewAs<T>() pour interprétation zero-copy des données binaires
//
// Usage recommandé :
//   // Définir un type d'événement applicatif
//   enum class MyAppEvent : uint32 {
//       PLAYER_DIED    = 1,
//       LEVEL_COMPLETE = 2,
//       SCORE_UPDATED  = 3,
//   };
//
//   // Envoyer avec payload POD
//   struct ScorePayload { int score; int level; };
//   NkCustomEvent ev(static_cast<uint32>(MyAppEvent::SCORE_UPDATED));
//   ev.SetPayload(ScorePayload{4200, 3});
//   dispatcher.Dispatch(ev);
//
//   // Recevoir avec type-safe extraction
//   void OnEvent(NkEvent& e) {
//       if (auto* ce = e.As<NkCustomEvent>()) {
//           if (ce->GetCustomType() == static_cast<uint32>(MyAppEvent::SCORE_UPDATED)) {
//               ScorePayload p{};
//               if (ce->GetPayload(p)) {
//                   UpdateHud(p.score, p.level);
//               }
//           }
//       }
//   }
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKCUSTOMEVENTS_H
#define NKENTSEU_EVENT_NKCUSTOMEVENTS_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des dépendances internes au projet.
    // Toutes les dépendances utilisent les modules NK* du framework.

    #include "NKEvent/NkEvent.h"                        // Classe de base NkEvent + macros
    #include "NKEvent/NkEventApi.h"                     // Macros d'export NKENTSEU_EVENT_API
    #include "NKContainers/String/NkStringUtils.h"      // Conversion types → NkString
    #include "NKCore/NkTraits.h"                        // Utilitaires de traits (NkMove, etc.)
    #include "NKMemory/NkUtils.h"                       // Utilitaires mémoire (NkMemCopy, etc.)

    namespace nkentseu {

        // =====================================================================
        // Constantes globales pour les événements personnalisés
        // =====================================================================

        /// @brief Taille maximale du payload inline de NkCustomEvent [octets]
        /// @note Cette limite garantit l'absence d'allocation dynamique pour les petits payloads
        /// @note Pour des données plus grandes, utiliser NkCustomPtrEvent avec NkVector
        static constexpr uint32 NK_CUSTOM_PAYLOAD_MAX = 128;

        // =====================================================================
        // NkCustomEvent — Événement générique à payload inline fixe
        // =====================================================================

        /**
         * @brief Événement personnalisé avec payload inline de taille fixe (128 octets max)
         *
         * Cette classe est optimisée pour transporter des structures de données simples (POD)
         * sans allocation dynamique, idéale pour les événements fréquents et performants :
         *   - Scores, statistiques, compteurs
         *   - États de jeu simples (bool, enum, petits structs)
         *   - Commandes de contrôle avec paramètres limités
         *
         * Fonctionnalités :
         *   - customType : identifiant libre défini par l'application (ex: valeur d'enum)
         *   - userPtr    : pointeur opaque optionnel pour métadonnées externes (non géré)
         *   - payload    : buffer inline de NK_CUSTOM_PAYLOAD_MAX octets pour données POD
         *
         * @warning Le type T utilisé avec SetPayload/GetPayload DOIT être trivially copyable
         *          pour garantir la sécurité de memory::NkMemCopy (pas de constructeurs,
         *          pas de pointeurs internes, pas de ressources à gérer).
         *
         * @warning sizeof(T) DOIT être <= NK_CUSTOM_PAYLOAD_MAX (128 octets), vérifié par
         *          static_assert à la compilation pour éviter les débordements mémoire.
         *
         * @note Pour des payloads plus grands ou des types complexes, utiliser
         *       NkCustomPtrEvent (avec NkVector) ou NkCustomStringEvent (pour texte).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkCustomEvent final : public NkEvent {
            public:
                /// @brief Associe le type NK_CUSTOM et la catégorie NK_CAT_CUSTOM à cette classe
                NK_EVENT_TYPE_FLAGS(NK_CUSTOM)
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_CUSTOM)

                /**
                 * @brief Constructeur avec identifiant applicatif et fenêtre source
                 * @param customType Identifiant libre défini par l'application (ex: enum cast)
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 */
                NKENTSEU_EVENT_API NkCustomEvent(
                    uint32 customType = 0,
                    uint64 windowId = 0
                ) noexcept
                    : NkEvent(windowId)
                    , mCustomType(customType)
                    , mDataSize(0)
                    , mUserPtr(nullptr) {
                    // Initialisation sécurisée du buffer payload à zéro
                    memory::NkMemSet(mPayload, 0, sizeof(mPayload));
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                /// @note Copie profonde du payload inline via copy-ctor par défaut
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkCustomEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le type custom et la taille des données
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return "CustomEvent(type=" + string::NkToString(mCustomType)
                         + " dataSize=" + string::NkToString(mDataSize) + "B)";
                }

                // -------------------------------------------------------------
                // Accès au type applicatif personnalisé
                // -------------------------------------------------------------

                /// @brief Retourne l'identifiant applicatif de l'événement
                /// @return uint32 défini par l'application (ex: static_cast<uint32>(MyEnum::Value))
                NKENTSEU_EVENT_API_INLINE uint32 GetCustomType() const noexcept {
                    return mCustomType;
                }

                /// @brief Définit l'identifiant applicatif de l'événement
                /// @param t Nouvelle valeur d'identifiant à associer
                NKENTSEU_EVENT_API_INLINE void SetCustomType(uint32 t) noexcept {
                    mCustomType = t;
                }

                // -------------------------------------------------------------
                // Accès au pointeur utilisateur opaque
                // -------------------------------------------------------------

                /// @brief Retourne le pointeur utilisateur associé (métadonnées externes)
                /// @return void* opaque (durée de vie gérée par l'appelant, non-owned)
                NKENTSEU_EVENT_API_INLINE void* GetUserPtr() const noexcept {
                    return mUserPtr;
                }

                /// @brief Définit le pointeur utilisateur associé
                /// @param p Nouveau pointeur opaque à associer (non-owned)
                /// @note L'événement ne gère pas la mémoire pointée : responsabilité de l'appelant
                NKENTSEU_EVENT_API_INLINE void SetUserPtr(void* p) noexcept {
                    mUserPtr = p;
                }

                // -------------------------------------------------------------
                // Payload typé via templates (type-safe)
                // -------------------------------------------------------------

                /**
                 * @brief Stocke une valeur de type T dans le payload inline
                 * @tparam T Type de la valeur à copier (doit être trivially copyable)
                 * @param value Référence constante vers la valeur à copier dans le payload
                 * @return true si la copie a réussi (toujours true sauf static_assert)
                 *
                 * @note Vérification à la compilation : sizeof(T) <= NK_CUSTOM_PAYLOAD_MAX
                 * @note Utilise memory::NkMemCopy pour copie binaire efficace et portable
                 * @note Met à jour mDataSize pour permettre la validation dans GetPayload
                 *
                 * @warning T doit être trivially copyable : pas de constructeurs complexes,
                 *          pas de pointeurs internes, pas de ressources à gérer manuellement.
                 *          Pour les types complexes, utiliser NkCustomPtrEvent.
                 */
                template<typename T>
                bool SetPayload(const T& value) noexcept {
                    // Vérification à la compilation de la taille maximale du payload
                    static_assert(
                        sizeof(T) <= NK_CUSTOM_PAYLOAD_MAX,
                        "NkCustomEvent: payload trop grand (max 128 octets). "
                        "Utiliser NkCustomPtrEvent pour des données plus volumineuses."
                    );

                    // Copie binaire sécurisée de la valeur dans le buffer inline
                    memory::NkMemCopy(mPayload, &value, sizeof(T));

                    // Mise à jour de la taille effective des données stockées
                    mDataSize = static_cast<uint32>(sizeof(T));

                    return true;
                }

                /**
                 * @brief Extrait une valeur de type T depuis le payload inline
                 * @tparam T Type de la valeur à extraire (doit correspondre au type stocké)
                 * @param out Référence vers la variable de destination pour la copie
                 * @return true si l'extraction a réussi, false si mDataSize < sizeof(T)
                 *
                 * @note Vérification à l'exécution : la taille stockée doit être suffisante
                 * @note Utilise memory::NkMemCopy pour extraction binaire efficace
                 * @note Ne modifie pas mDataSize : lecture seule
                 *
                 * @warning L'appelant doit garantir que T correspond au type réellement stocké.
                 *          Un mismatch de type entraîne un comportement indéfini (UB).
                 *          Pour plus de sécurité, vérifier GetCustomType() avant extraction.
                 */
                template<typename T>
                bool GetPayload(T& out) const noexcept {
                    // Validation : la taille stockée doit être au moins égale à sizeof(T)
                    if (mDataSize < sizeof(T)) {
                        return false;
                    }

                    // Copie binaire sécurisée depuis le buffer inline vers la destination
                    memory::NkMemCopy(&out, mPayload, sizeof(T));

                    return true;
                }

                // -------------------------------------------------------------
                // Accès brut au payload (pour sérialisation ou interop bas niveau)
                // -------------------------------------------------------------

                /// @brief Retourne un pointeur en lecture seule vers le buffer payload
                /// @return const uint8* pointant vers le début des données brutes
                NKENTSEU_EVENT_API_INLINE const uint8* GetRawPayload() const noexcept {
                    return mPayload;
                }

                /// @brief Retourne un pointeur modifiable vers le buffer payload
                /// @return uint8* pointant vers le début des données brutes
                /// @note À utiliser avec précaution : pas de vérification de type
                NKENTSEU_EVENT_API_INLINE uint8* GetRawPayload() noexcept {
                    return mPayload;
                }

                /// @brief Retourne la taille effective des données stockées dans le payload
                /// @return uint32 représentant le nombre d'octets valides dans mPayload
                NKENTSEU_EVENT_API_INLINE uint32 GetDataSize() const noexcept {
                    return mDataSize;
                }

                /// @brief Indique si le payload contient des données (taille > 0)
                /// @return true si mDataSize > 0, false si le payload est vide
                NKENTSEU_EVENT_API_INLINE bool HasPayload() const noexcept {
                    return mDataSize > 0;
                }

            private:
                uint32 mCustomType;                          ///< Identifiant applicatif libre
                uint32 mDataSize;                            ///< Taille effective des données [octets]
                void* mUserPtr;                              ///< Pointeur opaque utilisateur (non-owned)
                uint8 mPayload[NK_CUSTOM_PAYLOAD_MAX];       ///< Buffer inline pour données POD
        };

        // =====================================================================
        // NkCustomPtrEvent — Événement avec payload dynamique via NkVector
        // =====================================================================

        /**
         * @brief Événement personnalisé transportant des données de taille arbitraire
         *
         * Cette classe est conçue pour les payloads qui dépassent la limite de 128 octets
         * de NkCustomEvent, ou dont la taille n'est connue qu'à l'exécution :
         *   - Données binaires sérialisées (protobuf, JSON binaire, etc.)
         *   - Tableaux dynamiques de structures complexes
         *   - Buffers de réseau ou de fichier chargés en mémoire
         *
         * Gestion mémoire :
         *   - Les données sont copiées dans un NkVector<uint8> interne (ownership)
         *   - L'événement gère automatiquement l'allocation et la libération
         *   - Le pointeur userPtr reste non-owned (responsabilité de l'appelant)
         *
         * @note Préférer NkCustomEvent pour les petits payloads POD : évite l'allocation heap.
         * @note Utiliser GetData().Data() pour accès direct au buffer, ou ViewAs<T>() pour casting.
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkCustomPtrEvent final : public NkEvent {
            public:
                /// @brief Associe le type NK_CUSTOM et la catégorie NK_CAT_CUSTOM à cette classe
                NK_EVENT_TYPE_FLAGS(NK_CUSTOM)
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_CUSTOM)

                /**
                 * @brief Constructeur avec données copiées et métadonnées
                 * @param customType Identifiant applicatif libre (ex: enum cast en uint32)
                 * @param data Vecteur de données à copier dans l'événement (move semantics)
                 * @param userPtr Pointeur opaque optionnel pour métadonnées externes (non-owned)
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 *
                 * @note Le vecteur data est déplacé (move) si possible, sinon copié
                 * @note L'événement prend ownership des données : pas de risque de dangling
                 */
                NKENTSEU_EVENT_API NkCustomPtrEvent(
                    uint32 customType,
                    NkVector<uint8> data = {},
                    void* userPtr = nullptr,
                    uint64 windowId = 0
                )
                    : NkEvent(windowId)
                    , mCustomType(customType)
                    , mData(traits::NkMove(data))
                    , mUserPtr(userPtr) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                /// @note Copie profonde du vecteur mData via copy-ctor de NkVector
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkCustomPtrEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString décrivant le type custom et la taille des données
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "CustomPtrEvent(type={0} size={1}B)",
                        mCustomType,
                        mData.size()
                    );
                }

                // -------------------------------------------------------------
                // Accès aux métadonnées et au payload
                // -------------------------------------------------------------

                /// @brief Retourne l'identifiant applicatif de l'événement
                /// @return uint32 défini par l'application
                NKENTSEU_EVENT_API_INLINE uint32 GetCustomType() const noexcept {
                    return mCustomType;
                }

                /// @brief Retourne le pointeur utilisateur opaque associé
                /// @return void* pour métadonnées externes (non-owned, durée de vie appelant)
                NKENTSEU_EVENT_API_INLINE void* GetUserPtr() const noexcept {
                    return mUserPtr;
                }

                /// @brief Définit le pointeur utilisateur opaque associé
                /// @param p Nouveau pointeur à associer (non-owned)
                NKENTSEU_EVENT_API_INLINE void SetUserPtr(void* p) noexcept {
                    mUserPtr = p;
                }

                /// @brief Retourne une référence constante vers le vecteur de données
                /// @return const NkVector<uint8>& pour lecture seule des données
                NKENTSEU_EVENT_API_INLINE const NkVector<uint8>& GetData() const noexcept {
                    return mData;
                }

                /// @brief Retourne une référence modifiable vers le vecteur de données
                /// @return NkVector<uint8>& pour modification directe (rare, avancé)
                NKENTSEU_EVENT_API_INLINE NkVector<uint8>& GetData() noexcept {
                    return mData;
                }

                /// @brief Retourne un pointeur en lecture seule vers le buffer interne
                /// @return const uint8* pointant vers les données brutes (ou nullptr si vide)
                NKENTSEU_EVENT_API_INLINE const uint8* GetBytes() const noexcept {
                    return mData.Data();
                }

                /// @brief Retourne la taille des données en octets
                /// @return uint32 représentant le nombre d'octets dans le vecteur
                NKENTSEU_EVENT_API_INLINE uint32 GetSize() const noexcept {
                    return static_cast<uint32>(mData.size());
                }

                /// @brief Indique si l'événement contient des données (vecteur non vide)
                /// @return true si mData.empty() == false
                NKENTSEU_EVENT_API_INLINE bool HasData() const noexcept {
                    return !mData.empty();
                }

                // -------------------------------------------------------------
                // Interprétation typée des données (zero-copy view)
                // -------------------------------------------------------------

                /**
                 * @brief Interprète les données brutes comme une référence à un type T
                 * @tparam T Type cible pour l'interprétation (doit être POD/trivially copyable)
                 * @return const T* pointant vers les données réinterprétées, ou nullptr si taille insuffisante
                 *
                 * @note Aucune copie : retourne un pointeur directement dans le buffer interne
                 * @note Validation : retourne nullptr si mData.size() < sizeof(T)
                 * @note Durée de vie : le pointeur est valide tant que l'événement existe
                 *
                 * @warning L'appelant doit garantir que les données sont effectivement de type T.
                 *          Un mismatch entraîne un comportement indéfini (UB).
                 *          Vérifier GetCustomType() et GetSize() avant d'utiliser le résultat.
                 *
                 * @warning Le pointeur retourné ne doit pas être utilisé après la destruction
                 *          de l'événement, ni passé à un thread différent sans synchronisation.
                 */
                template<typename T>
                const T* ViewAs() const noexcept {
                    // Validation de taille avant réinterprétation
                    if (mData.size() >= sizeof(T)) {
                        // Réinterprétation zero-copy du buffer comme type T
                        return reinterpret_cast<const T*>(mData.Data());
                    }
                    // Retourne nullptr si les données sont insuffisantes
                    return nullptr;
                }

            private:
                uint32 mCustomType;           ///< Identifiant applicatif libre
                NkVector<uint8> mData;        ///< Vecteur propriétaire des données binaires
                void* mUserPtr;               ///< Pointeur opaque utilisateur (non-owned)
        };

        // =====================================================================
        // NkCustomStringEvent — Événement transportant un message texte UTF-8
        // =====================================================================

        /**
         * @brief Événement personnalisé simple transportant un message texte UTF-8 avec tag
         *
         * Cette classe est optimisée pour les scénarios de communication légère via le bus
         * d'événements, sans besoin de sérialisation binaire complexe :
         *   - Notifications utilisateur ("Niveau terminé !", "Succès : objet obtenu")
         *   - Messages de journalisation via le système d'événements (découplage logger)
         *   - Commandes scriptées ou configuration dynamique ("set.volume=0.8")
         *   - Communication inter-modules avec sémantique textuelle
         *
         * Structure :
         *   - customType : identifiant numérique pour filtrage rapide par type
         *   - tag        : étiquette textuelle courte pour routing sémantique (ex: "LEVEL_COMPLETE")
         *   - message    : contenu textuel principal, UTF-8, longueur arbitraire
         *
         * @code
         *   // Envoyer un événement de notification
         *   dispatcher.Dispatch(
         *       NkCustomStringEvent(
         *           100,                           // customType
         *           "LEVEL_COMPLETE",              // tag pour routing
         *           "World 1-1 cleared! Score: 4200"  // message détaillé
         *       )
         *   );
         *
         *   // Recevoir et router par tag
         *   if (auto* e = evt.As<NkCustomStringEvent>()) {
         *       if (e->IsTag("LEVEL_COMPLETE")) {
         *           LoadNextLevel(e->GetMessage());
         *       }
         *       else if (e->IsTag("PLAYER_DIED")) {
         *           ShowGameOverScreen();
         *       }
         *   }
         * @endcode
         *
         * @note NkString utilise l'encodage UTF-8 : compatible avec toutes les langues.
         * @note Le tag est conçu pour être court et comparé rapidement (IsTag() optimisée).
         */
        class NKENTSEU_EVENT_CLASS_EXPORT NkCustomStringEvent final : public NkEvent {
            public:
                /// @brief Associe le type NK_CUSTOM et la catégorie NK_CAT_CUSTOM à cette classe
                NK_EVENT_TYPE_FLAGS(NK_CUSTOM)
                NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_CUSTOM)

                /**
                 * @brief Constructeur avec identifiant, tag et message
                 * @param customType Identifiant applicatif libre (ex: enum cast en uint32)
                 * @param tag Étiquette textuelle courte pour routing sémantique (ex: "SCORE")
                 * @param message Contenu textuel principal, UTF-8, longueur arbitraire
                 * @param windowId Identifiant de la fenêtre source (0 = événement global)
                 *
                 * @note Les paramètres NkString sont déplacés (move) si possible pour performance
                 * @note Le tag et le message peuvent être vides : vérifier avec HasTag()/HasMessage()
                 */
                NKENTSEU_EVENT_API NkCustomStringEvent(
                    uint32 customType = 0,
                    NkString tag = {},
                    NkString message = {},
                    uint64 windowId = 0
                )
                    : NkEvent(windowId)
                    , mCustomType(customType)
                    , mTag(traits::NkMove(tag))
                    , mMessage(traits::NkMove(message)) {
                }

                /// @brief Crée une copie polymorphe de cet événement sur le heap
                /// @return Pointeur brut vers une nouvelle instance (caller responsable du delete)
                /// @note Copie profonde des NkString via copy-ctor de NkString
                NKENTSEU_EVENT_API NkEvent* Clone() const override {
                    return new NkCustomStringEvent(*this);
                }

                /// @brief Retourne une représentation lisible pour le débogage et les logs
                /// @return NkString tronqué à 32 caractères pour éviter les logs trop longs
                NKENTSEU_EVENT_API NkString ToString() const override {
                    return NkString::Fmt(
                        "CustomString(type={0} tag=\"{1}\" msg=\"{2}{3}\")",
                        mCustomType,
                        mTag,
                        mMessage.SubStr(0, 32),
                        (mMessage.Size() > 32 ? "..." : "")
                    );
                }

                // -------------------------------------------------------------
                // Accès aux métadonnées et au contenu textuel
                // -------------------------------------------------------------

                /// @brief Retourne l'identifiant applicatif de l'événement
                /// @return uint32 défini par l'application
                NKENTSEU_EVENT_API_INLINE uint32 GetCustomType() const noexcept {
                    return mCustomType;
                }

                /// @brief Retourne l'étiquette textuelle pour routing sémantique
                /// @return const NkString& référence vers le tag (lecture seule)
                NKENTSEU_EVENT_API_INLINE const NkString& GetTag() const noexcept {
                    return mTag;
                }

                /// @brief Retourne le contenu textuel principal du message
                /// @return const NkString& référence vers le message UTF-8 (lecture seule)
                NKENTSEU_EVENT_API_INLINE const NkString& GetMessage() const noexcept {
                    return mMessage;
                }

                /// @brief Indique si un tag est présent (non vide)
                /// @return true si mTag.Empty() == false
                NKENTSEU_EVENT_API_INLINE bool HasTag() const noexcept {
                    return !mTag.Empty();
                }

                /// @brief Indique si un message est présent (non vide)
                /// @return true si mMessage.Empty() == false
                NKENTSEU_EVENT_API_INLINE bool HasMessage() const noexcept {
                    return !mMessage.Empty();
                }

                /// @brief Compare l'étiquette avec une chaîne C donnée (optimisé)
                /// @param tag Chaîne C à comparer (ex: "LEVEL_COMPLETE")
                /// @return true si mTag == tag (comparaison exacte, case-sensitive)
                /// @note Plus efficace que GetTag() == tag car évite la construction temporaire
                NKENTSEU_EVENT_API_INLINE bool IsTag(const char* tag) const noexcept {
                    return mTag == tag;
                }

            private:
                uint32 mCustomType;    ///< Identifiant applicatif libre
                NkString mTag;         ///< Étiquette courte pour routing sémantique
                NkString mMessage;     ///< Contenu textuel principal UTF-8
        };

    } // namespace nkentseu

#endif // NKENTSEU_EVENT_NKCUSTOMEVENTS_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCUSTOMEVENTS.H
// =============================================================================
// Ce fichier définit les événements personnalisés pour extension applicative.
// Les exemples ci-dessous illustrent les patterns d'usage recommandés.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Système de scoring avec payload POD via NkCustomEvent
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkCustomEvents.h"

    // Définition des types d'événements métier
    enum class GameEvent : uint32 {
        SCORE_UPDATED = 1,
        PLAYER_LEVELUP = 2,
        ACHIEVEMENT_UNLOCKED = 3,
    };

    // Structure POD pour le payload (trivially copyable requis)
    struct ScoreUpdatePayload {
        int32 playerId;
        int32 scoreDelta;
        int32 newTotal;
        uint32 timestamp;  // Frame index ou timestamp
    };
    static_assert(std::is_trivially_copyable<ScoreUpdatePayload>::value, "Must be POD");

    // Envoi de l'événement
    void AwardPoints(int32 playerId, int32 points) {
        using namespace nkentseu;

        NkCustomEvent event(static_cast<uint32>(GameEvent::SCORE_UPDATED));

        ScoreUpdatePayload payload{
            .playerId = playerId,
            .scoreDelta = points,
            .newTotal = CalculateTotalScore(playerId) + points,
            .timestamp = static_cast<uint32>(GetCurrentFrameIndex())
        };

        event.SetPayload(payload);
        event.SetUserPtr(&GetPlayer(playerId));  // Métadonnée externe optionnelle

        EventManager::GetInstance().Dispatch(event);
    }

    // Réception et traitement
    void OnGameEvent(NkEvent& baseEvent) {
        using namespace nkentseu;

        if (auto* customEvt = baseEvent.As<NkCustomEvent>()) {
            if (customEvt->GetCustomType() == static_cast<uint32>(GameEvent::SCORE_UPDATED)) {
                ScoreUpdatePayload payload{};
                if (customEvt->GetPayload(payload)) {
                    UpdatePlayerHUD(payload.playerId, payload.newTotal);
                    PlayScoreSound(payload.scoreDelta > 0);

                    // Logging conditionnel
                    if (payload.scoreDelta >= 1000) {
                        NK_FOUNDATION_LOG_INFO("Big score! Player {} +{} = {}",
                            payload.playerId, payload.scoreDelta, payload.newTotal);
                    }
                }
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Transfert de données binaires arbitraires via NkCustomPtrEvent
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkCustomEvents.h"
    #include <cstring>

    // Envoi de données sérialisées (ex: état de jeu pour réseau ou replay)
    void SendGameStateSnapshot(uint32 snapshotType, const void* data, size_t size) {
        using namespace nkentseu;

        // Copie des données dans un vecteur géré par l'événement
        NkVector<uint8> buffer(size);
        if (data && size > 0) {
            std::memcpy(buffer.Data(), data, size);
        }

        // Création et dispatch de l'événement
        NkCustomPtrEvent event(snapshotType, traits::NkMove(buffer));
        EventManager::GetInstance().Dispatch(event);
    }

    // Réception avec interprétation typée zero-copy
    void OnGameStateSnapshot(NkEvent& baseEvent) {
        using namespace nkentseu;

        if (auto* ptrEvt = baseEvent.As<NkCustomPtrEvent>()) {
            // Vérification du type avant interprétation
            if (ptrEvt->GetCustomType() == SNAPTYPE_PLAYER_STATE) {
                // ViewAs retourne nullptr si taille insuffisante : sécurité intégrée
                if (auto* state = ptrEvt->ViewAs<PlayerStateSnapshot>()) {
                    ApplyPlayerState(*state);
                }
            }
            else if (ptrEvt->GetCustomType() == SNAPTYPE_WORLD_STATE) {
                if (auto* world = ptrEvt->ViewAs<WorldStateSnapshot>()) {
                    ApplyWorldState(*world);
                }
            }
        }
    }

    // Alternative : accès brut pour désérialisation personnalisée
    void OnCustomBinary(NkCustomPtrEvent& event) {
        const uint8* bytes = event.GetBytes();
        uint32 size = event.GetSize();

        if (bytes && size >= 4) {
            // Lecture d'un header 32-bit big-endian par exemple
            uint32 header = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
            ProcessCustomProtocol(header, bytes + 4, size - 4);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Système de notifications textuelles via NkCustomStringEvent
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkCustomEvents.h"

    class NotificationSystem {
    public:
        // Envoi d'une notification utilisateur
        void ShowNotification(uint32 category, const char* tag, const char* message) {
            using namespace nkentseu;

            NkCustomStringEvent event(category, tag, message);
            EventManager::GetInstance().Dispatch(event);
        }

        // Enregistrement des handlers par tag
        void RegisterHandlers(nkentseu::event::EventManager& mgr) {
            using namespace nkentseu;

            mgr.Subscribe<NkCustomStringEvent>(
                [this](NkCustomStringEvent& event) {
                    // Routing par tag pour traitement spécifique
                    if (event.IsTag("QUEST_UPDATE")) {
                        ShowQuestNotification(event.GetMessage());
                    }
                    else if (event.IsTag("ITEM_ACQUIRED")) {
                        ShowItemAcquiredToast(event.GetMessage());
                    }
                    else if (event.IsTag("CHAT_MESSAGE")) {
                        AppendToChatLog(event.GetMessage());
                    }
                    // Fallback : notification générique
                    else if (event.HasMessage()) {
                        ShowGenericToast(event.GetMessage());
                    }
                }
            );
        }

    private:
        void ShowQuestNotification(const nkentseu::NkString& text);
        void ShowItemAcquiredToast(const nkentseu::NkString& text);
        void AppendToChatLog(const nkentseu::NkString& text);
        void ShowGenericToast(const nkentseu::NkString& text);
    };

    // Usage :
    // notifications.ShowNotification(
    //     100,
    //     "QUEST_UPDATE",
    //     "New quest available: \"The Lost Artifact\""
    // );
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Pattern de sérialisation/désérialisation avec custom events
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkCustomEvents.h"
    #include "NKSerialization/NkSerializer.h"  // Supposé exister

    // Sérialisation d'un état complexe vers événement personnalisé
    NkCustomPtrEvent SerializeGameState(const GameState& state, uint32 eventType) {
        using namespace nkentseu;

        // Sérialisation dans un buffer temporaire
        NkVector<uint8> buffer;
        if (NkSerializer::ToBinary(state, buffer)) {
            // Création de l'événement avec le buffer sérialisé (move semantics)
            return NkCustomPtrEvent(eventType, traits::NkMove(buffer));
        }
        // Fallback : événement vide en cas d'échec de sérialisation
        return NkCustomPtrEvent(eventType);
    }

    // Désérialisation depuis événement personnalisé
    bool DeserializeGameState(const NkCustomPtrEvent& event, GameState& outState) {
        if (!event.HasData()) {
            return false;
        }

        // Désérialisation depuis le buffer interne de l'événement
        return NkSerializer::FromBinary(
            event.GetBytes(),
            event.GetSize(),
            outState
        );
    }

    // Usage réseau : envoi d'état à un client distant
    void SendStateToClient(ClientId clientId, const GameState& state) {
        auto event = SerializeGameState(state, NET_EVENT_GAME_STATE);
        Network::SendEvent(clientId, event);  // Transmission via socket
    }

    // Usage replay : enregistrement d'états pour rejeu ultérieur
    class ReplayRecorder {
    public:
        void RecordFrame(const GameState& state) {
            auto event = SerializeGameState(state, REPLAY_EVENT_FRAME);
            event.SetCustomType(static_cast<uint32>(mCurrentFrame++));
            mRecordedFrames.push_back(traits::NkMove(event));
        }

        void SaveToFile(const char* filename) {
            NkFile file(filename, FileMode::Write);
            for (const auto& event : mRecordedFrames) {
                // Écriture du customType + taille + données
                uint32 type = event.GetCustomType();
                uint32 size = event.GetSize();
                file.Write(&type, sizeof(type));
                file.Write(&size, sizeof(size));
                file.Write(event.GetBytes(), size);
            }
        }

    private:
        uint64 mCurrentFrame = 0;
        std::vector<NkCustomPtrEvent> mRecordedFrames;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Filtrage et routing avancé dans un EventManager personnalisé
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkCustomEvents.h"

    class SmartCustomDispatcher {
    public:
        // Enregistrement d'un handler avec filtrage par type ET tag
        template<typename Handler>
        void SubscribeByTag(uint32 customType, const char* tag, Handler&& handler) {
            using namespace nkentseu;

            mEventManager.Subscribe<NkCustomStringEvent>(
                [customType, tag, handler = std::forward<Handler>(handler)]
                (NkCustomStringEvent& event) mutable {
                    // Double filtrage : type numérique + tag textuel
                    if (event.GetCustomType() == customType && event.IsTag(tag)) {
                        handler(event);  // Appel du handler utilisateur
                    }
                }
            );
        }

        // Enregistrement d'un handler avec prédicat personnalisé
        template<typename Handler, typename Predicate>
        void SubscribeIf(uint32 customType, Predicate&& pred, Handler&& handler) {
            using namespace nkentseu;

            mEventManager.Subscribe<NkCustomEvent>(
                [customType, pred = std::forward<Predicate>(pred),
                 handler = std::forward<Handler>(handler)]
                (NkCustomEvent& event) mutable {
                    if (event.GetCustomType() == customType && pred(event)) {
                        handler(event);
                    }
                }
            );
        }

    private:
        nkentseu::event::EventManager& mEventManager;
    };

    // Usage :
    // dispatcher.SubscribeByTag(
    //     static_cast<uint32>(GameEvent::NOTIFICATION),
    //     "CRITICAL",
    //     [](NkCustomStringEvent& e) {
    //         ShowCriticalAlert(e.GetMessage());  // Handler spécialisé
    //     }
    // );
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. CHOIX ENTRE LES TROIS CLASSES CUSTOM :
       - NkCustomEvent        : POD simples <= 128 octets, performance max, pas d'allocation
       - NkCustomPtrEvent     : données arbitraires > 128 octets ou taille dynamique
       - NkCustomStringEvent  : messages textuels UTF-8 avec routing par tag sémantique

    2. SÉCURITÉ DES TEMPLATES (SetPayload/GetPayload/ViewAs) :
       - static_assert à la compilation pour sizeof(T) <= NK_CUSTOM_PAYLOAD_MAX
       - Vérification runtime de mDataSize avant extraction dans GetPayload()
       - ViewAs<T>() retourne nullptr si taille insuffisante : toujours vérifier le résultat
       - Documenter clairement le type attendu pour chaque customType dans l'API métier

    3. GESTION DES POINTERS UTILISATEUR (userPtr) :
       - Ces pointeurs sont NON-OWNED : l'événement ne les libère pas
       - Durée de vie : doit être >= durée de traitement de l'événement
       - Éviter de passer des pointeurs vers des objets stack locaux
       - Pour ownership, préférer inclure les données dans le payload lui-même

    4. PERFORMANCE ET ALLOCATIONS :
       - NkCustomEvent : zéro allocation heap (buffer inline) — idéal pour événements fréquents
       - NkCustomPtrEvent : allocation via NkVector — utiliser avec modération dans les boucles critiques
       - NkCustomStringEvent : allocation pour NkString — acceptable pour notifications peu fréquentes
       - Pour du logging haute fréquence, envisager un buffer pool ou logging asynchrone

    5. EXTENSIBILITÉ ET MAINTENANCE :
       - Centraliser la définition des customType dans un header commun (ex: GameEvents.h)
       - Documenter le format attendu du payload pour chaque type (struct, sérialisation...)
       - Utiliser des enums scoped (enum class) pour éviter les collisions de valeurs
       - Ajouter des tests unitaires pour la sérialisation/désérialisation des payloads

    6. COMPATIBILITÉ RÉSEAU ET REPLAY :
       - Pour la transmission réseau, garantir que le payload est sérialisable de façon portable
       - Éviter les pointeurs, les références et les types dépendants de la plateforme dans les payloads
       - Pour le replay, s'assurer que Clone() produit une copie fidèle et indépendante
       - Tester la round-trip : SetPayload → GetPayload doit restaurer la valeur exacte
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================