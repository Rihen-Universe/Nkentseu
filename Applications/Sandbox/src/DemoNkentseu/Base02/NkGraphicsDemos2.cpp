// =============================================================================
// NkGraphicsDemos.cpp
// Démos complètes pour chaque API graphique.
// Reprend exactement le pattern de ton nkmain / NkEntryState.
//
// Chaque démo est une fonction autonome appelable depuis nkmain.
// =============================================================================

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
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKContext/NkContextFactory.h"
#include "NKContext/NkContextDesc.h"
#include "NKContext/NkNativeContextAccess.h"
#include "NKContext/Backends/OpenGL/NkOpenGLDesc.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {
    struct NkEntryState;
}

namespace nkentseu {

// =============================================================================
// Utilitaires communs aux démos
// =============================================================================
static NkWindowConfig MakeWindowConfig(const char* title,
                                       uint32 w = 1280, uint32 h = 720) {
    NkWindowConfig cfg;
    cfg.title       = title;
    cfg.width       = w;
    cfg.height      = h;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = false;
    return cfg;
}

// Boucle principale commune — retourne false si la fenêtre est fermée
static bool RunLoop(NkWindow& window, NkIGraphicsContext* ctx,
                    std::function<void(NkIGraphicsContext*, float dt)> onRender)
{
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    NkChrono chrono;
    auto& events = NkEvents();
    bool shouldQuit = false;

    while (window.IsOpen()) {
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                shouldQuit = true;
                break;
            }
        }
        if (shouldQuit || !window.IsOpen()) {
            break;
        }

        const NkElapsedTime elapsed = chrono.Reset();
        float dt = static_cast<float>(elapsed.seconds);
        if (dt <= 0.f || dt > 0.25f) {
            dt = 1.f / 60.f;
        }

        if (ctx->BeginFrame()) {
            onRender(ctx, dt);
            ctx->EndFrame();
            ctx->Present();
        }

        // Pattern web utilisé dans les autres mains du sandbox:
        // cède la main au navigateur à chaque frame.
        emscripten_sleep(0);
    }
    return true;
#else
    NkChrono chrono;
    NkElapsedTime elapsed;
    auto& events = NkEvents();
    uint32 frameCount = 0;
    const uint32 maxFrames = 36000; // Safety guard if no OS event pump is attached.
    bool shouldQuit = false;

    while (window.IsOpen() && frameCount < maxFrames) {
        // Pump + consume OS events so WM_CLOSE and input are handled.
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                shouldQuit = true;
                break;
            }
        }
        if (shouldQuit || !window.IsOpen()) break;

        elapsed = chrono.Reset();
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;

        if (ctx->BeginFrame()) {
            onRender(ctx, dt);
            ctx->EndFrame();
            ctx->Present();
        }
        ++frameCount;

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
    return true;
#endif
}

// =============================================================================
// DÉMO 1 — OpenGL 4.6 Core (triangle RGB)
// =============================================================================
//
// Installation de GLAD2 :
//   1. Aller sur https://gen.glad.dav1d.de/
//   2. API : gl, Version : 4.6, Spec : gl
//   3. Extensions : WGL_ARB_create_context, WGL_ARB_pixel_format,
//                   WGL_EXT_swap_control, WGL_ARB_framebuffer_sRGB,
//                   WGL_ARB_multisample, GLX_ARB_create_context, etc.
//   4. Generator : C/C++, Loader : On
//   5. Télécharger et extraire dans extern/glad2/
//   6. CMakeLists.txt :
//        add_subdirectory(extern/glad2)
//        target_link_libraries(MyApp PRIVATE glad)
//   7. Dans le code : #include <glad/gl.h> et #include <glad/wgl.h> (Windows)
//
int DemoOpenGL(const NkEntryState& /*state*/) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — WebGL2 Demo");
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — OpenGL ES Demo");
#else
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — OpenGL 4.6 Demo");
#endif

    // ── Configurer le contexte OpenGL ────────────────────────────────────────
    NkContextDesc desc;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    desc.api = NkGraphicsApi::NK_GFX_API_WEBGL;
    desc.opengl.majorVersion    = 3; // WebGL2 ~ GLES 3.0
    desc.opengl.minorVersion    = 0;
    desc.opengl.profile         = NkGLProfile::ES;
    desc.opengl.contextFlags    = NkGLContextFlags::NoneFlag;
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
    desc = NkContextDesc::MakeOpenGLES(3, 0);
#else
    desc.api    = NkGraphicsApi::NK_GFX_API_OPENGL;
    desc.opengl = NkOpenGLDesc::Desktop46(/*debug=*/true);
#endif
    desc.opengl.msaaSamples      = 4;
    desc.opengl.srgbFramebuffer  = true;
    desc.opengl.swapInterval     = NkGLSwapInterval::AdaptiveVSync;

    // Personnaliser le PIXELFORMATDESCRIPTOR bootstrap (optionnel)
    // desc.opengl.wglFallback = NkWGLFallbackPixelFormat::Standard(); // déjà par défaut
    // desc.opengl.wglFallback = NkWGLFallbackPixelFormat::Minimal();  // driver ancien

    // GLAD2 : installer le debug callback automatiquement
    desc.opengl.runtime.autoLoadEntryPoints = true;
    desc.opengl.runtime.validateVersion     = true;
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    // WebGL2 n'expose pas GL_KHR_debug comme un desktop GL debug context.
    desc.opengl.runtime.installDebugCallback = false;
#else
    desc.opengl.runtime.installDebugCallback = true;
#endif
    desc.opengl.runtime.debugSeverityLevel  = 2; // medium+

    // CRITIQUE sur Linux XLib/XCB : injecter les hints GLX AVANT Create()
    NkContextFactory::PrepareWindowConfig(desc, cfg);

    NkWindow window(cfg);
    if (!window.IsOpen()) { printf("[DemoGL] Window failed\n"); return -1; }

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) { printf("[DemoGL] Context failed\n"); return -2; }

    // ── Données GPU : triangle RGB ────────────────────────────────────────────
    static const float verts[] = {
        // position (xy)   couleur (rgb)
         0.0f,  0.5f,      1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,      0.0f, 1.0f, 0.0f,
         0.5f, -0.5f,      0.0f, 0.0f, 1.0f,
    };

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
    static const char* vsSource = R"(
        #version 300 es
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec3 aColor;
        out vec3 vColor;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vColor = aColor;
        }
    )";
    static const char* fsSource = R"(
        #version 300 es
        precision mediump float;
        in vec3 vColor;
        out vec4 fragColor;
        void main() { fragColor = vec4(vColor, 1.0); }
    )";
#else
    static const char* vsSource = R"(
        #version 460 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec3 aColor;
        out vec3 vColor;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vColor = aColor;
        }
    )";
    static const char* fsSource = R"(
        #version 460 core
        in vec3 vColor;
        out vec4 fragColor;
        void main() { fragColor = vec4(vColor, 1.0); }
    )";
#endif

    // Compiler shaders
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
            printf("[DemoGL][Shader error] %s\n", log);
        }
        return s;
    };

    GLuint vs  = compile(GL_VERTEX_SHADER,   vsSource);
    GLuint fs  = compile(GL_FRAGMENT_SHADER, fsSource);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float),
                          (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    NkContextInfo info = ctx->GetInfo();
    printf("[DemoGL] Running on: %s | %s\n", info.renderer, info.version);

    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float) {
        const auto surface = window.GetSurfaceDesc();
        glViewport(0, 0, surface.width, surface.height);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    });

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    ctx->Shutdown();
    window.Close();
    return 0;
}

// =============================================================================
// DÉMO 2 — Vulkan (clear dynamique)
// =============================================================================
//
// Installation de Vulkan SDK :
//   Windows  : https://vulkan.lunarg.com/sdk/home → installer le SDK
//              Variables d'env : VULKAN_SDK auto-configurées
//   Linux    : sudo apt install libvulkan-dev vulkan-tools
//              ou via le SDK LunarG
//   macOS    : SDK LunarG + MoltenVK (inclus dans le SDK)
//              brew install molten-vk  (alternative)
//   Android  : inclus dans le NDK 25+
//   CMake    : find_package(Vulkan REQUIRED)
//              target_link_libraries(App PRIVATE Vulkan::Vulkan)
//
int DemoVulkan(const NkEntryState& /*state*/) {
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
    printf("[DemoVK] Vulkan is not supported on Web/Emscripten. Use --demo=opengl or --demo=software.\n");
    return -3;
#endif

    NkWindowConfig cfg = MakeWindowConfig("NkEngine — Vulkan Demo");
    NkContextDesc desc = NkContextDesc::MakeVulkan(/*validation=*/true);
    desc.vulkan.swapchainImages = 3;
    desc.vulkan.vsync           = true;

    NkContextFactory::PrepareWindowConfig(desc, cfg); // no-op pour Vulkan
    NkWindow window(cfg);
    if (!window.IsOpen()) return -1;

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    // Accès typé aux données Vulkan via nativecontext
#if NKENTSEU_HAS_VULKAN_HEADERS
    VkDevice         device     = nativecontext::GetVkDevice(ctx.get());
    VkRenderPass     renderPass = nativecontext::GetVkRenderPass(ctx.get());
    printf("[DemoVK] Device: %s\n", ctx->GetInfo().renderer);
    printf("[DemoVK] RenderPass: %p\n", (void*)renderPass);
#else
    printf("[DemoVK] Vulkan headers unavailable at compile time.\n");
#endif

    float hue = 0.0f;
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float dt) {
        // Le BeginFrame / EndFrame / Present gère toute la synchronisation.
        // À l'intérieur de BeginFrame → EndFrame, le render pass est actif.
        // On peut récupérer le command buffer pour des draw calls additionnels.
    #if NKENTSEU_HAS_VULKAN_HEADERS
        VkCommandBuffer cmd = nativecontext::GetVkCurrentCommandBuffer(ctx.get());
        (void)cmd;
    #endif

        // Ici on ferait : vkCmdBindPipeline, vkCmdDraw, etc.
        // Pour la démo : clear couleur animée via le render pass (déjà dans BeginFrame)
        hue = fmodf(hue + dt * 60.0f, 360.0f);
    });

    ctx->Shutdown();
    window.Close();
    return 0;
}

// =============================================================================
// DÉMO 3 — DirectX 11
// =============================================================================
//
// Installation :
//   DirectX 11 est inclus dans le SDK Windows (Windows SDK).
//   Il n'y a rien à installer séparément sur Windows 7+.
//   CMake :
//     target_link_libraries(App PRIVATE d3d11 dxgi d3dcompiler)
//   Requis : Visual Studio ou Windows SDK (10.0.22621 recommandé)
//   Outils de debug : RenderDoc, PIX for Windows
//
#if defined(NKENTSEU_PLATFORM_WINDOWS)
int DemoDirectX11(const NkEntryState& /*state*/) {
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — DirectX 11 Demo");
    NkContextDesc desc = NkContextDesc::MakeDirectX11(/*debug=*/true);
    desc.dx11.swapchainBuffers = 2;
    desc.dx11.allowTearing     = true;

    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);
    if (!window.IsOpen()) return -1;

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    // Accès typé — pas de cast void* à écrire
    ID3D11Device1*        device  = nativecontext::GetDX11Device(ctx.get());
    ID3D11DeviceContext1* devCtx  = nativecontext::GetDX11Context(ctx.get());

    // ── Triangle DX11 ─────────────────────────────────────────────────────────
    static const char* hlslSrc = R"(
        struct VS_OUT { float4 pos : SV_POSITION; float3 col : COLOR; };
        VS_OUT VS(float2 pos : POSITION, float3 col : COLOR) {
            VS_OUT o; o.pos = float4(pos, 0, 1); o.col = col; return o;
        }
        float4 PS(VS_OUT i) : SV_TARGET { return float4(i.col, 1); }
    )";

    // Compiler les shaders
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    D3DCompile(hlslSrc, strlen(hlslSrc), nullptr, nullptr, nullptr,
               "VS", "vs_5_0", 0, 0, &vsBlob, &errBlob);
    D3DCompile(hlslSrc, strlen(hlslSrc), nullptr, nullptr, nullptr,
               "PS", "ps_5_0", 0, 0, &psBlob, &errBlob);

    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader>  ps;
    device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                vsBlob->GetBufferSize(), nullptr, &vs);
    device->CreatePixelShader(psBlob->GetBufferPointer(),
                               psBlob->GetBufferSize(), nullptr, &ps);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT,  0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ComPtr<ID3D11InputLayout> inputLayout;
    device->CreateInputLayout(layout, 2,
                               vsBlob->GetBufferPointer(),
                               vsBlob->GetBufferSize(), &inputLayout);

    float verts[] = {
         0.0f,  0.5f,   1,0,0,
        -0.5f, -0.5f,   0,1,0,
         0.5f, -0.5f,   0,0,1,
    };
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(verts);
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { verts };
    ComPtr<ID3D11Buffer> vbuf;
    device->CreateBuffer(&bd, &sd, &vbuf);

    printf("[DemoDX11] Running on: %s\n", ctx->GetInfo().renderer);

    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float) {
        UINT stride = 5 * sizeof(float), offset = 0;
        devCtx->IASetInputLayout(inputLayout.Get());
        devCtx->IASetVertexBuffers(0, 1, vbuf.GetAddressOf(), &stride, &offset);
        devCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        devCtx->VSSetShader(vs.Get(), nullptr, 0);
        devCtx->PSSetShader(ps.Get(), nullptr, 0);
        devCtx->Draw(3, 0);
    });

    ctx->Shutdown();
    window.Close();
    return 0;
}

// =============================================================================
// DÉMO 4 — DirectX 12
// =============================================================================
//
// Installation :
//   Idem DX11 — inclus dans le Windows SDK.
//   DX12 requiert Windows 10 minimum (build 1607+).
//   CMake : target_link_libraries(App PRIVATE d3d12 dxgi d3dcompiler dxguid)
//   Outils de debug : PIX for Windows (meilleur pour DX12), RenderDoc
//   Validation layer : activée via NkDirectX12Desc::debugDevice = true
//
int DemoDirectX12(const NkEntryState& /*state*/) {
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — DirectX 12 Demo");
    NkContextDesc desc = NkContextDesc::MakeDirectX12(/*debug=*/true);
    desc.dx12.swapchainBuffers   = 3;
    desc.dx12.allowTearing       = true;
    desc.dx12.gpuValidation      = false; // Très lent, activer seulement si bug

    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);
    if (!window.IsOpen()) return -1;

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    // Accès typé direct
    ID3D12Device5*              device    = nativecontext::GetDX12Device(ctx.get());
    ID3D12CommandQueue*         queue     = nativecontext::GetDX12CommandQueue(ctx.get());
    ID3D12GraphicsCommandList4* cmdList   = nativecontext::GetDX12CommandList(ctx.get());
    auto* dx12Data = nativecontext::DX12(ctx.get());

    printf("[DemoDX12] Running on: %s\n", ctx->GetInfo().renderer);
    printf("[DemoDX12] Swapchain: %d images\n", kNkDX12MaxFrames);

    // Pour DX12, la création de pipeline state objects (PSO) est explicite.
    // Cette démo fait juste un clear animé pour rester concise.
    // Un triangle nécessiterait : RootSignature + PSO + vertex buffer + shader blobs.

    float r = 0.1f, g = 0.1f, b = 0.3f;
    float dr = 0.3f;
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float dt) {
        // BeginFrame a déjà :
        //   - Reset cmdAllocator + cmdList
        //   - Transition PRESENT → RENDER_TARGET
        //   - Bind RTV + DSV
        //   - Clear + set viewport/scissor
        //
        // Ici on peut ajouter des draw calls sur cmdList.
        // EndFrame fait : transition RENDER_TARGET → PRESENT + Close + Execute
        r = fmodf(r + dr * dt, 1.0f);
        if (r >= 1.0f || r <= 0.0f) dr = -dr;
        // Pour changer la couleur de clear : modifier cmdList dans BeginFrame
        // (non exposé ici — à sous-classer ou utiliser une callback BeginFrame)
        (void)device; (void)queue; (void)cmdList; (void)dx12Data;
    });

    // Shutdown() du backend DX12 flush déjà le GPU avant destruction.
    ctx->Shutdown();
    window.Close();
    return 0;
}
#endif // NKENTSEU_PLATFORM_WINDOWS

// =============================================================================
// DÉMO 5 — Metal (macOS/iOS)
// =============================================================================
//
// Installation :
//   Metal est inclus dans macOS SDK / Xcode.
//   Aucune installation externe nécessaire.
//   CMake :
//     target_link_libraries(App PRIVATE "-framework Metal"
//                                       "-framework MetalKit"
//                                       "-framework QuartzCore")
//   Xcode : ajouter Metal.framework dans Build Phases > Link Binary With Libraries
//   Outils : Xcode GPU Frame Capture (intégré), RenderDoc (via Metal plugin)
//
#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
int DemoMetal(const NkEntryState& /*state*/) {
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — Metal Demo");
    NkContextDesc desc = NkContextDesc::MakeMetal();
    desc.metal.sampleCount = 1;
    desc.metal.vsync       = true;

    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);
    if (!window.IsOpen()) return -1;

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    // Accès typé aux données Metal
    void* mtlDevice = nativecontext::GetMetalDevice(ctx.get());
    printf("[DemoMetal] Running on: %s\n", ctx->GetInfo().renderer);
    printf("[DemoMetal] MTLDevice: %p\n", mtlDevice);

    // Le rendu Metal se fait via l'encoder récupéré dans currentEncoder.
    // Pour un triangle : créer MTLRenderPipelineDescriptor + MTLLibrary + draw.
    // Cette démo fait juste un clear animé (géré par BeginFrame).
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float) {
        void* encoder = nativecontext::GetMetalCommandEncoder(ctx.get());
        // id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)encoder;
        // [enc setRenderPipelineState:pipeline];
        // [enc setVertexBuffer:vbuf offset:0 atIndex:0];
        // [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        (void)encoder;
    });

    ctx->Shutdown();
    window.Close();
    return 0;
}
#endif

// =============================================================================
// DÉMO 6 — Software Renderer (dégradé animé)
// =============================================================================
//
// Installation :
//   Aucune dépendance externe — 100% CPU, toutes plateformes.
//   Utilise GDI (Windows), XShm (Linux), wl_shm (Wayland),
//   ANativeWindow (Android), putImageData (WebGL canvas).
//
int DemoSoftware(const NkEntryState& /*state*/) {
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — Software Renderer Demo",
                                          800, 600);
    NkContextDesc desc = NkContextDesc::MakeSoftware(/*threaded=*/true);

    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);
    if (!window.IsOpen()) return -1;

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    auto* sw = static_cast<NkSoftwareContext*>(ctx.get());
    printf("[DemoSW] Software renderer started — %dx%d\n",
           cfg.width, cfg.height);

    float time = 0.f;
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float dt) {
        time += dt;
        auto& fb = sw->GetBackBuffer();
        uint32 w = fb.width, h = fb.height;

        // Dégradé animé + onde sinusoïdale
        for (uint32 y = 0; y < h; ++y) {
            uint8* row = fb.RowPtr(y);
            for (uint32 x = 0; x < w; ++x) {
                float nx = (float)x / w;
                float ny = (float)y / h;
                float wave = sinf(nx * 10.f + time * 3.f) * 0.5f + 0.5f;

                row[x*4 + 0] = (uint8)(nx * 255);                    // R
                row[x*4 + 1] = (uint8)(ny * wave * 255);              // G
                row[x*4 + 2] = (uint8)((1.f - nx) * 200 + 55);       // B
                row[x*4 + 3] = 255;                                   // A
            }
        }
    });

    ctx->Shutdown();
    window.Close();
    return 0;
}

// =============================================================================
// DÉMO 7 — Sélection automatique de l'API avec fallback
// =============================================================================
int DemoAutoAPI(const NkEntryState& state) {
    // Chaîne de fallback : meilleure API → software
    const NkGraphicsApi chain[] = {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        NkGraphicsApi::NK_GFX_API_D3D12,
        NkGraphicsApi::NK_GFX_API_D3D11,
#elif defined(NKENTSEU_PLATFORM_MACOS)
        NkGraphicsApi::NK_GFX_API_METAL,
#elif defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
        NkGraphicsApi::NK_GFX_API_VULKAN,
        NkGraphicsApi::NK_GFX_API_OPENGLES,
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
        NkGraphicsApi::NK_GFX_API_WEBGL,
        NkGraphicsApi::NK_GFX_API_OPENGLES,
#else
        NkGraphicsApi::NK_GFX_API_VULKAN,
        NkGraphicsApi::NK_GFX_API_OPENGL,
#endif
        NkGraphicsApi::NK_GFX_API_SOFTWARE,
    };

    auto makeDescForApi = [](NkGraphicsApi api) {
        NkContextDesc d;
        switch (api) {
            case NkGraphicsApi::NK_GFX_API_D3D12:
                d = NkContextDesc::MakeDirectX12(/*debug=*/true);
                break;
            case NkGraphicsApi::NK_GFX_API_D3D11:
                d = NkContextDesc::MakeDirectX11(/*debug=*/true);
                break;
            case NkGraphicsApi::NK_GFX_API_METAL:
                d = NkContextDesc::MakeMetal();
                break;
            case NkGraphicsApi::NK_GFX_API_VULKAN:
                d = NkContextDesc::MakeVulkan(/*validation=*/true);
                break;
            case NkGraphicsApi::NK_GFX_API_OPENGL:
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(NKENTSEU_WINDOWING_WAYLAND)
                d = NkContextDesc::MakeOpenGLES(3, 0);
#else
                d = NkContextDesc::MakeOpenGL(4, 6, /*debug=*/true);
#endif
                break;
            case NkGraphicsApi::NK_GFX_API_OPENGLES:
                d = NkContextDesc::MakeOpenGLES(3, 0);
                break;
            case NkGraphicsApi::NK_GFX_API_WEBGL:
                d = NkContextDesc::MakeOpenGLES(3, 0);
                d.api = NkGraphicsApi::NK_GFX_API_WEBGL;
                d.opengl.runtime.installDebugCallback = false;
                break;
            case NkGraphicsApi::NK_GFX_API_SOFTWARE:
                d = NkContextDesc::MakeSoftware(/*threaded=*/true);
                break;
            default:
                d.api = NkGraphicsApi::NK_GFX_API_NONE;
                break;
        }
        return d;
    };

    NkContextDesc desc;
    for (auto api : chain) {
        if (NkContextFactory::IsApiSupported(api)) {
            desc = makeDescForApi(api);
            break;
        }
    }
    if (desc.api == NkGraphicsApi::NK_GFX_API_NONE) {
        printf("[DemoAuto] No API available!\n"); return -1;
    }

    printf("[DemoAuto] Selected API: %s\n", NkGraphicsApiName(desc.api));

    NkWindowConfig cfg = MakeWindowConfig("NkEngine — Auto API");
    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);
    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx && desc.api != NkGraphicsApi::NK_GFX_API_SOFTWARE) {
        printf("[DemoAuto] Failed with %s, falling back to Software\n",
               NkGraphicsApiName(desc.api));
        desc = makeDescForApi(NkGraphicsApi::NK_GFX_API_SOFTWARE);
        ctx = NkContextFactory::Create(desc, window);
    }
    if (!ctx) return -2;

    NkContextInfo info = ctx->GetInfo();
    printf("[DemoAuto] Renderer : %s\n", info.renderer);
    printf("[DemoAuto] Version  : %s\n", info.version);
    printf("[DemoAuto] VRAM     : %u MB\n", info.vramMB);

    float time = 0.f;
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext* activeCtx, float dt) {
        time += dt;
        const NkGraphicsApi api = activeCtx ? activeCtx->GetApi() : NkGraphicsApi::NK_GFX_API_NONE;

        if (api == NkGraphicsApi::NK_GFX_API_OPENGL ||
            api == NkGraphicsApi::NK_GFX_API_OPENGLES ||
            api == NkGraphicsApi::NK_GFX_API_WEBGL) {
            const auto surface = window.GetSurfaceDesc();
            glViewport(0, 0, surface.width, surface.height);
            const float r = 0.35f + 0.35f * (0.5f + 0.5f * static_cast<float>(std::sin(time * 1.1f)));
            const float g = 0.35f + 0.35f * (0.5f + 0.5f * static_cast<float>(std::sin(time * 1.7f + 1.0f)));
            const float b = 0.45f + 0.35f * (0.5f + 0.5f * static_cast<float>(std::sin(time * 2.3f + 2.0f)));
            glClearColor(r, g, b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }

        if (api == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
            auto* fb = nativecontext::GetSoftwareBackBuffer(activeCtx);
            if (!fb || !fb->IsValid()) return;
            for (uint32 y = 0; y < fb->height; ++y) {
                uint8* row = fb->RowPtr(y);
                for (uint32 x = 0; x < fb->width; ++x) {
                    const float nx = static_cast<float>(x) / static_cast<float>(fb->width);
                    const float ny = static_cast<float>(y) / static_cast<float>(fb->height);
                    const float wave = 0.5f + 0.5f * static_cast<float>(std::sin((nx + ny) * 10.0f + time * 3.0f));
                    row[x * 4 + 0] = static_cast<uint8>(nx * 255.0f);
                    row[x * 4 + 1] = static_cast<uint8>(ny * 255.0f * wave);
                    row[x * 4 + 2] = static_cast<uint8>((1.0f - nx) * 255.0f);
                    row[x * 4 + 3] = 255;
                }
            }
        }
    });

    ctx->Shutdown();
    window.Close();
    return 0;
}

} // namespace nkentseu

// =============================================================================
// nkmain — point d'entrée principal avec sélection de la démo
// =============================================================================
int nkmain(const nkentseu::NkEntryState& state) {
    using namespace nkentseu;

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
            "opengl", "opengles", "webgl", "vulkan", "dx11", "dx12", "metal", "software", "auto"
        };
        for (const char* k : kKnown) {
            if (equalsIgnoreCase(arg, k)) return k;
        }
        return nullptr;
    };

    // Choisir la démo à exécuter
    // Priorité: CLI (--demo/--api) > env NK_DEMO > fallback auto.
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
        if ((equalsIgnoreCase(a, "--demo") || equalsIgnoreCase(a, "--api")) &&
            (i + 1) < args.Size()) {
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
#if defined(NKENTSEU_PLATFORM_ANDROID)
    if (!demo) {
        char propValue[PROP_VALUE_MAX] = {};
        if (__system_property_get("debug.nk.demo", propValue) > 0) {
            demo = parseDemoFromArg(propValue);
        }
    }
#endif
    if (!demo) {
        demo = "auto";
    }

    printf("[NewGeneration] demo=%s\n", demo);

    if (strcmp(demo, "opengl")   == 0) return DemoOpenGL(state);
    if (strcmp(demo, "opengles") == 0) return DemoOpenGL(state);
    if (strcmp(demo, "webgl")    == 0) return DemoOpenGL(state);
    if (strcmp(demo, "vulkan")   == 0) return DemoVulkan(state);
    if (strcmp(demo, "software") == 0) return DemoSoftware(state);
    if (strcmp(demo, "auto")     == 0) return DemoAutoAPI(state);

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    if (strcmp(demo, "dx11")     == 0) return DemoDirectX11(state);
    if (strcmp(demo, "dx12")     == 0) return DemoDirectX12(state);
#endif

#if defined(NKENTSEU_PLATFORM_MACOS) || defined(NKENTSEU_PLATFORM_IOS)
    if (strcmp(demo, "metal")    == 0) return DemoMetal(state);
#endif

    return DemoAutoAPI(state); // fallback
}
