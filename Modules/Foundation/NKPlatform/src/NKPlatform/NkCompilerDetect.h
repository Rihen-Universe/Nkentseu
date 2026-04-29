// =============================================================================
// NKPlatform/NkCompilerDetect.h
// Détection du compilateur et des fonctionnalités du standard C++.
//
// Design :
//  - Identification du compilateur (MSVC, GCC, Clang, Intel, etc.) et version
//  - Détection du standard C++ utilisé (C++11 à C++23)
//  - Macros de convenance pour les fonctionnalités C++ (constexpr, noexcept, etc.)
//  - Attributs spécifiques au compilateur (packed, restrict, thread_local, etc.)
//  - Gestion portable des pragmas et warnings
//  - Aucune dépendance externe — header-only compatible C/C++
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_NKCOMPILERDETECT_H
#define NKENTSEU_PLATFORM_NKCOMPILERDETECT_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES STANDARD NÉCESSAIRES
    // -------------------------------------------------------------------------
    // Ce fichier est header-only et ne dépend d'aucun autre en-tête personnalisé.
    // Les en-têtes standard sont inclus uniquement si nécessaire par l'utilisateur.

    // -------------------------------------------------------------------------
    // SECTION 2 : DOCUMENTATION DES GROUPES DOXYGEN
    // -------------------------------------------------------------------------
    // Définition des groupes Doxygen pour une documentation structurée.

    /**
     * @defgroup CompilerDetection Détection de Compilateur
     * @brief Macros pour identifier le compilateur et sa version
     *
     * Ce groupe contient les macros permettant de détecter le compilateur utilisé
     * lors de la compilation, ainsi que sa version numérique.
     *
     * @see NKENTSEU_COMPILER_MSVC
     * @see NKENTSEU_COMPILER_GCC
     * @see NKENTSEU_COMPILER_CLANG
     */

    /**
     * @defgroup CppStandards Standards C++
     * @brief Macros pour identifier la version du standard C++
     *
     * Ce groupe contient les macros indiquant quelle version du standard C++
     * est supportée par le compilateur (de C++98 à C++23).
     */

    /**
     * @defgroup Cpp11Features Fonctionnalités C++11
     * @brief Macros indiquant la disponibilité des fonctionnalités C++11
     */

    /**
     * @defgroup Cpp14Features Fonctionnalités C++14
     * @brief Macros indiquant la disponibilité des fonctionnalités C++14
     */

    /**
     * @defgroup Cpp17Features Fonctionnalités C++17
     * @brief Macros indiquant la disponibilité des fonctionnalités C++17
     */

    /**
     * @defgroup Cpp20Features Fonctionnalités C++20
     * @brief Macros indiquant la disponibilité des fonctionnalités C++20
     */

    /**
     * @defgroup Cpp23Features Fonctionnalités C++23
     * @brief Macros indiquant la disponibilité des fonctionnalités C++23
     */

    /**
     * @defgroup CppConvenience Conveniences C++
     * @brief Macros pratiques pour utiliser les fonctionnalités C++ de manière portable
     */

    /**
     * @defgroup CompilerAttributes Attributs Compilateur
     * @brief Macros pour les attributs spécifiques au compilateur
     */

    /**
     * @defgroup StandardMacros Macros Standard
     * @brief Macros fournies nativement par le compilateur
     */

    /**
     * @defgroup SpecialCapabilities Capacités Spéciales
     * @brief Détection de fonctionnalités spécifiques au compilateur
     */

    /**
     * @defgroup Pragmas Pragmas
     * @brief Macros pour manipuler les pragmas de manière portable
     */

    // =========================================================================
    // SECTION 3 : DÉTECTION DU COMPILATEUR
    // =========================================================================
    // Identification du compilateur via ses macros prédéfinies.
    // Chaque compilateur définit des macros uniques permettant son identification.

    /**
     * @brief Macro de détection MSVC (Microsoft Visual C++)
     * @def NKENTSEU_COMPILER_MSVC
     * @ingroup CompilerDetection
     *
     * Définie lorsque le code est compilé avec MSVC (Visual Studio).
     * La version est accessible via NKENTSEU_COMPILER_VERSION (_MSC_VER).
     *
     * @example
     * @code
     * #ifdef NKENTSEU_COMPILER_MSVC
     *     // Code spécifique à MSVC
     *     #pragma warning(disable: 4100)
     * #endif
     * @endcode
     */

    /**
     * @brief Version numérique du compilateur
     * @def NKENTSEU_COMPILER_VERSION
     * @ingroup CompilerDetection
     * @return Valeur numérique représentant la version du compilateur
     *
     * Format variable selon le compilateur :
     * - MSVC : _MSC_VER (ex: 1930 pour VS 2022)
     * - GCC/Clang : MAJOR*10000 + MINOR*100 + PATCH (ex: 110200 pour GCC 11.2.0)
     * - Intel : __INTEL_COMPILER (ex: 2021 pour ICC 2021)
     */

    // -------------------------------------------------------------------------
    // Sous-section : Microsoft Visual C++ (MSVC)
    // -------------------------------------------------------------------------
    // Détection via _MSC_VER, macro définie par tous les compilateurs compatibles MSVC.
    // Inclut MinGW-w64 en mode MSVC compatibility.

    #if defined(_MSC_VER)
        #define NKENTSEU_COMPILER_MSVC
        #define NKENTSEU_COMPILER_VERSION _MSC_VER

        /**
         * @brief MSVC 2024 ou supérieur (VS 17.10+)
         * @def NKENTSEU_COMPILER_MSVC_2024
         * @ingroup CompilerDetection
         *
         * _MSC_VER >= 1940 correspond à Visual Studio 2024 version 17.10 ou ultérieure.
         */
        #if _MSC_VER >= 1940
            #define NKENTSEU_COMPILER_MSVC_2024
        #endif

        /**
         * @brief MSVC 2022 ou supérieur (VS 17.0+)
         * @def NKENTSEU_COMPILER_MSVC_2022
         * @ingroup CompilerDetection
         *
         * _MSC_VER >= 1930 correspond à Visual Studio 2022.
         */
        #if _MSC_VER >= 1930
            #define NKENTSEU_COMPILER_MSVC_2022
        #endif

        /**
         * @brief MSVC 2019 ou supérieur (VS 16.0+)
         * @def NKENTSEU_COMPILER_MSVC_2019
         * @ingroup CompilerDetection
         *
         * _MSC_VER >= 1920 correspond à Visual Studio 2019.
         */
        #if _MSC_VER >= 1920
            #define NKENTSEU_COMPILER_MSVC_2019
        #endif

        /**
         * @brief MSVC 2017 ou supérieur (VS 15.0+)
         * @def NKENTSEU_COMPILER_MSVC_2017
         * @ingroup CompilerDetection
         *
         * _MSC_VER >= 1910 correspond à Visual Studio 2017.
         */
        #if _MSC_VER >= 1910
            #define NKENTSEU_COMPILER_MSVC_2017
        #endif

    // -------------------------------------------------------------------------
    // Sous-section : Clang (LLVM) et Apple Clang
    // -------------------------------------------------------------------------
    // Clang définit __clang__. Apple Clang définit en plus __apple_build_version__.

    #elif defined(__clang__)
        /**
         * @brief Détection du compilateur Clang (LLVM)
         * @def NKENTSEU_COMPILER_CLANG
         * @ingroup CompilerDetection
         *
         * Définie pour Clang/LLVM, y compris Apple Clang.
         * Utiliser NKENTSEU_COMPILER_APPLE_CLANG pour distinguer Apple Clang.
         */
        #ifndef NKENTSEU_COMPILER_CLANG
            #define NKENTSEU_COMPILER_CLANG
        #endif

        // Version encodée : MAJOR*10000 + MINOR*100 + PATCH
        // Exemple : Clang 14.0.0 -> 140000
        #define NKENTSEU_COMPILER_VERSION ( \
            __clang_major__ * 10000 + \
            __clang_minor__ * 100 + \
            __clang_patchlevel__ \
        )

        /**
         * @brief Détection d'Apple Clang
         * @def NKENTSEU_COMPILER_APPLE_CLANG
         * @ingroup CompilerDetection
         *
         * Définie lorsque Clang est la version fournie par Apple (Xcode toolchain).
         * Apple Clang utilise un schéma de versionnage différent de Clang upstream.
         */
        #if defined(__apple_build_version__)
            #define NKENTSEU_COMPILER_APPLE_CLANG
        #endif

    // -------------------------------------------------------------------------
    // Sous-section : GCC (GNU Compiler Collection)
    // -------------------------------------------------------------------------
    // GCC définit __GNUC__. On exclut Clang car il définit aussi __GNUC__ pour compatibilité.

    #elif defined(__GNUC__) && !defined(__clang__)
        /**
         * @brief Détection du compilateur GCC
         * @def NKENTSEU_COMPILER_GCC
         * @ingroup CompilerDetection
         *
         * Définie pour GCC (GNU Compiler Collection) version 4.x et ultérieure.
         * Exclut Clang qui définit aussi __GNUC__ pour la compatibilité.
         */
        #define NKENTSEU_COMPILER_GCC

        // Version encodée : MAJOR*10000 + MINOR*100 + PATCH
        // Exemple : GCC 11.3.0 -> 110300
        #define NKENTSEU_COMPILER_VERSION ( \
            __GNUC__ * 10000 + \
            __GNUC_MINOR__ * 100 + \
            __GNUC_PATCHLEVEL__ \
        )

    // -------------------------------------------------------------------------
    // Sous-section : Intel C++ Compiler (ICC/ICX)
    // -------------------------------------------------------------------------
    // Intel Compiler définit plusieurs macros selon la version et le mode.

    #elif defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
        /**
         * @brief Détection du compilateur Intel C++
         * @def NKENTSEU_COMPILER_INTEL
         * @ingroup CompilerDetection
         *
         * Définie pour Intel C++ Compiler (ICC) et Intel LLVM-based Compiler (ICX).
         * __INTEL_COMPILER contient la version encodée (ex: 2021 pour ICX 2021).
         */
        #define NKENTSEU_COMPILER_INTEL
        #define NKENTSEU_COMPILER_VERSION __INTEL_COMPILER
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Compilateurs spécialisés / embarqués
    // -------------------------------------------------------------------------

    // Emscripten (WebAssembly)
    #if defined(__EMSCRIPTEN__)
        /**
         * @brief Détection du compilateur Emscripten
         * @def NKENTSEU_COMPILER_EMSCRIPTEN
         * @ingroup CompilerDetection
         *
         * Définie lors de la compilation vers WebAssembly via Emscripten.
         * Emscripten utilise Clang en backend mais nécessite une détection séparée
         * pour gérer les spécificités Web (pas de threads natifs, mémoire limitée, etc.).
         */
        #define NKENTSEU_COMPILER_EMSCRIPTEN
    #endif

    // NVIDIA CUDA Compiler (NVCC)
    #if defined(__NVCC__)
        /**
         * @brief Détection du compilateur NVCC (CUDA)
         * @def NKENTSEU_COMPILER_NVCC
         * @ingroup CompilerDetection
         *
         * Définie lors de la compilation de fichiers .cu avec NVCC.
         * NVCC peut utiliser GCC, Clang ou MSVC en backend pour le code hôte.
         */
        #define NKENTSEU_COMPILER_NVCC
    #endif

    // Oracle/Sun Studio Compiler
    #if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
        /**
         * @brief Détection du compilateur SunPro/Oracle
         * @def NKENTSEU_COMPILER_SUNPRO
         * @ingroup CompilerDetection
         *
         * Définie pour Oracle Developer Studio (anciennement Sun Studio).
         * Principalement utilisé sur Solaris/SPARC.
         */
        #define NKENTSEU_COMPILER_SUNPRO
    #endif

    // IBM XL C/C++ Compiler
    #if defined(__xlC__) || defined(__xlc__)
        /**
         * @brief Détection du compilateur IBM XL C/C++
         * @def NKENTSEU_COMPILER_XLC
         * @ingroup CompilerDetection
         *
         * Définie pour IBM XL C/C++ Compiler (AIX, Linux on Power, z/OS).
         */
        #define NKENTSEU_COMPILER_XLC
    #endif

    // =========================================================================
    // SECTION 4 : DÉTECTION DU STANDARD C++
    // =========================================================================
    // Détection de la version du standard C++ via __cplusplus.
    // Attention : MSVC nécessite /Zc:__cplusplus pour une détection correcte.

    /**
     * @brief Standard C++23 ou supérieur
     * @def NKENTSEU_CPP23
     * @ingroup CppStandards
     *
     * Définie si le compilateur supporte C++23 (__cplusplus >= 202302L).
     * Inclut les fonctionnalités : deducing this, if consteval, multidimensional subscript.
     */

    /**
     * @brief Standard C++20 ou supérieur
     * @def NKENTSEU_CPP20
     * @ingroup CppStandards
     *
     * Définie si le compilateur supporte C++20 (__cplusplus >= 202002L).
     * Inclut les fonctionnalités : concepts, modules, coroutines, three-way comparison.
     */

    /**
     * @brief Standard C++17 ou supérieur
     * @def NKENTSEU_CPP17
     * @ingroup CppStandards
     *
     * Définie si le compilateur supporte C++17 (__cplusplus >= 201703L).
     * Inclut les fonctionnalités : structured bindings, if constexpr, fold expressions.
     */

    /**
     * @brief Standard C++14 ou supérieur
     * @def NKENTSEU_CPP14
     * @ingroup CppStandards
     *
     * Définie si le compilateur supporte C++14 (__cplusplus >= 201402L).
     * Inclut les fonctionnalités : generic lambdas, variable templates, binary literals.
     */

    /**
     * @brief Standard C++11 ou supérieur
     * @def NKENTSEU_CPP11
     * @ingroup CppStandards
     *
     * Définie si le compilateur supporte C++11 (__cplusplus >= 201103L).
     * Minimum requis pour NKentseu. Inclut : auto, nullptr, lambda, constexpr, etc.
     */

    /**
     * @brief Standard C++98/C++03
     * @def NKENTSEU_CPP98
     * @ingroup CppStandards
     *
     * Définie si le compilateur ne supporte que C++98/03 (__cplusplus >= 199711L).
     * Non supporté par NKentseu — une erreur de compilation est émise.
     */

    // -------------------------------------------------------------------------
    // Sous-section : Détection hiérarchique du standard C++
    // -------------------------------------------------------------------------
    // Détection du plus récent vers le plus ancien pour garantir la macro la plus spécifique.

    #if __cplusplus >= 202302L
        #define NKENTSEU_CPP23
        #define NKENTSEU_CPP_VERSION 23

    #elif __cplusplus >= 202002L
        #ifndef NKENTSEU_CPP20
            #define NKENTSEU_CPP20
        #endif
        #define NKENTSEU_CPP_VERSION 20

    #elif __cplusplus >= 201703L
        #define NKENTSEU_CPP17
        #define NKENTSEU_CPP_VERSION 17

    #elif __cplusplus >= 201402L
        #define NKENTSEU_CPP14
        #define NKENTSEU_CPP_VERSION 14

    #elif __cplusplus >= 201103L
        #define NKENTSEU_CPP11
        #define NKENTSEU_CPP_VERSION 11

    #elif __cplusplus >= 199711L
        #define NKENTSEU_CPP98
        #define NKENTSEU_CPP_VERSION 98

    #else
        // Standard trop ancien ou non détecté — arrêt de la compilation
        #error "C++ standard not detected - minimum required is C++11"
    #endif

    // =========================================================================
    // SECTION 5 : FONCTIONNALITÉS C++11
    // =========================================================================
    // Macros indiquant la disponibilité des fonctionnalités introduites en C++11.
    // Ces macros sont définies automatiquement si le standard C++11+ est détecté.

    /**
     * @brief Support général de C++11
     * @def NKENTSEU_HAS_CPP11
     * @ingroup Cpp11Features
     *
     * Macro de convenance indiquant que toutes les fonctionnalités C++11
     * listées ci-dessous sont disponibles.
     */

    // -------------------------------------------------------------------------
    // Sous-section : Définition des macros C++11
    // -------------------------------------------------------------------------

    #if defined(NKENTSEU_CPP11) || defined(NKENTSEU_CPP14) || \
        defined(NKENTSEU_CPP17) || defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)

        #define NKENTSEU_HAS_CPP11

        /**
         * @brief Support du mot-clé nullptr
         * @def NKENTSEU_HAS_NULLPTR
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_NULLPTR

        /**
         * @brief Support du mot-clé auto pour l'inférence de type
         * @def NKENTSEU_HAS_AUTO
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_AUTO

        /**
         * @brief Support de decltype pour l'inspection de type
         * @def NKENTSEU_HAS_DECLTYPE
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_DECLTYPE

        /**
         * @brief Support des références rvalue et du move semantics
         * @def NKENTSEU_HAS_RVALUE_REFERENCES
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_RVALUE_REFERENCES

        /**
         * @brief Support des templates variadiques
         * @def NKENTSEU_HAS_VARIADIC_TEMPLATES
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_VARIADIC_TEMPLATES

        /**
         * @brief Support de static_assert pour les assertions à la compilation
         * @def NKENTSEU_HAS_STATIC_ASSERT
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_STATIC_ASSERT

        /**
         * @brief Support de constexpr pour l'évaluation à la compilation
         * @def NKENTSEU_HAS_CONSTEXPR
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_CONSTEXPR

        /**
         * @brief Support de noexcept pour spécifier l'absence d'exceptions
         * @def NKENTSEU_HAS_NOEXCEPT
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_NOEXCEPT

        /**
         * @brief Support du spécificateur override pour les méthodes virtuelles
         * @def NKENTSEU_HAS_OVERRIDE
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_OVERRIDE

        /**
         * @brief Support du spécificateur final pour empêcher l'héritage
         * @def NKENTSEU_HAS_FINAL
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_FINAL

        /**
         * @brief Support des fonctions default/delete
         * @def NKENTSEU_HAS_DEFAULT_DELETE
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_DEFAULT_DELETE

        /**
         * @brief Support des expressions lambda
         * @def NKENTSEU_HAS_LAMBDA
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_LAMBDA

        /**
         * @brief Support des boucles range-based for
         * @def NKENTSEU_HAS_RANGE_FOR
         * @ingroup Cpp11Features
         */
        #define NKENTSEU_HAS_RANGE_FOR

    #endif

    // =========================================================================
    // SECTION 6 : FONCTIONNALITÉS C++14
    // =========================================================================

    /**
     * @brief Support général de C++14
     * @def NKENTSEU_HAS_CPP14
     * @ingroup Cpp14Features
     */

    #if defined(NKENTSEU_CPP14) || defined(NKENTSEU_CPP17) || \
        defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)

        #define NKENTSEU_HAS_CPP14

        /**
         * @brief Support des lambdas génériques (paramètres auto)
         * @def NKENTSEU_HAS_GENERIC_LAMBDAS
         * @ingroup Cpp14Features
         */
        #define NKENTSEU_HAS_GENERIC_LAMBDAS

        /**
         * @brief Support des variables templates
         * @def NKENTSEU_HAS_VARIABLE_TEMPLATES
         * @ingroup Cpp14Features
         */
        #define NKENTSEU_HAS_VARIABLE_TEMPLATES

        /**
         * @brief Support de constexpr relaxé (plus d'expressions autorisées)
         * @def NKENTSEU_HAS_RELAXED_CONSTEXPR
         * @ingroup Cpp14Features
         */
        #define NKENTSEU_HAS_RELAXED_CONSTEXPR

        /**
         * @brief Support des littéraux binaires (0b1010)
         * @def NKENTSEU_HAS_BINARY_LITERALS
         * @ingroup Cpp14Features
         */
        #define NKENTSEU_HAS_BINARY_LITERALS

        /**
         * @brief Support de decltype(auto) pour l'inférence de type parfaite
         * @def NKENTSEU_HAS_DECLTYPE_AUTO
         * @ingroup Cpp14Features
         */
        #define NKENTSEU_HAS_DECLTYPE_AUTO

    #endif

    // =========================================================================
    // SECTION 7 : FONCTIONNALITÉS C++17
    // =========================================================================

    /**
     * @brief Support général de C++17
     * @def NKENTSEU_HAS_CPP17
     * @ingroup Cpp17Features
     */

    #if defined(NKENTSEU_CPP17) || defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)

        #define NKENTSEU_HAS_CPP17

        /**
         * @brief Support des variables inline (définition dans les headers)
         * @def NKENTSEU_HAS_INLINE_VARIABLES
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_INLINE_VARIABLES

        /**
         * @brief Support des fold expressions pour les templates variadiques
         * @def NKENTSEU_HAS_FOLD_EXPRESSIONS
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_FOLD_EXPRESSIONS

        /**
         * @brief Support de if constexpr pour la compilation conditionnelle
         * @def NKENTSEU_HAS_IF_CONSTEXPR
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_IF_CONSTEXPR

        /**
         * @brief Support des structured bindings pour la décomposition de tuples/structs
         * @def NKENTSEU_HAS_STRUCTURED_BINDINGS
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_STRUCTURED_BINDINGS

        /**
         * @brief Alias pour NKENTSEU_HAS_IF_CONSTEXPR (cohérence de nommage)
         * @def NKENTSEU_HAS_CONSTEXPR_IF
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_CONSTEXPR_IF

        /**
         * @brief Support de l'attribut [[nodiscard]]
         * @def NKENTSEU_HAS_NODISCARD
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_NODISCARD

        /**
         * @brief Support de l'attribut [[maybe_unused]]
         * @def NKENTSEU_HAS_MAYBE_UNUSED
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_MAYBE_UNUSED

        /**
         * @brief Support de l'attribut [[fallthrough]] pour les switch
         * @def NKENTSEU_HAS_FALLTHROUGH
         * @ingroup Cpp17Features
         */
        #define NKENTSEU_HAS_FALLTHROUGH

    #endif

    // =========================================================================
    // SECTION 8 : FONCTIONNALITÉS C++20
    // =========================================================================

    /**
     * @brief Support général de C++20
     * @def NKENTSEU_HAS_CPP20
     * @ingroup Cpp20Features
     */

    #if defined(NKENTSEU_CPP20) || defined(NKENTSEU_CPP23)

        #define NKENTSEU_HAS_CPP20

        /**
         * @brief Support des concepts pour la contrainte de templates
         * @def NKENTSEU_HAS_CONCEPTS
         * @ingroup Cpp20Features
         */
        #define NKENTSEU_HAS_CONCEPTS

        /**
         * @brief Support des modules pour une compilation modulaire
         * @def NKENTSEU_HAS_MODULES
         * @ingroup Cpp20Features
         * @note Les modules nécessitent un support explicite du build system.
         */
        #define NKENTSEU_HAS_MODULES

        /**
         * @brief Support des coroutines pour la programmation asynchrone
         * @def NKENTSEU_HAS_COROUTINES
         * @ingroup Cpp20Features
         * @note Nécessite également les bibliothèques runtime appropriées.
         */
        #define NKENTSEU_HAS_COROUTINES

        /**
         * @brief Support de constexpr sur les fonctions virtuelles
         * @def NKENTSEU_HAS_CONSTEXPR_VIRTUAL
         * @ingroup Cpp20Features
         */
        #define NKENTSEU_HAS_CONSTEXPR_VIRTUAL

        /**
         * @brief Support de consteval pour l'évaluation immédiate à la compilation
         * @def NKENTSEU_HAS_CONSTEVAL
         * @ingroup Cpp20Features
         */
        #define NKENTSEU_HAS_CONSTEVAL

        /**
         * @brief Support de constinit pour l'initialisation statique garantie
         * @def NKENTSEU_HAS_CONSTINIT
         * @ingroup Cpp20Features
         */
        #define NKENTSEU_HAS_CONSTINIT

        /**
         * @brief Support de l'opérateur de comparaison à trois voies (<=>)
         * @def NKENTSEU_HAS_THREE_WAY_COMPARISON
         * @ingroup Cpp20Features
         */
        #define NKENTSEU_HAS_THREE_WAY_COMPARISON

        /**
         * @brief Support des designated initializers (initialiseurs nommés)
         * @def NKENTSEU_HAS_DESIGNATED_INITIALIZERS
         * @ingroup Cpp20Features
         */
        #define NKENTSEU_HAS_DESIGNATED_INITIALIZERS

    #endif

    // =========================================================================
    // SECTION 9 : FONCTIONNALITÉS C++23
    // =========================================================================

    /**
     * @brief Support général de C++23
     * @def NKENTSEU_HAS_CPP23
     * @ingroup Cpp23Features
     */

    #if defined(NKENTSEU_CPP23)

        #define NKENTSEU_HAS_CPP23

        /**
         * @brief Support de 'deducing this' pour les méthodes explicites
         * @def NKENTSEU_HAS_DEDUCING_THIS
         * @ingroup Cpp23Features
         */
        #define NKENTSEU_HAS_DEDUCING_THIS

        /**
         * @brief Support de if consteval pour la détection constexpr à l'exécution
         * @def NKENTSEU_HAS_IF_CONSTEVAL
         * @ingroup Cpp23Features
         */
        #define NKENTSEU_HAS_IF_CONSTEVAL

        /**
         * @brief Support des subscript multidimensionnels (obj[1, 2, 3])
         * @def NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT
         * @ingroup Cpp23Features
         */
        #define NKENTSEU_HAS_MULTIDIMENSIONAL_SUBSCRIPT

    #endif

    // =========================================================================
    // SECTION 10 : MACROS DE CONVENANCE POUR LES FONCTIONNALITÉS C++
    // =========================================================================
    // Macros portables qui s'adaptent au standard détecté pour une utilisation
    // uniforme des fonctionnalités C++ modernes.

    /**
     * @brief Macro portable pour constexpr
     * @def NKENTSEU_CONSTEXPR
     * @ingroup CppConvenience
     *
     * S'étend à 'constexpr' si supporté, sinon vide.
     * Permet d'écrire du code compatible avec des standards anciens.
     *
     * @example
     * @code
     * NKENTSEU_CONSTEXPR int Square(int x) { return x * x; }
     * @endcode
     */
    #if defined(NKENTSEU_HAS_CONSTEXPR)
        #define NKENTSEU_CONSTEXPR constexpr
    #else
        #define NKENTSEU_CONSTEXPR
    #endif

    /**
     * @brief Macro portable pour noexcept
     * @def NKENTSEU_NOEXCEPT
     * @ingroup CppConvenience
     *
     * S'étend à 'noexcept' si supporté, sinon à 'throw()' pour C++98.
     * Indique que la fonction ne lève pas d'exceptions.
     */
    #if defined(NKENTSEU_HAS_NOEXCEPT)
        #define NKENTSEU_NOEXCEPT noexcept

        /**
         * @brief Macro pour noexcept conditionnel
         * @def NKENTSEU_NOEXCEPT_IF
         * @param condition Expression booléenne évaluée à la compilation
         * @ingroup CppConvenience
         */
        #define NKENTSEU_NOEXCEPT_IF(condition) noexcept(condition)
    #else
        #define NKENTSEU_NOEXCEPT throw()
        #define NKENTSEU_NOEXCEPT_IF(condition)
    #endif

    /**
     * @brief Macro portable pour override
     * @def NKENTSEU_OVERRIDE
     * @ingroup CppConvenience
     *
     * Spécifie qu'une méthode virtuelle redéfinit une méthode de la classe de base.
     * Génère une erreur de compilation si aucune méthode correspondante n'existe.
     */
    #if defined(NKENTSEU_HAS_OVERRIDE)
        #define NKENTSEU_OVERRIDE override
    #else
        #define NKENTSEU_OVERRIDE
    #endif

    /**
     * @brief Macro portable pour final
     * @def NKENTSEU_FINAL
     * @ingroup CppConvenience
     *
     * Empêche la redéfinition d'une méthode virtuelle ou l'héritage d'une classe.
     */
    #if defined(NKENTSEU_HAS_FINAL)
        #define NKENTSEU_FINAL final
    #else
        #define NKENTSEU_FINAL
    #endif

    /**
     * @brief Macro portable pour [[nodiscard]]
     * @def NKENTSEU_NODISCARD
     * @ingroup CppConvenience
     *
     * Indique que la valeur de retour d'une fonction ne doit pas être ignorée.
     * Génère un warning si l'appelant ignore le résultat.
     *
     * @see NKENTSEU_NODISCARD_MSG pour un message personnalisé
     */
    #if defined(__cplusplus) && __cplusplus >= 201703L
        #define NKENTSEU_NODISCARD [[nodiscard]]

        /**
         * @brief [[nodiscard]] avec message personnalisé
         * @def NKENTSEU_NODISCARD_MSG
         * @param msg Chaîne expliquant pourquoi la valeur ne doit pas être ignorée
         * @ingroup CppConvenience
         */
        #define NKENTSEU_NODISCARD_MSG(msg) [[nodiscard(msg)]]

        /**
         * @brief Alias simple pour [[nodiscard]]
         * @def NKENTSEU_NODISCARD_SIMPLE
         * @ingroup CppConvenience
         */
        #define NKENTSEU_NODISCARD_SIMPLE [[nodiscard]]
    #else
        #define NKENTSEU_NODISCARD
        #define NKENTSEU_NODISCARD_MSG(msg)
        #define NKENTSEU_NODISCARD_SIMPLE
    #endif

    /**
     * @brief Macro portable pour [[maybe_unused]]
     * @def NKENTSEU_MAYBE_UNUSED
     * @ingroup CppConvenience
     *
     * Indique qu'un paramètre ou variable peut ne pas être utilisé.
     * Supprime les warnings "unused variable" sur les builds release.
     */
    #if defined(NKENTSEU_HAS_MAYBE_UNUSED)
        #define NKENTSEU_MAYBE_UNUSED [[maybe_unused]]
    #else
        #define NKENTSEU_MAYBE_UNUSED
    #endif

    /**
     * @brief Macro portable pour [[fallthrough]]
     * @def NKENTSEU_FALLTHROUGH
     * @ingroup CppConvenience
     *
     * Indique intentionnellement un fall-through dans un switch.
     * Supprime les warnings liés aux cases non breakées.
     */
    #if defined(NKENTSEU_HAS_FALLTHROUGH)
        #define NKENTSEU_FALLTHROUGH [[fallthrough]]
    #else
        #define NKENTSEU_FALLTHROUGH
    #endif

    // =========================================================================
    // SECTION 11 : ATTRIBUTS SPÉCIFIQUES AU COMPILATEUR
    // =========================================================================
    // Macros pour les attributs non standard mais largement supportés,
    // utiles pour l'optimisation et la compatibilité binaire.

    /**
     * @brief Structure packed (sans padding mémoire)
     * @def NKENTSEU_PACKED
     * @ingroup CompilerAttributes
     *
     * Supprime le padding entre les membres d'une structure.
     * Utile pour les formats binaires, les protocoles réseau, les headers de fichiers.
     *
     * @warning L'accès aux membres non-alignés peut être lent ou provoquer un crash
     * sur certaines architectures (ARM, etc.). Utiliser avec précaution.
     *
     * @example
     * @code
     * struct NKENTSEU_PACKED NetworkHeader {
     *     uint16_t type;
     *     uint32_t length;
     *     // Pas de padding entre type et length
     * };
     * @endcode
     */

    /**
     * @brief Début de bloc packed avec push/pop
     * @def NKENTSEU_PACK_BEGIN
     * @ingroup CompilerAttributes
     * @see NKENTSEU_PACK_END
     *
     * Alternative portable à NKENTSEU_PACKED pour MSVC qui utilise #pragma pack.
     */

    /**
     * @brief Fin de bloc packed
     * @def NKENTSEU_PACK_END
     * @ingroup CompilerAttributes
     * @see NKENTSEU_PACK_BEGIN
     */

    #if defined(NKENTSEU_COMPILER_MSVC)
        // MSVC : pas d'attribut, utilisation de #pragma pack
        #define NKENTSEU_PACKED
        #define NKENTSEU_PACK_BEGIN __pragma(pack(push, 1))
        #define NKENTSEU_PACK_END __pragma(pack(pop))
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        // GCC/Clang : attribut packed
        #define NKENTSEU_PACKED __attribute__((packed))
        #define NKENTSEU_PACK_BEGIN
        #define NKENTSEU_PACK_END
    #else
        // Compilateur inconnu : fallback sans packing (risque de padding)
        #define NKENTSEU_PACKED
        #define NKENTSEU_PACK_BEGIN
        #define NKENTSEU_PACK_END
    #endif

    /**
     * @brief Mot-clé restrict pour les pointeurs sans aliasing
     * @def NKENTSEU_RESTRICT
     * @ingroup CompilerAttributes
     *
     * Indique au compilateur que le pointeur est le seul moyen d'accéder
     * à la mémoire pointée pendant sa durée de vie. Permet des optimisations agressives.
     *
     * @example
     * @code
     * void CopyBuffer(
     *     float* NKENTSEU_RESTRICT dest,
     *     const float* NKENTSEU_RESTRICT src,
     *     size_t count
     * );
     * @endcode
     */
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_RESTRICT __restrict
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_RESTRICT __restrict__
    #else
        #define NKENTSEU_RESTRICT
    #endif

    /**
     * @brief Stockage thread-local (variable par thread)
     * @def NKENTSEU_THREAD_LOCAL
     * @ingroup CompilerAttributes
     *
     * Déclare une variable dont chaque thread possède sa propre instance.
     * Utile pour les caches thread-local, les états globaux thread-safe.
     *
     * @example
     * @code
     * NKENTSEU_THREAD_LOCAL int g_ThreadLocalCounter = 0;
     * @endcode
     */
    #if defined(NKENTSEU_CPP11)
        #define NKENTSEU_THREAD_LOCAL thread_local
    #elif defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_THREAD_LOCAL __declspec(thread)
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_THREAD_LOCAL __thread
    #else
        #define NKENTSEU_THREAD_LOCAL
    #endif

    /**
     * @brief Attribut deprecated avec message optionnel
     * @def NKENTSEU_DEPRECATED
     * @ingroup CompilerAttributes
     * @see NKENTSEU_DEPRECATED_MSG
     *
     * Marque une fonction/classe comme obsolète. Génère un warning à l'utilisation.
     */

    /**
     * @brief Attribut deprecated avec message personnalisé
     * @def NKENTSEU_DEPRECATED_MSG
     * @param msg Message expliquant l'alternative recommandée
     * @ingroup CompilerAttributes
     *
     * @example
     * @code
     * NKENTSEU_DEPRECATED_MSG("Utiliser NewFunction() à la place")
     * void OldFunction();
     * @endcode
     */
    #if defined(NKENTSEU_CPP14)
        #define NKENTSEU_DEPRECATED [[deprecated]]
        #define NKENTSEU_DEPRECATED_MSG(msg) [[deprecated(msg)]]
    #elif defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_DEPRECATED __declspec(deprecated)
        #define NKENTSEU_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_DEPRECATED __attribute__((deprecated))
        #define NKENTSEU_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
    #else
        #define NKENTSEU_DEPRECATED
        #define NKENTSEU_DEPRECATED_MSG(msg)
    #endif

    // =========================================================================
    // SECTION 12 : MACROS STANDARD FOURNIES PAR LE COMPILATEUR
    // =========================================================================
    // Wrappers portables autour des macros prédéfinies par les compilateurs.

    /**
     * @brief Nom signé de la fonction actuelle (avec signature complète)
     * @def NKENTSEU_FUNCTION_NAME
     * @ingroup StandardMacros
     *
     * Retourne une chaîne avec la signature complète de la fonction.
     * Format variable selon le compilateur :
     * - MSVC : __FUNCSIG__ (ex: "void __cdecl MyFunc(int)")
     * - GCC/Clang : __PRETTY_FUNCTION__ (ex: "void MyFunc(int)")
     * - Fallback : __func__ (nom simple, C99)
     *
     * @example
     * @code
     * void DebugLog(const char* msg) {
     *     printf("[%s] %s\n", NKENTSEU_FUNCTION_NAME, msg);
     * }
     * @endcode
     */
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_FUNCTION_NAME __FUNCSIG__
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_FUNCTION_NAME __PRETTY_FUNCTION__
    #else
        #define NKENTSEU_FUNCTION_NAME __func__
    #endif

    /**
     * @brief Chemin du fichier source actuel
     * @def NKENTSEU_FILE_NAME
     * @ingroup StandardMacros
     *
     * Macro standard __FILE__ fournie par tous les compilateurs C/C++.
     * Contient le chemin relatif ou absolu du fichier source.
     */
    #define NKENTSEU_FILE_NAME __FILE__

    /**
     * @brief Numéro de ligne source actuelle
     * @def NKENTSEU_LINE_NUMBER
     * @ingroup StandardMacros
     *
     * Macro standard __LINE__ fournie par tous les compilateurs C/C++.
     * Utile pour le logging, les assertions et le debugging.
     */
    #define NKENTSEU_LINE_NUMBER __LINE__

    // =========================================================================
    // SECTION 13 : DÉTECTION DES CAPACITÉS SPÉCIALES DU COMPILATEUR
    // =========================================================================
    // Détection de fonctionnalités optionnelles qui peuvent être désactivées
    // par les flags de compilation (-fno-rtti, -fno-exceptions, etc.).

    /**
     * @brief Support de RTTI (Run-Time Type Information)
     * @def NKENTSEU_HAS_RTTI
     * @ingroup SpecialCapabilities
     *
     * Définie si le compilateur supporte dynamic_cast et typeid.
     * Peut être désactivée via -fno-rtti (GCC/Clang) ou /GR- (MSVC).
     */
    #if defined(__GXX_RTTI) || defined(_CPPRTTI) || defined(__INTEL_RTTI__)
        #define NKENTSEU_HAS_RTTI
    #endif

    /**
     * @brief Support des exceptions C++
     * @def NKENTSEU_HAS_EXCEPTIONS
     * @ingroup SpecialCapabilities
     *
     * Définie si le compilateur supporte try/catch/throw.
     * Peut être désactivée via -fno-exceptions (GCC/Clang) ou /EHsc- (MSVC).
     */
    #if defined(__EXCEPTIONS) || defined(_CPPUNWIND) || defined(__cpp_exceptions)
        #define NKENTSEU_HAS_EXCEPTIONS
    #endif

    /**
     * @brief Support des entiers 128 bits (__int128)
     * @def NKENTSEU_HAS_INT128
     * @ingroup SpecialCapabilities
     *
     * Définie si le compilateur supporte le type __int128 (GCC/Clang sur 64-bit).
     * Non supporté par MSVC. Utile pour les calculs de très grande précision.
     *
     * @see NKENTSEU_int128, NKENTSEU_uint128
     */
    #if defined(__SIZEOF_INT128__) && !defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_HAS_INT128

        /**
         * @brief Type entier signé 128 bits
         * @typedef NKENTSEU_int128
         * @ingroup SpecialCapabilities
         */
        typedef __int128 NKENTSEU_int128;

        /**
         * @brief Type entier non signé 128 bits
         * @typedef NKENTSEU_uint128
         * @ingroup SpecialCapabilities
         */
        typedef unsigned __int128 NKENTSEU_uint128;
    #endif

    // =========================================================================
    // SECTION 14 : MACROS UTILITAIRES POUR LES PRAGMAS
    // =========================================================================
    // Gestion portable des pragmas pour contrôler les warnings et optimisations.

    /**
     * @brief Macro pour émettre un pragma de manière portable
     * @def NKENTSEU_PRAGMA
     * @param x Instruction pragma à émettre
     * @ingroup Pragmas
     *
     * Abstraction de la syntaxe différente entre MSVC (__pragma) et GCC/Clang (_Pragma).
     *
     * @example
     * @code
     * NKENTSEU_PRAGMA(optimize("O3"))  // GCC/Clang
     * NKENTSEU_PRAGMA(warning(disable: 4100))  // MSVC
     * @endcode
     */
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_PRAGMA(x) __pragma(x)
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_PRAGMA(x) _Pragma(#x)
    #else
        #define NKENTSEU_PRAGMA(x)
    #endif

    /**
     * @brief Sauvegarder l'état actuel des warnings (push)
     * @def NKENTSEU_DISABLE_WARNING_PUSH
     * @ingroup Pragmas
     * @see NKENTSEU_DISABLE_WARNING_POP
     */

    /**
     * @brief Restaurer l'état des warnings après un push (pop)
     * @def NKENTSEU_DISABLE_WARNING_POP
     * @ingroup Pragmas
     * @see NKENTSEU_DISABLE_WARNING_PUSH
     */

    /**
     * @brief Désactiver un warning spécifique
     * @def NKENTSEU_DISABLE_WARNING
     * @param warning Identifiant du warning (numérique pour MSVC, chaîne pour GCC/Clang)
     * @ingroup Pragmas
     *
     * @example
     * @code
     * // MSVC : désactiver warning 4100 (paramètre unused)
     * NKENTSEU_DISABLE_WARNING_PUSH
     * NKENTSEU_DISABLE_WARNING(4100)
     * void Func(int unused) { }
     * NKENTSEU_DISABLE_WARNING_POP
     *
     * // GCC/Clang : désactiver warning "-Wunused-parameter"
     * NKENTSEU_DISABLE_WARNING_PUSH
     * NKENTSEU_DISABLE_WARNING("-Wunused-parameter")
     * void Func(int unused) { }
     * NKENTSEU_DISABLE_WARNING_POP
     * @endcode
     */
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_DISABLE_WARNING_PUSH NKENTSEU_PRAGMA(warning(push))
        #define NKENTSEU_DISABLE_WARNING_POP NKENTSEU_PRAGMA(warning(pop))
        #define NKENTSEU_DISABLE_WARNING(warningNumber) NKENTSEU_PRAGMA(warning(disable : warningNumber))
        #define NKENTSEU_DISABLE_WARNING_MSVC(x) NKENTSEU_DISABLE_WARNING(x)
        #define NKENTSEU_DISABLE_WARNING_CLANG(x)
        #define NKENTSEU_DISABLE_WARNING_GCC(x)
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_DISABLE_WARNING_PUSH NKENTSEU_PRAGMA(GCC diagnostic push)
        #define NKENTSEU_DISABLE_WARNING_POP NKENTSEU_PRAGMA(GCC diagnostic pop)
        #define NKENTSEU_DISABLE_WARNING(warningName) NKENTSEU_PRAGMA(GCC diagnostic ignored warningName)
        #define NKENTSEU_DISABLE_WARNING_MSVC(x)
        #define NKENTSEU_DISABLE_WARNING_CLANG(x) NKENTSEU_DISABLE_WARNING(x)
        #define NKENTSEU_DISABLE_WARNING_GCC(x) NKENTSEU_DISABLE_WARNING(x)
    #else
        #define NKENTSEU_DISABLE_WARNING_PUSH
        #define NKENTSEU_DISABLE_WARNING_POP
        #define NKENTSEU_DISABLE_WARNING(warning)
        #define NKENTSEU_DISABLE_WARNING_MSVC(x)
        #define NKENTSEU_DISABLE_WARNING_CLANG(x)
        #define NKENTSEU_DISABLE_WARNING_GCC(x)
    #endif

    // =========================================================================
    // SECTION 15 : VALIDATIONS ET CONTRÔLES DE COHÉRENCE
    // =========================================================================
    // Vérifications à la compilation pour garantir un environnement valide.

    // -------------------------------------------------------------------------
    // Sous-section : Vérification du standard C++ minimum requis
    // -------------------------------------------------------------------------
    // NKentseu nécessite au minimum C++11 pour fonctionner correctement.

    #if !defined(NKENTSEU_HAS_CPP11)
        #error "NKentseu requires at least C++11 compiler support"
    #endif

    // -------------------------------------------------------------------------
    // Sous-section : Messages informatifs pour le debug de compilation
    // -------------------------------------------------------------------------
    // Optionnel : définir NKENTSEU_COMPILER_DEBUG pour afficher les détections.

    #ifdef NKENTSEU_COMPILER_DEBUG
        #pragma message("NKentseu: Compilateur détecté: " \
            #if defined(NKENTSEU_COMPILER_MSVC) \
                "MSVC " NKENTSEU_STRINGIZE(NKENTSEU_COMPILER_VERSION) \
            #elif defined(NKENTSEU_COMPILER_CLANG) \
                "Clang " NKENTSEU_STRINGIZE(NKENTSEU_COMPILER_VERSION) \
            #elif defined(NKENTSEU_COMPILER_GCC) \
                "GCC " NKENTSEU_STRINGIZE(NKENTSEU_COMPILER_VERSION) \
            #elif defined(NKENTSEU_COMPILER_INTEL) \
                "Intel " NKENTSEU_STRINGIZE(NKENTSEU_COMPILER_VERSION) \
            #else \
                "Unknown" \
            #endif \
        )

        #pragma message("NKentseu: Standard C++: " NKENTSEU_STRINGIZE(NKENTSEU_CPP_VERSION))

        #ifdef NKENTSEU_HAS_RTTI
            #pragma message("NKentseu: RTTI enabled")
        #else
            #pragma message("NKentseu: RTTI disabled")
        #endif

        #ifdef NKENTSEU_HAS_EXCEPTIONS
            #pragma message("NKentseu: Exceptions enabled")
        #else
            #pragma message("NKentseu: Exceptions disabled")
        #endif
    #endif

#endif // NKENTSEU_PLATFORM_NKCOMPILERDETECT_H

// =============================================================================
// EXEMPLES D'UTILISATION DE NKCOMPILERDETECT.H
// =============================================================================
// Ce fichier fournit des macros pour détecter le compilateur et les fonctionnalités
// C++ disponibles, permettant d'écrire du code portable et optimisé.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Code conditionnel par compilateur
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCompilerDetect.h"

    void PlatformSpecificCode() {
        #ifdef NKENTSEU_COMPILER_MSVC
            // Code spécifique à MSVC
            __assume(ptr != nullptr);  // Hint d'optimisation MSVC

        #elif defined(NKENTSEU_COMPILER_GCC)
            // Code spécifique à GCC
            if (__builtin_expect(ptr != nullptr, 1)) {
                ProcessData(ptr);
            }

        #elif defined(NKENTSEU_COMPILER_CLANG)
            // Code spécifique à Clang
            #if NKENTSEU_COMPILER_VERSION >= 140000  // Clang 14+
                UseNewClangAPI();
            #endif
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des macros de convenance C++
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCompilerDetect.h"

    class ResourceManager {
    public:
        // Constructeur avec noexcept portable
        ResourceManager() NKENTSEU_NOEXCEPT {
            // Initialisation sans risque d'exception
        }

        // Méthode virtuelle avec override portable
        virtual void Load() NKENTSEU_OVERRIDE {
            // Implémentation
        }

        // Méthode finale empêchant la redéfinition
        virtual void Finalize() NKENTSEU_FINAL {
            // Dernière étape, non redéfinissable
        }

        // Fonction dont le retour ne doit pas être ignoré
        NKENTSEU_NODISCARD_MSG("Vérifier le code de retour pour les erreurs")
        int Initialize() {
            return 0;  // 0 = succès, autre = code d'erreur
        }

        // Paramètre potentiellement inutilisé sans warning
        void Callback(int NKENTSEU_MAYBE_UNUSED userData) {
            // userData peut être ignoré dans certains cas
        }
    };
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion portable des warnings
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCompilerDetect.h"

    // Fonction avec paramètre unused intentionnel
    NKENTSEU_DISABLE_WARNING_PUSH

    #ifdef NKENTSEU_COMPILER_MSVC
        NKENTSEU_DISABLE_WARNING(4100)  // unreferenced formal parameter
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        NKENTSEU_DISABLE_WARNING("-Wunused-parameter")
    #endif

    void CallbackHandler(int eventId, void* NKENTSEU_RESTRICT context) {
        // Seul eventId est utilisé, context est requis par l'API mais ignoré
        ProcessEvent(eventId);
    }

    NKENTSEU_DISABLE_WARNING_POP
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Structures binaires avec packing portable
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCompilerDetect.h"
    #include <cstdint>

    NKENTSEU_PACK_BEGIN
    struct NKENTSEU_PACKED NetworkPacket {
        uint16_t magic;      // 2 octets
        uint16_t type;       // 2 octets
        uint32_t length;     // 4 octets
        uint8_t  flags;      // 1 octet
        // Total : 9 octets, pas de padding
    };
    NKENTSEU_PACK_END

    // Vérification à la compilation
    static_assert(sizeof(NetworkPacket) == 9, "NetworkPacket must be tightly packed");

    void SendPacket(const NetworkPacket* packet) {
        // Envoi binaire direct sans sérialisation
        WriteToSocket(packet, sizeof(NetworkPacket));
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Détection de fonctionnalités à la compilation
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCompilerDetect.h"

    template<typename T>
    class OptimizedContainer {
    public:
        #if defined(NKENTSEU_HAS_CONCEPTS)
            // Version C++20 avec concepts pour une meilleure erreur de compilation
            template<typename U>
            requires std::copyable<U> && std::destructible<U>
            void Add(U&& item) {
                // Implémentation optimisée avec concepts
            }
        #else
            // Fallback pour C++11/14/17 sans concepts
            template<typename U>
            void Add(U&& item) {
                // Implémentation générique avec SFINAE si nécessaire
            }
        #endif

        // Méthode constexpr si supporté
        NKENTSEU_CONSTEXPR size_t Capacity() const {
            return m_capacity;
        }

    private:
        size_t m_capacity = 0;
    };
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec NkPlatformDetect.h et NkArchDetect.h
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkPlatformDetect.h"   // Détection OS
    #include "NKPlatform/NkArchDetect.h"       // Détection architecture
    #include "NKPlatform/NkCompilerDetect.h"   // Détection compilateur

    void CrossPlatformOptimization() {
        // Combinaison OS + Architecture + Compilateur pour un ciblage précis

        NKENTSEU_PLATFORM_WINDOWS({
            NKENTSEU_ARCH_X86_64({
                NKENTSEU_COMPILER_MSVC({
                    // Windows x64 avec MSVC : utiliser les intrinsics MSVC
                    UseMSVCIntrinsics();
                });

                NKENTSEU_COMPILER_CLANG({
                    // Windows x64 avec Clang : utiliser les intrinsics compatibles
                    UseClangIntrinsics();
                });
            });
        });

        NKENTSEU_PLATFORM_LINUX({
            NKENTSEU_ARCH_ARM64({
                NKENTSEU_COMPILER_GCC({
                    // Linux ARM64 avec GCC : optimisations NEON + GCC
                    EnableNEONGCCOptimizations();
                });
            });
        });

        // Code utilisant les fonctionnalités C++ modernes si disponibles
        #if defined(NKENTSEU_HAS_IF_CONSTEXPR)
            if constexpr (std::is_pointer_v<T>) {
                HandlePointerCase();
            } else {
                HandleValueCase();
            }
        #else
            // Fallback sans if constexpr
            if (std::is_pointer<T>::value) {
                HandlePointerCase();
            } else {
                HandleValueCase();
            }
        #endif
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Macro utilitaire pour le logging portable
// -----------------------------------------------------------------------------
/*
    #include "NKPlatform/NkCompilerDetect.h"
    #include <cstdio>

    // Macro de logging avec informations de compilation
    #define NK_LOG(level, msg, ...) \
        do { \
            fprintf(stderr, "[%s:%d] [%s] " msg "\n", \
                NKENTSEU_FILE_NAME, \
                NKENTSEU_LINE_NUMBER, \
                level, \
                ##__VA_ARGS__); \
        } while(0)

    // Utilisation :
    void Initialize() {
        NK_LOG("INFO", "Initialisation en cours...");

        #ifdef NKENTSEU_COMPILER_DEBUG
            NK_LOG("DEBUG", "Compilateur: %d", NKENTSEU_COMPILER_VERSION);
            NK_LOG("DEBUG", "C++ Standard: %d", NKENTSEU_CPP_VERSION);
        #endif

        if (!CheckPrerequisites()) {
            NK_LOG("ERROR", "Prérequis non satisfaits");
            return;
        }

        NK_LOG("INFO", "Initialisation terminée");
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================