// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringHash.h
// DESCRIPTION: String hashing functions
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#pragma once
#include <cstdio>
#include <cstddef>

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGHASH_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGHASH_H_INCLUDED

#include "NKContainers/NkCompat.h"
#include "NkStringView.h"
#include "NKCore/NkTypes.h"
#include "NKMemory/NkMemoryFn.h"

namespace nkentseu {
    namespace core {
        namespace string {
            
            // ========================================
            // HASHING ALGORITHMS
            // ========================================
            
            /**
             * @brief FNV-1a hash (32-bit)
             */
            NKENTSEU_CORE_API uint32 NkHashFNV1a32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashFNV1a32(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashFNV1a32(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief FNV-1a hash (64-bit)
             */
            NKENTSEU_CORE_API uint64 NkHashFNV1a64(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashFNV1a64(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashFNV1a64(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief MurmurHash3 (32-bit)
             */
            NKENTSEU_CORE_API uint32 NkHashMurmur3_32(NkStringView str, uint32 seed = 0) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashMurmur3_32(const char* str, usize length, uint32 seed = 0) NK_NOEXCEPT;
            
            /**
             * @brief MurmurHash3 (128-bit)
             */
            NKENTSEU_CORE_API void NkHashMurmur3_128(NkStringView str, uint64 out[2], uint32 seed = 0) NK_NOEXCEPT;
            NKENTSEU_CORE_API void NkHashMurmur3_128(const char* str, usize length, uint64 out[2], uint32 seed = 0) NK_NOEXCEPT;
            
            /**
             * @brief CityHash (64-bit) - très rapide
             */
            NKENTSEU_CORE_API uint64 NkHashCity64(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashCity64(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief CityHash (128-bit)
             */
            NKENTSEU_CORE_API void NkHashCity128(NkStringView str, uint64 out[2]) NK_NOEXCEPT;
            NKENTSEU_CORE_API void NkHashCity128(const char* str, usize length, uint64 out[2]) NK_NOEXCEPT;
            
            /**
             * @brief SDBM hash (simple but good distribution)
             */
            NKENTSEU_CORE_API uint32 NkHashSDBM32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashSDBM32(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashSDBM32(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief DJB2 hash (Bernstein hash)
             */
            // NKENTSEU_CORE_API uint32 NkHashDJB2(NkStringView str) NK_NOEXCEPT;
            // NKENTSEU_CORE_API uint32 NkHashDJB2(const char* str) NK_NOEXCEPT;
            // NKENTSEU_CORE_API uint32 NkHashDJB2(const char* str, usize length) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashDJB2_32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashDJB2_32(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashDJB2_32(const char* str, usize length) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashDJB2_64(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashDJB2_64(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief CRC32 hash (cyclic redundancy check)
             */
            NKENTSEU_CORE_API uint32 NkHashCRC32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashCRC32(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashCRC32(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief Adler-32 checksum
             */
            NKENTSEU_CORE_API uint32 NkHashAdler32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashAdler32(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashAdler32(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief XXHash (32-bit) - very fast
             */
            NKENTSEU_CORE_API uint32 NkHashXX32(NkStringView str, uint32 seed = 0) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashXX32(const char* str, usize length, uint32 seed = 0) NK_NOEXCEPT;
            
            /**
             * @brief XXHash (64-bit)
             */
            NKENTSEU_CORE_API uint64 NkHashXX64(NkStringView str, uint64 seed = 0) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashXX64(const char* str, usize length, uint64 seed = 0) NK_NOEXCEPT;
            
            /**
             * @brief Jenkins hash (one-at-a-time)
             */
            NKENTSEU_CORE_API uint32 NkHashJenkins32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashJenkins32(const char* str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashJenkins32(const char* str, usize length) NK_NOEXCEPT;
            
            /**
             * @brief Lookup3 hash (Bob Jenkins)
             */
            NKENTSEU_CORE_API uint32 NkHashLookup3(NkStringView str, uint32 seed = 0) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint32 NkHashLookup3(const char* str, usize length, uint32 seed = 0) NK_NOEXCEPT;
            
            /**
             * @brief SpookyHash (Bob Jenkins) - 128-bit
             */
            NKENTSEU_CORE_API void NkHashSpooky128(NkStringView str, uint64 out[2], uint64 seed1 = 0, uint64 seed2 = 0) NK_NOEXCEPT;
            NKENTSEU_CORE_API void NkHashSpooky128(const char* str, usize length, uint64 out[2], uint64 seed1 = 0, uint64 seed2 = 0) NK_NOEXCEPT;
            
            // ========================================
            // HASH WRAPPERS & DEFAULT HASHES
            // ========================================
            
            /**
             * @brief Hash par défaut (FNV-1a 64-bit)
             */
            inline uint64 NkHash(NkStringView str) NK_NOEXCEPT {
                return NkHashFNV1a64(str);
            }
            
            inline uint32 NkHash32(NkStringView str) NK_NOEXCEPT {
                return NkHashFNV1a32(str);
            }
            
            inline uint64 NkHash64(NkStringView str) NK_NOEXCEPT {
                return NkHashFNV1a64(str);
            }
            
            /**
             * @brief Hash combining function
             */
            template<typename T>
            inline T NkHashCombine(T seed, const T& hash) NK_NOEXCEPT {
                // Combine hash values (from Boost)
                return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
            }
            
            /**
             * @brief Hash combine multiple values
             */
            template<typename T, typename... Args>
            inline T NkHashCombine(T seed, const T& hash, Args... args) NK_NOEXCEPT {
                seed = NkHashCombine(seed, hash);
                return NkHashCombine(seed, args...);
            }
            
            // ========================================
            // CASE-INSENSITIVE HASHES
            // ========================================
            
            /**
             * @brief Case-insensitive hash
             */
            NKENTSEU_CORE_API uint32 NkHashIgnoreCase32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashIgnoreCase64(NkStringView str) NK_NOEXCEPT;
            
            NKENTSEU_CORE_API uint32 NkHashFNV1aIgnoreCase32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashFNV1aIgnoreCase64(NkStringView str) NK_NOEXCEPT;
            
            /**
             * @brief Default case-insensitive hash
             */
            inline uint32 NkHash32IgnoreCase(NkStringView str) NK_NOEXCEPT {
                return NkHashIgnoreCase32(str);
            }
            
            inline uint64 NkHash64IgnoreCase(NkStringView str) NK_NOEXCEPT {
                return NkHashIgnoreCase64(str);
            }
            
            // ========================================
            // COMPILE-TIME HASHING
            // ========================================
            
            /**
             * @brief Compile-time string hash (constexpr FNV-1a)
             */
            constexpr uint32 NkHashCompileTime32(const char* str, usize length) NK_NOEXCEPT {
                uint32 hash = 2166136261u;  // FNV offset basis
                for (usize i = 0; i < length; ++i) {
                    hash ^= static_cast<uint32>(str[i]);
                    hash *= 16777619u;  // FNV prime
                }
                return hash;
            }
            
            constexpr uint64 NkHashCompileTime64(const char* str, usize length) NK_NOEXCEPT {
                uint64 hash = 14695981039346656037ULL;  // FNV offset basis
                for (usize i = 0; i < length; ++i) {
                    hash ^= static_cast<uint64>(str[i]);
                    hash *= 1099511628211ULL;  // FNV prime
                }
                return hash;
            }
            
            /**
             * @brief String hash literals (compile-time)
             */
            #if defined(NK_CPP11)
            inline namespace literals {
                constexpr uint32 operator""_hash32(const char* str, std::size_t length) NK_NOEXCEPT {
                    return NkHashCompileTime32(str, static_cast<usize>(length));
                }
                
                constexpr uint64 operator""_hash64(const char* str, std::size_t length) NK_NOEXCEPT {
                    return NkHashCompileTime64(str, static_cast<usize>(length));
                }
                
                constexpr uint32 operator""_hash(const char* str, std::size_t length) NK_NOEXCEPT {
                    return NkHashCompileTime32(str, static_cast<usize>(length));
                }
                
                constexpr uint32 operator""_murmur(const char* str, std::size_t length) NK_NOEXCEPT {
                    // Note: MurmurHash cannot be constexpr, fallback to FNV
                    return NkHashCompileTime32(str, static_cast<usize>(length));
                }
            }
            #endif
            
            // ========================================
            // HASH TABLE SUPPORT
            // ========================================
            
            /**
             * @brief Hash function for std::unordered_map
             */
            struct NKENTSEU_CORE_API NkStringHash {
                usize operator()(NkStringView str) const NK_NOEXCEPT {
                    return static_cast<usize>(NkHash64(str));
                }
                
                usize operator()(const char* str) const NK_NOEXCEPT {
                    return static_cast<usize>(NkHashFNV1a64(str));
                }
            };
            
            /**
             * @brief Case-insensitive hash for std::unordered_map
             */
            struct NKENTSEU_CORE_API NkStringHashIgnoreCase {
                usize operator()(NkStringView str) const NK_NOEXCEPT {
                    return static_cast<usize>(NkHash64IgnoreCase(str));
                }
                
                usize operator()(const char* str) const NK_NOEXCEPT {
                    if (!str) return 0;
                    return static_cast<usize>(NkHashIgnoreCase64(NkStringView(str)));
                }
            };
            
            /**
             * @brief Hash function with seed
             */
            template<typename SeedType = uint32>
            struct NKENTSEU_CORE_API NkSeededStringHash {
                SeedType seed;
                
                explicit NkSeededStringHash(SeedType seed_ = 0) : seed(seed_) {}
                
                usize operator()(NkStringView str) const NK_NOEXCEPT {
                    if constexpr (sizeof(SeedType) <= 4) {
                        return static_cast<usize>(NkHashMurmur3_32(str, static_cast<uint32>(seed)));
                    } else {
                        return static_cast<usize>(NkHashXX64(str, static_cast<uint64>(seed)));
                    }
                }
                
                usize operator()(const char* str) const NK_NOEXCEPT {
                    if (!str) return static_cast<usize>(seed);
                    return operator()(NkStringView(str));
                }
            };
            
            // ========================================
            // HASH COMPARATORS
            // ========================================
            
            /**
             * @brief Hash comparator for equality
             */
            template<typename HashType = NkStringHash>
            struct NKENTSEU_CORE_API NkStringHashEqual {
                bool operator()(NkStringView lhs, NkStringView rhs) const NK_NOEXCEPT {
                    return lhs == rhs;
                }
                
                bool operator()(const char* lhs, const char* rhs) const NK_NOEXCEPT {
                    return NkStringView(lhs) == NkStringView(rhs);
                }
            };
            
            /**
             * @brief Case-insensitive hash comparator
             */
            template<typename HashType = NkStringHashIgnoreCase>
            struct NKENTSEU_CORE_API NkStringHashEqualIgnoreCase {
                bool operator()(NkStringView lhs, NkStringView rhs) const NK_NOEXCEPT {
                    return lhs.EqualsIgnoreCase(rhs);
                }
                
                bool operator()(const char* lhs, const char* rhs) const NK_NOEXCEPT {
                    return NkStringView(lhs).EqualsIgnoreCase(NkStringView(rhs));
                }
            };
            
            // ========================================
            // UTILITY FUNCTIONS
            // ========================================
            
            /**
             * @brief Fast hash for small strings
             */
            NKENTSEU_CORE_API uint32 NkHashFast32(NkStringView str) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashFast64(NkStringView str) NK_NOEXCEPT;
            
            /**
             * @brief Cryptographic hash (SHA-1 simplified)
             */
            NKENTSEU_CORE_API void NkHashSHA1(NkStringView str, uint8 out[20]) NK_NOEXCEPT;
            NKENTSEU_CORE_API void NkHashSHA1(const char* str, usize length, uint8 out[20]) NK_NOEXCEPT;
            
            /**
             * @brief Hash to hex string
             */
            NKENTSEU_CORE_API NkString NkHashToHex32(uint32 hash) NK_NOEXCEPT;
            NKENTSEU_CORE_API NkString NkHashToHex64(uint64 hash) NK_NOEXCEPT;
            NKENTSEU_CORE_API NkString NkHashToHex128(const uint64 hash[2]) NK_NOEXCEPT;
            
            /**
             * @brief Hash to base64 string
             */
            NKENTSEU_CORE_API NkString NkHashToBase64_32(uint32 hash) NK_NOEXCEPT;
            NKENTSEU_CORE_API NkString NkHashToBase64_64(uint64 hash) NK_NOEXCEPT;
            
            /**
             * @brief Generate hash from multiple strings
             */
            NKENTSEU_CORE_API uint64 NkHashMulti(NkStringView str1, NkStringView str2) NK_NOEXCEPT;
            NKENTSEU_CORE_API uint64 NkHashMulti(NkStringView str1, NkStringView str2, NkStringView str3) NK_NOEXCEPT;
            
            template<typename... Args>
            inline uint64 NkHashMulti(NkStringView first, Args... args) NK_NOEXCEPT {
                uint64 hash = NkHash64(first);
                return NkHashCombine(hash, NkHash64(args)...);
            }
            
            /**
             * @brief Hash performance test
             */
            NKENTSEU_CORE_API void NkHashBenchmark(NkStringView testString, usize iterations = 1000000) NK_NOEXCEPT;
            
            /**
             * @brief Check hash collision probability
             */
            NKENTSEU_CORE_API double NkHashCollisionProbability(usize numItems, usize hashSpace) NK_NOEXCEPT;
            
        } // namespace string
    } // namespace core
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_NKSTRINGHASH_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================
