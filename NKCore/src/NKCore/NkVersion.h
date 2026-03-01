// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkVersion.h
// DESCRIPTION: Versioning du framework NKCore
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKVERSION_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKVERSION_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkMacros.h"

// ============================================================
// VERSION NKCORE
// ============================================================

/**
 * @defgroup CoreVersion Version NKCore
 * @brief Macros définissant la version du framework NKCore
 */

/**
 * @brief Version majeure (breaking changes)
 * @ingroup CoreVersion
 */
#ifndef NKENTSEU_VERSION_CORE_MAJOR
#define NKENTSEU_VERSION_CORE_MAJOR 1
#endif

/**
 * @brief Version mineure (nouvelles fonctionnalités)
 * @ingroup CoreVersion
 */
#ifndef NKENTSEU_VERSION_CORE_MINOR
#define NKENTSEU_VERSION_CORE_MINOR 0
#endif

/**
 * @brief Version patch (bug fixes)
 * @ingroup CoreVersion
 */
#ifndef NKENTSEU_VERSION_CORE_PATCH
#define NKENTSEU_VERSION_CORE_PATCH 0
#endif

/**
 * @brief Version complète encodée (32-bit)
 * @note Format: 0xMMmmpppp (Major, minor, patch)
 * @ingroup CoreVersion
 */
#define NKENTSEU_VERSION_CORE                                                                                          \
	NkVersionEncode(NKENTSEU_VERSION_CORE_MAJOR, NKENTSEU_VERSION_CORE_MINOR, NKENTSEU_VERSION_CORE_PATCH)

/**
 * @brief Version sous forme de chaîne
 * @ingroup CoreVersion
 */
#define NKENTSEU_VERSION_CORE_STRING                                                                                   \
	NkStringify(NKENTSEU_VERSION_CORE_MAJOR) "." NkStringify(NKENTSEU_VERSION_CORE_MINOR) "." NkStringify(             \
		NKENTSEU_VERSION_CORE_PATCH)

/**
 * @brief Nom complet du framework
 * @ingroup CoreVersion
 */
#define NKENTSEU_FRAMEWORK_CORE_NAME "NKCore"

/**
 * @brief Nom complet avec version
 * @ingroup CoreVersion
 */
#define NKENTSEU_FRAMEWORK_CORE_FULL_NAME NKENTSEU_FRAMEWORK_CORE_NAME " v" NKENTSEU_VERSION_CORE_STRING

// ============================================================
// BUILD INFORMATION
// ============================================================

/**
 * @defgroup BuildInfo Informations de Build
 * @brief Macros avec informations de compilation
 */

/**
 * @brief Date de compilation (format: "YYYY-MM-DD")
 * @ingroup BuildInfo
 */
#define NKENTSEU_BUILD_DATE __DATE__

/**
 * @brief Heure de compilation (format: "HH:MM:SS")
 * @ingroup BuildInfo
 */
#define NKENTSEU_BUILD_TIME __TIME__

/**
 * @brief Timestamp de compilation complet
 * @ingroup BuildInfo
 */
#define NKENTSEU_BUILD_TIMESTAMP NKENTSEU_BUILD_DATE " " NKENTSEU_BUILD_TIME

/**
 * @brief Numéro de build (peut être surchargé par système de build)
 * @ingroup BuildInfo
 */
#ifndef NKENTSEU_BUILD_NUMBER
#define NKENTSEU_BUILD_NUMBER 0
#endif

// ============================================================
// VERSION API (peut différer de la version framework)
// ============================================================

/**
 * @defgroup APIVersion Version API
 * @brief Macros pour la version de l'API publique
 */

/**
 * @brief Définit la version majeure de l'API
 * @ingroup APIVersion
 */
#ifndef NKENTSEU_API_VERSION_MAJOR
#define NKENTSEU_API_VERSION_MAJOR 1
#endif

/**
 * @brief Définit la version mineure de l'API
 * @ingroup APIVersion
 */
#ifndef NKENTSEU_API_VERSION_MINOR
#define NKENTSEU_API_VERSION_MINOR 0
#endif

/**
 * @brief Définit la version patch de l'API
 * @ingroup APIVersion
 */
#ifndef NKENTSEU_API_VERSION_PATCH
#define NKENTSEU_API_VERSION_PATCH 0
#endif

/**
 * @brief Version API complète encodée
 * @ingroup APIVersion
 */
#define NKENTSEU_API_VERSION                                                                                           \
	NkVersionEncode(NKENTSEU_API_VERSION_MAJOR, NKENTSEU_API_VERSION_MINOR, NKENTSEU_API_VERSION_PATCH)

/**
 * @brief Macro pour marquer une API comme stable
 * @ingroup APIVersion
 */
#define NKENTSEU_API_STABLE

/**
 * @brief Macro pour marquer une API comme expérimentale
 * @ingroup APIVersion
 */
#define NKENTSEU_API_EXPERIMENTAL NKENTSEU_DEPRECATED_MSG("This API is experimental and may change")

/**
 * @brief Macro pour marquer une API comme interne (non publique)
 * @ingroup APIVersion
 */
#define NKENTSEU_API_INTERNAL NKENTSEU_HIDDEN

// ============================================================
// COMPATIBILITÉ VERSION
// ============================================================

/**
 * @defgroup VersionCompatibility Compatibilité de Version
 * @brief Macros pour vérifier la compatibilité de version
 */

/**
 * @brief Macro pour vérifier la version minimale requise
 * @param major Version majeure minimale
 * @param minor Version mineure minimale
 * @param patch Version patch minimale
 * @return true si version actuelle >= version demandée
 * @example #if NKENTSEU_VERSION_AT_LEAST(1, 2, 0)
 * @ingroup VersionCompatibility
 */
#define NKENTSEU_VERSION_AT_LEAST(major, minor, patch) (NKENTSEU_VERSION_CORE >= NkVersionEncode(major, minor, patch))

/**
 * @brief Macro pour vérifier version exacte
 * @param major Version majeure exacte
 * @param minor Version mineure exacte
 * @param patch Version patch exacte
 * @return true si version actuelle == version spécifiée
 * @ingroup VersionCompatibility
 */
#define NKENTSEU_VERSION_EQUALS(major, minor, patch) (NKENTSEU_VERSION_CORE == NkVersionEncode(major, minor, patch))

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKVERSION_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================