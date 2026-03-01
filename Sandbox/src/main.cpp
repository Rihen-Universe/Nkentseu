// ============================================================================
// Sandbox/src/main.cpp
// ============================================================================

#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Time/NkClock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <iostream>

#ifndef NK_SANDBOX_RENDERER_API
#define NK_SANDBOX_RENDERER_API nkentseu::NkRendererApi::NK_SOFTWARE
#endif

namespace {
using namespace nkentseu;

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
            float rd  = std::sqrt(fx*fx + fy*fy);
            float mix = (std::sin((fx + phase.x)*13.5f + t*1.7f)
                        +std::sin((fy + phase.y)*11.0f - t*1.3f)
                        +std::sin(rd*24.f - t*2.1f)) * 0.33333334f;
            float r = ClampUnit((0.5f+0.5f*std::sin(6.2831853f*(mix+0.00f))-0.5f)*sat+0.5f);
            float g = ClampUnit((0.5f+0.5f*std::sin(6.2831853f*(mix+0.33f))-0.5f)*sat+0.5f);
            float b = ClampUnit((0.5f+0.5f*std::sin(6.2831853f*(mix+0.66f))-0.5f)*sat+0.5f);
            NkU32 col = PackWaveColor(r, g, b);
            for (NkU32 by=0;by<blk&&(y+by)<height;++by)
                for (NkU32 bx=0;bx<blk&&(x+bx)<width;++bx)
                    renderer.SetPixel((NkI32)(x+bx),(NkI32)(y+by),col);
        }
    }
}
} // namespace

// ============================================================================
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    // -------------------------------------------------------------------------
    // 1. Initialisation
    // -------------------------------------------------------------------------
    NkAppData app;
    app.appName           = "NkWindow Sandbox";
    app.preferredRenderer = NK_SANDBOX_RENDERER_API;

    if (!NkInitialise(app)) {
        std::fprintf(stderr, "[Sandbox] NkInitialise FAILED\n");
        return -1;
    }

    // Les gamepads sont pollés automatiquement par NkEventSystem::PollEvent()
    // (mAutoGamepadPoll = true par défaut). Pas besoin de les gérer manuellement.

    // -------------------------------------------------------------------------
    // 2. Fenêtre
    // -------------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "NkWindow Sandbox";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        std::fprintf(stderr, "[Sandbox] Window creation FAILED: %s\n",
                     window.GetLastError().ToString().c_str());
        NkClose();
        return -2;
    }

    // -------------------------------------------------------------------------
    // 3. Renderer
    // -------------------------------------------------------------------------
    NkRendererConfig rcfg;
    rcfg.api                   = NK_SANDBOX_RENDERER_API;
    rcfg.autoResizeFramebuffer = true;

    std::unique_ptr<NkRenderer> renderer;
    if (rcfg.api != NkRendererApi::NK_NONE) {
        renderer = std::make_unique<NkRenderer>();
        if (!renderer->Create(window, rcfg)) {
            std::fprintf(stderr, "[Sandbox] Renderer creation FAILED\n");
            NkClose();
            return -3;
        }
    }

    // -------------------------------------------------------------------------
    // 4. Boucle principale
    // -------------------------------------------------------------------------
    auto& eventSystem = NkEventSystem::Instance();

    bool    running         = true;
    bool    neonMode        = false;
    float   saturationBoost = 1.15f;
    NkVec2f phaseOffset     = {0.f, 0.f};
    float   timeSeconds     = 0.f;

    NkClock::TimePoint previousTick = NkClock::Now();

    while (running)
    {
        // --- Événements ---
        // PollEvent() pompe automatiquement les messages OS ET les gamepads
        // à chaque fois que la queue interne est vide.
        while (NkEvent* event = eventSystem.PollEvent())
        {
            // Fermeture fenêtre (clic sur la croix)
            if (event->Is<NkWindowCloseEvent>()) {
                running = false;
                window.Close();
                break;
            }

            // Redimensionnement
            if (auto* resize = event->As<NkWindowResizeEvent>()) {
                if (renderer)
                    renderer->Resize(resize->GetWidth(), resize->GetHeight());
                continue;
            }

            // Clavier
            if (auto* key = event->As<NkKeyPressEvent>()) {
                switch (key->GetKey()) {
                    case NkKey::NK_ESCAPE:
                        running = false;
                        window.Close();
                        break;
                    case NkKey::NK_F11:
                        window.SetFullscreen(!window.GetConfig().fullscreen);
                        break;
                    case NkKey::NK_SPACE:
                        neonMode = !neonMode;
                        break;
                    default: break;
                }
                if (!running) break;
                continue;
            }

            // Manette — axes
            if (auto* ax = event->As<NkGamepadAxisEvent>()) {
                float v = ax->GetValue();
                switch (ax->GetAxis()) {
                    case NkGamepadAxis::NK_GP_AXIS_LX:
                        phaseOffset.x += v * 0.02f; break;
                    case NkGamepadAxis::NK_GP_AXIS_LY:
                        phaseOffset.y += v * 0.02f; break;
                    case NkGamepadAxis::NK_GP_AXIS_RT:
                        saturationBoost = 1.f + ClampUnit(v) * 0.8f; break;
                    default: break;
                }
                continue;
            }

            // Manette — boutons
            if (auto* btn = event->As<NkGamepadButtonPressEvent>()) {
                if (btn->GetButton() == NkGamepadButton::NK_GP_SOUTH) {
                    neonMode = !neonMode;
                    // Rumble via NkGamepadSystem directement (pas besoin
                    // d'une référence à NkGamepads dans la boucle)
                    NkGamepadSystem::Instance().Rumble(
                        btn->GetGamepadIndex(), 0.35f, 0.45f, 0.f, 0.f, 40);
                }
                continue;
            }
        }

        if (!running || !window.IsOpen())
            break;

        // --- Delta-time ---
        const NkClock::TimePoint now = NkClock::Now();
        NkDuration delta = NkClock::ToNkDuration(now - previousTick);
        previousTick = now;
        float dt = (float)delta.ToSeconds();
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        timeSeconds += dt * (neonMode ? 1.8f : 1.0f);

        // --- Rendu ---
        if (renderer) {
            renderer->BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));
            const NkFramebufferInfo& fb = renderer->GetFramebufferInfo();
            NkU32 w = fb.width  ? fb.width  : window.GetSize().x;
            NkU32 h = fb.height ? fb.height : window.GetSize().y;
            DrawPlasma(*renderer, w, h, timeSeconds, phaseOffset, saturationBoost);
            renderer->EndFrame();
            renderer->Present();
        }

        // --- Cap 60 fps ---
        const NkDuration budget  = NkDuration::FromMilliseconds(16.f);
        const NkDuration elapsed = NkClock::ElapsedSince(now);
        if (elapsed < budget)
            NkClock::Sleep(budget - elapsed);
        else
            NkClock::YieldThread();
    }

    // -------------------------------------------------------------------------
    // 5. Nettoyage
    // -------------------------------------------------------------------------
    if (renderer)
        renderer->Shutdown();

    NkClose();
    return 0;
}