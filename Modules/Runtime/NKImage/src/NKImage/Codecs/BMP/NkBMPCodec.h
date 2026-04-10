#pragma once
/**
 * @File    NkBMPCodec.h
 * @Brief   Codec BMP — DIB complet (lecture + écriture).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkBMPCodec {
        public:
            /**
             * @Brief Décode un buffer BMP en mémoire.
             * Headers supportés : BITMAPCOREHEADER (12), BITMAPINFOHEADER (40),
             * BITMAPV4HEADER (108), BITMAPV5HEADER (124).
             * Bit depths : 1, 2, 4, 8 (indexé), 16, 24, 32 bpp.
             * Compression : BI_RGB, BI_RLE4, BI_RLE8, BI_BITFIELDS.
             */
            static NkImage* Decode(const uint8* data, usize size) noexcept;

            /**
             * @Brief Encode une NkImage en BMP vers un buffer mémoire.
             * Format de sortie : 24 bpp (RGB) ou 32 bpp (RGBA) avec BITMAPV4HEADER.
             */
            static bool Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
    };

} // namespace nkentseu
