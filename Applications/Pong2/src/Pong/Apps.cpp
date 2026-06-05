// =============================================================================
// Apps.cpp — Point d'entree Pong Ultra Arena
// Etape 1 (PoC) : init NkContext OpenGL + boucle minimale + lifecycle Android.
// Conforme au GDD v1.1 (docs/GDD_PONG_ULTRA_ARENA_v1.1.docx).
//
// Gestion lifecycle Android :
//   - FocusLost  : on auto-pause si Playing
//   - Hidden     : surface detruite (APP_CMD_TERM_WINDOW), on cesse de rendre
//   - Shown      : surface recreee (APP_CMD_INIT_WINDOW), on reinit le contexte
//   - FocusGain  : retour foreground, force re-render
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
    bool appResumed   = true;
    math::NkVec2u pendingSize = window.GetSize();

    while (window.IsOpen()) {
        elapsed = chrono.Reset();
        float dt = static_cast<float>(elapsed.seconds);
        if (dt <= 0.f || dt > 0.25f) dt = 1.0f / 60.0f;

        // ── Pump events ─────────────────────────────────────────────────────
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
            // Android : surface recreee (APP_CMD_INIT_WINDOW) — il faut
            // recreer l'eglSurface car l'ancien ANativeWindow a ete detruit
            // par APP_CMD_TERM_WINDOW. Sans ce RecreateSurface, le rendu
            // est noir au retour de foreground.
            if (ev->Is<NkWindowShownEvent>()) {
                if (!app.RecreateSurface()) {
                    logger.Warn("[Pong] RecreateSurface returned false (no native window yet?)");
                }
                auto sz = window.GetSize();
                if (sz.x > 0 && sz.y > 0) {
                    app.OnResize(sz.x, sz.y);
                }
                surfaceReady = true;
                appResumed   = true;
                logger.Info("[Pong] Surface shown");
                app.OnResume();
            }
            if (ev->Is<NkWindowHiddenEvent>()) {
                surfaceReady = false;
                logger.Info("[Pong] Surface hidden");
                // Auto-pause immediate quand on quitte l'app (back/home).
                // Le gameplay s'arretera proprement, evitant que la balle
                // bouge "dans le vide" et que la partie continue en bg.
                app.OnPause();
            }
            if (ev->Is<NkWindowFocusGainedEvent>()) {
                appResumed = true;
            }
            if (ev->Is<NkWindowFocusLostEvent>())
            {
                // Auto-pause aussi au simple focus lost (notification overlay,
                // splitscreen, etc.). La scene gameplay met mPaused=true.
                app.OnPause();
            }
            // Forward de tous les events restants a la scene active (clavier,
            // souris, touch, gamepad). Les events systeme ci-dessus ont deja
            // ete consommes pour leur effet de bord mais peuvent etre re-passes
            // sans danger.
            app.OnEvent(*ev);
        }
        if (!window.IsOpen()) break;

        if (!surfaceReady) {
            NkChrono::Sleep(16.0f);
            continue;
        }
        if (pendingSize.width == 0 || pendingSize.height == 0) {
            continue;
        }

        if (needResize) {
            app.OnResize(pendingSize.width, pendingSize.height);
            needResize = false;
        }

        app.Update(dt);

        // Double-render apres reprise foreground (Android : evite frame stale)
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

        // Cap 60 fps
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16) NkChrono::Sleep(16 - elapsed.milliseconds);
        else                            NkChrono::YieldThread();
    }

    app.Shutdown();
    window.Close();
    return 0;
}
