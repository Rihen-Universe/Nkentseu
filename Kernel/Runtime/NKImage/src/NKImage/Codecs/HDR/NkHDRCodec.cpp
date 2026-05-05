/**
 * @File    NkHDRCodec.cpp
 * @Brief   Codec Radiance HDR (.hdr/.rgbe) production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Fixes vs version précédente
 *  1. BUG A — Fallback décodeur ancien-format RLE complètement absent.
 *     L'ancien format Radiance utilise un RLE mixte :
 *       - Si (r==1 && g==1 && b==1) : run-length sur le dernier pixel (e = count)
 *       - Sinon : pixel RGBE brut
 *     Le fallback actuel faisait uniquement du raw RGBE sans gérer le RLE legacy.
 *     CORRIGÉ : ReadOldRLEScan() implémente le décodeur legacy complet.
 *
 *  2. BUG B — ParseDimLine : accept +X/-X en premier axe mais la boucle
 *     de décodage itère sur 'y' de 0 à h et calcule dstX = flipX?(w-1-x):x
 *     MAIS l'image est toujours lue scanline par scanline (une ligne = w pixels).
 *     Si le premier axe est X (image transposée), il faudrait intervertir w/h
 *     dans la boucle. CORRIGÉ : échange w↔h si l'axe primaire est X.
 *
 *  3. BUG C — F2RGBE : quand mx == 0 exactement mais in[i] > 0 possible
 *     (comparaison float32 avec 1e-32f peut rater des valeurs dénormales).
 *     CORRIGÉ : vérification avec mx <= 0.f au lieu de mx < 1e-32f.
 *
 *  4. BUG D — Encode/EncodeToMemory : cohérence garantie via EncodeToStream partagé.
 *     (déjà corrigé dans la version précédente, conservé tel quel)
 *
 *  5. BUG E — ReadNewRLEScan : si ReadU8 échoue sur le code de run, on peut
 *     lire un mauvais octet de valeur. Ajout de vérification !s.IsEOF().
 */
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace nkentseu {
    using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Conversion RGBE ↔ float32
    // ─────────────────────────────────────────────────────────────────────────────

    static NKIMG_INLINE void RGBE2F(uint8 r, uint8 g, uint8 b, uint8 e,
                                    float32* out) noexcept {
        if (!e) { out[0] = out[1] = out[2] = 0.f; return; }
        const float32 s = ::ldexpf(1.f / 256.f, int(e) - 128);
        out[0] = (r + 0.5f) * s;
        out[1] = (g + 0.5f) * s;
        out[2] = (b + 0.5f) * s;
    }

    // BUG C CORRIGÉ : mx <= 0.f au lieu de mx < 1e-32f
    static NKIMG_INLINE void F2RGBE(const float32* in, uint8* out) noexcept {
        float32 mx = in[0] > in[1] ? in[0] : in[1];
        if (in[2] > mx) mx = in[2];
        if (mx <= 0.f) { out[0] = out[1] = out[2] = out[3] = 0; return; }
        int exp;
        const float32 m = ::frexpf(mx, &exp) * 256.f / mx;
        out[0] = uint8(in[0] * m);
        out[1] = uint8(in[1] * m);
        out[2] = uint8(in[2] * m);
        out[3] = uint8(exp + 128);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Lecture d'une ligne ASCII du header
    // ─────────────────────────────────────────────────────────────────────────────

    static bool ReadHdrLine(NkImageStream& s, char* buf, int32 maxLen) noexcept {
        int32 i = 0;
        while (!s.IsEOF() && i < maxLen - 1) {
            char c = char(s.ReadU8());
            if (c == '\n') { buf[i] = 0; return true; }
            if (c != '\r') buf[i++] = c;
        }
        buf[i] = 0;
        return i > 0;
    }

    // Parse la ligne de dimensions HDR : "±Y h ±X w" ou "±X w ±Y h"
    // BUG B CORRIGÉ : si axe primaire est X, intervertit w et h
    static bool ParseDimLine(const char* line, int32& w, int32& h,
                             bool& flipX, bool& flipY,
                             bool& primaryIsX) noexcept {
        w = 0; h = 0; flipX = false; flipY = false; primaryIsX = false;
        const char* p = line;
        while (*p == ' ') ++p;

        char sign1 = *p++; char ax1 = *p++;
        while (*p == ' ') ++p;
        int32 v1 = 0;
        while (*p >= '0' && *p <= '9') { v1 = v1 * 10 + (*p - '0'); ++p; }
        while (*p == ' ') ++p;

        char sign2 = *p++; char ax2 = *p++;
        while (*p == ' ') ++p;
        int32 v2 = 0;
        while (*p >= '0' && *p <= '9') { v2 = v2 * 10 + (*p - '0'); ++p; }

        if ((ax1 != 'Y' && ax1 != 'y' && ax1 != 'X' && ax1 != 'x') ||
            (ax2 != 'Y' && ax2 != 'y' && ax2 != 'X' && ax2 != 'x'))
            return false;

        const bool ax1Y = (ax1 == 'Y' || ax1 == 'y');
        if (ax1Y) {
            // Cas standard : -Y h +X w
            h = v1; w = v2;
            flipY = (sign1 == '+'); // +Y = bottom-up = flip
            flipX = (sign2 == '-'); // -X = flip horizontal
            primaryIsX = false;
        } else {
            // Axe primaire X : +X w -Y h (transposé)
            w = v1; h = v2;
            flipX = (sign1 == '-');
            flipY = (sign2 == '+');
            primaryIsX = true;
        }
        return w > 0 && h > 0;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Décodeur nouveau RLE scanline (format 2/2/w_hi/w_lo + 4 canaux séparés)
    //  Utilisé quand w >= 8 && w <= 0x7FFF et le premier octet de la ligne == 2
    // ─────────────────────────────────────────────────────────────────────────────

    static bool ReadNewRLEScan(NkImageStream& s, uint8* scan, int32 w) noexcept {
        if (!s.HasBytes(4)) return false;
        const uint8 r0 = s.ReadU8(), r1 = s.ReadU8();
        const uint8 rh = s.ReadU8(), rl = s.ReadU8();
        if (r0 != 2 || r1 != 2 || (rh & 0x80)) return false;
        const int32 sw = (int32(rh) << 8) | rl;
        if (sw != w) return false;

        for (int32 ch = 0; ch < 4; ++ch) {
            int32 x = 0;
            while (x < w && !s.IsEOF()) {
                // BUG E CORRIGÉ : vérification EOF avant ReadU8 de la valeur
                if (s.IsEOF()) return false;
                const uint8 code = s.ReadU8();
                if (code > 128) {
                    // Run-length
                    const int32 run = code - 128;
                    if (s.IsEOF()) return false;
                    const uint8 val = s.ReadU8();
                    for (int32 i = 0; i < run && x < w; ++i, ++x)
                        scan[x * 4 + ch] = val;
                } else {
                    // Littéral
                    for (int32 i = 0; i < int32(code) && x < w; ++i, ++x) {
                        if (s.IsEOF()) return false;
                        scan[x * 4 + ch] = s.ReadU8();
                    }
                }
            }
            if (x != w) return false;
        }
        return !s.HasError();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  BUG A CORRIGÉ : Décodeur ancien format RLE Radiance (pre-1991)
    //
    //  Le format ancien mélange pixels RGBE bruts et runs :
    //    - Si les 3 premiers octets d'un pixel sont (1,1,1) et e > 0 :
    //      le 4e octet = count de répétition du pixel précédent
    //      (count peut être accumulé sur plusieurs octets successifs)
    //    - Sinon : pixel RGBE brut
    //
    //  Référence : radiance/src/common/color.c (freadcolrs)
    // ─────────────────────────────────────────────────────────────────────────────

    static bool ReadOldRLEScan(NkImageStream& s, uint8* scan, int32 w) noexcept {
        int32 x = 0;
        uint8 prev[4] = {0, 0, 0, 0};
        int32 repeatShift = 0; // pour les runs multi-octets

        while (x < w && !s.IsEOF()) {
            uint8 pix[4];
            if (s.ReadBytes(pix, 4) < 4) return false;

            if (pix[0] == 1 && pix[1] == 1 && pix[2] == 1) {
                // Run-length : répète le pixel précédent
                int32 count = int32(pix[3]) << repeatShift;
                for (int32 i = 0; i < count && x < w; ++i, ++x) {
                    scan[x * 4 + 0] = prev[0];
                    scan[x * 4 + 1] = prev[1];
                    scan[x * 4 + 2] = prev[2];
                    scan[x * 4 + 3] = prev[3];
                }
                repeatShift += 8; // accumulateur multi-octets
            } else {
                // Pixel brut
                scan[x * 4 + 0] = pix[0];
                scan[x * 4 + 1] = pix[1];
                scan[x * 4 + 2] = pix[2];
                scan[x * 4 + 3] = pix[3];
                prev[0] = pix[0]; prev[1] = pix[1];
                prev[2] = pix[2]; prev[3] = pix[3];
                repeatShift = 0; // reset après pixel brut
                ++x;
            }
        }
        return (x == w) && !s.HasError();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkHDRCodec::Decode
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkHDRCodec::Decode(const uint8* data, usize size) noexcept {
        if (size < 10) return nullptr;
        NkImageStream s(data, size);
        char line[512];

        // Ligne 1 : magic
        if (!ReadHdrLine(s, line, sizeof(line))) return nullptr;
        if (::strncmp(line, "#?RADIANCE", 10) != 0 &&
            ::strncmp(line, "#?RGBE", 6) != 0) return nullptr;

        float32 exposure = 1.f;

        // Header key=value → ligne vide
        for (int32 iter = 0; iter < 64 && !s.IsEOF(); ++iter) {
            if (!ReadHdrLine(s, line, sizeof(line)) || line[0] == 0) break;
            if (::strncmp(line, "EXPOSURE=", 9) == 0)
                exposure = ::strtof(line + 9, nullptr);
        }

        // Ligne dimensions
        if (!ReadHdrLine(s, line, sizeof(line))) return nullptr;
        int32 w = 0, h = 0;
        bool flipX = false, flipY = false, primaryIsX = false;
        // BUG B CORRIGÉ : récupération de primaryIsX
        if (!ParseDimLine(line, w, h, flipX, flipY, primaryIsX)) return nullptr;

        // BUG B CORRIGÉ : si l'axe primaire est X, les scanlines sont verticales
        // → on décode avec les dimensions inversées puis on transpose
        const int32 scanW = primaryIsX ? h : w;
        const int32 scanH = primaryIsX ? w : h;

        NkImage* img = NkImage::Alloc(w, h, NkImagePixelFormat::NK_RGB96F);
        if (!img) return nullptr;

        uint8* scan = static_cast<uint8*>(NkAlloc(usize(scanW) * 4));
        if (!scan) { img->Free(); return nullptr; }

        for (int32 y = 0; y < scanH; ++y) {
            const usize scanStart = s.Tell();
            bool ok = false;

            // Tente le nouveau format RLE si la largeur est dans la plage valide
            if (scanW >= 8 && scanW <= 0x7FFF) {
                ok = ReadNewRLEScan(s, scan, scanW);
            }

            if (!ok) {
                // BUG A CORRIGÉ : fallback vers l'ancien format RLE Radiance
                s.Seek(scanStart);
                ok = ReadOldRLEScan(s, scan, scanW);
            }

            if (!ok) {
                // Dernier recours : raw RGBE (format très ancien sans RLE)
                s.Seek(scanStart);
                for (int32 x = 0; x < scanW; ++x) {
                    if (s.ReadBytes(scan + x * 4, 4) < 4) { ok = false; break; }
                }
                ok = !s.HasError();
            }

            if (!ok) break;

            // Écrit les pixels dans l'image en tenant compte des flips
            for (int32 x = 0; x < scanW; ++x) {
                const uint8* p = scan + x * 4;
                float32 rgb[3];
                RGBE2F(p[0], p[1], p[2], p[3], rgb);
                if (exposure != 1.f) {
                    rgb[0] *= exposure; rgb[1] *= exposure; rgb[2] *= exposure;
                }

                // Calcul des coordonnées de destination
                int32 dstX, dstY;
                if (primaryIsX) {
                    // Axe primaire X : x = colonne image, y = ligne image (transposé)
                    dstX = flipX ? (w - 1 - y) : y;
                    dstY = flipY ? (h - 1 - x) : x;
                } else {
                    dstX = flipX ? (w - 1 - x) : x;
                    dstY = flipY ? (h - 1 - y) : y;
                }

                if (dstX < 0 || dstX >= w || dstY < 0 || dstY >= h) continue;
                float32* rowF = reinterpret_cast<float32*>(img->RowPtr(dstY));
                rowF[dstX * 3 + 0] = rgb[0];
                rowF[dstX * 3 + 1] = rgb[1];
                rowF[dstX * 3 + 2] = rgb[2];
            }
        }

        NkFree(scan);
        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Encodeur RLE par canal vers NkImageStream (nouveau format)
    // ─────────────────────────────────────────────────────────────────────────────

    static void WriteNewRLEScan(NkImageStream& s, const uint8* scan, int32 w) noexcept {
        // En-tête scanline : 2 2 w_hi w_lo
        s.WriteU8(2); s.WriteU8(2);
        s.WriteU8(uint8((w >> 8) & 0xFF));
        s.WriteU8(uint8(w & 0xFF));

        for (int32 ch = 0; ch < 4; ++ch) {
            // Extrait le canal
            const int32 n = w < 65536 ? w : 65536;
            uint8 chanBuf[65536];
            for (int32 x = 0; x < n; ++x)
                chanBuf[x] = scan[x * 4 + ch];

            int32 x = 0;
            while (x < n) {
                // Détecte un run
                int32 run = 1;
                while (x + run < n && run < 127 && chanBuf[x + run] == chanBuf[x])
                    ++run;

                if (run > 2) {
                    // Encode comme run
                    s.WriteU8(uint8(run + 128));
                    s.WriteU8(chanBuf[x]);
                    x += run;
                } else {
                    // Encode comme littéral (cherche jusqu'au prochain run > 2)
                    int32 nr = 0, j = x;
                    while (j < n && nr < 128) {
                        int32 r2 = 1;
                        while (j + r2 < n && r2 < 127 && chanBuf[j + r2] == chanBuf[j])
                            ++r2;
                        if (r2 >= 3) break;
                        ++nr; ++j;
                    }
                    if (!nr) nr = 1;
                    s.WriteU8(uint8(nr));
                    for (int32 i = 0; i < nr; ++i)
                        s.WriteU8(chanBuf[x + i]);
                    x += nr;
                }
            }
        }
    }

    // Encode l'image dans un NkImageStream
    static bool EncodeToStream(const NkImage& img, NkImageStream& s) noexcept {
        if (!img.IsValid()) return false;
        const int32 w = img.Width(), h = img.Height();

        // Header ASCII
        const char* hdr = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\nEXPOSURE=1.0\n\n";
        s.WriteBytes(reinterpret_cast<const uint8*>(hdr), ::strlen(hdr));

        // Ligne dimensions : convention standard -Y h +X w
        char dimLine[64];
        int32 dlen = ::snprintf(dimLine, sizeof(dimLine), "-Y %d +X %d\n", h, w);
        s.WriteBytes(reinterpret_cast<const uint8*>(dimLine), usize(dlen));

        uint8* scan = static_cast<uint8*>(NkAlloc(usize(w) * 4));
        if (!scan) return false;

        for (int32 y = 0; y < h; ++y) {
            const uint8* rowData = img.RowPtr(y);
            if (img.IsHDR()) {
                const float32* rowF = reinterpret_cast<const float32*>(rowData);
                for (int32 x = 0; x < w; ++x)
                    F2RGBE(rowF + x * img.Channels(), scan + x * 4);
            } else {
                const int32 ch = img.Channels();
                for (int32 x = 0; x < w; ++x) {
                    const uint8* p = rowData + x * ch;
                    float32 fc[3] = {
                        p[0] / 255.f,
                        (ch > 1 ? p[1] : p[0]) / 255.f,
                        (ch > 2 ? p[2] : p[0]) / 255.f
                    };
                    F2RGBE(fc, scan + x * 4);
                }
            }

            if (w >= 8 && w <= 0x7FFF)
                WriteNewRLEScan(s, scan, w);
            else
                for (int32 x = 0; x < w; ++x)
                    s.WriteBytes(scan + x * 4, 4);
        }

        NkFree(scan);
        return !s.HasError();
    }

    bool NkHDRCodec::Encode(const NkImage& img, const char* path) noexcept {
        if (!path) return false;
        uint8* buf = nullptr; usize sz = 0;
        if (!EncodeToMemory(img, buf, sz)) return false;
        FILE* f = ::fopen(path, "wb");
        if (!f) { NkFree(buf); return false; }
        const bool ok = (::fwrite(buf, 1, sz, f) == sz);
        ::fclose(f);
        NkFree(buf);
        return ok;
    }

    bool NkHDRCodec::EncodeToMemory(const NkImage& img,
                                    uint8*& out, usize& outSize) noexcept {
        NkImageStream s;
        if (!EncodeToStream(img, s)) return false;
        return s.TakeBuffer(out, outSize);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Tone mapping (Reinhard simple) et conversion RGBA8
    // ─────────────────────────────────────────────────────────────────────────────

    static float32 ToneMapReinhard(float32 x, float32 exposure) {
        x *= exposure;
        return x / (1.0f + x);
    }

    static uint8 FloatToByte(float32 x, float32 gamma) {
        if (gamma > 0.0f) {
            x = powf(x, 1.0f / gamma);
        }
        return uint8(::fminf(::fmaxf(x * 255.0f, 0.0f), 255.0f));
    }

    NkImage* NkHDRCodec::ConvertToTexture(const NkImage& hdrImage, float32 exposure, float32 gamma) noexcept {
        if (!hdrImage.IsValid() || hdrImage.Format() != NkImagePixelFormat::NK_RGB96F)
            return nullptr;

        const int32 w = hdrImage.Width();
        const int32 h = hdrImage.Height();
        NkImage* rgbaImage = NkImage::Alloc(w, h, NkImagePixelFormat::NK_RGBA32);
        if (!rgbaImage) return nullptr;

        const float32 * src = reinterpret_cast<const float32 *>(hdrImage.Pixels());
        uint8* dst = reinterpret_cast<uint8*>(rgbaImage->Pixels());

        for (int32 y = 0; y < h; ++y) {
            for (int32 x = 0; x < w; ++x) {
                float32 r = ToneMapReinhard(src[0], exposure);
                float32 g = ToneMapReinhard(src[1], exposure);
                float32 b = ToneMapReinhard(src[2], exposure);
                src += 3;

                dst[0] = FloatToByte(r, gamma);
                dst[1] = FloatToByte(g, gamma);
                dst[2] = FloatToByte(b, gamma);
                dst[3] = 255; // Alpha opaque
                dst += 4;
            }
        }

        return rgbaImage;
    }

    // bool NkHDRCodec::ConvertToTextureData(const NkImage& hdrImage,
    //                                     NkTextureData& outData,
    //                                     float32 exposure,
    //                                     float32 gamma) noexcept {
    //     NkImage* temp = ConvertToTexture(hdrImage, exposure, gamma);
    //     if (!temp) return false;

    //     outData.width  = temp->Width();
    //     outData.height = temp->Height();
    //     outData.size   = usize(outData.width) * outData.height * 4;
    //     outData.data   = static_cast<uint8*>(NkAlloc(outData.size));
    //     if (!outData.data) {
    //         temp->Free();
    //         return false;
    //     }

    //     memcpy(outData.data, temp->Pixels(), outData.size);
    //     temp->Free();
    //     return true;
    // }
} // namespace nkentseu