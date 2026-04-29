// =============================================================================
// NKNetwork/NkNetworkApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKNetwork.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform et NKCore (ZÉRO duplication)
//  - Macros spécifiques NKNetwork uniquement pour la configuration de build
//  - Indépendance totale des modes de build entre modules
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Règles d'application des macros :
//  - NKENTSEU_NETWORK_API sur les fonctions libres et variables globales publiques
//  - PAS sur les méthodes de classes (l'export de la classe suffit)
//  - PAS sur les classes/structs/templates (géré par NKENTSEU_NETWORK_CLASS_EXPORT)
//  - PAS sur les fonctions inline définies dans les headers
//
// Auteur : Rihen
// Date : 2024-2026
// License : Propriétaire - libre d'utilisation et de modification
// =============================================================================

#pragma once

#ifndef NKENTSEU_NETWORK_NKNETWORKAPI_H
#define NKENTSEU_NETWORK_NKNETWORKAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKNetwork dépend de NKCore et NKPlatform.
    // Nous importons leurs macros d'export pour assurer la cohérence.
    // AUCUNE duplication : nous utilisons directement les macros existantes.

    #include "NKCore/NkCoreApi.h"
    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"
    #include "NKMath/NKMath.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKNETWORK
    // -------------------------------------------------------------------------
    /**
     * @defgroup NetworkBuildConfig Configuration du Build NKNetwork
     * @brief Macros pour contrôler le mode de compilation de NKNetwork
     *
     * Ces macros sont INDÉPENDANTES de celles de NKCore et NKPlatform :
     *  - NKENTSEU_NETWORK_BUILD_SHARED_LIB : Compiler NKNetwork en DLL
     *  - NKENTSEU_NETWORK_STATIC_LIB : Utiliser NKNetwork en mode static
     *  - NKENTSEU_NETWORK_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKNetwork, NKCore et NKPlatform peuvent avoir des modes de build différents.
     *
     * @example CMakeLists.txt
     * @code
     * # NKNetwork en DLL
     * target_compile_definitions(nknetwork PRIVATE NKENTSEU_NETWORK_BUILD_SHARED_LIB)
     *
     * # NKNetwork en static
     * target_compile_definitions(monapp PRIVATE NKENTSEU_NETWORK_STATIC_LIB)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_NETWORK_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKNetwork
     * @def NKENTSEU_NETWORK_API
     * @ingroup NetworkApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKNetwork.
     * Elle est indépendante de NKENTSEU_CORE_API et NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_NETWORK_BUILD_SHARED_LIB : export (compilation en DLL)
     *  - NKENTSEU_NETWORK_STATIC_LIB ou NKENTSEU_NETWORK_HEADER_ONLY : vide
     *  - Sinon : import (utilisation en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base.
     */
    #if defined(NKENTSEU_NETWORK_BUILD_SHARED_LIB)
        // Compilation de NKNetwork en bibliothèque partagée : exporter les symboles
        #define NKENTSEU_NETWORK_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_NETWORK_STATIC_LIB) || defined(NKENTSEU_NETWORK_HEADER_ONLY)
        // Build statique ou header-only : aucune décoration de symbole nécessaire
        #define NKENTSEU_NETWORK_API
    #else
        // Utilisation de NKNetwork en mode DLL : importer les symboles
        #define NKENTSEU_NETWORK_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKNETWORK
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKNetwork sont définies.

    /**
     * @brief Macro pour exporter une classe complète de NKNetwork
     * @def NKENTSEU_NETWORK_CLASS_EXPORT
     * @ingroup NetworkApiMacros
     *
     * Alias vers NKENTSEU_NETWORK_API pour les déclarations de classe.
     * Les méthodes de la classe n'ont PAS besoin de NKENTSEU_NETWORK_API.
     *
     * @example
     * @code
     * class NKENTSEU_NETWORK_CLASS_EXPORT TcpClient {
     * public:
     *     bool Connect(const char* host, uint16_t port);  // Pas de macro ici
     * };
     * @endcode
     */
    #define NKENTSEU_NETWORK_CLASS_EXPORT NKENTSEU_NETWORK_API

    /**
     * @brief Fonction inline exportée pour NKNetwork
     * @def NKENTSEU_NETWORK_API_INLINE
     * @ingroup NetworkApiMacros
     *
     * Combinaison de NKENTSEU_NETWORK_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_NETWORK_HEADER_ONLY)
        // Mode header-only : forcer l'inlining pour éviter les erreurs de linkage
        #define NKENTSEU_NETWORK_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et inline hint
        #define NKENTSEU_NETWORK_API_INLINE NKENTSEU_NETWORK_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKNetwork
     * @def NKENTSEU_NETWORK_API_FORCE_INLINE
     * @ingroup NetworkApiMacros
     *
     * Combinaison de NKENTSEU_NETWORK_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     */
    #if defined(NKENTSEU_NETWORK_HEADER_ONLY)
        // Mode header-only : NKENTSEU_FORCE_INLINE suffit
        #define NKENTSEU_NETWORK_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        // Mode bibliothèque : combiner export et force inline
        #define NKENTSEU_NETWORK_API_FORCE_INLINE NKENTSEU_NETWORK_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKNetwork
     * @def NKENTSEU_NETWORK_API_NO_INLINE
     * @ingroup NetworkApiMacros
     *
     * Combinaison de NKENTSEU_NETWORK_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_NETWORK_API_NO_INLINE NKENTSEU_NETWORK_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup NetworkApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKNetwork
     *
     * Pour éviter la duplication, NKNetwork réutilise directement les macros
     * de NKPlatform. Voici les équivalences recommandées :
     *
     * | Besoin dans NKNetwork | Macro à utiliser | Source |
     * |----------------------|-----------------|--------|
     * | Dépréciation | `NKENTSEU_DEPRECATED` | NKPlatform |
     * | Dépréciation avec message | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement cache | `NKENTSEU_ALIGN_CACHE` | NKPlatform |
     * | Alignement 16/32/64 | `NKENTSEU_ALIGN_16`, etc. | NKPlatform |
     * | Liaison C | `NKENTSEU_EXTERN_C_BEGIN/END` | NKPlatform |
     * | Pragmas | `NKENTSEU_PRAGMA(x)` | NKPlatform |
     * | Gestion warnings | `NKENTSEU_DISABLE_WARNING_*` | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKNetwork :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser HttpClient::SendAsync()")
     * NKENTSEU_NETWORK_API void LegacyHttpRequest(const char* url);
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKNETWORK
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKNetwork.

    #if defined(NKENTSEU_NETWORK_BUILD_SHARED_LIB) && defined(NKENTSEU_NETWORK_STATIC_LIB)
        // Conflit : shared et static ne peuvent coexister
        #warning "NKNetwork: NKENTSEU_NETWORK_BUILD_SHARED_LIB et NKENTSEU_NETWORK_STATIC_LIB définis - NKENTSEU_NETWORK_STATIC_LIB ignoré"
        #undef NKENTSEU_NETWORK_STATIC_LIB
    #endif

    #if defined(NKENTSEU_NETWORK_BUILD_SHARED_LIB) && defined(NKENTSEU_NETWORK_HEADER_ONLY)
        // Conflit : shared et header-only sont mutuellement exclusifs
        #warning "NKNetwork: NKENTSEU_NETWORK_BUILD_SHARED_LIB et NKENTSEU_NETWORK_HEADER_ONLY définis - NKENTSEU_NETWORK_HEADER_ONLY ignoré"
        #undef NKENTSEU_NETWORK_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_NETWORK_STATIC_LIB) && defined(NKENTSEU_NETWORK_HEADER_ONLY)
        // Conflit : static et header-only ne peuvent coexister
        #warning "NKNetwork: NKENTSEU_NETWORK_STATIC_LIB et NKENTSEU_NETWORK_HEADER_ONLY définis - NKENTSEU_NETWORK_HEADER_ONLY ignoré"
        #undef NKENTSEU_NETWORK_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Affichage des informations de configuration au moment de la compilation.

    #ifdef NKENTSEU_NETWORK_DEBUG
        #pragma message("NKNetwork Export Config:")
        #if defined(NKENTSEU_NETWORK_BUILD_SHARED_LIB)
            #pragma message("  NKNetwork mode: Shared (export)")
        #elif defined(NKENTSEU_NETWORK_STATIC_LIB)
            #pragma message("  NKNetwork mode: Static")
        #elif defined(NKENTSEU_NETWORK_HEADER_ONLY)
            #pragma message("  NKNetwork mode: Header-only")
        #else
            #pragma message("  NKNetwork mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_NETWORK_API = " NKENTSEU_STRINGIZE(NKENTSEU_NETWORK_API))
    #endif

#endif // NKENTSEU_NETWORK_NKNETWORKAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKNETWORKAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKNetwork, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction publique NKNetwork (API C)
// -----------------------------------------------------------------------------
/*
    // Dans nknetwork_public.h :
    #include "NKNetwork/NkNetworkApi.h"

    NKENTSEU_NETWORK_EXTERN_C_BEGIN

    // Fonctions C publiques exportées avec liaison C
    NKENTSEU_NETWORK_API void Network_Initialize(void);
    NKENTSEU_NETWORK_API void Network_Shutdown(void);
    NKENTSEU_NETWORK_API int32 Network_GetLocalIP(char* buffer, int32 size);

    NKENTSEU_NETWORK_EXTERN_C_END
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe publique NKNetwork (API C++)
// -----------------------------------------------------------------------------
/*
    // Dans TcpClient.h :
    #include "NKNetwork/NkNetworkApi.h"
    #include "NKContainers/String/NkString.h"

    namespace nkentseu {
    namespace network {

        // Classe exportée : les méthodes n'ont PAS besoin de NKENTSEU_NETWORK_API
        class NKENTSEU_NETWORK_CLASS_EXPORT TcpClient {
        public:
            // Constructeurs/Destructeur : pas de macro nécessaire
            TcpClient();
            ~TcpClient();

            // Méthodes publiques : pas de macro
            bool Connect(const char* host, uint16_t port);
            bool Send(const void* data, size_t size);
            size_t Receive(void* buffer, size_t maxSize);
            void Disconnect();

            // Fonction inline dans la classe : pas de macro d'export
            NKENTSEU_FORCE_INLINE bool IsConnected() const {
                return m_connected;
            }

            // Getter avec macro force_inline si critique
            NKENTSEU_NETWORK_API_FORCE_INLINE uint16_t GetRemotePort() const {
                return m_remotePort;
            }

        protected:
            // Méthodes protégées : pas de macro non plus
            virtual void OnConnected();
            virtual void OnDisconnected();
            virtual void OnDataReceived(const void* data, size_t size);

        private:
            // Membres privés : pas de macro
            void* m_socketHandle;
            NkString m_remoteHost;
            uint16_t m_remotePort;
            bool m_connected;
        };

    } // namespace network
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Fonctions templates et inline (PAS de macro d'export)
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/NkNetworkApi.h"

    namespace nkentseu {
    namespace network {

        // Template : JAMAIS de NKENTSEU_NETWORK_API sur les templates
        template<typename PacketType>
        class PacketSerializer {
        public:
            PacketSerializer() = default;

            // Méthode template : pas de macro d'export
            template<typename BufferType>
            static bool Serialize(const PacketType& packet, BufferType& buffer) {
                // Logique de sérialisation...
                return true;
            }
        };

        // Fonction inline définie dans le header : pas de macro
        NKENTSEU_FORCE_INLINE bool IsValidPort(uint16_t port) {
            return port > 0 && port < 65536;
        }

        // Fonction avec macro d'export (non-inline, définie dans .cpp)
        NKENTSEU_NETWORK_API bool ResolveHostname(const char* hostname, NkString& outIP);

    } // namespace network
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des modes de build via CMake pour NKNetwork
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKNetwork

    cmake_minimum_required(VERSION 3.15)
    project(NKNetwork VERSION 1.0.0)

    # Options de build configurables
    option(NKNETWORK_BUILD_SHARED "Build NKNetwork as shared library" ON)
    option(NKNETWORK_HEADER_ONLY "Use NKNetwork in header-only mode" OFF)
    option(NKNETWORK_ENABLE_SSL "Enable SSL/TLS support" ON)
    option(NKNETWORK_ENABLE_WEBSOCKET "Enable WebSocket protocol" ON)

    # Configuration des defines de build
    if(NKNETWORK_HEADER_ONLY)
        add_definitions(-DNKENTSEU_NETWORK_HEADER_ONLY)
    elseif(NKNETWORK_BUILD_SHARED)
        add_definitions(-DNKENTSEU_NETWORK_BUILD_SHARED_LIB)
        set(NKNETWORK_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_NETWORK_STATIC_LIB)
        set(NKNETWORK_LIBRARY_TYPE STATIC)
    endif()

    # Options de fonctionnalités
    if(NKNETWORK_ENABLE_SSL)
        add_definitions(-DNKNETWORK_ENABLE_SSL)
        find_package(OpenSSL REQUIRED)
    endif()

    if(NKNETWORK_ENABLE_WEBSOCKET)
        add_definitions(-DNKNETWORK_ENABLE_WEBSOCKET)
    endif()

    # Dépendances requises
    find_package(NKCore REQUIRED)
    find_package(NKPlatform REQUIRED)
    find_package(NKContainers REQUIRED)

    # Création de la bibliothèque NKNetwork
    add_library(nknetwork ${NKNETWORK_LIBRARY_TYPE}
        src/Network.cpp
        src/TcpClient.cpp
        src/TcpServer.cpp
        src/UdpSocket.cpp
        src/Http/HttpClient.cpp
        src/Http/HttpServer.cpp
        src/WebSocket/WebSocket.cpp
    )

    # Liaison avec les dépendances
    target_link_libraries(nknetwork
        PUBLIC NKCore::NKCore
        PUBLIC NKPlatform::NKPlatform
        PUBLIC NKContainers::NKContainers
    )

    if(NKNETWORK_ENABLE_SSL)
        target_link_libraries(nknetwork PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()

    # Configuration des include directories
    target_include_directories(nknetwork PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Installation
    install(TARGETS nknetwork
        EXPORT NKNetworkTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans un fichier .cpp (avec pch.h)
// -----------------------------------------------------------------------------
/*
    // Dans TcpClient.cpp :

    #include "pch.h"                        // Precompiled header en premier
    #include "NKNetwork/NkNetworkApi.h"     // Remplace NkNetworkExport.h
    #include "NKNetwork/TcpClient.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKPlatform/NkSocket.h"

    namespace nkentseu {
    namespace network {

        // Implémentation : pas besoin de macros ici
        TcpClient::TcpClient()
            : m_socketHandle(nullptr)
            , m_remotePort(0)
            , m_connected(false)
        {
            // Initialisation...
        }

        TcpClient::~TcpClient() {
            Disconnect();
        }

        bool TcpClient::Connect(const char* host, uint16_t port) {
            if (!host || !IsValidPort(port)) {
                return false;
            }

            // Création du socket via NKPlatform
            m_socketHandle = PlatformSocket_CreateTcp();
            if (!m_socketHandle) {
                return false;
            }

            // Résolution et connexion
            if (!PlatformSocket_Connect(m_socketHandle, host, port)) {
                PlatformSocket_Destroy(m_socketHandle);
                m_socketHandle = nullptr;
                return false;
            }

            // Mise à jour de l'état
            m_remoteHost = host;
            m_remotePort = port;
            m_connected = true;

            // Callback de connexion
            OnConnected();

            return true;
        }

        bool TcpClient::Send(const void* data, size_t size) {
            if (!m_connected || !data || size == 0) {
                return false;
            }

            ssize_t sent = PlatformSocket_Send(m_socketHandle, data, size);
            return static_cast<size_t>(sent) == size;
        }

        size_t TcpClient::Receive(void* buffer, size_t maxSize) {
            if (!m_connected || !buffer || maxSize == 0) {
                return 0;
            }

            ssize_t received = PlatformSocket_Receive(m_socketHandle, buffer, maxSize);
            return (received > 0) ? static_cast<size_t>(received) : 0;
        }

        void TcpClient::Disconnect() {
            if (m_socketHandle) {
                PlatformSocket_Close(m_socketHandle);
                PlatformSocket_Destroy(m_socketHandle);
                m_socketHandle = nullptr;
            }

            if (m_connected) {
                m_connected = false;
                OnDisconnected();
            }
        }

        void TcpClient::OnConnected() {
            // Hook virtuel pour les sous-classes
            // Implémentation par défaut : vide
        }

        void TcpClient::OnDisconnected() {
            // Hook virtuel pour les sous-classes
            // Implémentation par défaut : vide
        }

        void TcpClient::OnDataReceived(const void* data, size_t size) {
            // Hook virtuel pour notification de données reçues
            // Implémentation par défaut : vide
        }

    } // namespace network
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Indentation stricte conforme aux standards NKEntseu
// -----------------------------------------------------------------------------
/*
    // Respect strict des règles d'indentation :

    namespace nkentseu {

        // Namespace imbriqué : indenté d'un niveau
        namespace network {

            // Classe : indentée d'un niveau supplémentaire
            class NKENTSEU_NETWORK_CLASS_EXPORT WebSocket {
            public:
                // Section public : indentée d'un niveau supplémentaire
                // Une instruction par ligne, pas de macro sur les méthodes
                WebSocket();
                ~WebSocket();

                bool Connect(const char* url);
                bool SendText(const char* message);
                bool SendBinary(const void* data, size_t size);
                void Close(uint16_t code = 1000);

            protected:
                // Section protected : même indentation que public
                virtual void OnOpen();
                virtual void OnMessage(const char* text);
                virtual void OnBinary(const void* data, size_t size);
                virtual void OnClose(uint16_t code, const char* reason);
                virtual void OnError(const char* error);

            private:
                // Section private : même indentation
                // Membres : une déclaration par ligne
                void* m_wsHandle;
                NkString m_url;
                bool m_isOpen;
                uint16_t m_lastCloseCode;
            };

        } // namespace network

        // Bloc conditionnel : contenu indenté
        #if defined(NKENTSEU_NETWORK_ENABLE_HTTP2)

            // Fonction d'export uniquement si HTTP/2 activé
            NKENTSEU_NETWORK_API void HttpClient_EnableHttp2(bool enabled);

        #endif

    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NKCore et NKPlatform
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>
    #include <NKPlatform/NkPlatformExport.h>
    #include <NKCore/NkCoreApi.h>
    #include <NKNetwork/NkNetworkApi.h>

    namespace nkentseu {
    namespace network {

        // Fonction utilisant les macros des trois modules
        NKENTSEU_NETWORK_API void Network_CrossModuleInit() {

            // Logging via NKCore/NKPlatform
            NK_FOUNDATION_LOG_INFO("[NKNetwork] Initializing network stack...");

            // Détection du mode de build NKNetwork
            #if defined(NKENTSEU_NETWORK_BUILD_SHARED_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKNetwork] Mode: Shared DLL");
            #elif defined(NKENTSEU_NETWORK_STATIC_LIB)
                NK_FOUNDATION_LOG_DEBUG("[NKNetwork] Mode: Static library");
            #endif

            // Code spécifique plateforme
            #ifdef NKENTSEU_PLATFORM_WINDOWS
                Network_InitWinSock();
            #elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
                Network_InitPosixSockets();
            #elif defined(NKENTSEU_PLATFORM_WEB)
                Network_InitEmscripten();
            #endif

            // Initialisation des sous-systèmes
            #if defined(NKNETWORK_ENABLE_SSL)
                Network_InitOpenSSL();
            #endif

            #if defined(NKNETWORK_ENABLE_WEBSOCKET)
                Network_InitWebSocketProtocol();
            #endif

            NK_FOUNDATION_LOG_INFO("[NKNetwork] Initialization complete");
        }

    } // namespace network
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Gestion des erreurs et logging cohérent
// -----------------------------------------------------------------------------
/*
    #include "NKNetwork/NkNetworkApi.h"
    #include "NKCore/Logger/NkLogger.h"

    namespace nkentseu {
    namespace network {

        // Fonction avec gestion d'erreur standardisée
        NKENTSEU_NETWORK_API bool Network_SendWithRetry(
            TcpClient* client,
            const void* data,
            size_t size,
            int maxRetries
        ) {
            if (!client || !data || size == 0) {
                NK_LOG_ERROR("[NKNetwork] Invalid parameters for SendWithRetry");
                return false;
            }

            int attempt = 0;
            while (attempt < maxRetries) {
                if (client->Send(data, size)) {
                    return true;
                }

                ++attempt;
                NK_LOG_WARN(
                    "[NKNetwork] Send attempt {}/{} failed, retrying...",
                    attempt,
                    maxRetries
                );

                // Backoff exponentiel simple
                PlatformThread_Sleep(10 * (1 << attempt));
            }

            NK_LOG_ERROR(
                "[NKNetwork] Send failed after {} attempts",
                maxRetries
            );
            return false;
        }

    } // namespace network
    } // namespace nkentseu
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Libre d'utilisation et de modification
// ============================================================

/*
NkNetwork.h — Déclaration des types fondamentaux (adresses, ports, protocoles)
NkSocketAddress.h/.cpp — Abstraction des adresses IP/ports cross-platform
NkTcpClient.h/.cpp — Client TCP avec API RAII
NkTcpServer.h/.cpp — Serveur TCP avec gestion de connexions multiples
NkUdpSocket.h/.cpp — Socket UDP pour communications sans connexion
NkHttpClient.h/.cpp — Client HTTP/HTTPS avec support async
NkWebSocket.h/.cpp — Support WebSocket pour communications bidirectionnelles
NkNetworkUtils.h/.cpp — Utilitaires : résolution DNS, validation, helpers
*/