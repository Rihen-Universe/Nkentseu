// -----------------------------------------------------------------------------
// FICHIER: Examples\ModularExports\NkGraphicsExport.h
// DESCRIPTION: Exemple d'export modulaire pour le module Graphics
// AUTEUR: Rihen
// DATE: 2026-02-10
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_EXAMPLES_MODULAREXPORTS_NKGRAPHICSEXPORT_H_INCLUDED
#define NKENTSEU_EXAMPLES_MODULAREXPORTS_NKGRAPHICSEXPORT_H_INCLUDED

// ============================================================
// INCLUDE DU SYSTÈME D'EXPORT PRINCIPAL
// ============================================================

#include "NkExport.h"

// ============================================================
// CONFIGURATION DU MODULE GRAPHICS
// ============================================================

/**
 * @defgroup GraphicsModule Module Graphics
 * @brief Système d'export pour le module de rendu graphique
 */

/**
 * @brief Détection du contexte de build pour Graphics
 * @ingroup GraphicsModule
 */
#ifndef NKENTSEU_BUILDING_GRAPHICS
#if defined(NKENTSEU_GRAPHICS_EXPORTS) || defined(GRAPHICS_EXPORTS) || defined(NKENTSEU_BUILDING_GFX)
#define NKENTSEU_BUILDING_GRAPHICS 1
#else
#define NKENTSEU_BUILDING_GRAPHICS 0
#endif
#endif

// ============================================================
// MACROS D'EXPORT POUR LE MODULE GRAPHICS
// ============================================================

/**
 * @brief API principale du module Graphics
 * @def NKENTSEU_GRAPHICS_API
 * @ingroup GraphicsModule
 *
 * Utilisé pour exporter/importer les classes et fonctions du module Graphics.
 *
 * @example
 * @code
 * class NKENTSEU_GRAPHICS_API Renderer {
 * public:
 *     NKENTSEU_GRAPHICS_API void render();
 * };
 * @endcode
 */
#if NKENTSEU_BUILDING_GRAPHICS
#define NKENTSEU_GRAPHICS_API NKENTSEU_SYMBOL_EXPORT
#elif NKENTSEU_SHARED_BUILD
#define NKENTSEU_GRAPHICS_API NKENTSEU_SYMBOL_IMPORT
#else
#define NKENTSEU_GRAPHICS_API
#endif

/**
 * @brief API C du module Graphics
 * @def NKENTSEU_GRAPHICS_C_API
 * @ingroup GraphicsModule
 */
#define NKENTSEU_GRAPHICS_C_API NKENTSEU_EXTERN_C NKENTSEU_GRAPHICS_API NKENTSEU_CALL

/**
 * @brief Symboles publics du module Graphics
 * @def NKENTSEU_GRAPHICS_PUBLIC
 * @ingroup GraphicsModule
 */
#define NKENTSEU_GRAPHICS_PUBLIC NKENTSEU_GRAPHICS_API

/**
 * @brief Symboles privés du module Graphics
 * @def NKENTSEU_GRAPHICS_PRIVATE
 * @ingroup GraphicsModule
 */
#define NKENTSEU_GRAPHICS_PRIVATE NKENTSEU_SYMBOL_HIDDEN

/**
 * @brief Alias court pour NKENTSEU_GRAPHICS_API
 * @def NK_GFX_API
 * @ingroup GraphicsModule
 */
#define NK_GFX_API NKENTSEU_GRAPHICS_API

// ============================================================
// MACROS SPÉCIFIQUES AU RENDU GRAPHIQUE
// ============================================================

/**
 * @brief API pour les shaders
 * @def NKENTSEU_SHADER_API
 * @ingroup GraphicsModule
 */
#define NKENTSEU_SHADER_API NKENTSEU_GRAPHICS_API

/**
 * @brief API pour les textures
 * @def NKENTSEU_TEXTURE_API
 * @ingroup GraphicsModule
 */
#define NKENTSEU_TEXTURE_API NKENTSEU_GRAPHICS_API

/**
 * @brief API pour les buffers
 * @def NKENTSEU_BUFFER_API
 * @ingroup GraphicsModule
 */
#define NKENTSEU_BUFFER_API NKENTSEU_GRAPHICS_API

// ============================================================
// EXEMPLE D'UTILISATION
// ============================================================

/*
 * UTILISATION DANS UN HEADER PUBLIC:
 *
 * #include "NkGraphicsExport.h"
 *
 * namespace nkentseu {
 * namespace graphics {
 *
 * class NKENTSEU_GRAPHICS_API Renderer {
 * public:
 *     NKENTSEU_GRAPHICS_API Renderer();
 *     NKENTSEU_GRAPHICS_API ~Renderer();
 *
 *     NKENTSEU_GRAPHICS_API void initialize();
 *     NKENTSEU_GRAPHICS_API void render();
 *     NKENTSEU_GRAPHICS_API void shutdown();
 *
 * private:
 *     NKENTSEU_GRAPHICS_PRIVATE void updateInternal(); // Non exportée
 * };
 *
 * class NKENTSEU_SHADER_API Shader {
 * public:
 *     NKENTSEU_SHADER_API bool compile(const char* source);
 * };
 *
 * } // namespace graphics
 * } // namespace nkentseu
 *
 * // API C
 * NKENTSEU_EXTERN_C_BEGIN
 *
 * NKENTSEU_GRAPHICS_C_API void* nkGraphicsCreateRenderer(void);
 * NKENTSEU_GRAPHICS_C_API void nkGraphicsDestroyRenderer(void* renderer);
 * NKENTSEU_GRAPHICS_C_API void nkGraphicsRender(void* renderer);
 *
 * NKENTSEU_EXTERN_C_END
 */

#endif // NKENTSEU_EXAMPLES_MODULAREXPORTS_NKGRAPHICSEXPORT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================