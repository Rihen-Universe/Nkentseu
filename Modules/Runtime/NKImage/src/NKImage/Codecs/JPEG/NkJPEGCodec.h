#pragma once
#include "NKImage/NkImage.h"
namespace nkentseu { class NKENTSEU_IMAGE_API NkJPEGCodec { public:
    static NkImage* Decode(const uint8* d, usize s) noexcept;
    static bool     Encode(const NkImage& img, uint8*& out, usize& outSize, int32 quality=90) noexcept;
}; }
