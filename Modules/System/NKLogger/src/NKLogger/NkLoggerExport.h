// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Export.h
// DESCRIPTION: Configuration d'exportation de symboles pour le module NkLogger.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#pragma once

#include "NKCore/NkCoreExport.h"
#include "NkCompilerDetect.h"

// -----------------------------------------------------------------------------
// MACROS D'EXPORTATION/IMPORTATION POUR LOGGER
// -----------------------------------------------------------------------------

// Pour les bibliothèques statiques, NKLOGGER_API est toujours vide
#define NKLOGGER_API

// -----------------------------------------------------------------------------
// MACROS D'INLINING POUR LOGGER
// -----------------------------------------------------------------------------

#if defined(NKENTSEU_COMPILER_MSVC)
#define NKLOGGER_FORCE_INLINE __forceinline
#define NKLOGGER_NO_INLINE __declspec(noinline)

#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NKLOGGER_FORCE_INLINE __attribute__((always_inline)) inline
#define NKLOGGER_NO_INLINE __attribute__((noinline))

#else
#define NKLOGGER_FORCE_INLINE inline
#define NKLOGGER_NO_INLINE
#endif

// Macro d'inlining standard pour NkLogger
#define NKLOGGER_INLINE inline