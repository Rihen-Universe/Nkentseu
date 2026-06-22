// =============================================================================
// Apps.cpp — Point d'entrée Songo'o
// Architecture IDENTIQUE à Applications/Songoo/Apps.cpp du repo Nkentseu
// NkWindow + boucle manuelle + NkEvents::PollEvent + SongooGame
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

#include "Songoo/Game/SongooGame.h"

using namespace nkentseu;
using nkentseu::songoo::SongooGame;

int nkmain(const NkEntryState& state) {
    (void)state;

    // ── Fenêtre paysage 1600×900 ─────────────────────────────────────────────
    NkWindowConfig cfg;
    cfg.title       = "Songo'o";
    cfg.width       = 1600;
    cfg.height      = 900;
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
        logger.Error("[Songo'o] Window creation failed");
        return -1;
    }

#if defined(NKENTSEU_PLATFORM_ANDROID)
    window.SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
    window.SetLockOrientation(true);
    window.SetFullscreen(true);
    window.SetHideSystemUI(true);
#endif

    SongooGame app(window);
    if (!app.Init()) {
        logger.Error("[Songo'o] SongooGame::Init failed");
        window.Close();
        return -2;
    }

    auto& events = NkEvents();
    NkChrono chrono;
    NkElapsedTime elapsed;

    bool surfaceReady = true;
    bool needResize   = false;
    bool appResumed   = true;
    math::NkVec2u pendingSize = window.GetSize();

    while (window.IsOpen()) {
        elapsed = chrono.Reset();
        float dt = static_cast<float>(elapsed.seconds);
        if (dt <= 0.f || dt > 0.25f) dt = 1.0f / 60.0f;

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
            if (ev->Is<NkWindowShownEvent>()) {
                if (!app.RecreateSurface())
                    logger.Warn("[Songo'o] RecreateSurface returned false");
                auto sz = window.GetSize();
                if (sz.x > 0 && sz.y > 0)
                    app.OnResize(sz.x, sz.y);
                surfaceReady = true;
                appResumed   = true;
                app.OnResume();
            }
            if (ev->Is<NkWindowHiddenEvent>()) {
                surfaceReady = false;
                app.OnPause();
            }
            if (ev->Is<NkWindowFocusGainedEvent>())
                appResumed = true;
            if (ev->Is<NkWindowFocusLostEvent>())
                app.OnPause();

            app.OnEvent(*ev);
        }
        if (!window.IsOpen()) break;

        if (!surfaceReady) {
            NkChrono::Sleep(16.0f);
            continue;
        }
        if (pendingSize.width == 0 || pendingSize.height == 0)
            continue;

        if (needResize) {
            app.OnResize(pendingSize.width, pendingSize.height);
            needResize = false;
        }

        app.Update(dt);

        if (appResumed) {
            app.Render();
            app.Render();
            appResumed = false;
        } else {
            app.Render();
        }

        if (app.WantsQuit()) {
            window.Close();
            break;
        }

        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16) NkChrono::Sleep(16 - elapsed.milliseconds);
        else                            NkChrono::YieldThread();
    }

    app.Shutdown();
    window.Close();
    return 0;
}
