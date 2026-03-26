#pragma once
#include "NKImage/Core/NkImage.h"

namespace nkentseu { 
    class NKENTSEU_IMAGE_API NkBMPCodec { public:
        static NkImage* Decode(const uint8* d, usize s) noexcept;
        static bool     Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
    }; 
}
