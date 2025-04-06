// @File Inline.h
// @Description Macros pour le contrôle d'inlining et de gestion des exceptions
// @Author TEUGUIA TADJUIDJE Rodolf Séderis
// @Date [AAAA-MM-JJ]
// @License Rihen

#pragma once

///////////////////////////////////////////////////////////////////////////////
// Contrôle d'optimisation et de visibilité
///////////////////////////////////////////////////////////////////////////////

// Compilateurs Microsoft
#if defined(_MSC_VER)
    #define NKENTSEU_FORCE_INLINE __forceinline
    #define NKENTSEU_NEVER_INLINE __declspec(noinline)
    #define NKENTSEU_NOINLINE __declspec(noinline)
    
// Compilateurs Clang/LLVM
#elif defined(__clang__)
    #define NKENTSEU_FORCE_INLINE __attribute__((always_inline)) inline
    #define NKENTSEU_NEVER_INLINE __attribute__((noinline))
    #define NKENTSEU_NOINLINE __attribute__((noinline))
    
// Compilateurs GCC/G++
#elif defined(__GNUC__) || defined(__GNUG__)
    #define NKENTSEU_FORCE_INLINE __attribute__((always_inline)) inline
    #define NKENTSEU_NEVER_INLINE __attribute__((noinline))
    #define NKENTSEU_NOINLINE __attribute__((noinline))
    
// Autres compilateurs
#else
    #define NKENTSEU_FORCE_INLINE inline
    #define NKENTSEU_NEVER_INLINE
    #define NKENTSEU_NOINLINE
#endif

///////////////////////////////////////////////////////////////////////////////
// Gestion des exceptions (noexcept)
///////////////////////////////////////////////////////////////////////////////
#if (__cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
    #define NKENTSEU_NOEXCEPT noexcept
#elif defined(_MSC_VER) && (_MSC_VER >= 1900)
    #define NKENTSEU_NOEXCEPT noexcept
#elif defined(__clang__) && __has_feature(cxx_noexcept)
    #define NKENTSEU_NOEXCEPT noexcept
#elif defined(__GNUC__) && (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6)
    #define NKENTSEU_NOEXCEPT noexcept
#else
    #define NKENTSEU_NOEXCEPT throw()
#endif

// Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
// Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
// © Rihen 2025 - Tous droits réservés.