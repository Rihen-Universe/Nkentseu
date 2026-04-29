// =============================================================================
// NKContainers/NkCompat.h
// Couche de compatibilité et macros utilitaires pour le module NKContainers.
//
// Design :
//  - Réutilisation DIRECTE des macros de NKPlatform (ZÉRO duplication)
//  - Détection automatique des standards C++ via NKPlatform
//  - Macros de compatibilité uniquement si NON définies par NKPlatform
//  - Préfixe cohérent NKENTSEU_CONTAINERS_* pour les ajouts spécifiques
//  - Undef proactif des macros génériques potentiellement conflictuelles
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_NKCOMPAT_H_INCLUDED
#define NKENTSEU_CONTAINERS_NKCOMPAT_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKContainers dépend de NKPlatform pour toutes les macros de base.
    // Nous importons d'abord les détections de plateforme et les assertions.

    #include "NKPlatform/NkPlatformDetect.h"      // NKENTSEU_PLATFORM_*, NKENTSEU_CXX_*
    #include "NKCore/Assert/NkAssert.h"      // NKENTSEU_ASSERT, NKENTSEU_ASSERT_MSG
    #include "NKPlatform/NkPlatformInline.h"      // NKENTSEU_INLINE, NKENTSEU_FORCE_INLINE
    #include <new>                                // std::nothrow_t, placement new

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉTECTION DES STANDARDS C++ (VIA NKPLATFORM)
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainersCppCompat Compatibilité C++ NKContainers
     * @brief Macros de détection des standards C++ pour NKContainers
     *
     * Ces macros sont des alias vers NKENTSEU_CXX_* de NKPlatform.
     * Elles sont fournies pour la lisibilité interne à NKContainers.
     *
     * @note Ne pas redéfinir si NKPlatform fournit déjà l'équivalent.
     */

    // Alias vers NKPlatform pour cohérence interne (optionnel, pour lisibilité)
    #if !defined(NKENTSEU_CONTAINERS_USE_CPP11)
        #define NKENTSEU_CONTAINERS_USE_CPP11 NKENTSEU_CXX11_OR_LATER
    #endif

    #if !defined(NKENTSEU_CONTAINERS_USE_CPP17)
        #define NKENTSEU_CONTAINERS_USE_CPP17 NKENTSEU_CXX17_OR_LATER
    #endif

    #if !defined(NKENTSEU_CONTAINERS_USE_CPP20)
        #define NKENTSEU_CONTAINERS_USE_CPP20 NKENTSEU_CXX20_OR_LATER
    #endif

    // -------------------------------------------------------------------------
    // SECTION 3 : MACROS DE LANGAGE (ALIASES VERS NKPLATFORM)
    // -------------------------------------------------------------------------
    /**
     * @brief Alias constexpr avec fallback sécurisé
     * @def NKENTSEU_CONTAINERS_CONSTEXPR
     * @ingroup ContainersCppCompat
     *
     * Utilise NKENTSEU_CONSTEXPR de NKPlatform si disponible,
     * sinon fallback vers constexpr standard.
     */
    #if !defined(NKENTSEU_CONTAINERS_CONSTEXPR)
        #ifdef NKENTSEU_CONSTEXPR
            #define NKENTSEU_CONTAINERS_CONSTEXPR NKENTSEU_CONSTEXPR
        #else
            #define NKENTSEU_CONTAINERS_CONSTEXPR constexpr
        #endif
    #endif

    /**
     * @brief Alias noexcept avec fallback sécurisé
     * @def NKENTSEU_CONTAINERS_NOEXCEPT
     * @ingroup ContainersCppCompat
     *
     * Utilise NKENTSEU_NOEXCEPT de NKPlatform si disponible,
     * sinon fallback vers noexcept standard.
     */
    #if !defined(NKENTSEU_CONTAINERS_NOEXCEPT)
        #ifdef NKENTSEU_NOEXCEPT
            #define NKENTSEU_CONTAINERS_NOEXCEPT NKENTSEU_NOEXCEPT
        #else
            #define NKENTSEU_CONTAINERS_NOEXCEPT noexcept
        #endif
    #endif

    /**
     * @brief Alias inline avec fallback sécurisé
     * @def NKENTSEU_CONTAINERS_INLINE
     * @ingroup ContainersCppCompat
     *
     * Utilise NKENTSEU_INLINE de NKPlatform.
     * Ne jamais redéfinir inline manuellement.
     */
    #if !defined(NKENTSEU_CONTAINERS_INLINE)
        #define NKENTSEU_CONTAINERS_INLINE NKENTSEU_INLINE
    #endif

    /**
     * @brief Macro pour fonctions force-inline dans NKContainers
     * @def NKENTSEU_CONTAINERS_FORCE_INLINE
     * @ingroup ContainersCppCompat
     *
     * Alias vers NKENTSEU_FORCE_INLINE pour cohérence.
     * Particulièrement utile pour les accesseurs de conteneurs.
     */
    #if !defined(NKENTSEU_CONTAINERS_FORCE_INLINE)
        #define NKENTSEU_CONTAINERS_FORCE_INLINE NKENTSEU_FORCE_INLINE
    #endif

    // -------------------------------------------------------------------------
    // SECTION 4 : INTÉGRATION AVEC NKCORE (API ET ASSERTIONS)
    // -------------------------------------------------------------------------
    /**
     * @brief Intégration de l'API NKCore dans NKContainers
     * @def NKENTSEU_CONTAINERS_CORE_API
     * @ingroup ContainersCoreIntegration
     *
     * Si NKCore est disponible, utilise NKENTSEU_CORE_API.
     * Sinon, fallback vers vide pour compilation standalone.
     *
     * @note Permet à NKContainers d'être utilisé sans NKCore si nécessaire.
     */
    #if !defined(NKENTSEU_CONTAINERS_CORE_API)
        #ifdef NKENTSEU_CORE_API
            #define NKENTSEU_CONTAINERS_CORE_API NKENTSEU_CORE_API
        #else
            #define NKENTSEU_CONTAINERS_CORE_API
        #endif
    #endif

    /**
     * @brief Macro d'assertion pour NKContainers
     * @def NKENTSEU_CONTAINERS_ASSERT
     * @ingroup ContainersCoreIntegration
     *
     * Utilise NKENTSEU_ASSERT de NKPlatform par défaut.
     * Peut être redéfini pour des besoins spécifiques de test.
     *
     * @example
     * @code
     * // Redéfinition pour tests unitaires
     * #ifdef NKCONTAINERS_UNIT_TESTS
     *     #define NKENTSEU_CONTAINERS_ASSERT(expr) CustomTestAssert(expr)
     * #endif
     * @endcode
     */
    #if !defined(NKENTSEU_CONTAINERS_ASSERT)
        #define NKENTSEU_CONTAINERS_ASSERT(expr) NKENTSEU_ASSERT(expr)
    #endif

    /**
     * @brief Macro d'assertion avec message pour NKContainers
     * @def NKENTSEU_CONTAINERS_ASSERT_MSG
     * @ingroup ContainersCoreIntegration
     *
     * Version avec message d'erreur personnalisé.
     */
    #if !defined(NKENTSEU_CONTAINERS_ASSERT_MSG)
        #define NKENTSEU_CONTAINERS_ASSERT_MSG(expr, msg) NKENTSEU_ASSERT_MSG(expr, msg)
    #endif

    // -------------------------------------------------------------------------
    // SECTION 5 : CONFIGURATION SPÉCIFIQUE NKCONTAINERS
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainersConfig Configuration NKContainers
     * @brief Paramètres de compilation spécifiques aux conteneurs
     */

    /**
     * @brief Taille du Small String Optimization pour nkentseu::String
     * @def NKENTSEU_CONTAINERS_STRING_SSO_SIZE
     * @ingroup ContainersConfig
     *
     * Définit la taille du buffer interne pour éviter les allocations
     * dynamiques sur les petites chaînes.
     *
     * @note Valeur par défaut : 24 bytes (aligné sur 3 pointers 64-bit)
     * @note Peut être ajusté via -DNKENTSEU_CONTAINERS_STRING_SSO_SIZE=N
     */
    #if !defined(NKENTSEU_CONTAINERS_STRING_SSO_SIZE)
        #define NKENTSEU_CONTAINERS_STRING_SSO_SIZE 24
    #endif

    /**
     * @brief Alignement par défaut pour les allocateurs NKContainers
     * @def NKENTSEU_CONTAINERS_DEFAULT_ALIGNMENT
     * @ingroup ContainersConfig
     *
     * Alignement utilisé pour les allocations de conteneurs.
     * Par défaut : alignement cache-friendly (64 bytes).
     */
    #if !defined(NKENTSEU_CONTAINERS_DEFAULT_ALIGNMENT)
        #define NKENTSEU_CONTAINERS_DEFAULT_ALIGNMENT 64
    #endif

    /**
     * @brief Taille minimale de croissance pour les conteneurs dynamiques
     * @def NKENTSEU_CONTAINERS_MIN_GROWTH_FACTOR
     * @ingroup ContainersConfig
     *
     * Facteur multiplicatif minimal lors du redimensionnement.
     * Valeur typique : 1.5 (équilibre mémoire/performance).
     */
    #if !defined(NKENTSEU_CONTAINERS_MIN_GROWTH_FACTOR)
        #define NKENTSEU_CONTAINERS_MIN_GROWTH_FACTOR 1.5f
    #endif

    // -------------------------------------------------------------------------
    // SECTION 6 : ALIASES DE PLATEFORME (COMPATIBILITÉ LEGACY)
    // -------------------------------------------------------------------------
    // Fournit des alias courts pour compatibilité avec code existant.
    // Ces macros sont DÉPRÉCIÉES : préférer les versions NKENTSEU_*.

    #if !defined(NKENTSEU_CONTAINERS_LEGACY_ALIAS_DISABLE)
        // Alias plateforme (déprécié - utiliser NKENTSEU_PLATFORM_*)
        #if !defined(NK_CONTAINERS_PLATFORM_WINDOWS) && defined(NKENTSEU_PLATFORM_WINDOWS)
            #define NK_CONTAINERS_PLATFORM_WINDOWS 1
            #ifndef NKENTSEU_CONTAINERS_SUPPRESS_LEGACY_WARNINGS
                #pragma message("NK_CONTAINERS_PLATFORM_WINDOWS est déprécié, utiliser NKENTSEU_PLATFORM_WINDOWS")
            #endif
        #endif

        #if !defined(NK_CONTAINERS_PLATFORM_LINUX) && defined(NKENTSEU_PLATFORM_LINUX)
            #define NK_CONTAINERS_PLATFORM_LINUX 1
            #ifndef NKENTSEU_CONTAINERS_SUPPRESS_LEGACY_WARNINGS
                #pragma message("NK_CONTAINERS_PLATFORM_LINUX est déprécié, utiliser NKENTSEU_PLATFORM_LINUX")
            #endif
        #endif

        #if !defined(NK_CONTAINERS_PLATFORM_MACOS) && defined(NKENTSEU_PLATFORM_MACOS)
            #define NK_CONTAINERS_PLATFORM_MACOS 1
            #ifndef NKENTSEU_CONTAINERS_SUPPRESS_LEGACY_WARNINGS
                #pragma message("NK_CONTAINERS_PLATFORM_MACOS est déprécié, utiliser NKENTSEU_PLATFORM_MACOS")
            #endif
        #endif
    #endif // NKENTSEU_CONTAINERS_LEGACY_ALIAS_DISABLE

    // -------------------------------------------------------------------------
    // SECTION 7 : UNDEF PROACTIF DES MACROS GÉNÉRIQUES CONFLICTUELLES
    // -------------------------------------------------------------------------
    /**
     * @defgroup ContainersMacroSafety Sécurité des Macros
     * @brief Gestion des conflits de noms de macros
     *
     * Certains noms de macros génériques (NkSwap, NkForEach, etc.) peuvent
     * entrer en conflit avec d'autres bibliothèques. Nous les undefinons
     * proactivement avant de définir nos versions NKENTSEU_*.
     */

    // Undef des macros potentiellement conflictuelles (noms courts)
    #ifdef NkSwap
        #undef NkSwap
    #endif
    #ifdef NkForEach
        #undef NkForEach
    #endif
    #ifdef NkAllocAligned
        #undef NkAllocAligned
    #endif
    #ifdef NkFreeAligned
        #undef NkFreeAligned
    #endif
    #ifdef NkMin
        #undef NkMin
    #endif
    #ifdef NkMax
        #undef NkMax
    #endif
    #ifdef NkClamp
        #undef NkClamp
    #endif

    // -------------------------------------------------------------------------
    // SECTION 8 : MACROS UTILITAIRES SPÉCIFIQUES NKCONTAINERS
    // -------------------------------------------------------------------------
    /**
     * @brief Macro de swap optimisé pour NKContainers
     * @def NKENTSEU_CONTAINERS_SWAP
     * @ingroup ContainersMacroSafety
     *
     * Utilise std::swap si disponible, sinon fallback manuel.
     * Optimisé pour les types trivialement copiables.
     */
    #define NKENTSEU_CONTAINERS_SWAP(a, b) \
        do { \
            using NKENTSEU_CONTAINERS_SWAP_TMP_T = decltype(a); \
            NKENTSEU_CONTAINERS_SWAP_TMP_T NKENTSEU_CONTAINERS_SWAP_TMP = static_cast<NKENTSEU_CONTAINERS_SWAP_TMP_T&&>(a); \
            a = static_cast<decltype(b)&&>(b); \
            b = static_cast<decltype(NKENTSEU_CONTAINERS_SWAP_TMP)&&>(NKENTSEU_CONTAINERS_SWAP_TMP); \
        } while(0)

    /**
     * @brief Macro pour boucle foreach style sur conteneurs NKContainers
     * @def NKENTSEU_CONTAINERS_FOREACH
     * @ingroup ContainersMacroSafety
     *
     * Wrapper autour de range-based for loop C++11+.
     * Fallback vers itérateur classique si C++11 non disponible.
     */
    #if NKENTSEU_CXX11_OR_LATER
        #define NKENTSEU_CONTAINERS_FOREACH(var, container) \
            for (auto&& var : container)
    #else
        #define NKENTSEU_CONTAINERS_FOREACH(var, container) \
            for (auto it = (container).Begin(), end = (container).End(); it != end; ++it) \
                if (auto&& var = *it, true)
    #endif

    /**
     * @brief Allocation alignée via NKCore/NKPlatform
     * @def NKENTSEU_CONTAINERS_ALLOC_ALIGNED
     * @ingroup ContainersMacroSafety
     *
     * Utilise NKENTSEU_ALLOC_ALIGNED de NKPlatform si disponible.
     * Fallback vers aligned_alloc ou _aligned_malloc selon plateforme.
     */
    #if !defined(NKENTSEU_CONTAINERS_ALLOC_ALIGNED)
        #ifdef NKENTSEU_ALLOC_ALIGNED
            #define NKENTSEU_CONTAINERS_ALLOC_ALIGNED(alignment, size) \
                NKENTSEU_ALLOC_ALIGNED(alignment, size)
        #elif defined(NKENTSEU_PLATFORM_WINDOWS)
            #define NKENTSEU_CONTAINERS_ALLOC_ALIGNED(alignment, size) \
                _aligned_malloc(size, alignment)
        #else
            #define NKENTSEU_CONTAINERS_ALLOC_ALIGNED(alignment, size) \
                aligned_alloc(alignment, ((size) + (alignment) - 1) & ~((alignment) - 1))
        #endif
    #endif

    /**
     * @brief Libération de mémoire alignée
     * @def NKENTSEU_CONTAINERS_FREE_ALIGNED
     * @ingroup ContainersMacroSafety
     */
    #if !defined(NKENTSEU_CONTAINERS_FREE_ALIGNED)
        #ifdef NKENTSEU_FREE_ALIGNED
            #define NKENTSEU_CONTAINERS_FREE_ALIGNED(ptr) \
                NKENTSEU_FREE_ALIGNED(ptr)
        #elif defined(NKENTSEU_PLATFORM_WINDOWS)
            #define NKENTSEU_CONTAINERS_FREE_ALIGNED(ptr) \
                _aligned_free(ptr)
        #else
            #define NKENTSEU_CONTAINERS_FREE_ALIGNED(ptr) \
                free(ptr)
        #endif
    #endif

    // -------------------------------------------------------------------------
    // SECTION 9 : VALIDATION ET COHÉRENCE DES MACROS
    // -------------------------------------------------------------------------
    // Vérifications de cohérence pour éviter les configurations invalides.

    #if NKENTSEU_CONTAINERS_STRING_SSO_SIZE < 8
        #warning "NKENTSEU_CONTAINERS_STRING_SSO_SIZE < 8 : SSO inefficace, valeur minimale recommandée : 16"
    #endif

    #if defined(NKENTSEU_CONTAINERS_HEADER_ONLY) && !defined(NKENTSEU_CONTAINERS_USE_CPP11)
        #error "NKContainers en mode header-only nécessite au minimum C++11"
    #endif

    // -------------------------------------------------------------------------
    // SECTION 10 : MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #pragma message("NKContainers Compat Config:")
        #pragma message("  C++ Standard: " NKENTSEU_STRINGIZE(NKENTSEU_CXX_STANDARD))
        #pragma message("  SSO Size: " NKENTSEU_STRINGIZE(NKENTSEU_CONTAINERS_STRING_SSO_SIZE))
        #pragma message("  Default Alignment: " NKENTSEU_STRINGIZE(NKENTSEU_CONTAINERS_DEFAULT_ALIGNMENT))
        #if defined(NKENTSEU_CONTAINERS_USE_CPP17)
            #pragma message("  C++17 features: ENABLED")
        #endif
        #if defined(NKENTSEU_CONTAINERS_USE_CPP20)
            #pragma message("  C++20 features: ENABLED")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_NKCOMPAT_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
    // Exemple 1 : Utilisation basique des macros de compatibilité
    #include "NKContainers/NkCompat.h"

    class NKENTSEU_CONTAINERS_CLASS_EXPORT MyContainer {
    public:
        // Fonction inline avec macro de compatibilité
        NKENTSEU_CONTAINERS_FORCE_INLINE void Push(int value) NKENTSEU_CONTAINERS_NOEXCEPT {
            NKENTSEU_CONTAINERS_ASSERT_MSG(value >= 0, "Value must be non-negative");
            // ... implémentation ...
        }

        // Utilisation de constexpr via macro
        NKENTSEU_CONTAINERS_CONSTEXPR static size_t MaxSize() {
            return 1024;
        }
    };

    // Exemple 2 : Configuration personnalisée via CMake
    // -DNKENTSEU_CONTAINERS_STRING_SSO_SIZE=32
    // -DNKENTSEU_CONTAINERS_DEFAULT_ALIGNMENT=32
    // -DNKENTSEU_CONTAINERS_LEGACY_ALIAS_DISABLE

    // Exemple 3 : Utilisation de NKENTSEU_CONTAINERS_SWAP
    template<typename T>
    void NKENTSEU_CONTAINERS_API SwapElements(T& a, T& b) {
        NKENTSEU_CONTAINERS_SWAP(a, b);  // Swap optimisé
    }

    // Exemple 4 : Boucle foreach portable
    void ProcessAll(nkentseu::containers::Vector<int>& vec) {
        NKENTSEU_CONTAINERS_FOREACH(value, vec) {
            // value est une référence à chaque élément
            Process(value);
        }
    }

    // Exemple 5 : Allocation alignée pour conteneurs SIMD
    float* NKENTSEU_CONTAINERS_API AllocSimdBuffer(size_t count) {
        constexpr size_t alignment = 32;  // AVX
        size_t size = count * sizeof(float);
        return static_cast<float*>(
            NKENTSEU_CONTAINERS_ALLOC_ALIGNED(alignment, size)
        );
    }

    void NKENTSEU_CONTAINERS_API FreeSimdBuffer(float* ptr) {
        NKENTSEU_CONTAINERS_FREE_ALIGNED(ptr);
    }
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis l'ancienne API "NK_*" vers "NKENTSEU_CONTAINERS_*" :

    Ancien code (déprécié)          | Nouveau code (recommandé)
    --------------------------------|----------------------------------
    NK_CPP11                        | NKENTSEU_CXX11_OR_LATER
    NK_CONSTEXPR                    | NKENTSEU_CONTAINERS_CONSTEXPR
    NK_NOEXCEPT                     | NKENTSEU_CONTAINERS_NOEXCEPT
    NK_ASSERT(expr)                 | NKENTSEU_CONTAINERS_ASSERT(expr)
    NK_STRING_SSO_SIZE              | NKENTSEU_CONTAINERS_STRING_SSO_SIZE
    NkSwap(a, b)                    | NKENTSEU_CONTAINERS_SWAP(a, b)
    NkAllocAligned(al, sz)          | NKENTSEU_CONTAINERS_ALLOC_ALIGNED(al, sz)

    Pour désactiver les warnings de migration :
    #define NKENTSEU_CONTAINERS_SUPPRESS_LEGACY_WARNINGS
    #include "NKContainers/NkCompat.h"
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================