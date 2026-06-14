// =============================================================================
// NkImageDemo/main.cpp — Point d'entree.
// -----------------------------------------------------------------------------
// Cree une fenetre + ViewerApp + boucle main avec gestion lifecycle Windows.
// Boucle 60 fps cappee, dispatch des events vers ViewerApp::OnEvent.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkEventSystem.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"

#include "Demo/ViewerApp.h"

using namespace nkentseu;
using nkentseu::demo::ViewerApp;

int nkmain(const NkEntryState& state)
{
    (void)state;

    NkWindowConfig cfg;
    cfg.title       = "NkImageDemo - Viewer NKImage";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = false;

    NkWindow window(cfg);
    if (!window.IsOpen())
    {
        logger.Error("[NkImageDemo] Window creation failed");
        return -1;
    }

    ViewerApp app(window);
    if (!app.Init())
    {
        logger.Error("[NkImageDemo] ViewerApp::Init failed");
        window.Close();
        return -2;
    }

    auto& events = NkEvents();
    NkChrono chrono;
    NkElapsedTime elapsed;

    while (window.IsOpen())
    {
        elapsed = chrono.Reset();
        float dt = static_cast<float>(elapsed.seconds);
        if (dt <= 0.f || dt > 0.25f) dt = 1.0f / 60.0f;

        while (NkEvent* ev = events.PollEvent())
        {
            if (ev->Is<NkWindowCloseEvent>())
            {
                window.Close();
                break;
            }
            if (auto* wr = ev->As<NkWindowResizeEvent>())
            {
                app.OnResize(wr->GetWidth(), wr->GetHeight());
            }
            app.OnEvent(*ev);
        }
        if (!window.IsOpen()) break;

        app.Update(dt);
        app.Render();

        if (app.WantsQuit())
        {
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
