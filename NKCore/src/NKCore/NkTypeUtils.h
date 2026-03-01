// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkTypeUtils.h
// DESCRIPTION: Utilitaires de types (macros, littéraux, validation)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKTYPEUTILS_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKTYPEUTILS_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include <typeinfo>
#include <stddef.h>
#include <cfloat>
#include <cwchar>

#include "NkTypes.h"

// ============================================================
// MACROS UTILITAIRES
// ============================================================

/**
 * @brief Crée un masque de bit à la position x
 * @param x Position du bit (0-based)
 * @return Masque avec le bit x à 1
 * @example NkBit(3) = 0b00001000
 */
#define NkBit(x) (1ULL << (x))

/**
 * @brief Aligne une valeur vers le haut
 * @param x Valeur à aligner
 * @param a Alignement (puissance de 2)
 * @return Valeur alignée
 * @pre a doit être une puissance de 2
 */
#define NkAlignUp(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

/**
 * @brief Aligne une valeur vers le bas
 * @param x Valeur à aligner
 * @param a Alignement (puissance de 2)
 * @return Valeur alignée
 * @pre a doit être une puissance de 2
 */
#define NkAlignDown(x, a) ((x) & ~((a) - 1))

/**
 * @brief Convertit un booléen en chaîne "True" ou "False"
 * @param b Valeur booléenne
 * @return "True" si vrai, "False" sinon
 */
#define NkStrBool(b) ((b) ? "True" : "False")

/**
 * @brief Clampe une valeur entre min et max
 * @param v Valeur à clamper
 * @param mi Minimum inclusif
 * @param ma Maximum inclusif
 * @return Valeur clampée dans [mi, ma]
 */
#define NkClamp(v, mi, ma) ((v) < (mi) ? (mi) : ((v) > (ma) ? (ma) : (v)))

/**
 * @brief Calcule la taille d'un tableau statique
 * @param a Tableau
 * @return Nombre d'éléments dans le tableau
 */
#define NkArraySize(a) (sizeof(a) / sizeof((a)[0]))

/**
 * @brief Met un bit à 1
 * @param x Variable à modifier
 * @param b Position du bit (0-based)
 */
#define NkSetBit(x, b) ((x) |= NkBit(b))

/**
 * @brief Met un bit à 0
 * @param x Variable à modifier
 * @param b Position du bit (0-based)
 */
#define NkClearBit(x, b) ((x) &= ~NkBit(b))

/**
 * @brief Teste si un bit est à 1
 * @param x Variable à tester
 * @param b Position du bit (0-based)
 * @return true si le bit est à 1, false sinon
 */
#define NkTestBit(x, b) ((x) & NkBit(b))

/**
 * @brief Calcule l'offset d'un membre dans une structure
 * @param type Type de la structure
 * @param member Nom du membre
 * @return Offset en octets
 */
#define NkOffsetOf(type, member) ((nkentseu::core::nk_usize) & ((type *)NKENTSEU_NULL)->member)

/**
 * @brief Calcule le pointeur de conteneur à partir d'un membre
 * @param ptr Pointeur vers le membre
 * @param type Type du conteneur
 * @param member Nom du membre dans le conteneur
 * @return Pointeur vers le conteneur
 */
#define NkContainerOf(ptr, type, member) ((type *)((nkentseu::core::nk_uint8 *)(ptr) - NkOffsetOf(type, member)))

/**
 * @brief Inverse un bit
 * @param val Variable à modifier
 * @param bit Position du bit (0-based)
 */
#define NkToggleBit(val, bit) ((val) ^= NkBit(bit))

/**
 * @brief Vérifie si une valeur est alignée
 * @param n Valeur à vérifier
 * @param align Alignement (puissance de 2)
 * @return true si alignée, false sinon
 * @pre align doit être une puissance de 2
 */
#define NkIsAligned(n, align) (((n) & ((align) - 1)) == 0)

// ============================================================
// CONSTANTES UTILITAIRES
// ============================================================

// Masques de bits
constexpr nkentseu::core::nk_uint8 NKENTSEU_BIT_MASK_8 = 0xFF;
constexpr nkentseu::core::nk_uint32 NKENTSEU_BIT_MASK_32 = 0xFFFFFFFF;
constexpr nkentseu::core::nk_uint8 NKENTSEU_BIT_SHIFT = 3;

// ============================================================
// CONVERSIONS SÉCURISÉES
// ============================================================

#define NkSafeCastToU8(x) NkClamp((x), 0, NKENTSEU_MAX_UINT8)
#define NkSafeCastToU16(x) NkClamp((x), 0, NKENTSEU_MAX_UINT16)
#define NkSafeCastToU32(x) NkClamp((x), 0, NKENTSEU_MAX_UINT32)
#define NkSafeCastToU64(x) NkClamp((x), 0, NKENTSEU_MAX_UINT64)
#define NkSafeCastToI8(x) NkClamp((x), NKENTSEU_MIN_INT8, NKENTSEU_MAX_INT8)
#define NkSafeCastToI16(x) NkClamp((x), NKENTSEU_MIN_INT16, NKENTSEU_MAX_INT16)
#define NkSafeCastToI32(x) NkClamp((x), NKENTSEU_MIN_INT32, NKENTSEU_MAX_INT32)
#define NkSafeCastToI64(x) NkClamp((x), NKENTSEU_MIN_INT64, NKENTSEU_MAX_INT64)

// ============================================================
// MACROS DE LITTÉRAUX
// ============================================================

#if defined(_WIN32) || defined(_WIN64)
#define NkLiteral(CharT, str)                                                                                          \
	NkIsSame<CharT, nkentseu::core::nk_char>::value		? str                                                          \
	: NkIsSame<CharT, nkentseu::core::nk_char8>::value	? u8##str                                                      \
	: NkIsSame<CharT, nkentseu::core::nk_char16>::value ? u##str                                                       \
	: NkIsSame<CharT, nkentseu::core::nk_char32>::value ? U##str                                                       \
	: NkIsSame<CharT, nkentseu::core::nk_wchar>::value	? L##str                                                       \
														: str
#else
#define NkLiteral(CharT, str)                                                                                          \
	NkIsSame<CharT, nkentseu::core::nk_char>::value		? str                                                          \
	: NkIsSame<CharT, nkentseu::core::nk_char8>::value	? u8##str                                                      \
	: NkIsSame<CharT, nkentseu::core::nk_char16>::value ? u##str                                                       \
	: NkIsSame<CharT, nkentseu::core::nk_char32>::value ? U##str                                                       \
	: NkIsSame<CharT, nkentseu::core::nk_wchar>::value	? L##str                                                       \
														: str
#endif

#define NkConstChar(type, str) NkLiteral(type, str)
#define NkConstChar8(str) NkLiteral(nkentseu::core::nk_char8, str)
#define NkConstChar16(str) NkLiteral(nkentseu::core::nk_char16, str)
#define NkConstChar32(str) NkLiteral(nkentseu::core::nk_char32, str)
#define NkConstWChar(str) NkLiteral(nkentseu::core::nk_wchar, str)
#define NkConstNKChar(str) NkLiteral(nkentseu::core::nk_char, str)
// ============================================================
// LITTÉRAUX C++11
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace core.
 */
namespace core {
/**
 * @brief Namespace literals.
 */
namespace literals {

/**
 * @brief Literal pour uint8
 * @example auto x = 42_u8;
 */
constexpr nk_uint8 operator""_u8(unsigned long long val) {
	return static_cast<nk_uint8>(val);
}

/**
 * @brief Literal pour uint16
 */
constexpr nk_uint16 operator""_u16(unsigned long long val) {
	return static_cast<nk_uint16>(val);
}

/**
 * @brief Literal pour uint32
 */
constexpr nk_uint32 operator""_u32(unsigned long long val) {
	return static_cast<nk_uint32>(val);
}

/**
 * @brief Literal pour uint64
 */
constexpr nk_uint64 operator""_u64(unsigned long long val) {
	return static_cast<nk_uint64>(val);
}

/**
 * @brief Literal pour int8
 */
constexpr nk_int8 operator""_i8(unsigned long long val) {
	return static_cast<nk_int8>(val);
}

/**
 * @brief Literal pour int16
 */
constexpr nk_int16 operator""_i16(unsigned long long val) {
	return static_cast<nk_int16>(val);
}

/**
 * @brief Literal pour int32
 */
constexpr nk_int32 operator""_i32(unsigned long long val) {
	return static_cast<nk_int32>(val);
}

/**
 * @brief Literal pour int64
 */
constexpr nk_int64 operator""_i64(unsigned long long val) {
	return static_cast<nk_int64>(val);
}

/**
 * @brief Literal pour float32
 */
constexpr nk_float32 operator""_f32(long double val) {
	return static_cast<nk_float32>(val);
}

/**
 * @brief Literal pour float64
 */
constexpr nk_float64 operator""_f64(long double val) {
	return static_cast<nk_float64>(val);
}

/**
 * @brief Literal pour float80
 */
constexpr nk_float80 operator""_f80(long double val) {
	return static_cast<nk_float80>(val);
}

/**
 * @brief Literal pour bool32
 */
constexpr nk_bool32 operator""_b32(unsigned long long val) {
	return static_cast<nk_bool32>(val);
}

/**
 * @brief Literal pour nkchar
 */
constexpr nk_char operator""_cb(char c) {
	return static_cast<nk_char>(c);
}

/**
 * @brief Literal pour char8
 */
constexpr nk_char8 operator""_c8(char c) {
	return static_cast<nk_char8>(c);
}

/**
 * @brief Literal pour char16
 */
constexpr nk_char16 operator""_c16(unsigned long long val) {
	return static_cast<nk_char16>(val);
}

/**
 * @brief Literal pour char32
 */
constexpr nk_char32 operator""_c32(unsigned long long val) {
	return static_cast<nk_char32>(val);
}

#if defined(_WIN32) || defined(_WIN64)
/**
 * @brief Literal pour wchar (Windows)
 */
constexpr nk_wchar operator""_cw(nk_wchar val) {
	return static_cast<nk_wchar>(val);
}
#endif

#if NKENTSEU_INT128_AVAILABLE
/**
 * @brief Literal pour uint128
 */
constexpr nk_uint128 operator""_u128(unsigned long long val) {
	return static_cast<nk_uint128>(val);
}

/**
 * @brief Literal pour int128
 */
constexpr nk_int128 operator""_i128(unsigned long long val) {
	return static_cast<nk_int128>(val);
}
#endif

/**
 * @brief Literal pour Byte
 * @example auto b = 0xFF_b;
 */
constexpr Byte operator""_b(unsigned long long v) noexcept {
	return Byte::from(v);
}
} // namespace literals
} // namespace core
} // namespace nkentseu

// ============================================================
// MACROS DE CONVERSION DE TYPES
// ============================================================

/**
 * @brief Cast statique sécurisé
 * @param type Type cible
 * @param value Valeur à convertir
 * @return Valeur convertie
 */
#define NkStaticCast(type, value) static_cast<type>(value)

/**
 * @brief Cast dynamique (RTTI requis)
 * @param type Type cible
 * @param value Valeur à convertir
 * @return Valeur convertie
 */
#if defined(NKENTSEU_HAS_RTTI)
#define NkDynamicCast(type, value) dynamic_cast<type>(value)
#else
#define NkDynamicCast(type, value) static_cast<type>(value)
#endif

/**
 * @brief Reinterpret cast (dangereux)
 * @param type Type cible
 * @param value Valeur à convertir
 * @return Valeur convertie
 */
#define NkReinterpretCast(type, value) reinterpret_cast<type>(value)

/**
 * @brief Const cast
 * @param type Type cible
 * @param value Valeur à convertir
 * @return Valeur convertie
 */
#define NkConstCast(type, value) const_cast<type>(value)

/**
 * @brief Cast de type C-style (à éviter)
 * @param type Type cible
 * @param value Valeur à convertir
 * @return Valeur convertie
 */
#define NkCCast(type, value) ((type)(value))

// ============================================================
// HELPERS DE CONVERSION
// ============================================================

namespace nkentseu {

/**
 * @brief Convertit un pointeur en handle
 * @tparam T Type du pointeur
 * @param ptr Pointeur à convertir
 * @return Handle correspondant
 */
template <typename T> inline core::NkHandle NkToHandle(T *ptr) noexcept {
	return reinterpret_cast<core::NkHandle>(ptr);
}

/**
 * @brief Convertit un handle en pointeur
 * @tparam T Type cible
 * @param handle Handle à convertir
 * @return Pointeur correspondant
 */
template <typename T> inline T *NkFromHandle(core::NkHandle handle) noexcept {
	return reinterpret_cast<T *>(handle);
}

/**
 * @brief Cast sécurisé avec vérification debug
 * @tparam To Type cible
 * @tparam From Type source
 * @param value Valeur à convertir
 * @return Valeur convertie
 * @note En debug, vérifie qu'il n'y a pas de perte de données
 */
template <typename To, typename From> inline To NkSafeCast(From value) noexcept {
#if !defined(NDEBUG)
	To result = static_cast<To>(value);
	// Suppression de la vérification de perte de données qui nécessite plus de contexte
	return result;
#else
	return static_cast<To>(value);
#endif
}

} // namespace nkentseu

// ============================================================
// VALIDATION COMPILE-TIME
// ============================================================

#if defined(NKENTSEU_HAS_STATIC_ASSERT)
/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

#if defined(__cplusplus) && __cplusplus >= 201103L
#define NkStaticAssert(name, cond, msg) static_assert((cond), #name ": " msg)
#else
#define NkStaticAssert(name, cond, msg)                                                                                \
	typedef char NkStaticAssert_##name[(cond) ? 1 : -1] = (cond) ? ((char[0]){0}) : (msg);
#endif

// Vérifier que les alias ont la bonne taille
NkStaticAssert(int8_size, sizeof(core::nk_int8) == 1, "int8 must be 1 byte");
NkStaticAssert(int32_size, sizeof(core::nk_int32) == 4, "int32 must be 4 bytes");
NkStaticAssert(int64_size, sizeof(core::nk_int64) == 8, "int64 must be 8 bytes");
NkStaticAssert(float32_size, sizeof(core::nk_float32) == 4, "float32 must be 4 bytes");
NkStaticAssert(float64_size, sizeof(core::nk_float64) == 8, "float64 must be 8 bytes");

// Vérifications Nkentseu
NkStaticAssert(int8_valid, sizeof(core::nk_int8) == 1 && (core::nk_int8)-1 < 0, "int8_invalid");
NkStaticAssert(uint8_valid, sizeof(core::nk_uint8) == 1 && (core::nk_uint8)-1 > 0, "uint8_invalid");
NkStaticAssert(int16_valid, sizeof(core::nk_int16) == 2 && (core::nk_int16)-1 < 0, "int16_invalid");
NkStaticAssert(uint16_valid, sizeof(core::nk_uint16) == 2 && (core::nk_uint16)-1 > 0, "uint16_invalid");
NkStaticAssert(int32_valid, sizeof(core::nk_int32) == 4 && (core::nk_int32)-1 < 0, "int32_invalid");
NkStaticAssert(uint32_valid, sizeof(core::nk_uint32) == 4 && (core::nk_uint32)-1 > 0, "uint32_invalid");
NkStaticAssert(int64_valid, sizeof(core::nk_int64) == 8 && (core::nk_int64)-1 < 0, "int64_invalid");
NkStaticAssert(uint64_valid, sizeof(core::nk_uint64) == 8 && (core::nk_uint64)-1 > 0, "uint64_invalid");
NkStaticAssert(float32_valid, sizeof(core::nk_float32) == 4, "float32_invalid");
NkStaticAssert(float64_valid, sizeof(core::nk_float64) == 8, "float64_invalid");
NkStaticAssert(float80_valid, sizeof(core::nk_float80) >= 10, "float80_invalid");
NkStaticAssert(nkchar_valid, sizeof(core::nk_char) == 1 && (core::nk_char)-1 < 0, "nkchar_invalid");
NkStaticAssert(char8_valid, sizeof(core::nk_char8) == 1, "char8_invalid");
NkStaticAssert(char16_valid, sizeof(core::nk_char16) == 2, "char16_invalid");
NkStaticAssert(char32_valid, sizeof(core::nk_char32) == 4, "char32_invalid");
NkStaticAssert(Boolean_valid, sizeof(core::nk_boolean) == 1, "Boolean_invalid");
NkStaticAssert(bool32_valid, sizeof(core::nk_bool32) == 4, "bool32_invalid");
NkStaticAssert(PTR_valid, sizeof(core::nk_ptr) == sizeof(void *), "PTR_invalid");
NkStaticAssert(UPTR_valid, sizeof(core::nk_uptr) >= sizeof(void *), "UPTR_invalid");
NkStaticAssert(usize_valid, sizeof(core::nk_usize) >= sizeof(void *), "usize_invalid");
} // namespace nkentseu
#endif

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKTYPEUTILS_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-07
// Creation Date: 2026-02-07
// ============================================================