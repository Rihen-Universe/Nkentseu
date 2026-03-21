// ============================================================================
// Sandbox/src/main.cpp
// Pattern A : Dispatcher typA (push - AvAnementiel)
// ============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventDispatcher.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/NkGamepadSystem.h"
#include "NKWindow/Core/NkMain.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKContext/Deprecate/NkRendererConfig.h"
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

// AppData pattern #1: one-shot inline builder expression.
NKENTSEU_DEFINE_APP_DATA(([]() {
    nkentseu::NkAppData d{};
    d.appName = "Sandbox";
    d.appVersion = "1.0.0";
    d.enableEventLogging = false;
    d.enableRendererDebug = false;
    d.enableMultiWindow = true;
    return d;
})());

namespace {
    using namespace nkentseu;
    using namespace nkentseu::math;

    float ClampUnit(float v) {
        if (v < 0.f) return 0.f;
        if (v > 1.f) return 1.f;
        return v;
    }

    uint32 PackWaveColor(float r, float g, float b) {
        return NkRenderer::PackColor(
            static_cast<uint8>(ClampUnit(r) * 255.f),
            static_cast<uint8>(ClampUnit(g) * 255.f),
            static_cast<uint8>(ClampUnit(b) * 255.f), 255);
    }

    void DrawPlasma(NkRenderer& renderer, uint32 width, uint32 height,
                    float t, const NkVec2f& phase, float sat)
    {
        if (!width || !height) return;
        const uint32 blk = (width * height > 1280u * 720u) ? 2u : 1u;
        const float iw = 1.f / width, ih = 1.f / height;
        for (uint32 y = 0; y < height; y += blk) {
            float fy = y * ih - 0.5f;
            for (uint32 x = 0; x < width; x += blk) {
                float fx  = x * iw - 0.5f;
                float rd  =  NkSqrt(fx*fx + fy*fy);
                float mix = (NkSin((fx + phase.x)*13.5f + t*1.7f)
                            +NkSin((fy + phase.y)*11.0f - t*1.3f)
                            +NkSin(rd*24.f - t*2.1f)) * 0.33333334f;
                float r = ClampUnit((0.5f+0.5f*NkSin(6.2831853f*(mix+0.00f))-0.5f)*sat+0.5f);
                float g = ClampUnit((0.5f+0.5f*NkSin(6.2831853f*(mix+0.33f))-0.5f)*sat+0.5f);
                float b = ClampUnit((0.5f+0.5f*NkSin(6.2831853f*(mix+0.66f))-0.5f)*sat+0.5f);
                uint32 col = PackWaveColor(r, g, b);
                for (uint32 by=0;by<blk&&(y+by)<height;++by)
                    for (uint32 bx=0;bx<blk&&(x+bx)<width;++bx)
                        renderer.SetPixel((int32)(x+bx),(int32)(y+by),col);
            }
        }
    }

    // =========================================================================
    // Couche applicative - Pattern A : Dispatcher typA
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

                // FenAtre
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
            uint32 mViewportW = 1280, mViewportH = 720;
    };

} // namespace

// ============================================================================
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

    // -------------------------------------------------------------------------
    // 2. FenAtre
    // -------------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "NkWindow Sandbox - Pattern A (Dispatcher)";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;
    NkContextApplyWindowHints(cfg);

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[Sandbox] Window creation FAILED");
        NkContextShutdown();
        return -2;
    }

    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        logger.Error("[Sandbox] Context creation FAILED: {0}", context.lastError.ToString());
        window.Close();
        NkContextShutdown();
        return -3;
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

        // --- Pattern A : Dispatcher typA (OnEvent pour chaque event)
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

        // Appliquer fullscreen si changA
        if (layer.GetFullscreen() != window.GetConfig().fullscreen) {
            window.SetFullscreen(layer.GetFullscreen());
        }

        // --- Delta-time ---
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        timeSeconds += dt * (layer.IsNeonMode() ? 1.8f : 1.0f);

        // --- Cap 60 fps ---
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}
