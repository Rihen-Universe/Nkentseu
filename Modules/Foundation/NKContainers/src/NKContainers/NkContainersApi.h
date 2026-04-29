// =============================================================================
// NKContainers/NkContainersApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKContainers.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKContainers uniquement pour la configuration de build NKContainers
//  - Indépendance totale des modes de build : NKContainers et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_NKCONTAINERSAPI_H
#define NKENTSEU_CONTAINERS_NKCONTAINERSAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKContainers dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKCONTAINERS
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainersBuildConfig Configuration du Build NKContainers
     * @brief Macros pour contrôler le mode de compilation de NKContainers
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_CONTAINERS_BUILD_SHARED_LIB : Compiler NKContainers en bibliothèque partagée
     *  - NKENTSEU_CONTAINERS_STATIC_LIB : Utiliser NKContainers en mode bibliothèque statique
     *  - NKENTSEU_CONTAINERS_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKContainers et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform en DLL + NKContainers en static.
     *
     * @example CMakeLists.txt
     * @code
     * # NKContainers en DLL, NKPlatform en static
     * target_compile_definitions(nkcontainers PRIVATE NKENTSEU_CONTAINERS_BUILD_SHARED_LIB)
     * target_compile_definitions(nkcontainers PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform
     *
     * # NKContainers en static, NKPlatform en DLL
     * target_compile_definitions(monapp PRIVATE NKENTSEU_CONTAINERS_STATIC_LIB)
     * # (NKPlatform en DLL par défaut, pas de define nécessaire)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_CONTAINERS_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKContainers
     * @def NKENTSEU_CONTAINERS_API
     * @ingroup ContainersApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKContainers.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_CONTAINERS_BUILD_SHARED_LIB : export (compilation de NKContainers en DLL)
     *  - NKENTSEU_CONTAINERS_STATIC_LIB ou NKENTSEU_CONTAINERS_HEADER_ONLY : vide (pas d'export)
     *  - Sinon : import (utilisation de NKContainers en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication.
     *
     * @example
     * @code
     * // Dans un header public de NKContainers :
     * NKENTSEU_CONTAINERS_API void ContainersInitialize();
     *
     * // Pour compiler NKContainers en DLL : -DNKENTSEU_CONTAINERS_BUILD_SHARED_LIB
     * // Pour utiliser NKContainers en DLL : (aucun define, import par défaut)
     * // Pour utiliser NKContainers en static : -DNKENTSEU_CONTAINERS_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_CONTAINERS_BUILD_SHARED_LIB)
        // Compilation de NKContainers en bibliothèque partagée : exporter
        #define NKENTSEU_CONTAINERS_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_CONTAINERS_STATIC_LIB) || defined(NKENTSEU_CONTAINERS_HEADER_ONLY)
        // Build statique ou header-only : pas de décoration
        #define NKENTSEU_CONTAINERS_API
    #else
        // Utilisation de NKContainers en mode DLL : importer
        #define NKENTSEU_CONTAINERS_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKCONTAINERS
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKContainers sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKContainers
     * @def NKENTSEU_CONTAINERS_CLASS_EXPORT
     * @ingroup ContainersApiMacros
     *
     * Alias vers NKENTSEU_CONTAINERS_API pour les déclarations de classe.
     *
     * @example
     * @code
     * class NKENTSEU_CONTAINERS_CLASS_EXPORT Vector {
     * public:
     *     void PushBack(int value);
     * };
     * @endcode
     */
    #define NKENTSEU_CONTAINERS_CLASS_EXPORT NKENTSEU_CONTAINERS_API

    /**
     * @brief Fonction inline exportée pour NKContainers
     * @def NKENTSEU_CONTAINERS_API_INLINE
     * @ingroup ContainersApiMacros
     *
     * Combinaison de NKENTSEU_CONTAINERS_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_CONTAINERS_HEADER_ONLY)
        #define NKENTSEU_CONTAINERS_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_CONTAINERS_API_INLINE NKENTSEU_CONTAINERS_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKContainers
     * @def NKENTSEU_CONTAINERS_API_FORCE_INLINE
     * @ingroup ContainersApiMacros
     *
     * Combinaison de NKENTSEU_CONTAINERS_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     * Particulièrement utile pour les conteneurs où l'inlining est essentiel.
     */
    #if defined(NKENTSEU_CONTAINERS_HEADER_ONLY)
        #define NKENTSEU_CONTAINERS_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_CONTAINERS_API_FORCE_INLINE NKENTSEU_CONTAINERS_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKContainers
     * @def NKENTSEU_CONTAINERS_API_NO_INLINE
     * @ingroup ContainersApiMacros
     *
     * Combinaison de NKENTSEU_CONTAINERS_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging.
     */
    #define NKENTSEU_CONTAINERS_API_NO_INLINE NKENTSEU_CONTAINERS_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainersApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKContainers
     *
     * Pour éviter la duplication, NKContainers réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKContainers | Macro à utiliser | Source |
     * |-------------------------|-----------------|--------|
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
     * // Dépréciation dans NKContainers (pas de NKENTSEU_CONTAINERS_DEPRECATED) :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser Vector::EmplaceBack()")
     * NKENTSEU_CONTAINERS_API void LegacyPushBack(int value);
     *
     * // Alignement dans NKContainers (pas de NKENTSEU_CONTAINERS_ALIGN_32) :
     * struct NKENTSEU_ALIGN_32 NKENTSEU_CONTAINERS_CLASS_EXPORT SimdVector {
     *     float data[8];  // 32 bytes, aligné pour SIMD
     * };
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKCONTAINERS
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKContainers.

    #if defined(NKENTSEU_CONTAINERS_BUILD_SHARED_LIB) && defined(NKENTSEU_CONTAINERS_STATIC_LIB)
        #warning "NKContainers: NKENTSEU_CONTAINERS_BUILD_SHARED_LIB et NKENTSEU_CONTAINERS_STATIC_LIB définis - NKENTSEU_CONTAINERS_STATIC_LIB ignoré"
        #undef NKENTSEU_CONTAINERS_STATIC_LIB
    #endif

    #if defined(NKENTSEU_CONTAINERS_BUILD_SHARED_LIB) && defined(NKENTSEU_CONTAINERS_HEADER_ONLY)
        #warning "NKContainers: NKENTSEU_CONTAINERS_BUILD_SHARED_LIB et NKENTSEU_CONTAINERS_HEADER_ONLY définis - NKENTSEU_CONTAINERS_HEADER_ONLY ignoré"
        #undef NKENTSEU_CONTAINERS_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_CONTAINERS_STATIC_LIB) && defined(NKENTSEU_CONTAINERS_HEADER_ONLY)
        #warning "NKContainers: NKENTSEU_CONTAINERS_STATIC_LIB et NKENTSEU_CONTAINERS_HEADER_ONLY définis - NKENTSEU_CONTAINERS_HEADER_ONLY ignoré"
        #undef NKENTSEU_CONTAINERS_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CONTAINERS_DEBUG
        #pragma message("NKContainers Export Config:")
        #if defined(NKENTSEU_CONTAINERS_BUILD_SHARED_LIB)
            #pragma message("  NKContainers mode: Shared (export)")
        #elif defined(NKENTSEU_CONTAINERS_STATIC_LIB)
            #pragma message("  NKContainers mode: Static")
        #elif defined(NKENTSEU_CONTAINERS_HEADER_ONLY)
            #pragma message("  NKContainers mode: Header-only")
        #else
            #pragma message("  NKContainers mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_CONTAINERS_API = " NKENTSEU_STRINGIZE(NKENTSEU_CONTAINERS_API))
    #endif

#endif // NKENTSEU_CONTAINERS_NKCONTAINERSAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Déclaration d'une fonction publique NKContainers
    #include "NKContainers/NkContainersApi.h"

    NKENTSEU_CONTAINERS_EXTERN_C_BEGIN
    NKENTSEU_CONTAINERS_API void Containers_Initialize(void);
    NKENTSEU_CONTAINERS_API void Containers_Shutdown(void);
    NKENTSEU_CONTAINERS_EXTERN_C_END

    // Exemple 2 : Déclaration d'une classe template avec dépréciation (via NKPlatform)
    #include "NKContainers/NkContainersApi.h"

    template<typename T>
    class NKENTSEU_CONTAINERS_CLASS_EXPORT Vector {
    public:
        Vector();
        ~Vector();

        // Dépréciation via macro NKPlatform (pas de duplication)
        NKENTSEU_DEPRECATED_MESSAGE("Utiliser EmplaceBack(Args&&...)")
        NKENTSEU_CONTAINERS_API void PushBack(const T& value);

        NKENTSEU_CONTAINERS_API_FORCE_INLINE T& operator[](size_t index) {
            return m_data[index];
        }

        NKENTSEU_CONTAINERS_API_FORCE_INLINE size_t Size() const {
            return m_size;
        }

    private:
        T* m_data;
        size_t m_size;
        size_t m_capacity;
    };

    // Exemple 3 : Alignement via macro NKPlatform (pas de duplication)
    struct NKENTSEU_ALIGN_64 NKENTSEU_CONTAINERS_CLASS_EXPORT CacheAlignedBuffer {
        char data[256];  // Aligné pour optimisation cache L1/L2
    };

    // Exemple 4 : Modes de build indépendants (CMake)
    // NKPlatform en DLL, NKContainers en static :
    //   target_compile_definitions(nkcontainers PRIVATE NKENTSEU_CONTAINERS_STATIC_LIB)
    //   target_compile_definitions(nkcontainers PRIVATE NKENTSEU_BUILD_SHARED_LIB)  # Pour NKPlatform
    //
    // NKPlatform en static, NKContainers en DLL :
    //   target_compile_definitions(nkcontainers PRIVATE NKENTSEU_CONTAINERS_BUILD_SHARED_LIB)
    //   target_compile_definitions(nkcontainers PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform

    // Exemple 5 : Utilisation dans le code client
    #include <NKContainers/Vector.h>

    int main() {
        nkentseu::containers::Vector<int> vec;
        vec.PushBack(42);
        vec.PushBack(1337);
        return 0;
    }
*/

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCONTAINERSAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKContainers, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une fonction C publique NKContainers
// -----------------------------------------------------------------------------
/*
    // Dans nkcontainers_public.h :
    #include "NKContainers/NkContainersApi.h"

    NKENTSEU_CONTAINERS_EXTERN_C_BEGIN

    // Fonction C publique exportée
    NKENTSEU_CONTAINERS_API void Containers_Initialize(void);
    NKENTSEU_CONTAINERS_API void Containers_Shutdown(void);
    NKENTSEU_CONTAINERS_API int Containers_GetVersion(void);

    NKENTSEU_CONTAINERS_EXTERN_C_END

    // Dans nkcontainers.cpp :
    #include "nkcontainers_public.h"

    void Containers_Initialize(void) {
        // Implémentation...
    }

    void Containers_Shutdown(void) {
        // Implémentation...
    }

    int Containers_GetVersion(void) {
        return 0x010200;  // Version 1.2.0
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'une classe template publique NKContainers
// -----------------------------------------------------------------------------
/*
    // Dans Vector.h :
    #include "NKContainers/NkContainersApi.h"

    template<typename T, typename Allocator = DefaultAllocator>
    class NKENTSEU_CONTAINERS_CLASS_EXPORT Vector {
    public:
        NKENTSEU_CONTAINERS_API Vector();
        NKENTSEU_CONTAINERS_API explicit Vector(size_t capacity);
        NKENTSEU_CONTAINERS_API ~Vector();

        // Copy/Move semantics
        NKENTSEU_CONTAINERS_API Vector(const Vector& other);
        NKENTSEU_CONTAINERS_API Vector(Vector&& other) noexcept;
        NKENTSEU_CONTAINERS_API Vector& operator=(const Vector& other);
        NKENTSEU_CONTAINERS_API Vector& operator=(Vector&& other) noexcept;

        // Element access
        NKENTSEU_CONTAINERS_API_FORCE_INLINE T& operator[](size_t index) {
            return m_data[index];
        }
        NKENTSEU_CONTAINERS_API_FORCE_INLINE const T& operator[](size_t index) const {
            return m_data[index];
        }

        // Capacity
        NKENTSEU_CONTAINERS_API_FORCE_INLINE size_t Size() const { return m_size; }
        NKENTSEU_CONTAINERS_API_FORCE_INLINE size_t Capacity() const { return m_capacity; }
        NKENTSEU_CONTAINERS_API_FORCE_INLINE bool Empty() const { return m_size == 0; }

        // Modifiers
        NKENTSEU_CONTAINERS_API void PushBack(const T& value);
        NKENTSEU_CONTAINERS_API void PushBack(T&& value);
        NKENTSEU_CONTAINERS_API void PopBack();
        NKENTSEU_CONTAINERS_API void Clear();
        NKENTSEU_CONTAINERS_API void Reserve(size_t newCapacity);

        // Iterators (inline pour performance)
        NKENTSEU_CONTAINERS_API_FORCE_INLINE T* Begin() { return m_data; }
        NKENTSEU_CONTAINERS_API_FORCE_INLINE T* End() { return m_data + m_size; }
        NKENTSEU_CONTAINERS_API_FORCE_INLINE const T* Begin() const { return m_data; }
        NKENTSEU_CONTAINERS_API_FORCE_INLINE const T* End() const { return m_data + m_size; }

    private:
        T* m_data;
        size_t m_size;
        size_t m_capacity;
        Allocator m_allocator;
    };

    // Implémentation dans Vector.inl (si header-only) ou Vector.cpp
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des modes de build via CMake
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKContainers

    cmake_minimum_required(VERSION 3.15)
    project(NKContainers VERSION 1.2.0)

    # Options de build
    option(NKCONTAINERS_BUILD_SHARED "Build NKContainers as shared library" ON)
    option(NKCONTAINERS_HEADER_ONLY "Use NKContainers in header-only mode" OFF)

    # Configuration des defines
    if(NKCONTAINERS_HEADER_ONLY)
        add_definitions(-DNKENTSEU_CONTAINERS_HEADER_ONLY)
    elseif(NKCONTAINERS_BUILD_SHARED)
        add_definitions(-DNKENTSEU_CONTAINERS_BUILD_SHARED_LIB)
        set(NKCONTAINERS_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_CONTAINERS_STATIC_LIB)
        set(NKCONTAINERS_LIBRARY_TYPE STATIC)
    endif()

    # Création de la bibliothèque
    add_library(nkcontainers ${NKCONTAINERS_LIBRARY_TYPE}
        src/Vector.cpp
        src/HashMap.cpp
        src/String.cpp
        # ... autres fichiers sources
    )

    # Installation des en-têtes publics
    install(DIRECTORY include/NKContainers DESTINATION include)

    # Pour les consommateurs de NKContainers
    target_include_directories(nkcontainers PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Link avec NKPlatform (requis)
    target_link_libraries(nkcontainers PUBLIC NKPlatform::NKPlatform)
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de NKContainers dans une application cliente
// -----------------------------------------------------------------------------
/*
    // Dans l'application cliente (CMakeLists.txt) :

    find_package(NKContainers REQUIRED)

    add_executable(monapp main.cpp)
    target_link_libraries(monapp PRIVATE NKContainers::NKContainers)

    # Si NKContainers est en mode DLL, le define d'import est géré par le package
    # Sinon, définir manuellement si nécessaire :
    # target_compile_definitions(monapp PRIVATE NKENTSEU_CONTAINERS_STATIC_LIB)

    // Dans main.cpp :
    #include <NKContainers/Vector.h>
    #include <NKContainers/HashMap.h>

    int main() {
        nkentseu::containers::Vector<int> numbers;
        numbers.PushBack(1);
        numbers.PushBack(2);
        numbers.PushBack(3);

        nkentseu::containers::HashMap<nkentseu::String, int> scores;
        scores.Insert("player1", 100);
        scores.Insert("player2", 250);

        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Mode header-only pour NKContainers
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKContainers en mode header-only :

    // 1. Définir la macro avant toute inclusion
    #define NKENTSEU_CONTAINERS_HEADER_ONLY
    #include <NKContainers/NkContainersApi.h>
    #include <NKContainers/Vector.h>
    #include <NKContainers/HashMap.h>

    // 2. Toutes les fonctions sont inline, pas de linkage nécessaire
    // 3. Idéal pour les petits projets ou l'intégration directe

    void QuickStart() {
        nkentseu::containers::Vector<int> vec;  // Tout est inline
        vec.PushBack(42);
        // ... utilisation ...
    }

    // Note : Le mode header-only peut augmenter la taille du binaire
    // car le code est dupliqué dans chaque unité de traduction.
    // Pour les templates, c'est souvent le mode recommandé.
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Dépréciation et migration d'API NKContainers
// -----------------------------------------------------------------------------
/*
    #include <NKContainers/NkContainersApi.h>

    // Ancienne API dépréciée
    NKENTSEU_DEPRECATED_MESSAGE("Utiliser Vector::EmplaceBack(Args&&...)")
    NKENTSEU_CONTAINERS_API void LegacyPushBack(int value);

    // Nouvelle API recommandée (template)
    template<typename... Args>
    NKENTSEU_CONTAINERS_API_FORCE_INLINE void Vector::EmplaceBack(Args&&... args) {
        // Implémentation optimisée...
    }

    // Dans le code client :
    void MigrateCode() {
        nkentseu::containers::Vector<int> vec;

        // Ceci génère un warning de dépréciation :
        // LegacyPushBack(42);

        // Utiliser la nouvelle API :
        vec.EmplaceBack(42);  // Construction in-place, plus efficace
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NKPlatform et NKCore pour code multi-module
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>      // NKENTSEU_PLATFORM_*
    #include <NKPlatform/NkPlatformExport.h>      // NKENTSEU_PLATFORM_API
    #include <NKCore/NkCoreApi.h>                 // NKENTSEU_CORE_API
    #include <NKContainers/NkContainersApi.h>     // NKENTSEU_CONTAINERS_API

    // Fonction qui utilise les trois modules
    NKENTSEU_CONTAINERS_API void MultiModuleInit() {
        // Logging via NKPlatform
        NK_FOUNDATION_LOG_INFO("Initializing multi-module containers...");

        // Configuration via NKCore
        #if defined(NKENTSEU_CONTAINERS_BUILD_SHARED_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKContainers mode: Shared DLL");
        #elif defined(NKENTSEU_CONTAINERS_STATIC_LIB)
            NK_FOUNDATION_LOG_DEBUG("NKContainers mode: Static library");
        #endif

        // Allocation via NKCore + conteneurs NKContainers
        auto* buffer = nkentseu::core::AllocateAligned(64, 256);
        nkentseu::containers::Vector<nkentseu::core::Handle> handles;
        handles.Reserve(16);

        // Code spécifique à la plateforme
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            InitializeWindowsContainers();
        #elif defined(NKENTSEU_PLATFORM_LINUX)
            InitializeLinuxContainers();
        #endif
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================