/**
 * @File    NkTGACodec.cpp
 * @Brief   Codec TGA (Truevision TARGA) — algorithme adapté de stb_image (public domain).
 *          Supporte :
 *            Type 1  (color-mapped)       + Type 9  (color-mapped RLE)
 *            Type 2  (true-color)         + Type 10 (true-color RLE)
 *            Type 3  (grayscale)          + Type 11 (grayscale RLE)
 *            15/16 bpp RGB555 → RGB24
 *            Palette 8/15/16/24/32 bpp
 *            Orientation top-down/bottom-up via image descriptor
 */
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {

using namespace nkentseu::memory;

// ─── helpers ─────────────────────────────────────────────────────────────────

// Convertit un pixel 15/16-bit (RGB555) en RGB24
static void ReadRGB16(const uint8* src, uint8* dst) noexcept {
    const uint16 px = static_cast<uint16>(src[0]) | (static_cast<uint16>(src[1]) << 8);
    dst[0] = static_cast<uint8>(((px >> 10) & 31) * 255 / 31); // R
    dst[1] = static_cast<uint8>(((px >>  5) & 31) * 255 / 31); // G
    dst[2] = static_cast<uint8>(( px        & 31) * 255 / 31); // B
}

// Retourne le nombre de canaux pour un bpp de palette
static int32 PalBppToChannels(int32 palBits) noexcept {
    if (palBits == 8)  return 1;
    if (palBits == 15 || palBits == 16) return 3; // RGB555 → RGB24
    if (palBits == 24) return 3;
    if (palBits == 32) return 4;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkTGACodec::Decode
// ─────────────────────────────────────────────────────────────────────────────

NkImage* NkTGACodec::Decode(const uint8* data, usize size) noexcept {
    if (size < 18) return nullptr;
    NkImageStream s(data, size);

    // ── En-tête TGA 18 octets ─────────────────────────────────────────────────
    const uint8  idLen        = s.ReadU8();   // longueur du champ Image ID
    const uint8  colormapType = s.ReadU8();   // 0 = pas de palette, 1 = palette
    const uint8  imageType    = s.ReadU8();   // type de l'image

    // Colormap specification (5 octets) — lus pour calculer la taille à sauter
    const uint16 cmapStart    = s.ReadU16LE(); // premier index de palette
    const uint16 cmapLen      = s.ReadU16LE(); // nombre d'entrées de palette
    const uint8  cmapBits     = s.ReadU8();    // bits par entrée de palette

    // Image specification (10 octets)
    s.Skip(4);                              // x/y origin (ignorés)
    const uint16 width        = s.ReadU16LE();
    const uint16 height       = s.ReadU16LE();
    const uint8  bpp          = s.ReadU8();    // bits par pixel (ou bits d'index si palette)
    const uint8  imgDesc      = s.ReadU8();    // image descriptor

    if (width == 0 || height == 0) return nullptr;

    // RLE ? Types >= 8 sont des variantes RLE
    const bool   isRLE     = (imageType >= 8 && imageType <= 11);
    // Indexé (palette) ?
    const bool   isIndexed = (colormapType == 1 &&
                               (imageType == 1 || imageType == 9));
    // Niveaux de gris ?
    const bool   isGray    = (imageType == 3 || imageType == 11);
    // 15/16 bpp (RGB555) ?
    const bool   isRGB16   = (!isIndexed && (bpp == 15 || bpp == 16));

    // Détermine le nombre de canaux et le format de sortie
    int32 channels = 0;
    if (isIndexed) {
        channels = PalBppToChannels(cmapBits);
    } else if (isGray) {
        channels = 1;
    } else if (isRGB16) {
        channels = 3;
    } else {
        channels = (bpp == 32) ? 4 : 3;
    }
    if (channels == 0) return nullptr;

    const NkImagePixelFormat fmt =
        (channels == 1) ? NkImagePixelFormat::NK_GRAY8  :
        (channels == 3) ? NkImagePixelFormat::NK_RGB24  : NkImagePixelFormat::NK_RGBA32;

    NkImage* img = NkImage::Alloc(width, height, fmt);
    if (!img) return nullptr;

    // ── Saute le champ Image ID ───────────────────────────────────────────────
    s.Skip(idLen);

    // ── Lit la palette (si présente) ─────────────────────────────────────────
    uint8* palette    = nullptr;
    bool   palIsRGB16 = (cmapBits == 15 || cmapBits == 16);
    if (isIndexed && cmapLen > 0) {
        // Saute les entrées avant cmapStart
        if (cmapStart > 0) {
            const int32 skipBytes = cmapStart * (palIsRGB16 ? 2 : cmapBits / 8);
            s.Skip(static_cast<usize>(skipBytes));
        }
        const int32 palEntryBytes = palIsRGB16 ? 2 : (cmapBits / 8);
        const usize palBufSize    = static_cast<usize>(cmapLen) * static_cast<usize>(channels);
        palette = static_cast<uint8*>(NkAlloc(palBufSize));
        if (!palette) { img->Free(); return nullptr; }

        for (int32 i = 0; i < cmapLen; ++i) {
            uint8 tmp[4] = {};
            if (palIsRGB16) {
                uint8 raw2[2]; s.ReadBytes(raw2, 2);
                ReadRGB16(raw2, tmp);
            } else {
                s.ReadBytes(tmp, palEntryBytes);
                if (cmapBits == 24) { // BGR → RGB
                    const uint8 t = tmp[0]; tmp[0] = tmp[2]; tmp[2] = t;
                } else if (cmapBits == 32) { // BGRA → RGBA
                    const uint8 t = tmp[0]; tmp[0] = tmp[2]; tmp[2] = t;
                } else if (cmapBits == 8) {
                    // grayscale — déjà correct
                }
            }
            for (int32 c = 0; c < channels; ++c)
                palette[i * channels + c] = tmp[c];
        }
    } else if (colormapType == 1 && !isIndexed) {
        // Palette présente mais image non indexée — skip
        const int32 entryBytes = palIsRGB16 ? 2 : (cmapBits > 0 ? cmapBits / 8 : 0);
        s.Skip(static_cast<usize>(cmapLen) * entryBytes);
    }

    // ── Lit les pixels ────────────────────────────────────────────────────────
    // image descriptor bit 5 : 0 = bottom-up (défaut TGA), 1 = top-down
    const bool topDown = ((imgDesc >> 5) & 1) != 0;

    const int32 pixBytesRaw = isRGB16 ? 2 : (!isIndexed ? bpp / 8 : (bpp / 8));

    // Variables RLE
    int32 rleCount     = 0;
    bool  rleRepeat    = false;
    uint8 rlePix[4]    = {};
    bool  needRead     = true;

    for (int32 i = 0; i < (int32)width * (int32)height; ++i) {

        if (isRLE) {
            if (rleCount == 0) {
                const uint8 cmd = s.ReadU8();
                rleCount  = (cmd & 0x7F) + 1;
                rleRepeat = (cmd & 0x80) != 0;
                needRead  = true;
            } else if (!rleRepeat) {
                needRead = true;
            }
        } else {
            needRead = true;
        }

        if (needRead) {
            if (isIndexed) {
                // Lit l'index (8 ou 16 bits)
                const int32 idx = (bpp == 16) ?
                    static_cast<int32>(s.ReadU16LE()) :
                    static_cast<int32>(s.ReadU8());
                const int32 clampedIdx = (idx >= 0 && idx < cmapLen) ? idx : 0;
                for (int32 c = 0; c < channels; ++c)
                    rlePix[c] = palette ? palette[clampedIdx * channels + c] : 0;
            } else if (isRGB16) {
                uint8 raw2[2]; s.ReadBytes(raw2, 2);
                ReadRGB16(raw2, rlePix);
            } else {
                s.ReadBytes(rlePix, pixBytesRaw);
                if (!isGray) {
                    // BGR → RGB
                    const uint8 t = rlePix[0]; rlePix[0] = rlePix[2]; rlePix[2] = t;
                    // BGRA → RGBA déjà correct après swap R et B
                }
            }
            needRead = false;
        }

        // Calcule la position de destination
        const int32 px = i % width;
        const int32 py = i / width;
        const int32 dstY = topDown ? py : (height - 1 - py);
        uint8* dst = img->RowPtr(dstY) + px * channels;
        for (int32 c = 0; c < channels; ++c) dst[c] = rlePix[c];

        --rleCount;
    }

    if (palette) NkFree(palette);

    if (s.HasError()) { img->Free(); return nullptr; }
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkTGACodec::Encode
// ─────────────────────────────────────────────────────────────────────────────

bool NkTGACodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
    if (!img.IsValid()) return false;
    NkImageStream s;
    const int32 w = img.Width(), h = img.Height(), ch = img.Channels();
    const uint8 bpp     = static_cast<uint8>(ch == 4 ? 32 : ch == 1 ? 8 : 24);
    const uint8 imgType = (ch == 1) ? 3 : 2; // 3=grayscale, 2=true-color (no RLE)

    // Header TGA 18 octets
    s.WriteU8(0);        // ID length
    s.WriteU8(0);        // color map type
    s.WriteU8(imgType);
    s.WriteU8(0); s.WriteU8(0); // color map first entry index
    s.WriteU8(0); s.WriteU8(0); // color map length
    s.WriteU8(0);               // color map entry size
    s.WriteU16LE(0); s.WriteU16LE(0); // x/y origin
    s.WriteU16LE(static_cast<uint16>(w));
    s.WriteU16LE(static_cast<uint16>(h));
    s.WriteU8(bpp);
    s.WriteU8(0x20); // image descriptor : bit5=1 → top-down

    // Pixels top-down, BGR/BGRA
    for (int32 y = 0; y < h; ++y) {
        const uint8* row = img.RowPtr(y);
        for (int32 x = 0; x < w; ++x) {
            const uint8* p = row + x * ch;
            if      (ch == 1) s.WriteU8(p[0]);
            else if (ch == 3) { s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); }
            else              { s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); s.WriteU8(p[3]); }
        }
    }

    // Footer TGA optionnel
    static const char kFooter[] = "\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0";
    s.WriteBytes(reinterpret_cast<const uint8*>(kFooter), 26);
    return s.TakeBuffer(out, outSize);
}

} // namespace nkentseu
