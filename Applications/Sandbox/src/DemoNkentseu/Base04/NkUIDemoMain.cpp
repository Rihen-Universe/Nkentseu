/**
 * @File    NkUIDemoMain.cpp
 * @Brief   Point d'entrée de la démo NkUI complète.
 *          Utilise NkUIDemo (header-only) + NkUICPURenderer offline.
 *
 * Compilation (OpenGL disabled, CPU renderer) :
 *   g++ -std=c++17 -O2 NkUIDemoMain.cpp -o NkUIDemo \
 *       -I../src -I../pch
 *
 * Avec OpenGL :
 *   g++ -std=c++17 -O2 -DNKUI_WITH_OPENGL NkUIDemoMain.cpp -o NkUIDemo \
 *       -I../src -lGL -ldl
 *
 * Avec NKRHI (viewport GPU) :
 *   g++ -std=c++17 -O2 -DNKUIDEMO_WITH_RHI NkUIDemoMain.cpp -o NkUIDemo \
 *       -I../src -I../../NKRHI/src -lNKRHI -lGL
 */

// ─────────────────────────────────────────────────────────────────────────────
//  En-têtes NkUI
// ─────────────────────────────────────────────────────────────────────────────
#include "NkUI/NkUI.h"
#include "NkUI/NkUILayout2.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Démo (header-only)
// ─────────────────────────────────────────────────────────────────────────────
#include "NkUIDemo.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Plateforme / Fenêtrage
//  → Si vous avez NKWindow, remplacez les blocs #ifdef SDL2 / #ifdef GLFW
// ─────────────────────────────────────────────────────────────────────────────
#ifdef NKUIDEMO_USE_SDL2
#  include <SDL2/SDL.h>
#  include <SDL2/SDL_opengl.h>
#elif defined(NKUIDEMO_USE_GLFW)
#  include <GLFW/glfw3.h>
#else
//  Mode offline pur (pas de fenêtre, sauvegarde PPM)
#  define NKUIDEMO_OFFLINE
#endif

// RHI optionnel
#ifdef NKUIDEMO_WITH_RHI
#  include "NKRHI/Core/NkIDevice.h"
#  include "NKRHI/Core/NkDeviceFactory.h"
#  include "NKRHI/Commands/NkICommandBuffer.h"
#endif

#include <cstdio>
#include <cstring>
#include <cmath>

using namespace nkentseu;

// =============================================================================
// Config de la démo
// =============================================================================
static constexpr int32 kWidth  = 1600;
static constexpr int32 kHeight = 900;
static constexpr float32 kTargetFPS = 60.f;
static constexpr float32 kTargetDT  = 1.f / kTargetFPS;

// =============================================================================
// Helpers input → NkUIInputState
// =============================================================================
#ifdef NKUIDEMO_USE_SDL2
static NkKey SDLKeyToNk(SDL_Keycode k) {
    switch(k) {
        case SDLK_TAB:       return NkKey::NK_TAB;
        case SDLK_RETURN:    return NkKey::NK_ENTER;
        case SDLK_ESCAPE:    return NkKey::NK_ESCAPE;
        case SDLK_BACKSPACE: return NkKey::NK_BACKSPACE;
        case SDLK_DELETE:    return NkKey::NK_DELETE;
        case SDLK_LEFT:      return NkKey::NK_LEFT;
        case SDLK_RIGHT:     return NkKey::NK_RIGHT;
        case SDLK_UP:        return NkKey::NK_UP;
        case SDLK_DOWN:      return NkKey::NK_DOWN;
        case SDLK_HOME:      return NkKey::NK_HOME;
        case SDLK_END:       return NkKey::NK_END;
        case SDLK_PAGEUP:    return NkKey::NK_PAGE_UP;
        case SDLK_PAGEDOWN:  return NkKey::NK_PAGE_DOWN;
        case SDLK_INSERT:    return NkKey::NK_INSERT;
        case SDLK_a: return NkKey::NK_A; case SDLK_b: return NkKey::NK_B;
        case SDLK_c: return NkKey::NK_C; case SDLK_d: return NkKey::NK_D;
        case SDLK_e: return NkKey::NK_E; case SDLK_f: return NkKey::NK_F;
        case SDLK_g: return NkKey::NK_G; case SDLK_h: return NkKey::NK_H;
        case SDLK_i: return NkKey::NK_I; case SDLK_j: return NkKey::NK_J;
        case SDLK_k: return NkKey::NK_K; case SDLK_l: return NkKey::NK_L;
        case SDLK_m: return NkKey::NK_M; case SDLK_n: return NkKey::NK_N;
        case SDLK_o: return NkKey::NK_O; case SDLK_p: return NkKey::NK_P;
        case SDLK_q: return NkKey::NK_Q; case SDLK_r: return NkKey::NK_R;
        case SDLK_s: return NkKey::NK_S; case SDLK_t: return NkKey::NK_T;
        case SDLK_u: return NkKey::NK_U; case SDLK_v: return NkKey::NK_V;
        case SDLK_w: return NkKey::NK_W; case SDLK_x: return NkKey::NK_X;
        case SDLK_y: return NkKey::NK_Y; case SDLK_z: return NkKey::NK_Z;
        case SDLK_LCTRL: case SDLK_RCTRL: return NkKey::NK_CTRL;
        case SDLK_LSHIFT:case SDLK_RSHIFT:return NkKey::NK_SHIFT;
        case SDLK_LALT:  case SDLK_RALT:  return NkKey::NK_ALT;
        default: return NkKey::NK_NONE;
    }
}
#endif // NKUIDEMO_USE_SDL2

// =============================================================================
// Application
// =============================================================================
struct NkUIDemoApp {
    // ── NkUI core ─────────────────────────────────────────────────────────
    NkUIContext       ctx;
    NkUIWindowManager wm;
    NkUIDockManager   dock;
    NkUILayoutStack   ls;
    NkUIFontManager   fonts;
    NkUICPURenderer   renderer;

    // ── Démo ──────────────────────────────────────────────────────────────
    NkUIDemo          demo;

    // ── Plateforme ────────────────────────────────────────────────────────
#ifdef NKUIDEMO_USE_SDL2
    SDL_Window*   window  = nullptr;
    SDL_GLContext glCtx   = nullptr;
#elif defined(NKUIDEMO_USE_GLFW)
    GLFWwindow*   window  = nullptr;
#endif

    bool  running  = false;
    int32 winW = kWidth, winH = kHeight;

    // ── Init ──────────────────────────────────────────────────────────────
    bool Init() {
        // NkUI
        if (!ctx.Init(kWidth, kHeight)) {
            ::printf("[NkUIDemo] ctx.Init failed\n");
            return false;
        }
        fonts.Init();
        wm.Init();
        dock.Init({0, 30, (float32)kWidth, (float32)kHeight - 30});
        renderer.Init(kWidth, kHeight);

        demo.Init(ctx, wm, fonts);

        // Fenêtrage
#ifdef NKUIDEMO_USE_SDL2
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        window = SDL_CreateWindow("NkUI Demo",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   kWidth, kHeight,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window) {
            ::printf("[NkUIDemo] SDL window creation failed: %s\n", SDL_GetError());
            return false;
        }
        glCtx = SDL_GL_CreateContext(window);
        SDL_GL_SetSwapInterval(1);
        ::printf("[NkUIDemo] SDL2 + OpenGL context created.\n");

#elif defined(NKUIDEMO_USE_GLFW)
        if (!glfwInit()) {
            ::printf("[NkUIDemo] GLFW init failed\n");
            return false;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(kWidth, kHeight, "NkUI Demo", nullptr, nullptr);
        if (!window) {
            ::printf("[NkUIDemo] GLFW window creation failed\n");
            return false;
        }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
        ::printf("[NkUIDemo] GLFW + OpenGL context created.\n");
#else
        ::printf("[NkUIDemo] Mode offline — rendu PPM.\n");
#endif
        running = true;
        return true;
    }

    // ── Main loop ─────────────────────────────────────────────────────────
    void Run() {
        NkUIInputState input;
        float32 dt = kTargetDT;
        float32 totalTime = 0.f;
        int32   frameCount= 0;

#ifdef NKUIDEMO_OFFLINE
        // Mode offline : 5 frames puis sauvegarde
        for (int32 frame = 0; frame < 5 && running; ++frame) {
            RunFrame(input, dt);
            dt = kTargetDT;
            ++frameCount;
        }
        SavePPM("nkui_demo_output.ppm");
        ::printf("[NkUIDemo] Offline render done. Output: nkui_demo_output.ppm\n");
        return;
#endif

#ifdef NKUIDEMO_USE_SDL2
        Uint64 prevTick = SDL_GetPerformanceCounter();
        const Uint64 freq = SDL_GetPerformanceFrequency();

        while (running) {
            // ── Événements SDL ─────────────────────────────────────────
            input.BeginFrame();
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_QUIT:
                        running = false;
                        break;
                    case SDL_MOUSEMOTION:
                        input.SetMousePos((float32)e.motion.x, (float32)e.motion.y);
                        break;
                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_MOUSEBUTTONUP: {
                        int32 btn = 0;
                        if (e.button.button == SDL_BUTTON_LEFT)   btn=0;
                        else if (e.button.button == SDL_BUTTON_RIGHT)  btn=1;
                        else if (e.button.button == SDL_BUTTON_MIDDLE) btn=2;
                        input.SetMouseButton(btn, e.type == SDL_MOUSEBUTTONDOWN);
                        // Double-clic
                        if (e.button.clicks == 2)
                            input.mouseDblClick[btn] = true;
                        break;
                    }
                    case SDL_MOUSEWHEEL:
                        input.AddMouseWheel((float32)e.wheel.y, (float32)e.wheel.x);
                        break;
                    case SDL_KEYDOWN:
                    case SDL_KEYUP: {
                        NkKey nk = SDLKeyToNk(e.key.keysym.sym);
                        if (nk != NkKey::NK_NONE)
                            input.SetKey(nk, e.type == SDL_KEYDOWN);
                        break;
                    }
                    case SDL_TEXTINPUT:
                        for (int32 i=0; e.text.text[i]&&i<32; ++i)
                            input.AddInputChar((uint32)e.text.text[i]);
                        break;
                    case SDL_WINDOWEVENT:
                        if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                            winW = e.window.data1;
                            winH = e.window.data2;
                            ctx.viewW = winW;
                            ctx.viewH = winH;
                            renderer.BeginFrame(winW, winH);
                        }
                        break;
                }
            }
            input.dt = dt;

            // ── Frame ─────────────────────────────────────────────────
            RunFrame(input, dt);

            // ── Present ───────────────────────────────────────────────
            // Ici on utilise le CPU renderer → copier les pixels dans
            // une texture OpenGL et blitter, OU utiliser NkUIOpenGLRenderer
            // Pour la démo on utilise glDrawPixels (compatibilité)
            // Dans un vrai projet : NkUIOpenGLRenderer::Submit(ctx)
            SDL_GL_SwapWindow(window);

            // ── Timing ────────────────────────────────────────────────
            const Uint64 now = SDL_GetPerformanceCounter();
            dt = (float32)(now - prevTick) / (float32)freq;
            prevTick = now;
            if (dt > 0.1f) dt = 0.1f;

            ++frameCount;
            totalTime += dt;
            if (totalTime > 5.f) {
                ::printf("[NkUIDemo] Avg FPS: %.1f\n", frameCount/totalTime);
                frameCount = 0; totalTime = 0.f;
            }
        }

#elif defined(NKUIDEMO_USE_GLFW)
        double prevTime = glfwGetTime();

        while (running && !glfwWindowShouldClose(window)) {
            glfwPollEvents();

            input.BeginFrame();
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            input.SetMousePos((float32)mx, (float32)my);
            for (int32 b = 0; b < 3; ++b) {
                bool down = glfwGetMouseButton(window, b) == GLFW_PRESS;
                input.SetMouseButton(b, down);
            }
            input.dt = dt;

            RunFrame(input, dt);

            glfwSwapBuffers(window);

            double now = glfwGetTime();
            dt = (float32)(now - prevTime);
            prevTime = now;
            if (dt > 0.1f) dt = 0.1f;

            ++frameCount;
            totalTime += dt;
            if (totalTime > 5.f) {
                ::printf("[NkUIDemo] Avg FPS: %.1f\n", frameCount/totalTime);
                frameCount = 0; totalTime = 0.f;
            }
        }
#endif
    }

    // ── Une frame ─────────────────────────────────────────────────────────
    void RunFrame(const NkUIInputState& input, float32 dt) {
        // === BeginFrame ===
        ctx.BeginFrame(input, dt);
        wm.BeginFrame(ctx);

        // === Rendu de la démo ===
        demo.Render(ctx, wm, dock, ls, dt);

        // === EndFrame ===
        ctx.EndFrame();
        wm.EndFrame(ctx);

        // === Rendu CPU ===
#ifdef NKUI_WITH_OPENGL
        // Backend OpenGL (nécessite NKUI_WITH_OPENGL)
        // NkUIOpenGLRenderer glr; (initialisé dans Init)
        // glr.BeginFrame(winW, winH);
        // glr.Submit(ctx);
        // glr.EndFrame();
#else
        // Backend CPU
        renderer.BeginFrame(winW, winH);
        renderer.Submit(ctx);
        renderer.EndFrame();
#endif
    }

    // ── Save PPM (offline) ────────────────────────────────────────────────
    void SavePPM(const char* path) {
        renderer.SavePNG(path);
        // La méthode SavePNG() écrit en réalité un PPM en fallback
        // si NKImage n'est pas disponible.
    }

    // ── Destroy ───────────────────────────────────────────────────────────
    void Destroy() {
        demo.Destroy();
        renderer.Destroy();
        ctx.Destroy();
        wm.Destroy();
        dock.Destroy();
        fonts.Destroy();

#ifdef NKUIDEMO_USE_SDL2
        if (glCtx)  SDL_GL_DeleteContext(glCtx);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
#elif defined(NKUIDEMO_USE_GLFW)
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
#endif
    }
};

// =============================================================================
// main
// =============================================================================
int main(int, char**) {
    ::printf("=== NkUI Demo v1.0 ===\n");
    ::printf("Backend: CPU Renderer (offline)\n");
    ::printf("Features: Widgets, Charts, 3D Viewport, Textures,\n");
    ::printf("          ColorPicker, Style Editor, Console,\n");
    ::printf("          Node Graph, Table, PerfMonitor, Scene3D\n");
    ::printf("Press ESC or close window to exit.\n\n");

    NkUIDemoApp app;
    if (!app.Init()) {
        ::printf("[NkUIDemo] Init failed.\n");
        return 1;
    }

    app.Run();
    app.Destroy();

    ::printf("[NkUIDemo] Done.\n");
    return 0;
}

// =============================================================================
// ============== EXEMPLE D'INTÉGRATION AVEC NKRHI =============================
// =============================================================================
/*

// Exemple d'utilisation avec NKRHI (NkIDevice) — remplace le CPU renderer
// par le rendu GPU avec NkUIOpenGLRenderer ou un backend custom.

// Dans votre boucle principale avec NkIDevice existant :
//
// void MyApp::IntegrateNkUI(NkIDevice* device, NkICommandBuffer* cmd) {
//
//     // 1. Init (une seule fois)
//     NkUIContext ctx;
//     ctx.Init(device->GetSwapchainWidth(), device->GetSwapchainHeight());
//     ctx.SetTheme(NkUITheme::Dark());
//
//     NkUIFontManager fonts; fonts.Init();
//     NkUIWindowManager wm;  wm.Init();
//     NkUIDockManager dock;
//     dock.Init({0, 30, (float32)ctx.viewW, (float32)ctx.viewH - 30});
//     NkUILayoutStack ls;
//     NkUIAnimator animator;
//
//     NkUIDemo demo;
//     demo.Init(ctx, wm, fonts);
//
//     // Upload texture atlas font si besoin
//     // NkUIFontAtlas* atlas = fonts.Default()->atlas;
//     // if (atlas && atlas->dirty) {
//     //     atlas->texId = device->UploadTextureGray8(atlas->pixels, 512, 512);
//     //     atlas->dirty = false;
//     // }
//
//     // 2. Par frame
//     NkUIInputState input = BuildInputFromPlatform(); // depuis SDL/GLFW/Win32
//     ctx.BeginFrame(input, dt);
//     wm.BeginFrame(ctx);
//
//     demo.Render(ctx, wm, dock, ls, dt);
//
//     ctx.EndFrame();
//     wm.EndFrame(ctx);
//
//     // 3. Soumission GPU
//     // Option A : NkUIOpenGLRenderer (nécessite -DNKUI_WITH_OPENGL)
//     // static NkUIOpenGLRenderer glr;
//     // if (!glr.mInitialized) glr.Init(ctx.viewW, ctx.viewH);
//     // glr.BeginFrame(ctx.viewW, ctx.viewH);
//     // glr.Submit(ctx);
//     // glr.EndFrame();
//
//     // Option B : Custom — itère les DrawLists et soumet via NkIDevice
//     // for (int i = 0; i < NkUIContext::LAYER_COUNT; ++i) {
//     //     const NkUIDrawList& dl = ctx.layers[i];
//     //     // Upload vertex/index buffers
//     //     device->WriteBuffer(hVBO, dl.vtx, dl.vtxCount * sizeof(NkUIVertex));
//     //     device->WriteBuffer(hIBO, dl.idx, dl.idxCount * sizeof(uint32));
//     //     // Pour chaque DrawCmd :
//     //     for (uint32 ci = 0; ci < dl.cmdCount; ++ci) {
//     //         const NkUIDrawCmd& dc = dl.cmds[ci];
//     //         if (dc.type == NkUIDrawCmdType::NK_CLIP_RECT) {
//     //             cmd->SetScissor({(int)dc.clipRect.x, (int)dc.clipRect.y,
//     //                              (int)dc.clipRect.w, (int)dc.clipRect.h});
//     //         } else {
//     //             cmd->BindDescriptorSets(hDesc_with_texture[dc.texId]);
//     //             cmd->DrawIndexed(dc.idxCount, 1, dc.idxOffset, 0, 0);
//     //         }
//     //     }
//     // }
// }

=============================================================================

// ===================== SHADER GLSL POUR NkUI ================================
//
// Vertex shader GLSL (OpenGL 3.3+) :
//
//   #version 330 core
//   layout(location=0) in vec2 aPos;
//   layout(location=1) in vec2 aUV;
//   layout(location=2) in vec4 aColor;  // ou uint aColorPacked;
//   uniform vec2 uViewport;
//   out vec2 vUV;
//   out vec4 vColor;
//   void main() {
//       vUV    = aUV;
//       vColor = aColor;
//       vec2 ndc = (aPos / uViewport) * 2.0 - 1.0;
//       gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
//   }
//
// Fragment shader :
//
//   #version 330 core
//   in  vec2 vUV;
//   in  vec4 vColor;
//   out vec4 fragColor;
//   uniform sampler2D uTex;
//   void main() {
//       vec4 tc = (vUV.x > 0.0 || vUV.y > 0.0)
//               ? texture(uTex, vUV)
//               : vec4(1.0);
//       fragColor = vColor * tc;
//   }
//
// Layout vertex (NkUIVertex) :
//   Location 0 : NkVec2 pos   (float32 x 2)
//   Location 1 : NkVec2 uv    (float32 x 2)
//   Location 2 : uint32 col   (RGBA packed → décompresser en vec4 dans le VS)
//
// Décompression de la couleur RGBA packed dans GLSL :
//   vec4 unpackColor(uint c) {
//       return vec4(
//           float((c >> 0u)  & 0xFFu) / 255.0,
//           float((c >> 8u)  & 0xFFu) / 255.0,
//           float((c >> 16u) & 0xFFu) / 255.0,
//           float((c >> 24u) & 0xFFu) / 255.0
//       );
//   }

=============================================================================

// ===================== SHADER HLSL POUR NkUI ================================
//
//   cbuffer Constants : register(b0) {
//       float2 uViewport;
//   };
//   Texture2D uTex : register(t0);
//   SamplerState uSampler : register(s0);
//
//   struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
//   struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
//
//   VSOut VSMain(VSIn v) {
//       VSOut o;
//       float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
//       o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
//       o.uv = v.uv;
//       o.col = v.col;
//       return o;
//   }
//
//   float4 PSMain(VSOut i) : SV_Target {
//       float4 tc = (i.uv.x > 0 || i.uv.y > 0)
//                   ? uTex.Sample(uSampler, i.uv)
//                   : float4(1,1,1,1);
//       return i.col * tc;
//   }

*/