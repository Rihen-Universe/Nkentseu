// ============================================================================
// Sandbox/src/main15.cpp
// Software-only context + software colorization.
// ============================================================================

#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Events/NkWindowEvent.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKContext/Deprecate/NkRendererConfig.h"
#include "NKLogger/NkLog.h"

namespace {
    using namespace nkentseu;

    static uint32 MakeColor(uint32 frame) {
        const uint8 r = static_cast<uint8>(((frame * 1u) + 30u) & 0xFFu);
        const uint8 g = static_cast<uint8>(((frame * 1u) + 120u) & 0xFFu);
        const uint8 b = static_cast<uint8>(((frame * 1u) + 210u) & 0xFFu);
        return NkRenderer::PackColor(r, g, b, 255);
    }
} // namespace

NKENTSEU_DEFINE_APP_DATA(([]() {
    nkentseu::NkAppData d{};
    d.appName = "SandboxSoftwareOnly";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    return d;
})());

int nkmain(const nkentseu::NkEntryState&) {
    using namespace nkentseu;

    NkContextInit();
    NkContextConfig contextConfig{};
    contextConfig.api = NkRendererApi::NK_SOFTWARE;
    contextConfig.vsync = true;
    contextConfig.debug = false;
    NkContextSetHints(contextConfig);
    contextConfig = NkContextGetHints();

    NkWindowConfig windowConfig{};
    windowConfig.title = "Sandbox Software Only";
    windowConfig.width = 1280;
    windowConfig.height = 720;
    windowConfig.resizable = true;
    NkContextApplyWindowHints(windowConfig);

    NkWindow window;
    if (!window.Create(windowConfig)) {
        logger.Error("[main15] Window creation failed: {0}", window.GetLastError().ToString());
        NkContextShutdown();
        return -1;
    }

    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        logger.Error("[main15] Context creation failed: {0}", context.lastError.ToString());
        window.Close();
        NkContextShutdown();
        return -2;
    }

    NkRenderer renderer;
    NkRendererConfig rcfg{};
    rcfg.api = NkRendererApi::NK_SOFTWARE;
    rcfg.vsync = true;
    if (!renderer.Create(window, rcfg)) {
        logger.Error("[main15] Software renderer creation failed: {0}", renderer.GetLastError().ToString());
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

