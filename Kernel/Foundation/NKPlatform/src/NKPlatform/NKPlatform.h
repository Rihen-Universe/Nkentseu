// =============================================================================
// NKPlatform/NKPlatform.h
// Point d'entrée principal du module NKPlatform.
//
// Design :
//  - Fichier "umbrella header" regroupant tous les exports publics du module
//  - Inclut les headers de détection, types, et utilitaires système
//  - Fournit une interface unifiée pour l'interrogation de la plateforme
//  - Conçu pour être le seul fichier à inclure pour utiliser NKPlatform
//
// Intégration :
//  - Inclut tous les headers publics de NKPlatform dans l'ordre de dépendance
//  - Compatible avec NKCore/NkTypes.h pour les types fondamentaux
//  - Peut être inclus avant ou après NKCore.h selon les besoins
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#pragma once

#ifndef NKENTSEU_PLATFORM_H_INCLUDED
#define NKENTSEU_PLATFORM_H_INCLUDED

// -------------------------------------------------------------------------
// SECTION 1 : DÉTECTION ET CONFIGURATION DE BASE
// -------------------------------------------------------------------------
// Headers fondamentaux pour la détection de l'environnement de compilation.

/**
 * @defgroup PlatformModule Module NKPlatform
 * @brief Détection de la cible (système, architecture, compilateur, rendu) et utilitaires portables de bas niveau.
 * @ingroup FrameworkModules
 * @note Inclut la détection, l'ordre des octets, le journal de fondation, l'export et l'inlining ; NkEnv.h et
 *       NkCPUFeatures.h s'incluent à part. Exemples compilés : Docs/exemples/Foundation/NKPlatform/.
 */
/** @{ */

// Détection : système (NKENTSEU_PLATFORM_*), architecture (NKENTSEU_ARCH_*), compilateur (NKENTSEU_COMPILER_*,
// NKENTSEU_HAS_*), ordre des octets (NkEndianness).
#include "NKPlatform/NkPlatformDetect.h"
#include "NKPlatform/NkArchDetect.h"
#include "NKPlatform/NkCompilerDetect.h"
#include "NKPlatform/NkEndianness.h"
// Rendu et calcul disponibles (NkGraphicsApi, NkGPUVendor), journal de fondation (NK_FOUNDATION_LOG_*).
#include "NKPlatform/NkCGXDetect.h"
#include "NKPlatform/NkFoundationLog.h"
// Export des symboles (NKENTSEU_PLATFORM_API) et inlining (NKENTSEU_FORCE_INLINE, NKENTSEU_NO_INLINE).
#include "NKPlatform/NkPlatformExport.h"
#include "NKPlatform/NkPlatformInline.h"

/** @} */ // End of PlatformModule

#endif // NKENTSEU_PLATFORM_H_INCLUDED

// =============================================================================
// Exemples : les exemples COMPILÉS et exécutés sont dans Docs/exemples/Foundation/NKPlatform/. Ceux qui étaient
// ici en commentaire sont gardés tels quels dans Docs/archives/NKPlatform/exemples_des_entetes.md (à convertir).

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================