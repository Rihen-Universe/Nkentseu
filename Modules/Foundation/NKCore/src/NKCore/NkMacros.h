// -----------------------------------------------------------------------------
// NKCore/NkMacros.h — Macros utilitaires générales
// Auteur : Rihen  Date : 2026  Version : 1.1.0
// Toutes les macros suivent la convention NKENTSEU_UPPER_SNAKE_CASE.
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKMACROS_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKMACROS_H_INCLUDED

#include "NKPlatform/NkCompilerDetect.h"
#include "NkVersion.h"
#include "NkTypes.h"

// ============================================================
// STRINGIFICATION
// ============================================================

#define NKENTSEU_STRINGIFY_IMPL(x) #x
#define NKENTSEU_STRINGIFY(x)      NKENTSEU_STRINGIFY_IMPL(x)

// ============================================================
// CONCATÉNATION DE TOKENS
// ============================================================

#define NKENTSEU_CONCAT_IMPL(a, b) a##b
#define NKENTSEU_CONCAT(a, b)      NKENTSEU_CONCAT_IMPL(a, b)
#define NKENTSEU_CONCAT3(a, b, c)  NKENTSEU_CONCAT(NKENTSEU_CONCAT(a, b), c)
#define NKENTSEU_CONCAT4(a, b, c, d) NKENTSEU_CONCAT(NKENTSEU_CONCAT3(a, b, c), d)

// ============================================================
// TAILLE & COMPTAGE
// ============================================================

#ifndef NKENTSEU_ARRAY_SIZE
    #define NKENTSEU_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define NKENTSEU_VA_ARGS_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
#define NKENTSEU_VA_ARGS_COUNT(...) \
    NKENTSEU_VA_ARGS_COUNT_IMPL(__VA_ARGS__, 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

// ============================================================
// MANIPULATION DE BITS
// ============================================================

#ifndef NKENTSEU_BIT
    #define NKENTSEU_BIT(x)   (1U  << (x))
#endif
#define NKENTSEU_BIT64(x)     (1ULL << (x))
#define NKENTSEU_BIT_TEST(v, b)   (((v) &  NKENTSEU_BIT(b)) != 0)
#define NKENTSEU_BIT_SET(v, b)    ((v) |=  NKENTSEU_BIT(b))
#define NKENTSEU_BIT_CLEAR(v, b)  ((v) &= ~NKENTSEU_BIT(b))
#define NKENTSEU_BIT_TOGGLE(v, b) ((v) ^=  NKENTSEU_BIT(b))

// ============================================================
// TAILLE MÉMOIRE
// ============================================================

#define NKENTSEU_KILOBYTES(x) ((x) * 1024LL)
#define NKENTSEU_MEGABYTES(x) (NKENTSEU_KILOBYTES(x) * 1024LL)
#define NKENTSEU_GIGABYTES(x) (NKENTSEU_MEGABYTES(x) * 1024LL)
#define NKENTSEU_TERABYTES(x) (NKENTSEU_GIGABYTES(x) * 1024LL)

// ============================================================
// ALIGNEMENT
// ============================================================

#define NKENTSEU_ALIGN_PTR(ptr, alignment) \
    ((void *)NKENTSEU_ALIGN_UP((uintptr_t)(ptr), (alignment)))

// ============================================================
// MIN / MAX / CLAMP / ABS
// ============================================================

#define NK_MIN(a, b)   (((a) < (b)) ? (a) : (b))
#define NK_MAX(a, b)   (((a) > (b)) ? (a) : (b))
#define NK_CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))
#define NK_ABS(x)      (((x) < 0) ? -(x) : (x))

// ============================================================
// SWAP
// ============================================================

#define NK_SWAP(a, b, type)                                  \
    do {                                                     \
        type NKENTSEU_CONCAT(nk_tmp_, __LINE__) = (a);      \
        (a) = (b);                                           \
        (b) = NKENTSEU_CONCAT(nk_tmp_, __LINE__);           \
    } while (0)

// ============================================================
// UNUSED
// ============================================================

#define NKENTSEU_UNUSED(x)          (void)(x)
#define NKENTSEU_UNUSED2(x, y)      NKENTSEU_UNUSED(x); NKENTSEU_UNUSED(y)
#define NKENTSEU_UNUSED3(x, y, z)   NKENTSEU_UNUSED2(x, y); NKENTSEU_UNUSED(z)
#define NKENTSEU_UNUSED4(x, y, z, w) NKENTSEU_UNUSED3(x, y, z); NKENTSEU_UNUSED(w)

// ============================================================
// OFFSET & CONTAINER_OF
// ============================================================

#ifndef NKENTSEU_OFFSETOF
    #if defined(_MSC_VER)
        #define NKENTSEU_OFFSETOF(type, member) offsetof(type, member)
    #else
        #define NKENTSEU_OFFSETOF(type, member) __builtin_offsetof(type, member)
    #endif
#endif

#define NKENTSEU_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - NKENTSEU_OFFSETOF(type, member)))

// ============================================================
// DO-WHILE(0) BLOCKS
// ============================================================

#define NKENTSEU_BLOCK_BEGIN do {
#define NKENTSEU_BLOCK_END   } while (0)

// ============================================================
// SCOPE GUARD (DEFER)
// ============================================================

#define NKENTSEU_DEFER_CONCAT(a, b)  a##b
#define NKENTSEU_DEFER_VARNAME(a, b) NKENTSEU_DEFER_CONCAT(a, b)
#if defined(__GNUC__) || defined(__clang__)
    #define NKENTSEU_DEFER(code) \
        auto NKENTSEU_DEFER_VARNAME(nk_defer_, __LINE__) = \
            nkentseu::NkScopeGuard([&]() { code; })
#else
    #define NKENTSEU_DEFER(code)
#endif

// ============================================================
// STATIC_ASSERT
// ============================================================

#ifndef NKENTSEU_STATIC_ASSERT
    #if defined(__cplusplus) && (__cplusplus >= 201103L || \
        (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L))
        #define NKENTSEU_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #define NKENTSEU_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
    #else
        #define NKENTSEU_STATIC_ASSERT(cond, msg) \
            typedef char NKENTSEU_CONCAT(nk_static_assert_, __LINE__)[(cond) ? 1 : -1]
    #endif
#endif

// ============================================================
// TYPE CHECKING
// ============================================================

#if defined(__GNUC__) || defined(__clang__)
    #define NKENTSEU_SAME_TYPE(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
#else
    #define NKENTSEU_SAME_TYPE(a, b) (0)
#endif

#define NKENTSEU_SIZEOF_TYPE(type)          sizeof(type)
#define NKENTSEU_SIZEOF_MEMBER(type, member) sizeof(((type *)0)->member)

// ============================================================
// ARITHMÉTIQUE SÉCURISÉE
// ============================================================

#define NKENTSEU_WILL_ADD_OVERFLOW(a, b, max) ((a) > (max) - (b))
#define NKENTSEU_WILL_MUL_OVERFLOW(a, b, max) ((b) != 0 && (a) > (max) / (b))

// ============================================================
// MESSAGES DE COMPILATION
// ============================================================

#if defined(_MSC_VER)
    #define NKENTSEU_COMPILE_MESSAGE(msg) __pragma(message(msg))
#elif defined(__GNUC__) || defined(__clang__)
    #define NKENTSEU_COMPILE_MESSAGE(msg) _Pragma(NKENTSEU_STRINGIFY(message msg))
#else
    #define NKENTSEU_COMPILE_MESSAGE(msg)
#endif

#define NKENTSEU_TODO(msg)  NKENTSEU_COMPILE_MESSAGE("TODO: "  msg)
#define NKENTSEU_FIXME(msg) NKENTSEU_COMPILE_MESSAGE("FIXME: " msg)

// ============================================================
// STATIC ARRAY (C99)
// ============================================================

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    #define NKENTSEU_STATIC_ARRAY(size) static size
#else
    #define NKENTSEU_STATIC_ARRAY(size)
#endif

// ============================================================
// ENCODAGE DE VERSION
// ============================================================

#define NKENTSEU_VERSION_ENCODE(major, minor, patch) \
    (((major) << 24) | ((minor) << 16) | (patch))
#define NKENTSEU_VERSION_MAJOR(v) (((v) >> 24) & 0xFF)
#define NKENTSEU_VERSION_MINOR(v) (((v) >> 16) & 0xFF)
#define NKENTSEU_VERSION_PATCH(v) ((v) & 0xFFFF)

// ============================================================
// FOR_EACH VARIADIQUE (max 4 args)
// ============================================================

#define NKENTSEU_FOR_EACH_IMPL1(f, a)          f(a)
#define NKENTSEU_FOR_EACH_IMPL2(f, a, b)       f(a); f(b)
#define NKENTSEU_FOR_EACH_IMPL3(f, a, b, c)    f(a); f(b); f(c)
#define NKENTSEU_FOR_EACH_IMPL4(f, a, b, c, d) f(a); f(b); f(c); f(d)
#define NKENTSEU_FOR_EACH_IMPL(f, count, ...) \
    NKENTSEU_FOR_EACH_IMPL##count(f, __VA_ARGS__)
#define NKENTSEU_FOR_EACH(f, ...) \
    NKENTSEU_FOR_EACH_IMPL(f, NKENTSEU_VA_ARGS_COUNT(__VA_ARGS__), __VA_ARGS__)

// ============================================================
// UTILITAIRES POINTEURS
// ============================================================

#define NKENTSEU_MASK_ADDRESS(ptr) \
    ((void *)((uintptr_t)(ptr) & 0xFFFFF000))
#define NKENTSEU_POINTER_DISTANCE(ptr1, ptr2) \
    ((intptr_t)((const char *)(ptr2) - (const char *)(ptr1)))

// ============================================================
// CONVERSIONS ANGLES
// ============================================================

#define NKENTSEU_DEGREES_TO_RADIANS(d) ((d) * 3.14159265358979323846 / 180.0)
#define NKENTSEU_RADIANS_TO_DEGREES(r) ((r) * 180.0 / 3.14159265358979323846)

// ============================================================
// HINTS D'OPTIMISATION (délégation à NkPlatformInline.h si déjà défini)
// ============================================================

#ifndef NKENTSEU_LIKELY
    #if defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_LIKELY(x)   __builtin_expect(!!(x), 1)
        #define NKENTSEU_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define NKENTSEU_LIKELY(x)   (x)
        #define NKENTSEU_UNLIKELY(x) (x)
    #endif
#endif

#ifndef NKENTSEU_UNREACHABLE
    #if defined(NKENTSEU_COMPILER_MSVC)
        #define NKENTSEU_UNREACHABLE() __assume(0)
    #elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
        #define NKENTSEU_UNREACHABLE() __builtin_unreachable()
    #else
        #define NKENTSEU_UNREACHABLE()
    #endif
#endif

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKMACROS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================
