// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontTypes.h
// DESCRIPTION: Types de base et macros du système NKFont.
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 2.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKFONT_CORE_NKFONTTYPES_H_INCLUDED
#define NK_NKFONT_CORE_NKFONTTYPES_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include <stdint.h>
#include <stddef.h>
#include <string.h>   // memcpy, memset
#include <math.h>     // floorf, ceilf

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"

// ============================================================
// ALIAS DE TYPES INTERNES (préfixe nkft_)
// Les types publics utilisent les alias nkentseu directement.
// ============================================================

using nkft_uint8   = nkentseu::uint8;
using nkft_uint16  = nkentseu::uint16;
using nkft_uint32  = nkentseu::uint32;
using nkft_uint64  = nkentseu::uint64;
using nkft_int8    = nkentseu::int8;
using nkft_int16   = nkentseu::int16;
using nkft_int32   = nkentseu::int32;
using nkft_int64   = nkentseu::int64;
using nkft_float32 = nkentseu::float32;
using nkft_float64 = nkentseu::float64;
using nkft_bool    = nkentseu::nk_bool;
using nkft_size    = nkentseu::usize;

/**
 * @brief Codepoint Unicode (valeur entière d'un caractère Unicode).
 */
using NkFontCodepoint = nkentseu::uint32;

/**
 * @brief Index de glyphe dans la table interne de la fonte.
 */
using NkGlyphId = nkentseu::uint32;

// ============================================================
// CONSTANTES
// ============================================================

/** Valeur sentinelle pour un glyphe invalide. */
static constexpr NkGlyphId NKFONT_INVALID_GLYPH_ID = 0xFFFFFFFFu;

/** Codepoint de remplacement Unicode U+FFFD. */
static constexpr NkFontCodepoint NKFONT_CODEPOINT_REPLACEMENT = 0x0000FFFDu;

/** Codepoint maximal Unicode. */
static constexpr NkFontCodepoint NKFONT_CODEPOINT_MAX = 0x0010FFFFu;

// ============================================================
// MACROS UTILITAIRES (constantes UPPER_SNAKE_CASE, fonctions PascalCase)
// ============================================================

#ifndef NK_FONT_UNUSED
    /// Masque le warning "variable non utilisée".
    #define NK_FONT_UNUSED(x) (void)(x)
#endif

#ifndef NK_FONT_ASSERT
    #include <assert.h>
    /// Assertion personnalisée pour le module NKFont.
    #define NK_FONT_ASSERT(x) assert(x)
#endif

#ifndef NK_FONT_LIKELY
    #if defined(__GNUC__) || defined(__clang__)
        /// Indique au compilateur qu'une condition est probablement vraie.
        #define NK_FONT_LIKELY(x)   __builtin_expect(!!(x), 1)
        /// Indique au compilateur qu'une condition est probablement fausse.
        #define NK_FONT_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define NK_FONT_LIKELY(x)   (x)
        #define NK_FONT_UNLIKELY(x) (x)
    #endif
#endif

// ============================================================
// TYPES MATH (alias depuis NKMath)
// ============================================================

namespace nkentseu {

    /// Rectangle entier.
    using NkRecti = math::NkIntRect;

    /// Rectangle flottant.
    using NkRectf = math::NkFloatRect;

} // namespace nkentseu

#endif // NK_NKFONT_CORE_NKFONTTYPES_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================