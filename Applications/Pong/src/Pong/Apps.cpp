// =============================================================================
// Apps.cpp  –  Point d'entrée nkmain avec gestion complète des inputs
// =============================================================================
#include "NKWindow/NKMain.h"
#include "NKWindow/NKWindow.h"
#include "NKLogger/NkLog.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "PongGame.h"
#include "NKTime/NkTime.h"
#include <tuple>

using namespace nkentseu;

int nkmain(const NkEntryState& state)
{
    (void)state;

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig cfg;
    cfg.title       = "Pong – Software Renderer";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = false;

#if defined(__ANDROID__)
    cfg.fullscreen        = true;
    cfg.hideSystemUI      = true;
    cfg.screenOrientation = NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE;
    cfg.lockOrientation   = true;
#endif

    NkWindow window;
    if (!window.Create(cfg)) {
        logger.Error("[PONG] Window creation failed");
        return -1;
    }

    // ── Renderer (double-buffered) ────────────────────────────────────────────
    NkRenderer* renderer = new NkRenderer();
    if (!renderer->Init(window)) {
        logger.Error("[PONG] Renderer init failed");
        window.Close();
        return -2;
    }

    // ── Jeu ──────────────────────────────────────────────────────────────────
    PongGame* game = new PongGame(*renderer);
#if defined(__ANDROID__)
    game->SetShowTouchButtons(true);
#endif
    game->Init();

#if defined(__ANDROID__)
    // ── Configuration Android spécifique ─────────────────────────────────────
    // ── Orientation landscape uniquement (verrouiller orientation) ──────────
    window.SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
    window.SetLockOrientation(true);  // Empêcher la rotation
    
    // ── Mode fullscreen + masquer barres système ───────────────────────────
    window.SetFullscreen(true);
    window.SetHideSystemUI(true);  // Masquer status bar + navigation bar
#endif

    // ── Boucle ───────────────────────────────────────────────────────────────
    auto& eventSystem = NkEvents();
    NkChrono     chrono;
    NkElapsedTime elapsed;

    bool running        = true;
    bool upHeld         = false;
    bool downHeld       = false;
    bool needResize     = false;
    // Touch button states (Android) — count of fingers in each button area
    int  touchEnterCount= 0;
    int  touchEscapeCount = 0;
    int  touchPauseCount = 0;
    // Touches à détecter sur un seul frame (flanc montant)
    bool enterPressed  = false;
    bool escapePressed = false;
    bool leftPressed   = false;
    bool rightPressed  = false;
    // État précédent (pour flanc montant)
    bool prevEnter     = false;
    bool prevEscape    = false;
    bool prevLeft      = false;
    bool prevRight     = false;
    // État courant des touches bool-style
    bool enterHeld     = false;
    bool escapeHeld    = false;
    bool leftHeld      = false;
    bool rightHeld     = false;
    // Mouse state
    int  mouseX        = -1;
    int  mouseY        = -1;
    bool mouseBtnHeld  = false;
    bool prevMouseBtn  = false;
    bool mouseClicked  = false;

    math::NkVec2u pendingSize = renderer->Size();
    
    // ── Gestion du pause/resume (Android) ──────────────────────────────────
    // Quand l'app revient au premier plan, forcer un refresh complet de l'ecran
    bool appResumed = true;  // Premier frame = init
    bool wasInBackground = false;
    bool gamePausedDueToBackground = false;  // true si le jeu a été pause automatiquement

    // surfaceReady : false quand la surface Android est détruite (APP_CMD_TERM_WINDOW),
    // true quand elle est (re)créée (APP_CMD_INIT_WINDOW -> NkWindowShownEvent).
    // On ne doit JAMAIS appeler renderer->Present() quand surfaceReady == false.
    bool surfaceReady = true;  // La surface est prête dès le départ

    while (running)
    {
        NkElapsedTime e = chrono.Reset();
        float dt = math::NkMin(static_cast<float>(e.milliseconds) / 1000.f, 0.033f);

        // Flancs montants (one-shot ce frame)
        enterPressed  = false;
        escapePressed = false;
        leftPressed   = false;
        rightPressed  = false;
        mouseClicked  = false;

        // ── Événements ───────────────────────────────────────────────────────
        while (NkEvent* event = eventSystem.PollEvent())
        {
            if (auto* wc = event->As<NkWindowCloseEvent>())
            {
                if (wc->GetWindowId() == window.GetId())
                {
                    running = false;
                    break;
                }
            }

            if (auto* wr = event->As<NkWindowResizeEvent>()) {
                needResize = true;
                pendingSize.width  = wr->GetWidth();
                pendingSize.height = wr->GetHeight();
            }
            if (auto* wm = event->As<NkWindowMaximizeEvent>()) {
                needResize = true;
                pendingSize = window.GetSize();
            }
            if (auto* wn = event->As<NkWindowMinimizeEvent>())
            {
                pendingSize = {0, 0};
            }
            // ── NkWindowShownEvent : surface Android recréée (APP_CMD_INIT_WINDOW) ──
            // C'est ici le vrai moment de réinitialiser le renderer avec le NOUVEAU
            // ANativeWindow*. NkWindowFocusGainedEvent arrive APRES, mais trop tard :
            // le renderer aurait déjà tenté de rendre sur l'ancienne surface invalide.
            if (auto* ws = event->As<NkWindowShownEvent>())
            {
                auto sz = window.GetSize();
                if (sz.x > 0 && sz.y > 0) {
                    renderer->Init(window, sz.x, sz.y);
                }
                surfaceReady = true;
                appResumed   = true;  // Forcer un rendu complet ce frame
            }

            // ── NkWindowHiddenEvent : surface Android détruite (APP_CMD_TERM_WINDOW) ──
            // Ne plus rendre jusqu'au prochain NkWindowShownEvent.
            if (auto* wh = event->As<NkWindowHiddenEvent>())
            {
                surfaceReady = false;
            }

            if (auto* fg = event->As<NkWindowFocusGainedEvent>())
            {
                // Le focus revient — la surface a DEJA ete reinit par NkWindowShownEvent.
                // On s'assure juste que appResumed est vrai pour le double-clear.
                appResumed = true;
            }
            if (auto* fl = event->As<NkWindowFocusLostEvent>())
            {
                // App is going to background — auto-pause the game if playing
                if (game->GetState() == GameState::Playing) {
                    game->SetState(GameState::Paused);
                    gamePausedDueToBackground = true;
                }
                wasInBackground = true;
            }

            if (auto* kp = event->As<NkKeyPressEvent>()) {
                NkKey k = kp->GetKey();
                if (k == NkKey::NK_UP    || k == NkKey::NK_W)
                {
                    upHeld = true;
                }
                if (k == NkKey::NK_DOWN  || k == NkKey::NK_S)
                {
                    downHeld = true;
                }
                if (k == NkKey::NK_LEFT  || k == NkKey::NK_A)
                {
                    leftHeld = true;
                }
                if (k == NkKey::NK_RIGHT || k == NkKey::NK_D)
                {
                    rightHeld = true;
                }
                if (k == NkKey::NK_ENTER || k == NkKey::NK_SPACE)
                {
                    enterHeld = true;
                }
                if (k == NkKey::NK_ESCAPE)
                {
                    escapeHeld = true;
                }
                // Pause avec P pendant la partie
                if (k == NkKey::NK_P && game->GetState() == GameState::Playing) {
                    // On simule escapePressed pour déclencher la pause
                    escapeHeld = true;
                }
            }
            if (auto* kr = event->As<NkKeyReleaseEvent>()) {
                NkKey k = kr->GetKey();
                if (k == NkKey::NK_UP    || k == NkKey::NK_W)
                {
                    upHeld = false;
                }
                if (k == NkKey::NK_DOWN  || k == NkKey::NK_S)
                {
                    downHeld = false;
                }
                if (k == NkKey::NK_LEFT  || k == NkKey::NK_A)
                {
                    leftHeld = false;
                }
                if (k == NkKey::NK_RIGHT || k == NkKey::NK_D)
                {
                    rightHeld = false;
                }
                if (k == NkKey::NK_ENTER || k == NkKey::NK_SPACE)
                {
                    enterHeld = false;
                }
                if (k == NkKey::NK_ESCAPE)
                {
                    escapeHeld = false;
                }
                if (k == NkKey::NK_P && escapeHeld) {
                    // On simule escapePressed pour déclencher la pause
                    escapeHeld = false;
                }
            }

            // ── Souris ────────────────────────────────────────────────────────
            if (auto* mm = event->As<NkMouseMoveEvent>()) {
                mouseX = mm->GetX();
                mouseY = mm->GetY();
            }
            if (auto* mb = event->As<NkMouseButtonPressEvent>()) {
                if (mb->IsLeft()) {
                    mouseBtnHeld = true;
                    mouseX = mb->GetX();
                    mouseY = mb->GetY();
                }
            }
            if (auto* mb = event->As<NkMouseButtonReleaseEvent>()) {
                if (mb->IsLeft()) mouseBtnHeld = false;
            }

            // ── Touch (Android) : boutons UP / DOWN / ENTER / ESCAPE / PAUSE ───────────
            // Test if touch is inside button or play area
            auto hitTest = [&](float tx, float ty,
                               const PongGame::TouchButtonRects& rects) {
                bool inEnter = tx >= rects.enterX && tx < rects.enterX + rects.enterW
                            && ty >= rects.enterY && ty < rects.enterY + rects.enterH;
                bool inEscape = tx >= rects.escapeX && tx < rects.escapeX + rects.escapeW
                             && ty >= rects.escapeY && ty < rects.escapeY + rects.escapeH;
                bool inPause = tx >= rects.pauseX && tx < rects.pauseX + rects.pauseW
                            && ty >= rects.pauseY && ty < rects.pauseY + rects.pauseH;
                return std::make_tuple(inEnter, inEscape, inPause);
            };

            if (auto* te = event->As<NkTouchBeginEvent>()) {
                auto rects = game->GetTouchButtonRects();
                for (uint32 i = 0; i < te->GetNumTouches(); ++i) {
                    const auto& pt = te->GetTouch(i);
                    auto [inEnter, inEscape, inPause] = hitTest(pt.clientX, pt.clientY, rects);
                    if (inEnter) ++touchEnterCount;
                    if (inEscape) ++touchEscapeCount;
                    if (inPause) ++touchPauseCount;
                    // Track first touch for gesture detection (swipe up/down for paddle)
                    if (i == 0 && !inEnter && !inEscape && !inPause) {
                        // Touch started in play area — track Y for vertical swipe
                        game->mTouchStartY    = pt.clientY;
                        game->mTouchCurrentY  = pt.clientY;
                        game->mTouchInProgress = true;
                    }
                }
                // First touch = mouse click for menu navigation
                if (te->GetNumTouches() > 0) {
                    const auto& pt = te->GetTouch(0);
                    mouseX      = static_cast<int>(pt.clientX);
                    mouseY      = static_cast<int>(pt.clientY);
                    mouseClicked = true;
                }
            }
            if (auto* te = event->As<NkTouchMoveEvent>()) {
                if (te->GetNumTouches() > 0 && game->mTouchInProgress) {
                    const auto& pt = te->GetTouch(0);
                    game->mTouchCurrentY = pt.clientY;
                }
            }
            if (auto* te = event->As<NkTouchEndEvent>()) {
                auto rects = game->GetTouchButtonRects();
                for (uint32 i = 0; i < te->GetNumTouches(); ++i) {
                    const auto& pt = te->GetTouch(i);
                    auto [inEnter, inEscape, inPause] = hitTest(pt.clientX, pt.clientY, rects);
                    if (inEnter && touchEnterCount > 0) --touchEnterCount;
                    if (inEscape && touchEscapeCount > 0) --touchEscapeCount;
                    if (inPause && touchPauseCount > 0) --touchPauseCount;
                }
                // End of gesture tracking
                game->mTouchInProgress = false;
            }
            if (event->As<NkTouchCancelEvent>()) {
                touchEnterCount = 0;
                touchEscapeCount = 0;
                touchPauseCount = 0;
                game->mTouchInProgress = false;
            }
        }
        if (!running)
        {
            break;
        }
        // ── Garde : surface Android non disponible (entre TERM_WINDOW et INIT_WINDOW) ──
        if (!surfaceReady)
        {
            NkChrono::Sleep(16.0f);
            continue;
        }
        if (pendingSize.width == 0 || pendingSize.height == 0)
        {
            continue;
        }

        if (needResize) {
            renderer->Resize(window, pendingSize.width, pendingSize.height);
            // Utiliser OnResize() au lieu de Init() pour conserver l'etat du jeu
            // Cela preserve le menu courant, les selections, les scores, etc.
            game->OnResize();
            needResize = false;
        }

        // Flancs montants
        enterPressed  = enterHeld  && !prevEnter;
        escapePressed = escapeHeld && !prevEscape;
        leftPressed   = leftHeld   && !prevLeft;
        rightPressed  = rightHeld  && !prevRight;
        prevEnter  = enterHeld;
        prevEscape = escapeHeld;
        prevLeft   = leftHeld;
        prevRight  = rightHeld;
        // Flanc montant souris (si pas déjà mis par le touch)
        if (!mouseClicked)
            mouseClicked = mouseBtnHeld && !prevMouseBtn;
        prevMouseBtn = mouseBtnHeld;

        // Quitter si le joueur sélectionne "QUITTER" dans le menu
        if (game->GetState() == GameState::MainMenu && enterPressed)
        {
            // Géré en interne – si mMainMenuSel==3 -> rien ne se passe côté jeu
            // On vérifie juste Escape au menu = fermer
        }

        // ── Update / Render / Present ─────────────────────────────────────
        // Merge keyboard + touch gesture inputs
        // Gesture detection: swipe down (positive delta) = up paddle, swipe up (negative delta) = down paddle
        // ── Détection de geste tactile (toujours paysage : swipe vertical) ────
        bool gestureUp   = false;
        bool gestureDown = false;
        if (game->mTouchInProgress) {
            const float SWIPE_THRESHOLD = 10.0f;
            float deltaY = game->mTouchCurrentY - game->mTouchStartY;
            if (deltaY < -SWIPE_THRESHOLD)     gestureUp   = true;
            else if (deltaY > SWIPE_THRESHOLD) gestureDown = true;
        }

        bool finalUp    = upHeld   || gestureUp;
        bool finalDown  = downHeld || gestureDown;
        bool finalLeft  = leftHeld;
        bool finalRight = rightHeld;
        bool finalEnter   = enterHeld || (touchEnterCount > 0);
        bool finalEscape  = escapeHeld || (touchEscapeCount > 0);
        bool finalPause   = touchPauseCount > 0;
        
        // Détecter les flancs montants pour les entrées tactiles
        static bool prevEnterTouch = false;
        static bool prevEscapeTouch = false;
        static bool prevPauseTouch = false;
        
        if (finalEnter && !prevEnterTouch)
            enterPressed = true;
        if (finalEscape && !prevEscapeTouch)
            escapePressed = true;
        // Pause est traitée comme Escape (pause du jeu)
        if (finalPause && !prevPauseTouch)
            escapePressed = true;
            
        prevEnterTouch = finalEnter;
        prevEscapeTouch = finalEscape;
        prevPauseTouch = finalPause;
        
        game->Update(dt, finalUp, finalDown,
                     enterPressed, escapePressed,
                     finalLeft, finalRight,
                     mouseX, mouseY, mouseClicked);

        if (game->WantsQuit()) {
            running = false;
            window.Close();
            break;
        }
        
        // ── Quitter depuis le menu principal (Android) ───────────────────────
#if defined(__ANDROID__)
        if (game->GetState() == GameState::MainMenu && escapePressed) {
            running = false;
            window.Close();
            break;
        }
#endif
        
        // ── Restaurer l'ecran apres un pause/resume (Android) ────────────────
        // Au retour de l'app, forcer un rendu complet et clear du buffer
        if (appResumed)
        {
            // Force multiple clears to ensure buffer is completely refreshed after resume
            for (int i = 0; i < 2; ++i) {
                renderer->Clear({ 0, 0, 0, 255 });
                game->Render();
            }
            appResumed = false;
        }
        else
        {
            game->Render();
        }
        renderer->Present();

        // ── Cap 60 FPS ─────────────────────────────────────────────────────
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
        {
            NkChrono::Sleep(16 - elapsed.milliseconds);
        }
        else
        {
            NkChrono::YieldThread();
        }
    }

    delete game;
    delete renderer;
    window.Close();
    return 0;
}
