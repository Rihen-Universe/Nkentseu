// =============================================================================
// ex03_pattern_b_polling.cpp
// Pattern B - Polling direct (NkInputQuery / NkInput)
//
// Couvre : PollEvent pour haute prioritA (Close / Resize), puis NkInput pour
//          tout le reste (clavier, souris, gamepad axes/buttons).
//          Illustre IsKeyDown vs IsKeyPressed (tenue vs front montant),
//          MouseDelta, GamepadAxis dead-zone, rumble conditionnel.
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKContext/Deprecate/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKTime/NkChrono.h"
#include "NKWindow/Core/NkMain.h"

#include "NKLogger/NkLog.h"
#include "NKMath/NKMath.h"
#include "NKMemory/NkMemory.h"

#include <cmath>

// AppData pattern #5: aggregate initialization (all fields in order).
static nkentseu::NkAppData gPatternPollingAppData{
    nkentseu::NkRendererApi::NK_SOFTWARE,
    false,
    false,
    "SandboxPatternPolling",
    "1.0.0",
    false
};
NKENTSEU_DEFINE_APP_DATA(gPatternPollingAppData);

namespace {
using namespace nkentseu;
using namespace nkentseu::math;

static float ClampUnit(float v) { return NkMax(0.f, NkMin(1.f, v)); }

// ---------------------------------------------------------------------------
// Etat applicatif plat (Pattern B - pas de classes)
// ---------------------------------------------------------------------------
struct AppState {
    bool    running      = true;
    bool    fullscreen   = false;
    bool    neon         = false;
    float   saturation   = 1.15f;
    NkVec2f phase        = { 0.f, 0.f };
    float   time         = 0.f;
    uint32   viewW        = 1280;
    uint32   viewH        = 720;
    uint32   fpsFrames    = 0;
    double  fpsAccum     = 0.0;
    float   currentFps   = 0.f;
};

// ---------------------------------------------------------------------------
// Rendu plasma identique aux autres exemples
// ---------------------------------------------------------------------------
static void DrawPlasma(NkRenderer& r, uint32 w, uint32 h,
                        float t, NkVec2f ph, float sat)
{
    if (!w || !h) return;
    const uint32 blk = (w*h > 1280u*720u) ? 2u : 1u;
    for (uint32 y = 0; y < h; y += blk) {
        float fy = y / (float)h - 0.5f;
        for (uint32 x = 0; x < w; x += blk) {
            float fx  = x / (float)w - 0.5f;
            float rd  = NkSqrt(fx*fx + fy*fy);
            float mix = (NkSin((fx+ph.x)*13.5f+t*1.7f)
                        +NkSin((fy+ph.y)*11.f -t*1.3f)
                        +NkSin(rd*24.f        -t*2.1f)) * 0.333f;
            uint8 ri = (uint8)(ClampUnit((0.5f+0.5f*NkSin(6.28f*(mix+0.00f))-0.5f)*sat+0.5f)*255);
            uint8 gi = (uint8)(ClampUnit((0.5f+0.5f*NkSin(6.28f*(mix+0.33f))-0.5f)*sat+0.5f)*255);
            uint8 bi = (uint8)(ClampUnit((0.5f+0.5f*NkSin(6.28f*(mix+0.66f))-0.5f)*sat+0.5f)*255);
            uint32 col = NkRenderer::PackColor(ri, gi, bi, 255);
            for (uint32 by=0;by<blk&&(y+by)<h;++by)
                for (uint32 bx=0;bx<blk&&(x+bx)<w;++bx)
                    r.SetPixel((int32)(x+bx),(int32)(y+by),col);
        }
    }
}

// ---------------------------------------------------------------------------
// Overlay debug (coins de l'Acran) - utilise FillRect
// ---------------------------------------------------------------------------

static void DrawFillRect(NkRenderer& r, uint32 x, uint32 y, uint32 w, uint32 h, uint32 col) {
    for (uint32 j = 0; j < h; ++j) {
        for (uint32 i = 0; i < w; ++i) {
            r.DrawPixel(static_cast<int32>(x + i), static_cast<int32>(y + j), col);
        }
    }
}

static void DrawHud(NkRenderer& r, const AppState& s)
{
    // Bande de fond pour HUD
    DrawFillRect(r, 0, 0, s.viewW, 22, NkRenderer::PackColor(0, 0, 0, 160));

    // Indicateur Neon
    uint32 neonCol = s.neon
        ? NkRenderer::PackColor(0, 255, 140, 255)
        : NkRenderer::PackColor(80, 80, 80, 255);
    DrawFillRect(r, 4, 4, 14, 14, neonCol);

    // Indicateur fullscreen
    uint32 fsCol = s.fullscreen
        ? NkRenderer::PackColor(255, 200, 0, 255)
        : NkRenderer::PackColor(80, 80, 80, 255);
    DrawFillRect(r, 24, 4, 14, 14, fsCol);

    // Indicateur gamepad connectA (remplace le cercle par un carrA)
    bool gpOk = NkGamepads().IsConnected(0);
    uint32 gpCol = gpOk
        ? NkRenderer::PackColor(50, 220, 50, 255)
        : NkRenderer::PackColor(80, 80, 80, 255);
    DrawFillRect(r, s.viewW - 24, 4, 14, 14, gpCol);
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
    cfg.title       = "ex03 - Pattern B : Direct Polling (NkInput)";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = false; // pas nAcessaire en mode polling pur

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

    // 4. Atat
    AppState s;
    s.viewW = cfg.width;
    s.viewH = cfg.height;

    auto& es = NkEvents();
    NkChrono chrono;
    NkElapsedTime elapsed;

    // 5. Boucle principale
    while (s.running && window.IsOpen())
    {
        NkElapsedTime e = chrono.Reset();

        // -------------------------------------------------------------------
        // 5a. Pomper uniquement les AvAnements haute-prioritA
        //     (Close, Resize) - le reste est gArA par polling
        // -------------------------------------------------------------------
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            if (ev->Is<NkWindowCloseEvent>()) {
                s.running = false;
                break;
            }
            if (auto* r = ev->As<NkWindowResizeEvent>()) {
                s.viewW = r->GetWidth();
                s.viewH = r->GetHeight();
                renderer.Resize(s.viewW, s.viewH);
            }
        }
        if (!s.running) break;

        // -------------------------------------------------------------------
        // 5d. Gamepad - polling axes + boutons via NkGamepads()
        // -------------------------------------------------------------------
        auto& gp = NkGamepads();

        if (gp.IsConnected(0))
        {
            const float deadzone = 0.12f;

            float lx = gp.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LX);
            float ly = gp.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LY);
            float rx = gp.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_RX);
            float rt = gp.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_RT);
            float lt = gp.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LT);

            // Stick gauche : dAplacer phase
            if (NkFabs(lx) > deadzone) s.phase.x += lx * 0.025f;
            if (NkFabs(ly) > deadzone) s.phase.y += ly * 0.025f;

            // Stick droit X : saturation
            if (NkFabs(rx) > deadzone)
                s.saturation = NkMax(0.1f, NkMin(3.f,
                    s.saturation + rx * 0.01f));

            // RT/LT : vitesse time
            float speed = 1.f + rt * 1.5f - lt * 0.8f;

            // Bouton A (south) tenu : neon mode
            bool aDown = gp.IsButtonDown(0, NkGamepadButton::NK_GP_SOUTH);
            if (aDown) {
                s.neon = true;
                gp.Rumble(0, 0.15f, 0.35f, 0.f, 0.f, 16);
            } else {
                s.neon = false;
            }

            // Bouton B (east) : reset (front montant)
            static bool lastBDown = false;
            bool bDown = gp.IsButtonDown(0, NkGamepadButton::NK_GP_EAST);
            if (bDown && !lastBDown) {
                s.phase      = { 0.f, 0.f };
                s.saturation = 1.15f;
            }
            lastBDown = bDown;

            // Bouton Start : toggle fullscreen (front montant)
            static bool lastStartDown = false;
            bool startDown = gp.IsButtonDown(0, NkGamepadButton::NK_GP_START);
            if (startDown && !lastStartDown) {
                s.fullscreen = !s.fullscreen;
                window.SetFullscreen(s.fullscreen);
            }
            lastStartDown = startDown;

            // Appliquer multiplicateur vitesse
            float dt = (float)elapsed.seconds;
            if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;
            s.time += dt * speed * (s.neon ? 1.8f : 1.0f);
        }

        // -------------------------------------------------------------------
        // 5e. Delta-time & temps
        // -------------------------------------------------------------------
        float dt = (float)elapsed.seconds;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;
        s.time += dt * (s.neon ? 1.8f : 1.0f);

        // FPS rolling average
        s.fpsAccum  += dt;
        s.fpsFrames += 1;
        if (s.fpsAccum >= 1.0) {
            s.currentFps = (float)s.fpsFrames / (float)s.fpsAccum;
            s.fpsAccum   = 0.0;
            s.fpsFrames  = 0;
            logger.Info("[FPS] {0}", s.currentFps);
        }

        // -------------------------------------------------------------------
        // 5f. Rendu
        // -------------------------------------------------------------------
        renderer.BeginFrame(NkRenderer::PackColor(8, 10, 18, 255));

        const auto& fb = renderer.GetFramebufferInfo();
        uint32 w = fb.width  ? fb.width  : s.viewW;
        uint32 h = fb.height ? fb.height : s.viewH;

        DrawPlasma(renderer, w, h, s.time, s.phase, s.saturation);
        DrawHud(renderer, s);

        renderer.EndFrame();
        renderer.Present();

        // -------------------------------------------------------------------
        // 5g. Cap 60fps
        // -------------------------------------------------------------------
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }

    renderer.Shutdown();
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}
