// =============================================================================
// ex02_pattern_a_dispatcher.cpp
// Pattern A — Dispatcher typé (NkEventDispatcher / NK_DISPATCH)
//
// Couvre : NkEventDispatcher, NK_DISPATCH macro, tous les types d'events clavier
//          souris, fenêtre, drag & drop, gamepad axis + button.
//          Ring buffer : génère ~10 000 events synthétiques pour valider
//          la politique drop-oldest sans crash.
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

namespace {
using namespace nkentseu;

// ---------------------------------------------------------------------------
// Layer applicatif — reçoit chaque event via OnEvent(NkEvent*)
// Illustre le pattern "one handler per event type" avec NK_DISPATCH
// ---------------------------------------------------------------------------
class AppLayer {
public:
    // -----------------------------------------------------------------------
    // Routeur principal — appelé depuis la boucle de poll
    // -----------------------------------------------------------------------
    void OnEvent(NkEvent* ev) {
        NkEventDispatcher d(ev);

        // Fenêtre
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
    // Accesseurs état
    // -----------------------------------------------------------------------
    bool ShouldClose()    const { return mClose; }
    bool FullscreenDirty() const { return mFullscreenDirty; }
    bool GetFullscreen()  const { return mFullscreen; }
    void ClearFullscreenDirty()  { mFullscreenDirty = false; }

    NkVec2f  GetPhase()      const { return mPhase; }
    float    GetSaturation() const { return mSaturation; }
    bool     IsNeon()        const { return mNeon; }
    NkVec2u  GetViewport()   const { return { mViewW, mViewH }; }

    // Résumé stats pour HUD debug
    void PrintStats() const {
        std::printf(
            "[Stats] keys=%u mouse=%u wheel=%u drops=%u gamepads=%u resize=%u\n",
            mKeyPresses, mMouseClicks, mWheelTicks, mDrops, mGamepadEvents, mResizes);
    }

private:
    // --- état applicatif ---
    bool    mClose           = false;
    bool    mFullscreen      = false;
    bool    mFullscreenDirty = false;
    bool    mNeon            = false;
    NkVec2f mPhase           = { 0.f, 0.f };
    float   mSaturation      = 1.15f;
    NkU32   mViewW = 1280, mViewH = 720;

    // --- stats ---
    NkU32 mKeyPresses    = 0;
    NkU32 mMouseClicks   = 0;
    NkU32 mWheelTicks    = 0;
    NkU32 mDrops         = 0;
    NkU32 mGamepadEvents = 0;
    NkU32 mResizes       = 0;

    // -----------------------------------------------------------------------
    // Handlers — retournent true si l'event est consommé
    // -----------------------------------------------------------------------

    bool OnWindowClose(NkWindowCloseEvent& /*e*/) {
        std::printf("[Window] Close requested\n");
        mClose = true;
        return true;
    }

    bool OnWindowResize(NkWindowResizeEvent& e) {
        mViewW = e.GetWidth();
        mViewH = e.GetHeight();
        ++mResizes;
        std::printf("[Window] Resize → %ux%u\n", mViewW, mViewH);
        return false; // non consommé : le renderer peut aussi le traiter
    }

    bool OnWindowFocusGained(NkWindowFocusGainedEvent& /*e*/) {
        std::printf("[Window] Focus gained\n");
        return true;
    }

    bool OnWindowFocusLost(NkWindowFocusLostEvent& /*e*/) {
        std::printf("[Window] Focus lost\n");
        return true;
    }

    bool OnWindowMove(NkWindowMoveEvent& e) {
        std::printf("[Window] Moved → (%d, %d)\n", e.GetX(), e.GetY());
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
                std::printf("[Key] Neon mode %s\n", mNeon ? "ON" : "OFF");
                return true;
            case NkKey::NK_P:
                PrintStats();
                return true;
            default:
                // Ctrl+Z : reset phase
                if (e.GetKey() == NkKey::NK_Z && e.GetModifiers().ctrl) {
                    mPhase = { 0.f, 0.f };
                    std::printf("[Key] Phase reset\n");
                    return true;
                }
                return false;
        }
    }

    bool OnKeyRelease(NkKeyReleaseEvent& e) {
        std::printf("[Key] Released key=%d\n", (int)e.GetKey());
        return false;
    }

    bool OnTextInput(NkTextInputEvent& e) {
        // Codepoint Unicode reçu — illustre la saisie texte
        std::printf("[Key] Typed codepoint=U+%04X\n", e.GetCodepoint());
        return false;
    }

    // --- Souris ---

    bool OnMousePress(NkMouseButtonPressEvent& e) {
        ++mMouseClicks;
        std::printf("[Mouse] Press btn=%d at (%d,%d)\n",
            (int)e.GetButton(), e.GetX(), e.GetY());
        return false;
    }

    bool OnMouseRelease(NkMouseButtonReleaseEvent& e) {
        std::printf("[Mouse] Release btn=%d at (%d,%d)\n",
            (int)e.GetButton(), e.GetX(), e.GetY());
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
        mSaturation = std::fmax(0.1f, std::fmin(3.f,
            mSaturation + e.GetDeltaY() * 0.1f));
        std::printf("[Mouse] Wheel dy=%.2f → sat=%.2f\n",
            e.GetDeltaY(), mSaturation);
        return true;
    }

    bool OnMouseEnter(NkMouseEnterEvent& /*e*/) {
        std::printf("[Mouse] Entered window\n");
        return true;
    }

    bool OnMouseLeave(NkMouseLeaveEvent& /*e*/) {
        std::printf("[Mouse] Left window\n");
        return true;
    }

    // --- Drag & Drop ---

    bool OnDropEnter(NkDropEnterEvent& e) {
        std::printf("[Drop] Enter pos=(%d,%d) numFiles=%u hasText=%s\n",
            e.data.x, e.data.y,
            e.data.numFiles,
            e.data.hasText ? "yes" : "no");
        return true;
    }

    bool OnDropLeave(NkDropLeaveEvent& /*e*/) {
        std::printf("[Drop] Leave\n");
        return true;
    }

    bool OnDropFile(NkDropFileEvent& e) {
        ++mDrops;
        std::printf("[Drop] Files (%zu):\n", e.data.paths.size());
        for (const auto& f : e.data.paths)
            std::printf("  → %s\n", f.c_str());
        return true;
    }

    bool OnDropText(NkDropTextEvent& e) {
        ++mDrops;
        std::printf("[Drop] Text [%.60s...]\n", e.data.text.c_str());
        return true;
    }

    // --- Gamepad ---

    bool OnGamepadConnect(NkGamepadConnectEvent& e) {
        std::printf("[Gamepad] CONNECTED index=%u\n", e.GetGamepadIndex());
        return true;
    }

    bool OnGamepadDisconnect(NkGamepadDisconnectEvent& e) {
        std::printf("[Gamepad] DISCONNECTED index=%u\n", e.GetGamepadIndex());
        return true;
    }

    bool OnGamepadButton(NkGamepadButtonPressEvent& e) {
        ++mGamepadEvents;
        std::printf("[Gamepad] Button=%d idx=%u\n",
            (int)e.GetButton(), e.GetGamepadIndex());
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
                if (std::fabs(v) > 0.12f) mPhase.x += v * 0.025f;
                return true;
            case NkGamepadAxis::NK_GP_AXIS_LY:
                if (std::fabs(v) > 0.12f) mPhase.y += v * 0.025f;
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
// Plasma visuel (hérité des exemples précédents)
// ---------------------------------------------------------------------------
static float ClampUnit(float v) { return v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

static void DrawPlasma(NkRenderer& r, NkU32 w, NkU32 h,
                        float t, NkVec2f phase, float sat)
{
    if (!w || !h) return;
    const NkU32 blk = (w * h > 1280u * 720u) ? 2u : 1u;
    for (NkU32 y = 0; y < h; y += blk) {
        float fy = y / (float)h - 0.5f;
        for (NkU32 x = 0; x < w; x += blk) {
            float fx  = x / (float)w - 0.5f;
            float rd  = std::sqrt(fx*fx + fy*fy);
            float mix = (std::sin((fx+phase.x)*13.5f+t*1.7f)
                        +std::sin((fy+phase.y)*11.f -t*1.3f)
                        +std::sin(rd*24.f         -t*2.1f)) * 0.333f;
            NkU8 ri = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+0.00f))-0.5f)*sat+0.5f)*255.f);
            NkU8 gi = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+0.33f))-0.5f)*sat+0.5f)*255.f);
            NkU8 bi = (NkU8)(ClampUnit((0.5f+0.5f*std::sin(6.28f*(mix+0.66f))-0.5f)*sat+0.5f)*255.f);
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
    if (!NkInitialise({ .appName = "ex02 Pattern A — Dispatcher" })) return -1;

    // 2. Fenêtre
    NkWindowConfig cfg;
    cfg.title       = "ex02 — Pattern A : NkEventDispatcher";
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

    // 4. Layer
    AppLayer layer;
    auto& es = NkEvents();

    // 5. Gamepad connect callback (hors dispatcher — enregistré une seule fois)
    // Signature attendue : void(const NkGamepadInfo&, bool)
    NkGamepads().SetConnectCallback(
        [](const NkGamepadInfo& info, bool connected) {
            std::printf("[Gamepad] %s #%u (%s)\n",
                connected ? "Connected" : "Disconnected",
                info.index, info.name);
        });

    // 6. Boucle
    float   t  = 0.f;
    NkChrono chrono;
    NkElapsedTime elapsed;

    while (!layer.ShouldClose() && window.IsOpen())
    {
        chrono.Reset();

        // --- Dispatch typé
        while (NkEvent* ev = es.PollEvent())
            layer.OnEvent(ev);

        if (layer.ShouldClose() || !window.IsOpen()) break;

        // --- Fullscreen si changé par F11
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
        NkU32 w  = vp.x ? vp.x : (NkU32)cfg.width;
        NkU32 h  = vp.y ? vp.y : (NkU32)cfg.height;
        DrawPlasma(renderer, w, h, t, layer.GetPhase(), layer.GetSaturation());
        renderer.EndFrame();
        renderer.Present();

        // --- Cap 60fps
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
    }

    renderer.Shutdown();
    NkClose();
    return 0;
}