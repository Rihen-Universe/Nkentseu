/**
 * @File    NkSTBCodec.cpp
 * @Brief   Implémentation du backend STB pour NKImage.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * Ce fichier contient les définitions d'implémentation STB (exactement une
 * unité de traduction par bibliothèque header-only).
 */

// ─────────────────────────────────────────────────────────────────────────────
//  Activation des implémentations STB (une seule fois dans tout le projet)
// ─────────────────────────────────────────────────────────────────────────────

// stb_image: désactive les formats peu utilisés pour réduire la taille binaire
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STB_IMAGE_IMPLEMENTATION
#include "../../STB/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../STB/stb_image_write.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../../STB/stb_image_resize2.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Includes NKImage
// ─────────────────────────────────────────────────────────────────────────────

#include "NKImage/Codecs/STB/NkSTBCodec.h"

#include <string.h>   // strlen, strchr, strcasecmp / _stricmp
#include <stdlib.h>   // malloc, free, memcpy
#include <stdio.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers internes
// ─────────────────────────────────────────────────────────────────────────────

namespace {

    // Conversion nombre de canaux STB → NkImagePixelFormat
    nkentseu::NkImagePixelFormat ChannelsToFormat(int ch) noexcept {
        using F = nkentseu::NkImagePixelFormat;
        switch (ch) {
            case 1:  return F::NK_GRAY8;
            case 2:  return F::NK_GRAY_A16;
            case 3:  return F::NK_RGB24;
            case 4:  return F::NK_RGBA32;
            default: return F::NK_UNKNOW;
        }
    }

    // Extraction de l'extension (en minuscules) depuis un chemin.
    // Retourne un pointeur statique de 8 chars max, ou "" si absent.
    const char* GetExt(const char* path) noexcept {
        if (!path) return "";
        const char* dot = nullptr;
        for (const char* p = path; *p; ++p)
            if (*p == '.') dot = p;
        return dot ? dot + 1 : "";
    }

    // Comparaison d'extension insensible à la casse
    bool ExtEq(const char* ext, const char* ref) noexcept {
        if (!ext || !ref) return false;
        while (*ext && *ref) {
            char a = *ext >= 'A' && *ext <= 'Z' ? (char)(*ext + 32) : *ext;
            char b = *ref >= 'A' && *ref <= 'Z' ? (char)(*ref + 32) : *ref;
            if (a != b) return false;
            ++ext; ++ref;
        }
        return *ext == 0 && *ref == 0;
    }

    // Alloue une NkImage et copie les pixels depuis un buffer stbi.
    // Libère le buffer stbi après copie.
    nkentseu::NkImage* WrapSTBI(
        stbi_uc* stbiPixels, int w, int h, int ch
    ) noexcept {
        if (!stbiPixels) return nullptr;
        auto fmt = ChannelsToFormat(ch);
        if (fmt == nkentseu::NkImagePixelFormat::NK_UNKNOW) {
            stbi_image_free(stbiPixels);
            return nullptr;
        }
        nkentseu::NkImage* img = nkentseu::NkImage::Alloc(w, h, fmt);
        if (!img) {
            stbi_image_free(stbiPixels);
            return nullptr;
        }
        // NkImage::Alloc alloue avec malloc — on copie depuis stbi
        const nkentseu::usize sz = (nkentseu::usize)w * h * ch;
        ::memcpy(img->Pixels(), stbiPixels, sz);
        stbi_image_free(stbiPixels);
        return img;
    }

    // Même chose pour les pixels float HDR
    nkentseu::NkImage* WrapSTBIFloat(
        float* stbiPixels, int w, int h, int ch
    ) noexcept {
        if (!stbiPixels) return nullptr;
        nkentseu::NkImagePixelFormat fmt;
        if      (ch == 4) fmt = nkentseu::NkImagePixelFormat::NK_RGBA128F;
        else if (ch == 3) fmt = nkentseu::NkImagePixelFormat::NK_RGB96F;
        else {
            stbi_image_free(stbiPixels);
            return nullptr;
        }
        nkentseu::NkImage* img = nkentseu::NkImage::Alloc(w, h, fmt);
        if (!img) {
            stbi_image_free(stbiPixels);
            return nullptr;
        }
        const nkentseu::usize sz = (nkentseu::usize)w * h * ch * sizeof(float);
        ::memcpy(img->Pixels(), stbiPixels, sz);
        stbi_image_free(stbiPixels);
        return img;
    }

    // Pixel layout stb_image_resize2 à partir du nombre de canaux
    stbir_pixel_layout LayoutFromChannels(int ch) noexcept {
        switch (ch) {
            case 1:  return STBIR_1CHANNEL;
            case 2:  return STBIR_2CHANNEL;
            case 3:  return STBIR_RGB;
            case 4:  return STBIR_RGBA;
            default: return STBIR_RGBA;
        }
    }

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  NkSTBCodec — Chargement
// ─────────────────────────────────────────────────────────────────────────────

namespace nkentseu {

NkImage* NkSTBCodec::Load(const char* path, int32 desiredChannels) noexcept {
    if (!path) return nullptr;
    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = stbi_load(path, &w, &h, &ch,
                                desiredChannels > 0 ? desiredChannels : 0);
    if (!pixels) return nullptr;
    const int outCh = desiredChannels > 0 ? desiredChannels : ch;
    return WrapSTBI(pixels, w, h, outCh);
}

NkImage* NkSTBCodec::LoadFromMemory(
    const uint8* data, usize size, int32 desiredChannels
) noexcept {
    if (!data || size == 0) return nullptr;
    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        data, static_cast<int>(size),
        &w, &h, &ch,
        desiredChannels > 0 ? desiredChannels : 0
    );
    if (!pixels) return nullptr;
    const int outCh = desiredChannels > 0 ? desiredChannels : ch;
    return WrapSTBI(pixels, w, h, outCh);
}

NkImage* NkSTBCodec::LoadHDR(const char* path) noexcept {
    if (!path) return nullptr;
    int w = 0, h = 0, ch = 0;
    float* pixels = stbi_loadf(path, &w, &h, &ch, 0);
    return WrapSTBIFloat(pixels, w, h, ch);
}

NkImage* NkSTBCodec::LoadHDRFromMemory(const uint8* data, usize size) noexcept {
    if (!data || size == 0) return nullptr;
    int w = 0, h = 0, ch = 0;
    float* pixels = stbi_loadf_from_memory(
        data, static_cast<int>(size), &w, &h, &ch, 0
    );
    return WrapSTBIFloat(pixels, w, h, ch);
}

bool NkSTBCodec::QueryInfo(
    const char* path, int32& outW, int32& outH, int32& outCh
) noexcept {
    if (!path) return false;
    return stbi_info(path, &outW, &outH, &outCh) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSTBCodec — Sauvegarde
// ─────────────────────────────────────────────────────────────────────────────

bool NkSTBCodec::Save(const NkImage& img, const char* path, int32 quality) noexcept {
    if (!img.IsValid() || !path) return false;
    const char* ext = GetExt(path);
    if      (ExtEq(ext, "png"))                    return SavePNG(img, path);
    else if (ExtEq(ext, "jpg") || ExtEq(ext, "jpeg")) return SaveJPEG(img, path, quality);
    else if (ExtEq(ext, "bmp"))                    return SaveBMP(img, path);
    else if (ExtEq(ext, "tga"))                    return SaveTGA(img, path);
    else if (ExtEq(ext, "hdr"))                    return SaveHDR(img, path);
    return false;
}

bool NkSTBCodec::SavePNG(const NkImage& img, const char* path) noexcept {
    if (!img.IsValid() || !path) return false;
    return stbi_write_png(
        path,
        img.Width(), img.Height(), img.Channels(),
        img.Pixels(), img.Stride()
    ) != 0;
}

bool NkSTBCodec::SaveJPEG(const NkImage& img, const char* path, int32 quality) noexcept {
    if (!img.IsValid() || !path) return false;
    // stbi_write_jpg ne supporte pas Gray+Alpha ni les formats float
    if (img.Format() == NkImagePixelFormat::NK_GRAY_A16   ||
        img.Format() == NkImagePixelFormat::NK_RGBA128F    ||
        img.Format() == NkImagePixelFormat::NK_RGB96F) {
        return false;
    }
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    return stbi_write_jpg(
        path,
        img.Width(), img.Height(), img.Channels(),
        img.Pixels(), quality
    ) != 0;
}

bool NkSTBCodec::SaveBMP(const NkImage& img, const char* path) noexcept {
    if (!img.IsValid() || !path) return false;
    return stbi_write_bmp(
        path,
        img.Width(), img.Height(), img.Channels(),
        img.Pixels()
    ) != 0;
}

bool NkSTBCodec::SaveTGA(const NkImage& img, const char* path) noexcept {
    if (!img.IsValid() || !path) return false;
    return stbi_write_tga(
        path,
        img.Width(), img.Height(), img.Channels(),
        img.Pixels()
    ) != 0;
}

bool NkSTBCodec::SaveHDR(const NkImage& img, const char* path) noexcept {
    if (!img.IsValid() || !path) return false;
    if (!img.IsHDR()) return false;
    return stbi_write_hdr(
        path,
        img.Width(), img.Height(), img.Channels(),
        reinterpret_cast<const float*>(img.Pixels())
    ) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkSTBCodec — Redimensionnement
// ─────────────────────────────────────────────────────────────────────────────

NkImage* NkSTBCodec::Resize(
    const NkImage& src, int32 newW, int32 newH
) noexcept {
    if (!src.IsValid() || newW <= 0 || newH <= 0) return nullptr;
    if (src.IsHDR()) return nullptr; // non supporté pour l'instant

    const int ch = src.Channels();
    NkImage* dst = NkImage::Alloc(newW, newH, src.Format());
    if (!dst) return nullptr;

    stbir_pixel_layout layout = LayoutFromChannels(ch);
    unsigned char* result = stbir_resize_uint8_linear(
        src.Pixels(),  src.Width(),  src.Height(),  src.Stride(),
        dst->Pixels(), newW,         newH,           dst->Stride(),
        layout
    );

    if (!result) {
        dst->Free();
        return nullptr;
    }
    return dst;
}

} // namespace nkentseu
