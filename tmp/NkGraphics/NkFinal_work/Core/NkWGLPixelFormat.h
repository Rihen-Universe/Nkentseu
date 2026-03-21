#pragma once
// NkWGLPixelFormat.h — PIXELFORMATDESCRIPTOR bootstrap configurable (Windows)
#include "NkTypes.h"

namespace nkentseu {

#if defined(NKENTSEU_PLATFORM_WINDOWS)

    enum class NkPFDFlags : uint32 {
        DrawToWindow  = 0x00000004,
        DrawToBitmap  = 0x00000008,
        SupportOpenGL = 0x00000020,
        DoubleBuffer  = 0x00000001,
        Stereo        = 0x00000002,
        SwapExchange  = 0x00000200,
        SwapCopy      = 0x00000400,
        Default       = 0x00000004 | 0x00000020 | 0x00000001,
        DefaultSingle = 0x00000004 | 0x00000020,
    };
    inline NkPFDFlags operator|(NkPFDFlags a, NkPFDFlags b) {
        return static_cast<NkPFDFlags>(uint32(a) | uint32(b));
    }

    enum class NkPFDPixelType : uint8 { RGBA = 0, ColorIndex = 1 };

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
    struct NkWGLFallbackPixelFormat {};
#endif

} // namespace nkentseu