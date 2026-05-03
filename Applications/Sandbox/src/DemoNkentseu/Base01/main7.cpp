// =============================================================================
// ex06_stress_ringbuffer_gamepad.cpp
// Stress test ring buffer + couverture complAte gamepad
//
// Couvre : NkEventRingBuffer (drop-oldest), injection synthAtique d'events,
//          NkGamepadSystem::PollGamepads / GetAxis / IsButtonDown / Rumble,
//          NkGamepadButtonReleaseEvent, NkGamepadAxisEvent dead-zone adaptative,
//          comptage d'events droppAs, report de santA chaque seconde.
//
// ATTENTION : L'injection synthAtique nAcessite une mAthode Enqueue() non publique.
//             Cette partie a AtA dAsactivAe ; l'exemple teste uniquement le gamepad.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Chemin correct pour NkChrono et NkElapsedTime
#include "NKMath/NKMath.h"
#include "NKCore/NkAtomic.h"

#include "NKLogger/NkLog.h"

#include <cmath>
#include <cstring>

// AppData pattern #8: direct API override via NkSetEntryAppData().
namespace {
struct StressAppDataBootstrap {
    StressAppDataBootstrap() {
        nkentseu::NkAppData d{};
        d.appName = "SandboxStressRingBuffer";
        d.appVersion = "1.0.0";
        d.enableEventLogging = false;
        d.enableRendererDebug = false;
        d.enableMultiWindow = false;
        nkentseu::NkSetEntryAppData(d);
    }
};
const StressAppDataBootstrap gStressAppDataBootstrap{};
} // namespace

namespace {
using namespace nkentseu;

static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
static float SafeDt(float dt) {
    if (!math::NkIsFinite(dt) || dt <= 0.f || dt > 0.25f) return 1.f / 60.f;
    return dt;
}
static float SafeAxis(float v) {
    return math::NkIsFinite(v) ? v : 0.f;
}

// ---------------------------------------------------------------------------
// Stats globales thread-safe (remplies depuis callbacks)
// ---------------------------------------------------------------------------
struct Stats {
    NkAtomic<uint64> pushed   { 0 };  // events injectAs
    NkAtomic<uint64> polled   { 0 };  // events rAcupArAs via PollEvent
    NkAtomic<uint64> dropped  { 0 };  // estimA : pushed - polled (en excAs)
    NkAtomic<uint64> gpAxis   { 0 };
    NkAtomic<uint64> gpBtn    { 0 };
    NkAtomic<uint64> gpRumble { 0 };

    void Print(float elapsed) const {
        logger.Info("[HEALTH t={0}s] pushed={1} polled={2} diff={3} gpAxis={4} gpBtn={5} rumble={6}",
            elapsed,
            (unsigned long long)pushed.Load(),
            (unsigned long long)polled.Load(),
            (long long)(pushed.Load() - polled.Load()),
            (unsigned long long)gpAxis.Load(),
            (unsigned long long)gpBtn.Load(),
            (unsigned long long)gpRumble.Load());
    }
};

// ---------------------------------------------------------------------------
// Plasma de fond
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, uint32 w, uint32 h, float t)
{
    if (!w || !h) return;
    if (!math::NkIsFinite(t)) t = 0.f;
    const uint32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (uint32 y = 0; y < h; y += blk) {
        float fy = y/(float)h - 0.5f;
        for (uint32 x = 0; x < w; x += blk) {
            float fx  = x/(float)w - 0.5f;
            float rd  = math::NkSqrt(fx*fx + fy*fy);
            float mix = (math::NkSin(fx*13.5f+t*1.7f)
                        +math::NkSin(fy*11.f -t*1.3f)
                        +math::NkSin(rd*24.f -t*2.1f)) * 0.333f;
            uint8 ri = static_cast<uint8>(ClampUnit(0.5f + 0.5f * math::NkSin(6.28f * (mix + 0.f))) * 255);
            uint8 gi = static_cast<uint8>(ClampUnit(0.5f + 0.5f * math::NkSin(6.28f * (mix + 0.33f))) * 255);
            uint8 bi = static_cast<uint8>(ClampUnit(0.5f + 0.5f * math::NkSin(6.28f * (mix + 0.66f))) * 255);
            uint32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (uint32 by=0;by<blk&&(y+by)<h;++by)
                for (uint32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((int32)(x+bx),(int32)(y+by),col);
        }
    }
}

// ---------------------------------------------------------------------------
// Gamepads - polling explicite de tous les axes et boutons
// ---------------------------------------------------------------------------
struct GpState {
    float axes[static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX)] = {};
    bool  buttons[static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)] = {};
    bool  connected   = false;
};

static void PollGamepad(uint32 idx, GpState& gs, Stats& stats)
{
    auto& gp = NkGamepads();   // Utilisation de NkGamepads()
    gs.connected = gp.IsConnected(idx);
    if (!gs.connected) return;

    for (uint32 ai = 0; ai < static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX); ++ai) {
        const NkGamepadAxis axis = static_cast<NkGamepadAxis>(ai);
        float v = SafeAxis(gp.GetAxis(idx, axis));
        if (math::NkFabs(v - gs.axes[ai]) > 0.001f) {
            gs.axes[ai] = v;
            ++stats.gpAxis;
        }
    }

    for (uint32 bi = static_cast<uint32>(NkGamepadButton::NK_GP_SOUTH);
         bi < static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX); ++bi) {
        const NkGamepadButton btn = static_cast<NkGamepadButton>(bi);
        bool down = gp.IsButtonDown(idx, btn);
        if (down != gs.buttons[bi]) {
            gs.buttons[bi] = down;
            ++stats.gpBtn;
            if (down) {
                logger.Info("[GP#{0}] Button {1} ({2}) pressed",
                    idx, bi, NkGamepadButtonToString(btn));
                // Rumble sur chaque nouveau bouton pressA
                gp.Rumble(idx,
                    0.1f + static_cast<float>(bi) * 0.02f,
                    0.2f + static_cast<float>(bi) * 0.015f,
                    0.f, 0.f, 40);
                ++stats.gpRumble;
            }
        }
    }

    // Stick gauche contrAle la frAquence de rumble continu
    float lx = SafeAxis(gs.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX)]);
    float rt = SafeAxis(gs.axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT)]);
    if (rt > 0.5f && math::NkFabs(lx) > 0.2f) {
        gp.Rumble(idx, math::NkFabs(lx)*0.4f, rt*0.6f, 0.f, 0.f, 16);
        ++stats.gpRumble;
    }
}

} // namespace

// =============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    NkContextInit();
    NkContextConfig contextConfig{};
    contextConfig.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
    contextConfig.vsync = true;
    contextConfig.debug = false;
    NkContextSetHints(contextConfig);
    contextConfig = NkContextGetHints();

    // 1. Runtime dAjA  initialisA par l'entrypoint NkMain

    // 2. FenAtre
    NkWindowConfig cfg;
    cfg.title     = "ex06 - Stress Ring Buffer + Full Gamepad";
    cfg.width     = 1280;
    cfg.height    = 720;
    cfg.centered  = true;
    cfg.resizable = true;

    NkContextApplyWindowHints(cfg);
    NkWindow window(cfg);
    if (!window.IsOpen()) {
        NkContextShutdown();
        return -2;
    }

    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        window.Close();
        NkContextShutdown();
        return -3;
    }

    // 3. Renderer
    NkRendererConfig rcfg;
    rcfg.api                   = NkGraphicsApi::NK_GFX_API_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;
    NkRenderer renderer;
    if (!renderer.Create(window, rcfg)) {
        NkContextDestroy(context);
        window.Close();
        NkContextShutdown();
        return -4;
    }

    // 4. Gamepad callbacks (event-driven)
    // CORRECTION : la lambda doit correspondre A  NkGamepadConnectCallback (const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            logger.Info("[Gamepad] {0} #{1} ({2})",
                connected ? "CONNECTED" : "DISCONNECTED", info.index, info.name);
        });

    Stats stats;
    auto& es = NkEvents();

    // Callbacks event-driven pour statistiques gamepad
    es.AddEventCallback<NkGamepadAxisEvent>(
        [&stats](NkGamepadAxisEvent* ev) {
            ++stats.gpAxis;
            (void)ev;
        });
    es.AddEventCallback<NkGamepadButtonPressEvent>(
        [&stats](NkGamepadButtonPressEvent* ev) {
            ++stats.gpBtn;
            logger.Info("[GP Event] ButtonPress {0} idx={1}", (int)ev->GetButton(), ev->GetGamepadIndex());
        });
    es.AddEventCallback<NkGamepadButtonReleaseEvent>(
        [&stats](NkGamepadButtonReleaseEvent* ev) {
            ++stats.gpBtn;
            logger.Info("[GP Event] ButtonRelease {0} idx={1}", (int)ev->GetButton(), ev->GetGamepadIndex());
        });

    // 5. Atat
    bool    running  = true;
    float   time     = 0.f;
    GpState gpStates[NK_MAX_GAMEPADS];

    // Horloge pour rapport de santA
    const NkElapsedTime t0 = NkChrono::Now();
    float health           = 0.f;

    // L'injection synthAtique est dAsactivAe car Enqueue n'existe pas.
    constexpr int INJECT_PER_FRAME = 0; // dAsactivA

    NkChrono chrono;
    NkElapsedTime elapsed{};

    logger.Info("[ex06] Stress test desactive (Enqueue non publique).");
    logger.Info("[ex06] Connect a gamepad for full coverage");
    logger.Info("[ex06] ESC or close window to exit");

    while (running && window.IsOpen())
    {
        NkElapsedTime e = chrono.Reset();

        // -------------------------------------------------------------------
        // Pomper - avec gestion Close/Resize/Key
        // -------------------------------------------------------------------
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr) {
            ++stats.polled;
            if (ev->Is<NkWindowCloseEvent>()) {
                running = false; break;
            }
            if (auto* r = ev->As<NkWindowResizeEvent>())
                renderer.Resize(r->GetWidth(), r->GetHeight());
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                if (k->GetKey() == NkKey::NK_ESCAPE)
                    running = false;
            }
        }
        if (!running) break;

        // -------------------------------------------------------------------
        // Polling gamepad - polling direct + event-driven dAjA  actif via CB
        // -------------------------------------------------------------------
        for (uint32 gpIdx = 0; gpIdx < NK_MAX_GAMEPADS; ++gpIdx) {
            PollGamepad(gpIdx, gpStates[gpIdx], stats);
        }

        static int gpPrint = 0;
        if (++gpPrint % 120 == 0) {
            for (uint32 gpIdx = 0; gpIdx < NK_MAX_GAMEPADS; ++gpIdx) {
                if (!gpStates[gpIdx].connected) continue;
                logger.Info("[GP#{0}] LX={1} LY={2} RX={3} RY={4} LT={5} RT={6}",
                    gpIdx,
                    gpStates[gpIdx].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LX)],
                    gpStates[gpIdx].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LY)],
                    gpStates[gpIdx].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RX)],
                    gpStates[gpIdx].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RY)],
                    gpStates[gpIdx].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_LT)],
                    gpStates[gpIdx].axes[static_cast<uint32>(NkGamepadAxis::NK_GP_AXIS_RT)]);
            }
        }

        // -------------------------------------------------------------------
        // Delta-time + rapport santA
        // -------------------------------------------------------------------
        float dt = SafeDt(static_cast<float>(elapsed.seconds));
        time   += dt;
        health += dt;

        if (health >= 1.0f) {
            const float sec = static_cast<float>((NkChrono::Now() - t0).seconds);
            stats.Print(sec);
            health = 0.f;
        }

        // -------------------------------------------------------------------
        // Rendu
        // -------------------------------------------------------------------
        renderer.BeginFrame(NkRenderer::PackColor(4, 6, 12, 255));
        const auto& fb = renderer.GetFramebufferInfo();
        uint32 w = fb.width  ? fb.width  : 1280u;
        uint32 h = fb.height ? fb.height : 720u;
        DrawPlasma(renderer, w, h, time);

        // Overlay "stress" - barre rouge proportionnelle aux drops estimAs (inactif)
        renderer.EndFrame();
        renderer.Present();

        // -------------------------------------------------------------------
        // Cap 60fps
        // -------------------------------------------------------------------
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    // Final stats
    const float sec = static_cast<float>((NkChrono::Now() - t0).seconds);
    logger.Info("=== FINAL STATS ===");
    stats.Print(sec);

    renderer.Shutdown();
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}

