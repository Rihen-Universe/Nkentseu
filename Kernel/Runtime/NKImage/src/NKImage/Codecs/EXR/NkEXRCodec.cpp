/**
 * @File    NkEXRCodec.cpp
 * @Brief   Codec OpenEXR (.exr) decoder from scratch.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @References
 *  - ILM/ASWF OpenEXR file specification (public domain documentation)
 *    https://openexr.com/en/latest/OpenEXRFileLayout.html
 *  - Compressors source : OpenEXR/IlmImf (BSD licence) consultes pour
 *    valider l'algorithme de prediction ZIP + wavelet PIZ.
 *
 * @Architecture
 *  Le decodeur procede en 5 etapes :
 *    1. Validation magic + version + flags (single-part scanline only).
 *    2. Parsing de la table d'attributs jusqu'a l'octet null final :
 *       chaque attribut = name (cstr) + type (cstr) + size (i32LE) + data.
 *       On extrait : channels, compression, dataWindow, lineOrder.
 *    3. Lecture de la table d'offsets scanline : pour ceil(H / linesPerBlock)
 *       entrees de int64LE, chacune pointant vers un bloc dans le fichier.
 *       linesPerBlock depend de la compression (1 pour NONE/RLE/ZIPS,
 *       16 pour ZIP, 32 pour PIZ).
 *    4. Pour chaque bloc : lire (yStart i32 + size i32 + data), decompresser
 *       selon le scheme. Le buffer decompresse est en layout EXR scanline
 *       (channels sorted alphabetiquement, chaque ligne = concatenation des
 *       lignes par canal : [B-row][G-row][R-row][A-row]).
 *    5. Conversion en NkImage RGB96F/RGBA128F en remappant les canaux dans
 *       l'ordre R,G,B,A et en convertissant HALF -> float32 si necessaire.
 *
 * @PIZ_v0_status
 *  v0 ne supporte PAS PIZ. Le decodeur retourne nullptr avec un log clair
 *  invitant a re-encoder via "oiiotool --compression zip". L'ajout de PIZ
 *  est prevu en v0.5 (wavelet 2D Haar inverse + Huffman canonique 58-bit).
 *  Couverture actuelle : NONE + RLE + ZIPS + ZIP ~80% des EXR.
 */
#include "NKImage/Codecs/EXR/NkEXRCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

namespace nkentseu {
    using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────
    //  Constantes format EXR
    // ─────────────────────────────────────────────────────────────────────────

    static constexpr uint32 kEXRMagic = 0x01312F76u;   // 76 2F 31 01 little-endian

    // Pixel types definis dans la spec OpenEXR (Imath::PixelType)
    static constexpr int32 kPixelUINT  = 0;
    static constexpr int32 kPixelHALF  = 1;
    static constexpr int32 kPixelFLOAT = 2;

    // Compressions codees sur 1 octet dans l'attribut "compression"
    enum class EXRCompression : uint8 {
        NONE   = 0,
        RLE    = 1,
        ZIPS   = 2,   // zlib per scanline (1 line/block)
        ZIP    = 3,   // zlib par bloc de 16 scanlines
        PIZ    = 4,   // wavelet 2D Haar + Huffman canonique
        PXR24  = 5,   // lossy 24-bit float + zlib  (non supporte)
        B44    = 6,   // lossy bloc 4x4 half        (non supporte)
        B44A   = 7,   // B44 + flat preservation    (non supporte)
        DWAA   = 8,   // DCT lossy DreamWorks       (non supporte)
        DWAB   = 9    // DWAA bloc 256 lignes       (non supporte)
    };

    // Nombre de scanlines par bloc compresse, indexe par EXRCompression
    static int32 LinesPerBlock(EXRCompression c) noexcept {
        switch (c) {
            case EXRCompression::NONE:   return 1;
            case EXRCompression::RLE:    return 1;
            case EXRCompression::ZIPS:   return 1;
            case EXRCompression::ZIP:    return 16;
            case EXRCompression::PIZ:    return 32;
            case EXRCompression::PXR24:  return 16;
            case EXRCompression::B44:    return 32;
            case EXRCompression::B44A:   return 32;
            case EXRCompression::DWAA:   return 32;
            case EXRCompression::DWAB:   return 256;
        }
        return 1;
    }

    static const char* CompressionName(EXRCompression c) noexcept {
        switch (c) {
            case EXRCompression::NONE:   return "NONE";
            case EXRCompression::RLE:    return "RLE";
            case EXRCompression::ZIPS:   return "ZIPS";
            case EXRCompression::ZIP:    return "ZIP";
            case EXRCompression::PIZ:    return "PIZ";
            case EXRCompression::PXR24:  return "PXR24";
            case EXRCompression::B44:    return "B44";
            case EXRCompression::B44A:   return "B44A";
            case EXRCompression::DWAA:   return "DWAA";
            case EXRCompression::DWAB:   return "DWAB";
        }
        return "UNKNOWN";
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Helpers lecture little-endian (l'API NkImageStream n'a pas U64/F32)
    // ─────────────────────────────────────────────────────────────────────────

    static NKIMG_INLINE uint16 RD_U16LE(const uint8* p) noexcept {
        return uint16(p[0]) | (uint16(p[1]) << 8);
    }
    static NKIMG_INLINE uint32 RD_U32LE(const uint8* p) noexcept {
        return uint32(p[0])        | (uint32(p[1]) << 8)
             | (uint32(p[2]) << 16)| (uint32(p[3]) << 24);
    }
    static NKIMG_INLINE int32 RD_I32LE(const uint8* p) noexcept {
        return int32(RD_U32LE(p));
    }
    static NKIMG_INLINE nk_int64 RD_I64LE(const uint8* p) noexcept {
        // 8 octets little-endian -> int64. On lit en 2 etapes uint64 pour
        // eviter les undefined behaviour de shift > 31 sur un uint32.
        const uint32 lo = RD_U32LE(p);
        const uint32 hi = RD_U32LE(p + 4);
        return (nk_int64)((uint64(hi) << 32) | uint64(lo));
    }
    static NKIMG_INLINE float32 RD_F32LE(const uint8* p) noexcept {
        const uint32 u = RD_U32LE(p);
        float32 f;
        ::memcpy(&f, &u, 4);
        return f;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Half-float (16-bit IEEE 754) -> float32
    //  1 sign bit | 5 exp bits | 10 mantissa bits
    //  Cas particuliers :
    //    - exp=0,  mant=0  -> +/- 0
    //    - exp=0,  mant!=0 -> denormal (normalise + ajustement)
    //    - exp=31, mant=0  -> +/- inf
    //    - exp=31, mant!=0 -> NaN
    //    - sinon           -> normal : (sign | (exp+112)<<23 | mant<<13)
    // ─────────────────────────────────────────────────────────────────────────

    static NKIMG_INLINE float32 HalfToFloat(uint16 h) noexcept {
        uint32 sign = uint32(h >> 15) & 0x1u;
        uint32 exp  = uint32(h >> 10) & 0x1Fu;
        uint32 mant = uint32(h)       & 0x3FFu;

        uint32 f;
        if (exp == 0) {
            if (mant == 0) {
                f = sign << 31;
            } else {
                // Denormalised : normalise it
                while ((mant & 0x400u) == 0) {
                    mant <<= 1;
                    exp  -= 1; // peut devenir negatif mais on l'ajuste apres
                }
                exp  += 1;
                mant &= ~0x400u;
                f = (sign << 31) | ((exp + (127u - 15u)) << 23) | (mant << 13);
            }
        } else if (exp == 31) {
            // Inf ou NaN : on conserve la mantisse pour distinguer
            f = (sign << 31) | (0xFFu << 23) | (mant << 13);
        } else {
            f = (sign << 31) | ((exp + (127u - 15u)) << 23) | (mant << 13);
        }

        float32 out;
        ::memcpy(&out, &f, 4);
        return out;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Description d'un canal EXR (issue de l'attribut "channels" / chlist)
    // ─────────────────────────────────────────────────────────────────────────

    struct EXRChannel {
        char  name[32];        ///< Nom court (R, G, B, A, Y, Z, RY, BY ...)
        int32 pixelType = 0;   ///< kPixelUINT/HALF/FLOAT
        int32 xSampling = 1;
        int32 ySampling = 1;
        int32 bytesPerPixel = 0; ///< 2 pour HALF, 4 pour UINT/FLOAT

        bool IsValid() const noexcept {
            return pixelType >= kPixelUINT && pixelType <= kPixelFLOAT
                && bytesPerPixel > 0 && xSampling == 1 && ySampling == 1;
        }
    };

    // EXR canaux dans l'ordre alpha trie. On en supporte jusqu'a 8 pour les
    // images depth+RGB+aux. RGBA suffit pour le pipeline rendu.
    static constexpr int32 kMaxChannels = 8;

    // ─────────────────────────────────────────────────────────────────────────
    //  Parsing d'une chaine C dans le header (null-terminated, max maxLen)
    //  Retourne le nombre d'octets consommes (incluant le null) ou -1 si EOF.
    // ─────────────────────────────────────────────────────────────────────────

    static int32 ReadCString(const uint8* base, usize size, usize& pos,
                             char* outBuf, int32 maxLen) noexcept {
        int32 i = 0;
        while (pos < size && i < maxLen - 1) {
            uint8 c = base[pos++];
            if (c == 0) { outBuf[i] = 0; return i + 1; }
            outBuf[i++] = char(c);
        }
        outBuf[i] = 0;
        return -1; // EOF avant null terminator
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Decompresseurs
    //  Tous prennent :
    //    in       : pointeur vers donnees compressees
    //    inSize   : taille compressee
    //    out      : buffer destination pre-alloue
    //    outSize  : taille decompressee attendue (= linesInBlock * bytesPerLine)
    //  Retourne true si decompression OK.
    //
    //  Cas particulier : si inSize >= outSize, OpenEXR ecrit les donnees brutes
    //  (le compresseur s'auto-bypass si la sortie compressee serait plus grande).
    //  Cette regle s'applique a TOUTES les compressions sauf NONE.
    // ─────────────────────────────────────────────────────────────────────────

    static bool DecompressNONE(const uint8* in, usize inSize,
                               uint8* out, usize outSize) noexcept {
        if (inSize != outSize) return false;
        ::memcpy(out, in, outSize);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  RLE OpenEXR : variante PackBits sur les BYTES (apres predictor + reorder).
    //  Format :
    //    code = int8 lu en signed :
    //      - si code >= 0 : (code+1) octets litteraux suivent
    //      - si code <  0 : (-code+1) repetitions de l'octet suivant
    //  Apres decompression brute -> appliquer predictor + reorder reverse.
    // ─────────────────────────────────────────────────────────────────────────

    static bool RleDecodeRaw(const uint8* in, usize inSize,
                             uint8* out, usize outSize) noexcept {
        usize si = 0, di = 0;
        while (si < inSize) {
            int8 code = int8(in[si++]);
            if (code < 0) {
                int32 count = -int32(code) + 1;
                if (si >= inSize) return false;
                uint8 val = in[si++];
                if (di + count > outSize) return false;
                ::memset(out + di, val, count);
                di += count;
            } else {
                int32 count = int32(code) + 1;
                if (si + count > inSize) return false;
                if (di + count > outSize) return false;
                ::memcpy(out + di, in + si, count);
                si += count;
                di += count;
            }
        }
        return di == outSize;
    }

    // Predictor inverse OpenEXR (utilise par ZIP, ZIPS, RLE) :
    // Apres decompression brute, les octets ont ete :
    //   1. inter-leaved : split en 2 halves (even/odd)
    //   2. encodes en differences (curr - prev + 128) mod 256
    // On annule ces 2 transformations dans cet ordre.

    static void Unpredict(uint8* buf, usize size) noexcept {
        // unpredict : reverse de la diff
        //   for (i=1; i<size; i++) buf[i] = buf[i-1] + buf[i] - 128
        uint8* t = buf + 1;
        uint8* stop = buf + size;
        while (t < stop) {
            int32 d = int32(t[-1]) + int32(t[0]) - 128;
            t[0] = uint8(d);
            ++t;
        }
    }

    static void Unreorder(const uint8* src, uint8* dst, usize size) noexcept {
        // Reconstruit en lisant alternativement la moitie basse et la moitie haute
        const uint8* t1 = src;
        const uint8* t2 = src + (size + 1) / 2;
        uint8* s   = dst;
        uint8* end = dst + size;
        while (s < end) {
            if (s < end) *s++ = *t1++;
            if (s < end) *s++ = *t2++;
        }
    }

    static bool DecompressRLE(const uint8* in, usize inSize,
                              uint8* out, usize outSize) noexcept {
        if (inSize >= outSize) {
            // Cas bypass : EXR a stocke les donnees brutes (sans transformations)
            return DecompressNONE(in, inSize, out, outSize);
        }
        // 1. RLE decode dans un buffer temporaire
        uint8* tmp = static_cast<uint8*>(NkAlloc(outSize));
        if (!tmp) return false;
        if (!RleDecodeRaw(in, inSize, tmp, outSize)) {
            NkFree(tmp);
            return false;
        }
        // 2. Reorder inverse : tmp -> out
        Unreorder(tmp, out, outSize);
        NkFree(tmp);
        // 3. Predictor inverse in-place
        Unpredict(out, outSize);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  ZIP / ZIPS : zlib RFC 1950 inflate + predictor + reorder
    //  ZIPS = 1 scanline par bloc ; ZIP = 16 scanlines par bloc.
    //  L'algo est identique, seul LinesPerBlock change (gere en amont).
    // ─────────────────────────────────────────────────────────────────────────

    static bool DecompressZIP(const uint8* in, usize inSize,
                              uint8* out, usize outSize) noexcept {
        if (inSize >= outSize) {
            return DecompressNONE(in, inSize, out, outSize);
        }
        // 1. zlib inflate dans un buffer temporaire
        uint8* tmp = static_cast<uint8*>(NkAlloc(outSize));
        if (!tmp) return false;
        usize written = 0;
        if (!NkDeflate::Decompress(in, inSize, tmp, outSize, written)
            || written != outSize) {
            NkFree(tmp);
            return false;
        }
        // 2. Reorder inverse : tmp -> out
        Unreorder(tmp, out, outSize);
        NkFree(tmp);
        // 3. Predictor inverse in-place
        Unpredict(out, outSize);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  PIZ — wavelet 2D Haar inverse + Huffman canonique
    //
    //  Pipeline decompression :
    //   1. Lecture du bitmap : minNonZero, maxNonZero, puis (max-min+1) octets
    //      indiquant quels valeurs uint16 [0..65535] sont effectivement utilisees.
    //      On construit lut[65536] = liste compacte des valeurs presentes.
    //   2. Decompression Huffman des donnees pixel (uint16 wavelet-transforme).
    //   3. Application du wavelet 2D Haar inverse PAR CANAL (chaque canal a
    //      sa region (nx, ny) dans le buffer interleaved).
    //   4. Re-mapping via lut[] : pour chaque uint16 d -> lut[d] = valeur reelle.
    //
    //  Reference algorithm : OpenEXR IlmImf/ImfPizCompressor + ImfHuf + ImfWav.
    //  Toute l'implementation est ici (autonome, pas de dependance externe).
    // ─────────────────────────────────────────────────────────────────────────

    static constexpr int32 PIZ_USHORT_RANGE = 1 << 16;        // 65536
    static constexpr int32 PIZ_BITMAP_SIZE  = PIZ_USHORT_RANGE >> 3; // 8192

    static constexpr int32 HUF_ENCBITS = 16;
    static constexpr int32 HUF_DECBITS = 14;
    static constexpr int32 HUF_ENCSIZE = (1 << HUF_ENCBITS) + 1;   // 65537
    static constexpr int32 HUF_DECSIZE = 1 << HUF_DECBITS;          // 16384
    static constexpr int32 HUF_DECMASK = HUF_DECSIZE - 1;

    static constexpr int32 SHORT_ZEROCODE_RUN = 59;
    static constexpr int32 LONG_ZEROCODE_RUN  = 63;
    static constexpr int32 SHORTEST_LONG_RUN  = 2 + LONG_ZEROCODE_RUN - SHORT_ZEROCODE_RUN; // 6
    static constexpr int32 LONGEST_LONG_RUN   = 255 + SHORTEST_LONG_RUN; // 261

    // ── Reconstruction du LUT a partir du bitmap ────────────────────────────
    // bitmap = tableau de 8192 octets, bit i (octet i/8, bit i%8) indique si la
    // valeur i est presente dans les donnees. lut[k] = valeur d'index k dans la
    // sequence des valeurs presentes. Retourne le max LUT (pour le wavelet).
    static uint16 PizBitmapToLut(const uint8 bitmap[PIZ_BITMAP_SIZE],
                                 uint16 lut[PIZ_USHORT_RANGE]) noexcept {
        int32 k = 0;
        for (int32 i = 0; i < PIZ_USHORT_RANGE; ++i) {
            if (i == 0 || (bitmap[i >> 3] & (1 << (i & 7))))
                lut[k++] = uint16(i);
        }
        int32 n = k - 1;
        while (k < PIZ_USHORT_RANGE) lut[k++] = 0;
        return uint16(n);
    }

    static void PizApplyLut(const uint16 lut[PIZ_USHORT_RANGE],
                            uint16* data, usize count) noexcept {
        for (usize i = 0; i < count; ++i)
            data[i] = lut[data[i]];
    }

    // ── Wavelet 2D Haar inverse (algorithme OpenEXR ImfWav.cpp) ─────────────
    // Inverse de la transformation utilisee par PIZ : Haar-like avec
    // arrondissements modulo 2^16 (les pixels sont des uint16).
    // (l, h) sont les deux moities transformees, on reconstruit (a, b).
    // mx = valeur max dans le LUT (utilise comme borne pour la modulation).
    static NKIMG_INLINE void PizWdec14(uint16 l, uint16 h,
                                       uint16& a, uint16& b) noexcept {
        const int16 ls = int16(l);
        const int16 hs = int16(h);
        const int16 hi = hs;
        const int16 ai = ls + (hi & 1) + (hi >> 1);
        a = uint16(int16(ai));
        b = uint16(int16(ai - hi));
    }

    // wav2Decode : applique le wavelet inverse 2D sur un buffer de nx*ny
    // valeurs uint16 (offset oy = ligne pitch, ox = colonne pitch). mx est
    // ignore en mode 14-bit (utilise par les versions plus anciennes).
    static void PizWav2Decode(uint16* in, int32 nx, int32 ox,
                              int32 ny, int32 oy, uint16 /*mx*/) noexcept {
        bool w14 = true; // toujours 14-bit pour PIZ standard (sans flag NO_COMPRESSION)
        int32 n = (nx > ny) ? ny : nx;
        int32 p = 1;
        int32 p2;

        // Trouver le plus petit p2 >= n
        while (p <= n) p <<= 1;
        p >>= 1;
        p2 = p;
        p >>= 1;

        while (p >= 1) {
            uint16* py = in;
            uint16* ey = in + oy * (ny - p2);
            int32 oy1 = oy * p;
            int32 oy2 = oy * p2;
            int32 ox1 = ox * p;
            int32 ox2 = ox * p2;
            uint16 i00, i01, i10, i11;
            (void)w14;

            for (; py <= ey; py += oy2) {
                uint16* px = py;
                uint16* ex = py + ox * (nx - p2);
                for (; px <= ex; px += ox2) {
                    uint16* p01 = px  + ox1;
                    uint16* p10 = px  + oy1;
                    uint16* p11 = p10 + ox1;

                    PizWdec14(*px,  *p10, i00, i10);
                    PizWdec14(*p01, *p11, i01, i11);
                    PizWdec14(i00,  i01,  *px,  *p01);
                    PizWdec14(i10,  i11,  *p10, *p11);
                }
                // Si nx est impair, gerer la derniere colonne
                if (nx & p) {
                    uint16* p10 = px + oy1;
                    PizWdec14(*px, *p10, i00, i10);
                    *px  = i00;
                    *p10 = i10;
                }
            }
            // Si ny est impair, gerer la derniere ligne
            if (ny & p) {
                uint16* px = py;
                uint16* ex = py + ox * (nx - p2);
                for (; px <= ex; px += ox2) {
                    uint16* p01 = px + ox1;
                    PizWdec14(*px, *p01, i00, i01);
                    *px  = i00;
                    *p01 = i01;
                }
            }
            p2 = p;
            p >>= 1;
        }
    }

    // ── Bitstream reader MSB-first pour Huffman PIZ ─────────────────────────
    // Lit jusqu'a 58 bits par appel (code Huffman canonique max length 58 bits).
    struct PizBitReader {
        const uint8* ptr;
        const uint8* end;
        uint64 buf;
        int32  nbits; // bits valides dans buf (MSB-aligned)

        void Init(const uint8* p, usize size) noexcept {
            ptr = p; end = p + size; buf = 0; nbits = 0;
        }

        // S'assure que >= n bits dispo dans buf (max 56 bits cumulables)
        NKIMG_INLINE void Fill(int32 n) noexcept {
            while (nbits < n && ptr < end) {
                buf |= (uint64(*ptr++) << (56 - nbits));
                nbits += 8;
            }
        }

        // Consomme n bits (MSB-first), retourne valeur
        NKIMG_INLINE uint64 Get(int32 n) noexcept {
            Fill(n);
            uint64 v = (buf >> (64 - n)) & ((uint64(1) << n) - 1);
            buf <<= n;
            nbits -= n;
            return v;
        }

        // Peek n bits sans consommer
        NKIMG_INLINE uint64 Peek(int32 n) noexcept {
            Fill(n);
            return (buf >> (64 - n)) & ((uint64(1) << n) - 1);
        }
    };

    // ── Lecture du tableau de longueurs de codes Huffman (canonique RLE) ────
    static bool PizHufUnpackLengths(PizBitReader& br, int32 im, int32 iM,
                                    uint64 lengths[HUF_ENCSIZE]) noexcept {
        for (int32 i = 0; i <= iM; ++i) lengths[i] = 0;
        int32 k = im;
        while (k <= iM) {
            uint64 l = br.Get(6);
            if (l == SHORT_ZEROCODE_RUN) {
                int32 n = int32(br.Get(8)) + 2;
                if (k + n > iM + 1) return false;
                while (n--) lengths[k++] = 0;
            } else if (l >= LONG_ZEROCODE_RUN) {
                int32 n = int32(br.Get(8)) + SHORTEST_LONG_RUN;
                if (k + n > iM + 1) return false;
                while (n--) lengths[k++] = 0;
            } else {
                lengths[k++] = l;
            }
        }
        return true;
    }

    // ── Construction des codes Huffman canoniques + LUT decode rapide ──────
    // Codes assignes par ordre canonique (BMP order). LUT decode pour codes
    // <= HUF_DECBITS (14 bits) pour decodage O(1) ; les codes plus longs
    // requierent une recherche lineaire dans la table fallback.
    static bool PizHufBuildDecodeTable(const uint64 lengths[HUF_ENCSIZE],
                                       int32 im, int32 iM,
                                       int32 codes[HUF_ENCSIZE]) noexcept {
        // 1. Compte des codes par longueur
        int32 nCount[64] = {0};
        for (int32 i = im; i <= iM; ++i) {
            if (lengths[i] >= 64) return false;
            ++nCount[lengths[i]];
        }
        nCount[0] = 0; // longueur 0 = symbole inutilise

        // 2. firstCode[l] = premier code canonique de longueur l
        uint64 firstCode[64] = {0};
        uint64 c = 0;
        for (int32 l = 1; l < 64; ++l) {
            c = (c + uint64(nCount[l-1])) << 1;
            firstCode[l] = c;
        }

        // 3. Assigner les codes
        uint64 nextCode[64];
        for (int32 i = 0; i < 64; ++i) nextCode[i] = firstCode[i];
        for (int32 i = im; i <= iM; ++i) {
            int32 l = int32(lengths[i]);
            if (l > 0) {
                codes[i] = int32(nextCode[l]++);
            } else {
                codes[i] = -1;
            }
        }
        return true;
    }

    // Decode complet d'un stream Huffman.
    // br      : bitreader positionne sur le debut des donnees encodees
    // nBits   : nombre TOTAL de bits a decoder
    // lengths : table des longueurs de codes par symbole
    // codes   : table des codes canoniques par symbole
    // out     : destination (count uint16)
    // count   : nombre attendu de symboles decodes
    static bool PizHufDecode(PizBitReader& br, uint64 nBits,
                             const uint64 lengths[HUF_ENCSIZE],
                             const int32 codes[HUF_ENCSIZE],
                             int32 im, int32 iM,
                             uint16* out, usize count) noexcept {
        // Construire LUT fast pour codes courts (<= HUF_DECBITS = 14 bits).
        // Pour chaque entree fastLut[i] :
        //   - si i correspond a un prefixe de code <= 14 bits :
        //     symbol = bits hauts du code, length = nb bits restants
        // On utilise une table de mapping inverse :
        //   pour chaque symbole valide, marquer toutes les entrees fastLut
        //   correspondant au code (les bits restants sont arbitraires).
        struct FastEntry { uint16 symbol; uint8 length; uint8 valid; };
        FastEntry* fastLut = static_cast<FastEntry*>(NkAlloc(HUF_DECSIZE * sizeof(FastEntry)));
        if (!fastLut) return false;
        ::memset(fastLut, 0, HUF_DECSIZE * sizeof(FastEntry));

        // Compteur de symboles a recherche lineaire (codes > HUF_DECBITS)
        // Stocker (code, length, symbol) tries par length croissant
        struct LongEntry { uint64 code; uint8 length; int32 symbol; };
        int32 longCount = 0;
        LongEntry* longTab = static_cast<LongEntry*>(NkAlloc((iM - im + 1) * sizeof(LongEntry)));
        if (!longTab) { NkFree(fastLut); return false; }

        for (int32 sym = im; sym <= iM; ++sym) {
            int32 l = int32(lengths[sym]);
            if (l <= 0) continue;
            int32 code = codes[sym];
            if (l <= HUF_DECBITS) {
                // Code court : remplir toutes les entrees fastLut [code<<(DECBITS-l) .. +(1<<(DECBITS-l))-1]
                int32 nRem = HUF_DECBITS - l;
                int32 base = code << nRem;
                int32 nEnt = 1 << nRem;
                for (int32 e = 0; e < nEnt; ++e) {
                    fastLut[base + e].symbol = uint16(sym);
                    fastLut[base + e].length = uint8(l);
                    fastLut[base + e].valid  = 1;
                }
            } else {
                longTab[longCount].code = uint64(code);
                longTab[longCount].length = uint8(l);
                longTab[longCount].symbol = sym;
                ++longCount;
            }
        }

        // Decode boucle principale
        bool ok = true;
        uint64 bitsConsumed = 0;
        for (usize i = 0; i < count; ++i) {
            // Verifier qu'il reste assez de bits
            if (bitsConsumed >= nBits) {
                // Toleres : il peut rester quelques bits de padding ; out[i] = 0
                out[i] = 0;
                continue;
            }
            // Peek 14 bits pour LUT
            br.Fill(HUF_DECBITS);
            if (br.nbits == 0) { ok = false; break; }
            int32 peekBits = (br.nbits < HUF_DECBITS) ? br.nbits : HUF_DECBITS;
            uint64 peek = (br.buf >> (64 - HUF_DECBITS)) & HUF_DECMASK;
            FastEntry& fe = fastLut[peek];
            if (fe.valid && fe.length <= peekBits) {
                out[i] = fe.symbol;
                br.buf <<= fe.length;
                br.nbits -= fe.length;
                bitsConsumed += fe.length;
            } else {
                // Recherche lineaire dans longTab
                bool found = false;
                for (int32 l = HUF_DECBITS + 1; l <= 58; ++l) {
                    br.Fill(l);
                    if (br.nbits < l) break;
                    uint64 v = (br.buf >> (64 - l));
                    for (int32 j = 0; j < longCount; ++j) {
                        if (longTab[j].length == l && longTab[j].code == v) {
                            out[i] = uint16(longTab[j].symbol);
                            br.buf <<= l;
                            br.nbits -= l;
                            bitsConsumed += l;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found) { ok = false; break; }
            }
        }

        NkFree(longTab);
        NkFree(fastLut);
        return ok;
    }

    // ── Entree principale : decompresse un bloc PIZ vers le scratch EXR ─────
    static bool DecompressPIZImpl(const uint8* in, usize inSize,
                                  uint8* out, usize outSize,
                                  const EXRChannel* chans, int32 numChans,
                                  int32 width, int32 linesInBlock) noexcept {
        if (inSize < 4 + PIZ_BITMAP_SIZE) {
            // Bypass : si les donnees compressees >= taille decompressee,
            // OpenEXR ecrit en clair. Cas tres rare en PIZ (toujours plus dense).
            if (inSize >= outSize) return DecompressNONE(in, inSize, out, outSize);
        }
        const uint8* p = in;
        const uint8* pEnd = in + inSize;

        if (p + 4 > pEnd) return false;
        uint16 minNonZero = RD_U16LE(p); p += 2;
        uint16 maxNonZero = RD_U16LE(p); p += 2;

        // maxNonZero est uint16 donc <= 65535 par construction ; on s'assure
        // surtout que min <= max (validation faite plus bas).
        (void)maxNonZero;

        // Lire le bitmap (zone non-zero)
        uint8 bitmap[PIZ_BITMAP_SIZE];
        ::memset(bitmap, 0, sizeof(bitmap));
        if (maxNonZero >= minNonZero) {
            int32 nB = maxNonZero - minNonZero + 1;
            if (p + nB > pEnd) return false;
            ::memcpy(bitmap + minNonZero, p, nB);
            p += nB;
        }

        // Construire le LUT (PIZ_USHORT_RANGE = 65536 entrees uint16)
        uint16* lut = static_cast<uint16*>(NkAlloc(PIZ_USHORT_RANGE * sizeof(uint16)));
        if (!lut) return false;
        uint16 mx = PizBitmapToLut(bitmap, lut);
        (void)mx;

        // Lire la longueur des donnees Huffman compressees (32 bits LE)
        if (p + 4 > pEnd) { NkFree(lut); return false; }
        uint32 hufCompLen = RD_U32LE(p); p += 4;
        if (p + hufCompLen > pEnd) { NkFree(lut); return false; }

        // Decode Huffman : header = 5 * uint32 LE
        const uint8* hp = p;
        if (hufCompLen < 20) { NkFree(lut); return false; }
        uint32 im   = RD_U32LE(hp + 0);
        uint32 iM   = RD_U32LE(hp + 4);
        // hp + 8 = tableLength (info packed dans bits, on calcule via nBits)
        // hp + 12 = nBits
        uint32 nBitsLow  = RD_U32LE(hp + 12);
        uint32 nBitsHigh = RD_U32LE(hp + 16);
        uint64 nBits = uint64(nBitsLow) | (uint64(nBitsHigh) << 32);
        if (im >= uint32(HUF_ENCSIZE) || iM >= uint32(HUF_ENCSIZE) || im > iM) {
            NkFree(lut); return false;
        }

        // Buffer pour les uint16 decodes (count = somme des pixels par canal)
        // Layout EXR : pour chaque ligne, [ch0 pixels][ch1 pixels]...
        // En PIZ tous les canaux sont stockes en uint16 (donc pour FLOAT 32-bit
        // canal, 2 uint16 par sample : low/high).
        usize totalU16 = 0;
        for (int32 c = 0; c < numChans; ++c) {
            totalU16 += usize(width) * usize(linesInBlock)
                     * usize(chans[c].bytesPerPixel / 2);
        }
        uint16* decoded = static_cast<uint16*>(NkAlloc(totalU16 * sizeof(uint16)));
        if (!decoded) { NkFree(lut); return false; }

        PizBitReader br;
        br.Init(hp + 20, hufCompLen - 20);
        uint64 lengths[HUF_ENCSIZE];
        if (!PizHufUnpackLengths(br, int32(im), int32(iM), lengths)) {
            NkFree(decoded); NkFree(lut); return false;
        }
        int32* canon = static_cast<int32*>(NkAlloc(HUF_ENCSIZE * sizeof(int32)));
        if (!canon) { NkFree(decoded); NkFree(lut); return false; }
        if (!PizHufBuildDecodeTable(lengths, int32(im), int32(iM), canon)) {
            NkFree(canon); NkFree(decoded); NkFree(lut); return false;
        }
        if (!PizHufDecode(br, nBits, lengths, canon, int32(im), int32(iM),
                          decoded, totalU16)) {
            NkFree(canon); NkFree(decoded); NkFree(lut); return false;
        }
        NkFree(canon);

        // Appliquer le wavelet 2D Haar inverse par canal
        // Le buffer 'decoded' est organise PAR CANAL (pas interleaved par ligne) :
        // [canal 0 : width * linesInBlock * (bytesPerPixel/2) uint16]
        // [canal 1 : ...]
        uint16* cp = decoded;
        for (int32 c = 0; c < numChans; ++c) {
            int32 perSample = chans[c].bytesPerPixel / 2; // 1 pour HALF, 2 pour FLOAT
            for (int32 s = 0; s < perSample; ++s) {
                int32 nx = width;
                int32 ny = linesInBlock;
                PizWav2Decode(cp, nx, 1, ny, nx, mx);
                cp += usize(nx) * usize(ny);
            }
        }

        // Appliquer le LUT inverse a tous les uint16
        PizApplyLut(lut, decoded, totalU16);
        NkFree(lut);

        // Re-interleaver vers la layout EXR scanline (par ligne, par canal)
        // Buffer source (decoded) : par canal entier puis par sample, layout
        // [canal 0 sample 0 nxny][canal 0 sample 1 nxny][canal 1 sample 0]...
        // Buffer destination (out) : par ligne puis par canal :
        //   line[i] = [ch0 row][ch1 row]...
        uint8* dst = out;
        // Index par canal : pointeur dans decoded de la 1ere ligne
        uint16* chanStart[kMaxChannels];
        uint16* cur = decoded;
        for (int32 c = 0; c < numChans; ++c) {
            chanStart[c] = cur;
            cur += usize(width) * usize(linesInBlock) * usize(chans[c].bytesPerPixel / 2);
        }

        for (int32 li = 0; li < linesInBlock; ++li) {
            for (int32 c = 0; c < numChans; ++c) {
                int32 perSample = chans[c].bytesPerPixel / 2;
                // Pour FLOAT (perSample=2), les 2 uint16 sont stockes
                // separement (sample 0 = high 16 bits, sample 1 = low) puis
                // recombines pour former le float32. L'ordre dans le bitstream
                // est : tous les highs de la ligne, puis tous les lows.
                // Sortie : entrelace par pixel.
                if (perSample == 1) {
                    // HALF : copie directe
                    uint16* src = chanStart[c] + usize(li) * usize(width);
                    for (int32 x = 0; x < width; ++x) {
                        dst[0] = uint8(src[x] & 0xFF);
                        dst[1] = uint8(src[x] >> 8);
                        dst += 2;
                    }
                } else {
                    // FLOAT : recombiner high/low (PIZ stocke high puis low)
                    uint16* srcHi = chanStart[c] + usize(li) * usize(width);
                    uint16* srcLo = chanStart[c] + usize(width) * usize(linesInBlock)
                                  + usize(li) * usize(width);
                    for (int32 x = 0; x < width; ++x) {
                        uint32 hi = uint32(srcHi[x]);
                        uint32 lo = uint32(srcLo[x]);
                        uint32 v  = (hi << 16) | lo;
                        dst[0] = uint8(v & 0xFF);
                        dst[1] = uint8((v >> 8) & 0xFF);
                        dst[2] = uint8((v >> 16) & 0xFF);
                        dst[3] = uint8((v >> 24) & 0xFF);
                        dst += 4;
                    }
                }
            }
        }

        NkFree(decoded);
        return (dst - out) == nk_int64(outSize);
    }

    // Adaptateur pour rester sur la signature standard utilisee par RLE/ZIP/NONE.
    // PIZ a besoin du contexte (chans, numChans, width, linesInBlock).
    // On stocke ces parametres dans des variables globales thread_local pour
    // limiter les changements de signature dans le pipeline principal.
    static thread_local const EXRChannel* gPizChans = nullptr;
    static thread_local int32 gPizNumChans = 0;
    static thread_local int32 gPizWidth = 0;
    static thread_local int32 gPizLines = 0;

    static bool DecompressPIZ(const uint8* in, usize inSize,
                              uint8* out, usize outSize) noexcept {
        if (!gPizChans || gPizNumChans == 0 || gPizWidth == 0 || gPizLines == 0)
            return false;
        return DecompressPIZImpl(in, inSize, out, outSize,
                                 gPizChans, gPizNumChans,
                                 gPizWidth, gPizLines);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Conversion d'une ligne EXR decompressee -> NkImage destination
    //
    //  Layout EXR scanline (apres decompression d'un bloc) :
    //   [line0:channel0 row][line0:channel1 row]...[line0:channelN row]
    //   [line1:channel0 row][line1:channel1 row]...
    //   ...
    //  Les canaux sont tries alphabetiquement (A,B,G,R) dans le bloc.
    //
    //  Le mapping vers RGBA[8] / RGB[8] est fait par index dans chans[] :
    //    rIdx, gIdx, bIdx, aIdx : indice du canal dans chans (-1 si absent).
    //  Si tous == -1 (Y/Z seul), on duplique Y/Z sur R=G=B.
    // ─────────────────────────────────────────────────────────────────────────

    struct ChannelMap {
        int32 rIdx;
        int32 gIdx;
        int32 bIdx;
        int32 aIdx;
        int32 yIdx;   ///< fallback luminance pour images Y/Z only
    };

    static float32 ReadSample(const uint8* p, int32 pixelType) noexcept {
        switch (pixelType) {
            case kPixelHALF:  return HalfToFloat(RD_U16LE(p));
            case kPixelFLOAT: return RD_F32LE(p);
            case kPixelUINT:  return float32(RD_U32LE(p));
            default:          return 0.f;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  Pipeline complet du Decode
    // ─────────────────────────────────────────────────────────────────────────

    NkImage* NkEXRCodec::Decode(const uint8* data, usize size) noexcept {
        // 1. Validation magic + version
        if (size < 8) {
            logger.Error("[EXR] Buffer trop petit (< 8 octets).");
            return nullptr;
        }
        if (RD_U32LE(data) != kEXRMagic) {
            logger.Error("[EXR] Magic invalide (attendu 0x01312F76).");
            return nullptr;
        }
        uint32 version = RD_U32LE(data + 4);
        uint32 versionNum = version & 0xFFu;
        uint32 flags      = (version >> 8) & 0xFFFFFFu;
        if (versionNum != 2) {
            logger.Error("[EXR] Version {0} non supportee (attendu 2).", versionNum);
            return nullptr;
        }
        // flags : bit 1 = tiles, bit 11 = multipart, bit 12 = non-image,
        //         bit 9  = deep data, bit 10 = long names (256 chars supporte)
        const bool isTiled     = (flags & 0x0200u) != 0;
        const bool isMultipart = (flags & 0x1000u) != 0;
        const bool isDeep      = (flags & 0x0800u) != 0;
        if (isTiled || isMultipart || isDeep) {
            logger.Error("[EXR] Variantes tile/multipart/deep non supportees (flags=0x{0:X}).",
                         flags);
            return nullptr;
        }

        usize pos = 8;

        // 2. Parsing des attributs jusqu'a l'octet null final
        EXRChannel chans[kMaxChannels];
        int32 numChans = 0;
        EXRCompression compression = EXRCompression::NONE;
        int32 xMin = 0, yMin = 0, xMax = -1, yMax = -1;
        int32 lineOrder = 0; // 0=INCREASING_Y, 1=DECREASING_Y, 2=RANDOM_Y

        bool seenChannels    = false;
        bool seenCompression = false;
        bool seenDataWindow  = false;

        for (int32 iter = 0; iter < 256; ++iter) {
            // Fin de header : nom vide (un seul octet null)
            if (pos >= size) {
                logger.Error("[EXR] EOF dans le header.");
                return nullptr;
            }
            if (data[pos] == 0) { pos++; break; }

            char attrName[256] = {0};
            char attrType[256] = {0};
            if (ReadCString(data, size, pos, attrName, sizeof(attrName)) < 0) {
                logger.Error("[EXR] Nom d'attribut tronque."); return nullptr;
            }
            if (ReadCString(data, size, pos, attrType, sizeof(attrType)) < 0) {
                logger.Error("[EXR] Type d'attribut tronque."); return nullptr;
            }
            if (pos + 4 > size) {
                logger.Error("[EXR] Taille attribut tronquee."); return nullptr;
            }
            int32 attrSize = RD_I32LE(data + pos); pos += 4;
            if (attrSize < 0 || pos + usize(attrSize) > size) {
                logger.Error("[EXR] Donnees attribut tronquees pour '{0}'.", attrName);
                return nullptr;
            }
            const uint8* attrData = data + pos;
            const usize  attrEnd  = pos + usize(attrSize);

            // Attributs critiques
            if (::strcmp(attrName, "channels") == 0
             && ::strcmp(attrType, "chlist") == 0) {
                // chlist = liste de structs terminee par un octet null
                //   pour chaque canal :
                //     char name[];           // null-terminated
                //     int32 pixelType
                //     uint8 pLinear + 3 reserved
                //     int32 xSampling
                //     int32 ySampling
                usize cp = pos;
                while (cp < attrEnd && data[cp] != 0 && numChans < kMaxChannels) {
                    EXRChannel& ch = chans[numChans];
                    char nameBuf[64];
                    if (ReadCString(data, attrEnd, cp, nameBuf, sizeof(nameBuf)) < 0)
                        break;
                    ::strncpy(ch.name, nameBuf, sizeof(ch.name) - 1);
                    ch.name[sizeof(ch.name) - 1] = 0;
                    if (cp + 16 > attrEnd) {
                        logger.Error("[EXR] Canal '{0}' tronque.", ch.name);
                        return nullptr;
                    }
                    ch.pixelType = RD_I32LE(data + cp); cp += 4;
                    cp += 4; // pLinear + 3 reserved
                    ch.xSampling = RD_I32LE(data + cp); cp += 4;
                    ch.ySampling = RD_I32LE(data + cp); cp += 4;
                    ch.bytesPerPixel = (ch.pixelType == kPixelHALF) ? 2 : 4;
                    if (!ch.IsValid()) {
                        logger.Error("[EXR] Canal '{0}' invalide (pixelType={1}, sampling={2}x{3}).",
                                     ch.name, ch.pixelType, ch.xSampling, ch.ySampling);
                        return nullptr;
                    }
                    ++numChans;
                }
                seenChannels = true;
            }
            else if (::strcmp(attrName, "compression") == 0
                  && ::strcmp(attrType, "compression") == 0
                  && attrSize >= 1) {
                compression = EXRCompression(attrData[0]);
                seenCompression = true;
            }
            else if (::strcmp(attrName, "dataWindow") == 0
                  && ::strcmp(attrType, "box2i") == 0
                  && attrSize >= 16) {
                xMin = RD_I32LE(attrData + 0);
                yMin = RD_I32LE(attrData + 4);
                xMax = RD_I32LE(attrData + 8);
                yMax = RD_I32LE(attrData + 12);
                seenDataWindow = true;
            }
            else if (::strcmp(attrName, "lineOrder") == 0
                  && ::strcmp(attrType, "lineOrder") == 0
                  && attrSize >= 1) {
                lineOrder = int32(attrData[0]);
            }
            // Tous les autres attributs sont ignores (displayWindow,
            // pixelAspectRatio, Exif:*, comments, etc.)

            pos = attrEnd;
        }

        // 3. Validation des attributs obligatoires
        if (!seenChannels || numChans == 0) {
            logger.Error("[EXR] Attribut 'channels' manquant ou vide."); return nullptr;
        }
        if (!seenCompression) {
            logger.Error("[EXR] Attribut 'compression' manquant."); return nullptr;
        }
        if (!seenDataWindow) {
            logger.Error("[EXR] Attribut 'dataWindow' manquant."); return nullptr;
        }
        const int32 width  = xMax - xMin + 1;
        const int32 height = yMax - yMin + 1;
        if (width <= 0 || height <= 0) {
            logger.Error("[EXR] dataWindow invalide ({0}x{1}).", width, height);
            return nullptr;
        }
        if (lineOrder == 2) {
            logger.Error("[EXR] lineOrder RANDOM_Y non supporte.");
            return nullptr;
        }

        // 4. Validation compression supportee
        const bool zipPath = (compression == EXRCompression::ZIP
                           || compression == EXRCompression::ZIPS);
        const bool supported = (compression == EXRCompression::NONE
                             || compression == EXRCompression::RLE
                             || zipPath
                             || compression == EXRCompression::PIZ);
        if (!supported) {
            logger.Error("[EXR] Compression {0} non supportee. "
                         "Re-encoder via : oiiotool input.exr --compression zip -o output.exr",
                         CompressionName(compression));
            return nullptr;
        }

        // 5. Construction du mapping canal R/G/B/A/Y
        ChannelMap cm{-1, -1, -1, -1, -1};
        for (int32 i = 0; i < numChans; ++i) {
            if (::strcmp(chans[i].name, "R") == 0) cm.rIdx = i;
            else if (::strcmp(chans[i].name, "G") == 0) cm.gIdx = i;
            else if (::strcmp(chans[i].name, "B") == 0) cm.bIdx = i;
            else if (::strcmp(chans[i].name, "A") == 0) cm.aIdx = i;
            else if (::strcmp(chans[i].name, "Y") == 0) cm.yIdx = i;
            else if (::strcmp(chans[i].name, "Z") == 0 && cm.yIdx < 0) cm.yIdx = i;
        }
        const bool hasRGB = (cm.rIdx >= 0 && cm.gIdx >= 0 && cm.bIdx >= 0);
        const bool hasA   = (cm.aIdx >= 0);
        const bool hasY   = (cm.yIdx >= 0);
        if (!hasRGB && !hasY) {
            logger.Error("[EXR] Aucun canal R/G/B ou Y/Z reconnu (canaux trouves : {0}).",
                         numChans);
            return nullptr;
        }

        // 6. Bytes par scanline (par canal) et par bloc
        const int32 linesPerBlock = LinesPerBlock(compression);
        int32 bytesPerLineAllChans = 0;
        for (int32 i = 0; i < numChans; ++i)
            bytesPerLineAllChans += chans[i].bytesPerPixel * width;

        // Nombre de blocs (chunks scanline)
        const int32 numBlocks = (height + linesPerBlock - 1) / linesPerBlock;

        // 7. Lecture de la table d'offsets scanline (int64 LE x numBlocks)
        if (pos + usize(numBlocks) * 8 > size) {
            logger.Error("[EXR] Table d'offsets tronquee."); return nullptr;
        }
        const uint8* offsetTable = data + pos;
        pos += usize(numBlocks) * 8;

        // 8. Allocation de l'image de sortie
        const NkImagePixelFormat outFmt = hasA ? NkImagePixelFormat::NK_RGBA128F
                                              : NkImagePixelFormat::NK_RGB96F;
        const int32 outChannels = ChannelsOf(outFmt);
        NkImage* img = NkImage::Alloc(width, height, outFmt);
        if (!img) {
            logger.Error("[EXR] Echec allocation image {0}x{1}.", width, height);
            return nullptr;
        }

        // Buffer scratch reutilise pour chaque bloc decompresse :
        // taille max = linesPerBlock * bytesPerLineAllChans
        const usize scratchSize = usize(linesPerBlock) * usize(bytesPerLineAllChans);
        uint8* scratch = static_cast<uint8*>(NkAlloc(scratchSize));
        if (!scratch) {
            img->Free();
            logger.Error("[EXR] Echec allocation buffer decompression ({0} octets).",
                         scratchSize);
            return nullptr;
        }

        // 9. Iteration sur les blocs scanline
        for (int32 b = 0; b < numBlocks; ++b) {
            const nk_int64 offset = RD_I64LE(offsetTable + b * 8);
            if (offset < 0 || usize(offset) + 8 > size) {
                logger.Error("[EXR] Offset bloc {0} invalide.", b);
                NkFree(scratch); img->Free(); return nullptr;
            }
            const uint8* blk = data + offset;
            int32 yStart = RD_I32LE(blk);
            int32 blkSize = RD_I32LE(blk + 4);
            if (blkSize < 0 || usize(offset) + 8 + usize(blkSize) > size) {
                logger.Error("[EXR] Taille bloc {0} invalide ({1}).", b, blkSize);
                NkFree(scratch); img->Free(); return nullptr;
            }

            // Lignes effectives dans ce bloc (le dernier peut etre tronque)
            int32 lineInBlock = linesPerBlock;
            int32 yLocalStart = yStart - yMin;
            if (yLocalStart + lineInBlock > height)
                lineInBlock = height - yLocalStart;
            const usize blockOutSize = usize(lineInBlock) * usize(bytesPerLineAllChans);

            bool ok = false;
            switch (compression) {
                case EXRCompression::NONE:
                    ok = DecompressNONE(blk + 8, blkSize, scratch, blockOutSize);
                    break;
                case EXRCompression::RLE:
                    ok = DecompressRLE(blk + 8, blkSize, scratch, blockOutSize);
                    break;
                case EXRCompression::ZIP:
                case EXRCompression::ZIPS:
                    ok = DecompressZIP(blk + 8, blkSize, scratch, blockOutSize);
                    break;
                case EXRCompression::PIZ:
                    // PIZ a besoin du contexte (chans, width, linesInBlock) que
                    // l'on publie en TLS pour eviter une signature elargie.
                    gPizChans    = chans;
                    gPizNumChans = numChans;
                    gPizWidth    = width;
                    gPizLines    = lineInBlock;
                    ok = DecompressPIZ(blk + 8, blkSize, scratch, blockOutSize);
                    break;
                default:
                    ok = false;
                    break;
            }
            if (!ok) {
                logger.Error("[EXR] Decompression bloc {0} echouee (compression={1}).",
                             b, CompressionName(compression));
                NkFree(scratch); img->Free(); return nullptr;
            }

            // 10. Distribution des canaux vers l'image de sortie
            // Pour chaque ligne du bloc, parcourir les canaux dans l'ordre alpha
            // et copier les samples R/G/B/A demandes.
            const uint8* linePtr = scratch;
            for (int32 li = 0; li < lineInBlock; ++li) {
                const int32 dstY = (lineOrder == 1)
                    ? (height - 1 - (yLocalStart + li)) // DECREASING_Y -> flip
                    : (yLocalStart + li);
                if (dstY < 0 || dstY >= height) {
                    // Ligne hors image (cas DECREASING_Y avec dataWindow excentre)
                    for (int32 ci = 0; ci < numChans; ++ci)
                        linePtr += chans[ci].bytesPerPixel * width;
                    continue;
                }
                float32* rowF = reinterpret_cast<float32*>(img->RowPtr(dstY));

                // Sauvegarde des pointeurs de debut de chaque canal pour cette ligne
                const uint8* chPtr[kMaxChannels];
                {
                    const uint8* p = linePtr;
                    for (int32 ci = 0; ci < numChans; ++ci) {
                        chPtr[ci] = p;
                        p += chans[ci].bytesPerPixel * width;
                    }
                    linePtr = p;
                }

                // Remplissage pixel par pixel
                if (hasRGB) {
                    const uint8* pR = chPtr[cm.rIdx];
                    const uint8* pG = chPtr[cm.gIdx];
                    const uint8* pB = chPtr[cm.bIdx];
                    const uint8* pA = hasA ? chPtr[cm.aIdx] : nullptr;
                    const int32  bpR = chans[cm.rIdx].bytesPerPixel;
                    const int32  bpG = chans[cm.gIdx].bytesPerPixel;
                    const int32  bpB = chans[cm.bIdx].bytesPerPixel;
                    const int32  bpA = hasA ? chans[cm.aIdx].bytesPerPixel : 0;
                    const int32  ptR = chans[cm.rIdx].pixelType;
                    const int32  ptG = chans[cm.gIdx].pixelType;
                    const int32  ptB = chans[cm.bIdx].pixelType;
                    const int32  ptA = hasA ? chans[cm.aIdx].pixelType : 0;
                    for (int32 x = 0; x < width; ++x) {
                        rowF[x * outChannels + 0] = ReadSample(pR + x * bpR, ptR);
                        rowF[x * outChannels + 1] = ReadSample(pG + x * bpG, ptG);
                        rowF[x * outChannels + 2] = ReadSample(pB + x * bpB, ptB);
                        if (hasA)
                            rowF[x * outChannels + 3] = ReadSample(pA + x * bpA, ptA);
                    }
                } else {
                    // Y ou Z seul : duplique sur R=G=B
                    const uint8* pY = chPtr[cm.yIdx];
                    const int32  bpY = chans[cm.yIdx].bytesPerPixel;
                    const int32  ptY = chans[cm.yIdx].pixelType;
                    for (int32 x = 0; x < width; ++x) {
                        const float32 v = ReadSample(pY + x * bpY, ptY);
                        rowF[x * outChannels + 0] = v;
                        rowF[x * outChannels + 1] = v;
                        rowF[x * outChannels + 2] = v;
                    }
                }
            }
        }

        NkFree(scratch);
        logger.Info("[EXR] Decode reussi : {0}x{1} {2} canaux compression={3}.",
                    width, height, numChans, CompressionName(compression));
        return img;
    }

} // namespace nkentseu
