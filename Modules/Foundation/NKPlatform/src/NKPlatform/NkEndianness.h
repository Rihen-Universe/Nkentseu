// -----------------------------------------------------------------------------
// FICHIER: Core\Nkentseu\src\Nkentseu\Platform\NkEndianness.h
// DESCRIPTION: Endianness detection and byte order conversion
// AUTEUR: Rihen
// DATE: 2026-02-12
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKENDIANNESS_H_INCLUDED
#define NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKENDIANNESS_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NkPlatformDetect.h"
#include "NkCompilerDetect.h"
#include <string.h>

// ============================================================================
// ENDIANNESS DETECTION
// ============================================================================

namespace nkentseu {
/**
 * @brief Namespace platform.
 */
namespace platform {

/**
 * @brief Endianness enumeration
 */
enum class Endianness {
	Little, // Least significant byte first (x86, ARM)
	Big,	// Most significant byte first (Network, PowerPC)
	Unknown
};

/**
 * @brief Compile-time endianness detection
 */
constexpr Endianness GetCompileTimeEndianness() {
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return Endianness::Little;
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return Endianness::Big;
#elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
	// Most common platforms are little-endian
	return Endianness::Little;
#elif defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)
	return Endianness::Big;
#else
	return Endianness::Unknown;
#endif
}

/**
 * @brief Runtime endianness detection
 */
inline Endianness GetRuntimeEndianness() {
	constexpr union {
		uint32_t value;
		uint8_t bytes[4];
	} test = {0x01020304};

	if (test.bytes[0] == 0x04) {
		return Endianness::Little;
	} else if (test.bytes[0] == 0x01) {
		return Endianness::Big;
	} else {
		return Endianness::Unknown;
	}
}

/**
 * @brief Check if system is little-endian
 */
constexpr bool IsLittleEndian() {
	return GetCompileTimeEndianness() == Endianness::Little;
}

/**
 * @brief Check if system is big-endian
 */
constexpr bool IsBigEndian() {
	return GetCompileTimeEndianness() == Endianness::Big;
}

} // namespace platform
} // namespace nkentseu

// ============================================================================
// COMPILE-TIME ENDIANNESS MACROS
// ============================================================================

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define NK_LITTLE_ENDIAN 1
#define NK_BIG_ENDIAN 0
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define NK_LITTLE_ENDIAN 0
#define NK_BIG_ENDIAN 1
#elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
#define NK_LITTLE_ENDIAN 1
#define NK_BIG_ENDIAN 0
#else
#define NK_LITTLE_ENDIAN 0
#define NK_BIG_ENDIAN 0
#define NK_ENDIAN_UNKNOWN 1
#endif

// ============================================================================
// BYTE SWAP FUNCTIONS
// ============================================================================

namespace nkentseu {
/**
 * @brief Namespace platform.
 */
namespace platform {

/**
 * @brief Swap bytes of 16-bit integer
 */
NK_FORCEINLINE constexpr uint16_t ByteSwap16(uint16_t value) {
#if NK_COMPILER_MSVC
	return _byteswap_ushort(value);
#elif NK_COMPILER_GCC || NK_COMPILER_CLANG
	return __builtin_bswap16(value);
#else
	return (value >> 8) | (value << 8);
#endif
}

/**
 * @brief Swap bytes of 32-bit integer
 */
NK_FORCEINLINE constexpr uint32_t ByteSwap32(uint32_t value) {
#if NK_COMPILER_MSVC
	return _byteswap_ulong(value);
#elif NK_COMPILER_GCC || NK_COMPILER_CLANG
	return __builtin_bswap32(value);
#else
	return ((value >> 24) & 0x000000FF) | ((value >> 8) & 0x0000FF00) | ((value << 8) & 0x00FF0000) |
		   ((value << 24) & 0xFF000000);
#endif
}

/**
 * @brief Swap bytes of 64-bit integer
 */
NK_FORCEINLINE constexpr uint64_t ByteSwap64(uint64_t value) {
#if NK_COMPILER_MSVC
	return _byteswap_uint64(value);
#elif NK_COMPILER_GCC || NK_COMPILER_CLANG
	return __builtin_bswap64(value);
#else
	return ((value >> 56) & 0x00000000000000FFULL) | ((value >> 40) & 0x000000000000FF00ULL) |
		   ((value >> 24) & 0x0000000000FF0000ULL) | ((value >> 8) & 0x00000000FF000000ULL) |
		   ((value << 8) & 0x000000FF00000000ULL) | ((value << 24) & 0x0000FF0000000000ULL) |
		   ((value << 40) & 0x00FF000000000000ULL) | ((value << 56) & 0xFF00000000000000ULL);
#endif
}

/**
 * @brief Generic byte swap (template)
 */
template <typename T> NK_FORCEINLINE constexpr T ByteSwap(T value) {
	static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "ByteSwap only supports 2, 4, or 8 byte types");

	if constexpr (sizeof(T) == 2) {
		union {
			T val;
			uint16_t u16;
		} u;
		u.val = value;
		u.u16 = ByteSwap16(u.u16);
		return u.val;
	} else if constexpr (sizeof(T) == 4) {
		union {
			T val;
			uint32_t u32;
		} u;
		u.val = value;
		u.u32 = ByteSwap32(u.u32);
		return u.val;
	} else if constexpr (sizeof(T) == 8) {
		union {
			T val;
			uint64_t u64;
		} u;
		u.val = value;
		u.u64 = ByteSwap64(u.u64);
		return u.val;
	}
}

// ====================================================================
// NETWORK BYTE ORDER CONVERSION (Big-endian)
// ====================================================================

/**
 * @brief Convert 16-bit value from host to network byte order
 */
NK_FORCEINLINE constexpr uint16_t HostToNetwork16(uint16_t value) {
#if NK_LITTLE_ENDIAN
	return ByteSwap16(value);
#else
	return value;
#endif
}

/**
 * @brief Convert 32-bit value from host to network byte order
 */
NK_FORCEINLINE constexpr uint32_t HostToNetwork32(uint32_t value) {
#if NK_LITTLE_ENDIAN
	return ByteSwap32(value);
#else
	return value;
#endif
}

/**
 * @brief Convert 64-bit value from host to network byte order
 */
NK_FORCEINLINE constexpr uint64_t HostToNetwork64(uint64_t value) {
#if NK_LITTLE_ENDIAN
	return ByteSwap64(value);
#else
	return value;
#endif
}

/**
 * @brief Convert 16-bit value from network to host byte order
 */
NK_FORCEINLINE constexpr uint16_t NetworkToHost16(uint16_t value) {
	return HostToNetwork16(value); // Same operation
}

/**
 * @brief Convert 32-bit value from network to host byte order
 */
NK_FORCEINLINE constexpr uint32_t NetworkToHost32(uint32_t value) {
	return HostToNetwork32(value); // Same operation
}

/**
 * @brief Convert 64-bit value from network to host byte order
 */
NK_FORCEINLINE constexpr uint64_t NetworkToHost64(uint64_t value) {
	return HostToNetwork64(value); // Same operation
}

// ====================================================================
// LITTLE-ENDIAN CONVERSION
// ====================================================================

/**
 * @brief Convert to little-endian
 */
template <typename T> NK_FORCEINLINE constexpr T ToLittleEndian(T value) {
#if NK_LITTLE_ENDIAN
	return value;
#else
	return ByteSwap(value);
#endif
}

/**
 * @brief Convert from little-endian
 */
template <typename T> NK_FORCEINLINE constexpr T FromLittleEndian(T value) {
	return ToLittleEndian(value); // Same operation
}

// ====================================================================
// BIG-ENDIAN CONVERSION
// ====================================================================

/**
 * @brief Convert to big-endian
 */
template <typename T> NK_FORCEINLINE constexpr T ToBigEndian(T value) {
#if NK_BIG_ENDIAN
	return value;
#else
	return ByteSwap(value);
#endif
}

/**
 * @brief Convert from big-endian
 */
template <typename T> NK_FORCEINLINE constexpr T FromBigEndian(T value) {
	return ToBigEndian(value); // Same operation
}

// ====================================================================
// BUFFER BYTE SWAP
// ====================================================================

/**
 * @brief Swap bytes of an entire buffer
 * @param data Pointer to data buffer
 * @param count Number of elements (not bytes)
 */
template <typename T> inline void ByteSwapBuffer(T *data, size_t count) {
	for (size_t i = 0; i < count; ++i) {
		data[i] = ByteSwap(data[i]);
	}
}

/**
 * @brief Convert buffer to little-endian
 */
template <typename T> inline void BufferToLittleEndian(T *data, size_t count) {
#if !NK_LITTLE_ENDIAN
	ByteSwapBuffer(data, count);
#endif
}

/**
 * @brief Convert buffer from little-endian
 */
template <typename T> inline void BufferFromLittleEndian(T *data, size_t count) {
	BufferToLittleEndian(data, count); // Same operation
}

/**
 * @brief Convert buffer to big-endian
 */
template <typename T> inline void BufferToBigEndian(T *data, size_t count) {
#if !NK_BIG_ENDIAN
	ByteSwapBuffer(data, count);
#endif
}

/**
 * @brief Convert buffer from big-endian
 */
template <typename T> inline void BufferFromBigEndian(T *data, size_t count) {
	BufferToBigEndian(data, count); // Same operation
}

// ====================================================================
// UNALIGNED MEMORY ACCESS
// ====================================================================

/**
 * @brief Read 16-bit value from potentially unaligned memory
 */
inline uint16_t ReadUnaligned16(const void *ptr) {
	uint16_t value;
	::memcpy(&value, ptr, sizeof(uint16_t));
	return value;
}

/**
 * @brief Read 32-bit value from potentially unaligned memory
 */
inline uint32_t ReadUnaligned32(const void *ptr) {
	uint32_t value;
	::memcpy(&value, ptr, sizeof(uint32_t));
	return value;
}

/**
 * @brief Read 64-bit value from potentially unaligned memory
 */
inline uint64_t ReadUnaligned64(const void *ptr) {
	uint64_t value;
	::memcpy(&value, ptr, sizeof(uint64_t));
	return value;
}

/**
 * @brief Write 16-bit value to potentially unaligned memory
 */
inline void WriteUnaligned16(void *ptr, uint16_t value) {
	::memcpy(ptr, &value, sizeof(uint16_t));
}

/**
 * @brief Write 32-bit value to potentially unaligned memory
 */
inline void WriteUnaligned32(void *ptr, uint32_t value) {
	::memcpy(ptr, &value, sizeof(uint32_t));
}

/**
 * @brief Write 64-bit value to potentially unaligned memory
 */
inline void WriteUnaligned64(void *ptr, uint64_t value) {
	::memcpy(ptr, &value, sizeof(uint64_t));
}

// ====================================================================
// COMBINED UNALIGNED + ENDIAN CONVERSION
// ====================================================================

/**
 * @brief Read 16-bit little-endian value from unaligned memory
 */
inline uint16_t ReadLE16(const void *ptr) {
	return FromLittleEndian(ReadUnaligned16(ptr));
}

/**
 * @brief Read 32-bit little-endian value from unaligned memory
 */
inline uint32_t ReadLE32(const void *ptr) {
	return FromLittleEndian(ReadUnaligned32(ptr));
}

/**
 * @brief Read 64-bit little-endian value from unaligned memory
 */
inline uint64_t ReadLE64(const void *ptr) {
	return FromLittleEndian(ReadUnaligned64(ptr));
}

/**
 * @brief Read 16-bit big-endian value from unaligned memory
 */
inline uint16_t ReadBE16(const void *ptr) {
	return FromBigEndian(ReadUnaligned16(ptr));
}

/**
 * @brief Read 32-bit big-endian value from unaligned memory
 */
inline uint32_t ReadBE32(const void *ptr) {
	return FromBigEndian(ReadUnaligned32(ptr));
}

/**
 * @brief Read 64-bit big-endian value from unaligned memory
 */
inline uint64_t ReadBE64(const void *ptr) {
	return FromBigEndian(ReadUnaligned64(ptr));
}

/**
 * @brief Write 16-bit little-endian value to unaligned memory
 */
inline void WriteLE16(void *ptr, uint16_t value) {
	WriteUnaligned16(ptr, ToLittleEndian(value));
}

/**
 * @brief Write 32-bit little-endian value to unaligned memory
 */
inline void WriteLE32(void *ptr, uint32_t value) {
	WriteUnaligned32(ptr, ToLittleEndian(value));
}

/**
 * @brief Write 64-bit little-endian value to unaligned memory
 */
inline void WriteLE64(void *ptr, uint64_t value) {
	WriteUnaligned64(ptr, ToLittleEndian(value));
}

/**
 * @brief Write 16-bit big-endian value to unaligned memory
 */
inline void WriteBE16(void *ptr, uint16_t value) {
	WriteUnaligned16(ptr, ToBigEndian(value));
}

/**
 * @brief Write 32-bit big-endian value to unaligned memory
 */
inline void WriteBE32(void *ptr, uint32_t value) {
	WriteUnaligned32(ptr, ToBigEndian(value));
}

/**
 * @brief Write 64-bit big-endian value to unaligned memory
 */
inline void WriteBE64(void *ptr, uint64_t value) {
	WriteUnaligned64(ptr, ToBigEndian(value));
}

} // namespace platform
} // namespace nkentseu

// ============================================================================
// CONVENIENCE MACROS (Optional)
// ============================================================================

// Short aliases for common operations
#define NK_BSWAP16(x) nkentseu::platform::ByteSwap16(x)
#define NK_BSWAP32(x) nkentseu::platform::ByteSwap32(x)
#define NK_BSWAP64(x) nkentseu::platform::ByteSwap64(x)

#define NK_HTON16(x) nkentseu::platform::HostToNetwork16(x)
#define NK_HTON32(x) nkentseu::platform::HostToNetwork32(x)
#define NK_HTON64(x) nkentseu::platform::HostToNetwork64(x)

#define NK_NTOH16(x) nkentseu::platform::NetworkToHost16(x)
#define NK_NTOH32(x) nkentseu::platform::NetworkToHost32(x)
#define NK_NTOH64(x) nkentseu::platform::NetworkToHost64(x)

#endif // NK_CORE_NKENTSEU_SRC_NKENTSEU_PLATFORM_NKENDIANNESS_H_INCLUDED

// ============================================================
// Copyright Â© 2024-2026 Rihen. All rights reserved.
// ============================================================
