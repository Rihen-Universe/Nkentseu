// =============================================================================
// ex06_stress_ringbuffer_gamepad.cpp
// Stress test ring buffer + couverture complète gamepad
//
// Couvre : NkEventRingBuffer (drop-oldest), injection synthétique d'events,
//          NkGamepadSystem::PollGamepads / GetAxis / IsButtonDown / Rumble,
//          NkGamepadButtonReleaseEvent, NkGamepadAxisEvent dead-zone adaptative,
//          comptage d'events droppés, report de santé chaque seconde.
//
// ATTENTION : L'injection synthétique nécessite une méthode Enqueue() non publique.
//             Cette partie a été désactivée ; l'exemple teste uniquement le gamepad.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKRenderer/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Chemin correct pour NkChrono et NkElapsedTime

#include <cstdio>
#include <cmath>
#include <cstring>
#include <atomic>
#include <chrono>

namespace {
using namespace nkentseu;

static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

// ---------------------------------------------------------------------------
// Stats globales thread-safe (remplies depuis callbacks)
// ---------------------------------------------------------------------------
struct Stats {
    std::atomic<NkU64> pushed   { 0 };  // events injectés
    std::atomic<NkU64> polled   { 0 };  // events récupérés via PollEvent
    std::atomic<NkU64> dropped  { 0 };  // estimé : pushed - polled (en excès)
    std::atomic<NkU64> gpAxis   { 0 };
    std::atomic<NkU64> gpBtn    { 0 };
    std::atomic<NkU64> gpRumble { 0 };

    void Print(float elapsed) const {
        std::printf(
            "[HEALTH t=%.1fs] pushed=%llu polled=%llu diff=%lld "
            "gpAxis=%llu gpBtn=%llu rumble=%llu\n",
            elapsed,
            (unsigned long long)pushed.load(),
            (unsigned long long)polled.load(),
            (long long)(pushed.load() - polled.load()),
            (unsigned long long)gpAxis.load(),
            (unsigned long long)gpBtn.load(),
            (unsigned long long)gpRumble.load());
    }
};

// ---------------------------------------------------------------------------
// Plasma de fond
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, NkU32 w, NkU32 h, float t)
{
    if (!w || !h) return;
    const NkU32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (NkU32 y = 0; y < h; y += blk) {
        float fy = y/(float)h - 0.5f;
        for (NkU32 x = 0; x < w; x += blk) {
            float fx  = x/(float)w - 0.5f;
            float rd  = std::sqrt(fx*fx + fy*fy);
            float mix = (std::sin(fx*13.5f+t*1.7f)
                        +std::sin(fy*11.f -t*1.3f)
                        +std::sin(rd*24.f -t*2.1f)) * 0.333f;
            NkU8 ri = (NkU8)(ClampUnit(0.5f+0.5f*std::sin(6.28f*(mix+0.f  )))*255);
            NkU8 gi = (NkU8)(ClampUnit(0.5f+0.5f*std::sin(6.28f*(mix+0.33f)))*255);
            NkU8 bi = (NkU8)(ClampUnit(0.5f+0.5f*std::sin(6.28f*(mix+0.66f)))*255);
            NkU32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (NkU32 by=0;by<blk&&(y+by)<h;++by)
                for (NkU32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((NkI32)(x+bx),(NkI32)(y+by),col);
        }
    }
}

// ---------------------------------------------------------------------------
// Gamepads — polling explicite de tous les axes et boutons
// ---------------------------------------------------------------------------
struct GpState {
    float axes[static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX)] = {};
    bool  buttons[static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX)] = {};
    bool  connected   = false;
};

static void PollGamepad(NkU32 idx, GpState& gs, Stats& stats)
{
    auto& gp = NkGamepads();   // Utilisation de NkGamepads()
    gs.connected = gp.IsConnected(idx);
    if (!gs.connected) return;

    for (NkU32 ai = 0; ai < static_cast<NkU32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX); ++ai) {
        const NkGamepadAxis axis = static_cast<NkGamepadAxis>(ai);
        float v = gp.GetAxis(idx, axis);
        if (std::fabs(v - gs.axes[ai]) > 0.001f) {
            gs.axes[ai] = v;
            ++stats.gpAxis;
        }
    }

    for (NkU32 bi = static_cast<NkU32>(NkGamepadButton::NK_GP_SOUTH);
         bi < static_cast<NkU32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX); ++bi) {
        const NkGamepadButton btn = static_cast<NkGamepadButton>(bi);
        bool down = gp.IsButtonDown(idx, btn);
        if (down != gs.buttons[bi]) {
            gs.buttons[bi] = down;
            ++stats.gpBtn;
            if (down) {
                std::printf("[GP#%u] Button %u (%s) pressed\n",
                    idx, bi, NkGamepadButtonToString(btn));
                // Rumble sur chaque nouveau bouton pressé
                gp.Rumble(idx,
                    0.1f + static_cast<float>(bi) * 0.02f,
                    0.2f + static_cast<float>(bi) * 0.015f,
                    0.f, 0.f, 40);
                ++stats.gpRumble;
            }
        }
    }

    // Stick gauche contrôle la fréquence de rumble continu
    float lx = gs.axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_LX)];
    float rt = gs.axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_RT)];
    if (rt > 0.5f && std::fabs(lx) > 0.2f) {
        gp.Rumble(idx, std::fabs(lx)*0.4f, rt*0.6f, 0.f, 0.f, 16);
        ++stats.gpRumble;
    }
}

} // namespace

// =============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    // 1. Init
    if (!NkInitialise({ .appName = "ex06 Stress RingBuffer + Gamepad" })) return -1;

    // 2. Fenêtre
    NkWindowConfig cfg;
    cfg.title     = "ex06 — Stress Ring Buffer + Full Gamepad";
    cfg.width     = 1280;
    cfg.height    = 720;
    cfg.centered  = true;
    cfg.resizable = true;

    NkWindow window(cfg);
    if (!window.IsOpen()) { NkClose(); return -2; }

    // 3. Renderer
    NkRendererConfig rcfg;
    rcfg.api                   = NkRendererApi::NK_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;
    NkRenderer renderer;
    if (!renderer.Create(window, rcfg)) { NkClose(); return -3; }

    // 4. Gamepad callbacks (event-driven)
    // CORRECTION : la lambda doit correspondre à NkGamepadConnectCallback (const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            std::printf("[Gamepad] %s #%u (%s)\n",
                connected ? "CONNECTED" : "DISCONNECTED",
                info.index, info.name);
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
            std::printf("[GP Event] ButtonPress %d idx=%u\n",
                (int)ev->GetButton(), ev->GetGamepadIndex());
        });
    es.AddEventCallback<NkGamepadButtonReleaseEvent>(
        [&stats](NkGamepadButtonReleaseEvent* ev) {
            ++stats.gpBtn;
            std::printf("[GP Event] ButtonRelease %d idx=%u\n",
                (int)ev->GetButton(), ev->GetGamepadIndex());
        });

    // 5. État
    bool    running  = true;
    float   time     = 0.f;
    GpState gpStates[NK_MAX_GAMEPADS];

    // Horloge pour rapport de santé
    auto t0      = std::chrono::steady_clock::now();
    float health = 0.f;

    // L'injection synthétique est désactivée car Enqueue n'existe pas.
    constexpr int INJECT_PER_FRAME = 0; // désactivé

    NkChrono chrono;
    NkElapsedTime elapsed;

    std::printf("[ex06] Stress test désactivé (Enqueue non publique).\n");
    std::printf("[ex06] Connect a gamepad for full coverage\n");
    std::printf("[ex06] ESC or close window to exit\n");

    while (running && window.IsOpen())
    {
        chrono.Reset();

        // -------------------------------------------------------------------
        // Pomper — avec gestion Close/Resize/Key
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
        // Polling gamepad — polling direct + event-driven déjà actif via CB
        // -------------------------------------------------------------------
        for (NkU32 gpIdx = 0; gpIdx < NK_MAX_GAMEPADS; ++gpIdx) {
            PollGamepad(gpIdx, gpStates[gpIdx], stats);
        }

        static int gpPrint = 0;
        if (++gpPrint % 120 == 0) {
            for (NkU32 gpIdx = 0; gpIdx < NK_MAX_GAMEPADS; ++gpIdx) {
                if (!gpStates[gpIdx].connected) continue;
                std::printf("[GP#%u] LX=%.2f LY=%.2f RX=%.2f RY=%.2f "
                            "LT=%.2f RT=%.2f\n",
                    gpIdx,
                    gpStates[gpIdx].axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_LX)],
                    gpStates[gpIdx].axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_LY)],
                    gpStates[gpIdx].axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_RX)],
                    gpStates[gpIdx].axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_RY)],
                    gpStates[gpIdx].axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_LT)],
                    gpStates[gpIdx].axes[static_cast<NkU32>(NkGamepadAxis::NK_GP_AXIS_RT)]);
            }
        }

        // -------------------------------------------------------------------
        // Delta-time + rapport santé
        // -------------------------------------------------------------------
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;
        time   += dt;
        health += dt;

        if (health >= 1.0f) {
            auto now  = std::chrono::steady_clock::now();
            float sec = std::chrono::duration<float>(now - t0).count();
            stats.Print(sec);
            health = 0.f;
        }

        // -------------------------------------------------------------------
        // Rendu
        // -------------------------------------------------------------------
        renderer.BeginFrame(NkRenderer::PackColor(4, 6, 12, 255));
        const auto& fb = renderer.GetFramebufferInfo();
        NkU32 w = fb.width  ? fb.width  : 1280u;
        NkU32 h = fb.height ? fb.height : 720u;
        DrawPlasma(renderer, w, h, time);

        // Overlay "stress" — barre rouge proportionnelle aux drops estimés (inactif)
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
    auto now  = std::chrono::steady_clock::now();
    float sec = std::chrono::duration<float>(now - t0).count();
    std::printf("\n=== FINAL STATS ===\n");
    stats.Print(sec);

    renderer.Shutdown();
    NkClose();
    return 0;
}
