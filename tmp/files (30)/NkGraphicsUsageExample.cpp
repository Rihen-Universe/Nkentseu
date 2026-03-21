// =============================================================================
// NkGraphicsUsageExample.cpp
// Exemple complet d'utilisation du système de contextes graphiques.
// =============================================================================

#include "NkWindow.h"
#include "NkWindowConfig.h"
#include "NkContextFactory.h"
#include "NkContextDesc.h"

// Exemple 1 : Vulkan (recommandé sur la plupart des plateformes)
void ExampleVulkan() {
    using namespace nkentseu;

    // a) Config de la fenêtre — inchangée
    NkWindowConfig windowConfig;
    windowConfig.title  = "NkEngine — Vulkan";
    windowConfig.width  = 1280;
    windowConfig.height = 720;

    // b) Descripteur de contexte Vulkan
    NkContextDesc desc = NkContextDesc::MakeVulkan(
        /*validation=*/true   // mettre false en release
    );
    desc.vulkan.swapchainImages = 3; // Triple buffering

    // c) PrepareWindowConfig — no-op pour Vulkan, toujours safe à appeler
    NkContextFactory::PrepareWindowConfig(desc, windowConfig);

    // d) Créer la fenêtre normalement
    NkWindow window(windowConfig);

    // e) Créer le contexte
    auto ctx = NkContextFactory::Create(desc, window);
    if (!ctx) {
        // Fallback sur OpenGL
        ctx = NkContextFactory::Create(NkContextDesc::MakeOpenGL(4, 5), window);
    }

    // f) Boucle de rendu
    while (window.IsOpen()) {
        // Polling des événements (clavier, souris, resize...)
        // NkEvents::PollEvents();

        // Gestion du resize
        // if (resizeRequested) { ctx->OnResize(newW, newH); }

        if (ctx->BeginFrame()) {
            // === Rendu Vulkan ===
            // Récupérer le command buffer courant
            auto* vkCtx = static_cast<NkVulkanContext*>(ctx.get());
            VkCommandBuffer cmd = vkCtx->GetCurrentCommandBuffer();

            // Bind pipeline, draw calls, etc.
            // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            // vkCmdDraw(cmd, 3, 1, 0, 0);

            ctx->EndFrame();
            ctx->Present();
        }
    }

    ctx->Shutdown();
}

// =============================================================================
// Exemple 2 : OpenGL sur Linux (avec injection GLX)
// =============================================================================
void ExampleOpenGL_Linux() {
    using namespace nkentseu;

    NkWindowConfig windowConfig;
    windowConfig.title = "NkEngine — OpenGL";

    NkContextDesc desc = NkContextDesc::MakeOpenGL(4, 6, /*debug=*/true);
    desc.opengl.msaaSamples     = 4;
    desc.opengl.depthBits       = 24;
    desc.opengl.srgbFramebuffer = true;

    // CRITIQUE pour GLX uniquement :
    // Appeler PrepareWindowConfig AVANT NkWindow::Create()
    // → Injecte le GLX FBConfig et VisualID dans windowConfig.surfaceHints
    // → NkXLibWindow utilisera ce VisualID pour XCreateWindow
    NkContextFactory::PrepareWindowConfig(desc, windowConfig);

    // Maintenant créer la fenêtre avec le bon Visual
    NkWindow window(windowConfig);

    auto ctx = NkContextFactory::Create(desc, window);

    while (window.IsOpen()) {
        if (ctx->BeginFrame()) {
            // glClear, glDrawArrays, etc.
            ctx->EndFrame();
            ctx->Present(); // SwapBuffers
        }
    }

    ctx->Shutdown();
}

// =============================================================================
// Exemple 3 : DirectX 12 sur Windows
// =============================================================================
void ExampleDirectX12_Windows() {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    using namespace nkentseu;

    NkWindowConfig windowConfig;
    windowConfig.title = "NkEngine — DirectX 12";

    NkContextDesc desc = NkContextDesc::MakeDirectX12(/*debug=*/true);
    desc.dx12.swapchainBuffers = 3;
    desc.dx12.allowTearing     = true;

    NkContextFactory::PrepareWindowConfig(desc, windowConfig); // no-op
    NkWindow window(windowConfig);

    auto ctx = NkContextFactory::Create(desc, window);
    auto* dx12 = static_cast<NkDX12Context*>(ctx.get());

    while (window.IsOpen()) {
        if (ctx->BeginFrame()) {
            ID3D12GraphicsCommandList4* cmdList = dx12->GetCommandList();
            // Bind root signature, pipeline state, draw...
            ctx->EndFrame();
            ctx->Present();
        }
    }

    dx12->FlushGPU(); // Attendre que le GPU ait tout traité avant de détruire
    ctx->Shutdown();
#endif
}

// =============================================================================
// Exemple 4 : Software Renderer (toutes plateformes)
// =============================================================================
void ExampleSoftware() {
    using namespace nkentseu;

    NkWindowConfig windowConfig;
    windowConfig.title = "NkEngine — Software";

    NkContextDesc desc = NkContextDesc::MakeSoftware(/*threaded=*/true);

    NkContextFactory::PrepareWindowConfig(desc, windowConfig); // no-op
    NkWindow window(windowConfig);

    auto ctx = NkContextFactory::Create(desc, window);
    auto* sw = static_cast<NkSoftwareContext*>(ctx.get());

    while (window.IsOpen()) {
        if (ctx->BeginFrame()) {
            // Dessiner dans le framebuffer CPU
            auto& fb = sw->GetBackBuffer();

            // Tracer un dégradé simple
            for (uint32 y = 0; y < fb.height; ++y)
                for (uint32 x = 0; x < fb.width; ++x)
                    sw->SetPixel(x, y,
                        (uint8)(x * 255 / fb.width),
                        (uint8)(y * 255 / fb.height),
                        128);

            ctx->EndFrame();
            ctx->Present(); // Copie CPU → fenêtre via GDI/XPutImage/etc.
        }
    }

    ctx->Shutdown();
}

// =============================================================================
// Exemple 5 : Sélection automatique de l'API optimale
// =============================================================================
void ExampleAutoApi() {
    using namespace nkentseu;

    NkGraphicsApi bestApi = NkContextFactory::GetDefaultApi();

    // Fallback chain si l'API par défaut n'est pas supportée à l'exécution
    NkGraphicsApi fallbacks[] = {
        bestApi,
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE,
    };

    NkContextDesc desc;
    for (auto api : fallbacks) {
        if (NkContextFactory::IsApiSupported(api)) {
            desc.api = api;
            break;
        }
    }

    printf("Selected API: %s\n", NkGraphicsApiName(desc.api));

    NkWindowConfig cfg;
    NkContextFactory::PrepareWindowConfig(desc, cfg);
    NkWindow window(cfg);

    auto ctx = NkContextFactory::Create(desc, window);
    NkContextInfo info = ctx->GetInfo();
    printf("Renderer: %s | %s\n", info.renderer, info.version);

    while (window.IsOpen()) {
        if (ctx->BeginFrame()) {
            // Rendu agnostique API
            ctx->EndFrame();
            ctx->Present();
        }
    }
    ctx->Shutdown();
}

// =============================================================================
// CMakeLists.txt
// =============================================================================
/*
cmake_minimum_required(VERSION 3.22)
project(NkEngine)

set(CMAKE_CXX_STANDARD 17)

# ── Sources communes (toutes plateformes) ────────────────────────────────────
set(NK_GRAPHICS_SOURCES
    NkGraphics/Core/NkGraphicsApi.h
    NkGraphics/Core/NkContextDesc.h
    NkGraphics/Core/NkIGraphicsContext.h
    NkGraphics/OpenGL/NkOpenGLContextData.h
    NkGraphics/OpenGL/NkOpenGLContext.h
    NkGraphics/OpenGL/NkOpenGLContext.cpp
    NkGraphics/Vulkan/NkVulkanContextData.h
    NkGraphics/Vulkan/NkVulkanContext.h
    NkGraphics/Vulkan/NkVulkanContext.cpp
    NkGraphics/Software/NkSoftwareContext.h
    NkGraphics/Software/NkSoftwareContext.cpp
    NkGraphics/Factory/NkContextFactory.h
    NkGraphics/Factory/NkContextFactory.cpp
)

# ── Sources Windows ───────────────────────────────────────────────────────────
if(WIN32)
    list(APPEND NK_GRAPHICS_SOURCES
        NkGraphics/DirectX/NkDirectXContextData.h
        NkGraphics/DirectX/NkDXContext.h
        NkGraphics/DirectX/NkDX11Context.cpp
        NkGraphics/DirectX/NkDX12Context.cpp
    )
    target_link_libraries(NkEngine PRIVATE
        d3d11 d3d12 dxgi d3dcompiler dxguid)
endif()

# ── Sources macOS / iOS ───────────────────────────────────────────────────────
if(APPLE)
    list(APPEND NK_GRAPHICS_SOURCES
        NkGraphics/Metal/NkMetalContext.h
        NkGraphics/Metal/NkMetalContext.mm   # ← Objective-C++
    )
    set_source_files_properties(
        NkGraphics/Metal/NkMetalContext.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(NkEngine PRIVATE
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework AppKit")
endif()

# ── Bibliothèques Vulkan ──────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
target_link_libraries(NkEngine PRIVATE Vulkan::Vulkan)

# ── Bibliothèques OpenGL/GL ───────────────────────────────────────────────────
if(UNIX AND NOT APPLE AND NOT ANDROID)
    if(NKENTSEU_WINDOWING_XLIB)
        target_link_libraries(NkEngine PRIVATE GL X11 Xext)
    elseif(NKENTSEU_WINDOWING_XCB)
        target_link_libraries(NkEngine PRIVATE GL xcb xcb-image X11 X11-xcb)
    elseif(NKENTSEU_WINDOWING_WAYLAND)
        target_link_libraries(NkEngine PRIVATE EGL GLESv3 wayland-client xkbcommon)
    endif()
elseif(ANDROID)
    target_link_libraries(NkEngine PRIVATE EGL GLESv3 android)
elseif(EMSCRIPTEN)
    target_link_options(NkEngine PRIVATE
        "-sUSE_WEBGL2=1" "-sUSE_GLFW=3" "-sMIN_WEBGL_VERSION=2")
endif()

add_executable(NkEngine
    ${NK_GRAPHICS_SOURCES}
    NkGraphicsUsageExample.cpp
    platform/<current>/main.cpp    # Ton bootstrapper platform
)

target_include_directories(NkEngine PRIVATE
    NkGraphics/Core
    NkGraphics/OpenGL
    NkGraphics/Vulkan
    NkGraphics/DirectX
    NkGraphics/Metal
    NkGraphics/Software
    NkGraphics/Factory
    include    # NkWindow.h, NkTypes.h, etc.
)
*/
