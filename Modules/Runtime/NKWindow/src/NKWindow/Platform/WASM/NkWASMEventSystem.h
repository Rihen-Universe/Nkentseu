#pragma once
// =============================================================================
// NkWASMEventSystem.h — WASM/Emscripten platform data for NkEventSystem
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_WEB) || defined(__EMSCRIPTEN__)

namespace nkentseu {

struct NkEventSystemData {
    // Emscripten callbacks are registered globally in Init()
};

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_WEB || __EMSCRIPTEN__
