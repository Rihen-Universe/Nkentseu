// =============================================================================
// NKMemory/NkMemoryApi.h
// Gestion de la visibilité des symboles et de l'API publique du module NKMemory.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Macros spécifiques NKMemory uniquement pour la configuration de build NKMemory
//  - Indépendance totale des modes de build : NKMemory et NKPlatform peuvent
//    avoir des configurations différentes (static vs shared)
//  - Compatibilité multiplateforme et multi-compilateur via NKPlatform
//  - Aucune redéfinition de deprecated, alignement, pragma, etc.
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_MEMORY_NKMEMORYAPI_H
#define NKENTSEU_MEMORY_NKMEMORYAPI_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKMemory dépend de NKPlatform. Nous importons ses macros d'export.
    // AUCUNE duplication : nous utilisons directement les macros NKPlatform.

    #include "NKPlatform/NkPlatformExport.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : CONFIGURATION DU MODE DE BUILD NKMEMORY
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryBuildConfig Configuration du Build NKMemory
     * @brief Macros pour contrôler le mode de compilation de NKMemory
     *
     * Ces macros sont INDÉPENDANTES de celles de NKPlatform :
     *  - NKENTSEU_MEMORY_BUILD_SHARED_LIB : Compiler NKMemory en bibliothèque partagée
     *  - NKENTSEU_MEMORY_STATIC_LIB : Utiliser NKMemory en mode bibliothèque statique
     *  - NKENTSEU_MEMORY_HEADER_ONLY : Mode header-only (tout inline)
     *
     * @note NKMemory et NKPlatform peuvent avoir des modes de build différents.
     * Exemple valide : NKPlatform en DLL + NKMemory en static.
     *
     * @example CMakeLists.txt
     * @code
     * # NKMemory en DLL, NKPlatform en static
     * target_compile_definitions(nkmemory PRIVATE NKENTSEU_MEMORY_BUILD_SHARED_LIB)
     * target_compile_definitions(nkmemory PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform
     *
     * # NKMemory en static, NKPlatform en DLL
     * target_compile_definitions(monapp PRIVATE NKENTSEU_MEMORY_STATIC_LIB)
     * # (NKPlatform en DLL par défaut, pas de define nécessaire)
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 3 : MACRO PRINCIPALE NKENTSEU_MEMORY_API
    // -------------------------------------------------------------------------
    /**
     * @brief Macro principale pour l'export/import des symboles NKMemory
     * @def NKENTSEU_MEMORY_API
     * @ingroup MemoryApiMacros
     *
     * Cette macro gère UNIQUEMENT la visibilité des symboles de NKMemory.
     * Elle est indépendante de NKENTSEU_PLATFORM_API.
     *
     * Logique :
     *  - NKENTSEU_MEMORY_BUILD_SHARED_LIB : export (compilation de NKMemory en DLL)
     *  - NKENTSEU_MEMORY_STATIC_LIB ou NKENTSEU_MEMORY_HEADER_ONLY : vide (pas d'export)
     *  - Sinon : import (utilisation de NKMemory en mode DLL)
     *
     * @note Cette macro utilise NKENTSEU_PLATFORM_API_EXPORT/IMPORT comme base,
     * garantissant la compatibilité multiplateforme sans duplication.
     *
     * @example
     * @code
     * // Dans un header public de NKMemory :
     * NKENTSEU_MEMORY_API void* Memory_Allocate(size_t size);
     *
     * // Pour compiler NKMemory en DLL : -DNKENTSEU_MEMORY_BUILD_SHARED_LIB
     * // Pour utiliser NKMemory en DLL : (aucun define, import par défaut)
     * // Pour utiliser NKMemory en static : -DNKENTSEU_MEMORY_STATIC_LIB
     * @endcode
     */
    #if defined(NKENTSEU_MEMORY_BUILD_SHARED_LIB)
        // Compilation de NKMemory en bibliothèque partagée : exporter
        #define NKENTSEU_MEMORY_API NKENTSEU_PLATFORM_API_EXPORT
    #elif defined(NKENTSEU_MEMORY_STATIC_LIB) || defined(NKENTSEU_MEMORY_HEADER_ONLY)
        // Build statique ou header-only : pas de décoration
        #define NKENTSEU_MEMORY_API
    #else
        // Utilisation de NKMemory en mode DLL : importer
        #define NKENTSEU_MEMORY_API NKENTSEU_PLATFORM_API_IMPORT
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : MACROS DE CONVENANCE SPÉCIFIQUES À NKMEMORY
    // -------------------------------------------------------------------------
    // Seules les macros qui ajoutent une valeur spécifique à NKMemory sont définies.
    // Tout le reste (deprecated, alignement, pragma, etc.) est utilisé directement
    // depuis NKPlatform via le préfixe NKENTSEU_*.

    /**
     * @brief Macro pour exporter une classe complète de NKMemory
     * @def NKENTSEU_MEMORY_CLASS_EXPORT
     * @ingroup MemoryApiMacros
     *
     * Alias vers NKENTSEU_MEMORY_API pour les déclarations de classe.
     *
     * @example
     * @code
     * class NKENTSEU_MEMORY_CLASS_EXPORT MemoryPool {
     * public:
     *     NKENTSEU_MEMORY_API void* Allocate(size_t size);
     * };
     * @endcode
     */
    #define NKENTSEU_MEMORY_CLASS_EXPORT NKENTSEU_MEMORY_API

    /**
     * @brief Fonction inline exportée pour NKMemory
     * @def NKENTSEU_MEMORY_API_INLINE
     * @ingroup MemoryApiMacros
     *
     * Combinaison de NKENTSEU_MEMORY_API et NKENTSEU_INLINE.
     * En mode header-only, équivaut à NKENTSEU_FORCE_INLINE.
     */
    #if defined(NKENTSEU_MEMORY_HEADER_ONLY)
        #define NKENTSEU_MEMORY_API_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_MEMORY_API_INLINE NKENTSEU_MEMORY_API NKENTSEU_INLINE
    #endif

    /**
     * @brief Fonction force_inline exportée pour NKMemory
     * @def NKENTSEU_MEMORY_API_FORCE_INLINE
     * @ingroup MemoryApiMacros
     *
     * Combinaison de NKENTSEU_MEMORY_API et NKENTSEU_FORCE_INLINE.
     * Pour les fonctions critiques en performance dans l'API publique.
     * Idéal pour les allocateurs rapides ou les accès mémoire hot-path.
     */
    #if defined(NKENTSEU_MEMORY_HEADER_ONLY)
        #define NKENTSEU_MEMORY_API_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #else
        #define NKENTSEU_MEMORY_API_FORCE_INLINE NKENTSEU_MEMORY_API NKENTSEU_FORCE_INLINE
    #endif

    /**
     * @brief Fonction no_inline exportée pour NKMemory
     * @def NKENTSEU_MEMORY_API_NO_INLINE
     * @ingroup MemoryApiMacros
     *
     * Combinaison de NKENTSEU_MEMORY_API et NKENTSEU_NO_INLINE.
     * Pour éviter l'explosion de code dans les DLL ou faciliter le debugging
     * des fonctions d'allocation complexes.
     */
    #define NKENTSEU_MEMORY_API_NO_INLINE NKENTSEU_MEMORY_API NKENTSEU_NO_INLINE

    // -------------------------------------------------------------------------
    // SECTION 5 : UTILISATION DIRECTE DES MACROS NKPLATFORM (PAS DE DUPLICATION)
    // -------------------------------------------------------------------------
    /**
     * @defgroup MemoryApiReuse Réutilisation des Macros NKPlatform
     * @brief Guide d'utilisation des macros NKPlatform dans NKMemory
     *
     * Pour éviter la duplication, NKMemory réutilise directement les macros
     * de NKPlatform. Voici les équivalences :
     *
     * | Besoin dans NKMemory | Macro à utiliser | Source |
     * |---------------------|-----------------|--------|
     * | Dépréciation | `NKENTSEU_DEPRECATED` | NKPlatform |
     * | Dépréciation avec message | `NKENTSEU_DEPRECATED_MESSAGE(msg)` | NKPlatform |
     * | Alignement cache | `NKENTSEU_ALIGN_CACHE` | NKPlatform |
     * | Alignement 16/32/64 | `NKENTSEU_ALIGN_16`, etc. | NKPlatform |
     * | Liaison C | `NKENTSEU_EXTERN_C_BEGIN/END` | NKPlatform |
     * | Pragmas | `NKENTSEU_PRAGMA(x)` | NKPlatform |
     * | Gestion warnings | `NKENTSEU_DISABLE_WARNING_*` | NKPlatform |
     * | Symbole local | `NKENTSEU_API_LOCAL` | NKPlatform |
     * | Restreint (restrict) | `NKENTSEU_RESTRICT` | NKPlatform |
     * | Probable branch | `NKENTSEU_LIKELY/UNLIKELY` | NKPlatform |
     *
     * @example
     * @code
     * // Dépréciation dans NKMemory (pas de NKENTSEU_MEMORY_DEPRECATED) :
     * NKENTSEU_DEPRECATED_MESSAGE("Utiliser Memory_AllocateAligned()")
     * NKENTSEU_MEMORY_API void* Memory_Allocate(size_t size);
     *
     * // Alignement dans NKMemory (pas de NKENTSEU_MEMORY_ALIGN_64) :
     * struct NKENTSEU_ALIGN_64 NKENTSEU_MEMORY_CLASS_EXPORT CacheLineBuffer {
     *     uint8_t data[64];
     * };
     *
     * // Optimisation de branchement pour allocateurs rapides :
     * NKENTSEU_MEMORY_API_FORCE_INLINE void* FastAllocate(size_t size) {
     *     if (NKENTSEU_LIKELY(size < SMALL_ALLOC_THRESHOLD)) {
     *         return m_smallPool.Allocate(size);
     *     }
     *     return Memory_Allocate(size);  // Slow path
     * }
     * @endcode
     */

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION DES MACROS DE BUILD NKMEMORY
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour les macros spécifiques à NKMemory.

    #if defined(NKENTSEU_MEMORY_BUILD_SHARED_LIB) && defined(NKENTSEU_MEMORY_STATIC_LIB)
        #warning "NKMemory: NKENTSEU_MEMORY_BUILD_SHARED_LIB et NKENTSEU_MEMORY_STATIC_LIB définis - NKENTSEU_MEMORY_STATIC_LIB ignoré"
        #undef NKENTSEU_MEMORY_STATIC_LIB
    #endif

    #if defined(NKENTSEU_MEMORY_BUILD_SHARED_LIB) && defined(NKENTSEU_MEMORY_HEADER_ONLY)
        #warning "NKMemory: NKENTSEU_MEMORY_BUILD_SHARED_LIB et NKENTSEU_MEMORY_HEADER_ONLY définis - NKENTSEU_MEMORY_HEADER_ONLY ignoré"
        #undef NKENTSEU_MEMORY_HEADER_ONLY
    #endif

    #if defined(NKENTSEU_MEMORY_STATIC_LIB) && defined(NKENTSEU_MEMORY_HEADER_ONLY)
        #warning "NKMemory: NKENTSEU_MEMORY_STATIC_LIB et NKENTSEU_MEMORY_HEADER_ONLY définis - NKENTSEU_MEMORY_HEADER_ONLY ignoré"
        #undef NKENTSEU_MEMORY_HEADER_ONLY
    #endif

    // -------------------------------------------------------------------------
    // SECTION 7 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_MEMORY_DEBUG
        #pragma message("NKMemory Export Config:")
        #if defined(NKENTSEU_MEMORY_BUILD_SHARED_LIB)
            #pragma message("  NKMemory mode: Shared (export)")
        #elif defined(NKENTSEU_MEMORY_STATIC_LIB)
            #pragma message("  NKMemory mode: Static")
        #elif defined(NKENTSEU_MEMORY_HEADER_ONLY)
            #pragma message("  NKMemory mode: Header-only")
        #else
            #pragma message("  NKMemory mode: DLL import (default)")
        #endif
        #pragma message("  NKENTSEU_MEMORY_API = " NKENTSEU_STRINGIZE(NKENTSEU_MEMORY_API))
    #endif

#endif // NKENTSEU_MEMORY_NKMEMORYAPI_H

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Déclaration d'une fonction C publique NKMemory
    #include "NKMemory/NkMemoryApi.h"

    NKENTSEU_MEMORY_EXTERN_C_BEGIN

    NKENTSEU_MEMORY_API void* Memory_Allocate(size_t size);
    NKENTSEU_MEMORY_API void  Memory_Free(void* ptr);
    NKENTSEU_MEMORY_API void* Memory_Reallocate(void* ptr, size_t newSize);

    NKENTSEU_MEMORY_EXTERN_C_END

    // Exemple 2 : Déclaration d'une classe avec dépréciation (via NKPlatform)
    #include "NKMemory/NkMemoryApi.h"

    class NKENTSEU_MEMORY_CLASS_EXPORT MemoryPool {
    public:
        NKENTSEU_MEMORY_API MemoryPool(size_t blockSize, size_t blockCount);
        NKENTSEU_MEMORY_API ~MemoryPool();

        // Dépréciation via macro NKPlatform (pas de duplication)
        NKENTSEU_DEPRECATED_MESSAGE("Utiliser Allocate(size_t, Alignment)")
        NKENTSEU_MEMORY_API void* Allocate(size_t size);

        NKENTSEU_MEMORY_API void* Allocate(size_t size, size_t alignment);
        NKENTSEU_MEMORY_API void Free(void* ptr);
        NKENTSEU_MEMORY_API void Reset();

        // Fonction inline critique pour performance
        NKENTSEU_MEMORY_API_FORCE_INLINE size_t GetBlockSize() const {
            return m_blockSize;
        }

    private:
        size_t m_blockSize;
        size_t m_blockCount;
        void*  m_pool;
    };

    // Exemple 3 : Alignement via macro NKPlatform (pas de duplication)
    struct NKENTSEU_ALIGN_64 NKENTSEU_MEMORY_CLASS_EXPORT SimdAllocator {
        NKENTSEU_MEMORY_API void* Allocate(size_t size);  // Retourne mémoire alignée 64B
        NKENTSEU_MEMORY_API void Free(void* ptr);
        
        uint8_t* m_buffer;  // Aligné pour AVX-512 / NEON
    };

    // Exemple 4 : Modes de build indépendants (CMake)
    // NKPlatform en DLL, NKMemory en static :
    //   target_compile_definitions(nkmemory PRIVATE NKENTSEU_MEMORY_STATIC_LIB)
    //   target_compile_definitions(nkmemory PRIVATE NKENTSEU_BUILD_SHARED_LIB)  # Pour NKPlatform
    //
    // NKPlatform en static, NKMemory en DLL :
    //   target_compile_definitions(nkmemory PRIVATE NKENTSEU_MEMORY_BUILD_SHARED_LIB)
    //   target_compile_definitions(nkmemory PRIVATE NKENTSEU_STATIC_LIB)  # Pour NKPlatform

    // Exemple 5 : Utilisation dans le code client
    #include <NKMemory/MemoryPool.h>

    int main() {
        nkentseu::memory::MemoryPool pool(256, 1024);  // 256B x 1024 blocs
        
        void* obj = pool.Allocate(128, 16);  // Allocation alignée 16B
        if (obj) {
            // ... utilisation ...
            pool.Free(obj);
        }
        return 0;
    }
*/

// =============================================================================
// EXEMPLES D'UTILISATION DE NKMEMORYAPI.H
// =============================================================================
// Ce fichier fournit les macros pour gérer la visibilité des symboles du module
// NKMemory, avec support multiplateforme et multi-compilateur.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Déclaration d'une API C publique NKMemory
// -----------------------------------------------------------------------------
/*
    // Dans nkmemory_public.h :
    #include "NKMemory/NkMemoryApi.h"

    NKENTSEU_MEMORY_EXTERN_C_BEGIN

    // Allocation standard
    NKENTSEU_MEMORY_API void* Memory_Allocate(size_t size);
    NKENTSEU_MEMORY_API void  Memory_Free(void* ptr);
    NKENTSEU_MEMORY_API void* Memory_Reallocate(void* ptr, size_t newSize);

    // Allocation alignée
    NKENTSEU_MEMORY_API void* Memory_AllocateAligned(size_t size, size_t alignment);
    NKENTSEU_MEMORY_API void  Memory_FreeAligned(void* ptr);

    // Utilities
    NKENTSEU_MEMORY_API size_t Memory_GetAllocatedSize(void* ptr);
    NKENTSEU_MEMORY_API void   Memory_SetDebugName(void* ptr, const char* name);

    NKENTSEU_MEMORY_EXTERN_C_END

    // Dans nkmemory.cpp :
    #include "nkmemory_public.h"

    void* Memory_Allocate(size_t size) {
        // Implémentation plateforme-specific...
        #ifdef NKENTSEU_PLATFORM_WINDOWS
            return _aligned_malloc(size, NKENTSEU_CACHE_LINE_SIZE);
        #else
            void* ptr = nullptr;
            posix_memalign(&ptr, NKENTSEU_CACHE_LINE_SIZE, size);
            return ptr;
        #endif
    }

    void Memory_Free(void* ptr) {
        if (ptr) {
            #ifdef NKENTSEU_PLATFORM_WINDOWS
                _aligned_free(ptr);
            #else
                free(ptr);
            #endif
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Déclaration d'un allocateur personnalisé NKMemory
// -----------------------------------------------------------------------------
/*
    // Dans ArenaAllocator.h :
    #include "NKMemory/NkMemoryApi.h"

    class NKENTSEU_MEMORY_CLASS_EXPORT ArenaAllocator {
    public:
        NKENTSEU_MEMORY_API ArenaAllocator(size_t arenaSize);
        NKENTSEU_MEMORY_API ~ArenaAllocator();

        // Allocation rapide (pas de free individuel)
        NKENTSEU_MEMORY_API_FORCE_INLINE void* Allocate(size_t size) {
            // Alignement automatique sur cache line
            size = (size + NKENTSEU_CACHE_LINE_SIZE - 1) & ~(NKENTSEU_CACHE_LINE_SIZE - 1);
            
            if (NKENTSEU_LIKELY(m_offset + size <= m_arenaSize)) {
                void* ptr = static_cast<char*>(m_arena) + m_offset;
                m_offset += size;
                return ptr;
            }
            return AllocateSlow(size);  // Slow path
        }

        NKENTSEU_MEMORY_API void Reset();
        NKENTSEU_MEMORY_API size_t GetUsedSize() const;

        // No-copy, no-move pour sécurité mémoire
        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    private:
        NKENTSEU_MEMORY_API_NO_INLINE void* AllocateSlow(size_t size);

        void*  m_arena;
        size_t m_arenaSize;
        size_t m_offset;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des modes de build via CMake
// -----------------------------------------------------------------------------
/*
    # CMakeLists.txt pour NKMemory

    cmake_minimum_required(VERSION 3.15)
    project(NKMemory VERSION 1.2.0)

    # Options de build
    option(NKMEMORY_BUILD_SHARED "Build NKMemory as shared library" ON)
    option(NKMEMORY_HEADER_ONLY "Use NKMemory in header-only mode" OFF)

    # Configuration des defines
    if(NKMEMORY_HEADER_ONLY)
        add_definitions(-DNKENTSEU_MEMORY_HEADER_ONLY)
    elseif(NKMEMORY_BUILD_SHARED)
        add_definitions(-DNKENTSEU_MEMORY_BUILD_SHARED_LIB)
        set(NKMEMORY_LIBRARY_TYPE SHARED)
    else()
        add_definitions(-DNKENTSEU_MEMORY_STATIC_LIB)
        set(NKMEMORY_LIBRARY_TYPE STATIC)
    endif()

    # Création de la bibliothèque
    add_library(nkmemory ${NKMEMORY_LIBRARY_TYPE}
        src/MemoryAllocator.cpp
        src/MemoryPool.cpp
        src/ArenaAllocator.cpp
        # ... autres fichiers sources
    )

    # Installation des en-têtes publics
    install(DIRECTORY include/NKMemory DESTINATION include)

    # Pour les consommateurs de NKMemory
    target_include_directories(nkmemory PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    # Link avec NKPlatform (dépendance requise)
    target_link_libraries(nkmemory PUBLIC NKPlatform::NKPlatform)
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Utilisation de NKMemory dans une application cliente
// -----------------------------------------------------------------------------
/*
    // Dans l'application cliente (CMakeLists.txt) :

    find_package(NKMemory REQUIRED)

    add_executable(monapp main.cpp)
    target_link_libraries(monapp PRIVATE NKMemory::NKMemory)

    # Si NKMemory est en mode DLL, le define d'import est géré par le package
    # Sinon, définir manuellement si nécessaire :
    # target_compile_definitions(monapp PRIVATE NKENTSEU_MEMORY_STATIC_LIB)

    // Dans main.cpp :
    #include <NKMemory/ArenaAllocator.h>

    int main() {
        nkentseu::memory::ArenaAllocator arena(1024 * 1024);  // 1MB arena

        // Allocation rapide pour frame temporaire
        struct Particle {
            float x, y, z;
            float life;
        };

        Particle* particles = static_cast<Particle*>(arena.Allocate(1000 * sizeof(Particle)));
        
        // ... simulation ...
        
        arena.Reset();  // Libération instantanée de tous les objets
        return 0;
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Mode header-only pour NKMemory
// -----------------------------------------------------------------------------
/*
    // Pour utiliser NKMemory en mode header-only :

    // 1. Définir la macro avant toute inclusion
    #define NKENTSEU_MEMORY_HEADER_ONLY
    #include <NKMemory/NkMemoryApi.h>
    #include <NKMemory/InlineAllocator.h>

    // 2. Toutes les fonctions sont inline, pas de linkage nécessaire
    // 3. Idéal pour les petits projets ou l'intégration directe

    void FastAllocExample() {
        // Allocation inline sans overhead de fonction
        void* ptr = InlineAllocator::Allocate<64>(256);
        // ... utilisation ...
        InlineAllocator::Free(ptr);
    }

    // Note : Le mode header-only peut augmenter la taille du binaire
    // car le code est dupliqué dans chaque unité de traduction.
    // À réserver aux allocateurs simples ou aux projets embarqués.
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Dépréciation et migration d'API NKMemory
// -----------------------------------------------------------------------------
/*
    #include <NKMemory/NkMemoryApi.h>

    // Ancienne API dépréciée
    NKENTSEU_DEPRECATED_MESSAGE("Utiliser Memory_AllocateAligned(size, align)")
    NKENTSEU_MEMORY_API void* Memory_Allocate(size_t size);

    // Nouvelle API recommandée avec alignement explicite
    NKENTSEU_MEMORY_API void* Memory_AllocateAligned(size_t size, size_t alignment);

    // Dans le code client :
    void MigrateCode() {
        // Ceci génère un warning de dépréciation :
        // void* old = Memory_Allocate(256);

        // Utiliser la nouvelle API avec alignement cache-line :
        void* aligned = Memory_AllocateAligned(256, NKENTSEU_CACHE_LINE_SIZE);
        
        // ... utilisation ...
        
        Memory_FreeAligned(aligned);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NKPlatform pour code multi-module
// -----------------------------------------------------------------------------
/*
    #include <NKPlatform/NkPlatformDetect.h>   // NKENTSEU_PLATFORM_*
    #include <NKPlatform/NkPlatformExport.h>   // NKENTSEU_PLATFORM_API
    #include <NKMemory/NkMemoryApi.h>          // NKENTSEU_MEMORY_API

    // Fonction qui utilise les deux modules pour un allocateur plateforme-aware
    NKENTSEU_MEMORY_API void* PlatformOptimizedAllocate(size_t size) {
        // Logging via NKPlatform
        NK_FOUNDATION_LOG_DEBUG("Allocating %zu bytes", size);

        // Choix de stratégie selon la plateforme
        #if defined(NKENTSEU_PLATFORM_WINDOWS)
            // Windows : utiliser VirtualAlloc pour grandes allocations
            if (size > 65536) {
                return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            }
        #elif defined(NKENTSEU_PLATFORM_LINUX)
            // Linux : utiliser mmap pour allocations NUMA-aware
            if (size > 65536) {
                return mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            }
        #endif

        // Fallback : allocateur standard NKMemory
        return Memory_AllocateAligned(size, NKENTSEU_CACHE_LINE_SIZE);
    }

    // Gestion des modes de build indépendants
    NKENTSEU_MEMORY_API void PrintBuildConfig() {
        #if defined(NKENTSEU_MEMORY_BUILD_SHARED_LIB)
            NK_FOUNDATION_LOG_INFO("NKMemory mode: Shared DLL");
        #elif defined(NKENTSEU_MEMORY_STATIC_LIB)
            NK_FOUNDATION_LOG_INFO("NKMemory mode: Static library");
        #elif defined(NKENTSEU_MEMORY_HEADER_ONLY)
            NK_FOUNDATION_LOG_INFO("NKMemory mode: Header-only");
        #else
            NK_FOUNDATION_LOG_INFO("NKMemory mode: DLL import (default)");
        #endif
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================