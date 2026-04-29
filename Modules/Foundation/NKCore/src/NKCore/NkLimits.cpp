// =============================================================================
// NKCore/NkLimits.cpp
// Implémentation des méthodes non-constexpr pour limites flottantes.
//
// Design :
//  - Implémentation des valeurs spéciales IEEE 754 (infinity, NaN) via union
//  - Manipulation bit-level portable pour nk_float32 et nk_float64
//  - Aucune dépendance à <cmath> ou <limits> pour portabilité maximale
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "NkLimits.h"

// -------------------------------------------------------------------------
// SECTION 1 : ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------

namespace nkentseu {

    // ====================================================================
    // SECTION 2 : IMPLÉMENTATIONS NK_FLOAT32
    // ====================================================================
    // Méthodes non-constexpr pour nk_float32 nécessitant manipulation bit-level.

    // Union pour interprétation bit-level de nk_float32 (IEEE 754 binary32)
    // Layout : [1 bit signe][8 bits exposant][23 bits mantisse]
    union NkFloat32Bits {
        nk_float32 asFloat;  ///< Représentation flottante
        nk_uint32 asUInt;    ///< Représentation entière pour manipulation bit-level
    };

    nk_float32 NkNumericLimits<nk_float32>::infinity() noexcept {
        // IEEE 754 binary32 : +∞ = signe=0, exposant=0xFF (255), mantisse=0
        // Représentation hexadécimale : 0x7F800000
        NkFloat32Bits bits;
        bits.asUInt = 0x7F800000u;
        return bits.asFloat;
    }

    nk_float32 NkNumericLimits<nk_float32>::quiet_NaN() noexcept {
        // IEEE 754 binary32 : qNaN = signe=0, exposant=0xFF, mantisse!=0, bit MSB=1
        // Représentation hexadécimale : 0x7FC00000 (bit de silence à 1)
        NkFloat32Bits bits;
        bits.asUInt = 0x7FC00000u;
        return bits.asFloat;
    }

    // ====================================================================
    // SECTION 3 : IMPLÉMENTATIONS NK_FLOAT64
    // ====================================================================
    // Méthodes non-constexpr pour nk_float64 nécessitant manipulation bit-level.

    // Union pour interprétation bit-level de nk_float64 (IEEE 754 binary64)
    // Layout : [1 bit signe][11 bits exposant][52 bits mantisse]
    union NkFloat64Bits {
        nk_float64 asFloat;  ///< Représentation flottante
        nk_uint64 asUInt;    ///< Représentation entière pour manipulation bit-level
    };

    nk_float64 NkNumericLimits<nk_float64>::infinity() noexcept {
        // IEEE 754 binary64 : +∞ = signe=0, exposant=0x7FF (2047), mantisse=0
        // Représentation hexadécimale : 0x7FF0000000000000
        NkFloat64Bits bits;
        bits.asUInt = 0x7FF0000000000000ULL;
        return bits.asFloat;
    }

    nk_float64 NkNumericLimits<nk_float64>::quiet_NaN() noexcept {
        // IEEE 754 binary64 : qNaN = signe=0, exposant=0x7FF, mantisse!=0, bit MSB=1
        // Représentation hexadécimale : 0x7FF8000000000000 (bit de silence à 1)
        NkFloat64Bits bits;
        bits.asUInt = 0x7FF8000000000000ULL;
        return bits.asFloat;
    }

} // namespace nkentseu

// =============================================================================
// NOTES TECHNIQUES SUR LES VALEURS SPÉCIALES IEEE 754
// =============================================================================
//
// Format binary32 (nk_float32) :
//   - Bit 31      : signe (0 = positif, 1 = négatif)
//   - Bits 30-23  : exposant biaisé (bias = 127)
//   - Bits 22-0   : mantisse (fraction, bit implicite = 1 pour valeurs normalisées)
//
// Valeurs spéciales binary32 :
//   - +0.0        : 0x00000000
//   - -0.0        : 0x80000000
//   - +∞          : 0x7F800000 (exposant max, mantisse nulle)
//   - -∞          : 0xFF800000 (signe + exposant max, mantisse nulle)
//   - qNaN        : 0x7FC00000 (exposant max, mantisse non-nulle, bit 22 = 1)
//   - sNaN        : 0x7F800001 (exposant max, mantisse non-nulle, bit 22 = 0)
//
// Format binary64 (nk_float64) :
//   - Bit 63      : signe
//   - Bits 62-52  : exposant biaisé (bias = 1023)
//   - Bits 51-0   : mantisse (bit implicite = 1 pour valeurs normalisées)
//
// Valeurs spéciales binary64 :
//   - +0.0        : 0x0000000000000000
//   - -0.0        : 0x8000000000000000
//   - +∞          : 0x7FF0000000000000
//   - -∞          : 0xFFF0000000000000
//   - qNaN        : 0x7FF8000000000000
//   - sNaN        : 0x7FF0000000000001
//
// Remarque sur constexpr :
//   En C++11/14/17, la manipulation d'union pour type-punning n'est pas constexpr.
//   En C++20, std::bit_cast<T>(U) permettrait des implémentations constexpr.
//   Pour une compatibilité maximale, ces méthodes restent non-constexpr.
//
// Alternative portable (si union pose problème) :
//   Utiliser memcpy pour copier les bits, bien que moins efficace :
//   @code
//   nk_float32 infinity() noexcept {
//       nk_uint32 bits = 0x7F800000u;
//       nk_float32 result;
//       memcpy(&result, &bits, sizeof(result));
//       return result;
//   }
//   @endcode
//
// =============================================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================