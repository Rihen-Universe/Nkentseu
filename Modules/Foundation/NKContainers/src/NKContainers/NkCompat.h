#pragma once

#ifndef NKENTSEU_CONTAINERS_NKCOMPAT_H_INCLUDED
#define NKENTSEU_CONTAINERS_NKCOMPAT_H_INCLUDED

#include <new>

#if !defined(NK_CPP11) && (__cplusplus >= 201103L)
#define NK_CPP11 1
#endif

#if !defined(NK_CPP17) && (__cplusplus >= 201703L)
#define NK_CPP17 1
#endif

#if !defined(NK_CONSTEXPR)
#define NK_CONSTEXPR constexpr
#endif

#if !defined(NK_NOEXCEPT)
#define NK_NOEXCEPT noexcept
#endif

#if !defined(NK_INLINE)
#define NK_INLINE inline
#endif

#if !defined(NKENTSEU_CORE_API)
#define NKENTSEU_CORE_API
#endif

#if !defined(NK_ASSERT)
#define NK_ASSERT(expr) NKENTSEU_ASSERT(expr)
#endif

#if !defined(NK_ASSERT_MSG)
#define NK_ASSERT_MSG(expr, message) NKENTSEU_ASSERT_MSG(expr, message)
#endif

#if !defined(NK_PLATFORM_WINDOWS) && defined(NKENTSEU_PLATFORM_WINDOWS)
#define NK_PLATFORM_WINDOWS 1
#endif

#if !defined(NK_STRING_SSO_SIZE)
#define NK_STRING_SSO_SIZE 24
#endif

#ifdef NkSwap
#undef NkSwap
#endif

#ifdef NkForEach
#undef NkForEach
#endif

#ifdef NkAllocAligned
#undef NkAllocAligned
#endif

#ifdef NkFreeAligned
#undef NkFreeAligned
#endif

#endif // NKENTSEU_CONTAINERS_NKCOMPAT_H_INCLUDED
