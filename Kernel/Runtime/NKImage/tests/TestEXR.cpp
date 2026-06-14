/**
 * @File   TestEXR.cpp
 * @Brief  Test EXR codec : charge un fichier EXR reel et imprime stats.
 *         (Code de validation, gardé pour reference future.)
 */
#include "NKImage/NKImage.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace nkentseu;

int main(int argc, char** argv) {
    const char* path = (argc > 1)
        ? argv[1]
        : "Resources/Textures/HDR/piazza_bologni_1k.exr";

    std::printf("[TestEXR] Chargement %s ...\n", path);
    NkImage img;
    if (!img.Load(path)) {
        std::printf("[TestEXR] FAIL : NkImage::Load retourne nullptr\n");
        return 1;
    }
    const int32 w = img.Width(), h = img.Height();
    const int32 ch = img.Channels();
    std::printf("[TestEXR] OK : %dx%d, %d canaux, format=%d, HDR=%d\n", w, h, ch, int(img.Format()), img.IsHDR() ? 1 : 0);

    // Stats : min/max/avg sur chaque canal
    if (img.IsHDR()) {
        float mn[4] = { 1e30f, 1e30f, 1e30f, 1e30f };
        float mx[4] = { -1e30f, -1e30f, -1e30f, -1e30f };
        double sum[4] = { 0, 0, 0, 0 };
        usize nPix = usize(w) * usize(h);
        const float* p = reinterpret_cast<const float*>(img.Pixels());
        for (usize i = 0; i < nPix; ++i) {
            for (int32 c = 0; c < ch; ++c) {
                float v = p[i * ch + c];
                if (v < mn[c]) mn[c] = v;
                if (v > mx[c]) mx[c] = v;
                sum[c] += double(v);
            }
        }
        for (int32 c = 0; c < ch; ++c) {
            std::printf("[TestEXR] canal %d : min=%.4f max=%.4f avg=%.4f\n",
                        c, mn[c], mx[c], float(sum[c] / double(nPix)));
        }
    }

    // Convertit en RGBA8 tone-mappe et sauve en PNG pour verification visuelle
    NkImage* rgba = NkHDRCodec::ConvertToTexture(img, 1.0f, 2.2f);
    if (rgba) {
        const char* outPath = "Build/test_exr_output.png";
        bool ok = rgba->SavePNG(outPath);
        std::printf("[TestEXR] Sauve PNG %s : %s\n", outPath, ok ? "OK" : "FAIL");
        rgba->Free();   // rgba vient de ConvertToTexture() (heap) → Free() OK
    }

    // `img` est sur la PILE : pas de Free() (nkFree(this) sur la pile = corruption).
    // Le destructeur ~NkImage() libère les pixels à la sortie de scope.
    return 0;
}
