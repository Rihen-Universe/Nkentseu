#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"

using namespace nkentseu;

// =============================================================================
// Point d'entrée — nkmain (appelé par NkMetalEntryPoint.mm sur Apple,
//                           ou directement depuis main() sur Windows/Linux)
// =============================================================================
int nkmain(const NkEntryState& state) {

    // -------------------------------------------------------------------------
    // 2. Fenetre
    // -------------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "Vulkna-TP01";
    cfg.width       = 1280;
    cfg.height      = 720;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[Sandbox] Window creation FAILED");
        NkContextShutdown();
        return -2;
    }

    // -------------------------------------------------------------------------
    // 5. Boucle principale
    // -------------------------------------------------------------------------
    auto& eventSystem = NkEvents();

    bool running = true;
    float timeSeconds = 0.f;
    NkChrono chrono;
    NkElapsedTime elapsed;

    while (running)
    {
        NkElapsedTime e = chrono.Reset();

        // --- Pattern A : Dispatcher typA (OnEvent pour chaque event)
        while (NkEvent* event = eventSystem.PollEvent())
        {
            if (auto wcl = event->As<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
        }

        if (!running) break;

        // --- Cap 60 fps ---
        elapsed = chrono.Elapsed();
        if (elapsed.milliseconds < 16)
            NkChrono::Sleep(16 - elapsed.milliseconds);
        else
            NkChrono::YieldThread();
    }
    window.Close();
    return 0;
}