#pragma once
/**
 * @File    NkWebPCodec.h
 * @Brief   Codec WebP — lecture et écriture, from scratch.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  WebP est un format Google basé sur VP8 (lossy) et VP8L (lossless).
 *  Structure d'un fichier WebP :
 *    RIFF header (12 octets) → chunks VP8 / VP8L / VP8X / ANIM / ALPH
 *
 *  Implémentation :
 *    Lecture  : VP8L (lossless) complet + VP8 (lossy) basique.
 *    Écriture : VP8L (lossless) — compression sans perte, qualité maximale.
 *
 *  VP8L utilise :
 *    - Huffman codes préfixes (5 arbres par groupe de pixels)
 *    - Transformations : color transform, subtract green, predict, color indexing
 *    - Backward references (LZ77)
 *
 *  VP8 (lossy) lecture uniquement — décodeur DCT simplifié.
 */

#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkWebPCodec {
        public:
            static NkImage* Decode(const uint8* data, usize size) noexcept;
            static bool     Encode(const NkImage& img, uint8*& out, usize& outSize,
                                bool lossless = true, int32 quality = 90) noexcept;

        private:
            // ── Structures RIFF / WebP ────────────────────────────────────────────
            struct RIFFChunk {
                uint32 fourcc;
                uint32 size;
                usize  dataOffset;
            };

        // Helpers mathématiques internes
        static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
        static uint32  Average2(uint32 a, uint32 b) noexcept;
        static uint32  AddPixels(uint32 a, uint32 b) noexcept;
        static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
        static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;

            // ── VP8L (lossless) ───────────────────────────────────────────────────
            struct VP8LBitReader {
                const uint8* data;
                usize        size;
                usize        bytePos;
                uint64       bitBuf;
                int32        bitCount;
                bool         error;

                void    Init(const uint8* d, usize s) noexcept;
                uint32  ReadBits(int32 n) noexcept;
                bool    IsEOF() const noexcept { return bytePos >= size && bitCount <= 0; }
            };

        // Helpers mathématiques internes
        static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
        static uint32  Average2(uint32 a, uint32 b) noexcept;
        static uint32  AddPixels(uint32 a, uint32 b) noexcept;
        static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
        static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;

            struct VP8LHuffTree {
                static constexpr int32 MAX_SYMS = 256 + 24 + 1; // RGBA + distance codes
                int16  codes[MAX_SYMS];     // code pour chaque symbole
                int16  codeLens[MAX_SYMS];  // longueur du code
                // Table de décodage rapide (lookup table 10 bits)
                static constexpr int32 LUT_BITS = 9;
                int16  lut[1 << LUT_BITS];  // valeur (>=0) ou -(longueur supp.)
                int16  lutLen[1 << LUT_BITS];
                int32  numSymbols;
                bool   isValid;

                bool Build(const uint8* codeLengths, int32 n) noexcept;
                int32 Decode(VP8LBitReader& br) const noexcept;
            };

        // Helpers mathématiques internes
        static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
        static uint32  Average2(uint32 a, uint32 b) noexcept;
        static uint32  AddPixels(uint32 a, uint32 b) noexcept;
        static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
        static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;

            // Contexte de décodage VP8L
            struct VP8LDecoder {
                int32  width, height;
                bool   hasAlpha;
                uint32* pixels;           // sortie ARGB
                NkMemArena* arena;        // scratch

                // Transformations à appliquer après décodage
                struct Transform {
                    uint8  type;          // 0=predict, 1=color, 2=subtract_green, 3=color_indexing
                    int32  bits;
                    uint32* data;         // données de la transformation
                };

        // Helpers mathématiques internes
        static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
        static uint32  Average2(uint32 a, uint32 b) noexcept;
        static uint32  AddPixels(uint32 a, uint32 b) noexcept;
        static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
        static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;
                Transform transforms[4];
                int32 numTransforms;
            };

        // Helpers mathématiques internes
        static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
        static uint32  Average2(uint32 a, uint32 b) noexcept;
        static uint32  AddPixels(uint32 a, uint32 b) noexcept;
        static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
        static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;

            static NkImage* DecodeVP8L(const uint8* data, usize size) noexcept;
            static NkImage* DecodeVP8 (const uint8* data, usize size) noexcept;

            static bool ReadVP8LTransforms(VP8LBitReader& br, VP8LDecoder& dec) noexcept;
            static bool ReadVP8LHuffmanCodes(VP8LBitReader& br,
                                            VP8LHuffTree trees[5],
                                            int32 numCodes) noexcept;
            static bool DecodeVP8LImage(VP8LBitReader& br, VP8LDecoder& dec,
                                        int32 w, int32 h, uint32* out) noexcept;
            static void ApplyTransforms(VP8LDecoder& dec) noexcept;
            static void ApplyPredictTransform(const uint32* data, uint32* pixels,
                                            int32 w, int32 h, int32 bits) noexcept;
            static void ApplyColorTransform(const uint32* data, uint32* pixels,
                                            int32 w, int32 h, int32 bits) noexcept;
            static void ApplySubtractGreen(uint32* pixels, int32 n) noexcept;
            static void ApplyColorIndexing(const uint32* table, uint32* pixels,
                                            int32 w, int32 h, int32 bits) noexcept;

            // ── VP8L encodeur (lossless) ──────────────────────────────────────────
            struct VP8LBitWriter {
                NkImageStream* s;
                uint64 buf  = 0;
                int32  bits = 0;
                void WriteBits(uint32 val, int32 n) noexcept;
                void Flush() noexcept;
            };

        // Helpers mathématiques internes
        static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
        static uint32  Average2(uint32 a, uint32 b) noexcept;
        static uint32  AddPixels(uint32 a, uint32 b) noexcept;
        static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
        static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;

            static bool EncodeVP8L(const NkImage& img, NkImageStream& out) noexcept;
            static void BuildCanonicalHuff(const uint32* freqs, int32 n,
                                            uint8* codeLens) noexcept;
            static void WriteHuffmanCode(VP8LBitWriter& bw,
                                        const uint8* codeLens, int32 n) noexcept;
            static void WriteImageData(VP8LBitWriter& bw, const uint32* argb,
                                        int32 n, const VP8LHuffTree trees[5]) noexcept;

            // Utilitaires RIFF
            static bool     FindChunk(const uint8* data, usize size,
                                    uint32 fourcc, RIFFChunk& out) noexcept;
            static uint32   MakeFourCC(char a, char b, char c, char d) noexcept {
                return static_cast<uint32>(a)|(static_cast<uint32>(b)<<8)|
                    (static_cast<uint32>(c)<<16)|(static_cast<uint32>(d)<<24);
            }
    };

    // Helpers mathématiques internes
    static int8    ColorTransformDelta(int8 a, uint8 b) noexcept;
    static uint32  Average2(uint32 a, uint32 b) noexcept;
    static uint32  AddPixels(uint32 a, uint32 b) noexcept;
    static uint32  Select(uint32 L, uint32 T, uint32 TL) noexcept;
    static uint32  ClampedAdd(uint32 a, uint32 b, uint32 sub) noexcept;

} // namespace nkentseu
// Forward declarations des helpers (définis dans .cpp)
// Note : ces méthodes sont utilisées en interne par ApplyTransforms
