#pragma once
// NkTypes.h — Types fondamentaux NkEngine
#include <cstdint>
#include <cstddef>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <array>
#include <cassert>

namespace nkentseu {
    using int8    = std::int8_t;
    using int16   = std::int16_t;
    using int32   = std::int32_t;
    using int64   = std::int64_t;
    using uint8   = std::uint8_t;
    using uint16  = std::uint16_t;
    using uint32  = std::uint32_t;
    using uint64  = std::uint64_t;
    using uintptr = std::uintptr_t;
    using byte    = uint8;
    using float32 = float;
    using float64 = double;
}