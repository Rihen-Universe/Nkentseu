// =============================================================================
// GLContext.cpp
// -----------------------------------------------------------------------------
// Implementation : utilise NkContextFactory pour creer un NkOpenGLContext
// adapte a la plateforme, puis charge glad pour acceder a OpenGL/GLES.
// Copie litterale du pattern Pong/Render/GLContext.cpp (namespace pong -> cameradem).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

// ── GLAD2 : inclure AVANT tout autre header NK* pour eviter le conflit avec
// les gl.h systeme deja charges par defaut. ─────────────────────────────────
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define NKCAMDEM_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(NKCAMDEM_HAS_GLAD)
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
#   undef Bool   // X11/GLX redefinissent Bool en macro
#endif

#include "GLContext.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKCanvas/Core/NkIGraphicsContext.h"
#include "NKCanvas/Core/NkContextDesc.h"
#include "NKCanvas/Core/NkOpenGLDesc.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
#include "NKCanvas/Factory/NkContextFactory.h"
#include "NKCanvas/Backend/OpenGL/NkOpenGLContext.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten/html5.h>
    static void* GetWebGLProcAddrFallback(const char* name)
    {
        return reinterpret_cast<void*>(emscripten_webgl_get_proc_address(name));
    }
#endif

namespace nkentseu
{
    namespace cameradem
    {

        // ─────────────────────────────────────────────────────────────────────
        // Charge glad a partir de la fonction proc-address exposee par
        // NkContext. On selectionne le bon glad selon la plateforme.
        // ─────────────────────────────────────────────────────────────────────
        static bool LoadGladFromContext(NkIGraphicsContext* ctx)
        {
#if defined(NKCAMDEM_HAS_GLAD)
            auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
#   if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            if (loader == nullptr) loader = &GetWebGLProcAddrFallback;
#   endif
            if (loader == nullptr)
            {
                logger.Error("[GLContext] OpenGL proc loader unavailable");
                return false;
            }
#   if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            int ver = gladLoadGLES2(reinterpret_cast<GLADloadfunc>(loader));
#   else
            int ver = gladLoadGL(reinterpret_cast<GLADloadfunc>(loader));
#   endif
            if (ver == 0)
            {
                logger.Error("[GLContext] glad load failed");
                return false;
            }
            return true;
#else
            (void)ctx;
            return true;
#endif
        }

        GLContext::~GLContext()
        {
            Shutdown();
        }

        // ─────────────────────────────────────────────────────────────────────
        bool GLContext::Init(NkWindow& window)
        {
            NkContextDesc desc;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
            desc.api                 = NkGraphicsApi::NK_GFX_API_WEBGL;
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

            desc.opengl.msaaSamples                  = 0;   // pas de MSAA pour live texture
            desc.opengl.srgbFramebuffer              = false;
            desc.opengl.swapInterval                 = NkGLSwapInterval::AdaptiveVSync;
            desc.opengl.runtime.autoLoadEntryPoints  = false;  // glad manuel
            desc.opengl.runtime.validateVersion      = true;
            desc.opengl.runtime.installDebugCallback = false;

            mContext = NkContextFactory::Create(window, desc);
            if (mContext == nullptr)
            {
                logger.Error("[GLContext] NkContextFactory::Create failed");
                return false;
            }
            if (!LoadGladFromContext(mContext))
            {
                mContext->Shutdown();
                delete mContext;
                mContext = nullptr;
                return false;
            }
            mGladLoaded = true;
            logger.Info("[GLContext] OpenGL context ready");
            return true;
        }

        // ─────────────────────────────────────────────────────────────────────
        void GLContext::Shutdown()
        {
            if (mContext != nullptr)
            {
                mContext->Shutdown();
                delete mContext;
                mContext = nullptr;
            }
            mGladLoaded = false;
        }

        // ─────────────────────────────────────────────────────────────────────
        bool GLContext::BeginFrame()
        {
            return mContext != nullptr ? mContext->BeginFrame() : false;
        }

        void GLContext::EndFrame()
        {
            if (mContext != nullptr) mContext->EndFrame();
        }

        void GLContext::Present()
        {
            if (mContext != nullptr) mContext->Present();
        }

        bool GLContext::OnResize(uint32 w, uint32 h)
        {
            return mContext != nullptr ? mContext->OnResize(w, h) : false;
        }

        bool GLContext::RecreateSurface(NkWindow& window)
        {
            if (mContext == nullptr) return false;
            auto* gl = static_cast<NkOpenGLContext*>(mContext);
            return gl->RecreateSurface(window);
        }

    } // namespace cameradem
} // namespace nkentseu
