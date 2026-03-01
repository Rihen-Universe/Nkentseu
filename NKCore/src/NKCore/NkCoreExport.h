// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkExport.h
// DESCRIPTION: API d'export pour le module Core de Nkentseu
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 4.0.0
// MODIFICATIONS: Intégration avec le système modulaire NkExport, types corrigés
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCOREEXPORT_H_INCLUDED
#define NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCOREEXPORT_H_INCLUDED

// ============================================================
// INCLUDE DU SYSTÈME D'EXPORT PRINCIPAL
// ============================================================

#include "NkExport.h"

// ============================================================
// CONFIGURATION DU MODULE CORE
// ============================================================

/**
 * @defgroup CoreModule Module Core
 * @brief Système d'export pour le module Core de Nkentseu
 *
 * Le module Core contient les fonctionnalités fondamentales de Nkentseu:
 * - Gestion de mémoire
 * - Système de types
 * - Containers de base
 * - Utilitaires système
 * - Gestion des threads
 * - Système de logging
 * - Gestion des erreurs
 */

/**
 * @brief Détection du contexte de build pour Core
 * @ingroup CoreModule
 *
 * Cette macro détecte automatiquement si on est en train de compiler
 * le module Core ou si on l'utilise comme bibliothèque externe.
 */
#ifndef NKENTSEU_BUILDING_CORE
#if defined(NKENTSEU_CORE_EXPORTS) || defined(CORE_EXPORTS) || defined(NKENTSEU_BUILDING_NKENTSEU)
#define NKENTSEU_BUILDING_CORE 1
#else
#define NKENTSEU_BUILDING_CORE 0
#endif
#endif

// ============================================================
// MACROS D'EXPORT POUR LE MODULE CORE
// ============================================================

/**
 * @brief API principale du module Core
 * @def NKENTSEU_CORE_API
 * @ingroup CoreModule
 *
 * Utilisé pour exporter/importer les classes et fonctions du module Core.
 * Cette macro gère automatiquement la distinction entre:
 * - Compilation du module (export)
 * - Utilisation en mode partagé (import)
 * - Utilisation en mode statique (rien)
 *
 * @example
 * @code
 * class NKENTSEU_CORE_API MemoryManager {
 * public:
 *     NKENTSEU_CORE_API void* allocate(nk_size size);
 *     NKENTSEU_CORE_API void deallocate(void* ptr);
 * };
 * @endcode
 */
#if NKENTSEU_BUILDING_CORE
#define NKENTSEU_CORE_API NKENTSEU_SYMBOL_EXPORT
#elif NKENTSEU_SHARED_BUILD
#define NKENTSEU_CORE_API NKENTSEU_SYMBOL_IMPORT
#else
#define NKENTSEU_CORE_API
#endif

/**
 * @brief API C du module Core
 * @def NKENTSEU_CORE_C_API
 * @ingroup CoreModule
 *
 * Version C de l'API Core avec le linkage et la convention d'appel appropriés.
 *
 * @example
 * @code
 * NKENTSEU_CORE_C_API void* nkCoreAllocateMemory(nk_size size);
 * NKENTSEU_CORE_C_API void nkCoreFreeMemory(void* ptr);
 * @endcode
 */
#define NKENTSEU_CORE_C_API NKENTSEU_EXTERN_C NKENTSEU_CORE_API NKENTSEU_CALL

/**
 * @brief Symboles publics du module Core
 * @def NKENTSEU_CORE_PUBLIC
 * @ingroup CoreModule
 *
 * Alias pour NKENTSEU_CORE_API. Utilisé pour la cohérence avec les autres modules.
 */
#define NKENTSEU_CORE_PUBLIC NKENTSEU_CORE_API

/**
 * @brief Symboles privés du module Core
 * @def NKENTSEU_CORE_PRIVATE
 * @ingroup CoreModule
 *
 * Utilisé pour les méthodes internes du module qui ne doivent pas être exportées.
 */
#define NKENTSEU_CORE_PRIVATE NKENTSEU_SYMBOL_HIDDEN

/**
 * @brief Alias court pour NKENTSEU_CORE_API
 * @def NK_CORE_API
 * @ingroup CoreModule
 */
#define NK_CORE_API NKENTSEU_CORE_API

// ============================================================
// MACROS SPÉCIFIQUES AUX COMPOSANTS DU CORE
// ============================================================

/**
 * @brief API pour la gestion de mémoire
 * @def NKENTSEU_MEMORY_API
 * @ingroup CoreModule
 *
 * Spécifique aux composants d'allocation mémoire.
 */
#define NKENTSEU_MEMORY_API NKENTSEU_CORE_API

/**
 * @brief API pour les containers
 * @def NKENTSEU_CONTAINER_API
 * @ingroup CoreModule
 *
 * Spécifique aux classes de containers (Vector, HashMap, etc.).
 */
#define NKENTSEU_CONTAINER_API NKENTSEU_CORE_API

/**
 * @brief API pour le système de types
 * @def NKENTSEU_TYPE_API
 * @ingroup CoreModule
 *
 * Spécifique au système de réflexion et de types.
 */
#define NKENTSEU_TYPE_API NKENTSEU_CORE_API

/**
 * @brief API pour le logging
 * @def NKENTSEU_LOG_API
 * @ingroup CoreModule
 *
 * Spécifique au système de journalisation.
 */
#define NKENTSEU_LOG_API NKENTSEU_CORE_API

/**
 * @brief API pour la gestion des threads
 * @def NKENTSEU_THREAD_API
 * @ingroup CoreModule
 *
 * Spécifique à la gestion des threads et synchronisation.
 */
#define NKENTSEU_THREAD_API NKENTSEU_CORE_API

/**
 * @brief API pour les utilitaires système
 * @def NKENTSEU_SYSTEM_API
 * @ingroup CoreModule
 *
 * Spécifique aux fonctions utilitaires système.
 */
#define NKENTSEU_SYSTEM_API NKENTSEU_CORE_API

/**
 * @brief API pour la gestion des erreurs
 * @def NKENTSEU_ERROR_API
 * @ingroup CoreModule
 *
 * Spécifique au système de gestion d'erreurs.
 */
#define NKENTSEU_ERROR_API NKENTSEU_CORE_API

// ============================================================
// MACROS DE VISIBILITÉ AVANCÉES
// ============================================================

/**
 * @brief Symbole visible mais non exporté
 * @def NKENTSEU_CORE_INTERNAL
 * @ingroup CoreModule
 *
 * Utilisé pour les fonctions qui doivent être visibles dans la bibliothèque
 * mais non exportées (pour l'usage interne entre fichiers).
 */
#define NKENTSEU_CORE_INTERNAL NKENTSEU_SYMBOL_INTERNAL

/**
 * @brief Force l'inclusion d'un symbole
 * @def NKENTSEU_CORE_KEEP
 * @ingroup CoreModule
 *
 * Empêche l'élimination de code mort pour les symboles importants.
 */
#if defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_CORE_KEEP __attribute__((used))
#else
#define NKENTSEU_CORE_KEEP
#endif

// ============================================================
// MACROS POUR LES TEMPLATES ET CODE INLINE
// ============================================================

/**
 * @brief Pour les fonctions templates exportées
 * @def NKENTSEU_CORE_TEMPLATE
 * @ingroup CoreModule
 *
 * Utilisé pour les instanciations explicites de templates qui doivent
 * être exportées.
 */
#define NKENTSEU_CORE_TEMPLATE NKENTSEU_CORE_API

// ============================================================
// MACROS DE DÉPRÉCIATION SPÉCIFIQUES AU CORE
// ============================================================

/**
 * @brief API Core dépréciée
 * @def NKENTSEU_CORE_DEPRECATED_API
 * @ingroup CoreModule
 */
#define NKENTSEU_CORE_DEPRECATED_API NKENTSEU_DEPRECATED NKENTSEU_CORE_API

/**
 * @brief API Core dépréciée avec message
 * @def NKENTSEU_CORE_DEPRECATED_API_MSG
 * @ingroup CoreModule
 */
#define NKENTSEU_CORE_DEPRECATED_API_MSG(msg) NKENTSEU_DEPRECATED_MSG(msg) NKENTSEU_CORE_API

// ============================================================
// EXEMPLE D'UTILISATION
// ============================================================

/*
 * UTILISATION DANS UN HEADER PUBLIC DU CORE:
 *
 * #include "Platform/NkCoreExport.h"
 * #include "Platform/NkTypes.h"
 *
 * namespace nkentseu {
 * namespace core {
 *
 * // Classe de base exportée
 * class NKENTSEU_CORE_API Object {
 * public:
 *     NKENTSEU_CORE_API Object();
 *     NKENTSEU_CORE_API virtual ~Object();
 *
 *     NKENTSEU_CORE_API virtual void update();
 *
 * private:
 *     NKENTSEU_CORE_PRIVATE void internalUpdate(); // Non exportée
 * };
 *
 * // Gestionnaire de mémoire
 * class NKENTSEU_MEMORY_API MemoryManager {
 * public:
 *     NKENTSEU_MEMORY_API static MemoryManager& instance();
 *     NKENTSEU_MEMORY_API void* allocate(nk_size size);
 *     NKENTSEU_MEMORY_API void deallocate(void* ptr);
 * };
 *
 * // Container template
 * template<typename T>
 * class NKENTSEU_CONTAINER_API Vector {
 * public:
 *     NKENTSEU_CONTAINER_API Vector();
 *     NKENTSEU_CONTAINER_API ~Vector();
 *
 *     NKENTSEU_CONTAINER_API void push_back(const T& value);
 *     NKENTSEU_CONTAINER_API T& operator[](nk_size index);
 *     NKENTSEU_CONTAINER_API nk_size size() const;
 * };
 *
 * // Instanciation explicite pour types communs
 * extern template class NKENTSEU_CORE_TEMPLATE Vector<nk_int32>;
 * extern template class NKENTSEU_CORE_TEMPLATE Vector<nk_float32>;
 *
 * // Fonction utilitaire
 * NKENTSEU_SYSTEM_API nk_uint64 getSystemTime();
 *
 * } // namespace core
 * } // namespace nkentseu
 *
 * // API C pour le Core
 * NKENTSEU_EXTERN_C_BEGIN
 *
 * // Gestion de mémoire
 * NKENTSEU_CORE_C_API void* nkCoreAllocate(nk_size size);
 * NKENTSEU_CORE_C_API void nkCoreFree(void* ptr);
 *
 * // Logging
 * NKENTSEU_CORE_C_API void nkCoreLogInfo(const nk_char* message);
 * NKENTSEU_CORE_C_API void nkCoreLogError(const nk_char* message);
 *
 * // Gestion des erreurs
 * NKENTSEU_CORE_C_API const nk_char* nkCoreGetLastError(void);
 *
 * NKENTSEU_EXTERN_C_END
 *
 * // Fonction dépréciée
 * NKENTSEU_CORE_DEPRECATED_API void oldCoreFunction();
 * NKENTSEU_CORE_DEPRECATED_API_MSG("Use newCoreFunction() instead")
 * void legacyCoreFunction();
 */

// ============================================================
// VÉRIFICATIONS ET VALIDATIONS
// ============================================================

// Validation de la configuration
#if defined(NKENTSEU_VERBOSE_BUILD) && NKENTSEU_BUILDING_CORE
#if NKENTSEU_SHARED_BUILD
#pragma message("Nkentseu Core: Building as SHARED")
#else
#pragma message("Nkentseu Core: Building as STATIC")
#endif
#endif

// Vérification de cohérence
#if NKENTSEU_BUILDING_CORE && !defined(NKENTSEU_CORE_EXPORTS)
#ifdef NKENTSEU_WARN_MISSING_EXPORT_DEFINE
#warning "Nkentseu Core: Building without NKENTSEU_CORE_EXPORTS defined"
#endif
#endif

// ============================================================
// COMPATIBILITÉ AVEC L'ANCIEN SYSTÈME
// ============================================================

/**
 * @brief Compatibilité avec l'ancienne macro NKENTSEU_API
 * @def NKENTSEU_API
 * @ingroup CoreModule
 *
 * Maintenue pour la compatibilité ascendante.
 */
#ifndef NKENTSEU_API
#define NKENTSEU_API NKENTSEU_CORE_API
#endif

/**
 * @brief Compatibilité avec l'ancienne macro NKENTSEU_CORE_API
 * @def NKENTSEU_CORE_API
 * @ingroup CoreModule
 */
#ifndef NKENTSEU_CORE_API
#define NKENTSEU_CORE_API NK_CORE_API
#endif

#endif // NKENTSEU_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKCOREEXPORT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================