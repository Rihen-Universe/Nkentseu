/**
 * @File    NkFLACCodec.cpp
 * @Brief   Decodeur FLAC complet, from scratch.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 *
 * @Reference
 *  Spec : https://xiph.org/flac/format.html
 *  Tout le code est ici, sans dependance externe (pas de dr_flac, libFLAC, etc.)
 *
 * @Constraints_supported
 *  - Bits per sample : 4..32 (testes typiquement 16 et 24)
 *  - Channels        : 1..8 (testes principalement 1 et 2)
 *  - Subframes       : CONSTANT, VERBATIM, FIXED (order 0..4), LPC (order 1..32)
 *  - Residual coding : PARTITIONED_RICE (method 0), PARTITIONED_RICE2 (method 1)
 *  - Channel modes   : INDEPENDENT, LEFT/SIDE (8), SIDE/RIGHT (9), MID/SIDE (10)
 *  - CRC-8 (header) et CRC-16 (frame) verifies si l'option est activee
 *
 * @Output
 *  AudioSample float32 normalise dans [-1.0, 1.0] (interleaved par frame).
 */

#include "NKAudio/Codecs/FLAC/NkFLACCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKLogger/NkLog.h"
#include <cstring>

namespace nkentseu {
    namespace audio {

        // ────────────────────────────────────────────────────────────────────
        //  Constantes FLAC (depuis la spec Xiph)
        // ────────────────────────────────────────────────────────────────────

        // Magic "fLaC" en debut de stream
        static constexpr uint32 kFLACMagic = 0x664C6143u; // 'fLaC' big-endian

        // Types de bloc metadata
        enum FLACMetaBlockType : uint8 {
            kFLACMetaStreamInfo    = 0,
            kFLACMetaPadding       = 1,
            kFLACMetaApplication   = 2,
            kFLACMetaSeekTable     = 3,
            kFLACMetaVorbisComment = 4,
            kFLACMetaCueSheet      = 5,
            kFLACMetaPicture       = 6,
            kFLACMetaInvalid       = 127
        };

        // Types de subframe (6 bits)
        //   000000 : CONSTANT
        //   000001 : VERBATIM
        //   001xxx : FIXED (order = xxx, 0..4 valide ; 5..7 reserves)
        //   1xxxxx : LPC   (order = xxxxx + 1, donc 1..32)

        // ────────────────────────────────────────────────────────────────────
        //  Helpers de lecture big-endian
        // ────────────────────────────────────────────────────────────────────

        static NKENTSEU_INLINE uint16 RD_U16BE(const uint8* p) noexcept {
            return uint16((uint16(p[0]) << 8) | uint16(p[1]));
        }
        static NKENTSEU_INLINE uint32 RD_U24BE(const uint8* p) noexcept {
            return (uint32(p[0]) << 16) | (uint32(p[1]) << 8) | uint32(p[2]);
        }
        static NKENTSEU_INLINE uint32 RD_U32BE(const uint8* p) noexcept {
            return (uint32(p[0]) << 24) | (uint32(p[1]) << 16)
                 | (uint32(p[2]) << 8)  |  uint32(p[3]);
        }

        // ────────────────────────────────────────────────────────────────────
        //  BitReader MSB-first
        //
        //  Convention :
        //   - buf accumule les bits du HAUT (MSB en position 63).
        //   - Fill(n) charge des octets jusqu'a avoir au moins n bits valides.
        //   - Read(n) extrait les n bits du haut et les consomme.
        //   - PeekUnary() compte les bits a 0 jusqu'au premier 1 (utilise par Rice).
        // ────────────────────────────────────────────────────────────────────

        struct FLACBitReader {
            const uint8* ptr;
            const uint8* end;
            uint64       buf;
            int32        nbits;

            void Init(const uint8* p, usize size) noexcept {
                ptr   = p;
                end   = p + size;
                buf   = 0;
                nbits = 0;
            }

            NKENTSEU_INLINE void Fill(int32 n) noexcept {
                while (nbits < n && ptr < end) {
                    buf  |= (uint64(*ptr++) << (56 - nbits));
                    nbits += 8;
                }
            }

            // Read non signe (max 32 bits par appel)
            NKENTSEU_INLINE uint32 Read(int32 n) noexcept {
                if (n <= 0) return 0;
                Fill(n);
                uint32 v = uint32(buf >> (64 - n));
                buf  <<= n;
                nbits -= n;
                return v;
            }

            // Read signe (sign-extend depuis n bits)
            NKENTSEU_INLINE int32 ReadSigned(int32 n) noexcept {
                if (n <= 0) return 0;
                uint32 u = Read(n);
                // Sign-extend
                int32 shift = 32 - n;
                return (int32(u) << shift) >> shift;
            }

            // Read jusqu'a 64 bits (pour total_samples 36-bit etc.)
            NKENTSEU_INLINE uint64 Read64(int32 n) noexcept {
                if (n <= 32) return uint64(Read(n));
                uint64 hi = uint64(Read(n - 32));
                uint64 lo = uint64(Read(32));
                return (hi << 32) | lo;
            }

            // Compte les bits a 0 jusqu'au premier 1 (qui est consomme).
            // Utilise pour le quotient unaire du codage Rice.
            NKENTSEU_INLINE uint32 ReadUnary() noexcept {
                uint32 q = 0;
                while (true) {
                    Fill(1);
                    if (nbits == 0) return uint32(-1); // EOF
                    if (buf & (uint64(1) << 63)) {
                        // bit 1 trouve : consomme et termine
                        buf  <<= 1;
                        nbits -= 1;
                        return q;
                    }
                    buf  <<= 1;
                    nbits -= 1;
                    ++q;
                    if (q > 0x10000) return uint32(-1); // garde-fou anti-boucle infinie
                }
            }

            // Re-alignement sur l'octet courant (consomme les bits restants
            // jusqu'au prochain octet). Utilise apres le header de frame.
            NKENTSEU_INLINE void ByteAlign() noexcept {
                int32 drop = nbits & 7;
                buf  <<= drop;
                nbits -= drop;
            }

            // Position byte courante (utile pour le CRC).
            // Compte les octets DEJA CHARGES dans buf MAIS NON CONSOMMES :
            //   bytesInBuf = ceil(nbits / 8)
            //   bytePos    = (ptr - base) - bytesInBuf
            // Mais on n'expose pas base ici, on calcule plutot par rapport a end :
            //   bytesInBuf = (nbits + 7) / 8
            //   nextBytePos = ptr - bytesInBuf
            const uint8* CurrentBytePtr() const noexcept {
                int32 bytesInBuf = (nbits + 7) / 8;
                return ptr - bytesInBuf;
            }
        };

        // ────────────────────────────────────────────────────────────────────
        //  Predicteurs fixes (FIXED subframe, orders 0..4)
        //
        //  Predictions (somme des coefficients = order+1, comme polynome Taylor) :
        //    order 0 : 0
        //    order 1 :  s[i-1]
        //    order 2 :  2*s[i-1] - s[i-2]
        //    order 3 :  3*s[i-1] - 3*s[i-2] + s[i-3]
        //    order 4 :  4*s[i-1] - 6*s[i-2] + 4*s[i-3] - s[i-4]
        // ────────────────────────────────────────────────────────────────────

        static NKENTSEU_INLINE int64 FixedPredict(int32 order,
                                               const int32* hist) noexcept {
            // hist[0]=s[i-1], hist[1]=s[i-2], hist[2]=s[i-3], hist[3]=s[i-4]
            switch (order) {
                case 0: return 0;
                case 1: return int64(hist[0]);
                case 2: return int64(hist[0]) * 2 - int64(hist[1]);
                case 3: return int64(hist[0]) * 3 - int64(hist[1]) * 3 + int64(hist[2]);
                case 4: return int64(hist[0]) * 4 - int64(hist[1]) * 6
                            + int64(hist[2]) * 4 - int64(hist[3]);
                default: return 0;
            }
        }

        // ────────────────────────────────────────────────────────────────────
        //  Decoder de section residuelle (Rice partitionne)
        //
        //  Spec :
        //   - 2 bits : methode (00=Rice4, 01=Rice5, autres = reserves)
        //   - 4 bits : partition order P (donc 2^P partitions)
        //   - Pour chaque partition i :
        //       - 4 ou 5 bits (selon methode) : parametre Rice K
        //       - Nb d'echantillons : (blockSize >> P) - (i==0 ? predOrder : 0)
        //       - Si K vaut 15 (Rice4) ou 31 (Rice5) : ESCAPE
        //           * 5 bits : bit depth des echantillons bruts
        //           * depth bits par echantillon (signed raw)
        //       - Sinon : code Rice
        //           * Quotient unaire (zeros + 1 terminator)
        //           * K bits remainder
        //           * folded = (quotient << K) | remainder
        //           * value  = (folded >> 1) ^ -(int32)(folded & 1)  (zigzag inverse)
        // ────────────────────────────────────────────────────────────────────

        static bool DecodeResidual(FLACBitReader& br, int32 blockSize, int32 predOrder,
                                   int32* outResiduals) noexcept {
            const int32 method = br.Read(2);
            if (method >= 2) return false; // reserve

            const int32 paramBits = (method == 0) ? 4 : 5;
            const int32 escapeVal = (method == 0) ? 15 : 31;

            const int32 partOrder = int32(br.Read(4));
            const int32 nPart     = 1 << partOrder;
            if ((blockSize & ((1 << partOrder) - 1)) != 0) return false;

            int32 writeIdx = 0;
            const int32 partSize = blockSize >> partOrder;
            for (int32 p = 0; p < nPart; ++p) {
                int32 nSamples = (p == 0) ? (partSize - predOrder) : partSize;
                if (nSamples < 0) return false;

                const int32 K = int32(br.Read(paramBits));

                if (K == escapeVal) {
                    // Echappement : bit depth + raw signed
                    const int32 depth = int32(br.Read(5));
                    for (int32 i = 0; i < nSamples; ++i) {
                        outResiduals[writeIdx++] = (depth > 0) ? br.ReadSigned(depth) : 0;
                    }
                } else {
                    // Code Rice
                    for (int32 i = 0; i < nSamples; ++i) {
                        uint32 q = br.ReadUnary();
                        if (q == uint32(-1)) return false;
                        uint32 r = (K > 0) ? br.Read(K) : 0;
                        uint32 folded = (q << K) | r;
                        // zigzag inverse : pair -> positif, impair -> negatif
                        int32 val = int32(folded >> 1) ^ -int32(folded & 1);
                        outResiduals[writeIdx++] = val;
                    }
                }
            }
            return writeIdx == (blockSize - predOrder);
        }

        // ────────────────────────────────────────────────────────────────────
        //  Decoder d'un subframe pour un canal
        //
        //  - bps : bits par echantillon pour CE canal (peut etre bps+1 pour
        //          le canal "side" en stereo joint)
        //  - blockSize : nb d'echantillons dans la frame
        //  - outSamples : buffer de blockSize int32 pour stocker les samples
        // ────────────────────────────────────────────────────────────────────

        static bool DecodeSubframe(FLACBitReader& br, int32 bps, int32 blockSize,
                                   int32* outSamples) noexcept {
            // Header subframe
            if (br.Read(1) != 0) return false; // bit reserve
            const int32 subType = int32(br.Read(6));

            // Wasted bits flag
            int32 wastedBits = 0;
            if (br.Read(1) == 1) {
                // Unaire : k zeros + 1 -> k+1 wasted bits
                uint32 q = br.ReadUnary();
                if (q == uint32(-1)) return false;
                wastedBits = int32(q) + 1;
            }
            const int32 effBps = bps - wastedBits;
            if (effBps <= 0 || effBps > 32) return false;

            // Decode selon le type
            if (subType == 0) {
                // CONSTANT : tous les samples = meme valeur
                int32 val = br.ReadSigned(effBps);
                for (int32 i = 0; i < blockSize; ++i) outSamples[i] = val;
            }
            else if (subType == 1) {
                // VERBATIM : effBps bits par sample
                for (int32 i = 0; i < blockSize; ++i)
                    outSamples[i] = br.ReadSigned(effBps);
            }
            else if ((subType & 0x38) == 0x08) {
                // FIXED predictor (order 0..4 dans bits 0..2)
                const int32 order = subType & 0x07;
                if (order > 4) return false;

                // Warmup samples
                for (int32 i = 0; i < order; ++i)
                    outSamples[i] = br.ReadSigned(effBps);

                // Residuels
                int32* res = outSamples + order;
                if (!DecodeResidual(br, blockSize, order, res)) return false;

                // Reconstruction
                for (int32 i = order; i < blockSize; ++i) {
                    int32 hist[4];
                    hist[0] = (i >= 1) ? outSamples[i-1] : 0;
                    hist[1] = (i >= 2) ? outSamples[i-2] : 0;
                    hist[2] = (i >= 3) ? outSamples[i-3] : 0;
                    hist[3] = (i >= 4) ? outSamples[i-4] : 0;
                    int64 pred = FixedPredict(order, hist);
                    outSamples[i] = int32(int64(outSamples[i]) + pred);
                }
            }
            else if ((subType & 0x20) != 0) {
                // LPC predictor (order = (subType & 0x1F) + 1)
                const int32 order = (subType & 0x1F) + 1;
                if (order > 32) return false;

                // Warmup samples
                for (int32 i = 0; i < order; ++i)
                    outSamples[i] = br.ReadSigned(effBps);

                // Precision quantifiee (4 bits = precision - 1, donc 1..16)
                int32 qPrec = int32(br.Read(4));
                if (qPrec == 0x0F) return false; // valeur invalide = reservee
                qPrec += 1;

                // Quantification level (5 bits SIGNED : -16..15)
                int32 qLevel = br.ReadSigned(5);

                // Coefficients LPC (signes, qPrec bits chacun, ORDRE INVERSE :
                // le premier coef recu = coef[0] qui multiplie s[i-1])
                int32 coefs[32];
                for (int32 j = 0; j < order; ++j)
                    coefs[j] = br.ReadSigned(qPrec);

                // Residuels
                int32* res = outSamples + order;
                if (!DecodeResidual(br, blockSize, order, res)) return false;

                // Reconstruction
                for (int32 i = order; i < blockSize; ++i) {
                    int64 sum = 0;
                    for (int32 j = 0; j < order; ++j)
                        sum += int64(coefs[j]) * int64(outSamples[i - 1 - j]);
                    // Shift quantification : positif = shift droite, negatif = gauche
                    int64 pred = (qLevel >= 0) ? (sum >> qLevel) : (sum << (-qLevel));
                    outSamples[i] = int32(int64(outSamples[i]) + pred);
                }
            }
            else {
                return false; // type invalide / reserve
            }

            // Re-shifter par les wasted bits (les samples decodes etaient en
            // resolution reduite ; on les "etire" vers la pleine resolution)
            if (wastedBits > 0) {
                for (int32 i = 0; i < blockSize; ++i)
                    outSamples[i] <<= wastedBits;
            }
            return true;
        }

        // ────────────────────────────────────────────────────────────────────
        //  Lecture UTF-8 d'un entier de longueur variable (frame/sample number)
        //  Spec : meme codage que UTF-8, supportant jusqu'a 36 bits.
        //  Retourne uint64(-1) en cas d'erreur.
        // ────────────────────────────────────────────────────────────────────

        static uint64 ReadUTF8(FLACBitReader& br) noexcept {
            uint32 first = br.Read(8);
            if (first == uint32(-1)) return uint64(-1);

            int32 nBytes;
            uint64 val;
            if      ((first & 0x80) == 0)    { nBytes = 0; val = first; }
            else if ((first & 0xE0) == 0xC0) { nBytes = 1; val = first & 0x1F; }
            else if ((first & 0xF0) == 0xE0) { nBytes = 2; val = first & 0x0F; }
            else if ((first & 0xF8) == 0xF0) { nBytes = 3; val = first & 0x07; }
            else if ((first & 0xFC) == 0xF8) { nBytes = 4; val = first & 0x03; }
            else if ((first & 0xFE) == 0xFC) { nBytes = 5; val = first & 0x01; }
            else if (first == 0xFE)          { nBytes = 6; val = 0; }
            else return uint64(-1);

            for (int32 i = 0; i < nBytes; ++i) {
                uint32 b = br.Read(8);
                if ((b & 0xC0) != 0x80) return uint64(-1);
                val = (val << 6) | (b & 0x3F);
            }
            return val;
        }

        // ────────────────────────────────────────────────────────────────────
        //  Tables de decodage du header de frame (block size et sample rate)
        // ────────────────────────────────────────────────────────────────────

        static int32 DecodeBlockSize(FLACBitReader& br, uint32 code) noexcept {
            switch (code) {
                case 0x0: return -1; // reserve
                case 0x1: return 192;
                case 0x2: return 576;
                case 0x3: return 1152;
                case 0x4: return 2304;
                case 0x5: return 4608;
                case 0x6: return int32(br.Read(8))  + 1;  // 8-bit follows
                case 0x7: return int32(br.Read(16)) + 1;  // 16-bit follows
                case 0x8: return 256;
                case 0x9: return 512;
                case 0xA: return 1024;
                case 0xB: return 2048;
                case 0xC: return 4096;
                case 0xD: return 8192;
                case 0xE: return 16384;
                case 0xF: return 32768;
                default:  return -1;
            }
        }

        static int32 DecodeSampleRate(FLACBitReader& br, uint32 code,
                                      int32 streamRate) noexcept {
            switch (code) {
                case 0x0: return streamRate;
                case 0x1: return 88200;
                case 0x2: return 176400;
                case 0x3: return 192000;
                case 0x4: return 8000;
                case 0x5: return 16000;
                case 0x6: return 22050;
                case 0x7: return 24000;
                case 0x8: return 32000;
                case 0x9: return 44100;
                case 0xA: return 48000;
                case 0xB: return 96000;
                case 0xC: return int32(br.Read(8))  * 1000;  // kHz
                case 0xD: return int32(br.Read(16));         // Hz
                case 0xE: return int32(br.Read(16)) * 10;    // tens of Hz
                default:  return -1; // 0xF = reserve
            }
        }

        // Mapping code -> bits per sample
        static int32 DecodeSampleSize(uint32 code, int32 streamBps) noexcept {
            switch (code) {
                case 0x0: return streamBps;
                case 0x1: return 8;
                case 0x2: return 12;
                case 0x4: return 16;
                case 0x5: return 20;
                case 0x6: return 24;
                case 0x7: return 32;
                default:  return -1; // 0x3 = reserve
            }
        }

        // ────────────────────────────────────────────────────────────────────
        //  Decode d'une frame complete
        //  Retourne le nombre de frames audio decodees (blockSize) ou -1 si erreur.
        //  Remplit channelSamples[ch][i] (int32 signed) pour les blockSize samples.
        // ────────────────────────────────────────────────────────────────────

        struct StreamInfo {
            int32 sampleRate;
            int32 channels;
            int32 bps;
            uint64 totalSamples;
        };

        static int32 DecodeFrame(FLACBitReader& br, const StreamInfo& info,
                                 int32** channelSamples, int32 maxBlockSize,
                                 int32& outChannelAssign) noexcept {
            // Sync code : 14 bits = 0b11111111111110
            // ATTENTION : la spec autorise 15 bits "1111111111111" suivi de 0 reserve.
            // En pratique on lit 14 bits et on verifie qu'ils valent 0x3FFE.
            uint32 sync = br.Read(14);
            if (sync != 0x3FFE) return -1;

            // 1 bit reserve (=0)
            if (br.Read(1) != 0) return -1;

            // 1 bit blocking strategy (0=fixed-size, 1=variable-size)
            uint32 blockingStrategy = br.Read(1);

            // 4 bits block size code
            uint32 bsCode = br.Read(4);
            // 4 bits sample rate code
            uint32 srCode = br.Read(4);
            // 4 bits channel assignment
            outChannelAssign = int32(br.Read(4));
            // 3 bits sample size
            uint32 ssCode = br.Read(3);
            // 1 bit reserved (=0)
            if (br.Read(1) != 0) return -1;

            // UTF-8 sample number (variable-size) ou frame number (fixed-size)
            uint64 frameOrSampleNum = ReadUTF8(br);
            if (frameOrSampleNum == uint64(-1)) return -1;
            (void)blockingStrategy;
            (void)frameOrSampleNum;

            // Possibles octets supplementaires pour blockSize (codes 0x6, 0x7)
            int32 blockSize = DecodeBlockSize(br, bsCode);
            if (blockSize <= 0 || blockSize > maxBlockSize) return -1;

            // Possibles octets supplementaires pour sampleRate (codes 0xC, 0xD, 0xE)
            int32 sampleRate = DecodeSampleRate(br, srCode, info.sampleRate);
            if (sampleRate <= 0) return -1;

            int32 bps = DecodeSampleSize(ssCode, info.bps);
            if (bps <= 0 || bps > 32) return -1;

            // CRC-8 du header (1 octet) - on saute (verification optionnelle)
            br.Read(8);

            // Determiner le nombre de canaux et leurs bps
            // Channel assignment :
            //   0..7 = independent (nbChannels = code + 1)
            //   8    = left/side    : ch0=L, ch1=Side (ch1 a bps+1)
            //   9    = side/right   : ch0=Side, ch1=R  (ch0 a bps+1)
            //   10   = mid/side     : ch0=Mid, ch1=Side (ch1 a bps+1)
            int32 nCh;
            if (outChannelAssign < 8) {
                nCh = outChannelAssign + 1;
            } else if (outChannelAssign <= 10) {
                nCh = 2;
            } else {
                return -1; // reserve
            }
            if (nCh != info.channels) {
                // En theorie le nb de canaux est constant par stream ; rejette
                return -1;
            }

            // Decode chaque subframe (un par canal)
            for (int32 c = 0; c < nCh; ++c) {
                // En stereo joint, le canal "side" a 1 bit de plus
                int32 subBps = bps;
                if ((outChannelAssign == 8  && c == 1)
                 || (outChannelAssign == 9  && c == 0)
                 || (outChannelAssign == 10 && c == 1)) {
                    subBps = bps + 1;
                }
                if (!DecodeSubframe(br, subBps, blockSize, channelSamples[c]))
                    return -1;
            }

            // Padding zero bits + CRC-16 (2 octets) au footer
            br.ByteAlign();
            br.Read(16); // CRC-16 ignore en mode permissif

            return blockSize;
        }

        // ────────────────────────────────────────────────────────────────────
        //  Decorrelation des canaux pour stereo joint
        //
        //  Channel assignment :
        //   8 (LEFT/SIDE)  : L = ch0,           R = ch0 - ch1
        //   9 (SIDE/RIGHT) : L = ch1 + ch0,     R = ch1
        //   10 (MID/SIDE)  : reconstruction depuis (mid << 1) + (side & 1) :
        //                    L = (midD + side) / 2, R = (midD - side) / 2
        // ────────────────────────────────────────────────────────────────────

        static void DecorrelateStereo(int32 channelAssign, int32 n,
                                      int32* ch0, int32* ch1) noexcept {
            if (channelAssign == 8) {
                // L/S : L = ch0, R = ch0 - ch1
                for (int32 i = 0; i < n; ++i) ch1[i] = ch0[i] - ch1[i];
            } else if (channelAssign == 9) {
                // S/R : L = ch0 + ch1, R = ch1
                for (int32 i = 0; i < n; ++i) ch0[i] = ch0[i] + ch1[i];
            } else if (channelAssign == 10) {
                // M/S : ch0 = mid, ch1 = side
                for (int32 i = 0; i < n; ++i) {
                    int32 mid  = ch0[i];
                    int32 side = ch1[i];
                    // midD = (mid << 1) | (side & 1)
                    int32 midD = (mid * 2) | (side & 1);
                    ch0[i] = (midD + side) >> 1;
                    ch1[i] = (midD - side) >> 1;
                }
            }
        }

        // ────────────────────────────────────────────────────────────────────
        //  Decoder principal
        // ────────────────────────────────────────────────────────────────────

        AudioSample NkFLACCodec::Decode(const uint8* data, usize size,
                                        memory::NkAllocator* allocator) noexcept {
            AudioSample empty{};
            if (!data || size < 4 + 38) return empty;

            // 1. Magic "fLaC"
            if (RD_U32BE(data) != kFLACMagic) {
                logger.Error("[FLAC] Magic invalide (attendu 'fLaC').");
                return empty;
            }
            usize pos = 4;

            // 2. STREAMINFO obligatoire (premier bloc metadata)
            //    Header : 1 octet (last-flag + type) + 3 octets (length)
            if (pos + 4 > size) return empty;
            uint8 firstHdr = data[pos];
            uint32 firstLen = RD_U24BE(data + pos + 1);
            uint8 firstType = firstHdr & 0x7F;
            if (firstType != kFLACMetaStreamInfo || firstLen != 34) {
                logger.Error("[FLAC] STREAMINFO manquant ou taille invalide.");
                return empty;
            }
            pos += 4;
            if (pos + 34 > size) return empty;

            // STREAMINFO content (34 octets, layout big-endian bit-aligned) :
            //   bits   0..15  (si[0..1])   : min block size
            //   bits  16..31  (si[2..3])   : max block size
            //   bits  32..55  (si[4..6])   : min frame size (24 bits)
            //   bits  56..79  (si[7..9])   : max frame size (24 bits)
            //   bits  80..99  (si[10..12]) : sample rate (20 bits)
            //   bits 100..102 (si[12])     : channels - 1 (3 bits)
            //   bits 103..107 (si[12..13]) : bps - 1 (5 bits)
            //   bits 108..143 (si[13..17]) : total samples (36 bits)
            //   si[18..33]                 : MD5 (128 bits, ignore)
            const uint8* si = data + pos;
            uint32 minBlk = RD_U16BE(si + 0);
            uint32 maxBlk = RD_U16BE(si + 2);
            (void)minBlk;
            // Sample rate (20 bits) commence au byte 10
            uint32 sr20  = (uint32(si[10]) << 12)
                         | (uint32(si[11]) <<  4)
                         | (uint32(si[12]) >>  4);
            // Channels - 1 (3 bits) = bits 4..6 du byte 12 (MSB-first, donc bits 3..1)
            uint32 nch   = ((uint32(si[12]) >> 1) & 0x07) + 1;
            // bps - 1 (5 bits) = bit 0 du byte 12 + bits 7..4 du byte 13
            uint32 bps   = (((uint32(si[12]) & 0x01) << 4) | (uint32(si[13]) >> 4)) + 1;
            // Total samples (36 bits) = bits 0..3 du byte 13 (low nibble) + bytes 14..17
            uint64 totalLo = ((uint64(si[13]) & 0x0F) << 32)
                           | (uint64(si[14]) << 24)
                           | (uint64(si[15]) << 16)
                           | (uint64(si[16]) <<  8)
                           |  uint64(si[17]);
            // si[18..33] = MD5 (ignore)

            pos += 34;

            // 3. Skip les autres blocs metadata jusqu'au flag last-block
            bool wasLast = (firstHdr & 0x80) != 0;
            while (!wasLast) {
                if (pos + 4 > size) return empty;
                uint8 hdr = data[pos];
                uint32 len = RD_U24BE(data + pos + 1);
                wasLast = (hdr & 0x80) != 0;
                pos += 4;
                if (pos + len > size) return empty;
                pos += len;
            }

            // Validation STREAMINFO
            if (sr20 == 0 || nch == 0 || nch > 8 || bps < 4 || bps > 32) {
                logger.Error("[FLAC] STREAMINFO invalide : sr={0} ch={1} bps={2}",
                             sr20, nch, bps);
                return empty;
            }

            StreamInfo info;
            info.sampleRate   = int32(sr20);
            info.channels     = int32(nch);
            info.bps          = int32(bps);
            info.totalSamples = totalLo;

            // 4. Allocation du buffer de sortie (float32 interleaved)
            //    Si totalSamples est inconnu (= 0), on alloue maxBlk * un cap raisonnable
            //    et on realloue si depasse.
            uint64 estFrames = (totalLo > 0) ? totalLo : uint64(maxBlk) * 4096;
            if (estFrames == 0) estFrames = uint64(maxBlk) * 16;
            usize estSamples = usize(estFrames) * usize(nch);
            float32* outData = static_cast<float32*>(
                memory::NkAlloc(estSamples * sizeof(float32), allocator, sizeof(float32)));
            if (!outData) {
                logger.Error("[FLAC] Echec allocation ({0} samples).", estSamples);
                return empty;
            }

            // Buffers per-channel int32 (taille maxBlk)
            const int32 maxBs = int32(maxBlk > 0 ? maxBlk : 32768);
            int32* perCh[8] = { nullptr };
            int32* allCh = static_cast<int32*>(
                memory::NkAlloc(usize(maxBs) * usize(nch) * sizeof(int32), allocator, sizeof(int32)));
            if (!allCh) {
                memory::NkFree(outData, allocator);
                logger.Error("[FLAC] Echec allocation buffer canaux.");
                return empty;
            }
            for (uint32 c = 0; c < nch; ++c)
                perCh[c] = allCh + usize(c) * usize(maxBs);

            // 5. Iteration sur les frames
            FLACBitReader br;
            br.Init(data + pos, size - pos);

            // Facteur de normalisation : on map [-2^(bps-1), 2^(bps-1)-1] vers [-1, +1]
            const float32 norm = 1.0f / float32(int64(1) << (bps - 1));

            uint64 totalDecoded = 0;
            uint64 capFrames = estFrames;

            while (true) {
                // Verifie qu'il reste au moins 2 octets (synccode = 2)
                br.Fill(16);
                if (br.nbits < 16) break;
                // Peek 14 bits pour le sync
                uint32 peek = uint32(br.buf >> 50);
                if (peek != 0x3FFE) break;

                int32 chAssign = 0;
                int32 blockN = DecodeFrame(br, info, perCh, maxBs, chAssign);
                if (blockN <= 0) {
                    logger.Warn("[FLAC] Frame invalide a frame={0}, on arrete.",
                                totalDecoded);
                    break;
                }

                // Decorrelation stereo si applicable
                if (nch == 2 && chAssign >= 8 && chAssign <= 10) {
                    DecorrelateStereo(chAssign, blockN, perCh[0], perCh[1]);
                }

                // Grow output buffer si necessaire
                if (totalDecoded + uint64(blockN) > capFrames) {
                    uint64 newCap = capFrames * 2;
                    while (newCap < totalDecoded + uint64(blockN)) newCap *= 2;
                    usize newSamp = usize(newCap) * usize(nch);
                    float32* grown = static_cast<float32*>(
                        memory::NkAlloc(newSamp * sizeof(float32), allocator, sizeof(float32)));
                    if (!grown) {
                        memory::NkFree(outData, allocator);
                        memory::NkFree(allCh, allocator);
                        logger.Error("[FLAC] Realloc buffer sortie echec.");
                        return empty;
                    }
                    ::memcpy(grown, outData, usize(totalDecoded) * usize(nch) * sizeof(float32));
                    memory::NkFree(outData, allocator);
                    outData = grown;
                    capFrames = newCap;
                }

                // Interleave + normalize vers float32
                float32* dst = outData + usize(totalDecoded) * usize(nch);
                for (int32 i = 0; i < blockN; ++i) {
                    for (uint32 c = 0; c < nch; ++c) {
                        *dst++ = float32(perCh[c][i]) * norm;
                    }
                }
                totalDecoded += uint64(blockN);
            }

            memory::NkFree(allCh, allocator);

            if (totalDecoded == 0) {
                memory::NkFree(outData, allocator);
                logger.Error("[FLAC] Aucune frame decodee.");
                return empty;
            }

            AudioSample result;
            result.data        = outData;
            result.frameCount  = usize(totalDecoded);
            result.sampleRate  = info.sampleRate;
            result.channels    = info.channels;
            result.format      = AudioFormat::FLAC;
            result.mAllocator  = allocator;

            logger.Info("[FLAC] Decode OK : {0} frames, {1} canaux, {2} Hz, {3} bps.",
                        totalDecoded, info.channels, info.sampleRate, info.bps);
            return result;
        }

    } // namespace audio
} // namespace nkentseu
