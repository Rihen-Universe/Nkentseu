// =============================================================================
// main.cpp  — Renderdemo entry point (NkRenderer v5.0)
//
// Usage :
//   renderdemo                       # demo par defaut (Demo 0 — Subsystems)
//   renderdemo --demo=N              # selectionne la demo (0=Subsystems, 1=2D, 2=3D, 3=Materials)
//   renderdemo --backend=opengl      # (par defaut)
//   renderdemo --backend=vulkan|dx11|dx12|metal|sw
//
// Liste des demos :
//   0 — Subsystems  : enable/disable runtime des sous-systemes
//   1 — 2D          : sprites + formes + texte (config For2D)
//   2 — 3D          : sphere grid + lights + ombres (config ForGame)
//   3 — Materials   : 5 spheres NkMaterial (PBR/Toon/Anime/Unlit), edition temps reel
// =============================================================================
#include "DemoCommon.h"

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::demo;

namespace nkentseu { namespace demo {

    // Forward declarations des demos
    bool DemoSubsystems_Init        (DemoCtx&); void DemoSubsystems_Frame    (DemoCtx&, float32); void DemoSubsystems_Shutdown    (DemoCtx&);
    bool Demo2D_Init                (DemoCtx&); void Demo2D_Frame            (DemoCtx&, float32); void Demo2D_Shutdown            (DemoCtx&);
    bool Demo3D_Init                (DemoCtx&); void Demo3D_Frame            (DemoCtx&, float32); void Demo3D_Shutdown            (DemoCtx&);
    bool Demo4_Materials_Init       (DemoCtx&); void Demo4_Materials_Frame   (DemoCtx&, float32); void Demo4_Materials_Shutdown   (DemoCtx&);
    bool Demo5_Materials_Init       (DemoCtx&); void Demo5_Materials_Frame   (DemoCtx&, float32); void Demo5_Materials_Shutdown   (DemoCtx&);

    static const DemoEntry kDemos[] = {
        { "Subsystems", "Runtime enable/disable des sous-systemes",
            DemoSubsystems_Init,      DemoSubsystems_Frame,    DemoSubsystems_Shutdown },
        { "2D",         "Render2D : sprites + shapes + texte",
            Demo2D_Init,              Demo2D_Frame,            Demo2D_Shutdown },
        { "3D",         "Render3D : grid PBR + lights + ombres",
            Demo3D_Init,              Demo3D_Frame,            Demo3D_Shutdown },
        { "Materials",  "NkMaterial : 5 spheres multi-materiau, modifications temps reel",
            Demo4_Materials_Init,     Demo4_Materials_Frame,   Demo4_Materials_Shutdown },
        { "Materials5", "NkMaterial v2 : evolutions M.2+ (MPC, blend vcolor, hierarchies, etc.)",
            Demo5_Materials_Init,     Demo5_Materials_Frame,   Demo5_Materials_Shutdown },
    };
    static constexpr uint32 kDemoCount = (uint32)(sizeof(kDemos) / sizeof(kDemos[0]));

    static NkRendererConfig BuildConfig(int demoIdx, NkGraphicsApi api,
                                         uint32 w, uint32 h) {
        switch (demoIdx) {
            case 0: {
                NkRendererConfig c;
                c.api        = api;
                c.width      = w;
                c.height     = h;
                c.subsystems = NK_SS_RENDER2D | NK_SS_TEXT | NK_SS_OVERLAY;
                c.hdr        = false;
                return c;
            }
            case 1: return NkRendererConfig::For2D(api, w, h);
            case 2: {
                auto c = NkRendererConfig::ForGame(api, w, h);
                // Demo3D scene tient dans ~7 unites -> 1 cascade large suffit
                // et evite les transitions de cascade qui font scintiller les
                // ombres quand la camera orbite. CSM 4-cascades reste actif
                // dans le code et utilisable pour des scenes plus ouvertes.
                c.shadow.cascadeCount = 1;
                return c;
            }
            case 3: {
                // Demo4 : meme config que Demo3D — 5 spheres, scene ~12 unites.
                // PCSS active (contact-hardening) : sphere ↔ sol -> ombres
                // nettes au contact, plus floues en s'eloignant.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = true;
                return c;
            }
            case 4: {
                // Demo5 : evolutions M.2+ par dessus Demo4, meme config de base.
                // PCSS off temporairement : on debug shadow CSM pour les spheres
                // du dessous (Y<0), PCSS peut introduire un bias different.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                return c;
            }
            default: return NkRendererConfig::ForGame(api, w, h);
        }
    }

}} // namespace

// =============================================================================
// nkmain — entry point unifie
// =============================================================================
int nkmain(const NkEntryState& state) {

    // ── Parse args ───────────────────────────────────────────────────────────
    NkGraphicsApi api    = ParseBackend(state.GetArgs());
    int           demoIx = ParseDemo(state.GetArgs(), 0);
    // Alias : --demo=N -> index N-1 pour les demos numerotees (Demo4 -> 3, Demo5 -> 4).
    // Coherence avec le nom de fichier plutot que l'index zero-based.
    if (demoIx == 4) demoIx = 3;
    if (demoIx == 5) demoIx = 4;
    if (demoIx < 0 || (uint32)demoIx >= kDemoCount) demoIx = 0;
    const DemoEntry& demo = kDemos[demoIx];

    logger.Info("=========================================================\n");
    logger.Info(" NkRenderer v5.0 — Demo {0} ({1}) — Backend : {2}\n",
                demoIx, demo.name, NkGraphicsApiName(api));
    logger.Info("=========================================================\n");

    // ── Fenetre ──────────────────────────────────────────────────────────────
    NkWindowConfig wcfg;
    wcfg.title     = NkFormat("NkRenderer demo : {0}", demo.name);
    wcfg.width     = 1280;
    wcfg.height    = 720;
    wcfg.centered  = true;
    wcfg.resizable = true;

    NkWindow window;
    if (!window.Create(wcfg)) {
        logger.Errorf("[main] Window creation failed\n");
        return 1;
    }

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkDeviceInitInfo devInfo;
    devInfo.api     = api;
    devInfo.surface = surface;
    devInfo.width   = (uint32)window.GetSize().width;
    devInfo.height  = (uint32)window.GetSize().height;
    devInfo.context.vulkan.appName    = "NkRenderer_Demo";
    devInfo.context.vulkan.engineName = "Nkentseu";

    NkIDevice* device = NkDeviceFactory::Create(devInfo);
    if (!device || !device->IsValid()) {
        logger.Errorf("[main] NkDeviceFactory::Create failed\n");
        window.Close();
        return 2;
    }

    // ── Renderer ─────────────────────────────────────────────────────────────
    uint32 W = (uint32)window.GetSize().width;
    uint32 H = (uint32)window.GetSize().height;
    NkRendererConfig cfg = BuildConfig(demoIx, api, W, H);

    char flagsBuf[256];
    SubsystemFlagsToString(cfg.subsystems, flagsBuf, sizeof(flagsBuf));
    logger.Info("[main] Config : {0}x{1}, subsystems = {2}\n", W, H, flagsBuf);

    NkRenderer* renderer = NkRenderer::Create(device, cfg);
    if (!renderer) {
        logger.Errorf("[main] NkRenderer::Create failed (last err : {0})\n",
                      NkRGetLastErrorMessage());
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    // ── Demo init ────────────────────────────────────────────────────────────
    DemoCtx ctx;
    ctx.device   = device;
    ctx.renderer = renderer;
    ctx.window   = &window;
    ctx.api      = api;
    ctx.width    = W;
    ctx.height   = H;
    if (!demo.init(ctx)) {
        logger.Errorf("[main] Demo init failed\n");
        NkRenderer::Destroy(renderer);
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 4;
    }

    // ── Boucle ───────────────────────────────────────────────────────────────
    bool running = true;
    NkClock clock;
    NkEventSystem& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        ctx.width  = (uint32)e->GetWidth();
        ctx.height = (uint32)e->GetHeight();
        if (ctx.width > 0 && ctx.height > 0)
            renderer->OnResize(ctx.width, ctx.height);
    });

    while (running) {
        events.PollEvents();
        if (!running) break;

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        ctx.totalTime += dt;
        ctx.frame++;

        if (ctx.width == 0 || ctx.height == 0) continue;
        demo.frame(ctx, dt);
    }

    // ── Cleanup ──────────────────────────────────────────────────────────────
    demo.shutdown(ctx);
    NkRenderer::Destroy(renderer);
    device->WaitIdle();
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[main] Bye\n");
    return 0;
}
