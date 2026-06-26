#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorExport.h
// @Brief   Macros d'export/import NKEditorKit (defaut statique, opt-in shared).
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// Convention alignee sur le fix export 2026-06 (defaut STATIQUE) :
//   - build de la lib en .dll  -> definir NKEDITORKIT_BUILD_SHARED_LIB
//   - conso d'une .dll          -> definir NKEDITORKIT_USE_SHARED_LIB
//   - sinon (defaut)            -> statique, macro vide (jamais de dllimport sur
//                                  inline/enum, cf. bug downstream resolu).
// Comme tout Nkentseu est construit en STATIC_LIB (config/modules.jenga), la
// macro se resout a vide partout — ce header reste pret pour un futur passage
// en shared sans toucher au code.
// -----------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
  #if defined(NKEDITORKIT_BUILD_SHARED_LIB)
    #define NKEDITORKIT_API __declspec(dllexport)
  #elif defined(NKEDITORKIT_USE_SHARED_LIB)
    #define NKEDITORKIT_API __declspec(dllimport)
  #else
    #define NKEDITORKIT_API
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #if defined(NKEDITORKIT_BUILD_SHARED_LIB)
    #define NKEDITORKIT_API __attribute__((visibility("default")))
  #else
    #define NKEDITORKIT_API
  #endif
#else
  #define NKEDITORKIT_API
#endif

#if defined(_MSC_VER)
#  define NKEDITORKIT_INLINE __forceinline
#else
#  define NKEDITORKIT_INLINE inline __attribute__((always_inline))
#endif

#include "NKCore/NkTypes.h"
