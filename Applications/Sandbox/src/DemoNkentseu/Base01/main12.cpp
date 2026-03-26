// ============================================================================
// Sandbox/src/main12.cpp
// DirectX11-only bootstrap + software colorization.
// ============================================================================

#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKContext/Deprecate/NkRendererConfig.h"
#include "NKLogger/NkLog.h"

namespace {
    using namespace nkentseu;

    static uint32 MakeColor(uint32 frame) {
        const uint8 r = static_cast<uint8>(((frame * 2u) + 20u) & 0xFFu);
        const uint8 g = static_cast<uint8>(((frame * 1u) + 110u) & 0xFFu);
        const uint8 b = static_cast<uint8>(((frame * 3u) + 200u) & 0xFFu);
        return NkRenderer::PackColor(r, g, b, 255);
    }
} // namespace

NKENTSEU_DEFINE_APP_DATA(([]() {
    nkentseu::NkAppData d{};
    d.appName = "SandboxDX11Only";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    return d;
})());

int nkmain(const nkentseu::NkEntryState&) {
    using namespace nkentseu;

    NkContextInit();
    NkContextConfig contextConfig{};
    contextConfig.api = NkRendererApi::NK_DIRECTX11;
    contextConfig.vsync = true;
    contextConfig.debug = false;
    NkContextSetHints(contextConfig);
    contextConfig = NkContextGetHints();

    NkWindowConfig windowConfig{};
    windowConfig.title = "Sandbox DirectX11 Only";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.resizable = true;
    NkContextApplyWindowHints(windowConfig);

    NkWindow window;
    if (!window.Create(windowConfig)) {
        logger.Error("[main12] Window creation failed: {0}", window.GetLastError().ToString());
        NkContextShutdown();
        return -1;
    }

    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        logger.Error("[main12] Context creation failed: {0}", context.lastError.ToString());
        window.Close();
        NkContextShutdown();
        return -2;
    }

    logger.Info("[main12] DirectX11 bootstrap handle ready (surface-only mode).");

    NkRenderer renderer;
    NkRendererConfig rcfg{};
    rcfg.api = NkRendererApi::NK_SOFTWARE;
    rcfg.vsync = true;
    if (!renderer.Create(window, rcfg)) {
        logger.Error("[main12] Software colorizer creation failed: {0}", renderer.GetLastError().ToString());
        NkContextDestroy(context);
        window.Close();
        NkContextShutdown();
        return -3;
    }

    auto& events = NkEvents();
    bool running = true;
    uint32 frame = 0;
    while (running && window.IsOpen()) {
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
        }

        renderer.BeginFrame(MakeColor(frame));
        renderer.EndFrame();
        renderer.Present();
        ++frame;
    }

    renderer.Shutdown();
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}

