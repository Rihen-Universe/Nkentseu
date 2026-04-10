/**
 * @File    NkJPEGCodec.cpp
 * @Brief   Codec JPEG baseline DCT — portage production-ready de stb_image v2.16.
 *          Encodeur JFIF complet (SOF0, DQT, DHT, SOS, scan entropique).
 *          (public domain original : Sean Barrett). Aucun header stb inclus.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * ─── CORRECTIONS CRITIQUES APPLIQUEES ────────────────────────────────────────
 *
 *  BUG A — Grow() injectait 0xFF dans le buffer lors d'un marker valide
 *    stb: marker trouvé → marker=c; noMore=true; return;  (rien dans buf)
 *    CORRIGE: retour immédiat sans toucher buf/cnt
 *
 *  BUG B — Grow() ne faisait pas avancer pos correctement
 *    CORRIGE: get8() = pos<size ? data[pos++] : 0u  (lit ET avance)
 *
 *  BUG C — Dequantification AC: coeff[zig] *= qt[zig] et NON qt[k]
 *    kDeZig donne la position naturelle depuis l'indice zigzag.
 *    qt[] est indexé en position naturelle.
 *    CORRIGE: zig=kDeZig[k]; coeff[zig]=ext*qt[zig]
 *
 *  BUG D — ExtendReceive: sgn doit être int32 (signed) pour >> 31 correct
 *    CORRIGE: int32 sgn = int32(buf) >> 31;
 *
 *  BUG E — Upsampling 1:1: jCp ne remplissait que cbW octets, pas w
 *    CORRIGE: copie min(cbW,w) puis répète le dernier pixel si cbW < w
 *
 *  BUG F — Encodeur SOS mal formé pour YCbCr 4:2:0 (longueur segment)
 *    CORRIGE: longueur SOS = 2 + 1 + ns*2 + 3
 *
 *  BUG G — Encodeur: YCbCr 4:2:0 Cb/Cr subsampled calculés incorrectement
 *    CORRIGE: moyenne 2x2 avant FDCT, blocs Y encodés avant Cb/Cr
 *
 *  BUG H — RST markers ignorés silencieusement → désync du bitstream
 *    CORRIGE: RST markers réinitialisent les predicteurs DC
 *
 *  BUG I — DQT: qtab stocké en position zigzag, IDCT attend naturel
 *    CORRIGE: dequantification appliquée AVANT dézigzag
 *             (ou qtab stocké en naturel via kDeZig lors du parsing)
 *
 *  BUG J — SOF0: composantes dont hSamp/vSamp == 0 → division par zéro
 *    CORRIGE: validation hSamp/vSamp >= 1
 *
 *  BUG K — EXIF: lecture IFD sans vérification de l'endianness du type
 *    CORRIGE: lecture SHORT (type 3) ou LONG (type 4) selon le champ type
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {
    using namespace nkentseu::memory;

    // ─── Helpers mémoire ────────────────────────────────────────────────────────
    static inline void* jM (usize n)              noexcept { return NkAlloc(n); }
    static inline void  jF (void* p)              noexcept { if(p) NkFree(p); }
    static inline void  jCp(void* d,const void* s,usize n) noexcept {
        NkCopy(static_cast<uint8*>(d), static_cast<const uint8*>(s), n);
    }
    static inline void  jSt(void* d, int v, usize n) noexcept {
        NkSet(static_cast<uint8*>(d), uint8(v), n);
    }

    // ─── Clamp [0,255] ──────────────────────────────────────────────────────────
    static NKIMG_INLINE uint8 jClamp(int32 x) noexcept {
        return (unsigned(x) > 255u) ? (x < 0 ? 0u : 255u) : uint8(x);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // TABLE ZIGZAG
    // kDeZig[k]  → index naturel (ligne-major 8x8) depuis l'indice zigzag k
    // kZigFwd[n] → indice zigzag depuis l'index naturel n
    // ═══════════════════════════════════════════════════════════════════════════
    static const uint8 kDeZig[64 + 15] = {
         0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
        12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
        // padding pour éviter les débordements lors de run=15 sur k=63
        63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63
    };
    static const uint8 kZigFwd[64] = {
         0,  1,  5,  6, 14, 15, 27, 28,
         2,  4,  7, 13, 16, 26, 29, 42,
         3,  8, 12, 17, 25, 30, 41, 43,
         9, 11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // TABLES QUANTIFICATION JFIF (ITU-T.81 Annexe K) — ordre naturel
    // ═══════════════════════════════════════════════════════════════════════════
    static const uint8 kQLuma[64] = {
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68,109,103, 77,
        24, 35, 55, 64, 81,104,113, 92,
        49, 64, 78, 87,103,121,120,101,
        72, 92, 95, 98,112,100,103, 99
    };
    static const uint8 kQChroma[64] = {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // TABLES HUFFMAN JFIF STANDARD (ITU-T.81 Annexe K)
    // ═══════════════════════════════════════════════════════════════════════════
    // bits[i] = nombre de codes de longueur i (indices 1..16, bits[0] ignoré)
    static const uint8 kDCLumaBits[17]   = {0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
    static const uint8 kDCLumaVals[12]   = {0,1,2,3,4,5,6,7,8,9,10,11};
    static const uint8 kDCChromaBits[17] = {0,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
    static const uint8 kDCChromaVals[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    static const uint8 kACLumaBits[17]   = {0,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125};
    static const uint8 kACChromaBits[17] = {0,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119};

    static const uint8 kACLumaVals[162] = {
        0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
        0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,
        0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
        0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
        0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
        0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
        0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
        0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,
        0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
        0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
        0xF9,0xFA
    };
    static const uint8 kACChromaVals[162] = {
        0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
        0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,
        0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,
        0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,
        0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,
        0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
        0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,
        0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,
        0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
        0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
        0xF9,0xFA
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // IDCT — Loeffler-Ligtenberg-Moschytz, portage exact de stbi__idct_block
    // Entrée  : coeff[64] en ordre naturel (ligne-major), déjà dequantifiés
    // Sortie  : bloc 8×8 uint8, stride = stride_bytes
    // ═══════════════════════════════════════════════════════════════════════════
    #define JPEG_F2F(x)  ((int32)(((x)*4096.0f + 0.5f)))
    #define JPEG_FSH(x)  ((x) << 12)

    // Macro IDCT 1D sur 8 valeurs (utilisée deux fois : colonnes puis lignes)
    #define IDCT_1D(s0,s1,s2,s3,s4,s5,s6,s7)                                   \
        int32 t0,t1,t2,t3,p1,p2,p3,p4,p5,x0,x1,x2,x3;                         \
        p2 = s2; p3 = s6;                                                        \
        p1 = (p2 + p3) * JPEG_F2F(0.5411961f);                                  \
        t2 = p1 + p3 * JPEG_F2F(-1.847759065f);                                 \
        t3 = p1 + p2 * JPEG_F2F( 0.765366865f);                                 \
        p2 = s0; p3 = s4;                                                        \
        t0 = JPEG_FSH(p2 + p3); t1 = JPEG_FSH(p2 - p3);                        \
        x0 = t0 + t3; x3 = t0 - t3; x1 = t1 + t2; x2 = t1 - t2;              \
        t0 = s7; t1 = s5; t2 = s3; t3 = s1;                                     \
        p3 = t0 + t2; p4 = t1 + t3; p1 = t0 + t3; p2 = t1 + t2;               \
        p5 = (p3 + p4) * JPEG_F2F(1.175875602f);                                \
        t0 *= JPEG_F2F(0.298631336f); t1 *= JPEG_F2F(2.053119869f);             \
        t2 *= JPEG_F2F(3.072711026f); t3 *= JPEG_F2F(1.501321110f);             \
        p1 = p5 + p1 * JPEG_F2F(-0.899976223f);                                 \
        p2 = p5 + p2 * JPEG_F2F(-2.562915447f);                                 \
        p3 *= JPEG_F2F(-1.961570560f); p4 *= JPEG_F2F(-0.390180644f);           \
        t3 += p1 + p4; t2 += p2 + p3; t1 += p2 + p4; t0 += p1 + p3;

    static void jIDCT(uint8* out, int32 stride, const short data[64]) noexcept {
        int32  val[64];
        int32* v = val;
        const short* d = data;

        // Passe 1 : IDCT sur chaque colonne (8 colonnes)
        for (int32 i = 0; i < 8; ++i, ++d, ++v) {
            // Optimisation : si tous les coefficients AC de la colonne sont nuls
            if (d[8]==0 && d[16]==0 && d[24]==0 && d[32]==0 &&
                d[40]==0 && d[48]==0 && d[56]==0) {
                int32 dc = d[0] << 2;
                v[0]=v[8]=v[16]=v[24]=v[32]=v[40]=v[48]=v[56] = dc;
            } else {
                IDCT_1D(d[0],d[8],d[16],d[24],d[32],d[40],d[48],d[56])
                x0 += 512; x1 += 512; x2 += 512; x3 += 512;
                v[ 0] = (x0 + t3) >> 10; v[56] = (x0 - t3) >> 10;
                v[ 8] = (x1 + t2) >> 10; v[48] = (x1 - t2) >> 10;
                v[16] = (x2 + t1) >> 10; v[40] = (x2 - t1) >> 10;
                v[24] = (x3 + t0) >> 10; v[32] = (x3 - t0) >> 10;
            }
        }

        // Passe 2 : IDCT sur chaque ligne (8 lignes)
        v = val;
        uint8* o = out;
        for (int32 i = 0; i < 8; ++i, v += 8, o += stride) {
            IDCT_1D(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7])
            // biais = 65536 + 128<<17 pour le level shift (+128) et l'arrondi
            x0 += 65536 + (128 << 17); x1 += 65536 + (128 << 17);
            x2 += 65536 + (128 << 17); x3 += 65536 + (128 << 17);
            o[0] = jClamp((x0 + t3) >> 17); o[7] = jClamp((x0 - t3) >> 17);
            o[1] = jClamp((x1 + t2) >> 17); o[6] = jClamp((x1 - t2) >> 17);
            o[2] = jClamp((x2 + t1) >> 17); o[5] = jClamp((x2 - t1) >> 17);
            o[3] = jClamp((x3 + t0) >> 17); o[4] = jClamp((x3 - t0) >> 17);
        }
    }
    #undef IDCT_1D
    #undef JPEG_F2F
    #undef JPEG_FSH

    // ═══════════════════════════════════════════════════════════════════════════
    // TABLE HUFFMAN DECODEUR — portage exact de stbi__build_huffman
    // Flux MSB-first, LUT rapide sur HUFF_FAST bits
    // ═══════════════════════════════════════════════════════════════════════════
    enum { HUFF_FAST = 9 };

    struct HuffDec {
        uint8  fast[1 << HUFF_FAST]; // LUT rapide: index = code MSB-aligné
        uint16 code[256];            // codes canoniques
        uint8  vals[256];            // symboles correspondants
        uint8  size[257];            // longueur de chaque code (size[n] = bits du n-ième code)
        uint32 maxcode[18];          // maxcode[l] = code max pour longueur l, pré-shifté à 16 bits
        int32  delta[17];            // delta[l] = firstsym[l] - firstcode[l]
        int32  nSyms;                // nombre total de symboles

        bool Build(const uint8* bits, const uint8* vIn, int32 nv) noexcept {
            nSyms = nv;
            if (nv > 256) return false;
            jCp(vals, vIn, nv);
            jSt(fast, 255, sizeof(fast));

            // Construit la liste des longueurs dans l'ordre des symboles
            int32 k = 0;
            for (int32 i = 1; i <= 16; ++i)
                for (int32 j = 0; j < int32(bits[i]); ++j)
                    size[k++] = uint8(i);
            size[k] = 0; // sentinelle

            // Construit les codes canoniques MSB-first
            int32 c = 0; k = 0;
            for (int32 j = 1; j <= 16; ++j) {
                delta[j] = k - c;
                if (size[k] == j) {
                    while (size[k] == j)
                        code[k++] = uint16(c++);
                }
                if (c - 1 >= (1 << j)) return false; // table invalide
                maxcode[j] = uint32(c) << (16 - j);  // pré-shifté pour comparaison rapide
                c <<= 1;
            }
            maxcode[17] = 0xFFFFFFFFu;

            // Remplit la LUT rapide
            for (int32 i = 0; i < k; ++i) {
                int32 s = size[i];
                if (s <= HUFF_FAST) {
                    int32 c2 = int32(code[i]) << (HUFF_FAST - s);
                    int32 m  = 1 << (HUFF_FAST - s);
                    for (int32 j = 0; j < m; ++j)
                        fast[c2 + j] = uint8(i);
                }
            }
            return true;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // LECTEUR DE BITS JPEG — portage exact de stbi__grow_buffer_unsafe
    //
    // BUG A CORRIGÉ : marker trouvé → return SANS toucher buf
    // BUG B CORRIGÉ : get8() lit ET avance (pos++)
    // BUG D CORRIGÉ : sgn = int32(buf) >> 31  (arithmétique)
    // ═══════════════════════════════════════════════════════════════════════════
    struct JBits {
        const uint8* data;
        usize        size;
        usize        pos;
        uint32       buf;
        int32        cnt;
        bool         err;
        uint8        marker;
        bool         noMore;

        void Init(const uint8* d, usize s) noexcept {
            data = d; size = s; pos = 0;
            buf = 0; cnt = 0; err = false;
            marker = 0; noMore = false;
        }

        // Lit un octet et avance (stbi__get8)
        NKIMG_INLINE uint32 get8() noexcept {
            return (pos < size) ? uint32(data[pos++]) : 0u;
        }

        // stbi__grow_buffer_unsafe — remplit buf (MSB-first) en gérant byte-stuffing et markers
        void Grow() noexcept {
            do {
                uint32 b = noMore ? 0u : get8();

                if (b == 0xFF) {
                    // Consomme les bytes de remplissage (padding JPEG)
                    uint32 c = get8();
                    while (c == 0xFF) c = get8();

                    if (c != 0x00) {
                        // BUG A CORRIGÉ : marker valide → on s'arrête SANS injecter dans buf
                        marker  = uint8(c);
                        noMore  = true;
                        return;
                    }
                    // c == 0x00 : byte-stuffing, b = 0xFF est une donnée réelle
                }

                // Accumule b dans buf MSB-first (stbi: buf |= b << (24 - cnt))
                buf |= b << (24 - cnt);
                cnt += 8;
            } while (cnt <= 24);
        }

        // stbi__jpeg_huff_decode — décode un symbole Huffman MSB-first
        int32 DecodeHuff(const HuffDec& h) noexcept {
            if (cnt < 16) Grow();
            if (err) return -1;

            // Chemin rapide : HUFF_FAST bits MSB
            int32 c = int32(buf >> (32 - HUFF_FAST)) & ((1 << HUFF_FAST) - 1);
            int32 k = int32(h.fast[c]);
            if (k < 255) {
                int32 s = int32(h.size[k]);
                if (s > cnt) { err = true; return -1; }
                buf <<= s; cnt -= s;
                return int32(h.vals[k]);
            }

            // Chemin lent : comparaison avec maxcode[] pré-shifté
            uint32 tmp = buf >> 16;
            int32  len;
            for (len = HUFF_FAST + 1; ; ++len)
                if (tmp < h.maxcode[len]) break;
            if (len == 17) { err = true; return -1; }
            if (len >  cnt){ err = true; return -1; }

            c = int32((buf >> (32 - len)) & 0xFFFFu) + h.delta[len];
            if (c < 0 || c >= h.nSyms) { err = true; return -1; }
            buf <<= len; cnt -= len;
            return int32(h.vals[c]);
        }

        // stbi__extend_receive — extrait n bits MSB-first et retourne la valeur signée
        // BUG D CORRIGÉ : int32 sgn = int32(buf) >> 31
        int32 ExtendReceive(int32 n) noexcept {
            if (n == 0) return 0;
            if (cnt < n) Grow();
            if (err)     return 0;

            // Bit de signe = MSB du buffer (arithmétique right-shift)
            int32  sgn = int32(buf) >> 31;
            uint32 k   = buf >> (32 - n);
            buf <<= n; cnt -= n;

            // Biais négatif pour les valeurs dont le bit de signe est 0
            // bias[n] = -(2^n - 1) = 1 - 2^n
            static const int32 bias[17] = {
                0, -1, -3, -7, -15, -31, -63, -127,
                -255, -511, -1023, -2047, -4095, -8191, -16383, -32767, -65535
            };
            return int32(k) + (bias[n] & ~sgn);
        }

        // Avance pos après un scan pour retrouver la position dans le flux d'origine
        // (nécessaire après décodage d'un scan pour pointer après les données entropiques)
        usize CurrentBytePos() const noexcept {
            // Les bits non consommés dans buf correspondent à des octets déjà lus
            return pos - usize(cnt / 8);
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // CONVERSION YCbCr → RGB — portage exact de stbi__YCbCr_to_RGB_row (BT.601)
    // ═══════════════════════════════════════════════════════════════════════════
    #define YCC_F(x) ((int32)(((x) * 4096.0f + 0.5f)))

    static void YCbCr2RGB(uint8* out, const uint8* y,
                          const uint8* pcb, const uint8* pcr, int32 w) noexcept {
        for (int32 i = 0; i < w; ++i) {
            int32 yf = (int32(y[i]) << 20) + (1 << 19); // niveau + arrondi
            int32 cb = int32(pcb[i]) - 128;
            int32 cr = int32(pcr[i]) - 128;
            out[i * 3 + 0] = jClamp((yf + cr  *  YCC_F(1.40200f))                              >> 20);
            out[i * 3 + 1] = jClamp((yf + cr  * (-YCC_F(0.71414f)) + cb * (-YCC_F(0.34414f))) >> 20);
            out[i * 3 + 2] = jClamp((yf + cb  *  YCC_F(1.77200f))                              >> 20);
        }
    }
    #undef YCC_F

    // ═══════════════════════════════════════════════════════════════════════════
    // UPSAMPLING — portage exact de stbi__resample_row_*
    // ═══════════════════════════════════════════════════════════════════════════

    // Horizontal ×2 + Vertical ×2  (4:2:0 → 4:4:4)
    // n_ = ligne courante (rows chrominance), f_ = ligne suivante
    // w entrées → w*2 sorties
    static void Ups_hv2(uint8* out, const uint8* n_, const uint8* f_, int32 w) noexcept {
        if (w == 1) { out[0] = out[1] = uint8((3*n_[0] + f_[0] + 2) >> 2); return; }
        int32 t1 = 3 * n_[0] + f_[0];
        out[0] = uint8((t1 + 2) >> 2);
        for (int32 i = 1; i < w; ++i) {
            int32 t0 = t1;
            t1 = 3 * n_[i] + f_[i];
            out[i * 2 - 1] = uint8((3 * t0 + t1 + 8) >> 4);
            out[i * 2    ] = uint8((3 * t1 + t0 + 8) >> 4);
        }
        out[w * 2 - 1] = uint8((t1 + 2) >> 2);
    }

    // Horizontal ×2  (4:2:2 → 4:4:4 horizontal)
    static void Ups_h2(uint8* out, const uint8* n_, int32 w) noexcept {
        if (w == 1) { out[0] = out[1] = n_[0]; return; }
        out[0] = n_[0];
        out[1] = uint8((3 * n_[0] + n_[1] + 2) >> 2);
        for (int32 i = 1; i < w - 1; ++i) {
            int32 n = 3 * n_[i] + 2;
            out[i * 2    ] = uint8((n + n_[i - 1]) >> 2);
            out[i * 2 + 1] = uint8((n + n_[i + 1]) >> 2);
        }
        out[(w - 1) * 2    ] = uint8((3 * n_[w - 2] + n_[w - 1] + 2) >> 2);
        out[(w - 1) * 2 + 1] = n_[w - 1];
    }

    // Vertical ×2  (4:4:0 → 4:4:4 vertical)
    static void Ups_v2(uint8* out, const uint8* n_, const uint8* f_, int32 w) noexcept {
        for (int32 i = 0; i < w; ++i)
            out[i] = uint8((3 * n_[i] + f_[i] + 2) >> 2);
    }

    // Copie simple (1:1, pas de sous-échantillonnage)
    // BUG E CORRIGÉ : remplit exactement dstW pixels
    // Si la source est plus courte, répète le dernier pixel
    static void Ups_11(uint8* out, const uint8* n_, int32 srcW, int32 dstW) noexcept {
        int32 copy = (srcW < dstW) ? srcW : dstW;
        if (copy > 0) jCp(out, n_, usize(copy));
        for (int32 i = copy; i < dstW; ++i)
            out[i] = (copy > 0) ? n_[copy - 1] : 128u;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // STRUCTURES INTERNES DU DÉCODEUR
    // ═══════════════════════════════════════════════════════════════════════════
    struct JComp {
        uint8  id    = 0;
        uint8  hSamp = 1, vSamp = 1;
        uint8  qtId  = 0;
        uint8  dcId  = 0, acId = 0;
        int32  dcPred = 0;
        uint8* data   = nullptr;
        int32  w2 = 0, h2 = 0; // dimensions du plan alloué (multiples de 8*hSamp/vSamp)
    };

    struct JDec {
        int32   w = 0, h = 0;
        int32   nComp = 0;
        uint16  qtab[4][64] = {};  // tables de quant en ordre NATUREL
        HuffDec htDC[4], htAC[4];
        JComp   comps[4] = {};
        bool    progressive = false;
        bool    valid = false;
    };

    static void jCleanup(JDec& d) noexcept {
        for (int32 i = 0; i < 4; ++i) {
            jF(d.comps[i].data);
            d.comps[i].data = nullptr;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // LECTURE EXIF — orientation uniquement
    // Retourne la valeur du tag 0x0112 (Orientation), 1 si absent
    // BUG K CORRIGÉ : lecture SHORT (2 octets) ou LONG (4 octets) selon le type
    // ═══════════════════════════════════════════════════════════════════════════
    static int32 ReadExifOrientation(NkImageStream& s, usize segEnd) noexcept {
        int32 ori = 1;
        const usize segStart = s.Tell();

        // En-tête Exif : "Exif\0\0"
        if (segStart + 8 > segEnd) return ori;
        uint8 hdr[6]; s.ReadBytes(hdr, 6);
        if (hdr[0]!='E'||hdr[1]!='x'||hdr[2]!='i'||hdr[3]!='f'||hdr[4]||hdr[5])
            return ori;

        // En-tête TIFF
        const usize tiffBase = s.Tell();
        if (tiffBase + 8 > segEnd) return ori;
        uint8 ord[2]; s.ReadBytes(ord, 2);
        bool le = (ord[0] == 'I' && ord[1] == 'I');
        bool be = (ord[0] == 'M' && ord[1] == 'M');
        if (!le && !be) return ori;

        // Magic TIFF
        uint16 magic = le ? s.ReadU16LE() : s.ReadU16BE();
        if (magic != 42) return ori;

        // Offset du premier IFD
        uint32 ifd0 = le ? s.ReadU32LE() : s.ReadU32BE();
        if (tiffBase + ifd0 + 2 > segEnd) return ori;
        s.Seek(tiffBase + usize(ifd0));

        uint16 ne = le ? s.ReadU16LE() : s.ReadU16BE();
        for (uint16 e = 0; e < ne && !s.HasError(); ++e) {
            if (s.Tell() + 12 > segEnd) break;
            uint16 tag  = le ? s.ReadU16LE() : s.ReadU16BE();
            uint16 type = le ? s.ReadU16LE() : s.ReadU16BE();
            /*uint32 cnt =*/ le ? s.ReadU32LE() : s.ReadU32BE(); // count (ignoré ici)
            uint32 val;
            if (type == 3) { // SHORT (2 octets, + 2 octets de padding)
                val = le ? s.ReadU16LE() : s.ReadU16BE();
                le ? s.ReadU16LE() : s.ReadU16BE(); // padding
            } else {         // LONG ou autre (4 octets)
                val = le ? s.ReadU32LE() : s.ReadU32BE();
            }
            if (tag == 0x0112) { ori = int32(val & 0xFF); break; }
        }
        return ori;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // NkJPEGCodec::Decode
    // ═══════════════════════════════════════════════════════════════════════════
    NkImage* NkJPEGCodec::Decode(const uint8* data, usize size) noexcept {
        if (!data || size < 4 || data[0] != 0xFF || data[1] != 0xD8) return nullptr;

        JDec dec;
        NkImageStream s(data, size);
        s.Skip(2); // SOI

        int32 hMax = 1, vMax = 1;
        int32 exifOri = 1;
        NkImage* result = nullptr;

        while (!s.IsEOF() && !s.HasError()) {
            // Synchronisation sur un marqueur JPEG (commence par 0xFF)
            uint8 b = s.ReadU8();
            if (b != 0xFF) continue;
            uint8 mk = s.ReadU8();
            if (mk == 0x00 || mk == 0xFF) continue; // byte-stuffing ou fill
            if (mk == 0xD9) break;                  // EOI

            // RST markers (0xD0..0xD7) : pas de payload, déjà gérés dans JBits
            if (mk >= 0xD0 && mk <= 0xD7) continue;

            // Lecture de la longueur du segment (inclut les 2 octets de longueur)
            if (s.IsEOF()) break;
            const uint16 segLen = s.ReadU16BE();
            if (segLen < 2) break;
            const usize segEnd = s.Tell() + usize(segLen - 2);
            if (segEnd > s.Size()) break;

            // ─── SOF0 : Start of Frame 0 (Baseline DCT) ────────────────────
            if (mk == 0xC0) {
                (void)s.ReadU8(); // précision (toujours 8 pour baseline)
                dec.h = s.ReadU16BE();
                dec.w = s.ReadU16BE();
                dec.nComp = s.ReadU8();

                if (dec.w <= 0 || dec.h <= 0 || dec.nComp < 1 || dec.nComp > 4) {
                    s.Seek(segEnd); continue;
                }

                hMax = 1; vMax = 1;
                for (int32 i = 0; i < dec.nComp && i < 4; ++i) {
                    dec.comps[i].id   = s.ReadU8();
                    uint8 sf = s.ReadU8();
                    dec.comps[i].hSamp = (sf >> 4) & 0xF;
                    dec.comps[i].vSamp = sf & 0xF;
                    dec.comps[i].qtId  = s.ReadU8();

                    // BUG J CORRIGÉ : hSamp/vSamp == 0 interdit
                    if (dec.comps[i].hSamp < 1) dec.comps[i].hSamp = 1;
                    if (dec.comps[i].vSamp < 1) dec.comps[i].vSamp = 1;

                    if (dec.comps[i].hSamp > hMax) hMax = dec.comps[i].hSamp;
                    if (dec.comps[i].vSamp > vMax) vMax = dec.comps[i].vSamp;
                }
                dec.valid = true;
            }
            // ─── SOF2 : Progressif (non supporté, signalé) ─────────────────
            else if (mk == 0xC2) {
                dec.progressive = true;
                s.Seek(segEnd); continue;
            }
            // ─── DQT : Define Quantization Table ───────────────────────────
            else if (mk == 0xDB) {
                while (s.Tell() + 1 <= segEnd && !s.HasError()) {
                    uint8 pqtq = s.ReadU8();
                    uint8 pq   = (pqtq >> 4) & 0xF; // précision : 0=8bit, 1=16bit
                    uint8 tq   = pqtq & 0xF;         // numéro de table (0..3)
                    if (tq >= 4) { s.Seek(segEnd); break; }

                    // BUG I CORRIGÉ : stockage en position NATURELLE via kDeZig
                    // kDeZig[i] donne la position naturelle de l'élément zigzag i
                    if (pq == 0) {
                        for (int32 i = 0; i < 64; ++i)
                            dec.qtab[tq][kDeZig[i]] = s.ReadU8();
                    } else {
                        for (int32 i = 0; i < 64; ++i)
                            dec.qtab[tq][kDeZig[i]] = s.ReadU16BE();
                    }
                }
            }
            // ─── DHT : Define Huffman Table ─────────────────────────────────
            else if (mk == 0xC4) {
                while (s.Tell() + 17 <= segEnd && !s.HasError()) {
                    uint8 tcid = s.ReadU8();
                    uint8 tc   = (tcid >> 4) & 0xF; // 0=DC, 1=AC
                    uint8 id   = tcid & 0xF;         // numéro de table (0..3)

                    uint8 bits[17] = {}; int32 total = 0;
                    for (int32 i = 1; i <= 16; ++i) { bits[i] = s.ReadU8(); total += bits[i]; }
                    if (total < 0 || total > 256) { s.Seek(segEnd); break; }

                    uint8 hv[256] = {};
                    s.ReadBytes(hv, usize(total));

                    if (id < 4) {
                        if (tc == 0) dec.htDC[id].Build(bits, hv, total);
                        else         dec.htAC[id].Build(bits, hv, total);
                    }
                }
            }
            // ─── APP1 : EXIF ────────────────────────────────────────────────
            else if (mk == 0xE1) {
                usize sv = s.Tell();
                exifOri = ReadExifOrientation(s, segEnd);
                (void)sv;
            }
            // ─── SOS : Start of Scan ─────────────────────────────────────────
            else if (mk == 0xDA) {
                // Validation préalable
                if (!dec.valid || dec.progressive || dec.w <= 0 || dec.h <= 0) {
                    s.Seek(segEnd); continue;
                }

                // Lecture de l'en-tête SOS
                int32 ns = s.ReadU8();
                for (int32 i = 0; i < ns && !s.HasError(); ++i) {
                    uint8 cid   = s.ReadU8();
                    uint8 tdta  = s.ReadU8();
                    for (int32 c = 0; c < dec.nComp && c < 4; ++c) {
                        if (dec.comps[c].id == cid) {
                            dec.comps[c].dcId = (tdta >> 4) & 0xF;
                            dec.comps[c].acId =  tdta & 0xF;
                        }
                    }
                }
                s.Skip(3); // Ss, Se, Ah/Al (spectre et approximation, baseline = 0/63/0)

                const int32 w    = dec.w;
                const int32 h    = dec.h;
                const int32 nC   = dec.nComp;
                const int32 mBW  = 8 * hMax; // largeur d'un MCU en pixels
                const int32 mBH  = 8 * vMax; // hauteur d'un MCU en pixels
                const int32 mCols = (w + mBW - 1) / mBW;
                const int32 mRows = (h + mBH - 1) / mBH;

                // Allocation des plans de composantes
                bool allocOK = true;
                for (int32 ci = 0; ci < nC && ci < 4; ++ci) {
                    JComp& c = dec.comps[ci];
                    c.w2 = mCols * c.hSamp * 8;
                    c.h2 = mRows * c.vSamp * 8;
                    jF(c.data);
                    c.data = static_cast<uint8*>(jM(usize(c.w2) * usize(c.h2)));
                    if (!c.data) { allocOK = false; break; }
                    // Y initialisé à 0, Cb/Cr initialisés à 128 (gris neutre)
                    jSt(c.data, (ci == 0) ? 0 : 128, usize(c.w2) * usize(c.h2));
                }
                if (!allocOK) { jCleanup(dec); return nullptr; }

                // Initialisation du BitReader sur les données entropiques
                const usize bStart = s.Tell();
                JBits br;
                br.Init(data + bStart, size - bStart);

                // ─── Décodage MCU par MCU ────────────────────────────────────
                for (int32 mr = 0; mr < mRows && !br.err; ++mr) {
                    for (int32 mc = 0; mc < mCols && !br.err; ++mc) {

                        // BUG H CORRIGÉ : gestion des RST markers
                        // Après le décodage d'un MCU, br.marker peut contenir un RST
                        if (br.noMore && br.marker >= 0xD0 && br.marker <= 0xD7) {
                            // Réinitialise les predicteurs DC et le bitreader
                            for (int32 ci = 0; ci < nC && ci < 4; ++ci)
                                dec.comps[ci].dcPred = 0;
                            br.noMore = false;
                            br.marker = 0;
                            br.buf    = 0;
                            br.cnt    = 0;
                        }

                        for (int32 ci = 0; ci < nC && ci < 4 && !br.err; ++ci) {
                            JComp&        comp = dec.comps[ci];
                            const uint16* qt   = dec.qtab[comp.qtId < 4 ? comp.qtId : 0];
                            const HuffDec& hdc = dec.htDC[comp.dcId < 4 ? comp.dcId : 0];
                            const HuffDec& hac = dec.htAC[comp.acId < 4 ? comp.acId : 0];

                            // Décode hSamp×vSamp blocs 8×8 pour cette composante
                            for (int32 vy = 0; vy < comp.vSamp && !br.err; ++vy) {
                                for (int32 vx = 0; vx < comp.hSamp && !br.err; ++vx) {

                                    // Coefficients DCT en ordre zigzag → stockés en ordre naturel
                                    short coeff[64] = {};

                                    // ── DC ──────────────────────────────────
                                    int32 dcs = br.DecodeHuff(hdc);
                                    if (dcs < 0 || dcs > 15) { br.err = true; break; }
                                    comp.dcPred += br.ExtendReceive(dcs);
                                    // qt[0] = facteur de dequant DC (position naturelle 0)
                                    coeff[0] = short(comp.dcPred * int32(qt[0]));

                                    // ── AC ──────────────────────────────────
                                    // BUG C CORRIGÉ :
                                    //   zig = kDeZig[k]  → position naturelle
                                    //   qt[zig]          → facteur de dequant correct
                                    for (int32 k = 1; k < 64; ) {
                                        int32 acs = br.DecodeHuff(hac);
                                        if (acs < 0) { br.err = true; break; }
                                        if (acs == 0x00) break;        // EOB
                                        if (acs == 0xF0) { k += 16; continue; } // ZRL (skip 16 zéros)

                                        int32 run  = (acs >> 4) & 0xF;
                                        int32 ssss =  acs & 0xF;

                                        k += run; // saute `run` zéros
                                        if (k >= 64) { br.err = true; break; }

                                        const uint8 zig = kDeZig[k]; // position naturelle
                                        coeff[zig] = short(br.ExtendReceive(ssss) * int32(qt[zig]));
                                        ++k;
                                    }
                                    if (br.err) break;

                                    // ── IDCT → plan de la composante ────────
                                    const int32 bx = (mc * comp.hSamp + vx) * 8;
                                    const int32 by = (mr * comp.vSamp + vy) * 8;

                                    // Bloc temporaire 8×8
                                    uint8 block[64];
                                    jIDCT(block, 8, coeff);

                                    // Copie ligne par ligne dans le plan
                                    const int32 copyW = (bx + 8 <= comp.w2) ? 8 : comp.w2 - bx;
                                    if (copyW > 0) {
                                        for (int32 iy = 0; iy < 8; ++iy) {
                                            const int32 py = by + iy;
                                            if (py >= comp.h2) break;
                                            jCp(comp.data + py * comp.w2 + bx,
                                                block + iy * 8, usize(copyW));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ─── Reconstruction de l'image finale ───────────────────────
                const NkImagePixelFormat fmt =
                    (nC == 1) ? NkImagePixelFormat::NK_GRAY8 : NkImagePixelFormat::NK_RGB24;
                NkImage* img = NkImage::Alloc(w, h, fmt);
                if (!img) { jCleanup(dec); return nullptr; }

                if (nC == 1) {
                    // Niveaux de gris : copie directe
                    const int32 sw = dec.comps[0].w2;
                    for (int32 y = 0; y < h; ++y) {
                        const uint8* src2 = dec.comps[0].data + y * sw;
                        jCp(img->RowPtr(y), src2, usize(w < sw ? w : sw));
                    }
                } else {
                    // YCbCr → RGB avec upsampling
                    const int32 cbW  = dec.comps[1].w2;
                    const int32 cbH  = dec.comps[1].h2;
                    const int32 crW  = dec.comps[2].w2;
                    const int32 crH  = dec.comps[2].h2;

                    // Facteurs d'upsampling pour Cb et Cr
                    const int32 hs1 = hMax / (dec.comps[1].hSamp > 0 ? dec.comps[1].hSamp : 1);
                    const int32 vs1 = vMax / (dec.comps[1].vSamp > 0 ? dec.comps[1].vSamp : 1);
                    const int32 hs2 = hMax / (dec.comps[2].hSamp > 0 ? dec.comps[2].hSamp : 1);
                    const int32 vs2 = vMax / (dec.comps[2].vSamp > 0 ? dec.comps[2].vSamp : 1);

                    // BUG 4 CORRIGÉ : buffer upsampling assez grand
                    // Ups_hv2 écrit jusqu'à max(cbW,crW)*2 pixels
                    const int32 maxCW = (cbW > crW ? cbW : crW);
                    const int32 lsz   = (maxCW * 2) + 16; // marge de sécurité
                    uint8* lCb = static_cast<uint8*>(jM(usize(lsz)));
                    uint8* lCr = static_cast<uint8*>(jM(usize(lsz)));
                    if (!lCb || !lCr) {
                        jF(lCb); jF(lCr);
                        img->Free();
                        jCleanup(dec);
                        return nullptr;
                    }

                    for (int32 y = 0; y < h; ++y) {
                        uint8*       row = img->RowPtr(y);
                        const uint8* yp  = dec.comps[0].data + y * dec.comps[0].w2;

                        // ── Upsampling Cb ────────────────────────────────────
                        {
                            const int32  cy = y / vs1;
                            const uint8* n0 = dec.comps[1].data + cy * cbW;
                            if (hs1 == 2 && vs1 == 2) {
                                const uint8* f0 = (cy + 1 < cbH)
                                    ? dec.comps[1].data + (cy + 1) * cbW : n0;
                                Ups_hv2(lCb, n0, f0, cbW);
                            } else if (hs1 == 2) {
                                Ups_h2(lCb, n0, cbW);
                            } else if (vs1 == 2) {
                                const uint8* f0 = (cy + 1 < cbH)
                                    ? dec.comps[1].data + (cy + 1) * cbW : n0;
                                Ups_v2(lCb, n0, f0, cbW);
                            } else {
                                // BUG E CORRIGÉ : remplissage jusqu'à w pixels
                                Ups_11(lCb, n0, cbW, w);
                            }
                        }

                        // ── Upsampling Cr ────────────────────────────────────
                        {
                            const int32  cy = y / vs2;
                            const uint8* n0 = dec.comps[2].data + cy * crW;
                            if (hs2 == 2 && vs2 == 2) {
                                const uint8* f0 = (cy + 1 < crH)
                                    ? dec.comps[2].data + (cy + 1) * crW : n0;
                                Ups_hv2(lCr, n0, f0, crW);
                            } else if (hs2 == 2) {
                                Ups_h2(lCr, n0, crW);
                            } else if (vs2 == 2) {
                                const uint8* f0 = (cy + 1 < crH)
                                    ? dec.comps[2].data + (cy + 1) * crW : n0;
                                Ups_v2(lCr, n0, f0, crW);
                            } else {
                                Ups_11(lCr, n0, crW, w);
                            }
                        }

                        // BUG 3 CORRIGÉ : YCbCr2RGB sans masque erroné sur G
                        YCbCr2RGB(row, yp, lCb, lCr, w);
                    }

                    jF(lCb);
                    jF(lCr);
                }

                jCleanup(dec);

                // ─── Correction d'orientation EXIF ──────────────────────────
                // Valeurs standard EXIF (TIFF/EP, tag 0x0112) :
                //  1 = Normal         2 = Miroir H       3 = 180°
                //  4 = Miroir V       5 = Miroir H + 90° 6 = 90° CW
                //  7 = Miroir V + 90° 8 = 270° CW
                switch (exifOri) {
                    case 2: img->FlipHorizontal(); break;
                    case 3: img->FlipVertical(); img->FlipHorizontal(); break;
                    case 4: img->FlipVertical(); break;
                    // Les cas 5-8 (rotations 90°/270°) nécessitent une transposition
                    // qui n'est pas encore implémentée dans NkImage → on laisse tel quel
                    default: break;
                }

                result = img;
                break; // Un seul scan pour le baseline
            }

            // Avance jusqu'à la fin du segment courant
            if (s.Tell() < segEnd) s.Seek(segEnd);
        }

        if (!result) jCleanup(dec);
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ENCODEUR JPEG BASELINE
    // FDCT AAN + Huffman JFIF standard + chrominance 4:2:0
    // ═══════════════════════════════════════════════════════════════════════════

    // ─── FDCT 8×8 (algorithme AAN — Arai, Agui, Nakajima) ──────────────────
    // Entrée  : bloc uint8 8×8 (stride = str) — valeurs [0,255]
    // Sortie  : out[64] en ordre ZIGZAG, quantifiés et arrondis
    // qt[]    : table de quantification en ordre NATUREL
    static void FDCT8x8(const uint8* blk, int32 str, int32* out, const uint8* qt) noexcept {
        // Passe 1 : FDCT sur les lignes → tmp[64] (virgule flottante)
        float32 tmp[64];
        for (int32 i = 0; i < 8; ++i) {
            const uint8* r = blk + i * str;
            float32 s0=r[0]+r[7], s1=r[1]+r[6], s2=r[2]+r[5], s3=r[3]+r[4];
            float32 s4=r[3]-r[4], s5=r[2]-r[5], s6=r[1]-r[6], s7=r[0]-r[7];
            float32 p0=s0+s3, p1=s1+s2, p2=s1-s2, p3=s0-s3;
            tmp[i*8+0] = (p0+p1) * 0.353553391f;
            tmp[i*8+4] = (p0-p1) * 0.353553391f;
            tmp[i*8+2] = p2 * 0.461939766f + p3 * 0.191341716f;
            tmp[i*8+6] = p3 * 0.461939766f - p2 * 0.191341716f;
            float32 q0=s4+s7, q1=s5+s6, q2=s5-s6, q3=s4-s7;
            float32 r0=(q0+q1)*0.353553391f, r1=(q0-q1)*0.353553391f;
            float32 r2= q2  * 0.707106781f,  r3= q3  * 0.707106781f;
            tmp[i*8+1] = r0*0.980785280f - r1*0.195090322f;
            tmp[i*8+3] = r0*0.831469612f + r1*0.555570233f;
            tmp[i*8+5] = r3*0.980785280f + r2*0.195090322f;
            tmp[i*8+7] = r3*0.831469612f - r2*0.555570233f;
        }
        // Passe 2 : FDCT sur les colonnes → quantification → ordre zigzag
        for (int32 j = 0; j < 8; ++j) {
            float32 s0=tmp[j]+tmp[56+j], s1=tmp[8+j]+tmp[48+j];
            float32 s2=tmp[16+j]+tmp[40+j], s3=tmp[24+j]+tmp[32+j];
            float32 s4=tmp[24+j]-tmp[32+j], s5=tmp[16+j]-tmp[40+j];
            float32 s6=tmp[ 8+j]-tmp[48+j], s7=tmp[   j]-tmp[56+j];
            float32 p0=s0+s3, p1=s1+s2, p2=s1-s2, p3=s0-s3;
            float32 c[8];
            c[0] = (p0+p1) * 0.353553391f;
            c[4] = (p0-p1) * 0.353553391f;
            c[2] = p2 * 0.461939766f + p3 * 0.191341716f;
            c[6] = p3 * 0.461939766f - p2 * 0.191341716f;
            float32 q0=s4+s7, q1=s5+s6, q2=s5-s6, q3=s4-s7;
            float32 r0=(q0+q1)*0.353553391f, r1=(q0-q1)*0.353553391f;
            float32 r2= q2 * 0.707106781f,   r3= q3 * 0.707106781f;
            c[1] = r0*0.980785280f - r1*0.195090322f;
            c[3] = r0*0.831469612f + r1*0.555570233f;
            c[5] = r3*0.980785280f + r2*0.195090322f;
            c[7] = r3*0.831469612f - r2*0.555570233f;

            for (int32 i = 0; i < 8; ++i) {
                // Position naturelle dans le bloc 8×8 : row=i, col=j
                const int32 nat = i * 8 + j;
                const int32 q   = int32(qt[nat]);
                if (q <= 0) { out[kZigFwd[nat]] = 0; continue; }
                // Quantification avec arrondi correct (vers zéro)
                float32 v = c[i] / float32(q);
                out[kZigFwd[nat]] = int32(v >= 0.0f ? v + 0.5f : v - 0.5f);
            }
        }
    }

    // ─── Table Huffman pour l'encodeur ──────────────────────────────────────
    struct HuffEnc {
        uint32 code[256] = {};
        uint8  len [256] = {};

        void Build(const uint8* bits, const uint8* vals) noexcept {
            int32 c = 0, k = 0;
            for (int32 i = 1; i <= 16; ++i) {
                for (int32 j = 0; j < int32(bits[i]); ++j, ++k) {
                    code[vals[k]] = uint32(c);
                    len [vals[k]] = uint8(i);
                    ++c;
                }
                c <<= 1;
            }
        }
    };

    // ─── Écrivain de bits pour l'encodeur ───────────────────────────────────
    struct JBitW {
        NkImageStream* s;
        uint32 buf = 0;
        int32  cnt = 0;

        // Écrit `len` bits de `code` (MSB-first) avec byte-stuffing 0xFF→0xFF00
        void Write(uint32 code2, int32 len2) noexcept {
            if (!len2) return;
            buf = (buf << len2) | (code2 & ((1u << len2) - 1));
            cnt += len2;
            while (cnt >= 8) {
                cnt -= 8;
                uint8 by = uint8(buf >> cnt);
                s->WriteU8(by);
                if (by == 0xFF) s->WriteU8(0x00); // byte-stuffing
            }
        }

        // Vide les bits restants (rembourrage avec des 1)
        void Flush() noexcept {
            if (cnt > 0) {
                // Les bits de rembourrage doivent être à 1 (JPEG spec)
                uint8 by = uint8((buf << (8 - cnt)) | ((1u << (8 - cnt)) - 1));
                s->WriteU8(by);
                if (by == 0xFF) s->WriteU8(0x00);
                cnt = 0; buf = 0;
            }
        }
    };

    // ─── RGB → YCbCr (coefficients BT.601 entiers) ──────────────────────────
    static NKIMG_INLINE void RGB2YCbCr(uint8 R, uint8 G, uint8 B,
                                        int32& Y, int32& Cb, int32& Cr) noexcept {
        // Coefficients ITU-T.81 / JFIF — BT.601
        Y  = ( 19595 * int32(R) + 38470 * int32(G) +  7471 * int32(B) + 32768) >> 16;
        Cb = ((-11056 * int32(R) - 21712 * int32(G) + 32768 * int32(B) + 32768) >> 16) + 128;
        Cr = (( 32768 * int32(R) - 27440 * int32(G) -  5328 * int32(B) + 32768) >> 16) + 128;
        Y  = Y  < 0 ? 0 : Y  > 255 ? 255 : Y;
        Cb = Cb < 0 ? 0 : Cb > 255 ? 255 : Cb;
        Cr = Cr < 0 ? 0 : Cr > 255 ? 255 : Cr;
    }

    // ─── Calcul du facteur d'échelle qualité ────────────────────────────────
    // Formule standard JFIF / libjpeg
    static NKIMG_INLINE int32 QualityScale(int32 q) noexcept {
        q = q < 1 ? 1 : (q > 100 ? 100 : q);
        return q < 50 ? (5000 / q) : (200 - q * 2);
    }

    // ─── Écriture d'une table DHT dans le flux ───────────────────────────────
    static void WriteDHT(NkImageStream& st, uint8 id,
                          const uint8* bits, const uint8* vals) noexcept {
        // Compte le nombre total de symboles
        int32 nb = 0;
        for (int32 i = 1; i <= 16; ++i) nb += int32(bits[i]);
        // Longueur du segment DHT = 2 (length) + 1 (class/id) + 16 (bits) + nb (vals)
        st.WriteU16BE(0xFFC4);
        st.WriteU16BE(uint16(2 + 1 + 16 + nb));
        st.WriteU8(id);
        st.WriteBytes(bits + 1, 16); // bits[1..16]
        st.WriteBytes(vals, usize(nb));
    }

    // ─── Encodage d'un coefficient DC ───────────────────────────────────────
    static NKIMG_INLINE void EncodeDC(JBitW& bw, int32 v,
                                       const HuffEnc& he, int32& pred) noexcept {
        int32 d = v - pred;
        pred = v;

        // Catégorie = nombre de bits nécessaires pour représenter |d|
        int32 ss = 0, t = (d < 0) ? -d : d;
        while (t) { ++ss; t >>= 1; }
        if (ss > 15) ss = 15; // protection (ne devrait pas arriver en baseline)

        bw.Write(he.code[ss], int32(he.len[ss]));
        if (ss > 0) {
            // Valeur de magnitude : si négatif → complément à 1
            uint32 b = (d < 0) ? uint32(d + (1 << ss) - 1) : uint32(d);
            bw.Write(b & ((1u << ss) - 1), ss);
        }
    }

    // ─── Encodage des coefficients AC ────────────────────────────────────────
    static void EncodeAC(JBitW& bw, const int32* cf, const HuffEnc& he) noexcept {
        int32 run = 0;
        for (int32 k = 1; k < 64; ++k) {
            int32 v = cf[k];
            if (v == 0) { ++run; continue; }

            // Émet des ZRL (0xF0) pour les runs de 16 zéros
            while (run >= 16) {
                bw.Write(he.code[0xF0], int32(he.len[0xF0]));
                run -= 16;
            }

            // Catégorie du coefficient AC
            int32 ss = 0, t = (v < 0) ? -v : v;
            while (t) { ++ss; t >>= 1; }
            if (ss > 10) ss = 10; // max catégorie AC JPEG = 10

            uint8 sym = uint8((run << 4) | ss);
            bw.Write(he.code[sym], int32(he.len[sym]));
            uint32 b = (v < 0) ? uint32(v + (1 << ss) - 1) : uint32(v);
            bw.Write(b & ((1u << ss) - 1), ss);
            run = 0;
        }
        // EOB si des zéros traînent en fin de bloc
        if (run > 0) bw.Write(he.code[0x00], int32(he.len[0x00]));
    }

    // ─── Extraction d'un bloc Y centré sur 0 ────────────────────────────────
    static void ExtractBlockY(const NkImage* src, int32 bx, int32 by,
                               uint8* blk, bool gray) noexcept {
        const int32 w  = src->Width();
        const int32 h  = src->Height();
        const int32 bpp = src->Channels();
        for (int32 y = 0; y < 8; ++y) {
            const int32 py = (by + y < h) ? by + y : h - 1;
            for (int32 x = 0; x < 8; ++x) {
                const int32 px = (bx + x < w) ? bx + x : w - 1;
                if (gray) {
                    blk[y * 8 + x] = src->RowPtr(py)[px];
                } else {
                    const uint8* p = src->RowPtr(py) + px * bpp;
                    int32 Y, Cb, Cr;
                    RGB2YCbCr(p[0], p[1], p[2], Y, Cb, Cr);
                    blk[y * 8 + x] = uint8(Y);
                }
            }
        }
    }

    // ─── Extraction d'un bloc Cb ou Cr (4:2:0 : moyenne 2×2) ───────────────
    static void ExtractBlockCbCr(const NkImage* src, int32 bx, int32 by,
                                  uint8* blk, int32 comp) noexcept {
        // comp : 1 = Cb, 2 = Cr
        const int32 w   = src->Width();
        const int32 h   = src->Height();
        const int32 bpp = src->Channels();
        for (int32 y = 0; y < 8; ++y) {
            for (int32 x = 0; x < 8; ++x) {
                int32 sum = 0;
                for (int32 dy = 0; dy < 2; ++dy) {
                    const int32 py = (by + y * 2 + dy < h) ? by + y * 2 + dy : h - 1;
                    for (int32 dx = 0; dx < 2; ++dx) {
                        const int32 px = (bx + x * 2 + dx < w) ? bx + x * 2 + dx : w - 1;
                        const uint8* p = src->RowPtr(py) + px * bpp;
                        int32 Y, Cb, Cr;
                        RGB2YCbCr(p[0], p[1], p[2], Y, Cb, Cr);
                        sum += (comp == 1) ? Cb : Cr;
                    }
                }
                blk[y * 8 + x] = uint8(sum / 4);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // NkJPEGCodec::Encode
    // Produit un JFIF baseline valide avec chrominance 4:2:0 (YCbCr)
    // ou niveaux de gris si l'image source est NK_GRAY8.
    // ═══════════════════════════════════════════════════════════════════════════
    bool NkJPEGCodec::Encode(const NkImage& img, uint8*& out,
                              usize& outSize, int32 quality) noexcept {
        if (!img.IsValid()) return false;

        // Conversion si nécessaire
        const NkImage* src  = &img;
        NkImage*       conv = nullptr;
        if (img.Format() != NkImagePixelFormat::NK_RGB24 &&
            img.Format() != NkImagePixelFormat::NK_GRAY8) {
            conv = img.Convert(NkImagePixelFormat::NK_RGB24);
            if (!conv) return false;
            src = conv;
        }

        const int32 w    = src->Width();
        const int32 h    = src->Height();
        const bool  gray = (src->Channels() == 1);

        // ─── Tables de quantification scalées selon la qualité ───────────────
        const int32 qs = QualityScale(quality);
        uint8 ql[64], qc[64];
        for (int32 i = 0; i < 64; ++i) {
            int32 v = (int32(kQLuma  [i]) * qs + 50) / 100;
            ql[i] = uint8(v < 1 ? 1 : (v > 255 ? 255 : v));
            v      = (int32(kQChroma[i]) * qs + 50) / 100;
            qc[i] = uint8(v < 1 ? 1 : (v > 255 ? 255 : v));
        }

        // ─── Tables Huffman encodeur ─────────────────────────────────────────
        HuffEnc heDCL, heDCC, heACL, heACC;
        heDCL.Build(kDCLumaBits,   kDCLumaVals);
        heDCC.Build(kDCChromaBits, kDCChromaVals);
        heACL.Build(kACLumaBits,   kACLumaVals);
        heACC.Build(kACChromaBits, kACChromaVals);

        // ─── Flux de sortie ──────────────────────────────────────────────────
        NkImageStream st;

        // SOI
        st.WriteU16BE(0xFFD8);

        // APP0 / JFIF
        st.WriteU16BE(0xFFE0);
        st.WriteU16BE(16); // longueur = 16
        {
            const uint8 jfif[] = {'J','F','I','F',0};
            st.WriteBytes(jfif, 5);
        }
        st.WriteU8(1);  // version majeure
        st.WriteU8(1);  // version mineure
        st.WriteU8(0);  // aspect ratio (sans unité)
        st.WriteU16BE(1); // Xdensity
        st.WriteU16BE(1); // Ydensity
        st.WriteU8(0);  // pas de miniature
        st.WriteU8(0);

        // DQT — tables de quantification (en ordre zigzag dans le fichier)
        auto writeDQT = [&](uint8 id, const uint8* qt) {
            st.WriteU16BE(0xFFDB);
            st.WriteU16BE(67); // 2 + 1 + 64
            st.WriteU8(id);
            for (int32 i = 0; i < 64; ++i)
                st.WriteU8(qt[kDeZig[i]]); // naturel → zigzag pour le fichier
        };
        writeDQT(0, ql);
        if (!gray) writeDQT(1, qc);

        // SOF0 — Start of Frame 0
        // BUG F CORRIGÉ : longueur = 2 + 1 + 2 + 2 + 1 + nComp*3
        const int32 nComp = gray ? 1 : 3;
        st.WriteU16BE(0xFFC0);
        st.WriteU16BE(uint16(8 + nComp * 3)); // = 11 (gray) ou 17 (YCbCr)
        st.WriteU8(8);                         // précision = 8 bits
        st.WriteU16BE(uint16(h));
        st.WriteU16BE(uint16(w));
        st.WriteU8(uint8(nComp));

        if (gray) {
            st.WriteU8(1);    // component ID
            st.WriteU8(0x11); // hSamp=1, vSamp=1
            st.WriteU8(0);    // quantization table 0
        } else {
            // Y  : 4:2:0 → hSamp=2, vSamp=2 (seulement pour Y, Cb et Cr restent 1:1)
            // Note : dans le standard JFIF 4:2:0 :
            //   Y  = 0x22 (hSamp=2, vSamp=2)
            //   Cb = 0x11 (hSamp=1, vSamp=1)
            //   Cr = 0x11 (hSamp=1, vSamp=1)
            st.WriteU8(1); st.WriteU8(0x22); st.WriteU8(0); // Y
            st.WriteU8(2); st.WriteU8(0x11); st.WriteU8(1); // Cb
            st.WriteU8(3); st.WriteU8(0x11); st.WriteU8(1); // Cr
        }

        // DHT — Tables Huffman
        WriteDHT(st, 0x00, kDCLumaBits,   kDCLumaVals);   // DC Luma
        WriteDHT(st, 0x10, kACLumaBits,   kACLumaVals);   // AC Luma
        if (!gray) {
            WriteDHT(st, 0x01, kDCChromaBits, kDCChromaVals); // DC Chroma
            WriteDHT(st, 0x11, kACChromaBits, kACChromaVals); // AC Chroma
        }

        // SOS — Start of Scan
        // BUG F CORRIGÉ : longueur = 2 + 1 + ns*2 + 3
        st.WriteU16BE(0xFFDA);
        st.WriteU16BE(uint16(6 + nComp * 2)); // = 8 (gray) ou 12 (YCbCr)
        st.WriteU8(uint8(nComp));
        if (gray) {
            st.WriteU8(1); st.WriteU8(0x00); // comp 1, DC=0, AC=0
        } else {
            st.WriteU8(1); st.WriteU8(0x00); // Y  : DC table 0, AC table 0
            st.WriteU8(2); st.WriteU8(0x11); // Cb : DC table 1, AC table 1
            st.WriteU8(3); st.WriteU8(0x11); // Cr : DC table 1, AC table 1
        }
        st.WriteU8(0);  // Ss = 0  (spectre DC)
        st.WriteU8(63); // Se = 63 (spectre AC complet)
        st.WriteU8(0);  // Ah=0, Al=0 (baseline)

        // ─── Encodage du scan entropique ────────────────────────────────────
        JBitW bw{&st};
        int32 dcP[3] = {0, 0, 0}; // prédicteurs DC par composante

        if (gray) {
            // Blocs de 8×8 en niveaux de gris
            const int32 mC = (w + 7) / 8;
            const int32 mR = (h + 7) / 8;
            for (int32 mr = 0; mr < mR; ++mr) {
                for (int32 mc = 0; mc < mC; ++mc) {
                    uint8 blk[64];
                    ExtractBlockY(src, mc * 8, mr * 8, blk, true);
                    int32 cf[64];
                    FDCT8x8(blk, 8, cf, ql);
                    EncodeDC(bw, cf[0], heDCL, dcP[0]);
                    EncodeAC(bw, cf,    heACL);
                }
            }
        } else {
            // YCbCr 4:2:0 : MCU = 16×16 pixels (4 blocs Y + 1 Cb + 1 Cr)
            // BUG G CORRIGÉ : ordre correct = Y00, Y10, Y01, Y11, Cb, Cr
            const int32 mC = (w + 15) / 16;
            const int32 mR = (h + 15) / 16;
            for (int32 mr = 0; mr < mR; ++mr) {
                for (int32 mc = 0; mc < mC; ++mc) {
                    const int32 ox = mc * 16; // origine X du MCU en pixels
                    const int32 oy = mr * 16; // origine Y du MCU en pixels

                    // 4 blocs Y (2×2 blocs de 8×8)
                    for (int32 vy = 0; vy < 2; ++vy) {
                        for (int32 vx = 0; vx < 2; ++vx) {
                            uint8 blk[64];
                            ExtractBlockY(src, ox + vx * 8, oy + vy * 8, blk, false);
                            int32 cf[64];
                            FDCT8x8(blk, 8, cf, ql);
                            EncodeDC(bw, cf[0], heDCL, dcP[0]);
                            EncodeAC(bw, cf,    heACL);
                        }
                    }

                    // 1 bloc Cb (sous-échantillonné 2×2)
                    {
                        uint8 blk[64];
                        ExtractBlockCbCr(src, ox, oy, blk, 1);
                        int32 cf[64];
                        FDCT8x8(blk, 8, cf, qc);
                        EncodeDC(bw, cf[0], heDCC, dcP[1]);
                        EncodeAC(bw, cf,    heACC);
                    }

                    // 1 bloc Cr (sous-échantillonné 2×2)
                    {
                        uint8 blk[64];
                        ExtractBlockCbCr(src, ox, oy, blk, 2);
                        int32 cf[64];
                        FDCT8x8(blk, 8, cf, qc);
                        EncodeDC(bw, cf[0], heDCC, dcP[2]);
                        EncodeAC(bw, cf,    heACC);
                    }
                }
            }
        }

        // Vide les bits restants (rembourrage à 1)
        bw.Flush();

        // EOI
        st.WriteU16BE(0xFFD9);

        // Libération de l'image convertie si nécessaire
        if (conv) conv->Free();

        // Transfert du buffer au appelant
        return st.TakeBuffer(out, outSize);
    }

} // namespace nkentseu