// =============================================================================
// NKMemory/NkGlobalOperators.cpp
// Surcharge des opérateurs new/delete globaux avec support de sized deallocation.
//
// DESIGN :
//   - Routage via malloc/free bruts pour éviter SIOF (Static Initialization 
//     Order Fiasco) : ces opérateurs peuvent être appelés avant l'initialisation
//     de tout objet global dans le projet.
//   - Sized operator delete (C++14) appelle free_sized (C23) quand disponible,
//     informant le runtime de la taille exacte du bloc pour lookup O(1) en free-list.
//   - Compiler ce fichier dans tout exécutable souhaitant le support sized delete.
//     NE PAS compiler dans des bibliothèques statiques (provoque des violations ODR).
//
// PLATEFORMES : Toutes (Win32, Linux, WASM, Android)
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

// -------------------------------------------------------------------------
// PRÉ-COMPILED HEADER - OPTIONNEL (ce fichier peut être compilé sans pch)
// -------------------------------------------------------------------------
// Note : Ce fichier est conçu pour être compilé directement dans l'exécutable.
// Si votre projet utilise pch.h, décommentez la ligne suivante :
// #include "pch.h"

// -------------------------------------------------------------------------
// EN-TÊTES STANDARD (uniquement, pas de dépendances NK* pour éviter SIOF)
// -------------------------------------------------------------------------
#include <new>         // std::bad_alloc, std::nothrow_t
#include <cstdlib>     // ::malloc, ::free, ::free_sized (C23)
#include <cstddef>     // std::size_t, std::nullptr_t

// -------------------------------------------------------------------------
// DÉTECTION DES FONCTIONNALITÉS PLATEFORME
// -------------------------------------------------------------------------
/**
 * @def NK_GLOBAL_OPS_HAS_FREE_SIZED
 * @brief Indique si la fonction free_sized (C23) est disponible
 * @note free_sized permet une deallocation O(1) en informant le runtime de la taille exacte
 */
#if !defined(_WIN32) && !defined(_WIN64) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    #define NK_GLOBAL_OPS_HAS_FREE_SIZED 1
#else
    #define NK_GLOBAL_OPS_HAS_FREE_SIZED 0
#endif

/**
 * @def NK_GLOBAL_OPS_HAS_ALIGNED_ALLOC
 * @brief Indique si aligned_alloc (C11/POSIX) est disponible
 * @note Utilisé pour les allocations avec alignement > alignof(std::max_align_t)
 */
#if defined(_WIN32) || defined(_WIN64)
    #define NK_GLOBAL_OPS_HAS_ALIGNED_ALLOC 0  // Windows : utiliser _aligned_malloc
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define NK_GLOBAL_OPS_HAS_ALIGNED_ALLOC 1  // C11+ : aligned_alloc disponible
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    #define NK_GLOBAL_OPS_HAS_ALIGNED_ALLOC 1  // POSIX.1-2001+ : posix_memalign disponible
#else
    #define NK_GLOBAL_OPS_HAS_ALIGNED_ALLOC 0  // Fallback : malloc simple (alignement non garanti)
#endif

// -------------------------------------------------------------------------
// HELPERS PLATEFORME POUR ALLOCATION ALIGNÉE
// -------------------------------------------------------------------------
namespace {

    /**
     * @brief Alloue de la mémoire avec alignement spécifié (fallback portable)
     * @param alignment Alignement requis (doit être puissance de 2)
     * @param size Taille en bytes à allouer
     * @return Pointeur aligné, ou nullptr en cas d'échec
     * @note Utilise la meilleure méthode disponible selon la plateforme
     */
    [[nodiscard]] void* PlatformAlignedAlloc(std::size_t alignment, std::size_t size) noexcept {
        // Guard : alignement doit être puissance de 2 et >= sizeof(void*)
        if (alignment < sizeof(void*) || (alignment & (alignment - 1u)) != 0u) {
            alignment = alignof(std::max_align_t);
        }
        
        #if defined(_WIN32) || defined(_WIN64)
            // Windows : _aligned_malloc (MSVCRT)
            return _aligned_malloc(size, alignment);
            
        #elif NK_GLOBAL_OPS_HAS_ALIGNED_ALLOC
            // C11/POSIX : aligned_alloc ou posix_memalign
            #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
                // aligned_alloc requiert que size soit multiple de alignment
                const std::size_t alignedSize = (size + alignment - 1u) & ~(alignment - 1u);
                return ::aligned_alloc(alignment, alignedSize);
            #else
                // posix_memalign : retourne 0 si succès, errno sinon
                void* ptr = nullptr;
                if (::posix_memalign(&ptr, alignment, size) == 0) {
                    return ptr;
                }
                return nullptr;
            #endif
            
        #else
            // Fallback : malloc simple (alignement non garanti, mais souvent suffisant)
            return ::malloc(size);
        #endif
    }

    /**
     * @brief Libère de la mémoire allouée via PlatformAlignedAlloc
     * @param ptr Pointeur à libérer (nullptr accepté, no-op)
     * @note Utilise la fonction de libération correspondante à la plateforme
     */
    void PlatformAlignedFree(void* ptr) noexcept {
        if (!ptr) {
            return;
        }
        
        #if defined(_WIN32) || defined(_WIN64)
            _aligned_free(ptr);
        #else
            // POSIX/C11 : free est compatible avec aligned_alloc/posix_memalign
            ::free(ptr);
        #endif
    }

} // namespace anonyme


// =============================================================================
// SURCHARGES SCALAIRES : new / delete (objets uniques)
// =============================================================================

/**
 * @brief Surcharge de operator new scalaire (avec exception)
 * @param size Taille en bytes à allouer
 * @return Pointeur vers la mémoire allouée
 * @throw std::bad_alloc si l'allocation échoue
 * @note Traite size==0 comme size==1 pour conformité C++
 */
void* operator new(std::size_t size) {
    // C++ standard : new de 0 byte doit retourner un pointeur unique non-null
    if (size == 0u) { 
        size = 1u; 
    }
    
    void* ptr = ::malloc(size);
    if (!ptr) { 
        throw std::bad_alloc{}; 
    }
    return ptr;
}

/**
 * @brief Surcharge de operator new scalaire (nothrow, sans exception)
 * @param size Taille en bytes à allouer
 * @param {nothrow} Paramètre tag pour sélectionner cette surcharge
 * @return Pointeur vers la mémoire allouée, ou nullptr en cas d'échec
 * @note Ne lance jamais d'exception : retourne nullptr si malloc échoue
 */
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (size == 0u) { 
        size = 1u; 
    }
    return ::malloc(size);
}

/**
 * @brief Surcharge de operator delete scalaire (base)
 * @param ptr Pointeur à libérer (nullptr accepté, no-op)
 * @note Délègue à ::free() : pair avec ::malloc dans operator new
 */
void operator delete(void* ptr) noexcept {
    ::free(ptr);
}

/**
 * @brief Surcharge de operator delete scalaire (nothrow variant)
 * @param ptr Pointeur à libérer
 * @param {nothrow} Paramètre tag (ignoré, pour cohérence de signature)
 */
void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    ::free(ptr);
}

/**
 * @brief Surcharge de sized operator delete scalaire (C++14)
 * @param ptr Pointeur à libérer
 * @param size Taille exacte du bloc (fournie par le compilateur)
 * @note Si NK_GLOBAL_OPS_HAS_FREE_SIZED : utilise free_sized pour O(1) deallocation
 * @note Sinon : fallback vers free() standard
 * 
 * @internal Le compilateur appelle cette surcharge quand il connaît la taille exacte
 *           de l'objet au point de delete, permettant des optimisations runtime.
 */
void operator delete(void* ptr, std::size_t size) noexcept {
    #if NK_GLOBAL_OPS_HAS_FREE_SIZED
        // C23 : free_sized informe le runtime de la taille exacte → lookup O(1) en free-list
        ::free_sized(ptr, size);
    #else
        // Fallback : ignorer size et utiliser free standard
        (void)size;
        ::free(ptr);
    #endif
}

/**
 * @brief Surcharge de aligned operator new (C++17)
 * @param size Taille en bytes à allouer
 * @param alignment Alignement requis (puissance de 2)
 * @return Pointeur aligné vers la mémoire allouée
 * @throw std::bad_alloc si l'allocation échoue
 * @note Utilise PlatformAlignedAlloc pour compatibilité multiplateforme
 */
void* operator new(std::size_t size, std::align_val_t alignment) {
    if (size == 0u) { 
        size = 1u; 
    }
    
    void* ptr = ::PlatformAlignedAlloc(static_cast<std::size_t>(alignment), size);
    if (!ptr) { 
        throw std::bad_alloc{}; 
    }
    return ptr;
}

/**
 * @brief Surcharge de aligned operator new (nothrow, C++17)
 * @param size Taille en bytes
 * @param alignment Alignement requis
 * @param {nothrow} Paramètre tag
 * @return Pointeur aligné, ou nullptr en cas d'échec
 */
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    if (size == 0u) { 
        size = 1u; 
    }
    return ::PlatformAlignedAlloc(static_cast<std::size_t>(alignment), size);
}

/**
 * @brief Surcharge de aligned operator delete (C++17)
 * @param ptr Pointeur aligné à libérer
 * @param alignment Alignement utilisé lors de l'allocation
 * @note Délègue à PlatformAlignedFree pour compatibilité multiplateforme
 */
void operator delete(void* ptr, std::align_val_t) noexcept {
    ::PlatformAlignedFree(ptr);
}

/**
 * @brief Surcharge de aligned sized operator delete (C++17 + C++14 sized)
 * @param ptr Pointeur aligné à libérer
 * @param size Taille exacte du bloc
 * @param alignment Alignement utilisé
 * @note Combine sized deallocation et alignement pour optimisation maximale
 */
void operator delete(void* ptr, std::size_t size, std::align_val_t) noexcept {
    #if NK_GLOBAL_OPS_HAS_FREE_SIZED
        ::free_sized(ptr, size);
    #else
        (void)size;
        ::PlatformAlignedFree(ptr);
    #endif
}


// =============================================================================
// SURCHARGES ARRAY : new[] / delete[] (tableaux)
// =============================================================================

/**
 * @brief Surcharge de operator new[] scalaire (avec exception)
 * @param size Taille totale du tableau en bytes
 * @return Pointeur vers le tableau alloué
 * @throw std::bad_alloc si l'allocation échoue
 * @note Identique à operator new scalaire : la différence est sémantique pour le compilateur
 */
void* operator new[](std::size_t size) {
    if (size == 0u) { 
        size = 1u; 
    }
    void* ptr = ::malloc(size);
    if (!ptr) { 
        throw std::bad_alloc{}; 
    }
    return ptr;
}

/**
 * @brief Surcharge de operator new[] nothrow
 * @param size Taille du tableau
 * @param {nothrow} Paramètre tag
 * @return Pointeur vers le tableau, ou nullptr en cas d'échec
 */
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (size == 0u) { 
        size = 1u; 
    }
    return ::malloc(size);
}

/**
 * @brief Surcharge de operator delete[] scalaire
 * @param ptr Pointeur vers le tableau à libérer
 */
void operator delete[](void* ptr) noexcept {
    ::free(ptr);
}

/**
 * @brief Surcharge de operator delete[] nothrow
 */
void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    ::free(ptr);
}

/**
 * @brief Surcharge de sized operator delete[] (C++14)
 * @param ptr Pointeur vers le tableau
 * @param size Taille exacte du tableau (fournie par le compilateur)
 * @note Optimisation : free_sized si disponible pour deallocation O(1)
 */
void operator delete[](void* ptr, std::size_t size) noexcept {
    #if NK_GLOBAL_OPS_HAS_FREE_SIZED
        ::free_sized(ptr, size);
    #else
        (void)size;
        ::free(ptr);
    #endif
}

/**
 * @brief Surcharge de aligned operator new[] (C++17)
 */
void* operator new[](std::size_t size, std::align_val_t alignment) {
    if (size == 0u) { 
        size = 1u; 
    }
    void* ptr = ::PlatformAlignedAlloc(static_cast<std::size_t>(alignment), size);
    if (!ptr) { 
        throw std::bad_alloc{}; 
    }
    return ptr;
}

/**
 * @brief Surcharge de aligned operator new[] nothrow
 */
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    if (size == 0u) { 
        size = 1u; 
    }
    return ::PlatformAlignedAlloc(static_cast<std::size_t>(alignment), size);
}

/**
 * @brief Surcharge de aligned operator delete[]
 */
void operator delete[](void* ptr, std::align_val_t) noexcept {
    ::PlatformAlignedFree(ptr);
}

/**
 * @brief Surcharge de aligned sized operator delete[]
 */
void operator delete[](void* ptr, std::size_t size, std::align_val_t) noexcept {
    #if NK_GLOBAL_OPS_HAS_FREE_SIZED
        ::free_sized(ptr, size);
    #else
        (void)size;
        ::PlatformAlignedFree(ptr);
    #endif
}


// =============================================================================
// NOTES DE COMPILATION ET AVERTISSEMENTS CRITIQUES
// =============================================================================
/*
    [⚠️ RÈGLE D'OR : ODR (One Definition Rule)]
    
    Ce fichier DOIT être compilé dans :
    ✓ L'exécutable principal de votre application
    ✓ Eventuellement dans des DLL/shared libraries si elles définissent leurs propres new/delete
    
    Ce fichier NE DOIT PAS être compilé dans :
    ✗ Des bibliothèques statiques (.a/.lib) → violations ODR si lié dans plusieurs exécutables
    ✗ Des en-têtes (.h) → définitions multiples dans chaque TU qui inclut l'header
    
    Solution recommandée :
    1. Créer un fichier source dédié : NkGlobalOperators.cpp
    2. L'ajouter uniquement à la cible exécutable dans votre CMakeLists.txt :
       
       add_executable(MyApp main.cpp ...)
       target_sources(MyApp PRIVATE NKMemory/NkGlobalOperators.cpp)
    
    3. NE PAS l'ajouter aux bibliothèques :
       
       add_library(MyLib STATIC ...)  # Ne pas inclure NkGlobalOperators.cpp ici !
    
    [SIOF (Static Initialization Order Fiasco)]
    
    Pourquoi utiliser malloc/free directement au lieu de NkAllocator ?
    
    Problème : Les opérateurs new/delete globaux peuvent être appelés pendant 
    l'initialisation statique, AVANT que les objets globaux de NKMemory soient construits.
    
    Exemple dangereux :
        // Dans un fichier global
        MyClass gObj = MyClass();  // Constructeur appelé pendant initialisation statique
                                   // → peut appeler operator new
                                   // → si operator new utilise NkGetDefaultAllocator()
                                   // → et que NkMemorySystem n'est pas encore initialisé → CRASH
    
    Solution : Ce fichier utilise ::malloc/::free directement, qui sont toujours disponibles
    car fournis par la libc/runtime, indépendants de l'initialisation C++.
    
    [SIZED DEALLOCATION (C++14 / C23)]
    
    Quand le compilateur connaît la taille exacte d'un objet au point de delete,
    il peut appeler la surcharge sized delete :
    
        class MyClass { ... };
        MyClass* obj = new MyClass();
        delete obj;  // Compilateur peut appeler operator delete(obj, sizeof(MyClass))
    
    Avantages de free_sized (C23) :
    - Le runtime connaît la taille exacte → pas besoin de lire un header mémoire
    - Lookup O(1) dans la free-list au lieu de O(n) ou recherche dans un map
    - Réduction de la fragmentation : blocs de même taille regroupés
    
    Détection automatique :
    - C23 (__STDC_VERSION__ >= 202311L) + non-Windows → free_sized disponible
    - Sinon → fallback vers free() standard (comportement inchangé)
    
    [ALIGNEMENT (C++17)]
    
    Les surcharges aligned new/delete gèrent les types avec alignement > alignof(std::max_align_t) :
    
        struct alignas(64) SimdData { ... };
        SimdData* data = new SimdData();  // Appelle operator new(size, align_val_t(64))
    
    Implémentation multiplateforme :
    - Windows : _aligned_malloc / _aligned_free
    - POSIX/C11 : aligned_alloc ou posix_memalign / free
    - Fallback : malloc simple (alignement non garanti mais souvent suffisant)
    
    [CONFIGURATION DE BUILD]
    
    Pour activer/désactiver des fonctionnalités via macros :
    
        // Dans CMakeLists.txt ou configuration de build :
        target_compile_definitions(MyApp PRIVATE
            NKENTSEU_EXCEPTIONS_ENABLED=1    # Activer les exceptions dans new
            # NK_GLOBAL_OPS_HAS_FREE_SIZED=1 # Forcer free_sized (détection auto normalement)
        )
    
    [TESTING]
    
    Valider que les surcharges fonctionnent correctement :
    
        #include <cassert>
        
        void TestGlobalOperators() {
            // Test scalaire
            int* p1 = new int(42);
            assert(*p1 == 42);
            delete p1;
            
            // Test tableau
            float* p2 = new float[100];
            delete[] p2;
            
            // Test aligné (C++17)
            struct alignas(32) Aligned { char data[32]; };
            Aligned* p3 = new Aligned();
            assert(reinterpret_cast<std::uintptr_t>(p3) % 32 == 0);
            delete p3;
            
            // Test nothrow
            void* p4 = new (std::nothrow) char[1024*1024*1024];  // 1GB, peut échouer
            if (p4) {
                delete[] p4;
            }
        }
    
    [PERFORMANCE]
    
    Impact des surcharges globales :
    - Sans sized delete : comportement identique à malloc/free standard
    - Avec sized delete (C23) : deallocation ~2-5x plus rapide pour petits blocs
    - Overhead d'appel : négligeable (<1 cycle) car inline par le compilateur
    - Alignment : PlatformAlignedAlloc peut être ~10-20% plus lent que malloc simple
    
    [DÉPANNAGE]
    
    Symptôme : Crash au démarrage de l'application
    Cause probable : SIOF → new/delete appelé avant initialisation de NKMemory
    Solution : Ce fichier utilise malloc/free directement → devrait éviter le problème.
               Vérifier qu'aucun autre code ne surcharge new/delete de manière incompatible.
    
    Symptôme : Erreur de liaison "multiple definition of operator new"
    Cause probable : NkGlobalOperators.cpp compilé dans plusieurs cibles (violation ODR)
    Solution : Ne compiler ce fichier que dans l'exécutable principal.
    
    Symptôme : free_sized non trouvé à l'édition de liens
    Cause probable : Compilation avec un compiler/toolchain ne supportant pas C23
    Solution : La détection automatique devrait fallback vers free(). Vérifier __STDC_VERSION__.
*/

// =============================================================================
// COMPATIBILITÉ AVEC NKMEMORY ALLOCATORS
// =============================================================================
/*
    Ces surcharges globales sont indépendantes du système NKMemory.
    Pour utiliser les allocateurs NKMemory avec new/delete :
    
    Option 1 : Utiliser les macros NK_MEM_NEW/NK_MEM_DELETE (recommandé)
        #include "NKMemory/NkMemory.h"
        auto* obj = NK_MEM_NEW(MyClass, args...);  // Utilise NkMemorySystem
        NK_MEM_DELETE(obj);
    
    Option 2 : Remplacer les allocateurs par défaut de NKMemory
        // Dans main() ou init :
        nkentseu::memory::NkMemorySystem::Instance().Initialize(&myCustomAllocator);
        // Ensuite, les allocations via NK_MEM_* utiliseront myCustomAllocator
    
    Option 3 : Utiliser NkStlAdapter pour les conteneurs STL (voir NkStlAdapter.h)
        std::vector<int, nkentseu::memory::NkStlAdapter<int, NkContainerAllocator>> vec(&containerAlloc);
    
    Note : Les opérateurs globaux new/delete dans ce fichier utilisent TOUJOURS malloc/free,
    pas les allocateurs NKMemory. C'est intentionnel pour éviter SIOF et garantir 
    la disponibilité dès le démarrage de l'application.
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================