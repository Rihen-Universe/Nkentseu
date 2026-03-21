#pragma once
// NkSurfaceDesc.h — Handles natifs de surface + hints inter-composants
#include "NkTypes.h"

namespace nkentseu {

    enum class NkSurfaceHintKey : uint32 {
        NK_GLX_VISUAL_ID     = 0,
        NK_GLX_FB_CONFIG_PTR = 1,
        NK_METAL_LAYER_PTR   = 2,
        NK_CUSTOM_1          = 16,
        NK_CUSTOM_2          = 17,
    };

    struct NkSurfaceHints {
        static constexpr uint32 kMax = 8;
        uint32  keys  [kMax] = {};
        uintptr values[kMax] = {};
        uint32  count        = 0;

        void Set(NkSurfaceHintKey k, uintptr v) {
            for (uint32 i = 0; i < count; ++i)
                if (keys[i] == uint32(k)) { values[i] = v; return; }
            if (count < kMax) { keys[count] = uint32(k); values[count] = v; ++count; }
        }
        uintptr Get(NkSurfaceHintKey k, uintptr def = 0) const {
            for (uint32 i = 0; i < count; ++i)
                if (keys[i] == uint32(k)) return values[i];
            return def;
        }
    };

    struct NkSurfaceDesc {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        void*         hwnd      = nullptr;
        void*         hinstance = nullptr;
#elif defined(NKENTSEU_WINDOWING_XLIB)
        void*         display   = nullptr;
        unsigned long window    = 0;
        int           screen    = 0;
#elif defined(NKENTSEU_WINDOWING_XCB)
        void*         connection= nullptr;
        unsigned long window    = 0;
        void*         display   = nullptr; // bridge Xlib-xcb pour GLX
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
        void*         display   = nullptr;
        void*         surface   = nullptr;
        void*         shmPixels = nullptr;
        int32         shmFd     = -1;
        uint64        shmSize   = 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
        void*         nativeWindow = nullptr;
#elif defined(NKENTSEU_PLATFORM_MACOS)
        void*         nsView    = nullptr;
        void*         nsWindow  = nullptr;
        void*         metalLayer= nullptr;
#elif defined(NKENTSEU_PLATFORM_IOS)
        void*         uiView    = nullptr;
        void*         metalLayer= nullptr;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        const char*   canvasId  = "#canvas";
#endif
        uint32        width     = 0;
        uint32        height    = 0;
        float32       dpiScale  = 1.0f;
        NkSurfaceHints appliedHints;

        bool IsValid() const {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
            return hwnd != nullptr && width > 0 && height > 0;
#elif defined(NKENTSEU_WINDOWING_XLIB)
            return display != nullptr && window != 0 && width > 0;
#elif defined(NKENTSEU_WINDOWING_XCB)
            return connection != nullptr && window != 0 && width > 0;
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
            return display != nullptr && surface != nullptr && width > 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
            return nativeWindow != nullptr && width > 0;
#elif defined(NKENTSEU_PLATFORM_MACOS)
            return nsView != nullptr && width > 0;
#elif defined(NKENTSEU_PLATFORM_IOS)
            return uiView != nullptr && width > 0;
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            return canvasId != nullptr && width > 0;
#else
            return width > 0 && height > 0;
#endif
        }
    };

} // namespace nkentseu