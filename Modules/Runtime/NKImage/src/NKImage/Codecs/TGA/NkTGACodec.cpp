/**
 * @File    NkTGACodec.cpp
 * @Brief   Codec TGA (Truevision TARGA) — lecture et écriture.
 *          Supporte : Type 1 (color-mapped), 2 (true-color), 3 (grayscale),
 *                     9, 10, 11 (versions RLE correspondantes).
 */
#include "NkTGACodec.h"
#include <cstring>

namespace nkentseu {

    NkImage* NkTGACodec::Decode(const uint8* data, usize size) noexcept {
        if (size < 18) return nullptr;
        NkImageStream s(data, size);

        const uint8 idLen      = s.ReadU8();
        const uint8 colorMap   = s.ReadU8();
        const uint8 imgType    = s.ReadU8();
        s.Skip(5);  // color map spec
        s.Skip(4);  // x/y origin
        const uint16 width     = s.ReadU16LE();
        const uint16 height    = s.ReadU16LE();
        const uint8  bpp       = s.ReadU8();
        const uint8  imgDesc   = s.ReadU8();
        s.Skip(idLen);
        if (colorMap) s.Skip(/* ignore color map */ 0);
        (void)imgDesc;

        if (width == 0 || height == 0) return nullptr;
        const bool rle   = (imgType == 9 || imgType == 10 || imgType == 11);
        const bool gray  = (imgType == 3 || imgType == 11);
        const bool tc    = (imgType == 2 || imgType == 10);
        (void)colorMap;

        int32 channels = gray ? 1 : (bpp == 32 ? 4 : 3);
        NkPixelFormat fmt = (channels == 4) ? NkPixelFormat::NK_RGBA32 :
                            (channels == 3) ? NkPixelFormat::NK_RGB24  : NkPixelFormat::NK_GRAY8;
        NkImage* img = NkImage::Alloc(width, height, fmt);
        if (!img) return nullptr;

        const int32 pixBytes = bpp / 8;
        int32 x = 0, y = 0;

        auto writePixel = [&](const uint8* px) {
            uint8* dst = img->RowPtr(height - 1 - y) + x * channels; // TGA est bottom-up
            if (gray) { dst[0] = px[0]; }
            else if (bpp == 24) { dst[0]=px[2]; dst[1]=px[1]; dst[2]=px[0]; }       // BGR→RGB
            else if (bpp == 32) { dst[0]=px[2]; dst[1]=px[1]; dst[2]=px[0]; dst[3]=px[3]; } // BGRA→RGBA
            ++x; if (x >= width) { x = 0; ++y; }
        };

        if (!rle) {
            for (int32 i = 0; i < width * height && !s.HasError(); ++i) {
                uint8 px[4] = {};
                s.ReadBytes(px, pixBytes);
                writePixel(px);
            }
        } else {
            while (y < height && !s.HasError()) {
                const uint8 rep = s.ReadU8();
                const int32 count = (rep & 0x7F) + 1;
                if (rep & 0x80) {  // RLE packet
                    uint8 px[4] = {}; s.ReadBytes(px, pixBytes);
                    for (int32 i = 0; i < count; ++i) writePixel(px);
                } else {           // Raw packet
                    for (int32 i = 0; i < count; ++i) {
                        uint8 px[4] = {}; s.ReadBytes(px, pixBytes);
                        writePixel(px);
                    }
                }
            }
        }
        (void)tc;
        if (s.HasError()) { img->Free(); return nullptr; }
        return img;
    }

    bool NkTGACodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
        if (!img.IsValid()) return false;
        NkImageStream s;
        const int32 w = img.Width(), h = img.Height(), ch = img.Channels();
        const uint8 bpp = static_cast<uint8>(ch == 4 ? 32 : ch == 1 ? 8 : 24);
        const uint8 imgType = ch == 1 ? 3 : 2; // grayscale ou true-color (no RLE)

        // Header TGA 18 octets
        s.WriteU8(0);        // ID length
        s.WriteU8(0);        // color map type
        s.WriteU8(imgType);  // image type
        s.WriteU8(0); s.WriteU8(0); s.WriteU8(0); s.WriteU8(0); s.WriteU8(0); // color map spec
        s.WriteU16LE(0); s.WriteU16LE(0); // origin X,Y
        s.WriteU16LE(static_cast<uint16>(w));
        s.WriteU16LE(static_cast<uint16>(h));
        s.WriteU8(bpp);
        s.WriteU8(0x20); // image descriptor : top-left origin

        // Pixels — TGA stocke bottom-up, on écrit top-down avec le flag 0x20
        for (int32 y = 0; y < h; ++y) {
            const uint8* row = img.RowPtr(y);
            for (int32 x = 0; x < w; ++x) {
                const uint8* p = row + x * ch;
                if (ch == 1) s.WriteU8(p[0]);
                else if (ch == 3) { s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); }
                else { s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); s.WriteU8(p[3]); }
            }
        }
        // Footer TGA (optionnel mais recommandé)
        const char* footer = "\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0";
        s.WriteBytes(reinterpret_cast<const uint8*>(footer), 26);
        return s.TakeBuffer(out, outSize);
    }
} // namespace nkentseu
