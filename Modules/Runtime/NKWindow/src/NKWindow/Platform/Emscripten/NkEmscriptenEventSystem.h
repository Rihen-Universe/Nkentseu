#pragma once
// =============================================================================
// NkEmscriptenEventSystem.h â€” WASM/Emscripten platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)

namespace nkentseu {

struct NkEventSystemData {
    // Emscripten callbacks are registered globally in Init()
};

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_EMSCRIPTEN || __EMSCRIPTEN__
