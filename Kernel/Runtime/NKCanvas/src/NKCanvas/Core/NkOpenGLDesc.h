#pragma once
// NkOpenGLDesc.h — Descripteur complet contexte OpenGL
#include "NKCore/NkTypes.h"
#include "NkWGLPixelFormat.h"

namespace nkentseu {

    enum class NkGLProfile : uint32 {
        NK_GL_PROFILE_CORE = 0,
        NK_GL_PROFILE_COMPATIBILITY = 1,
        NK_GL_PROFILE_ES = 2,

        // Backward-compatible aliases
        Core = NK_GL_PROFILE_CORE,
        Compatibility = NK_GL_PROFILE_COMPATIBILITY,
        ES = NK_GL_PROFILE_ES
    };

    enum class NkGLContextFlags : uint32 {
        NK_GL_CONTEXT_FLAG_NONE = 0,
        NK_GL_CONTEXT_FLAG_DEBUG = 1 << 0,
        NK_GL_CONTEXT_FLAG_FORWARD_COMPAT = 1 << 1,
        NK_GL_CONTEXT_FLAG_ROBUST_ACCESS = 1 << 2,
        NK_GL_CONTEXT_FLAG_NO_ERROR = 1 << 3,

        // Backward-compatible aliases
        NoneFlag = NK_GL_CONTEXT_FLAG_NONE,
        Debug = NK_GL_CONTEXT_FLAG_DEBUG,
        ForwardCompat = NK_GL_CONTEXT_FLAG_FORWARD_COMPAT,
        RobustAccess = NK_GL_CONTEXT_FLAG_ROBUST_ACCESS,
        NoError = NK_GL_CONTEXT_FLAG_NO_ERROR
    };
    inline NkGLContextFlags operator|(NkGLContextFlags a, NkGLContextFlags b) {
        return static_cast<NkGLContextFlags>(uint32(a)|uint32(b));
    }
    inline bool HasFlag(NkGLContextFlags s, NkGLContextFlags f) {
        return (uint32(s)&uint32(f))!=0;
    }

    enum class NkGLSwapInterval : int32 {
        NK_GL_SWAP_IMMEDIATE = 0,
        NK_GL_SWAP_VSYNC = 1,
        NK_GL_SWAP_ADAPTIVE_VSYNC = -1,

        // Backward-compatible aliases
        Immediate = NK_GL_SWAP_IMMEDIATE,
        VSync = NK_GL_SWAP_VSYNC,
        AdaptiveVSync = NK_GL_SWAP_ADAPTIVE_VSYNC
    };

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

    struct NkOpenGLRuntimeOptions {
        // If true, the context may auto-load OpenGL entry points via an internal loader.
        // When NK_NO_GLAD2 is defined, this option is ignored and external loading is expected.
        bool   autoLoadEntryPoints = false;
        bool   validateVersion     = true;
        bool   installDebugCallback= true;
        uint32 debugSeverityLevel  = 2;
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

        NkOpenGLRuntimeOptions runtime;

        NkWGLFallbackPixelFormat wglFallback = NkWGLFallbackPixelFormat::Standard();
        NkGLXHints               glxHints;
        NkEGLHints               eglHints;

        static NkOpenGLDesc Desktop46(bool dbg=false) {
            NkOpenGLDesc d; d.majorVersion=4; d.minorVersion=6;
            d.profile=NkGLProfile::Core;
            d.contextFlags=NkGLContextFlags::ForwardCompat|(dbg?NkGLContextFlags::Debug:NkGLContextFlags::NoneFlag);
            d.runtime.installDebugCallback=dbg; return d;
        }
        static NkOpenGLDesc Desktop33(bool dbg=false) {
            NkOpenGLDesc d; d.majorVersion=3; d.minorVersion=3;
            d.profile=NkGLProfile::Core;
            d.contextFlags=NkGLContextFlags::ForwardCompat|(dbg?NkGLContextFlags::Debug:NkGLContextFlags::NoneFlag);
            return d;
        }
        static NkOpenGLDesc ES32() {
            NkOpenGLDesc d; d.majorVersion=3; d.minorVersion=2;
            d.profile=NkGLProfile::ES; d.contextFlags=NkGLContextFlags::NoneFlag;
            d.runtime.installDebugCallback=false; return d;
        }
    };

} // namespace nkentseu
