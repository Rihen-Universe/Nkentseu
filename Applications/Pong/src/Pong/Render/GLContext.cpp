// =============================================================================
// GLContext.cpp — implementation
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 : inclure avant les autres headers pour eviter conflit gl.h ────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define PONG_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(PONG_HAS_GLAD)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <glad/wgl.h>
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if defined(__has_include)
#           if __has_include(<glad/glx.h>)
#               include <glad/glx.h>
#           endif
#       endif
#       include <glad/gl.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <glad/gles2.h>
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <glad/gles2.h>
#   endif
#endif

#if defined(Bool)
#   undef Bool
#endif

#include "GLContext.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKContext/Core/NkIGraphicsContext.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten/html5.h>
static void* GetWebGLProcAddrFallback(const char* name) {
    return reinterpret_cast<void*>(emscripten_webgl_get_proc_address(name));
}
#endif

namespace nkentseu { 
    namespace pong {

        GLContext::~GLContext() { Shutdown(); }

        static bool LoadGladFromContext(NkIGraphicsContext* ctx) {
        #if defined(PONG_HAS_GLAD)
            auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
        #   if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            if (!loader) loader = &GetWebGLProcAddrFallback;
        #   endif
            if (!loader) {
                logger.Error("[GLContext] OpenGL proc loader unavailable");
                return false;
            }
        #   if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            int ver = gladLoadGLES2((GLADloadfunc)loader);
        #   else
            int ver = gladLoadGL((GLADloadfunc)loader);
        #   endif
            if (!ver) {
                logger.Error("[GLContext] glad load failed");
                return false;
            }
            return true;
        #else
            (void)ctx;
            return true;
        #endif
        }

        bool GLContext::Init(NkWindow& window) {
            NkContextDesc desc;
        #if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            desc.api = NkGraphicsApi::NK_GFX_API_WEBGL;
            desc.opengl.majorVersion = 3;
            desc.opengl.minorVersion = 0;
            desc.opengl.profile      = NkGLProfile::ES;
            desc.opengl.contextFlags = NkGLContextFlags::NoneFlag;
        #elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
            desc = NkContextDesc::MakeOpenGLES(3, 0);
        #else
            desc.api    = NkGraphicsApi::NK_GFX_API_OPENGL;
            desc.opengl = NkOpenGLDesc::Desktop46(/*debug=*/false);
        #endif

            desc.opengl.msaaSamples      = 4;
            desc.opengl.srgbFramebuffer  = true;
            desc.opengl.swapInterval     = NkGLSwapInterval::AdaptiveVSync;
            desc.opengl.runtime.autoLoadEntryPoints = false; // on charge glad nous-memes
            desc.opengl.runtime.validateVersion     = true;
        #if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            desc.opengl.runtime.installDebugCallback = false;
        #else
            desc.opengl.runtime.installDebugCallback = false; // mettre true pour debug
        #endif

            mContext = NkContextFactory::Create(window, desc);
            if (!mContext) {
                logger.Error("[GLContext] NkContextFactory::Create failed");
                return false;
            }
            if (!LoadGladFromContext(mContext)) {
                mContext->Shutdown();
                delete mContext;
                mContext = nullptr;
                return false;
            }
            mGladLoaded = true;
            logger.Info("[GLContext] OpenGL context ready");
            return true;
        }

        void GLContext::Shutdown() {
            if (mContext) {
                mContext->Shutdown();
                delete mContext;
                mContext = nullptr;
            }
            mGladLoaded = false;
        }

        bool GLContext::BeginFrame() {
            return mContext ? mContext->BeginFrame() : false;
        }
        
        void GLContext::EndFrame() {
            if (mContext) mContext->EndFrame();
        }

        void GLContext::Present() {
            if (mContext) mContext->Present();
        }

        bool GLContext::OnResize(uint32 w, uint32 h) {
            return mContext ? mContext->OnResize(w, h) : false;
        }

    }
} // namespace nkentseu::pong
