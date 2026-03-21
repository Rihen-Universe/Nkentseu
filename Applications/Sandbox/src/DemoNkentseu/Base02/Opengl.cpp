#include <functional>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#if defined(__ANDROID__)
#   include <sys/system_properties.h>
#endif
#if defined(__EMSCRIPTEN__)
#   include <emscripten/emscripten.h>
#   include <emscripten/html5.h>
#   include <emscripten.h>
#endif

// Nécessaire ici pour disposer des macros plateforme/windowing
// (NKENTSEU_PLATFORM_*, NKENTSEU_WINDOWING_*) avant le bloc GLAD.
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"

// GLAD — inclure avant les headers NK* pour éviter un conflit
// "OpenGL header already included" quand un gl.h système est déjà chargé.
#if defined(__has_include)
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       if __has_include(<glad/wgl.h>) && __has_include(<glad/gl.h>)
#           define NK_DEMO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       if __has_include(<glad/gl.h>)
#           define NK_DEMO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       if __has_include(<glad/gles2.h>)
#           define NK_DEMO_HAS_GLAD 1
#       endif
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       if __has_include(<glad/gles2.h>)
#           define NK_DEMO_HAS_GLAD 1
#       endif
#   endif
#endif

#if defined(NK_DEMO_HAS_GLAD)
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
#else
#   if defined(NKENTSEU_PLATFORM_WINDOWS)
#       include <GL/gl.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       ifndef GL_GLEXT_PROTOTYPES
#           define GL_GLEXT_PROTOTYPES 1
#       endif
#       include <GL/gl.h>
#       include <GL/glext.h>
#       include <GL/glx.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#       include <GLES3/gl3.h>
#   elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <emscripten/html5.h>
#       include <GLES3/gl3.h>
#   endif
#endif

// X11 headers pulled by GLX expose `Bool` as macro; keep NK types stable.
#if defined(Bool)
#   undef Bool
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <d3d12.h>
#   include <d3dcompiler.h>
#endif

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKTime/NkChrono.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// #include <random>

namespace nkentseu {
    struct NkEntryState;
}

using namespace nkentseu;

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
static void* GetWebGLProcAddressFallback(const char* name) {
    return reinterpret_cast<void*>(emscripten_webgl_get_proc_address(name));
}
#endif

static bool LoadDemoOpenGLFunctions(NkIGraphicsContext* ctx) {
#if defined(NK_DEMO_HAS_GLAD)
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    if (!loader) {
        loader = &GetWebGLProcAddressFallback;
        printf("[DemoGL] Context loader unavailable; fallback to emscripten_webgl_get_proc_address\n");
    }
#endif
    if (!loader) {
        printf("[DemoGL] OpenGL proc loader unavailable from context\n");
        return false;
    }
#if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    int ver = gladLoadGLES2((GLADloadfunc)loader);
#else
    int ver = gladLoadGL((GLADloadfunc)loader);
#endif
    if (!ver) {
        printf("[DemoGL] GLAD load failed from context loader\n");
        return false;
    }
#endif
    return true;
}

// =============================================================================
// nkmain — point d'entrée principal avec sélection de la démo
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {
    NkWindowConfig cfg;

    srand((uint32)::time(NULL));

    cfg.title = "NkOpenGL Demo";
    cfg.width = 1200;
    cfg.height = 800;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = false;

    // ── Configurer le contexte OpenGL ────────────────────────────────────────
    NkContextDesc desc;
    
    #if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        desc.api = NkGraphicsApi::NK_API_WEBGL;
        desc.opengl.majorVersion    = 3; // WebGL2 ~ GLES 3.0
        desc.opengl.minorVersion    = 0;
        desc.opengl.profile         = NkGLProfile::ES;
        desc.opengl.contextFlags    = NkGLContextFlags::NoneFlag;
    #elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
        desc = NkContextDesc::MakeOpenGLES(3, 0);
    #else
        desc.api    = NkGraphicsApi::NK_API_OPENGL;
        desc.opengl = NkOpenGLDesc::Desktop46(/*debug=*/true);
    #endif

    desc.opengl.msaaSamples      = 4;
    desc.opengl.srgbFramebuffer  = true;
    desc.opengl.swapInterval     = NkGLSwapInterval::AdaptiveVSync;

    // Personnaliser le PIXELFORMATDESCRIPTOR bootstrap (optionnel)
    // desc.opengl.wglFallback = NkWGLFallbackPixelFormat::Standard(); // déjà par défaut
    // desc.opengl.wglFallback = NkWGLFallbackPixelFormat::Minimal();  // driver ancien

    // GLAD2 : installer le debug callback automatiquement
    desc.opengl.runtime.autoLoadEntryPoints = false;
    desc.opengl.runtime.validateVersion     = true;
    #if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        // WebGL2 n'expose pas GL_KHR_debug comme un desktop GL debug context.
        desc.opengl.runtime.installDebugCallback = false;
    #else
        desc.opengl.runtime.installDebugCallback = true;
    #endif
    desc.opengl.runtime.debugSeverityLevel  = 2; // medium+

    NkWindow window(cfg);
    if (!window.IsOpen()) { printf("[DemoGL] Window failed\n"); return -1; }

    auto ctx = NkContextFactory::Create(window, desc);
    if (!ctx) { printf("[DemoGL] Context failed\n"); return -2; }
    if (!LoadDemoOpenGLFunctions(ctx)) {
        ctx->Shutdown();
        window.Close();
        return -3;
    }

    NkChrono chrono;
    NkElapsedTime elapsed;
    auto& events = NkEvents();
    uint32 frameCount = 0;
    const uint32 maxFrames = 36000; // Safety guard if no OS event pump is attached.
    bool shouldQuit = false;

    while (window.IsOpen() && frameCount < maxFrames) {
        elapsed = chrono.Reset();
        
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                shouldQuit = true;
                break;
            }
        }
        if (shouldQuit || !window.IsOpen()) break;
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;

        float r = ((rand()) % 255) / 255.0f;
        float g = ((rand()) % 255) / 255.0f;
        float b = ((rand()) % 255) / 255.0f;
        float a = 1.0f;

        if (ctx->BeginFrame()) {
            const auto surface = window.GetSurfaceDesc();
            glViewport(0, 0, surface.width, surface.height);
            glClearColor(r, g, b, a);
            glClear(GL_COLOR_BUFFER_BIT);
            ctx->EndFrame();
            ctx->Present();
        }
        ++frameCount;

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        emscripten_sleep(0);
        continue;
#endif

        // Cap 60 fps
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    if (frameCount >= maxFrames && window.IsOpen()) {
        window.Close();
    }

    ctx->Shutdown();
    window.Close();
    return 0;
}
