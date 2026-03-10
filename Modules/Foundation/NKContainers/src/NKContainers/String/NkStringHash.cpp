// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\NkStringHash.cpp
// DESCRIPTION: String hashing implementation
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#include "NkStringHash.h"
#include "NkString.h"
#include "Encoding/NkASCII.h"
#include <cstring>
#include <cstdio>  // Pour snprintf
#include <cmath>

namespace nkentseu {
    
        namespace string {
            
            // ========================================
            // SIMPLE BASE64 IMPLEMENTATION (inline pour éviter les dépendances)
            // ========================================
            
            namespace {
                const char BASE64_CHARS[] = 
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";
                
                inline NkString SimpleBase64Encode(const uint8* data, usize length) {
                    NkString result;
                    int i = 0;
                    int j = 0;
                    uint8 char_array_3[3];
                    uint8 char_array_4[4];
                    
                    while (length--) {
                        char_array_3[i++] = *(data++);
                        if (i == 3) {
                            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                            char_array_4[3] = char_array_3[2] & 0x3f;
                            
                            for (i = 0; i < 4; i++) {
                                result.Append(1, BASE64_CHARS[char_array_4[i]]);
                            }
                            i = 0;
                        }
                    }
                    
                    if (i) {
                        for (j = i; j < 3; j++) {
                            char_array_3[j] = '\0';
                        }
                        
                        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                        char_array_4[3] = char_array_3[2] & 0x3f;
                        
                        for (j = 0; j < i + 1; j++) {
                            result.Append(1, BASE64_CHARS[char_array_4[j]]);
                        }
                        
                        while (i++ < 3) {
                            result.Append(1, '=');
                        }
                    }
                    
                    return result;
                }
            }

            // ========================================
            // CONSTANTS
            // ========================================
            
            static const uint32 FNV32_OFFSET_BASIS = 2166136261u;
            static const uint32 FNV32_PRIME = 16777619u;
            
            static const uint64 FNV64_OFFSET_BASIS = 14695981039346656037ULL;
            static const uint64 FNV64_PRIME = 1099511628211ULL;
            
            // ========================================
            // FNV-1a HASH (32-bit)
            // ========================================
            
            uint32 NkHashFNV1a32(NkStringView str) NK_NOEXCEPT {
                return NkHashFNV1a32(str.Data(), str.Length());
            }
            
            uint32 NkHashFNV1a32(const char* str) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 hash = FNV32_OFFSET_BASIS;
                while (*str) {
                    hash ^= static_cast<uint32>(*str++);
                    hash *= FNV32_PRIME;
                }
                return hash;
            }
            
            uint32 NkHashFNV1a32(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 hash = FNV32_OFFSET_BASIS;
                for (usize i = 0; i < length; ++i) {
                    hash ^= static_cast<uint32>(str[i]);
                    hash *= FNV32_PRIME;
                }
                return hash;
            }
            
            // ========================================
            // FNV-1a HASH (64-bit)
            // ========================================
            
            uint64 NkHashFNV1a64(NkStringView str) NK_NOEXCEPT {
                return NkHashFNV1a64(str.Data(), str.Length());
            }
            
            uint64 NkHashFNV1a64(const char* str) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint64 hash = FNV64_OFFSET_BASIS;
                while (*str) {
                    hash ^= static_cast<uint64>(*str++);
                    hash *= FNV64_PRIME;
                }
                return hash;
            }
            
            uint64 NkHashFNV1a64(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint64 hash = FNV64_OFFSET_BASIS;
                for (usize i = 0; i < length; ++i) {
                    hash ^= static_cast<uint64>(str[i]);
                    hash *= FNV64_PRIME;
                }
                return hash;
            }
            
            // ========================================
            // MURMUR HASH 3 (32-bit)
            // ========================================
            
            static NKENTSEU_FORCE_INLINE uint32 NkMurmurRotl32(uint32 x, int8 r) {
                return (x << r) | (x >> (32 - r));
            }
            
            static NKENTSEU_FORCE_INLINE uint32 NkMurmurFmix32(uint32 h) {
                h ^= h >> 16;
                h *= 0x85ebca6b;
                h ^= h >> 13;
                h *= 0xc2b2ae35;
                h ^= h >> 16;
                return h;
            }
            
            uint32 NkHashMurmur3_32(NkStringView str, uint32 seed) NK_NOEXCEPT {
                return NkHashMurmur3_32(str.Data(), str.Length(), seed);
            }
            
            uint32 NkHashMurmur3_32(const char* str, usize length, uint32 seed) NK_NOEXCEPT {
                if (!str) return seed;
                
                const uint8* data = reinterpret_cast<const uint8*>(str);
                const usize nblocks = length / 4;
                
                uint32 h1 = seed;
                const uint32 c1 = 0xcc9e2d51;
                const uint32 c2 = 0x1b873593;
                
                // Body
                const uint32* blocks = reinterpret_cast<const uint32*>(data + nblocks * 4);
                
                for (usize i = 0; i < nblocks; ++i) {
                    uint32 k1 = blocks[-static_cast<intptr>(i) - 1];
                    
                    k1 *= c1;
                    k1 = NkMurmurRotl32(k1, 15);
                    k1 *= c2;
                    
                    h1 ^= k1;
                    h1 = NkMurmurRotl32(h1, 13);
                    h1 = h1 * 5 + 0xe6546b64;
                }
                
                // Tail
                const uint8* tail = data + nblocks * 4;
                uint32 k1 = 0;
                
                switch (length & 3) {
                    case 3: k1 ^= tail[2] << 16; // fallthrough
                    case 2: k1 ^= tail[1] << 8;  // fallthrough
                    case 1: k1 ^= tail[0];
                            k1 *= c1;
                            k1 = NkMurmurRotl32(k1, 15);
                            k1 *= c2;
                            h1 ^= k1;
                }
                
                // Finalization
                h1 ^= static_cast<uint32>(length);
                h1 = NkMurmurFmix32(h1);
                
                return h1;
            }
            
            // ========================================
            // MURMUR HASH 3 (128-bit)
            // ========================================
            
            static NKENTSEU_FORCE_INLINE uint64 NkMurmurRotl64(uint64 x, int8 r) {
                return (x << r) | (x >> (64 - r));
            }
            
            static NKENTSEU_FORCE_INLINE uint64 NkMurmurFmix64(uint64 k) {
                k ^= k >> 33;
                k *= 0xff51afd7ed558ccdULL;
                k ^= k >> 33;
                k *= 0xc4ceb9fe1a85ec53ULL;
                k ^= k >> 33;
                return k;
            }
            
            void NkHashMurmur3_128(NkStringView str, uint64 out[2], uint32 seed) NK_NOEXCEPT {
                NkHashMurmur3_128(str.Data(), str.Length(), out, seed);
            }
            
            void NkHashMurmur3_128(const char* str, usize length, uint64 out[2], uint32 seed) NK_NOEXCEPT {
                if (!str) {
                    out[0] = out[1] = seed;
                    return;
                }
                
                const uint8* data = reinterpret_cast<const uint8*>(str);
                const usize nblocks = length / 16;
                
                uint64 h1 = seed;
                uint64 h2 = seed;
                
                const uint64 c1 = 0x87c37b91114253d5ULL;
                const uint64 c2 = 0x4cf5ad432745937fULL;
                
                // Body
                const uint64* blocks = reinterpret_cast<const uint64*>(data);
                
                for (usize i = 0; i < nblocks; ++i) {
                    uint64 k1 = blocks[i * 2];
                    uint64 k2 = blocks[i * 2 + 1];
                    
                    k1 *= c1;
                    k1 = NkMurmurRotl64(k1, 31);
                    k1 *= c2;
                    h1 ^= k1;
                    
                    h1 = NkMurmurRotl64(h1, 27);
                    h1 += h2;
                    h1 = h1 * 5 + 0x52dce729;
                    
                    k2 *= c2;
                    k2 = NkMurmurRotl64(k2, 33);
                    k2 *= c1;
                    h2 ^= k2;
                    
                    h2 = NkMurmurRotl64(h2, 31);
                    h2 += h1;
                    h2 = h2 * 5 + 0x38495ab5;
                }
                
                // Tail
                const uint8* tail = data + nblocks * 16;
                uint64 k1 = 0;
                uint64 k2 = 0;
                
                switch (length & 15) {
                    case 15: k2 ^= static_cast<uint64>(tail[14]) << 48;
                    case 14: k2 ^= static_cast<uint64>(tail[13]) << 40;
                    case 13: k2 ^= static_cast<uint64>(tail[12]) << 32;
                    case 12: k2 ^= static_cast<uint64>(tail[11]) << 24;
                    case 11: k2 ^= static_cast<uint64>(tail[10]) << 16;
                    case 10: k2 ^= static_cast<uint64>(tail[9]) << 8;
                    case 9:  k2 ^= static_cast<uint64>(tail[8]) << 0;
                            k2 *= c2;
                            k2 = NkMurmurRotl64(k2, 33);
                            k2 *= c1;
                            h2 ^= k2;
                    case 8:  k1 ^= static_cast<uint64>(tail[7]) << 56;
                    case 7:  k1 ^= static_cast<uint64>(tail[6]) << 48;
                    case 6:  k1 ^= static_cast<uint64>(tail[5]) << 40;
                    case 5:  k1 ^= static_cast<uint64>(tail[4]) << 32;
                    case 4:  k1 ^= static_cast<uint64>(tail[3]) << 24;
                    case 3:  k1 ^= static_cast<uint64>(tail[2]) << 16;
                    case 2:  k1 ^= static_cast<uint64>(tail[1]) << 8;
                    case 1:  k1 ^= static_cast<uint64>(tail[0]) << 0;
                            k1 *= c1;
                            k1 = NkMurmurRotl64(k1, 31);
                            k1 *= c2;
                            h1 ^= k1;
                }
                
                // Finalization
                h1 ^= length;
                h2 ^= length;
                
                h1 += h2;
                h2 += h1;
                
                h1 = NkMurmurFmix64(h1);
                h2 = NkMurmurFmix64(h2);
                
                h1 += h2;
                h2 += h1;
                
                out[0] = h1;
                out[1] = h2;
            }
            
            // ========================================
            // CITY HASH (64-bit)
            // ========================================
            
            static const uint64 k0 = 0xc3a5c85c97cb3127ULL;
            static const uint64 k1 = 0xb492b66fbe98f273ULL;
            static const uint64 k2 = 0x9ae16a3b2f90404fULL;
            static const uint64 k3 = 0xc949d7c7509e6557ULL;
            
            static NKENTSEU_FORCE_INLINE uint64 NkCityRotate(uint64 val, int shift) {
                return shift == 0 ? val : ((val >> shift) | (val << (64 - shift)));
            }
            
            static NKENTSEU_FORCE_INLINE uint64 NkCityShiftMix(uint64 val) {
                return val ^ (val >> 47);
            }
            
            static NKENTSEU_FORCE_INLINE uint64 NkCityHashLen16(uint64 u, uint64 v) {
                uint64 a = (u ^ v) * k2;
                a ^= (a >> 47);
                uint64 b = (v ^ a) * k2;
                b ^= (b >> 47);
                b *= k2;
                return b;
            }
            
            uint64 NkHashCity64(NkStringView str) NK_NOEXCEPT {
                return NkHashCity64(str.Data(), str.Length());
            }
            
            uint64 NkHashCity64(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 0;
                if (length == 0) return k2;
                
                const uint8* data = reinterpret_cast<const uint8*>(str);
                
                if (length <= 16) {
                    uint64 a = k2 + length * 2;
                    uint64 b = k0;
                    
                    if (length >= 8) {
                        uint64 val;
                        memory::NkMemCopy(&val, data, sizeof(uint64));
                        a = k2 + val;
                        memory::NkMemCopy(&val, data + length - 8, sizeof(uint64));
                        b = k1 + val;
                    } else if (length >= 4) {
                        uint32 val1, val2;
                        memory::NkMemCopy(&val1, data, sizeof(uint32));
                        memory::NkMemCopy(&val2, data + length - 4, sizeof(uint32));
                        a = k2 + static_cast<uint64>(val1);
                        b = k0 + static_cast<uint64>(val2);
                    }
                    
                    return NkCityHashLen16(a, b);
                }
                
                // For longer strings, use simplified version
                uint64 x = k2;
                uint64 y = k1;
                uint64 z = length * k2;
                
                for (usize i = 0; i < length; i += 8) {
                    usize remaining = length - i;
                    if (remaining >= 8) {
                        uint64 val;
                        memory::NkMemCopy(&val, data + i, sizeof(uint64));
                        x = NkCityRotate(x + val, 37) * k1;
                        y = NkCityRotate(y, 42) * k1;
                        z ^= val;
                    } else {
                        // Handle remaining bytes
                        for (usize j = 0; j < remaining; ++j) {
                            x ^= static_cast<uint64>(data[i + j]) << (j * 8);
                        }
                    }
                }
                
                return NkCityHashLen16(NkCityHashLen16(x, y), z);
            }
            
            // ========================================
            // CITY HASH (128-bit)
            // ========================================
            
            template<typename first, typename second>
            struct NkStringPair {
                first First;
                second Second;

                NkStringPair() = default;
                NkStringPair(const first& f, const second& s) : First(f), Second(s) {}

                bool operator==(const NkStringPair& other) const {
                    return First == other.First && Second == other.Second;
                }

                bool operator!=(const NkStringPair& other) const {
                    return !(*this == other);
                }

                struct Hash {
                    usize operator()(const NkStringPair& pair) const NK_NOEXCEPT {
                        return static_cast<usize>(NkHashFNV1a64(pair.First) ^ (NkHashFNV1a64(pair.Second) << 1));
                    }
                };
            };
            
            static NKENTSEU_FORCE_INLINE NkStringPair<uint64, uint64> NkCityHashLen16Pair(uint64 u, uint64 v, uint64 mul) {
                uint64 a = (u ^ v) * mul;
                a ^= (a >> 47);
                uint64 b = (v ^ a) * mul;
                b ^= (b >> 47);
                b *= mul;
                return NkStringPair<uint64, uint64>(a, b);
            }
            
            void NkHashCity128(NkStringView str, uint64 out[2]) NK_NOEXCEPT {
                NkHashCity128(str.Data(), str.Length(), out);
            }
            
            void NkHashCity128(const char* str, usize length, uint64 out[2]) NK_NOEXCEPT {
                if (!str || length == 0) {
                    out[0] = out[1] = 0;
                    return;
                }
                
                // Simplified version - for full implementation see Google's CityHash
                uint64 hash64 = NkHashCity64(str, length);
                out[0] = hash64;
                out[1] = NkMurmurFmix64(hash64 ^ k3);
            }
            
            // ========================================
            // SDBM HASH
            // ========================================
            
            uint32 NkHashSDBM32(NkStringView str) NK_NOEXCEPT {
                return NkHashSDBM32(str.Data(), str.Length());
            }
            
            uint32 NkHashSDBM32(const char* str) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 hash = 0;
                while (*str) {
                    hash = static_cast<uint32>(*str++) + (hash << 6) + (hash << 16) - hash;
                }
                return hash;
            }
            
            uint32 NkHashSDBM32(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 hash = 0;
                for (usize i = 0; i < length; ++i) {
                    hash = static_cast<uint32>(str[i]) + (hash << 6) + (hash << 16) - hash;
                }
                return hash;
            }
            
            // ========================================
            // DJB2 HASH
            // ========================================
            
            uint32 NkHashDJB2_32(NkStringView str) NK_NOEXCEPT {
                return NkHashDJB2_32(str.Data(), str.Length());
            }

            uint32 NkHashDJB2_32(const char* str) NK_NOEXCEPT {
                if (!str) return 5381;
                
                uint32 hash = 5381;
                while (*str) {
                    hash = ((hash << 5) + hash) + static_cast<uint32>(*str++);
                }
                return hash;
            }

            uint32 NkHashDJB2_32(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 5381;
                
                uint32 hash = 5381;
                for (usize i = 0; i < length; ++i) {
                    hash = ((hash << 5) + hash) + static_cast<uint32>(str[i]);
                }
                return hash;
            }

            uint64 NkHashDJB2_64(NkStringView str) NK_NOEXCEPT {
                return NkHashDJB2_64(str.Data(), str.Length());
            }
            
            uint64 NkHashDJB2_64(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 5381;
                
                uint64 hash = 5381;
                for (usize i = 0; i < length; ++i) {
                    hash = ((hash << 5) + hash) + static_cast<uint64>(str[i]);
                }
                return hash;
            }
            
            // ========================================
            // CRC32 HASH
            // ========================================
            
            static const uint32 CRC32_TABLE[256] = {
                0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
                0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
                0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
                0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
                0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
                0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
                0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
                0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
                0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
                0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
                0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
                0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
                0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
                0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
                0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
                0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
                0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
                0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
                0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
                0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
                0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
                0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
                0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
                0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
                0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
                0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
                0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
                0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
                0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
                0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
                0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
                0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
                0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
                0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
                0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
                0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
                0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
                0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
                0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
                0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
                0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
                0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
                0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
            };
            
            uint32 NkHashCRC32(NkStringView str) NK_NOEXCEPT {
                return NkHashCRC32(str.Data(), str.Length());
            }
            
            uint32 NkHashCRC32(const char* str) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 crc = 0xFFFFFFFF;
                while (*str) {
                    crc = (crc >> 8) ^ CRC32_TABLE[(crc ^ static_cast<uint32>(*str++)) & 0xFF];
                }
                return crc ^ 0xFFFFFFFF;
            }
            
            uint32 NkHashCRC32(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 crc = 0xFFFFFFFF;
                for (usize i = 0; i < length; ++i) {
                    crc = (crc >> 8) ^ CRC32_TABLE[(crc ^ static_cast<uint32>(str[i])) & 0xFF];
                }
                return crc ^ 0xFFFFFFFF;
            }
            
            // ========================================
            // ADLER-32 HASH
            // ========================================
            
            uint32 NkHashAdler32(NkStringView str) NK_NOEXCEPT {
                return NkHashAdler32(str.Data(), str.Length());
            }
            
            uint32 NkHashAdler32(const char* str) NK_NOEXCEPT {
                if (!str) return 1;
                
                uint32 a = 1, b = 0;
                while (*str) {
                    a = (a + static_cast<uint32>(*str++)) % 65521;
                    b = (b + a) % 65521;
                }
                return (b << 16) | a;
            }
            
            uint32 NkHashAdler32(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 1;
                
                uint32 a = 1, b = 0;
                for (usize i = 0; i < length; ++i) {
                    a = (a + static_cast<uint32>(str[i])) % 65521;
                    b = (b + a) % 65521;
                }
                return (b << 16) | a;
            }
            
            // ========================================
            // XXHASH (32-bit)
            // ========================================
            
            static const uint32 PRIME32_1 = 0x9E3779B1;
            static const uint32 PRIME32_2 = 0x85EBCA77;
            static const uint32 PRIME32_3 = 0xC2B2AE3D;
            static const uint32 PRIME32_4 = 0x27D4EB2F;
            static const uint32 PRIME32_5 = 0x165667B1;
            
            uint32 NkHashXX32(NkStringView str, uint32 seed) NK_NOEXCEPT {
                return NkHashXX32(str.Data(), str.Length(), seed);
            }
            
            uint32 NkHashXX32(const char* str, usize length, uint32 seed) NK_NOEXCEPT {
                if (!str) return seed;
                
                const uint8* data = reinterpret_cast<const uint8*>(str);
                uint32 h32;
                
                if (length >= 16) {
                    const uint8* limit = data + length - 16;
                    uint32 v1 = seed + PRIME32_1 + PRIME32_2;
                    uint32 v2 = seed + PRIME32_2;
                    uint32 v3 = seed + 0;
                    uint32 v4 = seed - PRIME32_1;
                    
                    do {
                        uint32 val;
                        memory::NkMemCopy(&val, data, sizeof(uint32));
                        v1 += val * PRIME32_2;
                        v1 = NkMurmurRotl32(v1, 13);
                        v1 *= PRIME32_1;
                        data += 4;
                        
                        memory::NkMemCopy(&val, data, sizeof(uint32));
                        v2 += val * PRIME32_2;
                        v2 = NkMurmurRotl32(v2, 13);
                        v2 *= PRIME32_1;
                        data += 4;
                        
                        memory::NkMemCopy(&val, data, sizeof(uint32));
                        v3 += val * PRIME32_2;
                        v3 = NkMurmurRotl32(v3, 13);
                        v3 *= PRIME32_1;
                        data += 4;
                        
                        memory::NkMemCopy(&val, data, sizeof(uint32));
                        v4 += val * PRIME32_2;
                        v4 = NkMurmurRotl32(v4, 13);
                        v4 *= PRIME32_1;
                        data += 4;
                    } while (data <= limit);
                    
                    h32 = NkMurmurRotl32(v1, 1) + NkMurmurRotl32(v2, 7) +
                          NkMurmurRotl32(v3, 12) + NkMurmurRotl32(v4, 18);
                } else {
                    h32 = seed + PRIME32_5;
                }
                
                h32 += static_cast<uint32>(length);
                
                while (data + 4 <= reinterpret_cast<const uint8*>(str) + length) {
                    uint32 val;
                    memory::NkMemCopy(&val, data, sizeof(uint32));
                    h32 += val * PRIME32_3;
                    h32 = NkMurmurRotl32(h32, 17) * PRIME32_4;
                    data += 4;
                }
                
                while (data < reinterpret_cast<const uint8*>(str) + length) {
                    h32 += static_cast<uint32>(*data++) * PRIME32_5;
                    h32 = NkMurmurRotl32(h32, 11) * PRIME32_1;
                }
                
                h32 ^= h32 >> 15;
                h32 *= PRIME32_2;
                h32 ^= h32 >> 13;
                h32 *= PRIME32_3;
                h32 ^= h32 >> 16;
                
                return h32;
            }
            
            // ========================================
            // XXHASH (64-bit)
            // ========================================
            
            static const uint64 PRIME64_1 = 0x9E3779B185EBCA87ULL;
            static const uint64 PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;
            static const uint64 PRIME64_3 = 0x165667B19E3779F9ULL;
            static const uint64 PRIME64_4 = 0x85EBCA77C2B2AE63ULL;
            static const uint64 PRIME64_5 = 0x27D4EB2F165667C5ULL;
            
            uint64 NkHashXX64(NkStringView str, uint64 seed) NK_NOEXCEPT {
                return NkHashXX64(str.Data(), str.Length(), seed);
            }
            
            uint64 NkHashXX64(const char* str, usize length, uint64 seed) NK_NOEXCEPT {
                if (!str) return seed;
                
                const uint8* data = reinterpret_cast<const uint8*>(str);
                uint64 h64;
                
                if (length >= 32) {
                    const uint8* limit = data + length - 32;
                    uint64 v1 = seed + PRIME64_1 + PRIME64_2;
                    uint64 v2 = seed + PRIME64_2;
                    uint64 v3 = seed + 0;
                    uint64 v4 = seed - PRIME64_1;
                    
                    do {
                        uint64 val;
                        memory::NkMemCopy(&val, data, sizeof(uint64));
                        v1 += val * PRIME64_2;
                        v1 = NkMurmurRotl64(v1, 31);
                        v1 *= PRIME64_1;
                        data += 8;
                        
                        memory::NkMemCopy(&val, data, sizeof(uint64));
                        v2 += val * PRIME64_2;
                        v2 = NkMurmurRotl64(v2, 31);
                        v2 *= PRIME64_1;
                        data += 8;
                        
                        memory::NkMemCopy(&val, data, sizeof(uint64));
                        v3 += val * PRIME64_2;
                        v3 = NkMurmurRotl64(v3, 31);
                        v3 *= PRIME64_1;
                        data += 8;
                        
                        memory::NkMemCopy(&val, data, sizeof(uint64));
                        v4 += val * PRIME64_2;
                        v4 = NkMurmurRotl64(v4, 31);
                        v4 *= PRIME64_1;
                        data += 8;
                    } while (data <= limit);
                    
                    h64 = NkMurmurRotl64(v1, 1) + NkMurmurRotl64(v2, 7) +
                          NkMurmurRotl64(v3, 12) + NkMurmurRotl64(v4, 18);
                    
                    v1 *= PRIME64_2;
                    v1 = NkMurmurRotl64(v1, 31);
                    v1 *= PRIME64_1;
                    h64 ^= v1;
                    h64 = h64 * PRIME64_1 + PRIME64_4;
                    
                    v2 *= PRIME64_2;
                    v2 = NkMurmurRotl64(v2, 31);
                    v2 *= PRIME64_1;
                    h64 ^= v2;
                    h64 = h64 * PRIME64_1 + PRIME64_4;
                    
                    v3 *= PRIME64_2;
                    v3 = NkMurmurRotl64(v3, 31);
                    v3 *= PRIME64_1;
                    h64 ^= v3;
                    h64 = h64 * PRIME64_1 + PRIME64_4;
                    
                    v4 *= PRIME64_2;
                    v4 = NkMurmurRotl64(v4, 31);
                    v4 *= PRIME64_1;
                    h64 ^= v4;
                    h64 = h64 * PRIME64_1 + PRIME64_4;
                } else {
                    h64 = seed + PRIME64_5;
                }
                
                h64 += static_cast<uint64>(length);
                
                while (data + 8 <= reinterpret_cast<const uint8*>(str) + length) {
                    uint64 val;
                    memory::NkMemCopy(&val, data, sizeof(uint64));
                    val *= PRIME64_2;
                    val = NkMurmurRotl64(val, 31);
                    val *= PRIME64_1;
                    h64 ^= val;
                    h64 = NkMurmurRotl64(h64, 27) * PRIME64_1 + PRIME64_4;
                    data += 8;
                }
                
                if (data + 4 <= reinterpret_cast<const uint8*>(str) + length) {
                    uint32 val;
                    memory::NkMemCopy(&val, data, sizeof(uint32));
                    h64 ^= static_cast<uint64>(val) * PRIME64_1;
                    h64 = NkMurmurRotl64(h64, 23) * PRIME64_2 + PRIME64_3;
                    data += 4;
                }
                
                while (data < reinterpret_cast<const uint8*>(str) + length) {
                    h64 ^= static_cast<uint64>(*data++) * PRIME64_5;
                    h64 = NkMurmurRotl64(h64, 11) * PRIME64_1;
                }
                
                h64 ^= h64 >> 33;
                h64 *= PRIME64_2;
                h64 ^= h64 >> 29;
                h64 *= PRIME64_3;
                h64 ^= h64 >> 32;
                
                return h64;
            }
            
            // ========================================
            // JENKINS HASH (one-at-a-time)
            // ========================================
            
            uint32 NkHashJenkins32(NkStringView str) NK_NOEXCEPT {
                return NkHashJenkins32(str.Data(), str.Length());
            }
            
            uint32 NkHashJenkins32(const char* str) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 hash = 0;
                while (*str) {
                    hash += static_cast<uint32>(*str++);
                    hash += (hash << 10);
                    hash ^= (hash >> 6);
                }
                hash += (hash << 3);
                hash ^= (hash >> 11);
                hash += (hash << 15);
                return hash;
            }
            
            uint32 NkHashJenkins32(const char* str, usize length) NK_NOEXCEPT {
                if (!str) return 0;
                
                uint32 hash = 0;
                for (usize i = 0; i < length; ++i) {
                    hash += static_cast<uint32>(str[i]);
                    hash += (hash << 10);
                    hash ^= (hash >> 6);
                }
                hash += (hash << 3);
                hash ^= (hash >> 11);
                hash += (hash << 15);
                return hash;
            }
            
            // ========================================
            // LOOKUP3 HASH
            // ========================================
            
            static NKENTSEU_FORCE_INLINE void NkLookup3Mix(uint32& a, uint32& b, uint32& c) {
                a -= c; a ^= NkMurmurRotl32(c, 4); c += b;
                b -= a; b ^= NkMurmurRotl32(a, 6); a += c;
                c -= b; c ^= NkMurmurRotl32(b, 8); b += a;
                a -= c; a ^= NkMurmurRotl32(c, 16); c += b;
                b -= a; b ^= NkMurmurRotl32(a, 19); a += c;
                c -= b; c ^= NkMurmurRotl32(b, 4); b += a;
            }
            
            static NKENTSEU_FORCE_INLINE void NkLookup3Final(uint32& a, uint32& b, uint32& c) {
                c ^= b; c -= NkMurmurRotl32(b, 14);
                a ^= c; a -= NkMurmurRotl32(c, 11);
                b ^= a; b -= NkMurmurRotl32(a, 25);
                c ^= b; c -= NkMurmurRotl32(b, 16);
                a ^= c; a -= NkMurmurRotl32(c, 4);
                b ^= a; b -= NkMurmurRotl32(a, 14);
                c ^= b; c -= NkMurmurRotl32(b, 24);
            }
            
            uint32 NkHashLookup3(NkStringView str, uint32 seed) NK_NOEXCEPT {
                return NkHashLookup3(str.Data(), str.Length(), seed);
            }
            
            uint32 NkHashLookup3(const char* str, usize length, uint32 seed) NK_NOEXCEPT {
                if (!str) return seed;
                
                const uint8* data = reinterpret_cast<const uint8*>(str);
                uint32 a = seed + static_cast<uint32>(length);
                uint32 b = a;
                uint32 c = a;
                
                while (length >= 12) {
                    uint32 val;
                    
                    memory::NkMemCopy(&val, data, sizeof(uint32));
                    a += val;
                    data += 4;
                    
                    memory::NkMemCopy(&val, data, sizeof(uint32));
                    b += val;
                    data += 4;
                    
                    memory::NkMemCopy(&val, data, sizeof(uint32));
                    c += val;
                    data += 4;
                    
                    NkLookup3Mix(a, b, c);
                    length -= 12;
                }
                
                // Handle the last 11 bytes
                c += static_cast<uint32>(length);
                
                switch (length) {
                    case 11: c += static_cast<uint32>(data[10]) << 24;
                    case 10: c += static_cast<uint32>(data[9]) << 16;
                    case 9:  c += static_cast<uint32>(data[8]) << 8;
                    case 8:  b += static_cast<uint32>(data[7]) << 24;
                    case 7:  b += static_cast<uint32>(data[6]) << 16;
                    case 6:  b += static_cast<uint32>(data[5]) << 8;
                    case 5:  b += static_cast<uint32>(data[4]);
                    case 4:  a += static_cast<uint32>(data[3]) << 24;
                    case 3:  a += static_cast<uint32>(data[2]) << 16;
                    case 2:  a += static_cast<uint32>(data[1]) << 8;
                    case 1:  a += static_cast<uint32>(data[0]);
                             break;
                    case 0:  return c; // zero length strings
                }
                
                NkLookup3Final(a, b, c);
                return c;
            }
            
            // ========================================
            // SPOOKY HASH (simplified)
            // ========================================
            
            void NkHashSpooky128(NkStringView str, uint64 out[2], uint64 seed1, uint64 seed2) NK_NOEXCEPT {
                NkHashSpooky128(str.Data(), str.Length(), out, seed1, seed2);
            }
            
            void NkHashSpooky128(const char* str, usize length, uint64 out[2], uint64 seed1, uint64 seed2) NK_NOEXCEPT {
                if (!str || length == 0) {
                    out[0] = seed1;
                    out[1] = seed2;
                    return;
                }
                
                // Simplified version - use Murmur3_128 as fallback
                uint32 seed = static_cast<uint32>(seed1 ^ seed2);
                uint64 temp[2];
                NkHashMurmur3_128(str, length, temp, seed);
                
                out[0] = temp[0] ^ seed1;
                out[1] = temp[1] ^ seed2;
            }
            
            // ========================================
            // CASE-INSENSITIVE HASHES
            // ========================================
            
            uint32 NkHashIgnoreCase32(NkStringView str) NK_NOEXCEPT {
                if (!str.Data()) return 0;
                
                uint32 hash = FNV32_OFFSET_BASIS;
                for (usize i = 0; i < str.Length(); ++i) {
                    char ch = encoding::ascii::NkToLower(str[i]);
                    hash ^= static_cast<uint32>(ch);
                    hash *= FNV32_PRIME;
                }
                return hash;
            }
            
            uint64 NkHashIgnoreCase64(NkStringView str) NK_NOEXCEPT {
                if (!str.Data()) return 0;
                
                uint64 hash = FNV64_OFFSET_BASIS;
                for (usize i = 0; i < str.Length(); ++i) {
                    char ch = encoding::ascii::NkToLower(str[i]);
                    hash ^= static_cast<uint64>(ch);
                    hash *= FNV64_PRIME;
                }
                return hash;
            }
            
            uint32 NkHashFNV1aIgnoreCase32(NkStringView str) NK_NOEXCEPT {
                return NkHashIgnoreCase32(str);
            }
            
            uint64 NkHashFNV1aIgnoreCase64(NkStringView str) NK_NOEXCEPT {
                return NkHashIgnoreCase64(str);
            }
            
            // ========================================
            // UTILITY FUNCTIONS
            // ========================================
            
            uint32 NkHashFast32(NkStringView str) NK_NOEXCEPT {
                // Use DJB2 for speed
                return NkHashDJB2_32(str);
            }
            
            uint64 NkHashFast64(NkStringView str) NK_NOEXCEPT {
                // Use FNV-1a for speed
                return NkHashFNV1a64(str);
            }
            
            // ========================================
            // SHA-1 (simplified version)
            // ========================================
            
            static NKENTSEU_FORCE_INLINE uint32 NkSHA1Rotl(uint32 x, uint32 n) {
                return (x << n) | (x >> (32 - n));
            }
            
            void NkHashSHA1(NkStringView str, uint8 out[20]) NK_NOEXCEPT {
                NkHashSHA1(str.Data(), str.Length(), out);
            }
            
            void NkHashSHA1(const char* str, usize length, uint8 out[20]) NK_NOEXCEPT {
                if (!str) {
                    for (int i = 0; i < 20; ++i) out[i] = 0;
                    return;
                }
                
                // Simplified SHA-1 implementation
                // Note: This is a simplified version for compatibility
                // For production, use a proper cryptographic library
                
                uint32 h0 = 0x67452301;
                uint32 h1 = 0xEFCDAB89;
                uint32 h2 = 0x98BADCFE;
                uint32 h3 = 0x10325476;
                uint32 h4 = 0xC3D2E1F0;
                
                // Padding (simplified)
                usize totalLength = length + 9;
                usize paddedLength = ((totalLength + 63) / 64) * 64;
                uint8* padded = new uint8[paddedLength];
                
                memory::NkMemCopy(padded, str, length);
                padded[length] = 0x80;
                
                for (usize i = length + 1; i < paddedLength - 8; ++i) {
                    padded[i] = 0;
                }
                
                uint64 bitLength = length * 8;
                for (int i = 0; i < 8; ++i) {
                    padded[paddedLength - 8 + i] = static_cast<uint8>((bitLength >> (56 - i * 8)) & 0xFF);
                }
                
                // Process each 512-bit chunk
                for (usize chunk = 0; chunk < paddedLength; chunk += 64) {
                    uint32 w[80];
                    
                    // Initialize message schedule
                    for (int i = 0; i < 16; ++i) {
                        w[i] = (padded[chunk + i * 4] << 24) |
                               (padded[chunk + i * 4 + 1] << 16) |
                               (padded[chunk + i * 4 + 2] << 8) |
                               (padded[chunk + i * 4 + 3]);
                    }
                    
                    for (int i = 16; i < 80; ++i) {
                        w[i] = NkSHA1Rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
                    }
                    
                    uint32 a = h0;
                    uint32 b = h1;
                    uint32 c = h2;
                    uint32 d = h3;
                    uint32 e = h4;
                    
                    // Main loop
                    for (int i = 0; i < 80; ++i) {
                        uint32 f, k;
                        
                        if (i < 20) {
                            f = (b & c) | ((~b) & d);
                            k = 0x5A827999;
                        } else if (i < 40) {
                            f = b ^ c ^ d;
                            k = 0x6ED9EBA1;
                        } else if (i < 60) {
                            f = (b & c) | (b & d) | (c & d);
                            k = 0x8F1BBCDC;
                        } else {
                            f = b ^ c ^ d;
                            k = 0xCA62C1D6;
                        }
                        
                        uint32 temp = NkSHA1Rotl(a, 5) + f + e + k + w[i];
                        e = d;
                        d = c;
                        c = NkSHA1Rotl(b, 30);
                        b = a;
                        a = temp;
                    }
                    
                    h0 += a;
                    h1 += b;
                    h2 += c;
                    h3 += d;
                    h4 += e;
                }
                
                delete[] padded;
                
                // Output
                out[0] = static_cast<uint8>((h0 >> 24) & 0xFF);
                out[1] = static_cast<uint8>((h0 >> 16) & 0xFF);
                out[2] = static_cast<uint8>((h0 >> 8) & 0xFF);
                out[3] = static_cast<uint8>(h0 & 0xFF);
                
                out[4] = static_cast<uint8>((h1 >> 24) & 0xFF);
                out[5] = static_cast<uint8>((h1 >> 16) & 0xFF);
                out[6] = static_cast<uint8>((h1 >> 8) & 0xFF);
                out[7] = static_cast<uint8>(h1 & 0xFF);
                
                out[8] = static_cast<uint8>((h2 >> 24) & 0xFF);
                out[9] = static_cast<uint8>((h2 >> 16) & 0xFF);
                out[10] = static_cast<uint8>((h2 >> 8) & 0xFF);
                out[11] = static_cast<uint8>(h2 & 0xFF);
                
                out[12] = static_cast<uint8>((h3 >> 24) & 0xFF);
                out[13] = static_cast<uint8>((h3 >> 16) & 0xFF);
                out[14] = static_cast<uint8>((h3 >> 8) & 0xFF);
                out[15] = static_cast<uint8>(h3 & 0xFF);
                
                out[16] = static_cast<uint8>((h4 >> 24) & 0xFF);
                out[17] = static_cast<uint8>((h4 >> 16) & 0xFF);
                out[18] = static_cast<uint8>((h4 >> 8) & 0xFF);
                out[19] = static_cast<uint8>(h4 & 0xFF);
            }
            
            // ========================================
            // HASH TO HEX
            // ========================================
            
            NkString NkHashToHex32(uint32 hash) NK_NOEXCEPT {
                char buffer[9];
                snprintf(buffer, sizeof(buffer), "%08X", hash);
                return NkString(buffer);
            }
            
            NkString NkHashToHex64(uint64 hash) NK_NOEXCEPT {
                char buffer[17];
                snprintf(buffer, sizeof(buffer), "%016llX", static_cast<unsigned long long>(hash));
                return NkString(buffer);
            }
            
            NkString NkHashToHex128(const uint64 hash[2]) NK_NOEXCEPT {
                char buffer[33];
                snprintf(buffer, sizeof(buffer), "%016llX%016llX",
                        static_cast<unsigned long long>(hash[0]),
                        static_cast<unsigned long long>(hash[1]));
                return NkString(buffer);
            }
            
            // ========================================
            // HASH TO BASE64 (en utilisant notre implémentation simple)
            // ========================================
            
            NkString NkHashToBase64_32(uint32 hash) NK_NOEXCEPT {
                uint8 bytes[4];
                bytes[0] = static_cast<uint8>((hash >> 24) & 0xFF);
                bytes[1] = static_cast<uint8>((hash >> 16) & 0xFF);
                bytes[2] = static_cast<uint8>((hash >> 8) & 0xFF);
                bytes[3] = static_cast<uint8>(hash & 0xFF);
                
                return SimpleBase64Encode(bytes, 4);
            }
            
            NkString NkHashToBase64_64(uint64 hash) NK_NOEXCEPT {
                uint8 bytes[8];
                bytes[0] = static_cast<uint8>((hash >> 56) & 0xFF);
                bytes[1] = static_cast<uint8>((hash >> 48) & 0xFF);
                bytes[2] = static_cast<uint8>((hash >> 40) & 0xFF);
                bytes[3] = static_cast<uint8>((hash >> 32) & 0xFF);
                bytes[4] = static_cast<uint8>((hash >> 24) & 0xFF);
                bytes[5] = static_cast<uint8>((hash >> 16) & 0xFF);
                bytes[6] = static_cast<uint8>((hash >> 8) & 0xFF);
                bytes[7] = static_cast<uint8>(hash & 0xFF);
                
                return SimpleBase64Encode(bytes, 8);
            }
            
            // ========================================
            // MULTI-STRING HASH
            // ========================================
            
            uint64 NkHashMulti(NkStringView str1, NkStringView str2) NK_NOEXCEPT {
                uint64 hash = NkHash64(str1);
                hash = NkHashCombine(hash, NkHash64(str2));
                return hash;
            }
            
            uint64 NkHashMulti(NkStringView str1, NkStringView str2, NkStringView str3) NK_NOEXCEPT {
                uint64 hash = NkHash64(str1);
                hash = NkHashCombine(hash, NkHash64(str2));
                hash = NkHashCombine(hash, NkHash64(str3));
                return hash;
            }
            
            // ========================================
            // BENCHMARK & STATISTICS
            // ========================================
            
            void NkHashBenchmark(NkStringView testString, usize iterations) NK_NOEXCEPT {
                // Placeholder for benchmark implementation
                (void)testString;
                (void)iterations;
            }
            
            double NkHashCollisionProbability(usize numItems, usize hashSpace) NK_NOEXCEPT {
                // Birthday problem approximation
                if (numItems <= 1 || hashSpace <= 1) return 0.0;
                
                // Use simplified formula: p ≈ n²/2m for small n/m
                double n = static_cast<double>(numItems);
                double m = static_cast<double>(hashSpace);
                
                if (n > sqrt(m)) {
                    // For larger n, use more accurate formula
                    return 1.0 - exp(-n * (n - 1) / (2.0 * m));
                } else {
                    return (n * (n - 1)) / (2.0 * m);
                }
            }
            
        } // namespace string
    
} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Generated by Rihen on 2026-02-05 22:26:13
// Creation Date: 2026-02-05 22:26:13
// ============================================================