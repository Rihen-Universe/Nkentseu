#pragma once
/**
 * @File    NkSTBCodec.h
 * @Brief   Backend STB pour NKImage — chargement/sauvegarde via stb_image.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Codec alternatif basé sur stb_image / stb_image_write / stb_image_resize2.
 *  Utilisé en remplacement temporaire des codecs natifs NKImage pendant
 *  leur développement. Ne supprime pas et ne remplace pas les codecs existants.
 *
 *  Usage :
 * @code
 *   NkImage* img = NkSTBCodec::Load("photo.png");
 *   if (!img) { ... }
 *   NkSTBCodec::Save(*img, "sortie.jpg", 90);
 *   img->Free();
 * @endcode
 *
 *  Formats supportés (via stb_image) :
 *    Lecture  : PNG, JPEG, BMP, TGA, PSD, GIF, HDR, PIC, PPM/PGM
 *    Écriture : PNG, JPEG, BMP, TGA, HDR
 */

#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkSTBCodec {
    public:

        // ── Chargement ───────────────────────────────────────────────────────

        /**
         * @Brief Charge une image depuis un fichier via stb_image.
         * @param path            Chemin vers le fichier.
         * @param desiredChannels 0 = natif, 1-4 = forcer le nombre de canaux.
         * @return NkImage alloué (Free() requis), nullptr si échec.
         */
        [[nodiscard]] static NkImage* Load(
            const char* path,
            int32       desiredChannels = 0
        ) noexcept;

        /**
         * @Brief Charge une image depuis un buffer en mémoire via stb_image.
         */
        [[nodiscard]] static NkImage* LoadFromMemory(
            const uint8* data,
            usize        size,
            int32        desiredChannels = 0
        ) noexcept;

        /**
         * @Brief Charge une image HDR (float) depuis un fichier.
         *        Résultat : NK_RGBA128F (4 canaux) ou NK_RGB96F (3 canaux).
         */
        [[nodiscard]] static NkImage* LoadHDR(const char* path) noexcept;

        /**
         * @Brief Charge une image HDR depuis un buffer en mémoire.
         */
        [[nodiscard]] static NkImage* LoadHDRFromMemory(
            const uint8* data, usize size
        ) noexcept;

        /**
         * @Brief Interroge les dimensions sans charger les pixels.
         */
        [[nodiscard]] static bool QueryInfo(
            const char* path,
            int32& outWidth, int32& outHeight, int32& outChannels
        ) noexcept;

        // ── Sauvegarde ───────────────────────────────────────────────────────

        /**
         * @Brief Sauvegarde en détectant le format depuis l'extension.
         *        Extensions : .png, .jpg/.jpeg, .bmp, .tga, .hdr
         * @param quality JPEG uniquement [1-100].
         */
        static bool Save(
            const NkImage& img, const char* path, int32 quality = 90
        ) noexcept;

        static bool SavePNG (const NkImage& img, const char* path) noexcept;
        static bool SaveJPEG(const NkImage& img, const char* path, int32 quality = 90) noexcept;
        static bool SaveBMP (const NkImage& img, const char* path) noexcept;
        static bool SaveTGA (const NkImage& img, const char* path) noexcept;
        static bool SaveHDR (const NkImage& img, const char* path) noexcept;

        // ── Redimensionnement ────────────────────────────────────────────────

        /**
         * @Brief Redimensionne via stb_image_resize2.
         *        Supporte Gray8, GrayAlpha16, RGB24, RGBA32.
         * @return Nouvelle NkImage allouée, nullptr si échec.
         */
        [[nodiscard]] static NkImage* Resize(
            const NkImage& src,
            int32 newWidth, int32 newHeight
        ) noexcept;
    };

} // namespace nkentseu
