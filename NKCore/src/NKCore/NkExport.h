// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkExport.h
// DESCRIPTION: Système d'export/import multiplateforme pour bibliothèques Nkentseu
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 4.0.0
// MODIFICATIONS: Intégration avec NkPlatformDetect et NkArchDetect, système modulaire
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKEXPORT_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKEXPORT_H_INCLUDED

// ============================================================
// INCLUDES DES SYSTÈMES DE DÉTECTION
// ============================================================

#include "NkPlatformDetect.h"
#include "NkArchDetect.h"

// ============================================================
// CONFIGURATION DU BUILD (STATIQUE VS PARTAGÉ)
// ============================================================

/**
 * @defgroup BuildConfiguration Configuration du Build
 * @brief Macros pour configurer le type de build (statique/partagé)
 *
 * Le système détecte automatiquement le type de build à partir de:
 * - Variables CMake (BUILD_SHARED_LIBS)
 * - Variables de préprocesseur (_WINDLL, _USRDLL)
 * - Macros définies manuellement (NKENTSEU_STATIC, NKENTSEU_SHARED)
 */

/**
 * @brief Active le build statique
 * @def NKENTSEU_STATIC_BUILD
 * @ingroup BuildConfiguration
 *
 * Définir cette macro à 1 pour forcer un build statique.
 * Si non définie, le système détecte automatiquement.
 */
#ifndef NKENTSEU_STATIC_BUILD
#if defined(NKENTSEU_STATIC) || defined(NKENTSEU_LIB)
#define NKENTSEU_STATIC_BUILD 1
#else
#define NKENTSEU_STATIC_BUILD 0
#endif
#endif

/**
 * @brief Active le build partagé (DLL/SO)
 * @def NKENTSEU_SHARED_BUILD
 * @ingroup BuildConfiguration
 *
 * Définir cette macro à 1 pour forcer un build partagé.
 * Si non définie, le système détecte automatiquement.
 */
#ifndef NKENTSEU_SHARED_BUILD
#if defined(NKENTSEU_SHARED) || defined(NKENTSEU_DLL)
#define NKENTSEU_SHARED_BUILD 1
#else
#define NKENTSEU_SHARED_BUILD 0
#endif
#endif

// ============================================================
// AUTO-DÉTECTION DU TYPE DE BUILD
// ============================================================

// Si aucune configuration n'est spécifiée, détecter automatiquement
#if !NKENTSEU_STATIC_BUILD && !NKENTSEU_SHARED_BUILD
NKENTSEU_WINDOWS_ONLY(
// Windows: détecter les flags de DLL
#if defined(_WINDLL) || defined(_USRDLL)
#undef NKENTSEU_SHARED_BUILD
#define NKENTSEU_SHARED_BUILD 1
#endif
)

// CMake standard
#if defined(BUILD_SHARED_LIBS) && BUILD_SHARED_LIBS
#undef NKENTSEU_SHARED_BUILD
#define NKENTSEU_SHARED_BUILD 1
#endif

// Consoles et embarqué: toujours statique
NKENTSEU_CONSOLE_ONLY(
#undef NKENTSEU_STATIC_BUILD
#define NKENTSEU_STATIC_BUILD 1
)

NKENTSEU_EMBEDDED_ONLY(
#undef NKENTSEU_STATIC_BUILD
#define NKENTSEU_STATIC_BUILD 1
)

// Si toujours rien, par défaut: statique
#if !NKENTSEU_STATIC_BUILD && !NKENTSEU_SHARED_BUILD
#define NKENTSEU_STATIC_BUILD 1
#endif
#endif

// ============================================================
// MACROS FONDAMENTALES D'EXPORT/IMPORT PAR PLATEFORME
// ============================================================

/**
 * @defgroup CoreExportMacros Macros Fondamentales d'Export
 * @brief Macros de base pour l'export/import selon la plateforme
 *
 * Ces macros sont automatiquement définies selon la plateforme détectée
 * par NkPlatformDetect.h et ne devraient pas être utilisées directement.
 */

/**
 * @brief Export de symbole (compilation de la bibliothèque)
 * @def NKENTSEU_SYMBOL_EXPORT
 * @ingroup CoreExportMacros
 */

#ifndef NKENTSEU_SYMBOL_EXPORT
    #if defined(_WIN32)
        #define NKENTSEU_SYMBOL_EXPORT __declspec(dllexport)
        #define NKENTSEU_SYMBOL_IMPORT __declspec(dllimport)
    #elif defined(__EMSCRIPTEN__)
        #define NKENTSEU_SYMBOL_EXPORT __attribute__((used))
        #define NKENTSEU_SYMBOL_IMPORT
    #elif defined(__GNUC__) || defined(__clang__)
        #define NKENTSEU_SYMBOL_EXPORT __attribute__((visibility("default")))
        #define NKENTSEU_SYMBOL_IMPORT
    #else
        #define NKENTSEU_SYMBOL_EXPORT
        #define NKENTSEU_SYMBOL_IMPORT
    #endif
#endif

/**
 * @brief Cache un symbole (non exporté)
 * @def NKENTSEU_SYMBOL_HIDDEN
 * @ingroup CoreExportMacros
 */
#if defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_SYMBOL_HIDDEN __attribute__((visibility("hidden")))
#define NKENTSEU_SYMBOL_INTERNAL __attribute__((visibility("internal")))
#else
#define NKENTSEU_SYMBOL_HIDDEN
#define NKENTSEU_SYMBOL_INTERNAL
#endif

/**
 * @brief Force la visibilité d'un symbole
 * @def NKENTSEU_SYMBOL_VISIBLE
 * @ingroup CoreExportMacros
 */
#if defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_SYMBOL_VISIBLE __attribute__((visibility("default")))
#else
#define NKENTSEU_SYMBOL_VISIBLE
#endif

// ============================================================
// CONVENTIONS D'APPEL SELON L'ARCHITECTURE
// ============================================================

/**
 * @defgroup CallingConventions Conventions d'Appel
 * @brief Conventions d'appel adaptées à la plateforme et l'architecture
 */

#if defined(_WIN32)
    #define NKENTSEU_CDECL __cdecl
    #define NKENTSEU_STDCALL __stdcall
    #define NKENTSEU_FASTCALL __fastcall
    #if defined(NKENTSEU_ARCH_X86_64)
        #define NKENTSEU_VECTORCALL __vectorcall
    #else
        #define NKENTSEU_VECTORCALL
    #endif
    #if defined(NKENTSEU_ARCH_X86_64)
        #define NKENTSEU_CALL __fastcall
    #elif defined(NKENTSEU_ARCH_X86)
        #define NKENTSEU_CALL __stdcall
    #elif defined(NKENTSEU_ARCH_ARM64)
        #define NKENTSEU_CALL
    #else
        #define NKENTSEU_CALL
    #endif
#else
    #define NKENTSEU_CDECL
    #define NKENTSEU_STDCALL
    #define NKENTSEU_FASTCALL
    #define NKENTSEU_VECTORCALL
    #define NKENTSEU_CALL
#endif

// ============================================================
// COMPATIBILITÉ C/C++
// ============================================================

/**
 * @defgroup CppCompatibility Compatibilité C/C++
 * @brief Macros pour interfaces C et C++
 */

#ifdef __cplusplus
/**
 * @brief Linkage C
 * @ingroup CppCompatibility
 */
#define NKENTSEU_EXTERN_C extern "C"
#define NKENTSEU_EXTERN_C_BEGIN extern "C" {
#define NKENTSEU_EXTERN_C_END }
#else
#define NKENTSEU_EXTERN_C
#define NKENTSEU_EXTERN_C_BEGIN
#define NKENTSEU_EXTERN_C_END
#endif

// ============================================================
// SYSTÈME D'EXPORT MODULAIRE (NKENTSEU_DEFINE_MODULE_API)
// ============================================================

/**
 * @defgroup ModularExport Système d'Export Modulaire
 * @brief Système pour créer des exports par module/application
 *
 * Ce système permet de créer facilement des macros d'export pour
 * différents modules (Graphics, Audio, Physics, etc.) tout en
 * utilisant la même logique d'export/import.
 *
 * @example
 * @code
 * // Définir l'API pour le module Graphics
 * NKENTSEU_DEFINE_MODULE_API(GRAPHICS)
 *
 * // Utilisation
 * class NKENTSEU_GRAPHICS_API Renderer {
 *     NKENTSEU_GRAPHICS_API void render();
 * };
 * @endcode
 */

/**
 * @brief Macro pour définir une API de module
 * @def NKENTSEU_DEFINE_MODULE_API
 * @param MODULE_NAME Nom du module en MAJUSCULES (ex: GRAPHICS, AUDIO)
 * @ingroup ModularExport
 *
 * Cette macro génère automatiquement:
 * - NKENTSEU_MODULE_API: Macro d'export/import principale
 * - NKENTSEU_MODULE_C_API: Version C de l'export
 * - NKENTSEU_MODULE_PRIVATE: Symboles privés du module
 * - NKENTSEU_MODULE_PUBLIC: Symboles publics du module
 *
 * Le module doit définir NKENTSEU_BUILDING_MODULE avant d'inclure les headers.
 */
#define NKENTSEU_DEFINE_MODULE_API(MODULE_NAME)                                                                        \
	/* Détection du contexte de build pour ce module */                                                                \
	_NKENTSEU_DETECT_MODULE_CONTEXT(MODULE_NAME)                                                                       \
	/* Définition de l'API principale */                                                                               \
	_NKENTSEU_DEFINE_MODULE_EXPORT(MODULE_NAME)                                                                        \
	/* Définition de l'API C */                                                                                        \
	_NKENTSEU_DEFINE_MODULE_C_API(MODULE_NAME)                                                                         \
	/* Définition des macros de visibilité */                                                                          \
	_NKENTSEU_DEFINE_MODULE_VISIBILITY(MODULE_NAME)

// ============================================================
// IMPLÉMENTATION INTERNE DU SYSTÈME MODULAIRE
// ============================================================

/**
 * @brief Détecte si on build ou utilise un module
 * @internal
 */
#define _NKENTSEU_DETECT_MODULE_CONTEXT(MODULE_NAME)                                                                   \
	/* Détection si on compile le module */                                                                            \
	NKENTSEU_CONCAT_IMPL(NKENTSEU_IS_BUILDING_, MODULE_NAME) =                                                         \
		defined(NKENTSEU_CONCAT_IMPL(NKENTSEU_BUILDING_, MODULE_NAME)) ||                                              \
		defined(NKENTSEU_CONCAT_IMPL(MODULE_NAME, _EXPORTS)) ||                                                        \
		defined(NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _EXPORTS))

/**
 * @brief Définit la macro d'export principale du module
 * @internal
 */
#define _NKENTSEU_DEFINE_MODULE_EXPORT(MODULE_NAME)                                                                    \
	/* Nom de la macro d'API */                                                                                        \
	NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _API) =		   /* Si on build le module, on exporte */             \
		_NKENTSEU_IF_BUILDING(MODULE_NAME, NKENTSEU_SYMBOL_EXPORT, /* Sinon si build partagé, on importe */            \
							  _NKENTSEU_IF_SHARED(NKENTSEU_SYMBOL_IMPORT, /* Sinon statique, rien */))

/**
 * @brief Définit la macro d'export C du module
 * @internal
 */
#define _NKENTSEU_DEFINE_MODULE_C_API(MODULE_NAME)                                                                     \
	NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _C_API) =                                                             \
		NKENTSEU_EXTERN_C NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _API) NKENTSEU_CALL

/**
 * @brief Définit les macros de visibilité du module
 * @internal
 */
#define _NKENTSEU_DEFINE_MODULE_VISIBILITY(MODULE_NAME)                                                                \
	NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _PUBLIC) = NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _API);        \
	NKENTSEU_CONCAT_IMPL(NKENTSEU_, MODULE_NAME, _PRIVATE) = NKENTSEU_SYMBOL_HIDDEN

// ============================================================
// UTILITAIRES POUR LE SYSTÈME MODULAIRE
// ============================================================

/**
 * @brief Concaténation de tokens (niveau 1)
 * @internal
 */
#define NKENTSEU_CONCAT_IMPL(a, b) a##b
#define NKENTSEU_CONCAT(a, b) NKENTSEU_CONCAT_IMPL(a, b)

/**
 * @brief Concaténation de 3 tokens
 * @internal
 */
#define NKENTSEU_CONCAT3_IMPL(a, b, c) a##b##c
#define NKENTSEU_CONCAT3(a, b, c) NKENTSEU_CONCAT3_IMPL(a, b, c)

/**
 * @brief Conditionnel pour build de module
 * @internal
 */
#define _NKENTSEU_IF_BUILDING(MODULE_NAME, THEN, ELSE)                                                                 \
	NKENTSEU_IF(NKENTSEU_CONCAT(NKENTSEU_IS_BUILDING_, MODULE_NAME), THEN, ELSE)

/**
 * @brief Conditionnel pour build partagé
 * @internal
 */
#define _NKENTSEU_IF_SHARED(THEN, ELSE) NKENTSEU_IF(NKENTSEU_SHARED_BUILD, THEN, ELSE)

/**
 * @brief Macro conditionnelle simple
 * @internal
 */
#define NKENTSEU_IF(COND, THEN, ELSE) NKENTSEU_IF_IMPL(COND, THEN, ELSE)
#define NKENTSEU_IF_IMPL(COND, THEN, ELSE) NKENTSEU_IF_##COND(THEN, ELSE)
#define NKENTSEU_IF_0(THEN, ELSE) ELSE
#define NKENTSEU_IF_1(THEN, ELSE) THEN

// ============================================================
// API PRINCIPALE NKENTSEU (MODULE CORE)
// ============================================================

/**
 * @defgroup CoreAPI API Principale Nkentseu
 * @brief API d'export pour le module Core de Nkentseu
 */

/**
 * @brief Détection du contexte de build pour Core
 * @ingroup CoreAPI
 */
#ifndef NKENTSEU_BUILDING_CORE
#if defined(NKENTSEU_CORE_EXPORTS) || defined(NKENTSEU_BUILDING_NKENTSEU) || defined(NKENTSEU_EXPORTS)
#define NKENTSEU_BUILDING_CORE 1
#else
#define NKENTSEU_BUILDING_CORE 0
#endif
#endif

/**
 * @brief API principale de Nkentseu
 * @def NKENTSEU_API
 * @ingroup CoreAPI
 *
 * Utilisé pour exporter/importer les symboles du module Core.
 *
 * @example
 * @code
 * class NKENTSEU_API MyClass {
 * public:
 *     NKENTSEU_API void myMethod();
 * };
 * @endcode
 */
#if NKENTSEU_BUILDING_CORE
#define NKENTSEU_API NKENTSEU_SYMBOL_EXPORT
#elif NKENTSEU_SHARED_BUILD
#define NKENTSEU_API NKENTSEU_SYMBOL_IMPORT
#else
#define NKENTSEU_API
#endif

/**
 * @brief API C de Nkentseu
 * @def NKENTSEU_C_API
 * @ingroup CoreAPI
 */
#define NKENTSEU_C_API NKENTSEU_EXTERN_C NKENTSEU_API NKENTSEU_CALL

/**
 * @brief Symboles publics de Nkentseu
 * @def NKENTSEU_PUBLIC
 * @ingroup CoreAPI
 */
#define NKENTSEU_PUBLIC NKENTSEU_API

/**
 * @brief Symboles privés de Nkentseu
 * @def NKENTSEU_PRIVATE
 * @ingroup CoreAPI
 */
#define NKENTSEU_PRIVATE NKENTSEU_SYMBOL_HIDDEN

/**
 * @brief Alias court pour NKENTSEU_API
 * @def NKENTSEU_CORE_API
 * @ingroup CoreAPI
 */
// #define NKENTSEU_CORE_API NKENTSEU_API

// ============================================================
// MACROS SPÉCIFIQUES PLATEFORMES (WEBASSEMBLY, CONSOLES)
// ============================================================

/**
 * @defgroup PlatformSpecificExport Export Spécifique Plateforme
 * @brief Macros d'export pour plateformes spécifiques
 */

#if defined(__EMSCRIPTEN__)
    #define NKENTSEU_WASM_EXPORT(name) __attribute__((export_name(#name)))
    #define NKENTSEU_WASM_IMPORT(name) __attribute__((import_name(#name)))
    #define NKENTSEU_WASM_KEEP __attribute__((used))
    #define NKENTSEU_WASM_MAIN __attribute__((export_name("main")))
#else
    #define NKENTSEU_WASM_EXPORT(name)
    #define NKENTSEU_WASM_IMPORT(name)
    #define NKENTSEU_WASM_KEEP
    #define NKENTSEU_WASM_MAIN
#endif

// ============================================================
// MACROS DE DÉPRÉCIATION
// ============================================================

/**
 * @defgroup Deprecation Dépréciation
 * @brief Macros pour marquer les API comme dépréciées
 */

/**
 * @brief Marque une API publique comme dépréciée
 * @ingroup Deprecation
 */
#define NKENTSEU_DEPRECATED_API NKENTSEU_DEPRECATED NKENTSEU_API
#define NKENTSEU_DEPRECATED_API_MSG(msg) NKENTSEU_DEPRECATED_MSG(msg) NKENTSEU_API

// ============================================================
// MACROS DE VALIDATION ET MESSAGES
// ============================================================

/**
 * @defgroup BuildValidation Validation du Build
 * @brief Macros pour valider la configuration du build
 */

// Vérification de cohérence
#if NKENTSEU_STATIC_BUILD && NKENTSEU_SHARED_BUILD
#error "Nkentseu: Build ne peut pas être à la fois statique et partagé"
#endif

// Message informatif en mode debug
#ifdef NKENTSEU_VERBOSE_BUILD
#if NKENTSEU_BUILDING_CORE
#pragma message("Nkentseu: Building Core as " NKENTSEU_IF(NKENTSEU_SHARED_BUILD, "SHARED", "STATIC"))
#else
#pragma message("Nkentseu: Using Core as " NKENTSEU_IF(NKENTSEU_SHARED_BUILD, "SHARED", "STATIC"))
#endif
#endif

// ============================================================
// EXEMPLES D'UTILISATION DANS LA DOCUMENTATION
// ============================================================

/**
 * @example Utilisation basique
 * @code
 * // Dans MyClass.h
 * #include "NkExport.h"
 *
 * class NKENTSEU_API MyClass {
 * public:
 *     NKENTSEU_API MyClass();
 *     NKENTSEU_API void publicMethod();
 * private:
 *     NKENTSEU_PRIVATE void privateMethod(); // Non exportée
 * };
 * @endcode
 *
 * @example Création d'une API pour un module Graphics
 * @code
 * // Dans NkGraphicsExport.h
 * #include "NkExport.h"
 *
 * // Définir l'API du module Graphics
 * #ifdef NKENTSEU_BUILDING_GRAPHICS
 *     #define NKENTSEU_GRAPHICS_API NKENTSEU_SYMBOL_EXPORT
 * #elif NKENTSEU_SHARED_BUILD
 *     #define NKENTSEU_GRAPHICS_API NKENTSEU_SYMBOL_IMPORT
 * #else
 *     #define NKENTSEU_GRAPHICS_API
 * #endif
 *
 * #define NKENTSEU_GRAPHICS_C_API NKENTSEU_EXTERN_C NKENTSEU_GRAPHICS_API
 * #define NKENTSEU_GRAPHICS_PUBLIC NKENTSEU_GRAPHICS_API
 * #define NKENTSEU_GRAPHICS_PRIVATE NKENTSEU_SYMBOL_HIDDEN
 *
 * // Utilisation
 * class NKENTSEU_GRAPHICS_API Renderer {
 *     NKENTSEU_GRAPHICS_API void render();
 * };
 * @endcode
 *
 * @example API C multiplateforme
 * @code
 * // API C avec convention d'appel appropriée
 * NKENTSEU_EXTERN_C_BEGIN
 *
 * NKENTSEU_C_API int nkInit(void);
 * NKENTSEU_C_API void nkShutdown(void);
 * NKENTSEU_C_API const char* nkGetVersion(void);
 *
 * NKENTSEU_EXTERN_C_END
 * @endcode
 *
 * @example Fonction dépréciée
 * @code
 * // Marquer une fonction comme obsolète
 * NKENTSEU_DEPRECATED_API void oldFunction();
 * NKENTSEU_DEPRECATED_API_MSG("Use newFunction() instead") void legacyFunc();
 * @endcode
 */

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKEXPORT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================