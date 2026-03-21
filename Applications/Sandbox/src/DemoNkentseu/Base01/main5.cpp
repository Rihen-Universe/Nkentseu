// =============================================================================
// ex04_pattern_c_callbacks.cpp
// Pattern C - Callbacks permanents (AddEventCallback<T>)
//
// Couvre : AddEventCallback<T>, abonnements A  durAe de vie limitAe,
//          NkGamepadSystem callbacks (connect, button, axis),
//          NkWindowCloseEvent, NkWindowResizeEvent, NkKeyPressEvent,
//          NkMouseMotionEvent, NkMouseWheelEvent, NkDropFileEvent,
//          NkDropTextEvent, NkGamepadAxisEvent, NkGamepadButtonPressEvent.
//
// ATTENTION : Le systAme de dAsabonnement par token n'est pas implAmentA.
//             La partie "unsubscribe after 3 resizes" a AtA commentAe.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Ajout pour NkChrono et NkElapsedTime
#include "NKMath/NKMath.h"

#include "NKLogger/NkLog.h"

#include <cmath>
#include <string>
#include <vector>

// AppData pattern #6: mutable global filled by static bootstrap object.
static nkentseu::NkAppData gPatternCallbackAppData{};
struct PatternCallbackAppDataBootstrap {
    PatternCallbackAppDataBootstrap() {
        gPatternCallbackAppData.appName = "SandboxPatternCallback";
        gPatternCallbackAppData.appVersion = "1.0.0";
        gPatternCallbackAppData.enableEventLogging = true;
        gPatternCallbackAppData.enableRendererDebug = false;
        gPatternCallbackAppData.enableMultiWindow = true;
    }
};
static const PatternCallbackAppDataBootstrap gPatternCallbackAppDataBootstrap{};
NKENTSEU_DEFINE_APP_DATA(gPatternCallbackAppData);

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
    if (!math::NkIsFinite(dt) || dt <= 0.f || dt > 0.25f) return 1.f / 60.f;
    return dt;
}

// ---------------------------------------------------------------------------
// Atat partagA - modifiA par les callbacks depuis diffArents "sous-systAmes"
// ---------------------------------------------------------------------------
struct SharedState {
    NkAtomicBool  running { true };
    NkAtomicBool  neon    { false };
    NkAtomicBool  fullscreen { false };
    float  saturation = 1.15f;
    float  phaseX     = 0.f;
    float  phaseY     = 0.f;
    float  time       = 0.f;
    uint32  viewW      = 1280;
    uint32  viewH      = 720;

    // Historique drops
    NkVector<NkString> droppedFiles;
    NkString              droppedText;

    // Compteur d'events par type (stats)
    NkAtomic<uint32> keyCount   { 0 };
    NkAtomic<uint32> mouseCount { 0 };
    NkAtomic<uint32> dropCount  { 0 };
    NkAtomic<uint32> gpCount    { 0 };
};

// ---------------------------------------------------------------------------
// Plasma visuel
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, uint32 w, uint32 h,
                        float t, float px, float py, float sat)
{
    if (!w || !h) return;
    if (!math::NkIsFinite(t)) t = 0.f;
    if (!math::NkIsFinite(px)) px = 0.f;
    if (!math::NkIsFinite(py)) py = 0.f;
    if (!math::NkIsFinite(sat)) sat = 1.f;

    const uint32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (uint32 y = 0; y < h; y += blk) {
        float fy = y / (float)h - 0.5f;
        for (uint32 x = 0; x < w; x += blk) {
            float fx  = x / (float)w - 0.5f;
            float rd  = math::NkSqrt(fx*fx + fy*fy);
            float mix = (math::NkSin((fx+px)*13.5f+t*1.7f)
                        +math::NkSin((fy+py)*11.f -t*1.3f)
                        +math::NkSin(rd*24.f      -t*2.1f)) * 0.333f;
            uint8 ri = static_cast<uint8>(ClampUnit((0.5f + 0.5f * math::NkSin(6.28f * (mix + 0.f)) - 0.5f) * sat + 0.5f) * 255);
            uint8 gi = static_cast<uint8>(ClampUnit((0.5f + 0.5f * math::NkSin(6.28f * (mix + 0.33f)) - 0.5f) * sat + 0.5f) * 255);
            uint8 bi = static_cast<uint8>(ClampUnit((0.5f + 0.5f * math::NkSin(6.28f * (mix + 0.66f)) - 0.5f) * sat + 0.5f) * 255);
            uint32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (uint32 by=0;by<blk&&(y+by)<h;++by)
                for (uint32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((int32)(x+bx),(int32)(y+by),col);
        }
    }
}

} // namespace

// =============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    NkContextInit();
    NkContextConfig contextConfig{};
    contextConfig.api = NkRendererApi::NK_SOFTWARE;
    contextConfig.vsync = true;
    contextConfig.debug = false;
    NkContextSetHints(contextConfig);
    contextConfig = NkContextGetHints();

    // 1. Runtime dAjA  initialisA par l'entrypoint NkMain

    // 2. FenAtre
    NkWindowConfig cfg;
    cfg.title       = "ex04 - Pattern C : Callbacks permanents";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;
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
    rcfg.api                   = NkRendererApi::NK_SOFTWARE;
    rcfg.autoResizeFramebuffer = true;
    NkRenderer renderer;
    if (!renderer.Create(window, rcfg)) {
        NkContextDestroy(context);
        window.Close();
        NkContextShutdown();
        return -4;
    }

    // 4. Atat partagA entre tous les callbacks
    SharedState state;
    state.viewW = cfg.width;
    state.viewH = cfg.height;

    auto& es = NkEvents();

    // =========================================================================
    // 5. ABONNEMENTS
    //
    //    Chaque sous-systAme (ici simulA par un bloc commentA) s'abonne
    //    indApendamment et ne voit que le type d'event qui l'intAresse.
    // =========================================================================

    // -- Sous-systAme "Window Manager" ----------------------------------------

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
            logger.Info("[CB:WindowMgr] Resize -> {0}x{1}", ev->GetWidth(), ev->GetHeight());
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

    // -- Sous-systAme "Input/Keyboard" ----------------------------------------

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

    // -- Sous-systAme "Mouse" -------------------------------------------------

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

    // -- Sous-systAme "Drag & Drop" --------------------------------------------

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

    // -- Sous-systAme "Gamepad" ------------------------------------------------

    // Callback connexion (hors event system - NkGamepadSystem direct)
    // CORRECTION : signature attendue (const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            logger.Info("[CB:Gamepad] {0} #{1} ({2})",
                connected ? "CONNECTED" : "DISCONNECTED", info.index, info.name);
        });

    // AvAnement de connexion (NkGamepadConnectEvent)
    es.AddEventCallback<NkGamepadConnectEvent>(
        [](NkGamepadConnectEvent* ev) {
            logger.Info("[CB:Gamepad] ConnectEvent idx={0}", ev->GetGamepadIndex());
        });

    // AvAnement de dAconnexion (NkGamepadDisconnectEvent)
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
            if (math::NkFabs(v) < 0.12f) return; // dead-zone

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

    // -- Abonnement A  durAe limitAe - se dAsinscrit aprAs 3 redimensionnements
    //     (Version simplifiAe : on utilise un compteur et on retire tous les callbacks de resize aprAs 3)
    {
        static int resizeCount = 0;
        es.AddEventCallback<NkWindowResizeEvent>(
            [&es](NkWindowResizeEvent* ev) {
                ++resizeCount;
                logger.Info("[CB:OneShotx3] Resize #{0} -> {1}x{2}",
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

        // Pomper la queue - les callbacks s'exAcutent ici.
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr) {
            // Les callbacks ont dAjA  AtA appelAs via DispatchEvent interne.
            (void)ev;
        }

        if (!state.running.Load() || !window.IsOpen()) break;

        // --- Delta-time
        float dt = SafeDt(static_cast<float>(elapsed.seconds));
        state.time += dt * (state.neon.Load() ? 1.8f : 1.0f);

        // --- Rendu
        renderer.BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));
        const auto& fb = renderer.GetFramebufferInfo();
        uint32 w = fb.width  ? fb.width  : state.viewW;
        uint32 h = fb.height ? fb.height : state.viewH;
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
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}
