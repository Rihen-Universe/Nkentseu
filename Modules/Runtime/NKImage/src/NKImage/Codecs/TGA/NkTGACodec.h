#pragma once
#include "NKImage/Core/NkImage.h"

namespace nkentseu {
    class NKENTSEU_IMAGE_API NkTGACodec {
        public:
            static NkImage* Decode(const uint8* data, usize size) noexcept;
            static bool     Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
    };
} // namespace nkentseu
