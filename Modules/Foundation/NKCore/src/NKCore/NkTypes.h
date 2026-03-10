// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkTypes.h
// DESCRIPTION: Typedefs et alias dans le namespace nkentseu
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKTYPES_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKTYPES_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NKPlatform/NkArchDetect.h"

// ============================================================
// DÉFINITIONS DE BASE
// ============================================================

/**
 * @brief Vérifie la disponibilité de stdint.h
 * @def NKENTSEU_HAS_STDINT
 * @ingroup TypeDetection
 */

// stdint.h pour les types entiers fixes
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
	#include <stdint.h>
	#define NKENTSEU_HAS_STDINT
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
	// ============================================================
	// TYPES ENTIERS FIXES
	// ============================================================

	#ifndef BIT
		#define BIT(x) (1 << x)
	#endif

	/**
	 * @brief Entier signé 8-bit
	 * @ingroup PrimitiveTypes
	 */
	using int8 = signed char;

	/**
	 * @brief Entier non-signé 8-bit
	 * @ingroup PrimitiveTypes
	 */
	using uint8 = unsigned char;

	/**
	 * @brief Entier non-signé 32-bit (long)
	 * @ingroup PrimitiveTypes
	 */
	using uintl32 = unsigned long int;

	// Définitions dépendantes du compilateur
	#if defined(NKENTSEU_COMPILER_MSVC)
		using int16 = __int16;
		using uint16 = unsigned __int16;
		using int32 = __int32;
		using uint32 = unsigned __int32;
		using int64 = __int64;
		using uint64 = unsigned __int64;
	#else
		using int16 = short;
		using uint16 = unsigned short;
		using int32 = int;
		using uint32 = unsigned int;
		using int64 = long long;
		using uint64 = unsigned long long;
	#endif

	/**
	 * @brief Structure wrapper pour byte avec opérateurs
	 *
	 * Encapsule un uint8 avec des opérations bitwise et conversions sécurisées.
	 *
	 * @note Utiliser Byte::from() pour conversion sécurisée
	 * @note Tous les opérateurs sont constexpr et noexcept
	 *
	 * @example
	 * @code
	 * Byte b = Byte::from(0xFF);
	 * Byte masked = b & Byte::from(0x0F);  // 0x0F
	 * uint8 value = static_cast<uint8>(b); // 0xFF
	 * @endcode
	 */
	struct Byte {
		/**
		 * @brief Valeurs constantes prédéfinies
		 */
		enum Value : uint8 { _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _a, _b, _c, _d, _e, _f };

		uint8 value;

		/**
		 * @brief Constructeur explicite
		 */
		constexpr explicit Byte(uint8 v) noexcept : value(v) {
		}

		/**
		 * @brief Constructeur par défaut (valeur 0)
		 */
		constexpr Byte() noexcept : value(0) {
		}

		// Opérateurs bitwise
		constexpr Byte operator|(Byte b) const noexcept {
			return Byte(value | b.value);
		}
		constexpr Byte operator&(Byte b) const noexcept {
			return Byte(value & b.value);
		}
		constexpr Byte operator^(Byte b) const noexcept {
			return Byte(value ^ b.value);
		}
		constexpr Byte operator~() const noexcept {
			return Byte(~value);
		}

		template <typename T> constexpr Byte operator<<(T shift) const noexcept {
			return Byte(value << shift);
		}

		template <typename T> constexpr Byte operator>>(T shift) const noexcept {
			return Byte(value >> shift);
		}

		/**
		 * @brief Conversion sécurisée depuis un type
		 * @tparam T Type source
		 * @param v Valeur à convertir
		 * @return Byte converti
		 */
		template <typename T> static constexpr Byte from(T v) noexcept {
			return Byte(static_cast<uint8>(v));
		}

		/**
		 * @brief Conversion vers un type
		 * @tparam T Type cible
		 * @return Valeur convertie
		 */
		template <typename T> constexpr explicit operator T() const noexcept {
			return static_cast<T>(value);
		}
	};

	// Support optionnel pour les types 128 bits
	#if defined(__SIZEOF_INT128__) && !defined(__PSP__) && !defined(__NDS__)
		#define NKENTSEU_INT128_AVAILABLE 1
		using int128 = __int128_t;
		using uint128 = __uint128_t;
	#else
		#define NKENTSEU_INT128_AVAILABLE 0
		/**
		 * @brief Struct int128.
		 */
		struct int128 {
			int64 low;
			int64 high;
		};
		/**
		 * @brief Struct uint128.
		 */
		struct uint128 {
			uint64 low;
			uint64 high;
		};
	#endif

	// ============================================================
	// TYPES FLOTTANTS
	// ============================================================

	/**
	 * @brief Flottant simple précision 32-bit
	 * @ingroup PrimitiveTypes
	 */
	using float32 = float;

	/**
	 * @brief Flottant double précision 64-bit
	 * @ingroup PrimitiveTypes
	 */
	using float64 = double;

	/**
	 * @brief Flottant étendu précision 80-bit
	 * @ingroup PrimitiveTypes
	 */
	using float80 = long double;

	// ============================================================
	// TYPES CARACTÈRES
	// ============================================================

	/**
	 * @brief Caractère par défaut du framework
	 * @ingroup CharacterTypes
	 */
	using Char = char;

	#if defined(__cplusplus) && __cplusplus >= 202002L
		using char8 = char8_t;
		using char16 = char16_t;
		using char32 = char32_t;
	#else
		using char8 = uint8;
		using char16 = uint16;
		using char32 = uint32;
	#endif

	using wchar =
	#if defined(_WIN32) || defined(_WIN64)
		wchar_t;
	#else
		char32;
	#endif

	// ============================================================
	// TYPES BOOLÉENS
	// ============================================================

	#ifndef Bool
		using Bool = bool;
	#endif

	/**
	 * @brief Booléen 8-bit (0 = false, 1 = true)
	 * @ingroup BooleanTypes
	 */
	using Boolean = uint8;

	/**
	 * @brief Booléen 32-bit (pour alignement)
	 * @ingroup BooleanTypes
	 */
	using bool32 = int32;

	/**
	 * @brief Valeur true pour Boolean
	 * @ingroup BooleanTypes
	 */
	#ifndef True
		static constexpr Boolean True = 1;
	#endif

	/**
	 * @brief Valeur false pour Boolean
	 * @ingroup BooleanTypes
	 */
	#ifndef False
		static constexpr Boolean False = 0;
	#endif

	// ============================================================
	// TYPES POINTEURS ET TAILLES
	// ============================================================

	using PTR = void *;
	using ConstBytePtr = const uint8 *;
	using ConstVoidPtr = const void *;
	using BytePtr = uint8 *;
	using VoidPtr = void *;
	using UPTR = unsigned long long int;
	using usize = uint64;

	#if !defined(NKENTSEU_USING_STD_PTRDIFF_T)
	#ifdef NKENTSEU_ARCH_64BIT
	using ptrdiff = int64;
	#else
	using ptrdiff = int32;
	#endif
	#endif

	/**
	 * @brief Conversion sécurisée de ConstVoidPtr vers T*
	 * @tparam T Type cible
	 * @param ptr Pointeur source
	 * @return Pointeur converti
	 */
	template <typename T> inline T *SafeConstCast(ConstVoidPtr ptr) noexcept {
		return (T *)ptr;
	}

	/**
	 * @brief Conversion sécurisée de VoidPtr vers T*
	 * @tparam T Type cible
	 * @param ptr Pointeur source
	 * @return Pointeur converti
	 */
	template <typename T> inline T *NkSafeCast(VoidPtr ptr) noexcept {
		return (T *)ptr;
	}

	// ============================================================
	// TYPES DE TAILLE UNIVERSELS
	// ============================================================

	#ifdef NKENTSEU_ARCH_64BIT
		using usize_cpu = NKENTSEU_ALIGN(8) uint64;
	#else
		using usize_cpu = NKENTSEU_ALIGN(4) uint32;
	#endif

	using usize_gpu = NKENTSEU_ALIGN(16) int64;

	#if defined(NKENTSEU_ARCH_64BIT)
		using intptr = int64;
		using uintptr = uint64;
	#else
		using intptr = int32;
		using uintptr = uint32;
	#endif

	// ============================================================
	// ALIAS NKENTSEU_ POUR LES TYPES DE BASE
	// ============================================================

	// Types entiers signés
	using nk_int8 = int8;
	using nk_int16 = int16;
	using nk_int32 = int32;
	using nk_int64 = int64;
	using nk_int128 = int128;

	// Types entiers non-signés
	using nk_uint8 = uint8;
	using nk_uint16 = uint16;
	using nk_uint32 = uint32;
	using nk_uint64 = uint64;
	using nk_uint128 = uint128;

	// Types flottants
	using nk_float32 = float32;
	using nk_float64 = float64;
	using nk_float80 = float80;

	// Types taille machine
	using nk_size = usize;
	using nk_ptrdiff = ptrdiff;

	using nk_intptr = intptr;
	using nk_uintptr = uintptr;

	// Types booléens
	using nk_bool = Bool;
	using nk_boolean = Boolean;
	using nk_bool8 = uint8;
	using nk_bool32 = bool32;

	// Types caractères (alias NKENTSEU_)
	using nk_char = Char;
	using nk_char8 = char8;
	using nk_uchar = unsigned char;
	using nk_char16 = char16;
	using nk_char32 = char32;
	using nk_wchar = wchar;

	// Types octets
	using nk_byte = uint8;
	using nk_sbyte = int8;

	using nk_ptr = PTR;
	using nk_constbyteptr = ConstBytePtr;
	using nk_constvoidptr = ConstVoidPtr;
	using nk_byteptr = BytePtr;
	using nk_voidptr = VoidPtr;
	using nk_uptr = UPTR;
	using nk_usize = usize;

	// ============================================================
	// ALIAS COURTS POUR COMPATIBILITÉ
	// ============================================================

	// Entiers signés
	using i8 = nk_int8;
	using i16 = nk_int16;
	using i32 = nk_int32;
	using i64 = nk_int64;
	#if NKENTSEU_INT128_AVAILABLE
	using i128 = nk_int128;
	#endif

	// Entiers non-signés
	using u8 = nk_uint8;
	using u16 = nk_uint16;
	using u32 = nk_uint32;
	using u64 = nk_uint64;
	#if NKENTSEU_INT128_AVAILABLE
	using u128 = nk_uint128;
	#endif

	// Flottants
	using f32 = nk_float32;
	using f64 = nk_float64;
	using f80 = nk_float80;

	// Tailles
	using usize = nk_size;
	using isize = nk_ptrdiff;

	// ============================================================
	// CONSTANTES ET MACROS DE LIMITES
	// ============================================================

	// Limites int8
	#define NKENTSEU_INT8_MIN ((nk_int8)(-128))
	#define NKENTSEU_INT8_MAX ((nk_int8)(127))
	#define NKENTSEU_UINT8_MAX ((nk_uint8)(255U))

	// Limites int16
	#define NKENTSEU_INT16_MIN ((nk_int16)(-32768))
	#define NKENTSEU_INT16_MAX ((nk_int16)(32767))
	#define NKENTSEU_UINT16_MAX ((nk_uint16)(65535U))

	// Limites int32
	#define NKENTSEU_INT32_MIN ((nk_int32)(-2147483647 - 1))
	#define NKENTSEU_INT32_MAX ((nk_int32)(2147483647))
	#define NKENTSEU_UINT32_MAX ((nk_uint32)(4294967295U))

	// Limites int64
	#define NKENTSEU_INT64_MIN ((nk_int64)(-9223372036854775807LL - 1))
	#define NKENTSEU_INT64_MAX ((nk_int64)(9223372036854775807LL))
	#define NKENTSEU_UINT64_MAX ((nk_uint64)(18446744073709551615ULL))

	// Limites float
	#define NKENTSEU_FLOAT32_MIN (1.175494351e-38f)
	#define NKENTSEU_FLOAT32_MAX (3.402823466e+38f)
	#define NKENTSEU_FLOAT64_MIN (2.2250738585072014e-308)
	#define NKENTSEU_FLOAT64_MAX (1.7976931348623158e+308)

	// Limites size_t
	#if defined(NKENTSEU_ARCH_64BIT)
	#define NKENTSEU_SIZE_MAX NKENTSEU_UINT64_MAX
	#else
	#define NKENTSEU_SIZE_MAX NKENTSEU_UINT32_MAX
	#endif

	// Macro de compatibilité Nkentseu
	#define NKENTSEU_MAX_UINT8 0xFFU
	#define NKENTSEU_MAX_INT8 0x7F
	#define NKENTSEU_MIN_INT8 (-0x7F - 1)
	#define NKENTSEU_MAX_UINT16 0xFFFFU
	#define NKENTSEU_MAX_INT16 0x7FFF
	#define NKENTSEU_MIN_INT16 (-0x7FFF - 1)
	#define NKENTSEU_MAX_UINT32 0xFFFFFFFFU
	#define NKENTSEU_MAX_INT32 0x7FFFFFFF
	#define NKENTSEU_MIN_INT32 (-0x7FFFFFFF - 1)
	#define NKENTSEU_MAX_UINT64 0xFFFFFFFFFFFFFFFFULL
	#define NKENTSEU_MAX_INT64 0x7FFFFFFFFFFFFFFFLL
	#define NKENTSEU_MIN_INT64 (-0x7FFFFFFFFFFFFFFFLL - 1)

	// Limites flottantes Nkentseu
	#define NKENTSEU_MAX_FLOAT32 FLT_MAX
	#define NKENTSEU_MIN_FLOAT32 FLT_MIN
	#define NKENTSEU_MAX_FLOAT64 DBL_MAX
	#define NKENTSEU_MIN_FLOAT64 DBL_MIN
	#define NKENTSEU_MAX_FLOAT80 LDBL_MAX
	#define NKENTSEU_MIN_FLOAT80 LDBL_MIN

	// ============================================================
	// VALEURS SPÉCIALES
	// ============================================================

	#if defined(NKENTSEU_HAS_NULLPTR)
	#define NK_NULL nullptr
	#else
	#define NK_NULL 0
	#endif

	#define NKENTSEU_INVALID_SIZE ((nk_size)(-1))
	#define NKENTSEU_INVALID_INDEX ((nk_size)(-1))

	// Identifiants invalides
	#define NKENTSEU_INVALID_ID UINT64_MAX
	#define NKENTSEU_INVALID_ID_UINT64 UINT64_MAX
	#define NKENTSEU_INVALID_ID_UINT32 UINT32_MAX
	#define NKENTSEU_INVALID_ID_UINT16 UINT16_MAX
	#define NKENTSEU_INVALID_ID_UINT8 UINT8_MAX
	#define NKENTSEU_USIZE_MAX UINT64_MAX

	// ============================================================
	// CONVERSION D'ENDIANNESS
	// ============================================================

	#ifdef NKENTSEU_ENDIAN_LITTLE
	#ifdef NKENTSEU_COMPILER_MSVC
	#define NkToBigEndian16(x) _byteswap_ushort(x)
	#define NkToBigEndian32(x) _byteswap_ulong(x)
	#define NkToBigEndian64(x) _byteswap_uint64(x)
	#else
	#define NkToBigEndian16(x) __builtin_bswap16(x)
	#define NkToBigEndian32(x) __builtin_bswap32(x)
	#define NkToBigEndian64(x) __builtin_bswap64(x)
	#endif
	#define NkToLittleEndian16(x) (x)
	#define NkToLittleEndian32(x) (x)
	#define NkToLittleEndian64(x) (x)
	#else
	#define NkToBigEndian16(x) (x)
	#define NkToBigEndian32(x) (x)
	#define NkToBigEndian64(x) (x)
	#if defined(NKENTSEU_COMPILER_MSVC)
	#define NkToLittleEndian16(x) _byteswap_ushort(x)
	#define NkToLittleEndian32(x) _byteswap_ulong(x)
	#define NkToLittleEndian64(x) _byteswap_uint64(x)
	#else
	#define NkToLittleEndian16(x) __builtin_bswap16(x)
	#define NkToLittleEndian32(x) __builtin_bswap32(x)
	#define NkToLittleEndian64(x) __builtin_bswap64(x)
	#endif
	#endif


} // namespace nkentseu

// ============================================================
// NAMESPACE NKENTSEU::CORE - TYPES AVANCÉS
// ============================================================

namespace nkentseu {
	// ========================================
	// TYPES HASH
	// ========================================

	/**
	 * @brief Type pour hash values (32-bit ou 64-bit selon arch)
	 * @ingroup AdvancedTypes
	 */
	#if defined(NKENTSEU_ARCH_64BIT)
	using NkHashValue = uint64;
	#else
	using NkHashValue = uint32;
	#endif

	/**
	 * @brief Hash 32-bit explicite
	 * @ingroup AdvancedTypes
	 */
	using NkHash32 = uint32;

	/**
	 * @brief Hash 64-bit explicite
	 * @ingroup AdvancedTypes
	 */
	using NkHash64 = uint64;

	// ========================================
	// TYPES HANDLE
	// ========================================

	/**
	 * @brief Handle opaque (pointeur ou index)
	 * @note Utilise taille pointeur pour compatibilité
	 * @ingroup AdvancedTypes
	 */
	using NkHandle = uintptr;

	/**
	 * @brief Handle invalide
	 * @ingroup AdvancedTypes
	 */
	constexpr NkHandle INVALID_HANDLE = 0;

	// ========================================
	// TYPES ID
	// ========================================

	/**
	 * @brief Identifiant unique 32-bit
	 * @ingroup AdvancedTypes
	 */
	using NkID32 = uint32;

	/**
	 * @brief Identifiant unique 64-bit
	 * @ingroup AdvancedTypes
	 */
	using NkID64 = uint64;

	/**
	 * @brief ID invalide 32-bit
	 * @ingroup AdvancedTypes
	 */
	constexpr NkID32 INVALID_ID32 = 0xFFFFFFFF;

	/**
	 * @brief ID invalide 64-bit
	 * @ingroup AdvancedTypes
	 */
	constexpr NkID64 INVALID_ID64 = 0xFFFFFFFFFFFFFFFF;


} // namespace nkentseu

// ============================================================
// NAMESPACE NKENTSEU::MATH - TYPES MATHÉMATIQUES
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace math.
 */
	namespace math {

		/**
		 * @brief Type flottant par défaut pour calculs mathématiques
		 * @note Configurable: float32 ou float64
		 * @ingroup MathTypes
		 */
		#if defined(NKENTSEU_MATH_USE_DOUBLE)
		using NkReal = float64;
		#else
		using NkReal = float32;
		#endif

		/**
		 * @brief Angle en radians (float par défaut)
		 * @ingroup MathTypes
		 */
		using NkRadians = NkReal;

		/**
		 * @brief Angle en degrés (float par défaut)
		 * @ingroup MathTypes
		 */
		using NkDegrees = NkReal;

	} // namespace math
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKTYPES_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================