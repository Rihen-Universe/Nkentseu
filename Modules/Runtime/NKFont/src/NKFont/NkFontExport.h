#pragma once
/**
 * @File    NkFontExport.h
 * @Brief   Macros de visibilité / export pour NKFont.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */

// ── Export DLL/SO ─────────────────────────────────────────────────────────────
#if defined(_WIN32) || defined(_WIN64)
#  if defined(NKENTSEU_FONT_BUILD_DLL)
#    define NKENTSEU_FONT_API __declspec(dllexport)
#  elif defined(NKENTSEU_FONT_USE_DLL)
#    define NKENTSEU_FONT_API __declspec(dllimport)
#  else
#    define NKENTSEU_FONT_API
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define NKENTSEU_FONT_API __attribute__((visibility("default")))
#else
#  define NKENTSEU_FONT_API
#endif

// ── Attributs utilitaires ─────────────────────────────────────────────────────
#if defined(__GNUC__) || defined(__clang__)
#  define NKFONT_NODISCARD    __attribute__((warn_unused_result))
#  define NKFONT_INLINE       __attribute__((always_inline)) inline
#  define NKFONT_NOINLINE     __attribute__((noinline))
#  define NKFONT_LIKELY(x)    __builtin_expect(!!(x), 1)
#  define NKFONT_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
#  define NKFONT_NODISCARD    [[nodiscard]]
#  define NKFONT_INLINE       __forceinline
#  define NKFONT_NOINLINE     __declspec(noinline)
#  define NKFONT_LIKELY(x)    (x)
#  define NKFONT_UNLIKELY(x)  (x)
#else
#  define NKFONT_NODISCARD
#  define NKFONT_INLINE       inline
#  define NKFONT_NOINLINE
#  define NKFONT_LIKELY(x)    (x)
#  define NKFONT_UNLIKELY(x)  (x)
#endif

// C++17 [[nodiscard]] override si disponible
#if __cplusplus >= 201703L
#  undef  NKFONT_NODISCARD
#  define NKFONT_NODISCARD [[nodiscard]]
#endif
