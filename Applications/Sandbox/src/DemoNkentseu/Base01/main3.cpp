// =============================================================================
// ex02_pattern_a_dispatcher.cpp
// Pattern A - Dispatcher typA (NkEventDispatcher / NK_DISPATCH)
//
// Couvre : NkEventDispatcher, NK_DISPATCH macro, tous les types d'events clavier
//          souris, fenAtre, drag & drop, gamepad axis + button.
//          Ring buffer : gAnAre ~10 000 events synthAtiques pour valider
//          la politique drop-oldest sans crash.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKTime/NkChrono.h"   // Ajout pour NkChrono et NkElapsedTime

#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKMemory/NkMemory.h"

#include <cmath>
#include <string>

// AppData pattern #4: dedicated factory function + macro.
static nkentseu::NkAppData BuildPatternDispatcherAppData() {
    nkentseu::NkAppData d{};
    d.appName = "SandboxPatternDispatcher";
    d.appVersion = "1.0.0";
    d.enableEventLogging = false;
    d.enableRendererDebug = false;
    d.enableMultiWindow = true;
    return d;
}
NKENTSEU_DEFINE_APP_DATA(BuildPatternDispatcherAppData());

namespace {
using namespace nkentseu;
using namespace nkentseu::math;

// ---------------------------------------------------------------------------
// Layer applicatif - reAoit chaque event via OnEvent(NkEvent*)
// Illustre le pattern "one handler per event type" avec NK_DISPATCH
// ---------------------------------------------------------------------------
class AppLayer {
public:
    // -----------------------------------------------------------------------
    // Routeur principal - appelA depuis la boucle de poll
    // -----------------------------------------------------------------------
    void OnEvent(NkEvent* ev) {
        NkEventDispatcher d(ev);

        // FenAtre
        NK_DISPATCH(d, NkWindowCloseEvent,        OnWindowClose);
        NK_DISPATCH(d, NkWindowResizeEvent,       OnWindowResize);
        NK_DISPATCH(d, NkWindowFocusGainedEvent,  OnWindowFocusGained);
        NK_DISPATCH(d, NkWindowFocusLostEvent,    OnWindowFocusLost);
        NK_DISPATCH(d, NkWindowMoveEvent,         OnWindowMove);

        // Clavier
        NK_DISPATCH(d, NkKeyPressEvent,           OnKeyPress);
        NK_DISPATCH(d, NkKeyReleaseEvent,         OnKeyRelease);
        NK_DISPATCH(d, NkTextInputEvent,          OnTextInput);   // au lieu de NkKeyTypedEvent

        // Souris
        NK_DISPATCH(d, NkMouseButtonPressEvent,   OnMousePress);
        NK_DISPATCH(d, NkMouseButtonReleaseEvent, OnMouseRelease);
        NK_DISPATCH(d, NkMouseMoveEvent,          OnMouseMove);   // au lieu de NkMouseMotionEvent
        NK_DISPATCH(d, NkMouseWheelVerticalEvent, OnMouseWheel);  // on utilise la molette verticale
        NK_DISPATCH(d, NkMouseEnterEvent,         OnMouseEnter);
        NK_DISPATCH(d, NkMouseLeaveEvent,         OnMouseLeave);

        // Drag & Drop
        NK_DISPATCH(d, NkDropEnterEvent, OnDropEnter);
        NK_DISPATCH(d, NkDropLeaveEvent, OnDropLeave);
        NK_DISPATCH(d, NkDropFileEvent,  OnDropFile);
        NK_DISPATCH(d, NkDropTextEvent,  OnDropText);

        // Gamepad
        NK_DISPATCH(d, NkGamepadConnectEvent,      OnGamepadConnect);
        NK_DISPATCH(d, NkGamepadButtonPressEvent,  OnGamepadButton);
        NK_DISPATCH(d, NkGamepadAxisEvent,         OnGamepadAxis);
    }

    // -----------------------------------------------------------------------
    // Accesseurs Atat
    // -----------------------------------------------------------------------
    bool ShouldClose()    const { return mClose; }
    bool FullscreenDirty() const { return mFullscreenDirty; }
    bool GetFullscreen()  const { return mFullscreen; }
    void ClearFullscreenDirty()  { mFullscreenDirty = false; }

    NkVec2f  GetPhase()      const { return mPhase; }
    float    GetSaturation() const { return mSaturation; }
    bool     IsNeon()        const { return mNeon; }
    NkVec2u  GetViewport()   const { return { mViewW, mViewH }; }

    // RAsumA stats pour HUD debug
    void PrintStats() const {
        logger.Info("[Stats] keys={0} mouse={1} wheel={2} drops={3} gamepads={4} resize={5}",
            mKeyPresses, mMouseClicks, mWheelTicks, mDrops, mGamepadEvents, mResizes);
    }

private:
    // --- Atat applicatif ---
    bool    mClose           = false;
    bool    mFullscreen      = false;
    bool    mFullscreenDirty = false;
    bool    mNeon            = false;
    NkVec2f mPhase           = { 0.f, 0.f };
    float   mSaturation      = 1.15f;
    uint32   mViewW = 1280, mViewH = 720;

    // --- stats ---
    uint32 mKeyPresses    = 0;
    uint32 mMouseClicks   = 0;
    uint32 mWheelTicks    = 0;
    uint32 mDrops         = 0;
    uint32 mGamepadEvents = 0;
    uint32 mResizes       = 0;

    // -----------------------------------------------------------------------
    // Handlers - retournent true si l'event est consommA
    // -----------------------------------------------------------------------

    bool OnWindowClose(NkWindowCloseEvent& /*e*/) {
        logger.Info("[Window] Close requested");
        mClose = true;
        return true;
    }

    bool OnWindowResize(NkWindowResizeEvent& e) {
        mViewW = e.GetWidth();
        mViewH = e.GetHeight();
        ++mResizes;
        logger.Info("[Window] Resize -> {0}x{1}", mViewW, mViewH);
        return false; // non consommA : le renderer peut aussi le traiter
    }

    bool OnWindowFocusGained(NkWindowFocusGainedEvent& /*e*/) {
        logger.Info("[Window] Focus gained");
        return true;
    }

    bool OnWindowFocusLost(NkWindowFocusLostEvent& /*e*/) {
        logger.Info("[Window] Focus lost");
        return true;
    }

    bool OnWindowMove(NkWindowMoveEvent& e) {
        logger.Info("[Window] Moved -> ({0}, {1})", e.GetX(), e.GetY());
        return true;
    }

    // --- Clavier ---

    bool OnKeyPress(NkKeyPressEvent& e) {
        ++mKeyPresses;
        switch (e.GetKey()) {
            case NkKey::NK_ESCAPE:
                mClose = true;
                return true;
            case NkKey::NK_F11:
                mFullscreen      = !mFullscreen;
                mFullscreenDirty = true;
                return true;
            case NkKey::NK_SPACE:
                mNeon = !mNeon;
                logger.Info("[Key] Neon mode {0}", mNeon ? "ON" : "OFF");
                return true;
            case NkKey::NK_P:
                PrintStats();
                return true;
            default:
                // Ctrl+Z : reset phase
                if (e.GetKey() == NkKey::NK_Z && e.GetModifiers().ctrl) {
                    mPhase = { 0.f, 0.f };
                    logger.Info("[Key] Phase reset");
                    return true;
                }
                return false;
        }
    }

    bool OnKeyRelease(NkKeyReleaseEvent& e) {
        logger.Info("[Key] Released key={0}", (int)e.GetKey());
        return false;
    }

    bool OnTextInput(NkTextInputEvent& e) {
        // Codepoint Unicode reAu - illustre la saisie texte
        logger.Info("[Key] Typed codepoint=U+{0}", e.GetCodepoint());
        return false;
    }

    // --- Souris ---

    bool OnMousePress(NkMouseButtonPressEvent& e) {
        ++mMouseClicks;
        logger.Info("[Mouse] Press btn={0} at ({1},{2})", (int)e.GetButton(), e.GetX(), e.GetY());
        return false;
    }

    bool OnMouseRelease(NkMouseButtonReleaseEvent& e) {
        logger.Info("[Mouse] Release btn={0} at ({1},{2})", (int)e.GetButton(), e.GetX(), e.GetY());
        return false;
    }

    bool OnMouseMove(NkMouseMoveEvent& e) {
        // Phase driven by mouse when left button held
        if (e.GetButtons().IsDown(NkMouseButton::NK_MB_LEFT)) {
            mPhase.x += e.GetDeltaX() * 0.002f;
            mPhase.y += e.GetDeltaY() * 0.002f;
        }
        return false;
    }

    bool OnMouseWheel(NkMouseWheelVerticalEvent& e) {
        ++mWheelTicks;
        const float nextSaturation = mSaturation + static_cast<float>(e.GetDeltaY()) * 0.1f;
        mSaturation = NkClamp(nextSaturation, 0.1f, 3.0f);
        logger.Info("[Mouse] Wheel dy={0} -> sat={1}", e.GetDeltaY(), mSaturation);
        return true;
    }

    bool OnMouseEnter(NkMouseEnterEvent& /*e*/) {
        logger.Info("[Mouse] Entered window");
        return true;
    }

    bool OnMouseLeave(NkMouseLeaveEvent& /*e*/) {
        logger.Info("[Mouse] Left window");
        return true;
    }

    // --- Drag & Drop ---

    bool OnDropEnter(NkDropEnterEvent& e) {
        logger.Info("[Drop] Enter pos=({0},{1}) numFiles={2} hasText={3}",
            e.data.x, e.data.y, e.data.numFiles, e.data.hasText ? "yes" : "no");
        return true;
    }

    bool OnDropLeave(NkDropLeaveEvent& /*e*/) {
        logger.Info("[Drop] Leave");
        return true;
    }

    bool OnDropFile(NkDropFileEvent& e) {
        ++mDrops;
        logger.Info("[Drop] Files ({0}):", e.data.paths.Size());
        for (const auto& f : e.data.paths)
            logger.Info("  -> {0}", f.CStr());
        return true;
    }

    bool OnDropText(NkDropTextEvent& e) {
        ++mDrops;
        logger.Info("[Drop] Text [{0}...]", e.data.text.SubStr(0, 60).CStr());
        return true;
    }

    // --- Gamepad ---

    bool OnGamepadConnect(NkGamepadConnectEvent& e) {
        logger.Info("[Gamepad] CONNECTED index={0}", e.GetGamepadIndex());
        return true;
    }

    bool OnGamepadDisconnect(NkGamepadDisconnectEvent& e) {
        logger.Info("[Gamepad] DISCONNECTED index={0}", e.GetGamepadIndex());
        return true;
    }

    bool OnGamepadButton(NkGamepadButtonPressEvent& e) {
        ++mGamepadEvents;
        logger.Info("[Gamepad] Button={0} idx={1}", (int)e.GetButton(), e.GetGamepadIndex());
        if (e.GetButton() == NkGamepadButton::NK_GP_SOUTH) {
            mNeon = !mNeon;
            NkGamepads().Rumble(e.GetGamepadIndex(),
                0.3f, 0.3f, 0.f, 0.f, 60);
            return true;
        }
        if (e.GetButton() == NkGamepadButton::NK_GP_START) {
            PrintStats();
            return true;
        }
        return false;
    }

    bool OnGamepadAxis(NkGamepadAxisEvent& e) {
        ++mGamepadEvents;
        float v = e.GetValue();
        switch (e.GetAxis()) {
            case NkGamepadAxis::NK_GP_AXIS_LX:
                if (NkFabs(v) > 0.12f) mPhase.x += v * 0.025f;
                return true;
            case NkGamepadAxis::NK_GP_AXIS_LY:
                if (NkFabs(v) > 0.12f) mPhase.y += v * 0.025f;
                return true;
            case NkGamepadAxis::NK_GP_AXIS_RT:
                mSaturation = 1.f + v * 0.8f;
                return true;
            default:
                return false;
        }
    }
};

// ---------------------------------------------------------------------------
// Plasma visuel (hAritA des exemples prAcAdents)
// ---------------------------------------------------------------------------
static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

static void DrawPlasma(NkRenderer& r, uint32 w, uint32 h,
                        float t, NkVec2f phase, float sat)
{
    if (!w || !h) return;
    const uint32 blk = (w * h > 1280u * 720u) ? 2u : 1u;
    for (uint32 y = 0; y < h; y += blk) {
        float fy = y / (float)h - 0.5f;
        for (uint32 x = 0; x < w; x += blk) {
            float fx  = x / (float)w - 0.5f;
            float rd  = NkSqrt(fx*fx + fy*fy);
            float mix = (NkSin((fx+phase.x)*13.5f+t*1.7f)
                        +NkSin((fy+phase.y)*11.f -t*1.3f)
                        +NkSin(rd*24.f         -t*2.1f)) * 0.333f;
            uint8 ri = (uint8)(ClampUnit((0.5f+0.5f*NkSin(6.28f*(mix+0.00f))-0.5f)*sat+0.5f)*255.f);
            uint8 gi = (uint8)(ClampUnit((0.5f+0.5f*NkSin(6.28f*(mix+0.33f))-0.5f)*sat+0.5f)*255.f);
            uint8 bi = (uint8)(ClampUnit((0.5f+0.5f*NkSin(6.28f*(mix+0.66f))-0.5f)*sat+0.5f)*255.f);
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
    cfg.title       = "ex02 - Pattern A : NkEventDispatcher";
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

    // 4. Layer
    AppLayer layer;
    auto& es = NkEvents();

    // 5. Gamepad connect callback (hors dispatcher - enregistrA une seule fois)
    // Signature attendue : void(const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            logger.Info("[Gamepad] {0} #{1} ({2})",
                connected ? "Connected" : "Disconnected",
                info.index, info.name);
        });

    // 6. Boucle
    float   t  = 0.f;
    NkChrono chrono;
    NkElapsedTime elapsed;

    while (!layer.ShouldClose() && window.IsOpen())
    {
        NkElapsedTime e = chrono.Reset();

        // --- Dispatch typA
        while (NkEvent* ev = es.PollEvent())
            layer.OnEvent(ev);

        if (layer.ShouldClose() || !window.IsOpen()) break;

        // --- Fullscreen si changA par F11
        if (layer.FullscreenDirty()) {
            window.SetFullscreen(layer.GetFullscreen());
            layer.ClearFullscreenDirty();
        }

        // --- Delta-time
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        t += dt * (layer.IsNeon() ? 2.2f : 1.0f);

        // --- Render
        renderer.BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));
        auto  vp = layer.GetViewport();
        uint32 w  = vp.x ? vp.x : (uint32)cfg.width;
        uint32 h  = vp.y ? vp.y : (uint32)cfg.height;
        DrawPlasma(renderer, w, h, t, layer.GetPhase(), layer.GetSaturation());
        renderer.EndFrame();
        renderer.Present();

        // --- Cap 60fps
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
    }

    renderer.Shutdown();
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}

