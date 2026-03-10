#pragma once
// =============================================================================
// NkTimeExport.h — Macros d'export/import et utilitaires de compilation NKTime
//
// NKENTSEU_TIME_API        : export/import DLL ou vide pour static (défaut)
// NKTIME_NODISCARD  : [[nodiscard]] si C++17+
// NKTIME_INLINE     : force-inline portable
// NKTIME_LIKELY /
// NKTIME_UNLIKELY   : hints de prédiction de branche
//
// Pour un build DLL : définir NKTIME_SHARED
//   - Côté bibliothèque : définir NKTIME_BUILD en plus de NKTIME_SHARED
//   - Côté client       : définir NKTIME_SHARED uniquement
// Pour un build statique : ne rien définir (défaut).
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"

// ── Export / Import ──────────────────────────────────────────────────────────
#if defined(NKTIME_SHARED)
#  if defined(NKTIME_BUILD)
#    if defined(NKENTSEU_PLATFORM_WINDOWS)
#      define NKENTSEU_TIME_API __declspec(dllexport)
#    elif defined(__GNUC__) || defined(__clang__)
#      define NKENTSEU_TIME_API __attribute__((visibility("default")))
#    else
#      define NKENTSEU_TIME_API
#    endif
#  else
#    if defined(NKENTSEU_PLATFORM_WINDOWS)
#      define NKENTSEU_TIME_API __declspec(dllimport)
#    else
#      define NKENTSEU_TIME_API
#    endif
#  endif
#else
#  define NKENTSEU_TIME_API  // Static library — pas d'annotation de visibilité
#endif

// ── [[nodiscard]] ────────────────────────────────────────────────────────────
#if defined(__cplusplus) && __cplusplus >= 201703L
#  define NKTIME_NODISCARD [[nodiscard]]
#else
#  define NKTIME_NODISCARD
#endif

// ── Force-inline ─────────────────────────────────────────────────────────────
#if defined(_MSC_VER)
#  define NKTIME_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define NKTIME_INLINE __attribute__((always_inline)) inline
#else
#  define NKTIME_INLINE inline
#endif

// ── Branch prediction hints ──────────────────────────────────────────────────
#if defined(__GNUC__) || defined(__clang__)
#  define NKTIME_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define NKTIME_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define NKTIME_LIKELY(x)   (x)
#  define NKTIME_UNLIKELY(x) (x)
#endif