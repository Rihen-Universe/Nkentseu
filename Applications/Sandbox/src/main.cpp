// ============================================================================
// Sandbox/src/main.cpp
// Pattern A : Dispatcher typé (push - événementiel)
// ============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventDispatcher.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKRenderer/NkRenderer.h"
#include "NKRenderer/NkRendererConfig.h"
#include "NKTime/NkChrono.h"

#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"

#include "NKMemory/NkMemory.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <iostream>
#include <cstdlib>

#ifndef NK_SANDBOX_RENDERER_API
#define NK_SANDBOX_RENDERER_API nkentseu::NkRendererApi::NK_SOFTWARE
#endif

namespace {
using namespace nkentseu;
using namespace nkentseu::math;

float ClampUnit(float v) {
    if (v < 0.f) return 0.f;
    if (v > 1.f) return 1.f;
    return v;
}

NkU32 PackWaveColor(float r, float g, float b) {
    return NkRenderer::PackColor(
        static_cast<NkU8>(ClampUnit(r) * 255.f),
        static_cast<NkU8>(ClampUnit(g) * 255.f),
        static_cast<NkU8>(ClampUnit(b) * 255.f), 255);
}

void DrawPlasma(NkRenderer& renderer, NkU32 width, NkU32 height,
                float t, const NkVec2f& phase, float sat)
{
    if (!width || !height) return;
    const NkU32 blk = (width * height > 1280u * 720u) ? 2u : 1u;
    const float iw = 1.f / width, ih = 1.f / height;
    for (NkU32 y = 0; y < height; y += blk) {
        float fy = y * ih - 0.5f;
        for (NkU32 x = 0; x < width; x += blk) {
            float fx  = x * iw - 0.5f;
            float rd  =  NkSqrt(fx*fx + fy*fy);
            float mix = (NkSin((fx + phase.x)*13.5f + t*1.7f)
                        +NkSin((fy + phase.y)*11.0f - t*1.3f)
                        +NkSin(rd*24.f - t*2.1f)) * 0.33333334f;
            float r = ClampUnit((0.5f+0.5f*NkSin(6.2831853f*(mix+0.00f))-0.5f)*sat+0.5f);
            float g = ClampUnit((0.5f+0.5f*NkSin(6.2831853f*(mix+0.33f))-0.5f)*sat+0.5f);
            float b = ClampUnit((0.5f+0.5f*NkSin(6.2831853f*(mix+0.66f))-0.5f)*sat+0.5f);
            NkU32 col = PackWaveColor(r, g, b);
            for (NkU32 by=0;by<blk&&(y+by)<height;++by)
                for (NkU32 bx=0;bx<blk&&(x+bx)<width;++bx)
                    renderer.SetPixel((NkI32)(x+bx),(NkI32)(y+by),col);
        }
    }
}

// =========================================================================
// Couche applicative — Pattern A : Dispatcher typé
// =========================================================================

class GameLayer {
public:
    void OnEvent(nkentseu::NkEvent* ev) {
        nkentseu::NkEventDispatcher d(ev);

        // Clavier
        NK_DISPATCH(d, nkentseu::NkKeyPressEvent, OnKeyPress);

        // Manette
        NK_DISPATCH(d, nkentseu::NkGamepadAxisEvent, OnGamepadAxis);
        NK_DISPATCH(d, nkentseu::NkGamepadButtonPressEvent, OnGamepadPress);

        // Fenêtre
        NK_DISPATCH(d, nkentseu::NkWindowCloseEvent, OnWindowClose);
        NK_DISPATCH(d, nkentseu::NkWindowResizeEvent, OnWindowResize);
    }

    bool OnKeyPress(nkentseu::NkKeyPressEvent& e) {
        switch (e.GetKey()) {
            case nkentseu::NkKey::NK_ESCAPE:
                mShouldClose = true;
                return true;
            case nkentseu::NkKey::NK_F11:
                mFullscreen = !mFullscreen;
                return true;
            case nkentseu::NkKey::NK_SPACE:
                mNeonMode = !mNeonMode;
                return true;
            default:
                return false;
        }
    }

    bool OnGamepadAxis(nkentseu::NkGamepadAxisEvent& e) {
        float v = e.GetValue();
        switch (e.GetAxis()) {
            case nkentseu::NkGamepadAxis::NK_GP_AXIS_LX:
                mPhaseOffset.x += v * 0.02f;
                return true;
            case nkentseu::NkGamepadAxis::NK_GP_AXIS_LY:
                mPhaseOffset.y += v * 0.02f;
                return true;
            case nkentseu::NkGamepadAxis::NK_GP_AXIS_RT:
                mSaturationBoost = 1.f + ClampUnit(v) * 0.8f;
                return true;
            default:
                return false;
        }
    }

    bool OnGamepadPress(nkentseu::NkGamepadButtonPressEvent& e) {
        if (e.GetButton() == nkentseu::NkGamepadButton::NK_GP_SOUTH) {
            mNeonMode = !mNeonMode;
            nkentseu::NkGamepads().Rumble(e.GetGamepadIndex(), 0.2f, 0.5f, 0.f, 0.f, 80);
            return true;
        }
        return false;
    }

    bool OnWindowClose(nkentseu::NkWindowCloseEvent& e) {
        (void)e;
        mShouldClose = true;
        return true;
    }

    bool OnWindowResize(nkentseu::NkWindowResizeEvent& e) {
        mViewportW = e.GetWidth();
        mViewportH = e.GetHeight();
        return false;
    }

    NkVec2f GetPhaseOffset() const { return mPhaseOffset; }
    float GetSaturation() const { return mSaturationBoost; }
    bool IsNeonMode() const { return mNeonMode; }
    bool ShouldClose() const { return mShouldClose; }
    bool GetFullscreen() const { return mFullscreen; }
    void SetFullscreen(bool v) { mFullscreen = v; }

private:
    bool mShouldClose = false;
    bool mFullscreen = false;
    bool mNeonMode = false;
    nkentseu::NkVec2f mPhaseOffset = {0.f, 0.f};
    float mSaturationBoost = 1.15f;
    NkU32 mViewportW = 1280, mViewportH = 720;
};

} // namespace

// ============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    // -------------------------------------------------------------------------
    // 1. Initialisation
    // -------------------------------------------------------------------------
    if (!NkInitialise({ .appName = "NkWindow Sandbox Pattern A" })) {
        logger.Error("[Sandbox] NkInitialise FAILED");
        return -1;
    }

    // -------------------------------------------------------------------------
    // 2. Fenêtre
    // -------------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "NkWindow Sandbox - Pattern A (Dispatcher)";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[Sandbox] Window creation FAILED");
        NkClose();
        return -2;
    }

    // -------------------------------------------------------------------------
    // 3. Renderer
    // -------------------------------------------------------------------------
    NkRendererConfig rcfg;
    rcfg.api                   = NK_SANDBOX_RENDERER_API;
    rcfg.autoResizeFramebuffer = true;

    mem::NkUniquePtr<NkRenderer> renderer;
    if (rcfg.api != NkRendererApi::NK_NONE) {
        renderer = mem::NkMakeUnique<NkRenderer>();
        if (!renderer->Create(window, rcfg)) {
            logger.Error("[Sandbox] Renderer creation FAILED");
            NkClose();
            return -3;
        }
    }

    // -------------------------------------------------------------------------
    // 4. GameLayer - Pattern A (Dispatcher)
    // -------------------------------------------------------------------------
    GameLayer layer;

    // -------------------------------------------------------------------------
    // 5. Boucle principale
    // -------------------------------------------------------------------------
    auto& eventSystem = NkEvents();

    bool running = true;
    float timeSeconds = 0.f;
    NkChrono chrono;
    NkElapsedTime elapsed;

    while (running && window.IsOpen())
    {
        NkElapsedTime e = chrono.Reset();

        // --- Pattern A : Dispatcher typé (OnEvent pour chaque event)
        while (NkEvent* event = eventSystem.PollEvent())
        {
            layer.OnEvent(event);
            
            if (layer.ShouldClose() || !window.IsOpen()) {
                running = false;
                break;
            }
        }

        if (!running || !window.IsOpen())
            break;

        // Appliquer fullscreen si changé
        if (layer.GetFullscreen() != window.GetConfig().fullscreen) {
            window.SetFullscreen(layer.GetFullscreen());
        }

        // --- Delta-time ---
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        timeSeconds += dt * (layer.IsNeonMode() ? 1.8f : 1.0f);

        // --- Rendu ---
        if (renderer) {
            renderer->BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));
            const NkFramebufferInfo& fb = renderer->GetFramebufferInfo();
            NkU32 w = fb.width  ? fb.width  : window.GetSize().x;
            NkU32 h = fb.height ? fb.height : window.GetSize().y;
            DrawPlasma(*renderer, w, h, timeSeconds, layer.GetPhaseOffset(), layer.GetSaturation());
            renderer->EndFrame();
            renderer->Present();
        }

        // --- Cap 60 fps ---
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    // -------------------------------------------------------------------------
    // 6. Nettoyage
    // -------------------------------------------------------------------------
    if (renderer)
        renderer->Shutdown();

    window.Close();
    NkClose();
    return 0;
}
