#pragma once
/**
 * @File    NkImageExport.h
 * @Brief   Macros de visibilité / export pour NKImage.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

#if defined(_WIN32) || defined(_WIN64)
#  if defined(NKENTSEU_IMAGE_BUILD_DLL)
#    define NKENTSEU_IMAGE_API __declspec(dllexport)
#  elif defined(NKENTSEU_IMAGE_USE_DLL)
#    define NKENTSEU_IMAGE_API __declspec(dllimport)
#  else
#    define NKENTSEU_IMAGE_API
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define NKENTSEU_IMAGE_API __attribute__((visibility("default")))
#else
#  define NKENTSEU_IMAGE_API
#endif

#if __cplusplus >= 201703L
#  define NKIMG_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#  define NKIMG_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#  define NKIMG_NODISCARD _Check_return_
#else
#  define NKIMG_NODISCARD
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define NKIMG_INLINE    __attribute__((always_inline)) inline
#  define NKIMG_NOINLINE  __attribute__((noinline))
#  define NKIMG_LIKELY(x)   __builtin_expect(!!(x),1)
#  define NKIMG_UNLIKELY(x) __builtin_expect(!!(x),0)
#elif defined(_MSC_VER)
#  define NKIMG_INLINE    __forceinline
#  define NKIMG_NOINLINE  __declspec(noinline)
#  define NKIMG_LIKELY(x)   (x)
#  define NKIMG_UNLIKELY(x) (x)
#else
#  define NKIMG_INLINE    inline
#  define NKIMG_NOINLINE
#  define NKIMG_LIKELY(x)   (x)
#  define NKIMG_UNLIKELY(x) (x)
#endif

// Types de base (si NKCore n'est pas disponible)
#ifndef NKENTSEU_TYPES_DEFINED
#  define NKENTSEU_TYPES_DEFINED
#  include <cstdint>
#  include <cstddef>
#include "NKCore/NkTypes.h"
#endif
