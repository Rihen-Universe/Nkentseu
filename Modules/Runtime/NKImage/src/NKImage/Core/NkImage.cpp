/**
 * @File    NkImage.cpp
 * @Brief   NkImage core + NkImageStream + NkDeflate inflate/deflate.
 *          Inflate adapté de stb_image v2.16 (public domain, Sean Barrett).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @inflate_correctness
 *  stb_image utilise inflate LSB-first (deflate RFC 1951) :
 *  - fill_bits  : bits |= byte << nbits  (accumule LSB-first)
 *  - zreceive   : v = bits & mask; bits >>= n  (consomme depuis le LSB)
 *  - fast table : index = bits & FAST_MASK  (direct, pas bit-reversed)
 *  - slow path  : k = bitrev16(bits) pour comparer avec maxcode[] pré-shifté
 *  - zBuildH    : remplit la fast table avec l'index bit-reversed du code
 *
 *  FIX zlib header (critique PNG) :
 *  - FDICT = bit 5 de FLG (in[1]), PAS de CMF (in[0])
 *  - CMF=0x78 a toujours bit5=1 (CINFO=7) → ancienne version if(in[0]&0x20)
 *    rejetait TOUS les PNG standard (0x78 0x9C, 0x78 0xDA, 0x78 0x5E, etc.)
 *  - Corrigé : if(flg & 0x20) return false  où flg = in[1]
 */
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include "NKImage/Codecs/QOI/NkQOICodec.h"
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include "NKImage/Codecs/ICO/NkICOCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include <new>
#include <cstdio>

namespace nkentseu {
    using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Helpers mémoire internes
    // ─────────────────────────────────────────────────────────────────────────────
    static inline void* nkMalloc(usize n) noexcept {
        return NkAlloc(n);
    }

    static inline void* nkCalloc(usize n, usize s) noexcept {
        return NkAllocZero(n, s);
    }

    static inline void nkFree(void* p) noexcept {
        if (p) NkFree(p);
    }

    static inline void* nkRealloc(void* p, usize o, usize n) noexcept {
        return NkRealloc(p, o, n);
    }

    static inline void nkMemcpy(void* d, const void* s, usize n) noexcept {
        NkCopy(static_cast<uint8*>(d), static_cast<const uint8*>(s), n);
    }

    static inline void nkMemset(void* d, int v, usize n) noexcept {
        NkSet(static_cast<uint8*>(d), static_cast<uint8>(v), n);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImageStream
    // ─────────────────────────────────────────────────────────────────────────────
    bool NkImageStream::Grow(usize n) noexcept {
        if (mWrSize + n <= mWrCap) return true;
        usize nc = mWrCap ? mWrCap * 2 : 4096;
        while (nc < mWrSize + n) nc *= 2;
        uint8* nb = static_cast<uint8*>(nkRealloc(mWrBuf, mWrCap, nc));
        if (!nb) return false;
        mWrBuf = nb;
        mWrCap = nc;
        return true;
    }

    uint8 NkImageStream::ReadU8() noexcept {
        if (mRdPos >= mRdSize) {
            mError = true;
            return 0;
        }
        return mRdData[mRdPos++];
    }

    uint16 NkImageStream::ReadU16BE() noexcept {
        if (mRdPos + 2 > mRdSize) {
            mError = true;
            return 0;
        }
        uint16 v = (uint16(mRdData[mRdPos]) << 8) | mRdData[mRdPos + 1];
        mRdPos += 2;
        return v;
    }

    uint16 NkImageStream::ReadU16LE() noexcept {
        if (mRdPos + 2 > mRdSize) {
            mError = true;
            return 0;
        }
        uint16 v = mRdData[mRdPos] | (uint16(mRdData[mRdPos + 1]) << 8);
        mRdPos += 2;
        return v;
    }

    uint32 NkImageStream::ReadU32BE() noexcept {
        if (mRdPos + 4 > mRdSize) {
            mError = true;
            return 0;
        }
        uint32 v = (uint32(mRdData[mRdPos]) << 24) |
                   (uint32(mRdData[mRdPos + 1]) << 16) |
                   (uint32(mRdData[mRdPos + 2]) << 8) |
                   mRdData[mRdPos + 3];
        mRdPos += 4;
        return v;
    }

    uint32 NkImageStream::ReadU32LE() noexcept {
        if (mRdPos + 4 > mRdSize) {
            mError = true;
            return 0;
        }
        uint32 v = mRdData[mRdPos] |
                   (uint32(mRdData[mRdPos + 1]) << 8) |
                   (uint32(mRdData[mRdPos + 2]) << 16) |
                   (uint32(mRdData[mRdPos + 3]) << 24);
        mRdPos += 4;
        return v;
    }

    int16 NkImageStream::ReadI16BE() noexcept {
        return int16(ReadU16BE());
    }

    int32 NkImageStream::ReadI32LE() noexcept {
        return int32(ReadU32LE());
    }

    usize NkImageStream::ReadBytes(uint8* dst, usize n) noexcept {
        if (mRdPos + n > mRdSize) {
            mError = true;
            n = mRdSize - mRdPos;
        }
        if (dst) nkMemcpy(dst, mRdData + mRdPos, n);
        mRdPos += n;
        return n;
    }

    void NkImageStream::Skip(usize n) noexcept {
        if (mRdPos + n > mRdSize) {
            mError = true;
            mRdPos = mRdSize;
        } else {
            mRdPos += n;
        }
    }

    void NkImageStream::Seek(usize p) noexcept {
        if (p > mRdSize) {
            mError = true;
            mRdPos = mRdSize;
        } else {
            mRdPos = p;
        }
    }

    bool NkImageStream::WriteU8(uint8 v) noexcept {
        if (!Grow(1)) return false;
        mWrBuf[mWrSize++] = v;
        return true;
    }

    bool NkImageStream::WriteU16BE(uint16 v) noexcept {
        uint8 b[2] = { uint8(v >> 8), uint8(v) };
        return WriteBytes(b, 2);
    }

    bool NkImageStream::WriteU16LE(uint16 v) noexcept {
        uint8 b[2] = { uint8(v), uint8(v >> 8) };
        return WriteBytes(b, 2);
    }

    bool NkImageStream::WriteU32BE(uint32 v) noexcept {
        uint8 b[4] = { uint8(v >> 24), uint8(v >> 16), uint8(v >> 8), uint8(v) };
        return WriteBytes(b, 4);
    }

    bool NkImageStream::WriteU32LE(uint32 v) noexcept {
        uint8 b[4] = { uint8(v), uint8(v >> 8), uint8(v >> 16), uint8(v >> 24) };
        return WriteBytes(b, 4);
    }

    bool NkImageStream::WriteI32LE(int32 v) noexcept {
        return WriteU32LE(uint32(v));
    }

    bool NkImageStream::WriteBytes(const uint8* src, usize n) noexcept {
        if (!Grow(n)) return false;
        nkMemcpy(mWrBuf + mWrSize, src, n);
        mWrSize += n;
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkDeflate — tables
    // ─────────────────────────────────────────────────────────────────────────────
    const uint16 NkDeflate::kLenBase[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
    };
    const uint8 NkDeflate::kLenExtra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
    };
    const uint16 NkDeflate::kDistBase[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
    };
    const uint8 NkDeflate::kDistExtra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
    };
    const uint8 NkDeflate::kCLOrder[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
    };
    const uint8 NkDeflate::kZDefLen[288] = {
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8
    };
    const uint8 NkDeflate::kZDefDist[32] = {
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkDeflate — inflate (stb_image stbi__zbuf LSB-first exact)
    // ─────────────────────────────────────────────────────────────────────────────

    // stbi__bit_reverse(v,16)
    static int32 nkBR16(int32 n) noexcept {
        n = ((n & 0xAAAA) >> 1) | ((n & 0x5555) << 1);
        n = ((n & 0xCCCC) >> 2) | ((n & 0x3333) << 2);
        n = ((n & 0xF0F0) >> 4) | ((n & 0x0F0F) << 4);
        n = ((n & 0xFF00) >> 8) | ((n & 0x00FF) << 8);
        return n;
    }

    static int32 nkBR(int32 v, int32 bits) noexcept {
        return nkBR16(v) >> (16 - bits);
    }

    // stbi__fill_bits — accumule depuis le LSB
    void NkDeflate::zFill(ZBuf& z) noexcept {
        while (z.nbits <= 24) {
            if (z.pos >= z.size) {
                z.err = true;
                return;
            }
            z.bits |= uint32(z.data[z.pos++]) << z.nbits;
            z.nbits += 8;
        }
    }

    // stbi__zreceive
    uint32 NkDeflate::zBits(ZBuf& z, int32 n) noexcept {
        if (!n) return 0;
        if (z.nbits < n) zFill(z);
        uint32 v = z.bits & ((1u << n) - 1);
        z.bits >>= n;
        z.nbits -= n;
        return v;
    }

    // stbi__zbuild_huffman — construit la LUT fast avec bit-reverse des codes
    bool NkDeflate::zBuildH(ZHuff& h, const uint8* szList, int32 num) noexcept {
        int32 i, k = 0, code, nc[16], sizes[17];
        nkMemset(sizes, 0, sizeof(sizes));
        nkMemset(h.fast, 0, sizeof(h.fast));
        for (i = 0; i < num; ++i) {
            if (szList[i]) ++sizes[szList[i]];
        }
        sizes[0] = 0;
        for (i = 1; i < 16; ++i) {
            if (sizes[i] > (1 << i)) {
                nkMemset(&h, 0, sizeof(h));
                return false;
            }
        }
        code = 0;
        for (i = 1; i < 16; ++i) {
            nc[i] = code;
            h.firstcode[i] = uint16(code);
            h.firstsym[i] = uint16(k);
            code += sizes[i];
            if (sizes[i] && code - 1 >= (1 << i)) {
                nkMemset(&h, 0, sizeof(h));
                return false;
            }
            h.maxcode[i] = code << (16 - i); // pré-shifté
            code <<= 1;
            k += sizes[i];
        }
        h.maxcode[16] = 0x10000;
        for (i = 0; i < num; ++i) {
            int32 s = szList[i];
            if (!s) continue;
            int32 c = nc[s] - h.firstcode[s] + h.firstsym[s];
            uint16 fv = uint16((s << 9) | i);
            h.sizes[c] = uint8(s);
            h.values[c] = uint16(i);
            if (s <= ZHuff::FAST) {
                // remplit toutes les entrées de la fast table avec ce préfixe bit-reversed
                int32 j = nkBR(nc[s], s);
                while (j < (1 << ZHuff::FAST)) {
                    h.fast[j] = fv;
                    j += (1 << s);
                }
            }
            ++nc[s];
        }
        return true;
    }

    // stbi__zhuffman_decode
    int32 NkDeflate::zDecode(ZBuf& z, const ZHuff& h) noexcept {
        if (z.nbits < 16) zFill(z);
        // fast path : index direct (bits LSB-first correspondent à l'index bit-reversed du code)
        uint16 fv = h.fast[z.bits & ((1 << ZHuff::FAST) - 1)];
        if (fv) {
            int32 s = fv >> 9;
            z.bits >>= s;
            z.nbits -= s;
            return fv & 511;
        }
        // slow path : bit-reverse pour trouver la longueur
        int32 k = nkBR16(int32(z.bits & 0xFFFF));
        int32 s;
        for (s = ZHuff::FAST + 1; ; ++s) {
            if (k < h.maxcode[s]) break;
        }
        if (s == 16) {
            z.err = true;
            return -1;
        }
        {
            int32 b = (k >> (16 - s)) - h.firstcode[s] + h.firstsym[s];
            if (b < 0 || b >= 288 || h.sizes[b] != s) {
                z.err = true;
                return -1;
            }
            z.bits >>= s;
            z.nbits -= s;
            return h.values[b];
        }
    }

    bool NkDeflate::zHuffBlock(ZBuf& z, const ZHuff& zl, const ZHuff& zd) noexcept {
        for (;;) {
            int32 sym = zDecode(z, zl);
            if (z.err || sym < 0) return false;
            if (sym < 256) {
                if (z.outPos >= z.outCap) {
                    z.err = true;
                    return false;
                }
                z.out[z.outPos++] = uint8(sym);
            } else if (sym == 256) {
                return true;
            } else {
                int32 li = sym - 257;
                if (li < 0 || li >= 29) {
                    z.err = true;
                    return false;
                }
                uint32 len = kLenBase[li] + zBits(z, kLenExtra[li]);
                int32 ds = zDecode(z, zd);
                if (z.err || ds < 0 || ds >= 30) {
                    z.err = true;
                    return false;
                }
                uint32 dist = kDistBase[ds] + zBits(z, kDistExtra[ds]);
                if (z.outPos < dist) {
                    z.err = true;
                    return false;
                }
                if (z.outPos + len > z.outCap) {
                    z.err = true;
                    return false;
                }
                for (uint32 i = 0; i < len; ++i) {
                    z.out[z.outPos] = z.out[z.outPos - dist];
                    ++z.outPos;
                }
            }
        }
    }

    bool NkDeflate::zStored(ZBuf& z) noexcept {
        // Aligne sur octet : jette les bits partiels
        if (z.nbits & 7) zBits(z, z.nbits & 7);
        // Si zFill a trop lu, on revient en arrière dans z.pos
        {
            int32 extra = z.nbits / 8;
            if (int32(z.pos) >= extra) z.pos -= extra;
        }
        z.bits = 0;
        z.nbits = 0;
        if (z.pos + 4 > z.size) {
            z.err = true;
            return false;
        }
        uint16 len  = z.data[z.pos] | (uint16(z.data[z.pos + 1]) << 8);
        z.pos += 2;
        uint16 nlen = z.data[z.pos] | (uint16(z.data[z.pos + 1]) << 8);
        z.pos += 2;
        if (uint16(len ^ nlen) != 0xFFFF) {
            z.err = true;
            return false;
        }
        if (z.pos + len > z.size) {
            z.err = true;
            return false;
        }
        if (z.outPos + len > z.outCap) {
            z.err = true;
            return false;
        }
        nkMemcpy(z.out + z.outPos, z.data + z.pos, len);
        z.pos += len;
        z.outPos += len;
        return true;
    }

    bool NkDeflate::zFixed(ZBuf& z) noexcept {
        ZHuff zl, zd;
        if (!zBuildH(zl, kZDefLen, 288)) {
            z.err = true;
            return false;
        }
        if (!zBuildH(zd, kZDefDist, 32)) {
            z.err = true;
            return false;
        }
        return zHuffBlock(z, zl, zd);
    }

    bool NkDeflate::zDynamic(ZBuf& z) noexcept {
        int32 hlit  = int32(zBits(z, 5)) + 257;
        int32 hdist = int32(zBits(z, 5)) + 1;
        int32 hclen = int32(zBits(z, 4)) + 4;
        if (z.err) return false;
        uint8 clen[19] = {};
        for (int32 i = 0; i < hclen; ++i) {
            clen[kCLOrder[i]] = uint8(zBits(z, 3));
        }
        ZHuff zc;
        if (!zBuildH(zc, clen, 19)) {
            z.err = true;
            return false;
        }
        uint8 lens[320] = {};
        int32 n = 0, total = hlit + hdist;
        while (n < total && !z.err) {
            int32 c = zDecode(z, zc);
            if (z.err || c < 0) return false;
            if (c < 16) {
                lens[n++] = uint8(c);
            } else if (c == 16) {
                if (n == 0) {
                    z.err = true;
                    return false;
                }
                int32 r = int32(zBits(z, 2)) + 3;
                uint8 p = lens[n - 1];
                while (r-- && n < total) lens[n++] = p;
            } else if (c == 17) {
                int32 r = int32(zBits(z, 3)) + 3;
                while (r-- && n < total) lens[n++] = 0;
            } else {
                int32 r = int32(zBits(z, 7)) + 11;
                while (r-- && n < total) lens[n++] = 0;
            }
        }
        ZHuff zl, zd;
        if (!zBuildH(zl, lens, hlit)) {
            z.err = true;
            return false;
        }
        if (!zBuildH(zd, lens + hlit, hdist)) {
            z.err = true;
            return false;
        }
        return zHuffBlock(z, zl, zd);
    }

    bool NkDeflate::zBlock(ZBuf& z, bool& last) noexcept {
        last = bool(zBits(z, 1));
        uint32 type = zBits(z, 2);
        if (z.err) return false;
        switch (type) {
            case 0: return zStored(z);
            case 1: return zFixed(z);
            case 2: return zDynamic(z);
        }
        z.err = true;
        return false;
    }

    bool NkDeflate::zInflate(ZBuf& z, bool hdr) noexcept {
        if (hdr) {
            if (z.size < 2) {
                z.err = true;
                return false;
            }
            uint8 cmf = z.data[z.pos++];
            uint8 flg = z.data[z.pos++];
            if ((cmf & 0x0F) != 8) {
                z.err = true;
                return false;
            }
            if (((uint16(cmf) << 8) | flg) % 31 != 0) {
                z.err = true;
                return false;
            }
            // *** FIX CRITIQUE ***
            // FLG = in[1]. Bit 5 de FLG = FDICT (preset dictionary).
            // ANCIEN: if(in[0] & 0x20) → lisait CMF, toujours vrai pour CMF=0x78 !
            // CORRECT: if(flg & 0x20) → lit FLG
            if (flg & 0x20) {
                z.err = true;
                return false;
            }
        }
        bool last = false;
        while (!last && !z.err) {
            if (!zBlock(z, last)) return false;
        }
        return !z.err;
    }

    bool NkDeflate::Decompress(const uint8* in, usize inSz, uint8* out, usize outCap, usize& w) noexcept {
        w = 0;
        if (!in || !out || inSz < 2) return false;
        ZBuf z{};
        z.data = in;
        z.size = inSz;
        z.out = out;
        z.outCap = outCap;
        if (!zInflate(z, true)) return false;
        w = z.outPos;
        return true;
    }

    bool NkDeflate::DecompressRaw(const uint8* in, usize inSz, uint8* out, usize outCap, usize& w) noexcept {
        w = 0;
        if (!in || !out) return false;
        ZBuf z{};
        z.data = in;
        z.size = inSz;
        z.out = out;
        z.outCap = outCap;
        if (!zInflate(z, false)) return false;
        w = z.outPos;
        return true;
    }

    uint32 NkDeflate::Adler32(const uint8* d, usize s, uint32 p) noexcept {
        uint32 s1 = p & 0xFFFF;
        uint32 s2 = (p >> 16) & 0xFFFF;
        for (usize i = 0; i < s; ++i) {
            s1 = (s1 + d[i]) % 65521;
            s2 = (s2 + s1) % 65521;
        }
        return (s2 << 16) | s1;
    }

    bool NkDeflate::Compress(const uint8* in, usize inSz, uint8*& outData, usize& outSz, int32) noexcept {
        NkImageStream s;
        // Header zlib valide : CMF=0x78, FLG tel que (CMF*256+FLG)%31==0, FDICT=0
        uint8 cmf = 0x78;
        uint8 flg = uint8(31 - (uint16(cmf) * 256 % 31));
        flg &= ~0x20u;
        if (((uint16(cmf) << 8) | flg) % 31 != 0) flg = 0;
        s.WriteU8(cmf);
        s.WriteU8(flg);
        // Blocs non-compressés (BTYPE=00)
        usize rem = inSz;
        const uint8* src = in;
        while (rem > 0) {
            uint16 bl = uint16(rem > 65535 ? 65535 : rem);
            uint16 nl = uint16(~bl);
            bool last = (bl == rem);
            s.WriteU8(last ? 0x01 : 0x00);
            s.WriteU16LE(bl);
            s.WriteU16LE(nl);
            s.WriteBytes(src, bl);
            src += bl;
            rem -= bl;
        }
        s.WriteU32BE(Adler32(in, inSz, 1));
        return s.TakeBuffer(outData, outSz);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImage
    // ─────────────────────────────────────────────────────────────────────────────
    NkImage* NkImage::Alloc(int32 w, int32 h, NkImagePixelFormat fmt) noexcept {
        if (w <= 0 || h <= 0) return nullptr;
        const int32 bpp = BytesPerPixelOf(fmt);
        if (!bpp) return nullptr;
        NkImage* img = static_cast<NkImage*>(nkMalloc(sizeof(NkImage)));
        if (!img) return nullptr;
        new(img) NkImage();
        img->mStride = (w * bpp + 3) & ~3;
        img->mPixels = static_cast<uint8*>(nkCalloc(usize(img->mStride) * h, 1));
        if (!img->mPixels) {
            nkFree(img);
            return nullptr;
        }
        img->mWidth  = w;
        img->mHeight = h;
        img->mFormat = fmt;
        img->mOwning = true;
        return img;
    }

    NkImage* NkImage::Wrap(uint8* px, int32 w, int32 h, NkImagePixelFormat fmt, int32 st) noexcept {
        NkImage* img = static_cast<NkImage*>(nkMalloc(sizeof(NkImage)));
        if (!img) return nullptr;
        new(img) NkImage();
        img->mPixels = px;
        img->mWidth  = w;
        img->mHeight = h;
        img->mFormat = fmt;
        img->mStride = (st > 0) ? st : w * BytesPerPixelOf(fmt);
        img->mOwning = false;
        return img;
    }

    NkImage* NkImage::ConvertToTexture(const NkImage& hdrImage, float exposure, float gamma) noexcept {
        return NkHDRCodec::ConvertToTexture(hdrImage, exposure, gamma);
    }

    // bool NkImage::ConvertToTextureData(const NkImage& hdrImage, NkTextureData& outData, float exposure, float gamma) noexcept {
    //     return NkHDRCodec::ConvertToTextureData(hdrImage, outData, exposure, gamma);
    // }

    NkImage::~NkImage() noexcept {
        if (mOwning && mPixels) nkFree(mPixels);
    }

    void NkImage::Free() noexcept {
        if (mOwning && mPixels) nkFree(mPixels);
        mPixels = nullptr;
        nkFree(this);
    }

    NkImageFormat NkImage::DetectFormat(const uint8* d, usize sz) noexcept {
        if (sz < 4) return NkImageFormat::NK_UNKNOWN;
        if (sz >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G')
            return NkImageFormat::NK_PNG;
        if (sz >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF)
            return NkImageFormat::NK_JPEG;
        if (sz >= 2 && d[0] == 'B' && d[1] == 'M')
            return NkImageFormat::NK_BMP;
        if (sz >= 4 && d[0] == 'q' && d[1] == 'o' && d[2] == 'i' && d[3] == 'f')
            return NkImageFormat::NK_QOI;
        if (sz >= 4 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F' && d[3] == '8')
            return NkImageFormat::NK_GIF;
        if (sz >= 4 && d[0] == 0 && d[1] == 0 && (d[2] == 1 || d[2] == 2) && d[3] == 0)
            return NkImageFormat::NK_ICO;
        if (sz >= 10 && d[0] == '#' && d[1] == '?')
            return NkImageFormat::NK_HDR;
        if (sz >= 2 && d[0] == 'P' && d[1] >= '1' && d[1] <= '6') {
            if (d[1] == '1' || d[1] == '4') return NkImageFormat::NK_PBM;
            if (d[1] == '2' || d[1] == '5') return NkImageFormat::NK_PGM;
            return NkImageFormat::NK_PPM;
        }
        if (sz >= 18) {
            uint8 it = d[2];
            if (it == 0 || it == 1 || it == 2 || it == 3 || it == 9 || it == 10 || it == 11)
                return NkImageFormat::NK_TGA;
        }
        return NkImageFormat::NK_UNKNOWN;
    }

    NkImage* NkImage::Dispatch(const uint8* d, usize sz, int32 ch, NkImageFormat fmt) noexcept {
        NkImage* img = nullptr;
        switch (fmt) {
            case NkImageFormat::NK_PNG:  img = NkPNGCodec::Decode(d, sz); break;
            case NkImageFormat::NK_JPEG: img = NkJPEGCodec::Decode(d, sz); break;
            case NkImageFormat::NK_BMP:  img = NkBMPCodec::Decode(d, sz); break;
            case NkImageFormat::NK_TGA:  img = NkTGACodec::Decode(d, sz); break;
            case NkImageFormat::NK_HDR:  img = NkHDRCodec::Decode(d, sz); break;
            case NkImageFormat::NK_PPM:
            case NkImageFormat::NK_PGM:
            case NkImageFormat::NK_PBM:  img = NkPPMCodec::Decode(d, sz); break;
            case NkImageFormat::NK_QOI:  img = NkQOICodec::Decode(d, sz); break;
            case NkImageFormat::NK_GIF:  img = NkGIFCodec::Decode(d, sz); break;
            case NkImageFormat::NK_ICO:  img = NkICOCodec::Decode(d, sz); break;
            default: return nullptr;
        }
        if (!img) return nullptr;
        if (ch > 0 && ch != img->Channels()) {
            NkImagePixelFormat tgt =
                ch == 1 ? NkImagePixelFormat::NK_GRAY8 :
                ch == 2 ? NkImagePixelFormat::NK_GRAY_A16 :
                ch == 3 ? NkImagePixelFormat::NK_RGB24 :
                          NkImagePixelFormat::NK_RGBA32;
            NkImage* c = img->Convert(tgt);
            img->Free();
            return c;
        }
        return img;
    }

    NkImage* NkImage::LoadFromMemory(const uint8* data, usize size, int32 ch) noexcept {
        if (!data || size < 4) return nullptr;
        NkImageFormat fmt = DetectFormat(data, size);
        if (fmt == NkImageFormat::NK_UNKNOWN) return nullptr;
        NkImage* img = Dispatch(data, size, ch, fmt);
        if (img) img->mSrcFmt = fmt;
        return img;
    }

    NkImage* NkImage::Load(const char* path, int32 ch) noexcept {
        if (!path) return nullptr;
        FILE* f = ::fopen(path, "rb");
        if (!f) return nullptr;
        ::fseek(f, 0, SEEK_END);
        long sz = ::ftell(f);
        ::fseek(f, 0, SEEK_SET);
        if (sz <= 0) {
            ::fclose(f);
            return nullptr;
        }
        uint8* buf = static_cast<uint8*>(nkMalloc(usize(sz)));
        if (!buf) {
            ::fclose(f);
            return nullptr;
        }
        usize rd = ::fread(buf, 1, usize(sz), f);
        ::fclose(f);
        NkImage* img = nullptr;
        if (rd == usize(sz)) img = LoadFromMemory(buf, rd, ch);
        nkFree(buf);
        return img;
    }

    static bool nkWF(const char* p, const uint8* d, usize s) noexcept {
        FILE* f = ::fopen(p, "wb");
        if (!f) return false;
        bool ok = (::fwrite(d, 1, s, f) == s);
        ::fclose(f);
        return ok;
    }

    static const char* nkExt(const char* p) noexcept {
        const char* e = nullptr;
        for (const char* q = p; *q; ++q) {
            if (*q == '.') e = q;
        }
        return e ? e + 1 : "";
    }

    static bool nkEq(const char* a, const char* b) noexcept {
        while (*a && *b) {
            if (((*a) | 32) != ((*b) | 32)) return false;
            ++a; ++b;
        }
        return *a == *b;
    }

    bool NkImage::Save(const char* p, int32 q) const noexcept {
        const char* e = nkExt(p);
        if (nkEq(e, "png")) return SavePNG(p);
        if (nkEq(e, "jpg") || nkEq(e, "jpeg")) return SaveJPEG(p, q);
        if (nkEq(e, "bmp")) return SaveBMP(p);
        if (nkEq(e, "tga")) return SaveTGA(p);
        if (nkEq(e, "ppm") || nkEq(e, "pgm")) return SavePPM(p);
        if (nkEq(e, "hdr")) return SaveHDR(p);
        if (nkEq(e, "qoi")) return SaveQOI(p);
        return false;
    }

    bool NkImage::SavePNG(const char* p) const noexcept {
        uint8* d = nullptr;
        usize s = 0;
        if (!EncodePNG(d, s)) return false;
        bool ok = nkWF(p, d, s);
        nkFree(d);
        return ok;
    }

    bool NkImage::SaveJPEG(const char* p, int32 q) const noexcept {
        uint8* d = nullptr;
        usize s = 0;
        if (!EncodeJPEG(d, s, q)) return false;
        bool ok = nkWF(p, d, s);
        nkFree(d);
        return ok;
    }

    bool NkImage::SaveBMP(const char* p) const noexcept {
        uint8* d = nullptr;
        usize s = 0;
        if (!EncodeBMP(d, s)) return false;
        bool ok = nkWF(p, d, s);
        nkFree(d);
        return ok;
    }

    bool NkImage::SaveTGA(const char* p) const noexcept {
        uint8* d = nullptr;
        usize s = 0;
        if (!EncodeTGA(d, s)) return false;
        bool ok = nkWF(p, d, s);
        nkFree(d);
        return ok;
    }

    bool NkImage::SavePPM(const char* p) const noexcept {
        return NkPPMCodec::Encode(*this, p);
    }

    bool NkImage::SaveHDR(const char* p) const noexcept {
        return NkHDRCodec::Encode(*this, p);
    }

    bool NkImage::SaveQOI(const char* p) const noexcept {
        uint8* d = nullptr;
        usize s = 0;
        if (!EncodeQOI(d, s)) return false;
        bool ok = nkWF(p, d, s);
        nkFree(d);
        return ok;
    }

    bool NkImage::SaveGIF(const char*) const noexcept {
        return false; // not implemented
    }

    bool NkImage::SaveWebP(const char*, bool, int32) const noexcept {
        return false;
    }

    bool NkImage::SaveSVG(const char*) const noexcept {
        return false;
    }

    bool NkImage::EncodePNG(uint8*& d, usize& s) const noexcept {
        return NkPNGCodec::Encode(*this, d, s);
    }

    bool NkImage::EncodeJPEG(uint8*& d, usize& s, int32 q) const noexcept {
        return NkJPEGCodec::Encode(*this, d, s, q);
    }

    bool NkImage::EncodeBMP(uint8*& d, usize& s) const noexcept {
        return NkBMPCodec::Encode(*this, d, s);
    }

    bool NkImage::EncodeTGA(uint8*& d, usize& s) const noexcept {
        return NkTGACodec::Encode(*this, d, s);
    }

    bool NkImage::EncodeQOI(uint8*& d, usize& s) const noexcept {
        return NkQOICodec::Encode(*this, d, s);
    }

    static uint8 nkY(uint8 r, uint8 g, uint8 b) noexcept {
        return uint8((uint32(r) * 77 + g * 150 + b * 29) >> 8);
    }

    uint8* NkImage::ConvertChannels(const uint8* src, int32 w, int32 h, int32 sc, int32 dc, int32 ss) noexcept {
        const int32 ds = (w * dc + 3) & ~3;
        uint8* dst = static_cast<uint8*>(nkCalloc(usize(ds) * h, 1));
        if (!dst) return nullptr;
        for (int32 y = 0; y < h; ++y) {
            const uint8* sr = src + usize(y) * ss;
            uint8* dr = dst + usize(y) * ds;
            for (int32 x = 0; x < w; ++x) {
                const uint8* sp = sr + x * sc;
                uint8* dp = dr + x * dc;
                uint8 r = sp[0];
                uint8 g = (sc > 1) ? sp[1] : r;
                uint8 b = (sc > 2) ? sp[2] : r;
                uint8 a = (sc > 3) ? sp[3] : 255;
                switch (dc) {
                    case 1: dp[0] = nkY(r, g, b); break;
                    case 2: dp[0] = nkY(r, g, b); dp[1] = a; break;
                    case 3: dp[0] = r; dp[1] = g; dp[2] = b; break;
                    case 4: dp[0] = r; dp[1] = g; dp[2] = b; dp[3] = a; break;
                }
            }
        }
        return dst;
    }

    NkImage* NkImage::Convert(NkImagePixelFormat nf) const noexcept {
        if (!IsValid()) return nullptr;
        if (nf == mFormat) {
            NkImage* c = Alloc(mWidth, mHeight, mFormat);
            if (c) nkMemcpy(c->mPixels, mPixels, TotalBytes());
            return c;
        }
        const int32 dc = ChannelsOf(nf);
        if (!dc) return nullptr;
        uint8* pix = ConvertChannels(mPixels, mWidth, mHeight, Channels(), dc, mStride);
        if (!pix) return nullptr;
        NkImage* img = static_cast<NkImage*>(nkMalloc(sizeof(NkImage)));
        if (!img) {
            nkFree(pix);
            return nullptr;
        }
        new(img) NkImage();
        img->mPixels = pix;
        img->mWidth  = mWidth;
        img->mHeight = mHeight;
        img->mFormat = nf;
        img->mStride = (mWidth * BytesPerPixelOf(nf) + 3) & ~3;
        img->mOwning = true;
        return img;
    }

    void NkImage::FlipVertical() noexcept {
        if (!IsValid()) return;
        uint8* tmp = static_cast<uint8*>(nkMalloc(mStride));
        if (!tmp) return;
        for (int32 y = 0; y < mHeight / 2; ++y) {
            nkMemcpy(tmp, RowPtr(y), mStride);
            nkMemcpy(RowPtr(y), RowPtr(mHeight - 1 - y), mStride);
            nkMemcpy(RowPtr(mHeight - 1 - y), tmp, mStride);
        }
        nkFree(tmp);
    }

    void NkImage::FlipHorizontal() noexcept {
        if (!IsValid()) return;
        const int32 bpp = BytesPP();
        for (int32 y = 0; y < mHeight; ++y) {
            uint8* row = RowPtr(y);
            for (int32 x = 0; x < mWidth / 2; ++x) {
                uint8* a = row + x * bpp;
                uint8* b = row + (mWidth - 1 - x) * bpp;
                for (int32 i = 0; i < bpp; ++i) {
                    uint8 t = a[i];
                    a[i] = b[i];
                    b[i] = t;
                }
            }
        }
    }

    void NkImage::PremultiplyAlpha() noexcept {
        if (!IsValid() || mFormat != NkImagePixelFormat::NK_RGBA32) return;
        for (int32 y = 0; y < mHeight; ++y) {
            uint8* p = RowPtr(y);
            for (int32 x = 0; x < mWidth; ++x, p += 4) {
                uint32 a = p[3];
                p[0] = uint8((p[0] * a + 127) / 255);
                p[1] = uint8((p[1] * a + 127) / 255);
                p[2] = uint8((p[2] * a + 127) / 255);
            }
        }
    }

    void NkImage::Blit(const NkImage& src, int32 dx, int32 dy) noexcept {
        if (!IsValid() || !src.IsValid() || mFormat != src.mFormat) return;
        int32 sx = 0, sy = 0, w = src.mWidth, h = src.mHeight;
        if (dx < 0) { sx -= dx; w += dx; dx = 0; }
        if (dy < 0) { sy -= dy; h += dy; dy = 0; }
        if (dx + w > mWidth)  w = mWidth - dx;
        if (dy + h > mHeight) h = mHeight - dy;
        if (w <= 0 || h <= 0) return;
        const int32 bpp = BytesPP(), rb = w * bpp;
        for (int32 row = 0; row < h; ++row) {
            nkMemcpy(RowPtr(dy + row) + dx * bpp,
                     src.RowPtr(sy + row) + sx * bpp,
                     rb);
        }
    }

    NkImage* NkImage::Crop(int32 x, int32 y, int32 w, int32 h) const noexcept {
        if (!IsValid() || x < 0 || y < 0 || x + w > mWidth || y + h > mHeight || w <= 0 || h <= 0)
            return nullptr;
        NkImage* dst = Alloc(w, h, mFormat);
        if (!dst) return nullptr;
        for (int32 row = 0; row < h; ++row) {
            nkMemcpy(dst->RowPtr(row),
                     RowPtr(y + row) + x * BytesPP(),
                     w * BytesPP());
        }
        return dst;
    }

    NkImage* NkImage::Resize(int32 nw, int32 nh, NkResizeFilter f) const noexcept {
        if (!IsValid() || nw <= 0 || nh <= 0) return nullptr;
        NkImage* dst = Alloc(nw, nh, mFormat);
        if (!dst) return nullptr;
        const int32 bpp = BytesPP();
        const float32 xs = float32(mWidth) / nw;
        const float32 ys = float32(mHeight) / nh;
        if (f == NkResizeFilter::NK_NEAREST) {
            for (int32 y = 0; y < nh; ++y) {
                const int32 sy = int32(y * ys);
                for (int32 x = 0; x < nw; ++x) {
                    nkMemcpy(dst->RowPtr(y) + x * bpp,
                             RowPtr(sy) + int32(x * xs) * bpp,
                             bpp);
                }
            }
            return dst;
        }
        for (int32 y = 0; y < nh; ++y) {
            const float32 fy = (y + 0.5f) * ys - 0.5f;
            const int32 y0 = int32(fy);
            const float32 yt = fy - y0;
            const int32 y1 = (y0 + 1 < mHeight) ? y0 + 1 : y0;
            for (int32 x = 0; x < nw; ++x) {
                const float32 fx = (x + 0.5f) * xs - 0.5f;
                const int32 x0 = int32(fx);
                const float32 xt = fx - x0;
                const int32 x1 = (x0 + 1 < mWidth) ? x0 + 1 : x0;
                uint8* dp = dst->RowPtr(y) + x * bpp;
                for (int32 c = 0; c < bpp; ++c) {
                    float32 p00 = RowPtr(y0)[x0 * bpp + c];
                    float32 p10 = RowPtr(y0)[x1 * bpp + c];
                    float32 p01 = RowPtr(y1)[x0 * bpp + c];
                    float32 p11 = RowPtr(y1)[x1 * bpp + c];
                    float32 v = p00 * (1 - xt) * (1 - yt) +
                                 p10 * xt * (1 - yt) +
                                 p01 * (1 - xt) * yt +
                                 p11 * xt * yt;
                    dp[c] = uint8(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f));
                }
            }
        }
        return dst;
    }

} // namespace nkentseu