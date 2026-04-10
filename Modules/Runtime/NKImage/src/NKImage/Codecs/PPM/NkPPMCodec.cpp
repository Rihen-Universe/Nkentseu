/**
 * @File    NkPPMCodec.cpp
 * @Brief   Codec PPM/PGM/PBM (P1-P6) production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Fixes vs version précédente
 *  1. PBM binaire (P4) : le décodage bit par bit était incorrect.
 *     L'index de bit dans l'octet source doit être relatif à l'octet courant
 *     de la LIGNE (pas de l'image entière). Chaque ligne commence sur un
 *     nouvel octet dans P4. L'ancienne version mélangeait index pixel et
 *     index bit globaux.
 *  2. PBM ASCII (P1) : la valeur 0 = blanc, 1 = noir (spec NetPBM).
 *     L'ancienne version inversait : 0→0 (noir) et 1→255 (blanc). Corrigé.
 *  3. readUint : renvoie maintenant 0 pour une valeur absente (EOF),
 *     sans provoquer d'accès hors-buffer.
 *  4. Encode : utilise src->Width()*src->Channels() octets exacts par ligne
 *     (et non src->Stride() qui peut inclure du padding).
 *  5. maxVal 16-bit : supporté en décodage (max > 255 → chaque sample 2 octets BE).
 */
#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include <cstdio>

namespace nkentseu {

    using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Helpers de parsing ASCII
    // ─────────────────────────────────────────────────────────────────────────────

    static void SkipWhitespaceAndComments(const uint8*& p, const uint8* end) noexcept {
        for (;;) {
            // Saute les blancs
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                ++p;
            // Saute les commentaires jusqu'à fin de ligne
            if (p < end && *p == '#') {
                while (p < end && *p != '\n') ++p;
            } else {
                break;
            }
        }
    }

    static uint32 ReadUint(const uint8*& p, const uint8* end) noexcept {
        SkipWhitespaceAndComments(p, end);
        if (p >= end) return 0;
        uint32 v = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            v = v * 10u + static_cast<uint32>(*p - '0');
            ++p;
        }
        return v;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkPPMCodec::Decode
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkPPMCodec::Decode(const uint8* data, usize size) noexcept {
        if (size < 3 || data[0] != 'P') return nullptr;
        const uint8* p   = data;
        const uint8* end = data + size;

        ++p; // saute 'P'
        const char magic = static_cast<char>(*p++);
        if (magic < '1' || magic > '6') return nullptr;

        // Dimensions
        const uint32 w = ReadUint(p, end);
        const uint32 h = ReadUint(p, end);
        if (w == 0 || h == 0) return nullptr;

        // maxVal (absent pour PBM P1/P4)
        uint32 maxVal = 1;
        if (magic != '1' && magic != '4') {
            maxVal = ReadUint(p, end);
            if (maxVal == 0 || maxVal > 65535) return nullptr;
        }

        // Séparateur obligatoire (exactement un blanc après le dernier entête)
        if (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;

        const bool isBinary  = (magic >= '4');
        const bool isGray    = (magic == '2' || magic == '5');
        const bool isBitmap  = (magic == '1' || magic == '4');
        const bool is16bit   = (maxVal > 255); // chaque sample sur 2 octets BE en binaire

        const int32 ch = (isGray || isBitmap) ? 1 : 3;
        const NkImagePixelFormat fmt =
            (ch == 1) ? NkImagePixelFormat::NK_GRAY8 : NkImagePixelFormat::NK_RGB24;

        NkImage* img = NkImage::Alloc(static_cast<int32>(w), static_cast<int32>(h), fmt);
        if (!img) return nullptr;

        for (uint32 y = 0; y < h; ++y) {
            uint8* row = img->RowPtr(static_cast<int32>(y));

            if (isBitmap) {
                if (isBinary) {
                    // P4 : pixels packed, 1 bit par pixel, MSB en premier.
                    // FIX: chaque ligne commence sur un nouvel octet.
                    const uint32 rowBytes = (w + 7) / 8;
                    for (uint32 bx = 0; bx < rowBytes && p < end; ++bx, ++p) {
                        const uint8 byte = *p;
                        for (int32 bit = 7; bit >= 0; --bit) {
                            const uint32 px = bx * 8u + static_cast<uint32>(7 - bit);
                            if (px < w) {
                                // FIX: 0 = noir (bit set), 1 = blanc (bit clear) — spec P4
                                row[px] = ((byte >> bit) & 1) ? 0 : 255;
                            }
                        }
                    }
                } else {
                    // P1 ASCII : '0' = blanc, '1' = noir (spec NetPBM)
                    for (uint32 x = 0; x < w; ++x) {
                        const uint32 v = ReadUint(p, end);
                        // FIX: v==1 → noir (0), v==0 → blanc (255)
                        row[x] = (v == 0) ? 255 : 0;
                    }
                }
            } else if (isBinary) {
                if (is16bit) {
                    // 16-bit big-endian : on ne garde que l'octet de poids fort
                    for (uint32 x = 0; x < w * static_cast<uint32>(ch); ++x) {
                        const uint8 hi = (p < end) ? *p++ : 0;
                        const uint8 lo = (p < end) ? *p++ : 0; (void)lo;
                        row[x] = hi; // on garde seulement le MSB → 8-bit
                    }
                } else {
                    // 8-bit binaire : copie directe
                    const usize lineBytes = static_cast<usize>(w) * ch;
                    const usize avail = static_cast<usize>(end - p);
                    const usize toCopy = (avail < lineBytes) ? avail : lineBytes;
                    NkCopy(row, p, toCopy);
                    p += lineBytes; // avance même si dépassement (on a clampé toCopy)
                }
            } else {
                // ASCII P2/P3 : un entier par sample
                for (uint32 x = 0; x < w * static_cast<uint32>(ch); ++x) {
                    const uint32 v = ReadUint(p, end);
                    row[x] = static_cast<uint8>(maxVal > 0 ? (v * 255u / maxVal) : 0);
                }
            }
        }

        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkPPMCodec::Encode
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkPPMCodec::Encode(const NkImage& img, const char* path) noexcept {
        if (!img.IsValid() || !path) return false;

        FILE* f = ::fopen(path, "wb");
        if (!f) return false;

        const NkImage* src  = &img;
        NkImage*       conv = nullptr;

        if (img.Channels() == 1) {
            // P5 grayscale
            ::fprintf(f, "P5\n%d %d\n255\n", img.Width(), img.Height());
        } else {
            // Convertit vers RGB24 si nécessaire
            if (img.Format() != NkImagePixelFormat::NK_RGB24) {
                conv = img.Convert(NkImagePixelFormat::NK_RGB24);
                if (!conv) { ::fclose(f); return false; }
                src = conv;
            }
            // P6 RGB
            ::fprintf(f, "P6\n%d %d\n255\n", src->Width(), src->Height());
        }

        // FIX: écriture pixel par pixel (Width*Channels octets, pas Stride)
        const int32 rowDataBytes = src->Width() * src->Channels();
        for (int32 y = 0; y < src->Height(); ++y) {
            if (::fwrite(src->RowPtr(y), 1, static_cast<usize>(rowDataBytes), f)
                    != static_cast<usize>(rowDataBytes))
            {
                ::fclose(f);
                if (conv) conv->Free();
                return false;
            }
        }

        ::fclose(f);
        if (conv) conv->Free();
        return true;
    }

} // namespace nkentseu