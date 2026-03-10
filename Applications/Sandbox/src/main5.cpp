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
#include "NKMath/NKMath.h"

#include "NKLogger/NkLog.h"

#include <cmath>
#include <string>
#include <vector>

#ifdef NkMin
#undef NkMin
#endif
#ifdef NkMax
#undef NkMax
#endif
#ifdef NkClamp
#undef NkClamp
#endif

namespace {
using namespace nkentseu;

static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }
static float SafeDt(float dt) {
    if (!std::isfinite(dt) || dt <= 0.f || dt > 0.25f) return 1.f / 60.f;
    return dt;
}

// ---------------------------------------------------------------------------
// État partagé — modifié par les callbacks depuis différents "sous-systèmes"
// ---------------------------------------------------------------------------
struct SharedState {
    NkAtomicBool  running { true };
    NkAtomicBool  neon    { false };
    NkAtomicBool  fullscreen { false };
    float  saturation = 1.15f;
    float  phaseX     = 0.f;
    float  phaseY     = 0.f;
    float  time       = 0.f;
    NkU32  viewW      = 1280;
    NkU32  viewH      = 720;

    // Historique drops
    NkVector<NkString> droppedFiles;
    NkString              droppedText;

    // Compteur d'events par type (stats)
    NkAtomic<NkU32> keyCount   { 0 };
    NkAtomic<NkU32> mouseCount { 0 };
    NkAtomic<NkU32> dropCount  { 0 };
    NkAtomic<NkU32> gpCount    { 0 };
};

// ---------------------------------------------------------------------------
// Plasma visuel
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, NkU32 w, NkU32 h,
                        float t, float px, float py, float sat)
{
    if (!w || !h) return;
    if (!std::isfinite(t)) t = 0.f;
    if (!std::isfinite(px)) px = 0.f;
    if (!std::isfinite(py)) py = 0.f;
    if (!std::isfinite(sat)) sat = 1.f;

    const NkU32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (NkU32 y = 0; y < h; y += blk) {
        float fy = y / (float)h - 0.5f;
        for (NkU32 x = 0; x < w; x += blk) {
            float fx  = x / (float)w - 0.5f;
            float rd  = math::NkSqrt(fx*fx + fy*fy);
            float mix = (math::NkSin((fx+px)*13.5f+t*1.7f)
                        +math::NkSin((fy+py)*11.f -t*1.3f)
                        +math::NkSin(rd*24.f      -t*2.1f)) * 0.333f;
            NkU8 ri = (NkU8)(ClampUnit((0.5f+0.5f*math::NkSin(6.28f*(mix+0.f  ))-0.5f)*sat+0.5f)*255);
            NkU8 gi = (NkU8)(ClampUnit((0.5f+0.5f*math::NkSin(6.28f*(mix+0.33f))-0.5f)*sat+0.5f)*255);
            NkU8 bi = (NkU8)(ClampUnit((0.5f+0.5f*math::NkSin(6.28f*(mix+0.66f))-0.5f)*sat+0.5f)*255);
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
            logger.Info("[CB:WindowMgr] Close event");
            state.running = false;
        });

    es.AddEventCallback<NkWindowResizeEvent>(
        [&state, &renderer](NkWindowResizeEvent* ev) {
            state.viewW = ev->GetWidth();
            state.viewH = ev->GetHeight();
            renderer.Resize(ev->GetWidth(), ev->GetHeight());
            logger.Info("[CB:WindowMgr] Resize → {0}x{1}", ev->GetWidth(), ev->GetHeight());
        });

    es.AddEventCallback<NkWindowFocusGainedEvent>(
        [](NkWindowFocusGainedEvent* /*ev*/) {
            logger.Info("[CB:WindowMgr] Focus gained");
        });

    es.AddEventCallback<NkWindowFocusLostEvent>(
        [](NkWindowFocusLostEvent* /*ev*/) {
            logger.Info("[CB:WindowMgr] Focus lost");
        });

    es.AddEventCallback<NkWindowMoveEvent>(
        [](NkWindowMoveEvent* ev) {
            logger.Info("[CB:WindowMgr] Moved ({0},{1})", ev->GetX(), ev->GetY());
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
                    state.fullscreen = !state.fullscreen.Load();
                    window.SetFullscreen(state.fullscreen.Load());
                    break;
                case NkKey::NK_SPACE:
                    state.neon = !state.neon.Load();
                    logger.Info("[CB:Input] Neon {0}", state.neon.Load() ? "ON" : "OFF");
                    break;
                case NkKey::NK_R:
                    state.phaseX = state.phaseY = 0.f;
                    state.saturation = 1.15f;
                    logger.Info("[CB:Input] Reset");
                    break;
                default:
                    break;
            }
        });

    es.AddEventCallback<NkKeyReleaseEvent>(
        [](NkKeyReleaseEvent* ev) {
            logger.Info("[CB:Input] Key released key={0}", (int)ev->GetKey());
        });

    es.AddEventCallback<NkTextInputEvent>(
        [](NkTextInputEvent* ev) {
            logger.Info("[CB:Input] Typed codepoint=U+{0}", ev->GetCodepoint());
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
            const float wheelDelta = static_cast<float>(ev->GetDeltaY());
            state.saturation = math::NkClamp(state.saturation + wheelDelta * 0.12f, 0.1f, 3.0f);
            logger.Info("[CB:Mouse] Wheel dy={0} sat={1}", ev->GetDeltaY(), state.saturation);
        });

    es.AddEventCallback<NkMouseButtonPressEvent>(
        [](NkMouseButtonPressEvent* ev) {
            logger.Info("[CB:Mouse] Press btn={0} at ({1},{2})", (int)ev->GetButton(), ev->GetX(), ev->GetY());
        });

    es.AddEventCallback<NkMouseEnterEvent>(
        [](NkMouseEnterEvent* /*ev*/) {
            logger.Info("[CB:Mouse] Enter window");
        });

    es.AddEventCallback<NkMouseLeaveEvent>(
        [](NkMouseLeaveEvent* /*ev*/) {
            logger.Info("[CB:Mouse] Leave window");
        });

    // ── Sous-système "Drag & Drop" ────────────────────────────────────────────

    es.AddEventCallback<NkDropEnterEvent>(
        [](NkDropEnterEvent* ev) {
            logger.Info("[CB:DnD] Enter pos=({0},{1}) files={2} text={3}",
                ev->data.x, ev->data.y, ev->data.numFiles, ev->data.hasText ? "yes" : "no");
        });

    es.AddEventCallback<NkDropLeaveEvent>(
        [](NkDropLeaveEvent* /*ev*/) {
            logger.Info("[CB:DnD] Leave");
        });

    es.AddEventCallback<NkDropFileEvent>(
        [&state](NkDropFileEvent* ev) {
            ++state.dropCount;
            state.droppedFiles = ev->data.paths;
            logger.Info("[CB:DnD] Files ({0}):", ev->data.paths.Size());
            for (const auto& f : ev->data.paths)
                logger.Info("  {0}", f.CStr());
        });

    es.AddEventCallback<NkDropTextEvent>(
        [&state](NkDropTextEvent* ev) {
            ++state.dropCount;
            state.droppedText = ev->data.text;
            logger.Info("[CB:DnD] Text [{0}]", ev->data.text.SubStr(0, 60).CStr());
        });

    // ── Sous-système "Gamepad" ────────────────────────────────────────────────

    // Callback connexion (hors event system — NkGamepadSystem direct)
    // CORRECTION : signature attendue (const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            logger.Info("[CB:Gamepad] {0} #{1} ({2})",
                connected ? "CONNECTED" : "DISCONNECTED", info.index, info.name);
        });

    // Événement de connexion (NkGamepadConnectEvent)
    es.AddEventCallback<NkGamepadConnectEvent>(
        [](NkGamepadConnectEvent* ev) {
            logger.Info("[CB:Gamepad] ConnectEvent idx={0}", ev->GetGamepadIndex());
        });

    // Événement de déconnexion (NkGamepadDisconnectEvent)
    es.AddEventCallback<NkGamepadDisconnectEvent>(
        [](NkGamepadDisconnectEvent* ev) {
            logger.Info("[CB:Gamepad] DisconnectEvent idx={0}", ev->GetGamepadIndex());
        });

    es.AddEventCallback<NkGamepadButtonPressEvent>(
        [&state](NkGamepadButtonPressEvent* ev) {
            ++state.gpCount;
            logger.Info("[CB:Gamepad] Button={0} idx={1}", (int)ev->GetButton(), ev->GetGamepadIndex());
            if (ev->GetButton() == NkGamepadButton::NK_GP_SOUTH) {
                state.neon = !state.neon.Load();
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
                logger.Info("[CB:OneShotx3] Resize #{0} → {1}x{2}",
                    resizeCount, ev->GetWidth(), ev->GetHeight());
                if (resizeCount >= 3) {
                    logger.Info("[CB:OneShotx3] Unsubscribing all resize callbacks after 3 resizes");
                    es.ClearEventCallbacks<NkWindowResizeEvent>();
                }
            });
    }

    // =========================================================================
    // 6. Boucle principale
    // =========================================================================

    NkChrono chrono;
    NkElapsedTime elapsed{};

    while (state.running.Load() && window.IsOpen())
    {
        NkElapsedTime e = chrono.Reset();

        // Pomper la queue — les callbacks s'exécutent ici.
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr) {
            // Les callbacks ont déjà été appelés via DispatchEvent interne.
            (void)ev;
        }

        if (!state.running.Load() || !window.IsOpen()) break;

        // --- Delta-time
        float dt = SafeDt(static_cast<float>(elapsed.seconds));
        state.time += dt * (state.neon.Load() ? 1.8f : 1.0f);

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

    logger.Info("[Stats] keys={0} mouse={1} drops={2} gamepad={3}",
        state.keyCount.Load(), state.mouseCount.Load(),
        state.dropCount.Load(), state.gpCount.Load());

    renderer.Shutdown();
    window.Close();
    NkClose();
    return 0;
}
