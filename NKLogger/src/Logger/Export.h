// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Export.h
// DESCRIPTION: Configuration d'exportation de symboles pour le module Logger.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKCore/NkCoreExport.h"
#include "NkCompilerDetect.h"

// -----------------------------------------------------------------------------
// MACROS D'EXPORTATION/IMPORTATION POUR LOGGER
// -----------------------------------------------------------------------------

// Pour les bibliothèques statiques, LOGGER_API est toujours vide
#define LOGGER_API

// -----------------------------------------------------------------------------
// MACROS D'INLINING POUR LOGGER
// -----------------------------------------------------------------------------

#if defined(NKENTSEU_COMPILER_MSVC)
#define LOGGER_FORCE_INLINE __forceinline
#define LOGGER_NO_INLINE __declspec(noinline)

#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define LOGGER_FORCE_INLINE __attribute__((always_inline)) inline
#define LOGGER_NO_INLINE __attribute__((noinline))

#else
#define LOGGER_FORCE_INLINE inline
#define LOGGER_NO_INLINE
#endif

// Macro d'inlining standard pour Logger
#define LOGGER_INLINE inline