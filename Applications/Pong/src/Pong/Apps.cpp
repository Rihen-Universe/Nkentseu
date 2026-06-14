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
    bool appResumed   = true;
    // Signale (depuis l'event Shown) qu'il faut recreer la surface GPU. Le travail
    // reel est fait dans la boucle, HORS du polling d'evenements (cf. plus bas).
    bool surfaceRecreatePending = false;
    // Lifecycle differe : les events Hidden/FocusLost / Shown SIGNALENT, la boucle
    // appelle app.OnPause()/OnResume() dans le bon ordre (pause, puis recreate
    // surface, puis resume une fois la surface prete). On ne fait JAMAIS de
    // OnPause/OnResume directement dans le polling d'evenements.
    bool pausePending  = false;
    bool resumePending = false;
    math::NkVec2u pendingSize = window.GetSize();
    // Taille reellement appliquee. Sert de garde-fou contre l'OnResize redondant :
    // Windows emet WM_SIZE a la CREATION de la fenetre (meme taille) -> declencher
    // OnResize la-dessus recree la swapchain avant que la 1ere frame ne soit
    // consommee -> sur DX12 reset cmdList fail -> contexte invalide (ecran blanc).
    math::NkVec2u currentSize = window.GetSize();

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
                // On capture seulement la derniere taille demandee. La comparaison
                // avec la taille courante + l'OnResize se font dans la boucle, HORS
                // de l'event (cf. plus bas).
                pendingSize.width  = wr->GetWidth();
                pendingSize.height = wr->GetHeight();
            }
            // Android : surface recreee (APP_CMD_INIT_WINDOW) — il faut
            // recreer l'eglSurface car l'ancien ANativeWindow a ete detruit
            // par APP_CMD_TERM_WINDOW. Sans ce RecreateSurface, le rendu
            // est noir au retour de foreground.
            if (ev->Is<NkWindowShownEvent>()) {
                // On ne fait RIEN de lourd dans l'event : on SIGNALE juste qu'il
                // faut recreer la surface (fait hors boucle d'event, cf. plus bas)
                // et on capture la taille demandee (le resize sera applique par la
                // boucle uniquement s'il differe vraiment -> evite l'OnResize
                // redondant au demarrage qui casse DX12).
                surfaceRecreatePending = true;
                pendingSize = window.GetSize();
                resumePending = true;   // app.OnResume() appele dans la boucle, APRES recreate surface
                logger.Info("[Pong] Surface shown");
            }
            if (ev->Is<NkWindowHiddenEvent>()) {
                surfaceReady = false;
                logger.Info("[Pong] Surface hidden");
                // Auto-pause immediate quand on quitte l'app (back/home).
                // Le gameplay s'arretera proprement, evitant que la balle
                // bouge "dans le vide" et que la partie continue en bg.
                // (OnPause reel appele dans la boucle, hors event.)
                pausePending = true;
            }
            if (ev->Is<NkWindowFocusGainedEvent>()) {
                appResumed = true;
            }
            if (ev->Is<NkWindowFocusLostEvent>())
            {
                // Auto-pause aussi au simple focus lost (notification overlay,
                // splitscreen, etc.). La scene gameplay met mPaused=true.
                // (OnPause reel appele dans la boucle, hors event.)
                pausePending = true;
            }
            // Forward de tous les events restants a la scene active (clavier,
            // souris, touch, gamepad). Les events systeme ci-dessus ont deja
            // ete consommes pour leur effet de bord mais peuvent etre re-passes
            // sans danger.
            app.OnEvent(*ev);
        }
        if (!window.IsOpen()) break;

        // Pause demandee (event Hidden / FocusLost) : OnPause appele ICI, hors du
        // polling d'evenements. Traite meme si la surface n'est pas prete (on peut
        // pauser une app cachee).
        if (pausePending) {
            app.OnPause();
            pausePending = false;
        }

        // Recreation de surface (signalee par l'event Shown), faite ICI, hors de
        // la boucle d'evenements ET AVANT la garde surfaceReady : c'est ELLE qui
        // determine si la surface est de nouveau prete. No-op desktop ; sur Android
        // recree la surface GPU apres retour foreground (APP_CMD_INIT_WINDOW).
        if (surfaceRecreatePending) {
            surfaceReady = app.RecreateSurface();
            if (!surfaceReady) {
                logger.Warn("[Pong] RecreateSurface failed (no native window yet?)");
            }
            surfaceRecreatePending = false;
        }

        // Resume demande (event Shown) : OnResume appele ICI, APRES que la surface
        // soit de nouveau prete. Si la recreation a echoue, on reste en attente.
        if (resumePending && surfaceReady) {
            app.OnResume();
            appResumed = true;   // double-render au retour -> evite une frame stale
            resumePending = false;
        }

        if (!surfaceReady) {
            NkChrono::Sleep(16.0f);
            continue;
        }
        if (pendingSize.width == 0 || pendingSize.height == 0) {
            continue;
        }

        // Hors de l'event resize : on teste si la taille demandee differe de la
        // taille deja appliquee (= taille courante de la swapchain). Si oui, on
        // applique le resize et on met a jour currentSize. Ca evite l'OnResize
        // redondant au demarrage (WM_SIZE meme taille) qui, sur DX12, reset une
        // cmdList non-consommee -> contexte invalide -> ecran blanc.
        if (pendingSize.width != currentSize.width ||
            pendingSize.height != currentSize.height) {
            app.OnResize(pendingSize.width, pendingSize.height);
            currentSize = pendingSize;
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
            // window.Close();
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