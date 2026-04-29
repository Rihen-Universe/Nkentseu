// =============================================================================
// NKEvent/NkEventApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKEvent.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKEvent uniquement pour la configuration de build NKEvent
//  - Indépendance totale des modes de build : NKEvent et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_EVENT_NKEVENTAPI_H
#define NKENTSEU_EVENT_NKEVENTAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKEvent dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKEVENT
    // -------------------------------------------------------------------------
    /**
     * @defgroup EventBuildConfig Configuration du Build NKEvent
     * @brief Macros pour contrôler le mode de compilation de NKEvent
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_EVENT_BUILD_SHARED_LIB : Compiler NKEvent en bibliothèque partagée
     *  - NKENTSEU_EVENT_STATIC_LIB : Utiliser NKEvent en mode bibliothèque statique
     *  - NKENTSEU_EVENT_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKEvent et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform en DLL + NKEvent en static.
     *
     * @example CMakeLists.txt
     * @code
     * # NKEvent en DLL, NKPlatform en static
     * target_compile_definitions(nkevent PRIVATE NKENTSEU_EVENT_BUILD_SHARED_LIB)
     * target_compile_definitions(nkevent PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform
     *
     * # NKEvent en static, NKPlatform en DLL
     * target_compile_definitions(monapp PRIVATE NKENTSEU_EVENT_STATIC_LIB)
     * # (NKPlatform en DLL par défaut, pas de define nécessaire)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_EVENT_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKEvent
     * @def NKENTSEU_EVENT_API
     * @ingroup EventApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKEvent.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_EVENT_BUILD_SHARED_LIB : export (compilation de NKEvent en DLL)
     *  - NKENTSEU_EVENT_STATIC_LIB ou NKENTSEU_EVENT_HEADER_ONLY : vide (pas d'export)
     *  - Sinon : import (utilisation de NKEvent en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication.
     *
     * @example
     * @code
     * // Dans un header public de NKEvent :
     * NKENTSEU_EVENT_API void EventRegisterCallback(EventId id, Callback cb);
     *
     * // Pour compiler NKEvent en DLL : -DNKENTSEU_EVENT_BUILD_SHARED_LIB
     * // Pour utiliser NKEvent en DLL : (aucun define, import par défaut)
     * // Pour utiliser NKEvent en static : -DNKENTSEU_EVENT_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_EVENT_BUILD_SHARED_LIB)
        #define NKENTSEU_EVENT_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_EVENT_STATIC_LIB) || defined(NKENTSEU_EVENT_HEADER_ONLY)
        #define NKENTSEU_EVENT_API
    #else
        #define NKENTSEU_EVENT_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKEVENT
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKEvent sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKEvent
     * @def NKENTSEU_EVENT_CLASS_EXPORT
     * @ingroup EventApiMacros
     *
     * Alias vers NKENTSEU_EVENT_API pour les déclarations de classe.
     *
     * @example
     * @code
     * class NKENTSEU_EVENT_CLASS_EXPORT EventManager {
     * public:
     *     void RegisterHandler(EventType type, Handler handler);
     * };
     * @endcode
     */
    #define NKENTSEU_EVENT_CLASS_EXPORT NKENTSEU_EVENT_API

    /**
     * @brief Fonction inline exportée pour NKEvent
     * @def NKENTSEU_EVENT_API_INLINE
     * @ingroup EventApiMacros
     *
     * Combinaison de NKENTSEU_EVENT_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_EVENT_HEADER_ONLY)
        #define NKENTSEU_EVENT_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_EVENT_API_INLINE NKENTSEU_EVENT_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKEvent
     * @def NKENTSEU_EVENT_API_FORCE_INLINE
     * @ingroup EventApiMacros
     *
     * Combinaison de NKENTSEU_EVENT_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_EVENT_HEADER_ONLY)
        #define NKENTSEU_EVENT_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_EVENT_API_FORCE_INLINE NKENTSEU_EVENT_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKEvent
     * @def NKENTSEU_EVENT_API_NO_INLINE
     * @ingroup EventApiMacros
     *
     * Combinaison de NKENTSEU_EVENT_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_EVENT_API_NO_INLINE NKENTSEU_EVENT_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup EventApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKEvent
     *
     * Pour éviter la duplication, NKEvent réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKEvent | Macro à utiliser | Source |
     * |---------------------|------------------|--------|
     * | Dépréciation | `NKENTSEU_DEPRECATED` | NKPlatform |
     * | Dépréciation avec message | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement cache | `NKENTSEU_ALIGN_CACHE` | NKPlatform |
     * | Alignement 16/32/64 | `NKENTSEU_ALIGN_16`, etc. | NKPlatform |
     * | Liaison C | `NKENTSEU_EXTERN_C_BEGIN/END` | NKPlatform |
     * | Pragmas | `NKENTSEU_PRAGMA(x)` | NKPlatform |
     * | Gestion warnings | `NKENTSEU_DISABLE_WARNING_*` | NKPlatform |
     * | Symbole local | `NKENTSEU_API_LOCAL` | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKEvent (pas de NKENTSEU_EVENT_DEPRECATED) :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser RegisterEventV2()")
     * NKENTSEU_EVENT_API void RegisterEventV1(EventId id);
     *
     * // Alignement dans NKEvent (pas de NKENTSEU_EVENT_ALIGN_32) :
     * struct NKENTSEU_ALIGN_32 NKENTSEU_EVENT_CLASS_EXPORT EventData {
     *     uint64_t timestamp;
     *     float payload[4];
     * };
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKEVENT
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKEvent.

    #if defined(NKENTSEU_EVENT_BUILD_SHARED_LIB) && defined(NKENTSEU_EVENT_STATIC_LIB)
        #warning "NKEvent: NKENTSEU_EVENT_BUILD_SHARED_LIB et NKENTSEU_EVENT_STATIC_LIB définis - NKENTSEU_EVENT_STATIC_LIB ignoré"
        #undef NKENTSEU_EVENT_STATIC_LIB
    #endif

    #if defined(NKENTSEU_EVENT_BUILD_SHARED_LIB) && defined(NKENTSEU_EVENT_HEADER_ONLY)
        #warning "NKEvent: NKENTSEU_EVENT_BUILD_SHARED_LIB et NKENTSEU_EVENT_HEADER_ONLY définis - NKENTSEU_EVENT_HEADER_ONLY ignoré"
        #undef NKENTSEU_EVENT_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_EVENT_STATIC_LIB) && defined(NKENTSEU_EVENT_HEADER_ONLY)
        #warning "NKEvent: NKENTSEU_EVENT_STATIC_LIB et NKENTSEU_EVENT_HEADER_ONLY définis - NKENTSEU_EVENT_HEADER_ONLY ignoré"
        #undef NKENTSEU_EVENT_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_EVENT_DEBUG
        #pragma message("NKEvent Export Config:")
        #if defined(NKENTSEU_EVENT_BUILD_SHARED_LIB)
            #pragma message("  NKEvent mode: Shared (export)")
        #elif defined(NKENTSEU_EVENT_STATIC_LIB)
            #pragma message("  NKEvent mode: Static")
        #elif defined(NKENTSEU_EVENT_HEADER_ONLY)
            #pragma message("  NKEvent mode: Header-only")
        #else
            #pragma message("  NKEvent mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_EVENT_API = " NKENTSEU_STRINGIZE(NKENTSEU_EVENT_API))
    #endif

#endif // NKENTSEU_EVENT_NKEVENTAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKEVENTAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKEvent, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKEvent (interface C)
// -----------------------------------------------------------------------------
/*
    // Dans nkevent_public.h :
    #include "NKEvent/NkEventApi.h"

    NKENTSEU_EVENT_EXTERN_C_BEGIN

    // Fonctions C publiques exportées pour l'API événementielle
    NKENTSEU_EVENT_API void Event_Initialize(void);
    NKENTSEU_EVENT_API void Event_Shutdown(void);
    NKENTSEU_EVENT_API bool Event_RegisterCallback(uint32_t eventId, EventCallback cb);
    NKENTSEU_EVENT_API bool Event_UnregisterCallback(uint32_t eventId, EventCallback cb);
    NKENTSEU_EVENT_API void Event_Dispatch(uint32_t eventId, const void* data, size_t size);

    NKENTSEU_EVENT_EXTERN_C_END

    // Dans nkevent.cpp :
    #include "nkevent_public.h"

    void Event_Initialize(void) {
        // Initialisation du système d'événements...
    }

    void Event_Shutdown(void) {
        // Nettoyage des ressources...
    }

    bool Event_RegisterCallback(uint32_t eventId, EventCallback cb) {
        // Enregistrement du callback...
        return true;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKEvent (interface C++)
// -----------------------------------------------------------------------------
/*
    // Dans EventManager.h :
    #include "NKEvent/NkEventApi.h"
    #include <functional>
    #include <unordered_map>

    namespace nkentseu {
    namespace event {

        // Type de callback pour les événements
        using EventCallback = std::function<void(const void*, size_t)>;

        class NKENTSEU_EVENT_CLASS_EXPORT EventManager {
        public:
            // Constructeur/Destructeur exportés
            NKENTSEU_EVENT_API EventManager();
            NKENTSEU_EVENT_API ~EventManager();

            // Méthodes publiques de gestion des événements
            NKENTSEU_EVENT_API bool Initialize();
            NKENTSEU_EVENT_API void Shutdown();
            NKENTSEU_EVENT_API bool RegisterHandler(uint32_t eventId, EventCallback handler);
            NKENTSEU_EVENT_API bool UnregisterHandler(uint32_t eventId, EventCallback handler);
            NKENTSEU_EVENT_API void Dispatch(uint32_t eventId, const void* data, size_t size);
            NKENTSEU_EVENT_API bool IsHandlerRegistered(uint32_t eventId) const;

            // Fonction inline critique pour la performance (accès fréquent)
            NKENTSEU_EVENT_API_FORCE_INLINE uint32_t GetEventCount() const {
                return static_cast<uint32_t>(m_handlers.size());
            }

            // Fonction inline pour accès rapide au statut
            NKENTSEU_EVENT_API_INLINE bool IsInitialized() const {
                return m_initialized;
            }

        protected:
            // Méthode protégée pour les classes dérivées
            NKENTSEU_EVENT_API void OnEventProcessed(uint32_t eventId);

        private:
            // Méthodes privées : pas d'export nécessaire (implémentation interne)
            void InternalCleanup();
            bool ValidateEventId(uint32_t eventId) const;

            // Conteneur des handlers (détail d'implémentation)
            std::unordered_map<uint32_t, std::vector<EventCallback>> m_handlers;

            // État interne
            bool m_initialized;
            uint64_t m_totalDispatched;
        };

    } // namespace event
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Structuration avec alignement et dépréciation (via NKPlatform)
// -----------------------------------------------------------------------------
/*
    #include "NKEvent/NkEventApi.h"

    // Structure de données événementielle alignée pour performances SIMD
    struct NKENTSEU_ALIGN_32 NKENTSEU_EVENT_CLASS_EXPORT EventPayload {
        uint64_t timestamp;      // Horodatage de l'événement
        uint32_t sourceId;       // Identifiant de la source
        uint32_t flags;          // Drapeaux de contrôle
        float data[8];           // Données payload alignées

        // Constructeur inline pour initialisation rapide
        NKENTSEU_EVENT_API_FORCE_INLINE EventPayload()
            : timestamp(0)
            , sourceId(0)
            , flags(0)
        {
            // Initialisation explicite du tableau pour éviter les valeurs indéfinies
            for (int i = 0; i < 8; ++i) {
                data[i] = 0.0f;
            }
        }
    };

    // Ancienne API dépréciée avec message de migration
    NKENTSEU_DEPRECATED_MESSAGE("Utiliser EventManager::Dispatch() avec EventPayload")
    NKENTSEU_EVENT_API void Legacy_DispatchEvent(uint32_t id, const char* raw);

    // Nouvelle API recommandée
    NKENTSEU_EVENT_API void EventManager_DispatchTyped(uint32_t id, const EventPayload& payload);
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Configuration des modes de build via CMake
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKEvent

    cmake_minimum_required(VERSION 3.15)
    project(NKEvent VERSION 1.0.0)

    # Options de build configurables
    option(NKEVENT_BUILD_SHARED "Build NKEvent as shared library" ON)
    option(NKEVENT_HEADER_ONLY "Use NKEvent in header-only mode" OFF)
    option(NKEVENT_ENABLE_DEBUG "Enable debug messages in NKEvent" OFF)

    # Configuration des defines de compilation
    if(NKEVENT_HEADER_ONLY)
        add_definitions(-DNKENTSEU_EVENT_HEADER_ONLY)
    elseif(NKEVENT_BUILD_SHARED)
        add_definitions(-DNKENTSEU_EVENT_BUILD_SHARED_LIB)
        set(NKEVENT_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_EVENT_STATIC_LIB)
        set(NKEVENT_LIBRARY_TYPE STATIC)
    endif()

    # Activation optionnelle du mode debug
    if(NKEVENT_ENABLE_DEBUG)
        add_definitions(-DNKENTSEU_EVENT_DEBUG)
    endif()

    # Création de la bibliothèque NKEvent
    add_library(nkevent ${NKEVENT_LIBRARY_TYPE}
        src/EventManager.cpp
        src/EventDispatcher.cpp
        src/EventQueue.cpp
        # ... autres fichiers sources du module
    )

    # Configuration des include directories publics
    target_include_directories(nkevent PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Liaison avec les dépendances (NKPlatform requis)
    target_link_libraries(nkevent PUBLIC NKPlatform::NKPlatform)

    # Installation des en-têtes publics pour distribution
    install(DIRECTORY include/NKEvent DESTINATION include)

    # Export de la cible pour les consommateurs via find_package()
    install(TARGETS nkevent EXPORT NKEventTargets)
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans une application cliente
// -----------------------------------------------------------------------------
/*
    // Dans l'application cliente (CMakeLists.txt) :

    find_package(NKEvent REQUIRED)

    add_executable(game_app main.cpp GameLoop.cpp)
    target_link_libraries(game_app PRIVATE NKEvent::NKEvent)

    # Gestion automatique des defines d'import/export via le package config
    # Définir manuellement uniquement si configuration non-standard :
    # target_compile_definitions(game_app PRIVATE NKENTSEU_EVENT_STATIC_LIB)

    // Dans GameLoop.cpp :
    #include <NKEvent/EventManager.h>

    void GameLoop::Initialize() {
        using namespace nkentseu::event;

        // Création du gestionnaire d'événements
        EventManager& eventMgr = EventManager::GetInstance();

        // Enregistrement des handlers pour les événements jeu
        eventMgr.RegisterHandler(EVENT_PLAYER_MOVE, [this](const void* data, size_t) {
            HandlePlayerMove(*static_cast<const PlayerMoveData*>(data));
        });

        eventMgr.RegisterHandler(EVENT_ENEMY_SPAWN, [this](const void* data, size_t) {
            SpawnEnemy(*static_cast<const EnemySpawnData*>(data));
        });

        // Initialisation du système
        if (!eventMgr.Initialize()) {
            NK_FOUNDATION_LOG_ERROR("Failed to initialize event system");
        }
    }

    void GameLoop::Update(float deltaTime) {
        // Dispatch d'un événement personnalisé
        PlayerMoveData moveData{m_playerPos, m_inputVector};
        EventManager::GetInstance().Dispatch(EVENT_PLAYER_MOVE, &moveData, sizeof(moveData));
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Mode header-only pour intégration légère
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKEvent en mode header-only (sans compilation séparée) :

    // 1. Définir la macro AVANT toute inclusion de NKEvent
    #define NKENTSEU_EVENT_HEADER_ONLY
    #include <NKEvent/NkEventApi.h>
    #include <NKEvent/EventManager.h>

    // 2. Toutes les fonctions marquées API_INLINE sont automatiquement inline
    // 3. Aucun linkage de bibliothèque nécessaire - idéal pour prototypes

    void QuickEventDemo() {
        namespace evt = nkentseu::event;

        // Utilisation directe sans gestion de mémoire manuelle
        evt::EventManager manager;  // Constructeur inline si header-only

        // Enregistrement simplifié avec lambda
        manager.RegisterHandler(1001, [](const void* data, size_t size) {
            // Traitement immédiat de l'événement
            NK_FOUNDATION_LOG_INFO("Event 1001 received");
        });

        // Dispatch avec données inline
        int payload = 42;
        manager.Dispatch(1001, &payload, sizeof(payload));
    }

    // Note importante :
    // Le mode header-only peut augmenter la taille du binaire final
    // car le code est instancié dans chaque unité de traduction.
    // À réserver aux petits projets ou aux bibliothèques purement template.
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison multi-modules (NKPlatform + NKCore + NKEvent)
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>   // Détection plateforme
    #include <NKPlatform/NkPlatformExport.h>   // Macros d'export base
    #include <NKCore/NkCoreApi.h>              // API Core
    #include <NKEvent/NkEventApi.h>            // API Event

    // Classe hybride utilisant plusieurs modules
    class NKENTSEU_CORE_CLASS_EXPORT HybridSystem {
    public:
        NKENTSEU_CORE_API HybridSystem()
            : m_eventMgr(nullptr)
            , m_initialized(false)
        {
        }

        NKENTSEU_CORE_API bool Initialize() {
            // Logging via NKPlatform
            NK_FOUNDATION_LOG_INFO("Initializing HybridSystem...");

            #if defined(NKENTSEU_EVENT_BUILD_SHARED_LIB)
                NK_FOUNDATION_LOG_DEBUG("NKEvent linked as shared library");
            #elif defined(NKENTSEU_EVENT_STATIC_LIB)
                NK_FOUNDATION_LOG_DEBUG("NKEvent linked as static library");
            #endif

            // Création du gestionnaire d'événements
            m_eventMgr = std::make_unique<nkentseu::event::EventManager>();

            // Enregistrement des callbacks système
            m_eventMgr->RegisterHandler(SYS_EVENT_SHUTDOWN,
                [this](const void*, size_t) { this->OnShutdownRequest(); });

            m_initialized = true;
            return true;
        }

        NKENTSEU_CORE_API void DispatchCustomEvent(uint32_t id, const void* data, size_t size) {
            if (m_initialized && m_eventMgr) {
                // Forward vers le système d'événements
                m_eventMgr->Dispatch(id, data, size);
            }
        }

        NKENTSEU_CORE_API_FORCE_INLINE bool IsReady() const {
            return m_initialized;
        }

    private:
        NKENTSEU_CORE_API void OnShutdownRequest() {
            NK_FOUNDATION_LOG_WARNING("Shutdown request received");
            // Traitement de la demande d'arrêt...
        }

        std::unique_ptr<nkentseu::event::EventManager> m_eventMgr;
        bool m_initialized;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Gestion des callbacks avec typage fort et sécurité
// -----------------------------------------------------------------------------
/*
    #include <NKEvent/NkEventApi.h>
    #include <memory>
    #include <type_traits>

    namespace nkentseu {
    namespace event {

        // Wrapper typé pour les callbacks événementiels
        template<typename EventType>
        class NKENTSEU_EVENT_CLASS_EXPORT TypedEventHandler {
        public:
            using CallbackType = std::function<void(const EventType&)>;

            NKENTSEU_EVENT_API TypedEventHandler(uint32_t eventId, CallbackType callback)
                : m_eventId(eventId)
                , m_callback(std::move(callback))
            {
            }

            NKENTSEU_EVENT_API void Invoke(const void* data, size_t size) {
                // Validation de sécurité basique
                if (data && size >= sizeof(EventType) && m_callback) {
                    const EventType& typedEvent = *static_cast<const EventType*>(data);
                    m_callback(typedEvent);
                }
            }

            NKENTSEU_EVENT_API_FORCE_INLINE uint32_t GetEventId() const {
                return m_eventId;
            }

            NKENTSEU_EVENT_API_INLINE bool IsValid() const {
                return m_callback != nullptr;
            }

        private:
            uint32_t m_eventId;
            CallbackType m_callback;
        };

        // Fonction helper pour création simplifiée
        template<typename EventType>
        NKENTSEU_EVENT_API_INLINE std::unique_ptr<TypedEventHandler<EventType>>
        MakeTypedHandler(uint32_t eventId, typename TypedEventHandler<EventType>::CallbackType cb) {
            return std::make_unique<TypedEventHandler<EventType>>(eventId, std::move(cb));
        }

    } // namespace event
    } // namespace nkentseu

    // Utilisation :
    // auto handler = MakeTypedHandler<PlayerJumpEvent>(
    //     EVENT_PLAYER_JUMP,
    //     [](const PlayerJumpEvent& e) {
    //         ProcessJump(e.playerId, e.jumpHeight);
    //     }
    // );
*/

// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. INDENTATION ET FORMATAGE :
       - Une instruction par ligne pour une meilleure lisibilité et diff
       - Indentation cohérente dans tous les blocs conditionnels (#if, #ifdef)
       - Indentation des sections public/private/protected dans les classes
       - Indentation des sous-namespaces pour hiérarchie visuelle claire

    2. ÉVITER LES DUPLICATIONS :
       - Ne jamais redéfinir les macros déjà présentes dans NKPlatform
       - Utiliser le préfixe NKENTSEU_* pour toutes les macros réutilisées
       - Centraliser la logique d'export dans les macros *_API principales

    3. DOCUMENTATION :
       - Commentaires Doxygen pour toutes les macros et fonctions publiques
       - Exemples d'utilisation concrets dans la section finale du fichier
       - Notes de migration pour les API dépréciées

    4. COMPATIBILITÉ :
       - Tester les 3 modes de build : shared, static, header-only
       - Vérifier la compatibilité croisée NKPlatform/NKCore/NKEvent
       - Utiliser NKENTSEU_PLATFORM_* pour les dépendances multiplateformes

    5. PERFORMANCE :
       - Marquer les fonctions fréquemment appelées avec _API_FORCE_INLINE
       - Utiliser NKENTSEU_ALIGN_* pour les structures critiques en performance
       - Éviter les exports inutiles sur les méthodes privées/protégées
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================