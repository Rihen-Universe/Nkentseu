// =============================================================================
// ex04_pattern_c_callbacks.cpp
// Pattern C — Callbacks permanents (AddEventCallback<T>)
//
// Couvre : AddEventCallback<T>, abonnements à durée de vie limitée,
//          NkGamepadSystem callbacks (connect, button, axis),
//          NkWindowCloseEvent, NkWindowResizeEvent, NkKeyPressEvent,
//          NkMouseMotionEvent, NkMouseWheelEvent, NkDropFileEvent,
//          NkDropTextEvent, NkGamepadAxisEvent, NkGamepadButtonPressEvent.
//
// ATTENTION : Le système de désabonnement par token n'est pas implémenté.
//             La partie "unsubscribe after 3 resizes" a été commentée.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKRenderer/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Ajout pour NkChrono et NkElapsedTime

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <atomic>

namespace {
using namespace nkentseu;

static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

// ---------------------------------------------------------------------------
// État partagé — modifié par les callbacks depuis différents "sous-systèmes"
// ---------------------------------------------------------------------------
struct SharedState {
    std::atomic<bool>  running { true };
    std::atomic<bool>  neon    { false };
    std::atomic<bool>  fullscreen { false };
    float  saturation = 1.15f;
    float  phaseX     = 0.f;
    float  phaseY     = 0.f;
    float  time       = 0.f;
    NkU32  viewW      = 1280;
    NkU32  viewH      = 720;

    // Historique drops
    std::vector<std::string> droppedFiles;
    std::string              droppedText;

    // Compteur d'events par type (stats)
    std::atomic<NkU32> keyCount   { 0 };
    std::atomic<NkU32> mouseCount { 0 };
    std::atomic<NkU32> dropCount  { 0 };
    std::atomic<NkU32> gpCount    { 0 };
};

// ---------------------------------------------------------------------------
// Plasma visuel
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, NkU32 w, NkU32 h,
                        float t, float px, float py, float sat)
{
    if (!w || !h) return;
    const NkU32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (NkU32 y = 0; y < h; y += blk) {
        float fy = y / (float)h - 0.5f;
        for (NkU32 x = 0; x < w; x += blk) {
            float fx  = x / (float)w - 0.5f;
            float rd  = std::sqrt(fx*fx + fy*fy);
            float mix = (std::sin((fx+px)*13.5f+t*1.7f)
                        +std::sin((fy+py)*11.f -t*1.3f)
                        +std::sin(rd*24.f      -t*2.1f)) * 0.333f;
            NkU8 ri = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+0.f  ))-0.5f)*sat+0.5f)*255);
            NkU8 gi = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+0.33f))-0.5f)*sat+0.5f)*255);
            NkU8 bi = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+0.66f))-0.5f)*sat+0.5f)*255);
            NkU32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (NkU32 by=0;by<blk&&(y+by)<h;++by)
                for (NkU32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((NkI32)(x+bx),(NkI32)(y+by),col);
        }
    }
}

} // namespace

// =============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    // 1. Init
    if (!NkInitialise({ .appName = "ex04 Pattern C — Callbacks" })) return -1;

    // 2. Fenêtre
    NkWindowConfig cfg;
    cfg.title       = "ex04 — Pattern C : Callbacks permanents";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;

    NkWindow window(cfg);
    if (!window.IsOpen()) { NkClose(); return -2; }

    // 3. Renderer
    NkRendererConfig rcfg;
    rcfg.api                   = NkRendererApi::NK_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;
    NkRenderer renderer;
    if (!renderer.Create(window, rcfg)) { NkClose(); return -3; }

    // 4. État partagé entre tous les callbacks
    SharedState state;
    state.viewW = cfg.width;
    state.viewH = cfg.height;

    auto& es = NkEvents();

    // =========================================================================
    // 5. ABONNEMENTS
    //
    //    Chaque sous-système (ici simulé par un bloc commenté) s'abonne
    //    indépendamment et ne voit que le type d'event qui l'intéresse.
    // =========================================================================

    // ── Sous-système "Window Manager" ────────────────────────────────────────

    es.AddEventCallback<NkWindowCloseEvent>(
        [&state](NkWindowCloseEvent* /*ev*/) {
            std::printf("[CB:WindowMgr] Close event\n");
            state.running = false;
        });

    es.AddEventCallback<NkWindowResizeEvent>(
        [&state, &renderer](NkWindowResizeEvent* ev) {
            state.viewW = ev->GetWidth();
            state.viewH = ev->GetHeight();
            renderer.Resize(ev->GetWidth(), ev->GetHeight());
            std::printf("[CB:WindowMgr] Resize → %ux%u\n",
                ev->GetWidth(), ev->GetHeight());
        });

    es.AddEventCallback<NkWindowFocusGainedEvent>(
        [](NkWindowFocusGainedEvent* /*ev*/) {
            std::printf("[CB:WindowMgr] Focus gained\n");
        });

    es.AddEventCallback<NkWindowFocusLostEvent>(
        [](NkWindowFocusLostEvent* /*ev*/) {
            std::printf("[CB:WindowMgr] Focus lost\n");
        });

    es.AddEventCallback<NkWindowMoveEvent>(
        [](NkWindowMoveEvent* ev) {
            std::printf("[CB:WindowMgr] Moved (%d,%d)\n", ev->GetX(), ev->GetY());
        });

    // ── Sous-système "Input/Keyboard" ────────────────────────────────────────

    es.AddEventCallback<NkKeyPressEvent>(
        [&state, &window](NkKeyPressEvent* ev) {
            ++state.keyCount;
            switch (ev->GetKey()) {
                case NkKey::NK_ESCAPE:
                    state.running = false;
                    break;
                case NkKey::NK_F11:
                    state.fullscreen = !state.fullscreen.load();
                    window.SetFullscreen(state.fullscreen.load());
                    break;
                case NkKey::NK_SPACE:
                    state.neon = !state.neon.load();
                    std::printf("[CB:Input] Neon %s\n",
                        state.neon.load() ? "ON" : "OFF");
                    break;
                case NkKey::NK_R:
                    state.phaseX = state.phaseY = 0.f;
                    state.saturation = 1.15f;
                    std::printf("[CB:Input] Reset\n");
                    break;
                default:
                    break;
            }
        });

    es.AddEventCallback<NkKeyReleaseEvent>(
        [](NkKeyReleaseEvent* ev) {
            std::printf("[CB:Input] Key released key=%d\n", (int)ev->GetKey());
        });

    es.AddEventCallback<NkTextInputEvent>(
        [](NkTextInputEvent* ev) {
            std::printf("[CB:Input] Typed codepoint=U+%04X\n", ev->GetCodepoint());
        });

    // ── Sous-système "Mouse" ─────────────────────────────────────────────────

    es.AddEventCallback<NkMouseMoveEvent>(
        [&state](NkMouseMoveEvent* ev) {
            ++state.mouseCount;
            // Phase uniquement si bouton gauche tenu
            if (ev->GetButtons().IsDown(NkMouseButton::NK_MB_LEFT)) {
                state.phaseX += ev->GetDeltaX() * 0.002f;
                state.phaseY += ev->GetDeltaY() * 0.002f;
            }
        });

    es.AddEventCallback<NkMouseWheelVerticalEvent>(
        [&state](NkMouseWheelVerticalEvent* ev) {
            state.saturation = std::fmax(0.1f, std::fmin(3.f,
                state.saturation + ev->GetDeltaY() * 0.12f));
            std::printf("[CB:Mouse] Wheel dy=%.2f sat=%.2f\n",
                ev->GetDeltaY(), state.saturation);
        });

    es.AddEventCallback<NkMouseButtonPressEvent>(
        [](NkMouseButtonPressEvent* ev) {
            std::printf("[CB:Mouse] Press btn=%d at (%d,%d)\n",
                (int)ev->GetButton(), ev->GetX(), ev->GetY());
        });

    es.AddEventCallback<NkMouseEnterEvent>(
        [](NkMouseEnterEvent* /*ev*/) {
            std::printf("[CB:Mouse] Enter window\n");
        });

    es.AddEventCallback<NkMouseLeaveEvent>(
        [](NkMouseLeaveEvent* /*ev*/) {
            std::printf("[CB:Mouse] Leave window\n");
        });

    // ── Sous-système "Drag & Drop" ────────────────────────────────────────────

    es.AddEventCallback<NkDropEnterEvent>(
        [](NkDropEnterEvent* ev) {
            std::printf("[CB:DnD] Enter pos=(%d,%d) files=%u text=%s\n",
                ev->data.x, ev->data.y,
                ev->data.numFiles,
                ev->data.hasText ? "yes" : "no");
        });

    es.AddEventCallback<NkDropLeaveEvent>(
        [](NkDropLeaveEvent* /*ev*/) {
            std::printf("[CB:DnD] Leave\n");
        });

    es.AddEventCallback<NkDropFileEvent>(
        [&state](NkDropFileEvent* ev) {
            ++state.dropCount;
            state.droppedFiles = ev->data.paths;
            std::printf("[CB:DnD] Files (%zu):\n", ev->data.paths.size());
            for (const auto& f : ev->data.paths)
                std::printf("  %s\n", f.c_str());
        });

    es.AddEventCallback<NkDropTextEvent>(
        [&state](NkDropTextEvent* ev) {
            ++state.dropCount;
            state.droppedText = ev->data.text;
            std::printf("[CB:DnD] Text [%.60s]\n", ev->data.text.c_str());
        });

    // ── Sous-système "Gamepad" ────────────────────────────────────────────────

    // Callback connexion (hors event system — NkGamepadSystem direct)
    // CORRECTION : signature attendue (const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            std::printf("[CB:Gamepad] %s #%u (%s)\n",
                connected ? "CONNECTED" : "DISCONNECTED",
                info.index, info.name);
        });

    // Événement de connexion (NkGamepadConnectEvent)
    es.AddEventCallback<NkGamepadConnectEvent>(
        [](NkGamepadConnectEvent* ev) {
            std::printf("[CB:Gamepad] ConnectEvent idx=%u\n",
                ev->GetGamepadIndex());
        });

    // Événement de déconnexion (NkGamepadDisconnectEvent)
    es.AddEventCallback<NkGamepadDisconnectEvent>(
        [](NkGamepadDisconnectEvent* ev) {
            std::printf("[CB:Gamepad] DisconnectEvent idx=%u\n",
                ev->GetGamepadIndex());
        });

    es.AddEventCallback<NkGamepadButtonPressEvent>(
        [&state](NkGamepadButtonPressEvent* ev) {
            ++state.gpCount;
            std::printf("[CB:Gamepad] Button=%d idx=%u\n",
                (int)ev->GetButton(), ev->GetGamepadIndex());
            if (ev->GetButton() == NkGamepadButton::NK_GP_SOUTH) {
                state.neon = !state.neon.load();
                NkGamepads().Rumble(
                    ev->GetGamepadIndex(), 0.25f, 0.5f, 0.f, 0.f, 80);
            }
            if (ev->GetButton() == NkGamepadButton::NK_GP_EAST) {
                state.phaseX = state.phaseY = 0.f;
                state.saturation = 1.15f;
            }
        });

    es.AddEventCallback<NkGamepadAxisEvent>(
        [&state](NkGamepadAxisEvent* ev) {
            float v = ev->GetValue();
            if (std::fabs(v) < 0.12f) return; // dead-zone

            switch (ev->GetAxis()) {
                case NkGamepadAxis::NK_GP_AXIS_LX:
                    state.phaseX += v * 0.025f;
                    break;
                case NkGamepadAxis::NK_GP_AXIS_LY:
                    state.phaseY += v * 0.025f;
                    break;
                case NkGamepadAxis::NK_GP_AXIS_RT:
                    state.saturation = 1.f + ClampUnit(v) * 0.8f;
                    break;
                default:
                    break;
            }
        });

    // ── Abonnement à durée limitée — se désinscrit après 3 redimensionnements
    //     (Version simplifiée : on utilise un compteur et on retire tous les callbacks de resize après 3)
    {
        static int resizeCount = 0;
        es.AddEventCallback<NkWindowResizeEvent>(
            [&es](NkWindowResizeEvent* ev) {
                ++resizeCount;
                std::printf("[CB:OneShotx3] Resize #%d → %ux%u\n",
                    resizeCount, ev->GetWidth(), ev->GetHeight());
                if (resizeCount >= 3) {
                    std::printf("[CB:OneShotx3] Unsubscribing all resize callbacks after 3 resizes\n");
                    es.ClearEventCallbacks<NkWindowResizeEvent>();
                }
            });
    }

    // =========================================================================
    // 6. Boucle principale
    // =========================================================================

    NkChrono chrono;
    NkElapsedTime elapsed;

    while (state.running.load() && window.IsOpen())
    {
        chrono.Reset();

        // Pomper la queue — les callbacks s'exécutent ici.
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr) {
            // Les callbacks ont déjà été appelés via DispatchEvent interne.
            (void)ev;
        }

        if (!state.running.load() || !window.IsOpen()) break;

        // --- Polling souris pour drift continu (complément aux callbacks)
        // Désactivé car NkInput n'est pas disponible dans ce contexte.
        // Si vous avez besoin de cette fonctionnalité, incluez NkInput.h et assurez-vous qu'il est défini.
        // /*
        if (NkInput.IsMouseDown(NkMouseButton::NK_MB_RIGHT)) {
            NkVec2i d = {NkInput.MouseDeltaX(), NkInput.MouseDeltaY()};
            state.phaseX -= d.x * 0.001f;
            state.phaseY -= d.y * 0.001f;
        }
        // */

        // --- Delta-time
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        state.time += dt * (state.neon.load() ? 1.8f : 1.0f);

        // --- Rendu
        renderer.BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));
        const auto& fb = renderer.GetFramebufferInfo();
        NkU32 w = fb.width  ? fb.width  : state.viewW;
        NkU32 h = fb.height ? fb.height : state.viewH;
        DrawPlasma(renderer, w, h, state.time,
                   state.phaseX, state.phaseY, state.saturation);
        renderer.EndFrame();
        renderer.Present();

        // --- Cap 60fps
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    std::printf("[Stats] keys=%u mouse=%u drops=%u gamepad=%u\n",
        state.keyCount.load(), state.mouseCount.load(),
        state.dropCount.load(), state.gpCount.load());

    renderer.Shutdown();
    NkClose();
    return 0;
}