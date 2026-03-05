// =============================================================================
// ex05_multi_window.cpp
// Multi-fenêtres — deux fenêtres indépendantes, events distingués par windowId
//
// Couvre : NkWindow x2, NkEventSystem multi-fenêtre, NkEvent::GetWindowId(),
//          WM_DESTROY conditionnel (PostQuitMessage seulement si 0 fenêtres),
//          rendu plasma avec couleur de teinte par fenêtre,
//          fermeture individuelle sans tuer l'autre.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKRenderer/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Ajout pour NkChrono et NkElapsedTime

#include <cstdio>
#include <cmath>
#include <array>
#include <string>

namespace {
using namespace nkentseu;

static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

// ---------------------------------------------------------------------------
// État par fenêtre
// ---------------------------------------------------------------------------
struct WinState {
    NkWindow*  window    = nullptr;
    NkRenderer renderer;
    NkWindowId id        = 0;
    bool       open      = false;
    bool       neon      = false;
    float      saturation = 1.15f;
    float      time       = 0.f;
    float      timeScale  = 1.f;
    float      phaseX     = 0.f;
    float      phaseY     = 0.f;
    float      hueShift   = 0.f;  // offset de teinte (0.0 / 0.33 / 0.66)
    std::string title;
};

// ---------------------------------------------------------------------------
// Plasma avec décalage de teinte par fenêtre
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, NkU32 w, NkU32 h,
                        float t, float px, float py,
                        float sat, float hue)
{
    if (!w || !h) return;
    const NkU32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (NkU32 y = 0; y < h; y += blk) {
        float fy = y/(float)h - 0.5f;
        for (NkU32 x = 0; x < w; x += blk) {
            float fx  = x/(float)w - 0.5f;
            float rd  = std::sqrt(fx*fx + fy*fy);
            float mix = (std::sin((fx+px)*13.5f + t*1.7f)
                        +std::sin((fy+py)*11.f  - t*1.3f)
                        +std::sin(rd*24.f        - t*2.1f)) * 0.333f;
            NkU8 ri = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+hue+0.f  ))-0.5f)*sat+0.5f)*255);
            NkU8 gi = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+hue+0.33f))-0.5f)*sat+0.5f)*255);
            NkU8 bi = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+hue+0.66f))-0.5f)*sat+0.5f)*255);
            NkU32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (NkU32 by=0;by<blk&&(y+by)<h;++by)
                for (NkU32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((NkI32)(x+bx),(NkI32)(y+by),col);
        }
    }
}

// ---------------------------------------------------------------------------
// Traiter les événements d'une fenêtre donnée
// ---------------------------------------------------------------------------
static void ProcessWindowEvent(NkEvent* ev, WinState& ws)
{
    if (!ev) return;

    NkEventDispatcher d(ev);

    // Fermeture individuelle
    if (auto* e = ev->As<NkWindowCloseEvent>()) {
        (void)e;
        std::printf("[Win:%s] Close\n", ws.title.c_str());
        ws.open = false;
        ws.window->Close();
        return;
    }

    // Resize
    if (auto* e = ev->As<NkWindowResizeEvent>()) {
        ws.renderer.Resize(e->GetWidth(), e->GetHeight());
        std::printf("[Win:%s] Resize → %ux%u\n",
            ws.title.c_str(), e->GetWidth(), e->GetHeight());
        return;
    }

    // Clavier
    if (auto* e = ev->As<NkKeyPressEvent>()) {
        switch (e->GetKey()) {
            case NkKey::NK_SPACE:
                ws.neon = !ws.neon;
                std::printf("[Win:%s] Neon %s\n",
                    ws.title.c_str(), ws.neon ? "ON" : "OFF");
                break;
            case NkKey::NK_UP:
                ws.saturation = std::fmin(3.f, ws.saturation + 0.1f);
                break;
            case NkKey::NK_DOWN:
                ws.saturation = std::fmax(0.1f, ws.saturation - 0.1f);
                break;
            case NkKey::NK_LEFT:
                ws.timeScale  = std::fmax(0.1f, ws.timeScale  - 0.1f);
                break;
            case NkKey::NK_RIGHT:
                ws.timeScale  = std::fmin(5.f,  ws.timeScale  + 0.1f);
                break;
            default: break;
        }
        return;
    }

    // Souris — déplacer la phase
    if (auto* e = ev->As<NkMouseMoveEvent>()) {
        if (e->GetButtons().IsDown(NkMouseButton::NK_MB_LEFT)) {
            ws.phaseX += e->GetDeltaX() * 0.003f;
            ws.phaseY += e->GetDeltaY() * 0.003f;
        }
        return;
    }

    if (auto* e = ev->As<NkMouseWheelVerticalEvent>()) {
        ws.saturation = std::fmax(0.1f, std::fmin(3.f,
            ws.saturation + e->GetDeltaY() * 0.1f));
        return;
    }

    // Focus
    if (auto* e = ev->As<NkWindowFocusGainedEvent>()) {
        std::printf("[Win:%s] Focus gained\n", ws.title.c_str());
        return;
    }
    if (auto* e = ev->As<NkWindowFocusLostEvent>()) {
        std::printf("[Win:%s] Focus lost\n", ws.title.c_str());
        return;
    }

    // Drag & Drop — fenêtre A seulement
    if (auto* e = ev->As<NkDropFileEvent>()) {
        std::printf("[Win:%s] Drop %zu file(s):\n",
            ws.title.c_str(), e->data.paths.size());   // CORRECTION
        for (const auto& f : e->data.paths)            // CORRECTION
            std::printf("  %s\n", f.c_str());
        return;
    }
}

} // namespace

// =============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    // 1. Init
    if (!NkInitialise({ .appName = "ex05 Multi-Window" })) return -1;

    // 2. Deux fenêtres
    std::array<WinState, 2> wins;

    // Fenêtre A — rouge/plasma chaud, en haut à gauche
    NkWindowConfig cfgA;
    cfgA.title       = "ex05 — Fenêtre A (rouge)";
    cfgA.width       = 640;
    cfgA.height      = 480;
    cfgA.x           = 80;
    cfgA.y           = 100;
    cfgA.resizable   = true;
    cfgA.dropEnabled = true;

    // Fenêtre B — cyan/plasma froid, en bas à droite
    NkWindowConfig cfgB;
    cfgB.title       = "ex05 — Fenêtre B (cyan)";
    cfgB.width       = 640;
    cfgB.height      = 480;
    cfgB.x           = 820;
    cfgB.y           = 300;
    cfgB.resizable   = true;
    cfgB.dropEnabled = false;

    NkWindow windowA(cfgA);
    NkWindow windowB(cfgB);

    if (!windowA.IsOpen() || !windowB.IsOpen()) {
        std::fprintf(stderr, "[ex05] Window creation failed\n");
        NkClose();
        return -2;
    }

    // 3. Renderer pour chaque fenêtre
    NkRendererConfig rcfg;
    rcfg.api                   = NkRendererApi::NK_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;

    // Init wins[0]
    wins[0].window    = &windowA;
    wins[0].id        = windowA.GetId();
    wins[0].open      = true;
    wins[0].hueShift  = 0.0f;   // rouge dominant
    wins[0].timeScale = 1.0f;
    wins[0].title     = "A";
    if (!wins[0].renderer.Create(windowA, rcfg)) { NkClose(); return -3; }

    // Init wins[1]
    wins[1].window    = &windowB;
    wins[1].id        = windowB.GetId();
    wins[1].open      = true;
    wins[1].hueShift  = 0.33f;  // cyan dominant
    wins[1].timeScale = 0.7f;
    wins[1].title     = "B";
    if (!wins[1].renderer.Create(windowB, rcfg)) { NkClose(); return -4; }

    // CORRECTION : format %llu pour NkWindowId (unsigned long long)
    std::printf("[ex05] Window A id=%llu, Window B id=%llu\n",
        static_cast<unsigned long long>(wins[0].id),
        static_cast<unsigned long long>(wins[1].id));
    std::printf("[ex05] Controls: SPACE=neon, UP/DOWN=saturation, "
                "LEFT/RIGHT=speed, LMB drag=phase, scroll=sat\n");
    std::printf("[ex05] Close one window independently — the other keeps running\n");

    auto& es = NkEvents();

    NkChrono chrono;
    NkElapsedTime elapsed;

    // 4. Boucle principale
    while (wins[0].open || wins[1].open)
    {
        chrono.Reset();

        // -----------------------------------------------------------------------
        // Pomper TOUS les events et les router par windowId
        // -----------------------------------------------------------------------
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            NkWindowId wid = ev->GetWindowId();

            // Touche ESC globale — ferme tout
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                if (k->GetKey() == NkKey::NK_ESCAPE) {
                    std::printf("[ex05] ESC — closing all windows\n");
                    for (auto& w : wins) {
                        w.open = false;
                        if (w.window) w.window->Close();
                    }
                    break;
                }
            }

            // Router vers la bonne fenêtre
            bool dispatched = false;
            for (auto& ws : wins) {
                if (ws.open && ws.id == wid) {
                    ProcessWindowEvent(ev, ws);
                    dispatched = true;
                    break;
                }
            }

            // Event sans fenêtre associée (ex: gamepad connect)
            if (!dispatched) {
                if (auto* e = ev->As<NkGamepadConnectEvent>()) {
                    // CORRECTION : pas de IsConnected(), on affiche simplement "connected"
                    std::printf("[ex05] Gamepad connected #%u (global)\n",
                        e->GetGamepadIndex());
                }
                if (auto* e = ev->As<NkGamepadDisconnectEvent>()) {
                    std::printf("[ex05] Gamepad disconnected #%u (global)\n",
                        e->GetGamepadIndex());
                }
            }
        }

        // Vérifier si une fenêtre a été fermée par le bouton X
        for (auto& ws : wins) {
            if (ws.open && ws.window && !ws.window->IsOpen()) {
                ws.open = false;
                std::printf("[ex05] Window %s closed externally\n", ws.title.c_str());
            }
        }

        if (!wins[0].open && !wins[1].open) break;

        // -----------------------------------------------------------------------
        // Delta-time
        // -----------------------------------------------------------------------
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;

        // -----------------------------------------------------------------------
        // Rendu de chaque fenêtre ouverte
        // -----------------------------------------------------------------------
        for (auto& ws : wins)
        {
            if (!ws.open || !ws.window || !ws.window->IsOpen()) continue;

            ws.time += dt * ws.timeScale * (ws.neon ? 1.8f : 1.0f);

            ws.renderer.BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));

            const auto& fb = ws.renderer.GetFramebufferInfo();
            NkU32 w = fb.width  ? fb.width  : 640u;
            NkU32 h = fb.height ? fb.height : 480u;

            DrawPlasma(ws.renderer, w, h,
                       ws.time, ws.phaseX, ws.phaseY,
                       ws.saturation, ws.hueShift);

            ws.renderer.EndFrame();
            ws.renderer.Present();
        }

        // -----------------------------------------------------------------------
        // Cap 60fps
        // -----------------------------------------------------------------------
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    // 5. Cleanup
    for (auto& ws : wins)
        ws.renderer.Shutdown();

    NkClose();
    return 0;
}