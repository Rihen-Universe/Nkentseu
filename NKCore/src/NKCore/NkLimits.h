// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkLimits.h
// DESCRIPTION: Limites numériques sans <limits>
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.1.0
// MODIFICATIONS: Correction types nk_xxx, conventions de nommage
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKLIMITS_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKLIMITS_H_INCLUDED

#include "NkTypes.h"
#include "NkExport.h"

// ============================================================
// LIMITES ENTIERS (Compile-time constants)
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {

/**
 * @brief Limites pour types entiers (template)
 */
template <typename T> struct NkNumericLimits {
	static constexpr nk_bool is_specialized = false;
};

// ========================================
// SPÉCIALISATIONS INT8
// ========================================

template <> struct NkNumericLimits<nk_int8> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = true;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_int8 min() {
		return NKENTSEU_INT8_MIN;
	}
	static constexpr nk_int8 max() {
		return NKENTSEU_INT8_MAX;
	}
	static constexpr nk_int8 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 7;
	static constexpr nk_int32 digits10 = 2;
};

template <> struct NkNumericLimits<nk_uint8> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = false;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_uint8 min() {
		return 0;
	}
	static constexpr nk_uint8 max() {
		return NKENTSEU_UINT8_MAX;
	}
	static constexpr nk_uint8 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 8;
	static constexpr nk_int32 digits10 = 2;
};

// ========================================
// SPÉCIALISATIONS INT16
// ========================================

template <> struct NkNumericLimits<nk_int16> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = true;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_int16 min() {
		return NKENTSEU_INT16_MIN;
	}
	static constexpr nk_int16 max() {
		return NKENTSEU_INT16_MAX;
	}
	static constexpr nk_int16 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 15;
	static constexpr nk_int32 digits10 = 4;
};

template <> struct NkNumericLimits<nk_uint16> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = false;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_uint16 min() {
		return 0;
	}
	static constexpr nk_uint16 max() {
		return NKENTSEU_UINT16_MAX;
	}
	static constexpr nk_uint16 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 16;
	static constexpr nk_int32 digits10 = 4;
};

// ========================================
// SPÉCIALISATIONS INT32
// ========================================

template <> struct NkNumericLimits<nk_int32> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = true;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_int32 min() {
		return NKENTSEU_INT32_MIN;
	}
	static constexpr nk_int32 max() {
		return NKENTSEU_INT32_MAX;
	}
	static constexpr nk_int32 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 31;
	static constexpr nk_int32 digits10 = 9;
};

template <> struct NkNumericLimits<nk_uint32> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = false;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_uint32 min() {
		return 0;
	}
	static constexpr nk_uint32 max() {
		return NKENTSEU_UINT32_MAX;
	}
	static constexpr nk_uint32 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 32;
	static constexpr nk_int32 digits10 = 9;
};

// ========================================
// SPÉCIALISATIONS INT64
// ========================================

template <> struct NkNumericLimits<nk_int64> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = true;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_int64 min() {
		return NKENTSEU_INT64_MIN;
	}
	static constexpr nk_int64 max() {
		return NKENTSEU_INT64_MAX;
	}
	static constexpr nk_int64 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 63;
	static constexpr nk_int32 digits10 = 18;
};

template <> struct NkNumericLimits<nk_uint64> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = false;
	static constexpr nk_bool is_integer = true;

	static constexpr nk_uint64 min() {
		return 0;
	}
	static constexpr nk_uint64 max() {
		return NKENTSEU_UINT64_MAX;
	}
	static constexpr nk_uint64 lowest() {
		return min();
	}

	static constexpr nk_int32 digits = 64;
	static constexpr nk_int32 digits10 = 19;
};

// ========================================
// SPÉCIALISATIONS FLOAT
// ========================================

template <> struct NkNumericLimits<nk_float32> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = true;
	static constexpr nk_bool is_integer = false;
	static constexpr nk_bool is_exact = false;

	static constexpr nk_float32 min();	   // Plus petite valeur normalisée
	static constexpr nk_float32 max();	   // Plus grande valeur
	static constexpr nk_float32 lowest();  // -max()
	static constexpr nk_float32 epsilon(); // Epsilon machine
	static nk_float32 infinity();		   // Infini positif
	static nk_float32 quiet_NaN();		   // NaN silencieux

	static constexpr nk_int32 digits = 24;		// Mantissa bits
	static constexpr nk_int32 digits10 = 6;		// Précision décimale
	static constexpr nk_int32 max_digits10 = 9; // Précision max pour round-trip

	static constexpr nk_int32 radix = 2;
	static constexpr nk_int32 min_exponent = -125;
	static constexpr nk_int32 max_exponent = 128;
	static constexpr nk_int32 min_exponent10 = -37;
	static constexpr nk_int32 max_exponent10 = 38;
};

template <> struct NkNumericLimits<nk_float64> {
	static constexpr nk_bool is_specialized = true;
	static constexpr nk_bool is_signed = true;
	static constexpr nk_bool is_integer = false;
	static constexpr nk_bool is_exact = false;

	static constexpr nk_float64 min();
	static constexpr nk_float64 max();
	static constexpr nk_float64 lowest();
	static constexpr nk_float64 epsilon();
	static nk_float64 infinity();
	static nk_float64 quiet_NaN();

	static constexpr nk_int32 digits = 53;
	static constexpr nk_int32 digits10 = 15;
	static constexpr nk_int32 max_digits10 = 17;

	static constexpr nk_int32 radix = 2;
	static constexpr nk_int32 min_exponent = -1021;
	static constexpr nk_int32 max_exponent = 1024;
	static constexpr nk_int32 min_exponent10 = -307;
	static constexpr nk_int32 max_exponent10 = 308;
};

} // namespace core
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKLIMITS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================