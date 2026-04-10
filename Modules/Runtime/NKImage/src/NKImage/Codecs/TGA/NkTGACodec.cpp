/**
 * @File    NkTGACodec.cpp
 * @Brief   Codec TGA (Truevision TARGA) — algorithme adapté de stb_image (public domain).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Fixes vs version précédente
 *  1. RLE : la variable needRead n'était pas réinitialisée correctement dans
 *     le cas non-RLE. En mode non-RLE needRead doit TOUJOURS être true au
 *     début de chaque pixel. Corrigé.
 *  2. Palette : le nombre d'octets à sauter avant la palette (cmapStart)
 *     était calculé sur la taille en bits et non en octets. Corrigé.
 *  3. Palette 32bpp : l'ordre BGRA→RGBA n'était pas appliqué (seul le swap
 *     R/B était fait mais pas indépendamment). Corrigé.
 *  4. Channels de sortie pour palette : PalBppToChannels(8) retournait 1
 *     (grayscale). Une palette 8bpp peut très bien contenir des couleurs —
 *     on retourne toujours au moins 3 canaux pour les images indexées couleur.
 *     Seul un grayscale explicite (imageType==3/11) retourne 1 canal.
 *  5. isGray : était évalué sur imageType sans tenir compte de isRLE
 *     (types 3 et 11). Corrigé.
 */
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {

    using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Helpers
    // ─────────────────────────────────────────────────────────────────────────────

    static void ReadRGB16(const uint8* src, uint8* dst) noexcept {
        const uint16 px = static_cast<uint16>(src[0]) | (static_cast<uint16>(src[1]) << 8);
        dst[0] = static_cast<uint8>(((px >> 10) & 31) * 255 / 31); // R
        dst[1] = static_cast<uint8>(((px >>  5) & 31) * 255 / 31); // G
        dst[2] = static_cast<uint8>(( px        & 31) * 255 / 31); // B
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkTGACodec::Decode
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkTGACodec::Decode(const uint8* data, usize size) noexcept {
        if (size < 18) return nullptr;
        NkImageStream s(data, size);

        // ── En-tête TGA 18 octets ─────────────────────────────────────────────────
        const uint8  idLen        = s.ReadU8();
        const uint8  colormapType = s.ReadU8();
        const uint8  imageType    = s.ReadU8();
        const uint16 cmapStart    = s.ReadU16LE();
        const uint16 cmapLen      = s.ReadU16LE();
        const uint8  cmapBits     = s.ReadU8();
        s.Skip(4);                              // x/y origin
        const uint16 width        = s.ReadU16LE();
        const uint16 height       = s.ReadU16LE();
        const uint8  bpp          = s.ReadU8();
        const uint8  imgDesc      = s.ReadU8();

        if (width == 0 || height == 0) return nullptr;

        // Types supportés
        const bool isRLE     = (imageType == 9 || imageType == 10 || imageType == 11);
        const bool isIndexed = (colormapType == 1 && (imageType == 1 || imageType == 9));
        // FIX: isGray doit couvrir les types RLE grayscale (11) aussi
        const bool isGray    = (imageType == 3 || imageType == 11);
        const bool isRGB16   = (!isIndexed && !isGray && (bpp == 15 || bpp == 16));

        // Nombre de canaux de sortie
        int32 channels = 0;
        if (isIndexed) {
            // FIX: toujours au moins 3 canaux pour une palette couleur
            if (cmapBits == 32) channels = 4;
            else                channels = 3;
        } else if (isGray) {
            channels = 1;
        } else if (isRGB16) {
            channels = 3;
        } else {
            channels = (bpp == 32) ? 4 : 3;
        }
        if (channels == 0) return nullptr;

        const NkImagePixelFormat fmt =
            (channels == 1) ? NkImagePixelFormat::NK_GRAY8 :
            (channels == 3) ? NkImagePixelFormat::NK_RGB24 : NkImagePixelFormat::NK_RGBA32;

        NkImage* img = NkImage::Alloc(width, height, fmt);
        if (!img) return nullptr;

        // ── Image ID ─────────────────────────────────────────────────────────────
        s.Skip(idLen);

        // ── Palette ──────────────────────────────────────────────────────────────
        uint8* palette    = nullptr;
        const bool palIsRGB16 = (cmapBits == 15 || cmapBits == 16);

        if (isIndexed && cmapLen > 0) {
            // FIX: sauter cmapStart entrées (en OCTETS, pas en bits)
            const int32 palEntryBytes = palIsRGB16 ? 2 : (cmapBits / 8);
            if (cmapStart > 0)
                s.Skip(static_cast<usize>(cmapStart) * palEntryBytes);

            const usize palBufSize = static_cast<usize>(cmapLen) * channels;
            palette = static_cast<uint8*>(NkAlloc(palBufSize));
            if (!palette) { img->Free(); return nullptr; }

            for (int32 i = 0; i < cmapLen; ++i) {
                uint8 tmp[4] = {0, 0, 0, 255};
                if (palIsRGB16) {
                    uint8 raw2[2]; s.ReadBytes(raw2, 2);
                    ReadRGB16(raw2, tmp); // donne directement RGB
                } else if (cmapBits == 24) {
                    s.ReadBytes(tmp, 3); // BGR dans le fichier
                    // FIX: swap B↔R pour obtenir RGB
                    const uint8 t = tmp[0]; tmp[0] = tmp[2]; tmp[2] = t;
                } else if (cmapBits == 32) {
                    s.ReadBytes(tmp, 4); // BGRA dans le fichier
                    // FIX: swap B↔R pour obtenir RGBA
                    const uint8 t = tmp[0]; tmp[0] = tmp[2]; tmp[2] = t;
                } else {
                    s.ReadBytes(tmp, palEntryBytes);
                }
                for (int32 c = 0; c < channels; ++c)
                    palette[i * channels + c] = tmp[c];
            }
        } else if (colormapType == 1 && !isIndexed) {
            // Palette présente mais non utilisée — skip
            const int32 entryBytes = palIsRGB16 ? 2 : (cmapBits > 0 ? cmapBits / 8 : 0);
            s.Skip(static_cast<usize>(cmapLen) * entryBytes);
        }

        // ── Lecture des pixels ────────────────────────────────────────────────────
        const bool topDown = ((imgDesc >> 5) & 1) != 0;
        const int32 pixBytesRaw = isRGB16 ? 2 : (bpp / 8);

        int32 rleCount  = 0;
        bool  rleRepeat = false;
        uint8 rlePix[4] = {0, 0, 0, 255};

        for (int32 i = 0; i < static_cast<int32>(width) * static_cast<int32>(height); ++i) {
            bool needRead = false;

            if (isRLE) {
                if (rleCount == 0) {
                    const uint8 cmd = s.ReadU8();
                    rleCount  = (cmd & 0x7F) + 1;
                    rleRepeat = (cmd & 0x80) != 0;
                    needRead  = true; // toujours lire pour le premier pixel du packet
                } else if (!rleRepeat) {
                    needRead = true; // absolute packet : lire un nouveau pixel
                }
                // Si rleRepeat et rleCount > 0 : réutiliser rlePix, pas de lecture
            } else {
                // FIX: en mode non-RLE, on lit TOUJOURS un nouveau pixel
                needRead = true;
            }

            if (needRead) {
                if (isIndexed) {
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
                        // BGR(A) → RGB(A) : swap R et B
                        const uint8 t = rlePix[0]; rlePix[0] = rlePix[2]; rlePix[2] = t;
                    }
                }
            }

            // Destination
            const int32 px   = i % width;
            const int32 py   = i / width;
            const int32 dstY = topDown ? py : (static_cast<int32>(height) - 1 - py);
            uint8* dst = img->RowPtr(dstY) + px * channels;
            NkCopy(dst, rlePix, channels);

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
        const int32 w  = img.Width();
        const int32 h  = img.Height();
        const int32 ch = img.Channels();
        const uint8 bppOut  = static_cast<uint8>(ch == 4 ? 32 : ch == 1 ? 8 : 24);
        const uint8 imgType = (ch == 1) ? 3u : 2u; // 3=grayscale, 2=true-color

        // Header TGA 18 octets
        s.WriteU8(0);         // ID length
        s.WriteU8(0);         // color map type
        s.WriteU8(imgType);
        // color map specification (5 octets à zéro)
        s.WriteU8(0); s.WriteU8(0);
        s.WriteU8(0); s.WriteU8(0);
        s.WriteU8(0);
        // image specification
        s.WriteU16LE(0); s.WriteU16LE(0); // x/y origin
        s.WriteU16LE(static_cast<uint16>(w));
        s.WriteU16LE(static_cast<uint16>(h));
        s.WriteU8(bppOut);
        s.WriteU8(0x20); // image descriptor : bit5=1 → top-down

        // Pixels top-down, BGR/BGRA
        for (int32 y = 0; y < h; ++y) {
            const uint8* row = img.RowPtr(y);
            for (int32 x = 0; x < w; ++x) {
                const uint8* p = row + x * ch;
                if      (ch == 1) { s.WriteU8(p[0]); }
                else if (ch == 3) { s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); }
                else              { s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); s.WriteU8(p[3]); }
            }
        }

        // Footer TGA optionnel (26 octets)
        const uint8 kFooter[26] = {
            0,0,0,0, 0,0,0,0, // extension area offset + developer dir offset
            'T','R','U','E','V','I','S','I','O','N','-','X','F','I','L','E','.','\0'
        };
        s.WriteBytes(kFooter, 26);
        return s.TakeBuffer(out, outSize);
    }

} // namespace nkentseu