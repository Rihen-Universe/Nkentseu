#pragma once
/**
 * @File    NkTGACodec.h
 * @Brief   Codec TGA (Truevision TARGA) — lecture + écriture.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * Types supportés en lecture :
 *   Type 1  (color-mapped)       + Type 9  (color-mapped RLE)
 *   Type 2  (true-color)         + Type 10 (true-color RLE)
 *   Type 3  (grayscale)          + Type 11 (grayscale RLE)
 *   15/16 bpp RGB555 → RGB24
 */
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    class NKENTSEU_IMAGE_API NkTGACodec {
        public:
            static NkImage* Decode(const uint8* data, usize size) noexcept;
            static bool     Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
    };

} // namespace nkentseu
