#pragma once
#include "NKImage/Core/NkImage.h"
namespace nkentseu { class NKENTSEU_IMAGE_API NkICOCodec { public:
    static NkImage* Decode(const uint8* d, usize s) noexcept;
}; }
