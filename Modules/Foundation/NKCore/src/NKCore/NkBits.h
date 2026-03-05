// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkBits.h
// DESCRIPTION: Manipulation de bits avancée
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.2.0
// MODIFICATIONS:
//   - Corrections des types et includes
//   - Suppression des doublons
//   - Organisation logique des sections
//   - Adaptation aux types NK
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKBITS_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKBITS_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkCompilerDetect.h"
#include "NkTypes.h"
#include "NkInline.h"
#include "NkExport.h"
#include "NkPlatform.h"
#include "Assert/NkAssert.h"

#include <cstdint>
#include <cstdlib>

#include <stdlib.h>
#if defined(_MSC_VER) || defined(_WIN32)
#include <intrin.h> // MSVC/Windows intrinsics only
#endif
// ============================================================
// MACROS FONDAMENTALES POUR MANIPULATION DE BITS
// ============================================================

/**
 * @defgroup BitMacros Macros de Manipulation de Bits
 * @brief Macros pour opérations bitwise optimisées
 */

// ------------------------------------------------------------------
// POPULATION COUNT (Compter les bits à 1)
// ------------------------------------------------------------------

/**
 * @brief Compte les bits à 1 dans un entier 32-bit
 * @def NK_POPCOUNT32(x)
 * @param x Entier 32-bit
 * @return Nombre de bits à 1
 * @ingroup BitMacros
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NK_POPCOUNT32(x) __popcnt((nkentseu::core::nk_uint32)(x))
#define NK_POPCOUNT64(x) __popcnt64((nkentseu::core::nk_uint64)(x))
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NK_POPCOUNT32(x) __builtin_popcount((nkentseu::core::nk_uint32)(x))
#define NK_POPCOUNT64(x) __builtin_popcountll((nkentseu::core::nk_uint64)(x))
#else
// Implémentation software fallback
#define NK_POPCOUNT32(x) nkentseu::core::NkBits::CountBitsSoftware((nkentseu::core::nk_uint32)(x))
#define NK_POPCOUNT64(x) nkentseu::core::NkBits::CountBitsSoftware((nkentseu::core::nk_uint64)(x))
#endif

// ------------------------------------------------------------------
// COUNT TRAILING ZEROS (Compter les zéros à droite)
// ------------------------------------------------------------------

/**
 * @brief Macro pour compter les zéros à droite (32-bit)
 * @def NK_CTZ32(x)
 * @param x Entier 32-bit
 * @return Nombre de zéros à droite (0-31)
 * @note Retourne 32 si x = 0
 * @ingroup BitMacros
 */
NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint32 NK_CTZ32(nkentseu::core::nk_uint32 x) {
	if (x == 0)
		return 32;

#if defined(NKENTSEU_COMPILER_MSVC)
	unsigned long index;
	_BitScanForward(&index, x);
	return (nkentseu::core::nk_uint32)index;
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	return (nkentseu::core::nk_uint32)__builtin_ctz(x);
#else
	// Implémentation software fallback
	nkentseu::core::nk_uint32 count = 0;
	while ((x & 1) == 0) {
		++count;
		x >>= 1;
	}
	return count;
#endif
}

/**
 * @brief Macro pour compter les zéros à droite (64-bit)
 * @def NK_CTZ64(x)
 * @param x Entier 64-bit
 * @return Nombre de zéros à droite (0-63)
 * @note Retourne 64 si x = 0
 * @ingroup BitMacros
 */
NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint32 NK_CTZ64(nkentseu::core::nk_uint64 x) {
	if (x == 0)
		return 64;

#if defined(NKENTSEU_COMPILER_MSVC)
#if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_ARM64)
	unsigned long index;
	_BitScanForward64(&index, x);
	return (nkentseu::core::nk_uint32)index;
#else
	// Fallback pour MSVC 32-bit
	unsigned long index;
	if (_BitScanForward(&index, (nkentseu::core::nk_uint32)(x & 0xFFFFFFFF))) {
		return (nkentseu::core::nk_uint32)index;
	}
	if (_BitScanForward(&index, (nkentseu::core::nk_uint32)(x >> 32))) {
		return (nkentseu::core::nk_uint32)(index + 32);
	}
	return 64;
#endif
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	return (nkentseu::core::nk_uint32)__builtin_ctzll(x);
#else
	// Implémentation software fallback
	nkentseu::core::nk_uint32 count = 0;
	while ((x & 1) == 0) {
		++count;
		x >>= 1;
	}
	return count;
#endif
}

// ------------------------------------------------------------------
// COUNT LEADING ZEROS (Compter les zéros à gauche)
// ------------------------------------------------------------------

/**
 * @brief Macro pour compter les zéros à gauche (32-bit)
 * @def NK_CLZ32(x)
 * @param x Entier 32-bit
 * @return Nombre de zéros à gauche (0-31)
 * @note Retourne 32 si x = 0
 * @ingroup BitMacros
 */
NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint32 NK_CLZ32(nkentseu::core::nk_uint32 x) {
	if (x == 0)
		return 32;

#if defined(NKENTSEU_COMPILER_MSVC)
	unsigned long index;
	_BitScanReverse(&index, x);
	return (nkentseu::core::nk_uint32)(31 - index);
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	return (nkentseu::core::nk_uint32)__builtin_clz(x);
#else
	// Implémentation software fallback
	nnkentseu::core::k_uint32 count = 0;
	nkentseu::core::nk_uint32 mask = 0x80000000;
	while ((x & mask) == 0) {
		++count;
		mask >>= 1;
	}
	return count;
#endif
}

/**
 * @brief Macro pour compter les zéros à gauche (64-bit)
 * @def NK_CLZ64(x)
 * @param x Entier 64-bit
 * @return Nombre de zéros à gauche (0-63)
 * @note Retourne 64 si x = 0
 * @ingroup BitMacros
 */
NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint32 NK_CLZ64(nkentseu::core::nk_uint64 x) {
	if (x == 0)
		return 64;

#if defined(NKENTSEU_COMPILER_MSVC)
#if defined(NKENTSEU_ARCH_X86_64) || defined(NKENTSEU_ARCH_ARM64)
	unsigned long index;
	_BitScanReverse64(&index, x);
	return (nkentseu::core::nk_uint32)(63 - index);
#else
	// Fallback pour MSVC 32-bit
	if (x >> 32) {
		return NK_CLZ32((nkentseu::core::nk_uint32)(x >> 32));
	} else {
		return NK_CLZ32((nkentseu::core::nk_uint32)x) + 32;
	}
#endif
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
	return (nkentseu::core::nk_uint32)__builtin_clzll(x);
#else
	// Implémentation software fallback
	nkentseu::core::nk_uint32 count = 0;
	nkentseu::core::nk_uint64 mask = (nkentseu::core::nk_uint64)1 << 63;
	while ((x & mask) == 0) {
		++count;
		mask >>= 1;
	}
	return count;
#endif
}

// ------------------------------------------------------------------
// BYTE SWAP (Changement d'endianness)
// ------------------------------------------------------------------

/**
 * @defgroup ByteSwapMacros Macros de Changement d'Endianness
 * @brief Inversion de l'ordre des octets
 */

/**
 * @brief Inverse l'ordre des octets (16-bit)
 * @def NK_BYTESWAP16(x)
 * @param x Valeur 16-bit
 * @return Valeur avec octets inversés
 * @ingroup ByteSwapMacros
 */
#if defined(NKENTSEU_COMPILER_MSVC)
#define NK_BYTESWAP16(x) _byteswap_ushort((nkentseu::core::nk_uint16)(x))
#define NK_BYTESWAP32(x) _byteswap_ulong((nkentseu::core::nk_uint32)(x))
#define NK_BYTESWAP64(x) _byteswap_uint64((nkentseu::core::nk_uint64)(x))
#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
#define NK_BYTESWAP16(x) __builtin_bswap16((nkentseu::core::nk_uint16)(x))
#define NK_BYTESWAP32(x) __builtin_bswap32((nkentseu::core::nk_uint32)(x))
#define NK_BYTESWAP64(x) __builtin_bswap64((nkentseu::core::nk_uint64)(x))
#else
// Implémentations manuelles (Fallback)
NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint16 NK_BYTESWAP16(nkentseu::core::nk_uint16 x) {
	return (nkentseu::core::nk_uint16)((x << 8) | (x >> 8));
}

NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint32 NK_BYTESWAP32(nkentseu::core::nk_uint32 x) {
	return ((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24));
}

NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint64 NK_BYTESWAP64(nkentseu::core::nk_uint64 x) {
	x = ((x << 32) | (x >> 32));
	x = (((x & 0xFFFF0000FFFF0000ULL) >> 16) | ((x & 0x0000FFFF0000FFFFULL) << 16));
	return (((x & 0xFF00FF00FF00FF00ULL) >> 8) | ((x & 0x00FF00FF00FF00FFULL) << 8));
}
#endif

// ============================================================
// CLASSE NK_BITS - MANIPULATION DE BITS AVANCÉE
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {

/**
 * @class NkBits
 * @brief Classe utilitaire pour manipulation avancée de bits
 * @ingroup BitManipulation
 */
class NKENTSEU_CORE_API NkBits {
public:
	// ========================================
	// BIT OPERATIONS
	// ========================================

	/**
	 * @brief Compte le nombre de bits à 1 (population count)
	 * @tparam T Type entier (nkentseu::core::nk_uint32, nkentseu::core::nk_uint64)
	 * @param value Valeur à analyser
	 * @return Nombre de bits à 1
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_int32 CountBits(T value) {
		if (sizeof(T) == 8)
			return NK_POPCOUNT64(value);
		if (sizeof(T) == 4)
			return NK_POPCOUNT32(value);

		// Fallback pour autres tailles
		return CountBitsSoftware(value);
	}

	/**
	 * @brief Compte les zéros à droite (trailing zeros)
	 * @tparam T Type entier
	 * @param value Valeur à analyser
	 * @return Nombre de zéros à droite
	 * @note Retourne sizeof(T)*8 si value = 0
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_int32 CountTrailingZeros(T value) {
		if (value == 0)
			return sizeof(T) * 8;

		if (sizeof(T) == 8)
			return NK_CTZ64(value);
		if (sizeof(T) == 4)
			return NK_CTZ32(value);

		return CountTrailingZerosSoftware(value);
	}

	/**
	 * @brief Compte les zéros à gauche (leading zeros)
	 * @tparam T Type entier
	 * @param value Valeur à analyser
	 * @return Nombre de zéros à gauche
	 * @note Retourne sizeof(T)*8 si value = 0
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_int32 CountLeadingZeros(T value) {
		if (value == 0)
			return sizeof(T) * 8;

		if (sizeof(T) == 8)
			return NK_CLZ64(value);
		if (sizeof(T) == 4)
			return NK_CLZ32(value);

		return CountLeadingZerosSoftware(value);
	}

	/**
	 * @brief Trouve l'index du bit le plus bas à 1
	 * @tparam T Type entier
	 * @param value Valeur à analyser
	 * @return Index du premier bit à 1 (0-based), -1 si aucun bit à 1
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_int32 FindFirstSet(T value) {
		if (value == 0)
			return -1;
		return CountTrailingZeros(value);
	}

	/**
	 * @brief Trouve l'index du bit le plus haut à 1
	 * @tparam T Type entier
	 * @param value Valeur à analyser
	 * @return Index du dernier bit à 1 (0-based), -1 si aucun bit à 1
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_int32 FindLastSet(T value) {
		if (value == 0)
			return -1;
		return sizeof(T) * 8 - 1 - CountLeadingZeros(value);
	}

	// ========================================
	// ROTATION DE BITS
	// ========================================

	/**
	 * @brief Rotation gauche (rotate left)
	 * @tparam T Type entier
	 * @param value Valeur à faire tourner
	 * @param shift Nombre de positions de rotation (modulo bits)
	 * @return Valeur après rotation
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static T RotateLeft(T value, nk_int32 shift) {
		const nk_int32 bits = sizeof(T) * 8;
		shift &= (bits - 1); // Modulo bits
		return (value << shift) | (value >> (bits - shift));
	}

	/**
	 * @brief Rotation droite (rotate right)
	 * @tparam T Type entier
	 * @param value Valeur à faire tourner
	 * @param shift Nombre de positions de rotation (modulo bits)
	 * @return Valeur après rotation
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static T RotateRight(T value, nk_int32 shift) {
		const nk_int32 bits = sizeof(T) * 8;
		shift &= (bits - 1); // Modulo bits
		return (value >> shift) | (value << (bits - shift));
	}

	// ========================================
	// CHANGEMENT D'ENDIANNESS
	// ========================================

	/**
	 * @brief Inverse l'ordre des octets (16-bit)
	 * @param value Valeur 16-bit
	 * @return Valeur avec octets inversés
	 */
	NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint16 ByteSwap16(nkentseu::core::nk_uint16 value) {
		return NK_BYTESWAP16(value);
	}

	/**
	 * @brief Inverse l'ordre des octets (32-bit)
	 * @param value Valeur 32-bit
	 * @return Valeur avec octets inversés
	 */
	NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint32 ByteSwap32(nkentseu::core::nk_uint32 value) {
		return NK_BYTESWAP32(value);
	}

	/**
	 * @brief Inverse l'ordre des octets (64-bit)
	 * @param value Valeur 64-bit
	 * @return Valeur avec octets inversés
	 */
	NKENTSEU_FORCE_INLINE static nkentseu::core::nk_uint64 ByteSwap64(nkentseu::core::nk_uint64 value) {
		return NK_BYTESWAP64(value);
	}

	// ========================================
	// PUISSANCE DE DEUX
	// ========================================

	/**
	 * @brief Vérifie si une valeur est une puissance de deux
	 * @tparam T Type entier
	 * @param value Valeur à vérifier
	 * @return true si puissance de deux, false sinon
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_bool IsPowerOfTwo(T value) {
		return value > 0 && (value & (value - 1)) == 0;
	}

	/**
	 * @brief Arrondit vers le haut à la prochaine puissance de 2
	 * @param value Valeur 32-bit
	 * @return Prochaine puissance de 2 supérieure ou égale
	 */
	static nkentseu::core::nk_uint32 NextPowerOfTwo(nkentseu::core::nk_uint32 value);

	/**
	 * @brief Arrondit vers le haut à la prochaine puissance de 2
	 * @param value Valeur 64-bit
	 * @return Prochaine puissance de 2 supérieure ou égale
	 */
	static nkentseu::core::nk_uint64 NextPowerOfTwo(nkentseu::core::nk_uint64 value);

	/**
	 * @brief Log2 d'une puissance de 2
	 * @tparam T Type entier
	 * @param value Valeur (doit être puissance de 2)
	 * @return Log2 de la valeur
	 * @pre value doit être puissance de 2
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static nk_int32 Log2(T value) {
		NKENTSEU_ASSERT_MSG(IsPowerOfTwo(value), "Value must be power of two");
		return FindLastSet(value);
	}

	// ========================================
	// MANIPULATION DE CHAMPS DE BITS
	// ========================================

	/**
	 * @brief Extrait un champ de bits
	 * @tparam T Type entier
	 * @param value Valeur source
	 * @param position Position du premier bit (0-based)
	 * @param count Nombre de bits à extraire
	 * @return Champ de bits extrait
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static T ExtractBits(T value, nk_int32 position, nk_int32 count) {
		NKENTSEU_ASSERT_MSG(position >= 0 && count > 0 && position + count <= sizeof(T) * 8, "Invalid bit range");
		T mask = ((T(1) << count) - 1);
		return (value >> position) & mask;
	}

	/**
	 * @brief Insère un champ de bits
	 * @tparam T Type entier
	 * @param dest Destination où insérer
	 * @param src Source des bits à insérer
	 * @param position Position du premier bit (0-based)
	 * @param count Nombre de bits à insérer
	 * @return Destination avec bits insérés
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static T InsertBits(T dest, T src, nk_int32 position, nk_int32 count) {
		NKENTSEU_ASSERT_MSG(position >= 0 && count > 0 && position + count <= sizeof(T) * 8, "Invalid bit range");
		T mask = ((T(1) << count) - 1);
		T cleared = dest & ~(mask << position);
		return cleared | ((src & mask) << position);
	}

	/**
	 * @brief Inverse tous les bits d'une valeur
	 * @tparam T Type entier
	 * @param value Valeur à inverser
	 * @return Valeur avec bits inversés
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static T ReverseBits(T value) {
		return ReverseBitsSoftware(value);
	}

	/**
	 * @brief Inverse les octets d'une valeur
	 * @tparam T Type entier
	 * @param value Valeur dont les octets doivent être inversés
	 * @return Valeur avec octets inversés
	 * @note Différent de ReverseBits qui inverse les bits
	 */
	template <typename T> NKENTSEU_FORCE_INLINE static T ReverseBytes(T value) {
		if (sizeof(T) == 8)
			return ByteSwap64(value);
		if (sizeof(T) == 4)
			return ByteSwap32(value);
		if (sizeof(T) == 2)
			return ByteSwap16(value);
		return value; // 1 byte, rien à inverser
	}

private:
	// ========================================
	// IMPLÉMENTATIONS SOFTWARE (FALLBACK)
	// ========================================

	/**
	 * @brief Implémentation software pour CountBits
	 */
	template <typename T> static nk_int32 CountBitsSoftware(T value) {
		nk_int32 count = 0;
		while (value) {
			count += (value & 1);
			value >>= 1;
		}
		return count;
	}

	/**
	 * @brief Implémentation software pour CountTrailingZeros
	 */
	template <typename T> static nk_int32 CountTrailingZerosSoftware(T value) {
		if (value == 0)
			return sizeof(T) * 8;
		nk_int32 count = 0;
		while ((value & 1) == 0) {
			++count;
			value >>= 1;
		}
		return count;
	}

	/**
	 * @brief Implémentation software pour CountLeadingZeros
	 */
	template <typename T> static nk_int32 CountLeadingZerosSoftware(T value) {
		if (value == 0)
			return sizeof(T) * 8;
		nk_int32 count = 0;
		T mask = T(1) << (sizeof(T) * 8 - 1);
		while ((value & mask) == 0) {
			++count;
			mask >>= 1;
		}
		return count;
	}

	/**
	 * @brief Implémentation software pour ReverseBits
	 */
	template <typename T> static T ReverseBitsSoftware(T value) {
		T result = 0;
		nk_int32 bits = sizeof(T) * 8;
		for (nk_int32 i = 0; i < bits; ++i) {
			result = (result << 1) | (value & 1);
			value >>= 1;
		}
		return result;
	}
};

} // namespace core
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKBITS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================