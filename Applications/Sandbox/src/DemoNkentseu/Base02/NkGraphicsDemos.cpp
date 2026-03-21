// =============================================================================
// NkGraphicsDemos.cpp — Démos NkEngine Graphics
// Version avec gestion complète des événements de focus
// =============================================================================

// Keep platform/windowing macros available before optional GLAD includes.
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"

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
#       include <GL/glext.h>
#   elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#       ifndef GL_GLEXT_PROTOTYPES
#           define GL_GLEXT_PROTOTYPES 1
#       endif
#       include <GL/gl.h>
#       include <GL/glext.h>
#   elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#       include <GLES3/gl3.h>
#   endif
#endif

// X11 headers can expose Bool as macro; keep NK types stable.
#if defined(Bool)
#   undef Bool
#endif

#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Compute/NkIComputeContext.h"
#include "NKLogger/NkLog.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKMath/NkFunctions.h"
#include "NKTime/NkChrono.h"
#include "NKTime/NkDuration.h"
#include "NKContainers/NkContainers.h"

#include <cstddef>
#include <cmath>
#include <cstring>
#include <cctype>
#include <cstdlib>
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include <emscripten/emscripten.h>
#   include <emscripten/html5.h>
#endif
#include "NkGraphicsDemosVkSpv.inl"

using namespace nkentseu;

// ─── Headers DX (Windows) ─────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <d3dcompiler.h>
#   include <wrl/client.h>
    using Microsoft::WRL::ComPtr;
#pragma comment(lib,"d3dcompiler.lib")
#   if defined(MemoryBarrier)
#       undef MemoryBarrier
#   endif
#endif

// ─── Headers Metal (Apple) ────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
#   import <Metal/Metal.h>
#endif

// =============================================================================
// Utilitaires communs
// =============================================================================
namespace {

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
static void* GetWebGLProcAddressFallback(const char* name) {
    return reinterpret_cast<void*>(emscripten_webgl_get_proc_address(name));
}
#endif

// NOUVEAU : Structure de contexte de boucle enrichie
struct LoopCtx { 
    float dt = 0; 
    float time = 0; 
    bool running = true;
    bool hasFocus = true;           // État du focus
    bool isMinimized = false;       // État de minimisation
    bool isResizing = false;        // État de redimensionnement
    bool needsResize = false;       // Redimensionnement en attente
    uint32 pendingW = 0; 
    uint32 pendingH = 0;
    
    // Compteurs pour éviter les recréations trop fréquentes
    uint32 resizeAttempts = 0;
    uint32 focusLostFrames = 0;
};

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   define NK_DEMO_GL_APIENTRY APIENTRY
#else
#   define NK_DEMO_GL_APIENTRY
#endif

struct DemoGLFunctions {
    using CreateShaderFn       = GLuint (NK_DEMO_GL_APIENTRY*)(GLenum);
    using ShaderSourceFn       = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    using CompileShaderFn      = void   (NK_DEMO_GL_APIENTRY*)(GLuint);
    using GetShaderivFn        = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLogFn   = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using CreateProgramFn      = GLuint (NK_DEMO_GL_APIENTRY*)(void);
    using AttachShaderFn       = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLuint);
    using LinkProgramFn        = void   (NK_DEMO_GL_APIENTRY*)(GLuint);
    using GetProgramivFn       = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLogFn  = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
    using BindAttribLocationFn = void   (NK_DEMO_GL_APIENTRY*)(GLuint, GLuint, const GLchar*);
    using DeleteShaderFn       = void   (NK_DEMO_GL_APIENTRY*)(GLuint);
    using DeleteProgramFn      = void   (NK_DEMO_GL_APIENTRY*)(GLuint);
    using UseProgramFn         = void   (NK_DEMO_GL_APIENTRY*)(GLuint);
    using GetUniformLocationFn = GLint  (NK_DEMO_GL_APIENTRY*)(GLuint, const GLchar*);
    using Uniform1fFn          = void   (NK_DEMO_GL_APIENTRY*)(GLint, GLfloat);
    using GenVertexArraysFn    = void   (NK_DEMO_GL_APIENTRY*)(GLsizei, GLuint*);
    using BindVertexArrayFn    = void   (NK_DEMO_GL_APIENTRY*)(GLuint);
    using DeleteVertexArraysFn = void   (NK_DEMO_GL_APIENTRY*)(GLsizei, const GLuint*);
    using GenBuffersFn         = void   (NK_DEMO_GL_APIENTRY*)(GLsizei, GLuint*);
    using BindBufferFn         = void   (NK_DEMO_GL_APIENTRY*)(GLenum, GLuint);
    using BufferDataFn         = void   (NK_DEMO_GL_APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
    using DeleteBuffersFn      = void   (NK_DEMO_GL_APIENTRY*)(GLsizei, const GLuint*);
    using EnableVertexAttribArrayFn = void (NK_DEMO_GL_APIENTRY*)(GLuint);
    using VertexAttribPointerFn = void  (NK_DEMO_GL_APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    using DrawArraysFn         = void   (NK_DEMO_GL_APIENTRY*)(GLenum, GLint, GLsizei);
    using DrawElementsFn       = void   (NK_DEMO_GL_APIENTRY*)(GLenum, GLsizei, GLenum, const void*);

    CreateShaderFn       CreateShader       = nullptr;
    ShaderSourceFn       ShaderSource       = nullptr;
    CompileShaderFn      CompileShader      = nullptr;
    GetShaderivFn        GetShaderiv        = nullptr;
    GetShaderInfoLogFn   GetShaderInfoLog   = nullptr;
    CreateProgramFn      CreateProgram      = nullptr;
    AttachShaderFn       AttachShader       = nullptr;
    LinkProgramFn        LinkProgram        = nullptr;
    GetProgramivFn       GetProgramiv       = nullptr;
    GetProgramInfoLogFn  GetProgramInfoLog  = nullptr;
    BindAttribLocationFn BindAttribLocation = nullptr;
    DeleteShaderFn       DeleteShader       = nullptr;
    DeleteProgramFn      DeleteProgram      = nullptr;
    UseProgramFn         UseProgram         = nullptr;
    GetUniformLocationFn GetUniformLocation = nullptr;
    Uniform1fFn          Uniform1f          = nullptr;
    GenVertexArraysFn    GenVertexArrays    = nullptr;
    BindVertexArrayFn    BindVertexArray    = nullptr;
    DeleteVertexArraysFn DeleteVertexArrays = nullptr;
    GenBuffersFn         GenBuffers         = nullptr;
    BindBufferFn         BindBuffer         = nullptr;
    BufferDataFn         BufferData         = nullptr;
    DeleteBuffersFn      DeleteBuffers      = nullptr;
    EnableVertexAttribArrayFn EnableVertexAttribArray = nullptr;
    VertexAttribPointerFn VertexAttribPointer = nullptr;
    DrawArraysFn         DrawArrays         = nullptr;
    DrawElementsFn       DrawElements       = nullptr;

    bool Ready() const {
        return CreateShader && ShaderSource && CompileShader && GetShaderiv &&
               GetShaderInfoLog && CreateProgram && AttachShader && LinkProgram &&
               GetProgramiv && GetProgramInfoLog &&
               DeleteShader && DeleteProgram && UseProgram && GetUniformLocation &&
               Uniform1f && GenVertexArrays && BindVertexArray && DeleteVertexArrays &&
               GenBuffers && BindBuffer && BufferData && DeleteBuffers &&
               EnableVertexAttribArray && VertexAttribPointer && DrawArrays &&
               DrawElements;
    }
};

DemoGLFunctions gGL{};
bool gGLSupportsVAO = true;

template <typename T>
bool LoadDemoGLProc(T& out, NkOpenGLContextData::ProcAddressFn loader, const char* name) {
    out = reinterpret_cast<T>(loader ? loader(name) : nullptr);
    if (!out) {
        logger.Errorf("[GL] Missing entry point: %s", name);
        return false;
    }
    return true;
}

static bool CreateDemoWindow(NkWindow& win, const char* title, uint32 width, uint32 height) {
    NkWindowConfig cfg{};
    cfg.title = title;
    cfg.width = width;
    cfg.height = height;
    cfg.centered = true;
    cfg.resizable = true;
    return win.Create(cfg);
}

// NOUVELLE VERSION : Boucle principale avec gestion complète des événements
template<typename OnFrameFn>
void RunLoop(NkWindow& win, NkIGraphicsContext* ctx, LoopCtx& lc,
             OnFrameFn&& onFrame,
             int maxFrames = 0)
{
    auto& events = NkSystem::Events();
    NkElapsedTime prev = NkChrono::Now();
    int frames = 0;
    
    while (lc.running && win.IsOpen()) {
        NkEvent* ev = nullptr;
        
        // Traitement de tous les événements en attente
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                lc.running = false;
                break;
            }
            
            // ── Gestion du focus (NOUVEAU) ─────────────────────────────────
            if (auto* focusEv = ev->As<NkWindowFocusLostEvent>()) {
                lc.hasFocus = false;
                lc.focusLostFrames++;
                logger.Debug("[Focus] Window lost focus");
            }
            if (auto* focusEv = ev->As<NkWindowFocusGainedEvent>()) {
                lc.hasFocus = true;
                lc.focusLostFrames = 0;
                logger.Debug("[Focus] Window gained focus");
                
                // Quand on regagne le focus, planifier un resize si nécessaire
                if (!lc.isMinimized) {
                    math::NkVec2u sz = win.GetSize();
                    if (sz.x > 0 && sz.y > 0) {
                        lc.pendingW = sz.x;
                        lc.pendingH = sz.y;
                        lc.needsResize = true;
                    }
                }
            }
            
            // ── Gestion de la minimisation ─────────────────────────────────
            if (ev->Is<NkWindowMinimizeEvent>()) {
                lc.isMinimized = true;
                lc.hasFocus = false;  // La fenêtre perd le focus quand minimisée
                lc.pendingW = lc.pendingH = 0;
                logger.Debug("[Window] Minimized");
            } 
            else if (ev->Is<NkWindowMaximizeEvent>() || ev->Is<NkWindowRestoreEvent>()) {
                lc.isMinimized = false;
                // Sur restauration, la fenêtre va regagner le focus
                // On planifie un resize mais on attend l'événement de focus
                math::NkVec2u sz = win.GetSize();
                if (sz.x > 0 && sz.y > 0) { 
                    lc.pendingW = sz.x; 
                    lc.pendingH = sz.y;
                    lc.needsResize = true;
                }
                logger.Debug("[Window] Restored/Maximized");
            }
            
            // ── Gestion du redimensionnement ──────────────────────────────
            else if (ev->Is<NkWindowResizeBeginEvent>()) {
                lc.isResizing = true;
                lc.pendingW = lc.pendingH = 0;
                lc.resizeAttempts = 0;
                logger.Debug("[Window] Resize begin");
            } 
            else if (ev->Is<NkWindowResizeEndEvent>()) {
                lc.isResizing = false;
                logger.Debug("[Window] Resize end");
            } 
            else if (auto* resize = ev->As<NkWindowResizeEvent>()) {
                if (resize->GetWidth() > 0 && resize->GetHeight() > 0) {
                    lc.pendingW = resize->GetWidth();
                    lc.pendingH = resize->GetHeight();
                    lc.needsResize = true;
                    logger.Debug("[Window] Resize to %ux%u", lc.pendingW, lc.pendingH);
                }
            }
            
            // ── Gestion du déplacement ────────────────────────────────────
            else if (ev->Is<NkWindowMoveBeginEvent>()) {
                // Rien de spécial à faire, mais on log
            } 
            else if (ev->Is<NkWindowMoveEndEvent>()) {
                // Rien de spécial
            } 
            else if (auto* mv = ev->As<NkWindowMoveEvent>()) {
                // Rien de spécial
            }
        }
        
        // ── Logique de rendu conditionnelle ──────────────────────────────
        
        // 1. Si la fenêtre est minimisée → pas de rendu
        if (lc.isMinimized) {
            NkChrono::SleepMilliseconds(16);
            continue;
        }
        
        // 2. Si la fenêtre n'a pas le focus, on peut réduire la cadence de rendu
        if (!lc.hasFocus) {
            // Optionnel : réduire le framerate quand la fenêtre n'est pas au premier plan
            if (lc.focusLostFrames > 0 && (lc.focusLostFrames % 10) == 0) {
                // Toutes les 10 frames sans focus, on fait une micro-pause
                NkChrono::SleepMilliseconds(1);
            }
        }
        
        // 3. Appliquer le redimensionnement quand approprié
        //    Important : ne pas recréer la swapchain pendant le redimensionnement
        //    et seulement si on a des dimensions valides
        if (!lc.isResizing && lc.needsResize && lc.pendingW > 0 && lc.pendingH > 0) {
            // Limiter le nombre de tentatives pour éviter les boucles infinies
            if (lc.resizeAttempts < 5) {
                if (ctx->OnResize(lc.pendingW, lc.pendingH)) {
                    lc.pendingW = lc.pendingH = 0;
                    lc.needsResize = false;
                    lc.resizeAttempts = 0;
                    // Laisser le temps à la swapchain de se stabiliser
                    NkChrono::SleepMilliseconds(16);
                } else {
                    lc.resizeAttempts++;
                    // Attendre un peu avant de réessayer
                    NkChrono::SleepMilliseconds(16 * lc.resizeAttempts);
                }
            } else {
                // Trop d'échecs, on abandonne pour cette fois
                logger.Warn("[Window] Too many resize failures, resetting");
                lc.needsResize = false;
                lc.resizeAttempts = 0;
                lc.pendingW = lc.pendingH = 0;
            }
        }
        
        // Calcul du delta time
        NkElapsedTime now = NkChrono::Now();
        lc.dt = static_cast<float>(now.seconds - prev.seconds);
        prev = now;
        lc.time += lc.dt;
        
        // Tentative de début de frame
        if (!ctx->BeginFrame()) {
            // Si BeginFrame échoue, on planifie un resize si nécessaire
            if (lc.pendingW == 0 || lc.pendingH == 0) {
                math::NkVec2u sz = win.GetSize();
                if (sz.x > 0 && sz.y > 0) {
                    lc.pendingW = sz.x;
                    lc.pendingH = sz.y;
                    lc.needsResize = true;
                }
            }
            NkChrono::SleepMilliseconds(16);
            continue;
        }
        
        // Appel de la fonction de rendu utilisateur
        onFrame(lc.dt, lc.time);
        
        // Fin de frame et présentation
        ctx->EndFrame();
        ctx->Present();
        
        // Compteur de frames pour les démos limitées
        if (maxFrames > 0 && ++frames >= maxFrames) {
            // lc.running = false;
        }

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        emscripten_sleep(0);
        continue;
#endif

        // Cap à 60 FPS
        const float elapsed = static_cast<float>(NkChrono::Now().seconds - now.seconds);
        if (elapsed < 1.f / 60.f) {
            NkChrono::SleepMicroseconds(static_cast<int64>((1.f / 60.f - elapsed) * 1e6f));
        } else {
            NkChrono::YieldThread();
        }
    }
}

// Conversion HSV → RGB (h,s,v ∈ [0,1])
void HsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    int i = (int)(h * 6.f);
    float f = h * 6.f - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);
    
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

bool LoadDemoOpenGLFunctions(NkIGraphicsContext* ctx) {
#if defined(NK_DEMO_HAS_GLAD)
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    if (!loader) {
        loader = &GetWebGLProcAddressFallback;
        logger.Warnf("[GL] Context loader unavailable; fallback to emscripten_webgl_get_proc_address");
    }
#endif
    if (!loader) {
        logger.Errorf("[GL] OpenGL proc loader unavailable from context");
        return false;
    }
#   if defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    int ver = gladLoadGLES2((GLADloadfunc)loader);
#   else
    int ver = gladLoadGL((GLADloadfunc)loader);
#   endif
    if (!ver) {
        logger.Errorf("[GL] GLAD load failed from context loader");
        return false;
    }

    gGL.CreateShader       = glCreateShader;
    gGL.ShaderSource       = glShaderSource;
    gGL.CompileShader      = glCompileShader;
    gGL.GetShaderiv        = glGetShaderiv;
    gGL.GetShaderInfoLog   = glGetShaderInfoLog;
    gGL.CreateProgram      = glCreateProgram;
    gGL.AttachShader       = glAttachShader;
    gGL.LinkProgram        = glLinkProgram;
    gGL.GetProgramiv       = glGetProgramiv;
    gGL.GetProgramInfoLog  = glGetProgramInfoLog;
    gGL.BindAttribLocation = glBindAttribLocation;
    gGL.DeleteShader       = glDeleteShader;
    gGL.DeleteProgram      = glDeleteProgram;
    gGL.UseProgram         = glUseProgram;
    gGL.GetUniformLocation = glGetUniformLocation;
    gGL.Uniform1f          = glUniform1f;
    gGL.GenVertexArrays    = glGenVertexArrays;
    gGL.BindVertexArray    = glBindVertexArray;
    gGL.DeleteVertexArrays = glDeleteVertexArrays;
    gGL.GenBuffers         = glGenBuffers;
    gGL.BindBuffer         = glBindBuffer;
    gGL.BufferData         = glBufferData;
    gGL.DeleteBuffers      = glDeleteBuffers;
    gGL.EnableVertexAttribArray = glEnableVertexAttribArray;
    gGL.VertexAttribPointer = glVertexAttribPointer;
    gGL.DrawArrays         = glDrawArrays;
    gGL.DrawElements       = glDrawElements;

    // VAO is optional on ES2 stacks.
    gGLSupportsVAO = gGL.GenVertexArrays && gGL.BindVertexArray && gGL.DeleteVertexArrays;
    if (!gGLSupportsVAO) {
        logger.Warnf("[GL] VAO not available, using VBO-only fallback path");
    }

    const bool requiredReady =
        gGL.CreateShader && gGL.ShaderSource && gGL.CompileShader &&
        gGL.GetShaderiv && gGL.GetShaderInfoLog &&
        gGL.CreateProgram && gGL.AttachShader && gGL.LinkProgram &&
        gGL.GetProgramiv && gGL.GetProgramInfoLog &&
        gGL.DeleteShader && gGL.DeleteProgram &&
        gGL.UseProgram && gGL.GetUniformLocation && gGL.Uniform1f &&
        gGL.GenBuffers && gGL.BindBuffer && gGL.BufferData && gGL.DeleteBuffers &&
        gGL.EnableVertexAttribArray && gGL.VertexAttribPointer &&
        gGL.DrawArrays && gGL.DrawElements;
    if (!requiredReady) {
        logger.Errorf("[GL] GLAD loaded, but required OpenGL functions are missing");
        return false;
    }
    return true;
#else
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    if (!loader) {
        loader = &GetWebGLProcAddressFallback;
        logger.Warnf("[GL] Context loader unavailable; fallback to emscripten_webgl_get_proc_address");
    }
#endif
    if (!loader) {
        logger.Errorf("[GL] OpenGL proc loader unavailable from context");
        return false;
    }
    bool ok = true;
    ok &= LoadDemoGLProc(gGL.CreateShader, loader, "glCreateShader");
    ok &= LoadDemoGLProc(gGL.ShaderSource, loader, "glShaderSource");
    ok &= LoadDemoGLProc(gGL.CompileShader, loader, "glCompileShader");
    ok &= LoadDemoGLProc(gGL.GetShaderiv, loader, "glGetShaderiv");
    ok &= LoadDemoGLProc(gGL.GetShaderInfoLog, loader, "glGetShaderInfoLog");
    ok &= LoadDemoGLProc(gGL.CreateProgram, loader, "glCreateProgram");
    ok &= LoadDemoGLProc(gGL.AttachShader, loader, "glAttachShader");
    ok &= LoadDemoGLProc(gGL.LinkProgram, loader, "glLinkProgram");
    ok &= LoadDemoGLProc(gGL.GetProgramiv, loader, "glGetProgramiv");
    ok &= LoadDemoGLProc(gGL.GetProgramInfoLog, loader, "glGetProgramInfoLog");
    ok &= LoadDemoGLProc(gGL.BindAttribLocation, loader, "glBindAttribLocation");
    ok &= LoadDemoGLProc(gGL.DeleteShader, loader, "glDeleteShader");
    ok &= LoadDemoGLProc(gGL.DeleteProgram, loader, "glDeleteProgram");
    ok &= LoadDemoGLProc(gGL.UseProgram, loader, "glUseProgram");
    ok &= LoadDemoGLProc(gGL.GetUniformLocation, loader, "glGetUniformLocation");
    ok &= LoadDemoGLProc(gGL.Uniform1f, loader, "glUniform1f");
    gGLSupportsVAO =
        LoadDemoGLProc(gGL.GenVertexArrays, loader, "glGenVertexArrays") &&
        LoadDemoGLProc(gGL.BindVertexArray, loader, "glBindVertexArray") &&
        LoadDemoGLProc(gGL.DeleteVertexArrays, loader, "glDeleteVertexArrays");
    if (!gGLSupportsVAO) {
        logger.Warnf("[GL] VAO not available, using VBO-only fallback path");
    }
    ok &= LoadDemoGLProc(gGL.GenBuffers, loader, "glGenBuffers");
    ok &= LoadDemoGLProc(gGL.BindBuffer, loader, "glBindBuffer");
    ok &= LoadDemoGLProc(gGL.BufferData, loader, "glBufferData");
    ok &= LoadDemoGLProc(gGL.DeleteBuffers, loader, "glDeleteBuffers");
    ok &= LoadDemoGLProc(gGL.EnableVertexAttribArray, loader, "glEnableVertexAttribArray");
    ok &= LoadDemoGLProc(gGL.VertexAttribPointer, loader, "glVertexAttribPointer");
    ok &= LoadDemoGLProc(gGL.DrawArrays, loader, "glDrawArrays");
    ok &= LoadDemoGLProc(gGL.DrawElements, loader, "glDrawElements");
    if (!ok) {
        logger.Errorf("[GL] Failed to initialize OpenGL function table");
        return false;
    }
    return true;
#endif
}

#if NKENTSEU_HAS_VULKAN_HEADERS

struct VkDemoPush {
    float time;
    float width;
    float height;
    float pad;
};
struct VkDemoPipeline {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

static VkShaderModule CreateVkShaderModule(VkDevice dev, const uint32* words, uint32 wordCount) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = (size_t)wordCount * sizeof(uint32);
    ci.pCode = words;
    VkShaderModule mod = VK_NULL_HANDLE;
    if (vkCreateShaderModule(dev, &ci, nullptr, &mod) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return mod;
}

static bool CreateVulkanShapePipeline(NkIGraphicsContext* ctx, VkDemoPipeline& outPipe) {
    NkVulkanContextData* d = NkNativeContext::Vulkan(ctx);
    if (!d || !d->device || !d->renderPass) return false;

    VkShaderModule vs = CreateVkShaderModule(d->device, kVkShapesVertSpv, kVkShapesVertSpvWordCount);
    VkShaderModule fs = CreateVkShaderModule(d->device, kVkShapesFragSpv, kVkShapesFragSpvWordCount);
    if (!vs || !fs) {
        logger.Errorf("[Vulkan] Shader module creation failed");
        if (vs) vkDestroyShaderModule(d->device, vs, nullptr);
        if (fs) vkDestroyShaderModule(d->device, fs, nullptr);
        return false;
    }

    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push.offset = 0;
    push.size = sizeof(VkDemoPush);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(d->device, &plci, nullptr, &outPipe.layout) != VK_SUCCESS) {
        logger.Errorf("[Vulkan] PipelineLayout creation failed");
        vkDestroyShaderModule(d->device, vs, nullptr);
        vkDestroyShaderModule(d->device, fs, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkDynamicState dynStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo gpci{};
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &vi;
    gpci.pInputAssemblyState = &ia;
    gpci.pViewportState = &vp;
    gpci.pRasterizationState = &rs;
    gpci.pMultisampleState = &ms;
    gpci.pDepthStencilState = &ds;
    gpci.pColorBlendState = &cb;
    gpci.pDynamicState = &dyn;
    gpci.layout = outPipe.layout;
    gpci.renderPass = d->renderPass;
    gpci.subpass = 0;

    VkResult r = vkCreateGraphicsPipelines(d->device, VK_NULL_HANDLE, 1, &gpci, nullptr, &outPipe.pipeline);
    vkDestroyShaderModule(d->device, vs, nullptr);
    vkDestroyShaderModule(d->device, fs, nullptr);
    if (r != VK_SUCCESS) {
        logger.Errorf("[Vulkan] Graphics pipeline creation failed");
        vkDestroyPipelineLayout(d->device, outPipe.layout, nullptr);
        outPipe.layout = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

static void DestroyVulkanShapePipeline(NkIGraphicsContext* ctx, VkDemoPipeline& p) {
    NkVulkanContextData* d = NkNativeContext::Vulkan(ctx);
    if (!d || !d->device) return;
    if (p.pipeline) vkDestroyPipeline(d->device, p.pipeline, nullptr);
    if (p.layout) vkDestroyPipelineLayout(d->device, p.layout, nullptr);
    p = VkDemoPipeline{};
}

#endif // NKENTSEU_HAS_VULKAN_HEADERS

#if defined(NKENTSEU_PLATFORM_WINDOWS)
struct Dx12DemoPipeline {
    ComPtr<ID3D12RootSignature> rootSig;
    ComPtr<ID3D12PipelineState> pso;
};

static bool CreateDx12ShapePipeline(NkIGraphicsContext* ctx, Dx12DemoPipeline& outPipe) {
    ID3D12Device5* device = NkNativeContext::GetDX12Device(ctx);
    if (!device) return false;

    const char* hlsl = R"(
cbuffer Cb : register(b0) { float4 uData; } // x=time, y=width, z=height, w=pad
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSMain(uint vid : SV_VertexID) {
    float2 pos[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };
    VSOut o;
    o.pos = float4(pos[vid], 0, 1);
    o.uv = pos[vid] * 0.5 + 0.5;
    return o;
}

float sdBox(float2 p, float2 b) {
    float2 d = abs(p) - b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

float3 hsv2rgb(float3 c) {
    float3 p = abs(frac(c.xxx + float3(0.0, 2.0/3.0, 1.0/3.0))*6.0 - 3.0);
    return c.z * lerp(float3(1,1,1), saturate(p - 1.0), c.y);
}

float4 PSMain(VSOut i) : SV_TARGET {
    float t = uData.x;
    float aspect = uData.y / max(uData.z, 1.0);
    float2 p = i.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 col = float3(0.04, 0.05, 0.08) + 0.06*float3(i.uv.y, 0.2, 1.0-i.uv.y);

    float2 c0 = p - float2(-0.75 + 0.03*sin(t*2.2), 0.08);
    float dCircle = length(c0) - (0.28 + 0.04*sin(t*2.8));
    float mCircle = smoothstep(0.02, 0.0, dCircle);

    float2 q = p;
    float cs = cos(t*1.1), sn = sin(t*1.1);
    q = float2(cs*q.x - sn*q.y, sn*q.x + cs*q.y);
    float dBox = sdBox(q, float2(0.22, 0.22));
    float mBox = smoothstep(0.02, 0.0, dBox);

    float2 c2 = p - float2(0.78, -0.04);
    float dCross = min(sdBox(c2, float2(0.24, 0.07)), sdBox(c2, float2(0.07, 0.24)));
    float mCross = smoothstep(0.02, 0.0, dCross);

    float3 circleCol = hsv2rgb(float3(frac(0.08*t + 0.05), 0.85, 1.0));
    float3 boxCol    = hsv2rgb(float3(frac(0.08*t + 0.35), 0.80, 1.0));
    float3 crossCol  = hsv2rgb(float3(frac(0.08*t + 0.65), 0.80, 1.0));

    col = lerp(col, circleCol, mCircle);
    col = lerp(col, boxCol, mBox);
    col = lerp(col, crossCol, mCross);
    return float4(col, 1.0);
}
)";

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        logger.Errorf("[DX12] VS compile failed: %s",
            errBlob ? static_cast<const char*>(errBlob->GetBufferPointer()) : "unknown error");
        return false;
    }
    hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        logger.Errorf("[DX12] PS compile failed: %s",
            errBlob ? static_cast<const char*>(errBlob->GetBufferPointer()) : "unknown error");
        return false;
    }

    D3D12_ROOT_PARAMETER rootParam{};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.Constants.RegisterSpace = 0;
    rootParam.Constants.Num32BitValues = 4;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters = &rootParam;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rsBlob;
    ComPtr<ID3DBlob> rsErrBlob;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsErrBlob);
    if (FAILED(hr)) {
        logger.Errorf("[DX12] RootSignature serialization failed: %s",
            rsErrBlob ? static_cast<const char*>(rsErrBlob->GetBufferPointer()) : "unknown error");
        return false;
    }
    hr = device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(),
                                     IID_PPV_ARGS(&outPipe.rootSig));
    if (FAILED(hr)) return false;

    D3D12_RASTERIZER_DESC rast{};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_NONE;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rast.DepthClipEnable = TRUE;
    rast.MultisampleEnable = FALSE;
    rast.AntialiasedLineEnable = FALSE;
    rast.ForcedSampleCount = 0;
    rast.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend{};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC rtBlend{};
    rtBlend.BlendEnable = FALSE;
    rtBlend.LogicOpEnable = FALSE;
    rtBlend.SrcBlend = D3D12_BLEND_ONE;
    rtBlend.DestBlend = D3D12_BLEND_ZERO;
    rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlend.DestBlendAlpha = D3D12_BLEND_ZERO;
    rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.RenderTarget[0] = rtBlend;

    D3D12_DEPTH_STENCIL_DESC dss{};
    dss.DepthEnable = FALSE;
    dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    dss.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    dss.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = outPipe.rootSig.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blend;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rast;
    psoDesc.DepthStencilState = dss;
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&outPipe.pso));
    return SUCCEEDED(hr);
}
#endif

} // anonymous namespace

// =============================================================================
// ██████  DEMO OPENGL
// Gauche  : Triangle RGB dont les sommets pulsent indépendamment
// Droite  : Quad UV qui tourne et colore en arc-en-ciel
// =============================================================================
namespace {
    GLuint CompileGL(GLenum type, const char* src) {
        GLuint s = gGL.CreateShader(type);
        gGL.ShaderSource(s, 1, &src, nullptr);
        gGL.CompileShader(s);
        GLint ok = 0;
        gGL.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char buf[512] = {};
            gGL.GetShaderInfoLog(s, 512, nullptr, buf);
            logger.Errorf("[GL] %s", buf);
            gGL.DeleteShader(s);
            return 0;
        }
        return s;
    }
    GLuint LinkGL(GLuint vs, GLuint fs, const char* attrib0 = nullptr, const char* attrib1 = nullptr) {
        if (!vs || !fs) {
            if (vs) gGL.DeleteShader(vs);
            if (fs) gGL.DeleteShader(fs);
            return 0;
        }
        GLuint p = gGL.CreateProgram();
        gGL.AttachShader(p, vs);
        gGL.AttachShader(p, fs);
        if (attrib0 && gGL.BindAttribLocation) gGL.BindAttribLocation(p, 0, attrib0);
        if (attrib1 && gGL.BindAttribLocation) gGL.BindAttribLocation(p, 1, attrib1);
        gGL.LinkProgram(p);
        GLint ok = 0;
        gGL.GetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[512] = {};
            gGL.GetProgramInfoLog(p, 512, nullptr, buf);
            logger.Errorf("[GL] Program link failed: %s", buf);
            gGL.DeleteProgram(p);
            p = 0;
        }
        gGL.DeleteShader(vs);
        gGL.DeleteShader(fs);
        return p;
    }

    struct DemoGLCaps {
        bool isES = false;
        int major = 0;
        int minor = 0;
    };

    DemoGLCaps DetectGLCaps() {
        DemoGLCaps caps{};
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        if (!version) return caps;

        caps.isES = std::strstr(version, "OpenGL ES") != nullptr;

        const char* p = version;
        while (*p && !std::isdigit(static_cast<unsigned char>(*p))) ++p;
        if (*p) {
            char* end = nullptr;
            caps.major = static_cast<int>(std::strtol(p, &end, 10));
            if (end && *end == '.') {
                caps.minor = static_cast<int>(std::strtol(end + 1, nullptr, 10));
            }
        }
        return caps;
    }
}

constexpr bool kDemoUseGLESShaders =
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
    true;
#else
    false;
#endif

static NkContextDesc MakeDemoOpenGLDesc() {
    NkContextDesc desc;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    desc = NkContextDesc::MakeOpenGLES(3, 0);
    desc.api = NkGraphicsApi::NK_API_WEBGL;
    desc.opengl.runtime.installDebugCallback = false;
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
    desc = NkContextDesc::MakeOpenGLES(3, 0);
#else
    desc = NkContextDesc::MakeOpenGL(4, 3, false);
#endif
    // Demo-side loader (custom), context does not force a loader.
    desc.opengl.runtime.autoLoadEntryPoints = false;
    return desc;
}

static bool DemoOpenGL(const NkEntryState&) {
    NkWindow win;
    logger.Warn("[DemoGL][DBG] before CreateDemoWindow");
    if (!CreateDemoWindow(win, "NkEngine - OpenGL: Triangle pulsant + Quad UV rotatif", 1280, 720)) {
        logger.Errorf("[GL] Window creation failed");
        return false;
    }
    logger.Warn("[DemoGL][DBG] after CreateDemoWindow");

    logger.Warn("[DemoGL][DBG] before MakeDemoOpenGLDesc");
    NkContextDesc desc = MakeDemoOpenGLDesc();
    logger.Warn("[DemoGL][DBG] after MakeDemoOpenGLDesc");
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { logger.Errorf("[GL] Context creation failed"); return false; }
    logger.Infof("[GL] Renderer : %s\n", ctx->GetInfo().renderer);
    logger.Infof("[GL] Compute  : %s\n", ctx->SupportsCompute() ? "yes (GL 4.3+)" : "no");

    if (!LoadDemoOpenGLFunctions(ctx)) {
        ctx->Shutdown();
        delete ctx;
        return false;
    }

    // ── Triangle pulsant ─────────────────────────────────────────────────
    const DemoGLCaps glCaps = DetectGLCaps();
    const bool useGLES2Shaders = glCaps.isES && glCaps.major > 0 && glCaps.major < 3;
    const bool useGLES3Shaders = !useGLES2Shaders && kDemoUseGLESShaders;

    const char* vsT = useGLES2Shaders
        ? R"(#version 100
attribute vec2 aPos;
attribute vec3 aCol;
uniform float uTime;
varying vec3 vCol;
void main(){
    float phase = aPos.x*2.1 + aPos.y*3.7;
    float scale = 0.65 + 0.25*sin(uTime*2.0 + phase);
    gl_Position = vec4(aPos * scale + vec2(-0.5, 0.0), 0.0, 1.0);
    vCol = aCol * (0.6 + 0.4*sin(uTime*3.0 + phase));
})"
        : useGLES3Shaders
        ? R"(#version 300 es
precision mediump float;
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aCol;
uniform float uTime;
out vec3 vCol;
void main(){
    float phase = aPos.x*2.1 + aPos.y*3.7;
    float scale = 0.65 + 0.25*sin(uTime*2.0 + phase);
    gl_Position = vec4(aPos * scale + vec2(-0.5, 0.0), 0.0, 1.0);
    vCol = aCol * (0.6 + 0.4*sin(uTime*3.0 + phase));
})"
        : R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec3 aCol;
uniform float uTime;
out vec3 vCol;
void main(){
    float phase = aPos.x*2.1 + aPos.y*3.7;
    float scale = 0.65 + 0.25*sin(uTime*2.0 + phase);
    gl_Position = vec4(aPos * scale + vec2(-0.5, 0.0), 0.0, 1.0);
    vCol = aCol * (0.6 + 0.4*sin(uTime*3.0 + phase));
})";
    const char* fsT = useGLES2Shaders
        ? R"(#version 100
precision mediump float;
varying vec3 vCol;
uniform float uTime;
void main(){
    float glow = 0.85 + 0.15*sin(uTime*5.0);
    gl_FragColor = vec4(vCol * glow, 1.0);
})"
        : useGLES3Shaders
        ? R"(#version 300 es
precision mediump float;
in vec3 vCol;
out vec4 frag;
uniform float uTime;
void main(){
    float glow = 0.85 + 0.15*sin(uTime*5.0);
    frag = vec4(vCol * glow, 1.0);
})"
        : R"(#version 330 core
in vec3 vCol; out vec4 frag;
uniform float uTime;
void main(){
    float glow = 0.85 + 0.15*sin(uTime*5.0);
    frag = vec4(vCol * glow, 1.0);
})";

    // ── Quad UV rotatif ─────────────────────────────────────────────────
    const char* vsQ = useGLES2Shaders
        ? R"(#version 100
attribute vec2 aPos;
attribute vec2 aUV;
uniform float uTime;
varying vec2 vUV;
void main(){
    float c = cos(uTime*0.8), s = sin(uTime*0.8);
    vec2 r = vec2(aPos.x*c - aPos.y*s, aPos.x*s + aPos.y*c);
    gl_Position = vec4(r * 0.38 + vec2(0.52, 0.0), 0.0, 1.0);
    vUV = aUV;
})"
        : useGLES3Shaders
        ? R"(#version 300 es
precision mediump float;
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform float uTime;
out vec2 vUV;
void main(){
    float c = cos(uTime*0.8), s = sin(uTime*0.8);
    vec2 r = vec2(aPos.x*c - aPos.y*s, aPos.x*s + aPos.y*c);
    gl_Position = vec4(r * 0.38 + vec2(0.52, 0.0), 0.0, 1.0);
    vUV = aUV;
})"
        : R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
uniform float uTime;
out vec2 vUV;
void main(){
    float c = cos(uTime*0.8), s = sin(uTime*0.8);
    vec2 r = vec2(aPos.x*c - aPos.y*s, aPos.x*s + aPos.y*c);
    gl_Position = vec4(r * 0.38 + vec2(0.52, 0.0), 0.0, 1.0);
    vUV = aUV;
})";
    const char* fsQ = useGLES2Shaders
        ? R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform float uTime;
void main(){
    float hue = fract(vUV.x + vUV.y*0.5 + uTime*0.15);
    float wave = 0.5+0.5*sin(vUV.x*12.0+uTime*4.0)*sin(vUV.y*12.0-uTime*3.0);
    float h6=hue*6.0;
    vec3 rgb = clamp(abs(mod(h6+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0,1.0);
    gl_FragColor = vec4(rgb * (0.6 + 0.4*wave), 1.0);
})"
        : useGLES3Shaders
        ? R"(#version 300 es
precision mediump float;
in vec2 vUV;
out vec4 frag;
uniform float uTime;
void main(){
    float hue = fract(vUV.x + vUV.y*0.5 + uTime*0.15);
    float wave = 0.5+0.5*sin(vUV.x*12.0+uTime*4.0)*sin(vUV.y*12.0-uTime*3.0);
    float h6=hue*6.0;
    vec3 rgb = clamp(abs(mod(h6+vec3(0,4,2),6.0)-3.0)-1.0, 0.0,1.0);
    frag = vec4(rgb * (0.6 + 0.4*wave), 1.0);
})"
        : R"(#version 330 core
in vec2 vUV; out vec4 frag;
uniform float uTime;
void main(){
    float hue = fract(vUV.x + vUV.y*0.5 + uTime*0.15);
    float wave = 0.5+0.5*sin(vUV.x*12.0+uTime*4.0)*sin(vUV.y*12.0-uTime*3.0);
    float h6=hue*6.0;
    vec3 rgb = clamp(abs(mod(h6+vec3(0,4,2),6.0)-3.0)-1.0, 0.0,1.0);
    frag = vec4(rgb * (0.6 + 0.4*wave), 1.0);
})";

    float triV[] = {
         0.0f, 0.6f,  1.f,0.25f,0.25f,
        -0.5f,-0.45f, 0.25f,1.f,0.25f,
         0.5f,-0.45f, 0.25f,0.25f,1.f,
    };
    float quadV[] = {
        -1.f,-1.f, 0.f,0.f,
         1.f,-1.f, 1.f,0.f,
         1.f, 1.f, 1.f,1.f,
        -1.f, 1.f, 0.f,1.f,
    };
    unsigned short quadI[] = {0,1,2, 0,2,3};

    GLuint vaoT = 0, vboT = 0, vaoQ = 0, vboQ = 0, eboQ = 0;
    if (gGLSupportsVAO) gGL.GenVertexArrays(1, &vaoT);
    gGL.GenBuffers(1, &vboT);
    if (gGLSupportsVAO) gGL.BindVertexArray(vaoT);
    gGL.BindBuffer(GL_ARRAY_BUFFER, vboT);
    gGL.BufferData(GL_ARRAY_BUFFER, sizeof(triV), triV, GL_STATIC_DRAW);
    gGL.EnableVertexAttribArray(0);
    gGL.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 20, (void*)0);
    gGL.EnableVertexAttribArray(1);
    gGL.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 20, (void*)8);

    if (gGLSupportsVAO) gGL.GenVertexArrays(1, &vaoQ);
    gGL.GenBuffers(1, &vboQ);
    gGL.GenBuffers(1, &eboQ);
    if (gGLSupportsVAO) gGL.BindVertexArray(vaoQ);
    gGL.BindBuffer(GL_ARRAY_BUFFER, vboQ);
    gGL.BufferData(GL_ARRAY_BUFFER, sizeof(quadV), quadV, GL_STATIC_DRAW);
    gGL.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboQ);
    gGL.BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadI), quadI, GL_STATIC_DRAW);
    gGL.EnableVertexAttribArray(0);
    gGL.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    gGL.EnableVertexAttribArray(1);
    gGL.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    const char* legacyAttr1Tri = useGLES2Shaders ? "aCol" : nullptr;
    const char* legacyAttr1Quad = useGLES2Shaders ? "aUV" : nullptr;
    GLuint progT = LinkGL(
        CompileGL(GL_VERTEX_SHADER,vsT),
        CompileGL(GL_FRAGMENT_SHADER,fsT),
        useGLES2Shaders ? "aPos" : nullptr,
        legacyAttr1Tri);
    GLuint progQ = LinkGL(
        CompileGL(GL_VERTEX_SHADER,vsQ),
        CompileGL(GL_FRAGMENT_SHADER,fsQ),
        useGLES2Shaders ? "aPos" : nullptr,
        legacyAttr1Quad);

    if (!progT || !progQ) {
        logger.Warnf("[GL] Shader pipeline unavailable on this stack, fallback to animated clear.");
        LoopCtx lcFallback;
        RunLoop(win, ctx, lcFallback, [&](float, float t) {
            float r2,g2,b2;
            HsvToRgb(fmodf(t*0.11f,1.f), 0.75f, 0.8f, r2,g2,b2);
            const NkSurfaceDesc surface = win.GetSurfaceDesc();
            glViewport(0, 0, (GLsizei)surface.width, (GLsizei)surface.height);
            glClearColor(r2,g2,b2,1);
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        });
        if (progT) gGL.DeleteProgram(progT);
        if (progQ) gGL.DeleteProgram(progQ);
        if (gGLSupportsVAO && vaoT) gGL.DeleteVertexArrays(1, &vaoT);
        if (vboT) gGL.DeleteBuffers(1, &vboT);
        if (gGLSupportsVAO && vaoQ) gGL.DeleteVertexArrays(1, &vaoQ);
        if (vboQ) gGL.DeleteBuffers(1, &vboQ);
        if (eboQ) gGL.DeleteBuffers(1, &eboQ);
        ctx->Shutdown(); delete ctx;
        return true;
    }

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Fond sombre légèrement teinté
        float r2,g2,b2;
        HsvToRgb(fmodf(t*0.05f,1.f), 0.5f, 0.08f, r2,g2,b2);
        glClearColor(r2,g2,b2,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        const NkSurfaceDesc surface = win.GetSurfaceDesc();
        glViewport(0, 0, (GLsizei)surface.width, (GLsizei)surface.height);

        gGL.UseProgram(progT);
        gGL.Uniform1f(gGL.GetUniformLocation(progT, "uTime"), t);
        if (gGLSupportsVAO) {
            gGL.BindVertexArray(vaoT);
        } else {
            gGL.BindBuffer(GL_ARRAY_BUFFER, vboT);
            gGL.EnableVertexAttribArray(0);
            gGL.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 20, (void*)0);
            gGL.EnableVertexAttribArray(1);
            gGL.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 20, (void*)8);
        }
        gGL.DrawArrays(GL_TRIANGLES, 0, 3);

        gGL.UseProgram(progQ);
        gGL.Uniform1f(gGL.GetUniformLocation(progQ, "uTime"), t);
        if (gGLSupportsVAO) {
            gGL.BindVertexArray(vaoQ);
        } else {
            gGL.BindBuffer(GL_ARRAY_BUFFER, vboQ);
            gGL.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboQ);
            gGL.EnableVertexAttribArray(0);
            gGL.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
            gGL.EnableVertexAttribArray(1);
            gGL.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
        }
        gGL.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    });

    gGL.DeleteProgram(progT);
    gGL.DeleteProgram(progQ);
    if (gGLSupportsVAO && vaoT) gGL.DeleteVertexArrays(1, &vaoT);
    gGL.DeleteBuffers(1, &vboT);
    if (gGLSupportsVAO && vaoQ) gGL.DeleteVertexArrays(1, &vaoQ);
    gGL.DeleteBuffers(1, &vboQ);
    gGL.DeleteBuffers(1, &eboQ);
    ctx->Shutdown(); delete ctx;
    return true;
}

// =============================================================================
// ██████  DEMO VULKAN — Pipeline + formes SDF
// =============================================================================
#if NKENTSEU_HAS_VULKAN_HEADERS
static void DemoVulkan(const NkEntryState&) {
    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - Vulkan: Formes SDF animees", 1280, 720)) {
        logger.Errorf("[Vulkan] Window creation failed");
        return;
    }

    auto desc = NkContextDesc::MakeVulkan(true);
    desc.vulkan.enableComputeQueue = true;
    desc.vulkan.validationLayers = true;          // ← Ajoutez ceci
    desc.vulkan.debugMessenger = true;            // ← Déjà dans MakeVulkan(true)
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { logger.Errorf("[Vulkan] Context failed"); return; }
    logger.Infof("[Vulkan] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);
    logger.Infof("[Vulkan] Compute queue disponible: %s\n", ctx->SupportsCompute() ? "oui" : "non");

    VkDemoPipeline vkPipe{};
    if (!CreateVulkanShapePipeline(ctx, vkPipe)) {
        logger.Errorf("[Vulkan] Failed to create shape pipeline");
        ctx->Shutdown(); delete ctx;
        return;
    }

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        NkVulkanContextData* d = NkNativeContext::Vulkan(ctx);
        VkCommandBuffer cmd = NkNativeContext::GetVkCurrentCommandBuffer(ctx);
        if (!d || cmd == VK_NULL_HANDLE || vkPipe.pipeline == VK_NULL_HANDLE) return;

        VkDemoPush push{
            t,
            static_cast<float>(d->swapExtent.width),
            static_cast<float>(d->swapExtent.height),
            0.0f
        };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipe.pipeline);
        vkCmdPushConstants(cmd, vkPipe.layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(push), &push);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }, 360);

    DestroyVulkanShapePipeline(ctx, vkPipe);

    ctx->Shutdown(); delete ctx;
}
#else
static void DemoVulkan(const NkEntryState&) {
    logger.Infof("[Vulkan] Demo skipped: Vulkan headers are not available at build time.");
}
#endif

// =============================================================================
// ██████  DEMO DIRECTX 11 — Triangle rouge + Quad vert + Croix bleue
// =============================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)
static void DemoDirectX11(const NkEntryState&) {
    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - DX11: Triangle + Quad + Croix HLSL", 1280, 720)) {
        logger.Errorf("[DX11] Window creation failed");
        return;
    }

    auto desc = NkContextDesc::MakeDirectX11(false);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { logger.Errorf("[DX11] Context failed"); return; }
    logger.Infof("[DX11] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    ID3D11Device1*        dev  = NkNativeContext::GetDX11Device(ctx);
    ID3D11DeviceContext1* dctx = NkNativeContext::GetDX11Context(ctx);

    // HLSL avec constant buffer temps
    const char* vsHLSL = R"(
cbuffer CB : register(b0) { float time; float3 _pad; };
struct VSIn  { float2 pos:POSITION; float4 col:COLOR; };
struct VSOut { float4 pos:SV_POSITION; float4 col:COLOR; };
VSOut main(VSIn v, uint vid:SV_VertexID) {
    VSOut o;
    float pulse = 0.8 + 0.2*sin(time*2.5 + float(vid)*0.9);
    o.pos = float4(v.pos * pulse, 0, 1);
    o.col = v.col;
    return o;
})";
    const char* psHLSL = R"(
cbuffer CB : register(b0) { float time; float3 _pad; };
float4 main(float4 pos:SV_POSITION, float4 col:COLOR) : SV_TARGET {
    float shimmer = 0.85 + 0.15*sin(time*6.0 + pos.x*0.01 + pos.y*0.01);
    return float4(col.rgb * shimmer, 1.0);
})";

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hr = D3DCompile(vsHLSL, strlen(vsHLSL), nullptr, nullptr, nullptr,
                            "main", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hr) || !vsBlob) {
        logger.Errorf("[DX11] VS compile failed: %s",
                      errBlob ? (const char*)errBlob->GetBufferPointer() : "unknown");
        ctx->Shutdown(); delete ctx;
        return;
    }
    errBlob.Reset();
    hr = D3DCompile(psHLSL, strlen(psHLSL), nullptr, nullptr, nullptr,
                    "main", "ps_5_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hr) || !psBlob) {
        logger.Errorf("[DX11] PS compile failed: %s",
                      errBlob ? (const char*)errBlob->GetBufferPointer() : "unknown");
        ctx->Shutdown(); delete ctx;
        return;
    }

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    hr = dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vs);
    if (FAILED(hr)) {
        logger.Errorf("[DX11] CreateVertexShader failed (0x%08X)", (unsigned)hr);
        ctx->Shutdown(); delete ctx;
        return;
    }
    hr = dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &ps);
    if (FAILED(hr)) {
        logger.Errorf("[DX11] CreatePixelShader failed (0x%08X)", (unsigned)hr);
        ctx->Shutdown(); delete ctx;
        return;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,       0, 0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",   0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8,D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    ComPtr<ID3D11InputLayout> il;
    hr = dev->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &il);
    if (FAILED(hr)) {
        logger.Errorf("[DX11] CreateInputLayout failed (0x%08X)", (unsigned)hr);
        ctx->Shutdown(); delete ctx;
        return;
    }

    D3D11_RASTERIZER_DESC rast{};
    rast.FillMode = D3D11_FILL_SOLID;
    rast.CullMode = D3D11_CULL_NONE;
    rast.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rs;
    hr = dev->CreateRasterizerState(&rast, &rs);
    if (FAILED(hr)) {
        logger.Errorf("[DX11] CreateRasterizerState failed (0x%08X)", (unsigned)hr);
        ctx->Shutdown(); delete ctx;
        return;
    }

    // Constant buffer
    struct CB { float time; float pad[3]; };
    D3D11_BUFFER_DESC cbd{sizeof(CB),D3D11_USAGE_DYNAMIC,D3D11_BIND_CONSTANT_BUFFER,
                          D3D11_CPU_ACCESS_WRITE,0,0};
    ComPtr<ID3D11Buffer> cbuf;
    hr = dev->CreateBuffer(&cbd, nullptr, &cbuf);
    if (FAILED(hr)) {
        logger.Errorf("[DX11] CreateBuffer(CB) failed (0x%08X)", (unsigned)hr);
        ctx->Shutdown(); delete ctx;
        return;
    }

    // Vertex buffer: 3 formes en un tableau (stride = sizeof Vert = 24 bytes)
    struct Vert { float x,y, r,g,b,a; };
    const Vert kVerts[] = {
        // ── Triangle rouge (gauche) ──
        {-0.75f, 0.55f, 1.f,0.15f,0.15f,1},
        {-1.00f,-0.45f, 1.f,0.15f,0.15f,1},
        {-0.50f,-0.45f, 1.f,0.60f,0.15f,1},

        // ── Quad vert (centre, 2 triangles) ──
        {-0.18f,-0.50f, 0.1f,1.f,0.2f,1},
        { 0.18f,-0.50f, 0.1f,1.f,0.2f,1},
        { 0.18f, 0.50f, 0.2f,0.7f,1.f,1},
        {-0.18f,-0.50f, 0.1f,1.f,0.2f,1},
        { 0.18f, 0.50f, 0.2f,0.7f,1.f,1},
        {-0.18f, 0.50f, 0.2f,0.7f,1.f,1},

        // ── Croix bleue (droite, barre H + barre V) ──
        // barre horizontale
        { 0.35f,-0.07f, 0.2f,0.4f,1.f,1},
        { 0.75f,-0.07f, 0.2f,0.4f,1.f,1},
        { 0.75f, 0.07f, 0.5f,0.7f,1.f,1},
        { 0.35f,-0.07f, 0.2f,0.4f,1.f,1},
        { 0.75f, 0.07f, 0.5f,0.7f,1.f,1},
        { 0.35f, 0.07f, 0.5f,0.7f,1.f,1},
        // barre verticale
        { 0.49f,-0.40f, 0.3f,0.5f,1.f,1},
        { 0.61f,-0.40f, 0.3f,0.5f,1.f,1},
        { 0.61f, 0.40f, 0.6f,0.8f,1.f,1},
        { 0.49f,-0.40f, 0.3f,0.5f,1.f,1},
        { 0.61f, 0.40f, 0.6f,0.8f,1.f,1},
        { 0.49f, 0.40f, 0.6f,0.8f,1.f,1},
    };
    NkVector<Vert> verts;
    verts.Assign(kVerts, sizeof(kVerts) / sizeof(kVerts[0]));

    D3D11_BUFFER_DESC vbd{(UINT)(verts.Size()*sizeof(Vert)),D3D11_USAGE_DEFAULT,
                          D3D11_BIND_VERTEX_BUFFER,0,0,0};
    D3D11_SUBRESOURCE_DATA vInit{verts.Data(),0,0};
    ComPtr<ID3D11Buffer> vb;
    hr = dev->CreateBuffer(&vbd, &vInit, &vb);
    if (FAILED(hr)) {
        logger.Errorf("[DX11] CreateBuffer(VB) failed (0x%08X)", (unsigned)hr);
        ctx->Shutdown(); delete ctx;
        return;
    }

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Update temps
        D3D11_MAPPED_SUBRESOURCE mapped{};
        dctx->Map(cbuf.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        CB cb{t,{0,0,0}}; memcpy(mapped.pData,&cb,sizeof(cb));
        dctx->Unmap(cbuf.Get(),0);

        dctx->IASetInputLayout(il.Get());
        UINT stride=sizeof(Vert), off=0;
        ID3D11Buffer* vbs[]={vb.Get()};
        dctx->IASetVertexBuffers(0,1,vbs,&stride,&off);
        dctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        dctx->VSSetShader(vs.Get(),nullptr,0);
        dctx->PSSetShader(ps.Get(),nullptr,0);
        dctx->RSSetState(rs.Get());
        ID3D11Buffer* cbs[]={cbuf.Get()};
        dctx->VSSetConstantBuffers(0,1,cbs);
        dctx->PSSetConstantBuffers(0,1,cbs);
        dctx->Draw((UINT)verts.Size(),0);
    });

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// ██████  DEMO DIRECTX 12 — Pipeline + formes SDF (VERSION CORRIGÉE)
// =============================================================================
static void DemoDirectX12(const NkEntryState&) {
    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - DX12: Formes SDF animees", 1280, 720)) {
        logger.Errorf("[DX12] Window creation failed");
        return;
    }

    auto desc = NkContextDesc::MakeDirectX12(true);
    desc.dx12.enableComputeQueue = true;
    desc.dx12.debugDevice = true;
    desc.dx12.gpuValidation = true;
    
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { 
        logger.Errorf("[DX12] Context failed"); 
        return; 
    }
    logger.Infof("[DX12] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    Dx12DemoPipeline pipe{};
    if (!CreateDx12ShapePipeline(ctx, pipe)) {
        logger.Errorf("[DX12] Failed to create shape pipeline");
        ctx->Shutdown(); delete ctx;
        return;
    }

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        logger.Warn("001");
        ID3D12GraphicsCommandList4* cmd = NkNativeContext::GetDX12CommandList(ctx);
        logger.Warn("002");
        NkDX12ContextData* d = NkNativeContext::DX12(ctx);
        logger.Warn("003");
        
        if (!cmd || !d || !pipe.rootSig || !pipe.pso) {
            logger.Error("[DX12] Invalid state in render loop");
            return;
        }
        logger.Warn("004");

        // IMPORTANT : Configurer le viewport et le scissor rect !
        D3D12_VIEWPORT viewport = {
            0.0f, 0.0f,
            static_cast<float>(d->width),
            static_cast<float>(d->height),
            0.0f, 1.0f
        };
        D3D12_RECT scissorRect = {
            0, 0,
            static_cast<LONG>(d->width),
            static_cast<LONG>(d->height)
        };
        
        cmd->RSSetViewports(1, &viewport);
        cmd->RSSetScissorRects(1, &scissorRect);
        logger.Warn("005");

        float push[4] = {
            t,
            static_cast<float>(d->width),
            static_cast<float>(d->height),
            0.0f
        };
        logger.Warn("006");

        cmd->SetGraphicsRootSignature(pipe.rootSig.Get());
        logger.Warn("007");
        cmd->SetPipelineState(pipe.pso.Get());
        logger.Warn("008");
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        logger.Warn("009");
        cmd->SetGraphicsRoot32BitConstants(0, 4, push, 0);
        logger.Warn("010");
        
        // Dessiner
        cmd->DrawInstanced(3, 1, 0, 0);
        logger.Warn("011");
    }, 360);

    ctx->Shutdown(); delete ctx;
}
#endif // WINDOWS

// =============================================================================
// ██████  DEMO METAL — Triangle animé (gauche) + Cercle SDF arc-en-ciel (droite)
// =============================================================================
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
static void DemoMetal(const NkEntryState&) {
    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - Metal: Triangle + Cercle SDF", 1280, 720)) {
        logger.Errorf("[Metal] Window creation failed");
        return;
    }

    auto desc = NkContextDesc::MakeMetal();
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { logger.Errorf("[Metal] Context failed"); return; }
    logger.Infof("[Metal] %s | VRAM %u MB\n", ctx->GetInfo().renderer, ctx->GetInfo().vramMB);

    void* rawDev = NkNativeContext::GetMetalDevice(ctx);
    if (!rawDev) { ctx->Shutdown(); delete ctx; return; }
    id<MTLDevice> device = (__bridge id<MTLDevice>)rawDev;

    // ── Shaders MSL ───────────────────────────────────────────────────────
    const char* mslSrc = R"(
#include <metal_stdlib>
using namespace metal;

struct Uniforms { float time; float2 resolution; float _pad; };

// ─── Triangle pulsant ───────────────────────────────────────────────────
struct TVert { float2 pos [[attribute(0)]]; float3 col [[attribute(1)]]; };
struct TFrag { float4 pos [[position]]; float3 col; float id; };

vertex TFrag tri_vert(TVert in [[stage_in]],
                      constant Uniforms& u [[buffer(1)]],
                      uint vid [[vertex_id]]) {
    TFrag out;
    float id = float(vid);
    float scale = 0.7 + 0.25*sin(u.time*2.0 + id*1.8);
    out.pos = float4(in.pos * scale + float2(-0.5, 0.0), 0.0, 1.0);
    out.col = in.col;
    out.id  = id;
    return out;
}
fragment float4 tri_frag(TFrag in [[stage_in]],
                          constant Uniforms& u [[buffer(1)]]) {
    float glow = 0.7 + 0.3*sin(u.time*5.0 + in.id*0.9);
    return float4(in.col * glow, 1.0);
}

// ─── Cercle SDF arc-en-ciel ─────────────────────────────────────────────
struct CFrag { float4 pos [[position]]; float2 uv; };

vertex CFrag circle_vert(uint vid [[vertex_id]],
                          constant Uniforms& u [[buffer(0)]]) {
    // 6 vertices = 2 triangles couvrant la moitié droite
    float2 pts[6] = {
        {0.05,-1},{1,-1},{1,1},
        {0.05,-1},{1,1},{0.05,1}
    };
    CFrag out;
    out.pos = float4(pts[vid], 0.0, 1.0);
    out.uv  = pts[vid];
    return out;
}
fragment float4 circle_frag(CFrag in [[stage_in]],
                              constant Uniforms& u [[buffer(0)]]) {
    float2 center = float2(0.55, 0.0);
    float2 p = in.uv - center;
    p.x *= u.resolution.x / u.resolution.y;

    // Rayon pulsant
    float R  = 0.28 + 0.06*sin(u.time*3.0);
    float R2 = 0.12 + 0.03*cos(u.time*5.0);
    float d  = length(p);

    // Anneau extérieur (entre R2 et R)
    float ring = smoothstep(0.008, 0.0, abs(d-R) - 0.015);

    // Disque intérieur
    float inner = smoothstep(0.008, 0.0, d - R2);

    // Rayons (8 branches tournantes)
    float angle = atan2(p.y, p.x);
    float spoke = pow(0.5+0.5*sin(angle*8.0 + u.time*4.0), 6.0);
    float spokes = spoke * smoothstep(R,R2,d) * smoothstep(R2*0.2f,R2,d);

    // Couleur arc-en-ciel selon angle + temps
    float hue  = fract(angle / (2*M_PI_F) + u.time*0.08);
    float3 col = clamp(abs(fmod(hue*6.0 + float3(0,4,2), 6.0) - 3.0) - 1.0, 0.0, 1.0);

    float mask = saturate(ring + inner*0.4 + spokes);
    return float4(col * mask, mask);
}
)";

    NSError* err = nil;
    id<MTLLibrary> lib = [device newLibraryWithSource:
        [NSString stringWithUTF8String:mslSrc] options:nil error:&err];
    if (!lib) {
        logger.Errorf("[Metal] MSL error: %s",
               err ? [[err localizedDescription] UTF8String] : "?");
        ctx->Shutdown(); delete ctx; return;
    }

    // Fonctions
    id<MTLFunction> triV    = [lib newFunctionWithName:@"tri_vert"];
    id<MTLFunction> triF    = [lib newFunctionWithName:@"tri_frag"];
    id<MTLFunction> circleV = [lib newFunctionWithName:@"circle_vert"];
    id<MTLFunction> circleF = [lib newFunctionWithName:@"circle_frag"];

    // Vertex descriptor pour le triangle
    MTLVertexDescriptor* vd = [MTLVertexDescriptor new];
    vd.attributes[0].format=MTLVertexFormatFloat2; vd.attributes[0].offset=0;  vd.attributes[0].bufferIndex=0;
    vd.attributes[1].format=MTLVertexFormatFloat3; vd.attributes[1].offset=8;  vd.attributes[1].bufferIndex=0;
    vd.layouts[0].stride = 20;

    // Pipeline triangle
    MTLRenderPipelineDescriptor* triPD = [MTLRenderPipelineDescriptor new];
    triPD.vertexFunction   = triV;
    triPD.fragmentFunction = triF;
    triPD.vertexDescriptor = vd;
    triPD.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    id<MTLRenderPipelineState> triPSO =
        [device newRenderPipelineStateWithDescriptor:triPD error:&err];

    // Pipeline cercle (avec blending alpha pour superposition)
    MTLRenderPipelineDescriptor* circlePD = [MTLRenderPipelineDescriptor new];
    circlePD.vertexFunction   = circleV;
    circlePD.fragmentFunction = circleF;
    circlePD.colorAttachments[0].pixelFormat         = MTLPixelFormatBGRA8Unorm_sRGB;
    circlePD.colorAttachments[0].blendingEnabled      = YES;
    circlePD.colorAttachments[0].sourceRGBBlendFactor      = MTLBlendFactorSourceAlpha;
    circlePD.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    id<MTLRenderPipelineState> circlePSO =
        [device newRenderPipelineStateWithDescriptor:circlePD error:&err];

    // Vertex buffer du triangle
    typedef struct { float x,y,r,g,b; } TV;
    TV tverts[] = {
        { 0.0f, 0.65f, 1.f,0.25f,0.25f},
        {-0.5f,-0.5f,  0.25f,1.f,0.25f},
        { 0.5f,-0.5f,  0.25f,0.25f,1.f},
    };
    id<MTLBuffer> triVB = [device newBufferWithBytes:tverts
                                              length:sizeof(tverts)
                                             options:MTLResourceStorageModeShared];

    struct Uniforms { float time, resX, resY, pad; };

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        void* rawEnc = NkNativeContext::GetMetalCommandEncoder(ctx);
        if (!rawEnc) return;
        id<MTLRenderCommandEncoder> enc =
            (__bridge id<MTLRenderCommandEncoder>)rawEnc;

        Uniforms u{t, (float)win.GetWidth(), (float)win.GetHeight(), 0.f};

        // ── Triangle ──
        [enc setRenderPipelineState:triPSO];
        [enc setVertexBuffer:triVB offset:0 atIndex:0];
        [enc setVertexBytes:&u  length:sizeof(u) atIndex:1];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:1];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];

        // ── Cercle SDF ──
        [enc setRenderPipelineState:circlePSO];
        [enc setVertexBytes:&u  length:sizeof(u) atIndex:0];
        [enc setFragmentBytes:&u length:sizeof(u) atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    });

    ctx->Shutdown(); delete ctx;
}
#endif // METAL

// =============================================================================
// ██████  DEMO SOFTWARE — Spirale de rayons + Dégradé radial HSV (CPU)
// =============================================================================
static void DemoSoftware(const NkEntryState&) {
    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - Software: Spirale HSV CPU", 900, 700)) {
        logger.Errorf("[SW] Window creation failed");
        return;
    }

    auto desc = NkContextDesc::MakeSoftware(true);
    NkIGraphicsContext* ctx = NkContextFactory::Create(win, desc);
    if (!ctx) { logger.Errorf("[SW] Context failed"); return; }
    logger.Infof("[SW] Software renderer actif\n");

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);
        if (!fb || !fb->IsValid()) return;

        uint32 W=fb->width, H=fb->height;
        float cx=W*0.5f, cy=H*0.5f;
        float maxR = math::NkMin(cx, cy) * 0.92f;

        for (uint32 y=0; y<H; ++y) {
            float fy = y - cy;
            for (uint32 x=0; x<W; ++x) {
                float fx = x - cx;
                float r  = sqrtf(fx*fx + fy*fy);
                float a  = atan2f(fy, fx);

                float nr = r / maxR; // 0..1

                // ── Dégradé radial HSV ──
                float hue = fmodf(nr*0.6f + t*0.07f, 1.f);
                float sat = 0.85f;
                float val = (nr < 1.f) ? (1.f - nr*0.35f) : 0.f;

                // ── Rayons animés (8 branches en spirale) ──
                float spinA = a - r*0.035f + t*1.5f;
                float rays  = powf(0.5f + 0.5f*sinf(spinA*8.f), 5.f);

                // ── Anneaux ──
                float rings = 0.5f + 0.5f*sinf(r*0.14f - t*5.f);
                rings = powf(rings, 3.f);

                float bright = 0.25f + 0.55f*rays + 0.20f*rings;
                // Trou central progressif
                float center = math::NkMin(1.f, r/(maxR*0.12f));
                // Bord doux
                float edge   = (nr < 1.f) ? 1.f : 0.f;
                bright *= center * edge;
                bright = math::NkMin(bright, 1.f);

                float pr,pg,pb;
                HsvToRgb(hue, sat, val, pr,pg,pb);

                fb->SetPixel(x, y,
                    (uint8)(pr*bright*255),
                    (uint8)(pg*bright*255),
                    (uint8)(pb*bright*255));
            }
        }

        // ── Logo "NK" en pixels blancs centré ──
        static const uint8 N7[7]={0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001};
        static const uint8 K7[7]={0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001};
        uint32 scale=9, ox=W/2-52, oy=H/2-31;
        for (uint32 row=0;row<7;row++) {
            for (uint32 col=0;col<5;col++) {
                // N
                if (N7[row]&(1<<(4-col)))
                    for (uint32 sy=0;sy<scale;sy++)
                        for (uint32 sx=0;sx<scale;sx++)
                            fb->SetPixel(ox+col*scale+sx, oy+row*scale+sy, 255,255,255);
                // K
                if (K7[row]&(1<<(4-col)))
                    for (uint32 sy=0;sy<scale;sy++)
                        for (uint32 sx=0;sx<scale;sx++)
                            fb->SetPixel(ox+col*scale+sx+54, oy+row*scale+sy, 255,255,255);
            }
        }
    });

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// ██████  DEMO COMPUTE — Addition de vecteurs GPU + vérification
// =============================================================================
static void DemoCompute(const NkEntryState&) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
    logger.Warnf("[Compute] Demo skipped on this platform\n");
    return;
#endif

    logger.Infof("\n===============================\n");
    logger.Infof("      DEMO COMPUTE (GLSL)\n");
    logger.Infof("===============================\n");

    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - Compute", 640, 480)) {
        logger.Errorf("[Compute] Window creation failed");
        return;
    }
    auto desc = NkContextDesc::MakeOpenGL(4,3,false);
    desc.compute.enable = true;
    desc.compute.opengl = true;
    NkIGraphicsContext* gfx = NkContextFactory::Create(win, desc);
    if (!gfx) { logger.Errorf("[Compute] GL context failed"); return; }

    NkIComputeContext* compute = NkContextFactory::ComputeFromGraphics(gfx);
    if (!compute) { logger.Errorf("[Compute] ComputeFromGraphics failed"); gfx->Shutdown(); delete gfx; return; }

    logger.Infof("API       : %s\n", NkGraphicsApiName(compute->GetApi()));
    logger.Infof("Max group : %u\n", compute->GetMaxGroupSizeX());
    logger.Infof("Shared mem: %llu KB\n", compute->GetSharedMemoryBytes()/1024);

    const uint32 N = 2048;
    NkVector<float> A, B, C;
    A.Resize((NkVector<float>::SizeType)N);
    B.Resize((NkVector<float>::SizeType)N);
    C.Resize((NkVector<float>::SizeType)N, 0.0f);
    for (uint32 i=0;i<N;i++) { A[i]=(float)i*0.5f; B[i]=sinf((float)i*0.1f)*100.f; }

    const char* glsl = R"(#version 430
layout(local_size_x=256) in;
layout(std430,binding=0) buffer BA { float a[]; };
layout(std430,binding=1) buffer BB { float b[]; };
layout(std430,binding=2) buffer BC { float c[]; };
void main(){
    uint i=gl_GlobalInvocationID.x;
    c[i] = sqrt(a[i]*a[i] + b[i]*b[i]); // norme 2D
})";

    NkComputeBufferDesc bd{};
    bd.cpuWritable=true; bd.cpuReadable=false;
    bd.sizeBytes=N*4; bd.initialData=A.Data(); auto bufA=compute->CreateBuffer(bd);
    bd.sizeBytes=N*4; bd.initialData=B.Data(); auto bufB=compute->CreateBuffer(bd);
    bd.sizeBytes=N*4; bd.initialData=nullptr; bd.cpuReadable=true; auto bufC=compute->CreateBuffer(bd);

    if (!bufA.valid||!bufB.valid||!bufC.valid) {
        logger.Errorf("[Compute] Buffer creation failed");
    } else {
        auto shader = compute->CreateShaderFromSource(glsl, "main");
        auto pipe   = compute->CreatePipeline(shader);

        compute->BindPipeline(pipe);
        compute->BindBuffer(0,bufA);
        compute->BindBuffer(1,bufB);
        compute->BindBuffer(2,bufC);
        compute->Dispatch(N/256,1,1);
        compute->MemoryBarrier();
        compute->WaitIdle();
        compute->ReadBuffer(bufC, C.Data(), N*4);

        // CPU verification
        bool ok=true; uint32 errs=0;
        for (uint32 i=0;i<N;i++) {
            float expected=sqrtf(A[i]*A[i]+B[i]*B[i]);
            if (fabsf(C[i]-expected)>0.01f) { ok=false; errs++; }
        }
        logger.Infof("\nGPU operation: c[i] = sqrt(a[i]^2 + b[i]^2)\n");
        logger.Infof("c[0]    = %.4f  (expected %.4f)\n", C[0],   sqrtf(A[0]*A[0]+B[0]*B[0]));
        logger.Infof("c[256]  = %.4f  (expected %.4f)\n", C[256], sqrtf(A[256]*A[256]+B[256]*B[256]));
        logger.Infof("c[1024] = %.4f  (expected %.4f)\n", C[1024],sqrtf(A[1024]*A[1024]+B[1024]*B[1024]));
        logger.Infof("Verification %u elements: %s (%u errors)\n\n",
               N, ok ? "PASS" : "FAIL", errs);

        compute->DestroyPipeline(pipe);
        compute->DestroyShader(shader);
    }
    compute->DestroyBuffer(bufA);
    compute->DestroyBuffer(bufB);
    compute->DestroyBuffer(bufC);
    compute->Shutdown(); delete compute;
    gfx->Shutdown();    delete gfx;
}

// =============================================================================
// ██████  DEMO AUTO — API auto-sélectionnée + dégradé générique
// =============================================================================
static void DemoAutoAPI(const NkEntryState&) {
    NkWindow win;
    if (!CreateDemoWindow(win, "NkEngine - Auto API (fallback chain)", 1280, 720)) {
        logger.Errorf("[Auto] Window creation failed");
        return;
    }

    const NkGraphicsApi fallbackOrder[] = {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        NkGraphicsApi::NK_API_DIRECTX12,
        NkGraphicsApi::NK_API_DIRECTX11,
#endif
#if defined(NKENTSEU_PLATFORM_MACOS)
        NkGraphicsApi::NK_API_METAL,
#endif
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE,
    };
    NkIGraphicsContext* ctx = NkContextFactory::CreateWithFallback(
        win,
        fallbackOrder,
        (uint32)(sizeof(fallbackOrder) / sizeof(fallbackOrder[0])));
    if (!ctx) { logger.Errorf("[Auto] Aucune API disponible!"); return; }

    logger.Infof("[Auto] API choisie : %s\n",    NkGraphicsApiName(ctx->GetApi()));
    logger.Infof("[Auto] Renderer    : %s\n",    ctx->GetInfo().renderer);
    logger.Infof("[Auto] Compute     : %s\n",    ctx->SupportsCompute() ? "oui" : "non");

    LoopCtx lc;
    RunLoop(win, ctx, lc, [&](float, float t) {
        // Si Software, dessiner un dégradé coloré
        if (ctx->GetApi() == NkGraphicsApi::NK_API_SOFTWARE) {
            auto* fb = NkNativeContext::GetSoftwareBackBuffer(ctx);
            if (fb && fb->IsValid()) {
                for (uint32 y=0;y<fb->height;y++) {
                    float fy=(float)y/fb->height;
                    for (uint32 x=0;x<fb->width;x++) {
                        float fx=(float)x/fb->width;
                        float h=fmodf(fx+fy*0.4f+t*0.06f,1.f);
                        float r,g,b; HsvToRgb(h,0.8f,0.85f,r,g,b);
                        fb->SetPixel(x,y,(uint8)(r*255),(uint8)(g*255),(uint8)(b*255));
                    }
                }
            }
        }
        // Autres APIs : le BeginFrame gère le clear
    }, 300);

    ctx->Shutdown(); delete ctx;
}

// =============================================================================
// Point d'entrée — nkmain (appelé par NkMetalEntryPoint.mm sur Apple,
//                           ou directement depuis main() sur Windows/Linux)
// =============================================================================
int nkmain(const NkEntryState& state) {
    logger.Infof("========================================\n");
    logger.Infof("    NkEngine - Graphics System Demos    \n");
    logger.Infof("========================================\n\n");
    logger.Infof("Demos disponibles (decommenter dans le code) :\n");
    logger.Infof("  DemoOpenGL    - Triangle pulsant + Quad UV rotatif (GLSL)\n");
    logger.Infof("  DemoVulkan    - Formes SDF via pipeline Vulkan\n");
    logger.Infof("  DemoDirectX11 - Triangle + Quad + Croix HLSL (Windows)\n");
    logger.Infof("  DemoDirectX12 - Formes SDF via pipeline DX12 (Windows)\n");
    logger.Infof("  DemoMetal     - Triangle + Cercle SDF MSL (macOS/iOS)\n");
    logger.Infof("  DemoSoftware  - Spirale HSV tracee pixel par pixel (CPU)\n");
    logger.Infof("  DemoCompute   - Norme 2D GPU, verification resultat\n");
    logger.Infof("  DemoAutoAPI   - Fallback chain automatique\n\n");

    auto equalsIgnoreCase = [](const char* a, const char* b) -> bool {
        if (!a || !b) return false;
        while (*a && *b) {
            const unsigned char ca = static_cast<unsigned char>(*a++);
            const unsigned char cb = static_cast<unsigned char>(*b++);
            if (std::tolower(ca) != std::tolower(cb)) return false;
        }
        return *a == '\0' && *b == '\0';
    };

    auto parseDemoFromArg = [&](const char* arg) -> const char* {
        if (!arg || !*arg) return nullptr;
        static const char* kKnown[] = {
            "opengl", "opengles", "webgl", "vulkan", "dx11", "dx12", "metal", "software", "compute", "auto"
        };
        for (const char* k : kKnown) {
            if (equalsIgnoreCase(arg, k)) return k;
        }
        return nullptr;
    };

    const char* demo = nullptr;
    const auto& args = state.GetArgs();
    for (nk_size i = 0; i < args.Size(); ++i) {
        const char* a = args[i].CStr();
        if (!a || !*a) continue;

        static constexpr const char* kDemoEq = "--demo=";
        static constexpr const char* kApiEq  = "--api=";

        if (std::strncmp(a, kDemoEq, std::strlen(kDemoEq)) == 0) {
            const char* parsed = parseDemoFromArg(a + std::strlen(kDemoEq));
            if (parsed) demo = parsed;
            continue;
        }
        if (std::strncmp(a, kApiEq, std::strlen(kApiEq)) == 0) {
            const char* parsed = parseDemoFromArg(a + std::strlen(kApiEq));
            if (parsed) demo = parsed;
            continue;
        }
        if ((equalsIgnoreCase(a, "--demo") || equalsIgnoreCase(a, "--api")) && (i + 1) < args.Size()) {
            const char* parsed = parseDemoFromArg(args[i + 1].CStr());
            if (parsed) {
                demo = parsed;
                ++i;
            }
            continue;
        }
        const char* parsed = parseDemoFromArg(a);
        if (parsed) demo = parsed;
    }

    if (!demo) {
        const char* envDemo = std::getenv("NK_DEMO");
        demo = parseDemoFromArg(envDemo);
    }
    if (!demo) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_PLATFORM_LINUX)
        demo = "opengl";
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
        demo = "dx11";
#else
        demo = "auto";
#endif
    }

    logger.Infof("[NewGeneration] demo=%s", demo);

    if (std::strcmp(demo, "opengl") == 0 || std::strcmp(demo, "opengles") == 0 || std::strcmp(demo, "webgl") == 0) {
        if (!DemoOpenGL(state)) {
            logger.Warnf("[NewGeneration] OpenGL demo failed, fallback to software.");
            DemoSoftware(state);
        }
    } else if (std::strcmp(demo, "software") == 0) {
        DemoSoftware(state);
    } else if (std::strcmp(demo, "compute") == 0) {
        DemoCompute(state);
    } else if (std::strcmp(demo, "vulkan") == 0) {
        DemoVulkan(state);
    } else if (std::strcmp(demo, "auto") == 0) {
        DemoAutoAPI(state);
    }
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    else if (std::strcmp(demo, "dx11") == 0) {
        DemoDirectX11(state);
    } else if (std::strcmp(demo, "dx12") == 0) {
        DemoDirectX12(state);
    }
#endif
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    else if (std::strcmp(demo, "metal") == 0) {
        DemoMetal(state);
    }
#endif
    else {
        logger.Warnf("[NewGeneration] Demo '%s' non supportee ici, fallback opengl.", demo);
        DemoOpenGL(state);
    }

    logger.Infof("\n[Demo] All demos completed.\n");
    return 0;
}