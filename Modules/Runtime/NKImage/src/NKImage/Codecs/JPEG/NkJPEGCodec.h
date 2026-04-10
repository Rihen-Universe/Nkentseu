#pragma once
/**
 * @File    NkJPEGCodec.h
 * @Brief   Codec JPEG — décodage et encodage JFIF/Exif baseline DCT.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkJPEGCodec {
        public:
            /**
             * @Brief Décode un buffer JPEG en mémoire.
             * @param data  Pointeur vers les données brutes du fichier JPEG.
             * @param size  Taille en octets.
             * @return NkImage alloué (appelant doit appeler Free()), nullptr si échec.
             *         Formats de sortie : NK_GRAY8 (grayscale) ou NK_RGB24 (couleur).
             *         JPEG progressif : rejeté (retourne nullptr).
             */
            static NkImage* Decode(const uint8* data, usize size) noexcept;

            /**
             * @Brief Encode une NkImage en JPEG vers un buffer mémoire.
             * @param img      Image source. Convertie en RGB24 ou Gray8 si nécessaire.
             * @param out      Buffer de sortie alloué avec malloc (appelant libère avec free).
             * @param outSize  Taille du buffer de sortie.
             * @param quality  Qualité JPEG [1-100], défaut 90.
             * @return true si succès.
             */
            static bool Encode(const NkImage& img, uint8*& out, usize& outSize, int32 quality = 90) noexcept;
    };

} // namespace nkentseu