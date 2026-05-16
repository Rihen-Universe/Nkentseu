// =============================================================================
// Apps.cpp — Point d'entree Pong Ultra Arena
// Etape 1 (PoC) : init NkContext OpenGL + boucle minimale.
// Conforme au GDD v1.1 (docs/GDD_PONG_ULTRA_ARENA_v1.1.docx).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/NKWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkTime.h"

#include "Pong/Game/PongApp.h"

using namespace nkentseu;
using nkentseu::pong::PongApp;

int nkmain(const NkEntryState& state) {
    (void)state;

    // ── Fenetre paysage 1280x720 selon GDD §5.2 ──────────────────────────────
    NkWindowConfig cfg;
    cfg.title       = "Pong Ultra Arena";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = false;

#if defined(NKENTSEU_PLATFORM_ANDROID)
    cfg.fullscreen        = true;
    cfg.hideSystemUI      = true;
    cfg.screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    cfg.lockOrientation   = true;
#endif

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[Pong] Window creation failed");
        return -1;
    }

#if defined(NKENTSEU_PLATFORM_ANDROID)
    window.SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
    window.SetLockOrientation(true);
    window.SetFullscreen(true);
    window.SetHideSystemUI(true);
#endif

    // ── Application Pong (init OpenGL via NkContext) ─────────────────────────
    PongApp app(window);
    if (!app.Init()) {
        logger.Error("[Pong] PongApp::Init failed");
        window.Close();
        return -2;
    }

    // ── Boucle principale ────────────────────────────────────────────────────
    auto& events = NkEvents();
    NkChrono chrono;
    NkElapsedTime elapsed;

    bool surfaceReady = true;
    bool needResize   = false;
    math::NkVec2u pendingSize = window.GetSize();

    while (window.IsOpen()) {
        elapsed = chrono.Reset();
        float dt = static_cast<float>(elapsed.seconds);
        if (dt <= 0.f || dt > 0.25f) dt = 1.0f / 60.0f;

        // ── Evenements ──────────────────────────────────────────────────────
        while (NkEvent* ev = events.PollEvent()) {
            if (ev->Is<NkWindowCloseEvent>()) {
                window.Close();
                break;
            }
            if (auto* wr = ev->As<NkWindowResizeEvent>()) {
                pendingSize.width  = wr->GetWidth();
                pendingSize.height = wr->GetHeight();
                needResize = true;
            }
            if (ev->Is<NkWindowShownEvent>())  surfaceReady = true;
            if (ev->Is<NkWindowHiddenEvent>()) surfaceReady = false;
        }
        if (!window.IsOpen()) break;

        if (!surfaceReady) {
            NkChrono::Sleep(16.0f);
            continue;
        }
        if (needResize && pendingSize.width > 0 && pendingSize.height > 0) {
            app.OnResize(pendingSize.width, pendingSize.height);
            needResize = false;
        }

        // ── Update + Render ─────────────────────────────────────────────────
        app.Update(dt);
        app.Render();

        if (app.WantsQuit()) {
            // window.Close();
            break;
        }

        // ── Cap 60 fps ──────────────────────────────────────────────────────
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16) NkChrono::Sleep(16 - elapsed.milliseconds);
        else                            NkChrono::YieldThread();
    }

    app.Shutdown();
    window.Close();
    return 0;
}
