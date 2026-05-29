#pragma once
// NkWGLPixelFormat.h — PIXELFORMATDESCRIPTOR bootstrap configurable (Windows)
#include "NKCore/NkTypes.h"

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS)

    enum class NkPFDFlags : uint32 {
        NK_PFD_DRAW_TO_WINDOW  = 0x00000004,
        NK_PFD_DRAW_TO_BITMAP  = 0x00000008,
        NK_PFD_SUPPORT_OPENGL  = 0x00000020,
        NK_PFD_DOUBLE_BUFFER   = 0x00000001,
        NK_PFD_STEREO          = 0x00000002,
        NK_PFD_SWAP_EXCHANGE   = 0x00000200,
        NK_PFD_SWAP_COPY       = 0x00000400,
        NK_PFD_DEFAULT         = 0x00000004 | 0x00000020 | 0x00000001,
        NK_PFD_DEFAULT_SINGLE  = 0x00000004 | 0x00000020,

        // Backward-compatible aliases
        DrawToWindow  = NK_PFD_DRAW_TO_WINDOW,
        DrawToBitmap  = NK_PFD_DRAW_TO_BITMAP,
        SupportOpenGL = NK_PFD_SUPPORT_OPENGL,
        DoubleBuffer  = NK_PFD_DOUBLE_BUFFER,
        Stereo        = NK_PFD_STEREO,
        SwapExchange  = NK_PFD_SWAP_EXCHANGE,
        SwapCopy      = NK_PFD_SWAP_COPY,
        Default       = NK_PFD_DEFAULT,
        DefaultSingle = NK_PFD_DEFAULT_SINGLE,
    };
    
    inline NkPFDFlags operator|(NkPFDFlags a, NkPFDFlags b) {
        return static_cast<NkPFDFlags>(uint32(a) | uint32(b));
    }

    enum class NkPFDPixelType : uint8 {
        NK_PFD_PIXEL_RGBA = 0,
        NK_PFD_PIXEL_COLOR_INDEX = 1,

        // Backward-compatible aliases
        RGBA = NK_PFD_PIXEL_RGBA,
        ColorIndex = NK_PFD_PIXEL_COLOR_INDEX
    };

    struct NkWGLFallbackPixelFormat {
        uint8          colorBits   = 32;
        uint8          alphaBits   = 8;
        uint8          depthBits   = 24;
        uint8          stencilBits = 8;
        uint8          accumBits   = 0;
        uint8          auxBuffers  = 0;
        uint16         version     = 1;
        NkPFDPixelType pixelType   = NkPFDPixelType::RGBA;
        NkPFDFlags     flags       = NkPFDFlags::Default;

        static NkWGLFallbackPixelFormat Minimal() {
            NkWGLFallbackPixelFormat f;
            f.colorBits=24; f.alphaBits=0; f.depthBits=16; f.stencilBits=0;
            return f;
        }
        static NkWGLFallbackPixelFormat Standard()      { return {}; }
        static NkWGLFallbackPixelFormat HighPrecision() {
            NkWGLFallbackPixelFormat f; f.colorBits=32; f.depthBits=32; return f;
        }
        static NkWGLFallbackPixelFormat SingleBuffer() {
            NkWGLFallbackPixelFormat f; f.flags=NkPFDFlags::DefaultSingle; return f;
        }
    };

#else
    struct NkWGLFallbackPixelFormat {
        static NkWGLFallbackPixelFormat Minimal()       { return {}; }
        static NkWGLFallbackPixelFormat Standard()      { return {}; }
        static NkWGLFallbackPixelFormat HighPrecision() { return {}; }
        static NkWGLFallbackPixelFormat SingleBuffer()  { return {}; }
    };
#endif

} // namespace nkentseu
