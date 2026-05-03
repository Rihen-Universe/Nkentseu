// =============================================================================
// Apps.cpp  –  Point d'entrée nkmain avec gestion complète des inputs
// =============================================================================
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/NkWindow.h"
#include "NKLogger/NkLog.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "PongGame.h"
#include "NKTime/NkTime.h"

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
    game->Init();

#if defined(__ANDROID__)
    game->SetShowTouchButtons(true);
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
    int  touchUpCount   = 0;
    int  touchDownCount = 0;
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
            auto hitTest = [&](float tx, float ty,
                               const PongGame::TouchButtonRects& rects) {
                bool inUp = tx >= rects.upX && tx < rects.upX + rects.upW
                         && ty >= rects.upY && ty < rects.upY + rects.upH;
                bool inDn = tx >= rects.dnX && tx < rects.dnX + rects.dnW
                         && ty >= rects.dnY && ty < rects.dnY + rects.dnH;
                bool inEnter = tx >= rects.enterX && tx < rects.enterX + rects.enterW
                            && ty >= rects.enterY && ty < rects.enterY + rects.enterH;
                bool inEscape = tx >= rects.escapeX && tx < rects.escapeX + rects.escapeW
                             && ty >= rects.escapeY && ty < rects.escapeY + rects.escapeH;
                bool inPause = tx >= rects.pauseX && tx < rects.pauseX + rects.pauseW
                            && ty >= rects.pauseY && ty < rects.pauseY + rects.pauseH;
                return std::make_tuple(inUp, inDn, inEnter, inEscape, inPause);
            };

            if (auto* te = event->As<NkTouchBeginEvent>()) {
                auto rects = game->GetTouchButtonRects();
                for (uint32 i = 0; i < te->GetNumTouches(); ++i) {
                    const auto& pt = te->GetTouch(i);
                    auto [inUp, inDn, inEnter, inEscape, inPause] = hitTest(pt.clientX, pt.clientY, rects);
                    if (inUp) ++touchUpCount;
                    if (inDn) ++touchDownCount;
                    if (inEnter) ++touchEnterCount;
                    if (inEscape) ++touchEscapeCount;
                    if (inPause) ++touchPauseCount;
                }
                // Premier toucher = clic souris pour navigation dans les menus
                if (te->GetNumTouches() > 0) {
                    const auto& pt = te->GetTouch(0);
                    mouseX      = static_cast<int>(pt.clientX);
                    mouseY      = static_cast<int>(pt.clientY);
                    mouseClicked = true;
                }
            }
            if (auto* te = event->As<NkTouchMoveEvent>()) {
                // On move, re-evaluate: clear then recount for moved touches
                (void)te;
            }
            if (auto* te = event->As<NkTouchEndEvent>()) {
                auto rects = game->GetTouchButtonRects();
                for (uint32 i = 0; i < te->GetNumTouches(); ++i) {
                    const auto& pt = te->GetTouch(i);
                    auto [inUp, inDn, inEnter, inEscape, inPause] = hitTest(pt.clientX, pt.clientY, rects);
                    if (inUp && touchUpCount   > 0) --touchUpCount;
                    if (inDn && touchDownCount > 0) --touchDownCount;
                    if (inEnter && touchEnterCount > 0) --touchEnterCount;
                    if (inEscape && touchEscapeCount > 0) --touchEscapeCount;
                    if (inPause && touchPauseCount > 0) --touchPauseCount;
                }
            }
            if (event->As<NkTouchCancelEvent>()) {
                touchUpCount   = 0;
                touchDownCount = 0;
                touchEnterCount = 0;
                touchEscapeCount = 0;
                touchPauseCount = 0;
            }
        }
        if (!running)
        {
            break;
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
        // Merge keyboard + touch button inputs (pour Up/Down/Enter/Escape/Pause)
        bool finalUp      = upHeld   || (touchUpCount   > 0);
        bool finalDown    = downHeld || (touchDownCount > 0);
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
                     leftPressed, rightPressed,
                     mouseX, mouseY, mouseClicked);
        
        // ── Restaurer l'ecran apres un pause/resume (Android) ────────────────
        // Au retour de l'app, forcer un rendu complet et clear du buffer
        if (appResumed)
        {
            // Effacer completement le backbuffer pour eviter un ecran noir au retour
            renderer->Clear({ 0, 0, 0, 255 });
            appResumed = false;
        }
        
        game->Render();
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