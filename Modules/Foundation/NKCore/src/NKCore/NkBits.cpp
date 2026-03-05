// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkBits.cpp
// DESCRIPTION: Implémentation des fonctions de manipulation de bits
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.1.0
// MODIFICATIONS: Correction types nk_xxx
// -----------------------------------------------------------------------------

#include "NkBits.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {

// ============================================================
// NEXT POWER OF TWO - 32-BIT
// ============================================================

nk_uint32 NkBits::NextPowerOfTwo(nk_uint32 value) {
	if (value == 0)
		return 1;
	if (IsPowerOfTwo(value))
		return value;

	// Algorithme: propager le bit le plus élevé vers la droite
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;

	return value;
}

// ============================================================
// NEXT POWER OF TWO - 64-BIT
// ============================================================

nk_uint64 NkBits::NextPowerOfTwo(nk_uint64 value) {
	if (value == 0)
		return 1;
	if (IsPowerOfTwo(value))
		return value;

	// Algorithme: propager le bit le plus élevé vers la droite
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
	value++;

	return value;
}

} // namespace core
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================