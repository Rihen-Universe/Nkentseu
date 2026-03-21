#pragma once
// NkOpenGLDesc.h — Descripteur complet contexte OpenGL
#include "NkTypes.h"
#include "NkWGLPixelFormat.h"

namespace nkentseu {

    enum class NkGLProfile : uint32 { Core=0, Compatibility=1, ES=2 };

    enum class NkGLContextFlags : uint32 {
        None=0, Debug=1<<0, ForwardCompat=1<<1, RobustAccess=1<<2, NoError=1<<3
    };
    inline NkGLContextFlags operator|(NkGLContextFlags a, NkGLContextFlags b) {
        return static_cast<NkGLContextFlags>(uint32(a)|uint32(b));
    }
    inline bool HasFlag(NkGLContextFlags s, NkGLContextFlags f) {
        return (uint32(s)&uint32(f))!=0;
    }

    enum class NkGLSwapInterval : int32 { Immediate=0, VSync=1, AdaptiveVSync=-1 };

    struct NkGLXHints {
        bool  stereoRendering   = false;
        bool  floatingPointFB   = false;
        bool  transparentWindow = false;
        bool  allowPixmap       = false;
        bool  allowPbuffer      = false;
        int32 redBits=8, greenBits=8, blueBits=8, alphaBits=8;
    };

    struct NkEGLHints {
        int32 redBits=8, greenBits=8, blueBits=8, alphaBits=8;
        bool  bindToTextureRGBA = false;
        bool  pbufferSurface    = false;
        bool  openGLConformant  = false;
    };

    struct NkOpenGLDesc {
        int32            majorVersion = 4;
        int32            minorVersion = 6;
        NkGLProfile      profile      = NkGLProfile::Core;
        NkGLContextFlags contextFlags = NkGLContextFlags::ForwardCompat;

        int32  colorBits      = 32;
        int32  redBits        = 8;
        int32  greenBits      = 8;
        int32  blueBits       = 8;
        int32  alphaBits      = 8;
        int32  depthBits      = 24;
        int32  stencilBits    = 8;
        int32  msaaSamples    = 0;
        bool   srgbFramebuffer= true;
        bool   doubleBuffer   = true;
        bool   accumBuffer    = false;

        NkGLSwapInterval swapInterval = NkGLSwapInterval::VSync;

        struct Glad2Options {
            bool   autoLoad             = true;
            bool   validateVersion      = true;
            bool   installDebugCallback = true;
            uint32 debugSeverityLevel   = 2;
        } glad2;

        NkWGLFallbackPixelFormat wglFallback = NkWGLFallbackPixelFormat::Standard();
        NkGLXHints               glxHints;
        NkEGLHints               eglHints;

        static NkOpenGLDesc Desktop46(bool dbg=false) {
            NkOpenGLDesc d; d.majorVersion=4; d.minorVersion=6;
            d.profile=NkGLProfile::Core;
            d.contextFlags=NkGLContextFlags::ForwardCompat|(dbg?NkGLContextFlags::Debug:NkGLContextFlags::None);
            d.glad2.installDebugCallback=dbg; return d;
        }
        static NkOpenGLDesc Desktop33(bool dbg=false) {
            NkOpenGLDesc d; d.majorVersion=3; d.minorVersion=3;
            d.profile=NkGLProfile::Core;
            d.contextFlags=NkGLContextFlags::ForwardCompat|(dbg?NkGLContextFlags::Debug:NkGLContextFlags::None);
            return d;
        }
        static NkOpenGLDesc ES32() {
            NkOpenGLDesc d; d.majorVersion=3; d.minorVersion=2;
            d.profile=NkGLProfile::ES; d.contextFlags=NkGLContextFlags::None;
            d.glad2.installDebugCallback=false; return d;
        }
    };

} // namespace nkentseu