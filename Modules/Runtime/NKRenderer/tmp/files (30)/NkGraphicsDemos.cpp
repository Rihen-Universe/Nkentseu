// =============================================================================
// NkGraphicsDemos.cpp
// Démos complètes pour chaque API graphique.
// Reprend exactement le pattern de ton nkmain / NkEntryState.
//
// Chaque démo est une fonction autonome appelable depuis nkmain.
// =============================================================================

#include "NkWindow.h"
#include "NkWindowConfig.h"
#include "NkContextFactory.h"
#include "NkContextDesc.h"
#include "NkNativeContextAccess.h"
#include "NkOpenGLDesc.h"

// GLAD2 — inclus ici pour les draw calls GL dans les démos
#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <glad/wgl.h>
#   include <glad/gl.h>
#elif defined(NKENTSEU_WINDOWING_XLIB) || defined(NKENTSEU_WINDOWING_XCB)
#   include <glad/glx.h>
#   include <glad/gl.h>
#elif defined(NKENTSEU_WINDOWING_WAYLAND) || defined(NKENTSEU_PLATFORM_ANDROID)
#   include <glad/gles2.h>
#endif

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <d3d12.h>
#   include <d3dcompiler.h>
#endif

#include <cstdio>

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
    auto& eventSystem = NkEvents();
    NkChrono chrono;
    NkElapsedTime elapsed;
    bool running = true;

    while (running && window.IsOpen()) {
        elapsed = chrono.Reset();
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;

        // Polling événements
        while (NkEvent* event = eventSystem.PollEvent()) {
            if (event->GetType() == NkEventType::WindowClosed ||
                event->GetType() == NkEventType::AppQuit) {
                running = false; break;
            }
            // Resize
            if (event->GetType() == NkEventType::WindowResized) {
                auto& re = event->As<NkWindowResizedEvent>();
                ctx->OnResize(re.width, re.height);
            }
        }
        if (!running || !window.IsOpen()) break;

        if (ctx->BeginFrame()) {
            onRender(ctx, dt);
            ctx->EndFrame();
            ctx->Present();
        }

        // Cap 60 fps
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }
    return true;
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
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — OpenGL 4.6 Demo");

    // ── Configurer le contexte OpenGL ────────────────────────────────────────
    NkContextDesc desc;
    desc.api    = NkGraphicsApi::NK_API_OPENGL;
    desc.opengl = NkOpenGLDesc::Desktop46(/*debug=*/true);
    desc.opengl.msaaSamples      = 4;
    desc.opengl.srgbFramebuffer  = true;
    desc.opengl.swapInterval     = NkGLSwapInterval::AdaptiveVSync;

    // Personnaliser le PIXELFORMATDESCRIPTOR bootstrap (optionnel)
    // desc.opengl.wglFallback = NkWGLFallbackPixelFormat::Standard(); // déjà par défaut
    // desc.opengl.wglFallback = NkWGLFallbackPixelFormat::Minimal();  // driver ancien

    // GLAD2 : installer le debug callback automatiquement
    desc.opengl.glad2.autoLoad              = true;
    desc.opengl.glad2.validateVersion       = true;
    desc.opengl.glad2.installDebugCallback  = true;
    desc.opengl.glad2.debugSeverityLevel    = 2; // medium+

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
        auto& surface = window.GetSurfaceDesc();
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
    NkWindowConfig cfg = MakeWindowConfig("NkEngine — Vulkan Demo");
    NkContextDesc desc = NkContextDesc::MakeVulkan(/*validation=*/true);
    desc.vulkan.swapchainImages = 3;
    desc.vulkan.vsync           = true;

    NkContextFactory::PrepareWindowConfig(desc, cfg); // no-op pour Vulkan
    NkWindow window(cfg);
    if (!window.IsOpen()) return -1;

    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    // Accès typé aux données Vulkan via NkNativeContext
    // Pas besoin de cast void* manuellement
    VkDevice         device     = NkNativeContext::GetVkDevice(ctx.get());
    VkRenderPass     renderPass = NkNativeContext::GetVkRenderPass(ctx.get());

    printf("[DemoVK] Device: %s\n", ctx->GetInfo().renderer);
    printf("[DemoVK] RenderPass: %p\n", (void*)renderPass);

    float hue = 0.0f;
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float dt) {
        // Le BeginFrame / EndFrame / Present gère toute la synchronisation.
        // À l'intérieur de BeginFrame → EndFrame, le render pass est actif.
        // On peut récupérer le command buffer pour des draw calls additionnels.
        VkCommandBuffer cmd = NkNativeContext::GetVkCurrentCommandBuffer(ctx.get());

        // Ici on ferait : vkCmdBindPipeline, vkCmdDraw, etc.
        // Pour la démo : clear couleur animée via le render pass (déjà dans BeginFrame)
        hue = fmodf(hue + dt * 60.0f, 360.0f);
        (void)cmd; // utilisé dans une vraie implémentation pipeline
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
    ID3D11Device1*        device  = NkNativeContext::GetDX11Device(ctx.get());
    ID3D11DeviceContext1* devCtx  = NkNativeContext::GetDX11Context(ctx.get());

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
    ID3D12Device5*              device    = NkNativeContext::GetDX12Device(ctx.get());
    ID3D12CommandQueue*         queue     = NkNativeContext::GetDX12CommandQueue(ctx.get());
    ID3D12GraphicsCommandList4* cmdList   = NkNativeContext::GetDX12CommandList(ctx.get());
    auto* dx12Data = NkNativeContext::DX12(ctx.get());

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

    auto* dx12Ctx = static_cast<NkDX12Context*>(ctx.get());
    dx12Ctx->FlushGPU(); // Attendre GPU avant destruction
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
    void* mtlDevice = NkNativeContext::GetMetalDevice(ctx.get());
    printf("[DemoMetal] Running on: %s\n", ctx->GetInfo().renderer);
    printf("[DemoMetal] MTLDevice: %p\n", mtlDevice);

    // Le rendu Metal se fait via l'encoder récupéré dans currentEncoder.
    // Pour un triangle : créer MTLRenderPipelineDescriptor + MTLLibrary + draw.
    // Cette démo fait juste un clear animé (géré par BeginFrame).
    RunLoop(window, ctx.get(), [&](NkIGraphicsContext*, float) {
        void* encoder = NkNativeContext::GetMetalCommandEncoder(ctx.get());
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
        NkGraphicsApi::NK_API_DIRECTX12,
        NkGraphicsApi::NK_API_DIRECTX11,
#elif defined(NKENTSEU_PLATFORM_MACOS)
        NkGraphicsApi::NK_API_METAL,
#endif
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE,
    };

    NkContextDesc desc;
    for (auto api : chain) {
        if (NkContextFactory::IsApiSupported(api)) {
            desc.api = api;
            break;
        }
    }
    if (desc.api == NkGraphicsApi::NK_API_NONE) {
        printf("[DemoAuto] No API available!\n"); return -1;
    }

    printf("[DemoAuto] Selected API: %s\n", NkGraphicsApiName(desc.api));

    NkWindowConfig cfg = MakeWindowConfig("NkEngine — Auto API");
    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);
    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) return -2;

    NkContextInfo info = ctx->GetInfo();
    printf("[DemoAuto] Renderer : %s\n", info.renderer);
    printf("[DemoAuto] Version  : %s\n", info.version);
    printf("[DemoAuto] VRAM     : %u MB\n", info.vramMB);

    RunLoop(window, ctx.get(), [](NkIGraphicsContext*, float) {
        // Clear simple — API agnostique
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

    // Choisir la démo à exécuter
    // En pratique : lire depuis les args de state ou un fichier de config
    const char* demo = "opengl"; // "opengl" | "vulkan" | "dx11" | "dx12"
                                  // "metal" | "software" | "auto"

    if (strcmp(demo, "opengl")   == 0) return DemoOpenGL(state);
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
