/**
 * @File    NkBMPCodec.cpp
 * @Brief   Codec BMP production-ready — DIB complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Fixes vs version précédente
 *  1. RLE4 absolute run : lecture corrigée — chaque octet source contient 2 index
 *     (nibbles haut et bas). L'ancienne version lisait un octet par index (faux).
 *  2. RLE4 padding : le padding de fin de run absolu est sur l'arrondi au mot
 *     (2 octets) du nombre d'octets lus, pas sur le nombre de pixels.
 *  3. Décodeur RLE8/RLE4 : dstY dans les runs encodés utilisait height-1-y
 *     (bottom-up), mais quand topDown=true il faut utiliser y directement.
 *     Maintenant les deux variantes sont gérées proprement.
 *  4. BMP 32bpp sans masque alpha : si tous les alpha sont 0 on force 255
 *     (comportement stb_image conforme).
 *  5. Encode : padding de ligne corrigé — on écrivait des 0 un par un après
 *     la ligne, maintenant calculé correctement depuis bmpStride.
 *  6. Encode RGBA : le BITMAPV4HEADER écrit maintenant tous les champs
 *     correctement (68 octets d'extension après les 40 de base).
 */
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {

    using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Helpers
    // ─────────────────────────────────────────────────────────────────────────────

    // Convertit une couleur masquée BITFIELDS → [0-255]
    static uint8 ScaleMask(uint32 val, uint32 mask) noexcept {
        if (!mask) return 0;
        // Trouve le LSB du masque
        int32 shift = 0;
        uint32 m = mask;
        while (m && !(m & 1)) { ++shift; m >>= 1; }
        uint32 v = (val & mask) >> shift;
        // Compte le nombre de bits du masque
        uint32 bits = 0;
        uint32 tmp = m;
        while (tmp) { ++bits; tmp >>= 1; }
        if (bits == 0) return 0;
        if (bits >= 8) return static_cast<uint8>(v >> (bits - 8));
        // Étire vers 8 bits (répétition des bits) comme stb_image
        return static_cast<uint8>((v * 255u) / ((1u << bits) - 1u));
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkBMPCodec::Decode
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkBMPCodec::Decode(const uint8* data, usize size) noexcept {
        if (size < 14) return nullptr;
        NkImageStream s(data, size);

        // ── BITMAPFILEHEADER ─────────────────────────────────────────────────────
        if (s.ReadU8() != 'B' || s.ReadU8() != 'M') return nullptr;
        s.Skip(4); // fileSize (non fiable)
        s.Skip(4); // reserved
        const uint32 pixelOffset = s.ReadU32LE();

        // ── DIB header ───────────────────────────────────────────────────────────
        const uint32 dibSize = s.ReadU32LE();
        int32  width = 0, height = 0;
        uint16 bpp = 0;
        uint32 compress = 0;
        uint32 clrUsed = 0;
        bool   isCore = false;

        if (dibSize == 12) { // BITMAPCOREHEADER (OS/2)
            width   = static_cast<int32>(s.ReadU16LE());
            height  = static_cast<int32>(s.ReadU16LE());
            (void)   s.ReadU16LE(); // planes
            bpp     = s.ReadU16LE();
            isCore  = true;
            compress = 0;
        } else { // BITMAPINFOHEADER ou supérieur
            width   = s.ReadI32LE();
            height  = s.ReadI32LE();
            (void)   s.ReadU16LE(); // planes
            bpp     = s.ReadU16LE();
            compress = s.ReadU32LE();
            s.Skip(4);  // imageSize
            s.Skip(4);  // xPixPerMeter
            s.Skip(4);  // yPixPerMeter
            clrUsed = s.ReadU32LE();
            s.Skip(4);  // clrImportant
        }

        // Orientation : hauteur > 0 = bottom-up, < 0 = top-down
        const bool topDown = (height < 0);
        if (topDown) height = -height;
        if (width <= 0 || height <= 0) return nullptr;
        if (bpp == 0 || bpp > 32) return nullptr;

        // ── Masques BITFIELDS ────────────────────────────────────────────────────
        uint32 maskR = 0, maskG = 0, maskB = 0, maskA = 0;
        const bool hasBitfields = (compress == 3 || compress == 6);

        if (hasBitfields) {
            const usize maskOff = 14 + dibSize;
            if (maskOff + 16 <= size) {
                NkImageStream ms(data + maskOff, 16);
                maskR = ms.ReadU32LE();
                maskG = ms.ReadU32LE();
                maskB = ms.ReadU32LE();
                maskA = ms.ReadU32LE();
            }
        }

        // Masques par défaut
        if (bpp == 16 && !hasBitfields) {
            maskR = 0x7C00; maskG = 0x03E0; maskB = 0x001F; maskA = 0;
        }
        if (bpp == 32 && !hasBitfields) {
            maskR = 0x00FF0000; maskG = 0x0000FF00; maskB = 0x000000FF; maskA = 0xFF000000;
        }

        // ── Palette ──────────────────────────────────────────────────────────────
        int32 palCount = 0;
        if (bpp <= 8) {
            if (isCore)   palCount = 1 << bpp;
            else          palCount = (clrUsed > 0) ? static_cast<int32>(clrUsed) : (1 << bpp);
        }
        uint8 palette[256 * 4] = {}; // BGRA
        if (palCount > 0) {
            const int32 entrySize = isCore ? 3 : 4;
            for (int32 i = 0; i < palCount && !s.HasError(); ++i)
                s.ReadBytes(palette + i * 4, entrySize);
        }

        // ── Format de sortie ─────────────────────────────────────────────────────
        // On détermine si on a de l'alpha après lecture (pour 32bpp sans masque)
        // En attendant on alloue RGBA si bpp==32 ou si maskA != 0
        const bool mayHaveAlpha = (bpp == 32) || (hasBitfields && maskA != 0);
        const NkImagePixelFormat fmt = mayHaveAlpha
            ? NkImagePixelFormat::NK_RGBA32
            : NkImagePixelFormat::NK_RGB24;
        const int32 outCh = mayHaveAlpha ? 4 : 3;

        NkImage* img = NkImage::Alloc(width, height, fmt);
        if (!img) return nullptr;

        // ── Lecture des pixels ───────────────────────────────────────────────────
        s.Seek(pixelOffset);

        const int32 srcStride = ((width * bpp + 31) / 32) * 4;
        uint8* lineBuf = static_cast<uint8*>(NkAlloc(static_cast<usize>(srcStride) + 4));
        if (!lineBuf) { img->Free(); return nullptr; }

        if (compress == 0 || compress == 3 || compress == 6) {
            // ── BI_RGB / BI_BITFIELDS : lecture directe ───────────────────────
            bool alphaAllZero = true;

            for (int32 row = 0; row < height; ++row) {
                const int32 dstY = topDown ? row : (height - 1 - row);
                uint8* dst = img->RowPtr(dstY);

                s.ReadBytes(lineBuf, srcStride);

                for (int32 x = 0; x < width; ++x) {
                    uint8 r = 0, g = 0, b = 0, a = 255;

                    if (bpp == 1) {
                        const uint8 idx = (lineBuf[x / 8] >> (7 - (x % 8))) & 1;
                        r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
                    } else if (bpp == 2) {
                        const uint8 idx = (lineBuf[x / 4] >> ((3 - (x % 4)) * 2)) & 3;
                        r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
                    } else if (bpp == 4) {
                        const uint8 idx = (x & 1) ? (lineBuf[x/2] & 0x0F) : (lineBuf[x/2] >> 4);
                        r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
                    } else if (bpp == 8) {
                        const uint8 idx = lineBuf[x];
                        r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
                    } else if (bpp == 16) {
                        const uint16 v = static_cast<uint16>(lineBuf[x*2])
                                    | (static_cast<uint16>(lineBuf[x*2+1]) << 8);
                        if (hasBitfields) {
                            r = ScaleMask(v, maskR);
                            g = ScaleMask(v, maskG);
                            b = ScaleMask(v, maskB);
                            a = maskA ? ScaleMask(v, maskA) : 255;
                        } else {
                            // RGB555 par défaut
                            r = static_cast<uint8>(((v >> 10) & 0x1F) * 255 / 31);
                            g = static_cast<uint8>(((v >>  5) & 0x1F) * 255 / 31);
                            b = static_cast<uint8>(( v        & 0x1F) * 255 / 31);
                        }
                    } else if (bpp == 24) {
                        b = lineBuf[x*3+0]; g = lineBuf[x*3+1]; r = lineBuf[x*3+2];
                    } else if (bpp == 32) {
                        const uint32 v = static_cast<uint32>(lineBuf[x*4+0])
                                    | (static_cast<uint32>(lineBuf[x*4+1]) <<  8)
                                    | (static_cast<uint32>(lineBuf[x*4+2]) << 16)
                                    | (static_cast<uint32>(lineBuf[x*4+3]) << 24);
                        if (hasBitfields) {
                            r = ScaleMask(v, maskR);
                            g = ScaleMask(v, maskG);
                            b = ScaleMask(v, maskB);
                            a = ScaleMask(v, maskA);
                        } else {
                            b = lineBuf[x*4+0]; g = lineBuf[x*4+1];
                            r = lineBuf[x*4+2]; a = lineBuf[x*4+3];
                        }
                        if (a != 0) alphaAllZero = false;
                    }

                    if (outCh == 4) {
                        dst[x*4+0] = r; dst[x*4+1] = g; dst[x*4+2] = b; dst[x*4+3] = a;
                    } else {
                        dst[x*3+0] = r; dst[x*3+1] = g; dst[x*3+2] = b;
                    }
                }
            }

            // Si 32bpp sans masque et tous alpha=0, force opaque (comportement stb)
            if (bpp == 32 && !hasBitfields && alphaAllZero) {
                for (int32 y = 0; y < height; ++y) {
                    uint8* row = img->RowPtr(y);
                    for (int32 x = 0; x < width; ++x) row[x*4+3] = 255;
                }
            }

        } else if (compress == 1) {
            // ── BI_RLE8 ───────────────────────────────────────────────────────
            int32 x = 0, y = 0;
            while (!s.IsEOF() && !s.HasError()) {
                const uint8 b0 = s.ReadU8();
                const uint8 b1 = s.ReadU8();
                if (b0 == 0) {
                    if (b1 == 0) {         // EOL
                        x = 0; ++y;
                    } else if (b1 == 1) { // EOF
                        break;
                    } else if (b1 == 2) { // DELTA
                        x += s.ReadU8(); y += s.ReadU8();
                    } else {              // absolute run
                        const int32 cnt = b1;
                        for (int32 i = 0; i < cnt; ++i) {
                            const uint8 idx = s.ReadU8();
                            const int32 dstY = topDown ? y : (height - 1 - y);
                            if (x < width && dstY >= 0 && dstY < height) {
                                uint8* d = img->RowPtr(dstY) + x * outCh;
                                d[0] = palette[idx*4+2];
                                d[1] = palette[idx*4+1];
                                d[2] = palette[idx*4+0];
                                if (outCh == 4) d[3] = 255;
                            }
                            ++x;
                        }
                        if (cnt & 1) (void)s.ReadU8(); // padding au mot
                    }
                } else {
                    // encoded run
                    const uint8 idx = b1;
                    for (int32 i = 0; i < b0; ++i) {
                        const int32 dstY = topDown ? y : (height - 1 - y);
                        if (x < width && dstY >= 0 && dstY < height) {
                            uint8* d = img->RowPtr(dstY) + x * outCh;
                            d[0] = palette[idx*4+2];
                            d[1] = palette[idx*4+1];
                            d[2] = palette[idx*4+0];
                            if (outCh == 4) d[3] = 255;
                        }
                        ++x;
                    }
                }
            }

        } else if (compress == 2) {
            // ── BI_RLE4 ───────────────────────────────────────────────────────
            int32 x = 0, y = 0;
            while (!s.IsEOF() && !s.HasError()) {
                const uint8 b0 = s.ReadU8();
                const uint8 b1 = s.ReadU8();

                auto plotPix = [&](uint8 idx) {
                    const int32 dstY = topDown ? y : (height - 1 - y);
                    if (x < width && dstY >= 0 && dstY < height) {
                        uint8* d = img->RowPtr(dstY) + x * outCh;
                        d[0] = palette[idx*4+2];
                        d[1] = palette[idx*4+1];
                        d[2] = palette[idx*4+0];
                        if (outCh == 4) d[3] = 255;
                    }
                    ++x;
                };

                if (b0 == 0) {
                    if (b1 == 0) { x = 0; ++y; }
                    else if (b1 == 1) { break; }
                    else if (b1 == 2) { x += s.ReadU8(); y += s.ReadU8(); }
                    else {
                        // absolute run : b1 pixels, chaque octet contient 2 index (nibbles)
                        const int32 cnt = b1;
                        // FIX: nombre d'octets à lire = ceil(cnt/2), padding sur mot de 2 octets
                        const int32 bytesToRead = (cnt + 1) / 2;
                        for (int32 i = 0; i < bytesToRead; ++i) {
                            const uint8 byte = s.ReadU8();
                            // pixel pair*2 : nibble haut
                            if (i * 2 < cnt)     plotPix((byte >> 4) & 0xF);
                            // pixel pair*2+1 : nibble bas
                            if (i * 2 + 1 < cnt) plotPix(byte & 0xF);
                        }
                        // Padding : si bytesToRead est impair, lire un octet de padding
                        if (bytesToRead & 1) (void)s.ReadU8();
                    }
                } else {
                    // encoded run : b0 pixels alternant deux couleurs (nibble haut/bas de b1)
                    const uint8 idx0 = (b1 >> 4) & 0xF;
                    const uint8 idx1 =  b1        & 0xF;
                    for (int32 i = 0; i < b0; ++i)
                        plotPix((i & 1) ? idx1 : idx0);
                }
            }
        }

        NkFree(lineBuf);
        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkBMPCodec::Encode
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkBMPCodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
        if (!img.IsValid()) return false;

        const NkImage* src  = &img;
        NkImage*       conv = nullptr;
        if (img.Format() != NkImagePixelFormat::NK_RGB24 &&
            img.Format() != NkImagePixelFormat::NK_RGBA32)
        {
            conv = img.Convert(NkImagePixelFormat::NK_RGB24);
            if (!conv) return false;
            src = conv;
        }

        const int32  w    = src->Width();
        const int32  h    = src->Height();
        const uint16 bpp  = static_cast<uint16>(src->Channels() == 4 ? 32 : 24);
        const int32  bytesPerPixel = bpp / 8;

        // Stride BMP : multiple de 4 octets
        const int32  bmpStride    = ((w * bytesPerPixel + 3) / 4) * 4;
        const uint32 pixDataSize  = static_cast<uint32>(bmpStride) * h;

        // Pour RGBA on utilise BITMAPV4HEADER (108 octets), sinon BITMAPINFOHEADER (40)
        const uint32 dibHdrSize  = (bpp == 32) ? 108u : 40u;
        const uint32 pixOffset   = 14 + dibHdrSize;
        const uint32 fileSize    = pixOffset + pixDataSize;

        NkImageStream s;

        // ── BITMAPFILEHEADER (14 octets) ─────────────────────────────────────────
        s.WriteU8('B'); s.WriteU8('M');
        s.WriteU32LE(fileSize);
        s.WriteU32LE(0); // reserved
        s.WriteU32LE(pixOffset);

        // ── DIB header ───────────────────────────────────────────────────────────
        s.WriteU32LE(dibHdrSize);
        s.WriteI32LE(w);
        s.WriteI32LE(-h);  // top-down (hauteur négative)
        s.WriteU16LE(1);   // planes
        s.WriteU16LE(bpp);
        s.WriteU32LE(bpp == 32 ? 3u : 0u); // BI_BITFIELDS pour RGBA, BI_RGB sinon
        s.WriteU32LE(pixDataSize);
        s.WriteI32LE(2835); // 72 DPI X
        s.WriteI32LE(2835); // 72 DPI Y
        s.WriteU32LE(0); // clrUsed
        s.WriteU32LE(0); // clrImportant

        if (bpp == 32) {
            // Masques RGBA — partie spécifique BITMAPV4HEADER (68 octets supplémentaires)
            s.WriteU32LE(0x00FF0000); // R mask
            s.WriteU32LE(0x0000FF00); // G mask
            s.WriteU32LE(0x000000FF); // B mask
            s.WriteU32LE(0xFF000000); // A mask
            s.WriteU32LE(0x57696E20); // CSType = "Win " (sRGB)
            // Endpoints CIEXYZTRIPLE (36 octets) — tous zéro pour sRGB
            for (int32 i = 0; i < 36; ++i) s.WriteU8(0);
            s.WriteU32LE(0); // gammaRed
            s.WriteU32LE(0); // gammaGreen
            s.WriteU32LE(0); // gammaBlue
            // Total écrit : 40 (base) + 4+4+4+4+4+36+4+4+4 = 40+68 = 108 ✓
        }

        // ── Pixels (top-down puisque height négatif dans le header) ──────────────
        uint8 pad[4] = {0, 0, 0, 0};
        for (int32 y = 0; y < h; ++y) {
            const uint8* row = src->RowPtr(y);
            int32 written = 0;
            for (int32 x = 0; x < w; ++x) {
                const uint8* p = row + x * bytesPerPixel;
                if (bpp == 24) {
                    s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); // BGR
                    written += 3;
                } else {
                    s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); s.WriteU8(p[3]); // BGRA
                    written += 4;
                }
            }
            // Padding de ligne pour aligner sur 4 octets
            const int32 paddingBytes = bmpStride - written;
            if (paddingBytes > 0)
                s.WriteBytes(pad, static_cast<usize>(paddingBytes));
        }

        if (conv) conv->Free();
        return s.TakeBuffer(out, outSize);
    }

} // namespace nkentseu