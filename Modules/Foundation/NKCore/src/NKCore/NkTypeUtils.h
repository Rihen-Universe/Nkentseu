// -----------------------------------------------------------------------------
// FICHIER: Core/NKCore/src/NKCore/NkTypeUtils.h
// DESCRIPTION: Utilitaires de types (macros, littéraux, validation compile-time)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKTYPEUTILS_H
#define NKENTSEU_CORE_NKTYPEUTILS_H

	// ============================================================
	// INCLUDES
	// ============================================================

	#include <stddef.h>
	#include <float.h>
	#include <wchar.h>

	#include "NkTypes.h"
	#include "NkTraits.h"
	#include "NkMacros.h"
	#include "NKPlatform/NkCompilerDetect.h"

	// ============================================================
	// @defgroup TypeUtilityMacros Macros Utilitaires de Types
	// @{
	// ============================================================

	/** @brief Crée un masque de bit à la position x (64-bit). @example NK_BIT(3) -> 0x08 */
	#define NK_BIT(x) (1ULL << (x))

	/** @brief Aligne x vers le haut sur l'alignement a (puissance de 2). */
	#define NK_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

	/** @brief Aligne x vers le bas sur l'alignement a (puissance de 2). */
	#define NK_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

	/** @brief Convertit un booléen en chaîne "True"/"False". */
	#define NK_STR_BOOL(b) ((b) ? "True" : "False")

	// NK_CLAMP is provided by NkMacros.h

	/** @brief Nombre d'éléments d'un tableau statique. @warning Tableaux statiques uniquement. */
	#define NK_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

	/** @} */

	// ============================================================
	// @defgroup BitManipulationMacros Manipulation de Bits
	// @{
	// ============================================================

	/** @brief Met le bit b à 1 dans x. */
	#define NK_SET_BIT(x, b)        ((x) |= NK_BIT(b))

	/** @brief Met le bit b à 0 dans x. */
	#define NK_CLEAR_BIT(x, b)      ((x) &= ~NK_BIT(b))

	/** @brief Retourne true si le bit b est à 1 dans x. */
	#define NK_TEST_BIT(x, b)       (((x) & NK_BIT(b)) != 0)

	/** @brief Inverse le bit b dans val. */
	#define NK_TOGGLE_BIT(val, bit) ((val) ^= NK_BIT(bit))

	/** @brief Vérifie si n est aligné sur align (puissance de 2). */
	#define NK_IS_ALIGNED(n, align) (((n) & ((align) - 1)) == 0)

	/** @} */

	// ============================================================
	// @defgroup OffsetMacros Offset et Container
	// @{
	// ============================================================

	/** @brief Offset d'un membre dans une structure. */
	#if defined(NKENTSEU_COMPILER_MSVC)
		#define NK_OFFSET_OF(type, member) offsetof(type, member)
	#else
		#define NK_OFFSET_OF(type, member) __builtin_offsetof(type, member)
	#endif

	/** @brief Pointeur du conteneur depuis un pointeur membre (technique Linux kernel). */
	#define NK_CONTAINER_OF(ptr, type, member) \
		((type *)((nkentseu::nk_uint8 *)(ptr) - NK_OFFSET_OF(type, member)))

	/** @} */

	// ============================================================
	// CONSTANTES UTILITAIRES
	// ============================================================

	namespace nkentseu {

		constexpr nk_uint8  NKENTSEU_BIT_MASK_8  = 0xFF;
		constexpr nk_uint32 NKENTSEU_BIT_MASK_32 = 0xFFFFFFFFu;
		constexpr nk_uint8  NKENTSEU_BIT_SHIFT   = 3;

	} // namespace nkentseu

	// ============================================================
	// @defgroup SafeCastMacros Conversions Sécurisées (clamp)
	// @{
	// ============================================================

	#define NK_SAFE_CAST_TO_U8(x)  NK_CLAMP((x), 0, NKENTSEU_MAX_UINT8)
	#define NK_SAFE_CAST_TO_U16(x) NK_CLAMP((x), 0, NKENTSEU_MAX_UINT16)
	#define NK_SAFE_CAST_TO_U32(x) NK_CLAMP((x), 0, NKENTSEU_MAX_UINT32)
	#define NK_SAFE_CAST_TO_U64(x) NK_CLAMP((x), 0, NKENTSEU_MAX_UINT64)
	#define NK_SAFE_CAST_TO_I8(x)  NK_CLAMP((x), NKENTSEU_MIN_INT8,  NKENTSEU_MAX_INT8)
	#define NK_SAFE_CAST_TO_I16(x) NK_CLAMP((x), NKENTSEU_MIN_INT16, NKENTSEU_MAX_INT16)
	#define NK_SAFE_CAST_TO_I32(x) NK_CLAMP((x), NKENTSEU_MIN_INT32, NKENTSEU_MAX_INT32)
	#define NK_SAFE_CAST_TO_I64(x) NK_CLAMP((x), NKENTSEU_MIN_INT64, NKENTSEU_MAX_INT64)

	/** @} */

	// ============================================================
	// @defgroup LiteralMacros Littéraux Caractère
	// @{
	// ============================================================

	/** @brief Sélectionne le préfixe de littéral selon le type CharT. */
	#define NK_LITERAL(CharT, str) \
		(nkentseu::traits::NkIsSame<CharT, nkentseu::nk_char>::value   ? str     : \
		nkentseu::traits::NkIsSame<CharT, nkentseu::nk_char8>::value  ? u8##str : \
		nkentseu::traits::NkIsSame<CharT, nkentseu::nk_char16>::value ? u##str  : \
		nkentseu::traits::NkIsSame<CharT, nkentseu::nk_char32>::value ? U##str  : \
		nkentseu::traits::NkIsSame<CharT, nkentseu::nk_wchar>::value  ? L##str  : \
																		str)

	#define NK_CONST_CHAR(type, str)  NK_LITERAL(type, str)
	#define NK_CONST_CHAR8(str)       NK_LITERAL(nkentseu::nk_char8,  str)
	#define NK_CONST_CHAR16(str)      NK_LITERAL(nkentseu::nk_char16, str)
	#define NK_CONST_CHAR32(str)      NK_LITERAL(nkentseu::nk_char32, str)
	#define NK_CONST_WCHAR(str)       NK_LITERAL(nkentseu::nk_wchar,  str)
	#define NK_CONST_NK_CHAR(str)     NK_LITERAL(nkentseu::nk_char,   str)

	/** @} */

	// ============================================================
	// @defgroup CppLiterals Littéraux C++11 User-Defined
	// @{
	// ============================================================

	namespace nkentseu {
		namespace literals {

			constexpr nk_uint8  operator""_u8 (unsigned long long val) noexcept { return static_cast<nk_uint8 >(val); }
			constexpr nk_uint16 operator""_u16(unsigned long long val) noexcept { return static_cast<nk_uint16>(val); }
			constexpr nk_uint32 operator""_u32(unsigned long long val) noexcept { return static_cast<nk_uint32>(val); }
			constexpr nk_uint64 operator""_u64(unsigned long long val) noexcept { return static_cast<nk_uint64>(val); }
			constexpr nk_int8   operator""_i8 (unsigned long long val) noexcept { return static_cast<nk_int8  >(val); }
			constexpr nk_int16  operator""_i16(unsigned long long val) noexcept { return static_cast<nk_int16 >(val); }
			constexpr nk_int32  operator""_i32(unsigned long long val) noexcept { return static_cast<nk_int32 >(val); }
			constexpr nk_int64  operator""_i64(unsigned long long val) noexcept { return static_cast<nk_int64 >(val); }

			constexpr nk_float32 operator""_f32(long double val) noexcept { return static_cast<nk_float32>(val); }
			constexpr nk_float64 operator""_f64(long double val) noexcept { return static_cast<nk_float64>(val); }
			constexpr nk_float80 operator""_f80(long double val) noexcept { return static_cast<nk_float80>(val); }

			constexpr nk_bool32 operator""_b32(unsigned long long val) noexcept { return static_cast<nk_bool32>(val != 0); }
			constexpr nk_char   operator""_cb (char c)                 noexcept { return static_cast<nk_char  >(c); }
			constexpr nk_char8  operator""_c8 (char c)                 noexcept { return static_cast<nk_char8 >(c); }
			constexpr nk_char16 operator""_c16(unsigned long long val) noexcept { return static_cast<nk_char16>(val); }
			constexpr nk_char32 operator""_c32(unsigned long long val) noexcept { return static_cast<nk_char32>(val); }

			#if defined(_WIN32) || defined(_WIN64)
				constexpr nk_wchar operator""_cw(nk_wchar val) noexcept { return static_cast<nk_wchar>(val); }
			#endif

			#if NKENTSEU_INT128_AVAILABLE
				constexpr nk_uint128 operator""_u128(unsigned long long val) noexcept { return static_cast<nk_uint128>(val); }
				constexpr nk_int128  operator""_i128(unsigned long long val) noexcept { return static_cast<nk_int128 >(val); }
			#endif

			constexpr Byte operator""_b(unsigned long long v) noexcept { return Byte::from(v); }

		} // namespace literals
	} // namespace nkentseu

	/** @} */

	// ============================================================
	// @defgroup CastMacros Macros de Conversion de Types
	// @{
	// ============================================================

	/** @brief static_cast<type>(value) */
	#define NK_STATIC_CAST(type, value)      static_cast<type>(value)

	/** @brief reinterpret_cast<type>(value) */
	#define NK_REINTERPRET_CAST(type, value) reinterpret_cast<type>(value)

	/** @brief const_cast<type>(value) */
	#define NK_CONST_CAST(type, value)       const_cast<type>(value)

	/** @brief Cast C ((type)(value)) - déconseillé. */
	#define NK_C_CAST(type, value)           ((type)(value))

	/** @brief dynamic_cast si RTTI disponible, sinon static_cast. */
	#if defined(NKENTSEU_HAS_RTTI)
		#define NK_DYNAMIC_CAST(type, value) dynamic_cast<type>(value)
	#else
		#define NK_DYNAMIC_CAST(type, value) static_cast<type>(value)
	#endif

	/** @} */

	// ============================================================
	// HELPERS DE CONVERSION (TEMPLATES)
	// ============================================================

	namespace nkentseu {

		/** @brief Convertit un pointeur en handle opaque. */
		template <typename T>
		inline core::NkHandle NkToHandle(T* ptr) noexcept {
			return reinterpret_cast<core::NkHandle>(ptr);
		}

		/** @brief Convertit un handle opaque en pointeur typé. */
		template <typename T>
		inline T* NkFromHandle(core::NkHandle handle) noexcept {
			return reinterpret_cast<T*>(handle);
		}

		/** @brief Cast sécurisé (static_cast typé). */
		template <typename To, typename From>
		inline To NkSafeCast(From value) noexcept {
			return static_cast<To>(value);
		}

	} // namespace nkentseu

	// ============================================================
	// @defgroup StaticAssertMacros Validation Compile-Time
	// @{
	// ============================================================

	#if defined(NKENTSEU_HAS_STATIC_ASSERT)

		namespace nkentseu {

			#if defined(__cplusplus) && __cplusplus >= 201103L
				#define NK_STATIC_ASSERT(name, cond, msg) static_assert((cond), #name ": " msg)
			#else
				#define NK_STATIC_ASSERT(name, cond, msg) \
					typedef char NK_STATIC_ASSERT_##name[(cond) ? 1 : -1] NKENTSEU_UNUSED
			#endif

			NK_STATIC_ASSERT(int8_size,    sizeof(nk_int8)    == 1, "nk_int8 must be 1 byte");
			NK_STATIC_ASSERT(int16_size,   sizeof(nk_int16)   == 2, "nk_int16 must be 2 bytes");
			NK_STATIC_ASSERT(int32_size,   sizeof(nk_int32)   == 4, "nk_int32 must be 4 bytes");
			NK_STATIC_ASSERT(int64_size,   sizeof(nk_int64)   == 8, "nk_int64 must be 8 bytes");
			NK_STATIC_ASSERT(float32_size, sizeof(nk_float32) == 4, "nk_float32 must be 4 bytes");
			NK_STATIC_ASSERT(float64_size, sizeof(nk_float64) == 8, "nk_float64 must be 8 bytes");

			NK_STATIC_ASSERT(int8_valid,   sizeof(nk_int8)==1   && (nk_int8)-1   < 0, "nk_int8 invalid");
			NK_STATIC_ASSERT(uint8_valid,  sizeof(nk_uint8)==1  && (nk_uint8)-1  > 0, "nk_uint8 invalid");
			NK_STATIC_ASSERT(int16_valid,  sizeof(nk_int16)==2  && (nk_int16)-1  < 0, "nk_int16 invalid");
			NK_STATIC_ASSERT(uint16_valid, sizeof(nk_uint16)==2 && (nk_uint16)-1 > 0, "nk_uint16 invalid");
			NK_STATIC_ASSERT(int32_valid,  sizeof(nk_int32)==4  && (nk_int32)-1  < 0, "nk_int32 invalid");
			NK_STATIC_ASSERT(uint32_valid, sizeof(nk_uint32)==4 && (nk_uint32)-1 > 0, "nk_uint32 invalid");
			NK_STATIC_ASSERT(int64_valid,  sizeof(nk_int64)==8  && (nk_int64)-1  < 0, "nk_int64 invalid");
			NK_STATIC_ASSERT(uint64_valid, sizeof(nk_uint64)==8 && (nk_uint64)-1 > 0, "nk_uint64 invalid");

			NK_STATIC_ASSERT(float32_valid, sizeof(nk_float32) == 4,  "nk_float32 invalid");
			NK_STATIC_ASSERT(float64_valid, sizeof(nk_float64) == 8,  "nk_float64 invalid");
			NK_STATIC_ASSERT(float80_valid, sizeof(nk_float80) >= 10, "nk_float80 must be >= 10 bytes");

			NK_STATIC_ASSERT(nkchar_valid,  sizeof(nk_char)   == 1 && (nk_char)-1 < 0, "nk_char invalid");
			NK_STATIC_ASSERT(char8_valid,   sizeof(nk_char8)  == 1,  "nk_char8 invalid");
			NK_STATIC_ASSERT(char16_valid,  sizeof(nk_char16) == 2,  "nk_char16 invalid");
			NK_STATIC_ASSERT(char32_valid,  sizeof(nk_char32) == 4,  "nk_char32 invalid");

			NK_STATIC_ASSERT(Boolean_valid, sizeof(nk_boolean) == 1,          "nk_boolean invalid");
			NK_STATIC_ASSERT(bool32_valid,  sizeof(nk_bool32)  == 4,          "nk_bool32 invalid");
			NK_STATIC_ASSERT(PTR_valid,     sizeof(nk_ptr)  == sizeof(void*), "nk_ptr size mismatch");
			NK_STATIC_ASSERT(UPTR_valid,    sizeof(nk_uptr) >= sizeof(void*), "nk_uptr too small");
			NK_STATIC_ASSERT(usize_valid,   sizeof(nk_usize) >= sizeof(void*), "nk_usize too small");

		} // namespace nkentseu

	#endif // NKENTSEU_HAS_STATIC_ASSERT

/** @} */

#endif // NKENTSEU_CORE_NKTYPEUTILS_H

// ============================================================
// Copyright (C) 2024-2026 Rihen. All rights reserved.
// ============================================================
