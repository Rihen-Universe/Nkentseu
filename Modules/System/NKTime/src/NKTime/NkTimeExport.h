#pragma once
// =============================================================================
// NkTimeExport.h — Macros d'export/import pour NKTime
// NKTIME_API est vide pour les builds statiques (défaut).
// Définir NKTIME_SHARED pour un build DLL/SO.
// =============================================================================
#include "NkPlatformDetect.h"

#if defined(NKTIME_SHARED)
#  if defined(NKTIME_BUILD)
#    if defined(NKENTSEU_PLATFORM_WINDOWS)
#      define NKTIME_API __declspec(dllexport)
#    elif defined(__GNUC__) || defined(__clang__)
#      define NKTIME_API __attribute__((visibility("default")))
#    else
#      define NKTIME_API
#    endif
#  else
#    if defined(NKENTSEU_PLATFORM_WINDOWS)
#      define NKTIME_API __declspec(dllimport)
#    else
#      define NKTIME_API
#    endif
#  endif
#else
// Static library: no visibility annotation needed.
#  define NKTIME_API
#endif
