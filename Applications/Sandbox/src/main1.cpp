// ============================================================================
// TEST MINIMAL — fenêtre + events seulement, rien d'autre
// Objectif : confirmer si OUI ou NON les events remontent dans PollEvent()
// ============================================================================
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"        // définit NkEvents() et inclut les événements
#include "NKRenderer/Deprecate/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"

#include "NKLogger/NkLog.h"

#include <iostream>
#include <fstream>

// AppData pattern #2: named variable initialized by lambda.
nkentseu::NkAppData appData = [] {
    nkentseu::NkAppData d{};
    d.appName = "SandboxDBase";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);


int nkmain(const nkentseu::NkEntryState&)
{
    using namespace nkentseu;

    logger.Info("[1] nkmain start");

    // --- Runtime already initialized by NkMain entrypoint ---
    logger.Info("[2] Runtime already initialized  platform={0}", NkEvents().GetPlatformName());

    // --- Fenetre ---
    NkWindowConfig cfg;
    cfg.title  = "Test Events";
    cfg.width  = 800;
    cfg.height = 600;

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[FATAL] Window failed: {0}", window.GetLastError().ToString());
        return -2;
    }
    logger.Info("[3] Window open OK");

    // --- Boucle minimale ---
    auto& es = NkEvents();
    int frames = 0;
    int events = 0;
    bool running = true;

    logger.Info("[4] Entering loop — close the window to exit");

    while (running)
    {
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            ++events;
            logger.Info("[EVENT #{0}] type={1}", events, static_cast<int>(ev->GetType()));

            if (ev->Is<NkWindowCloseEvent>()) {
                logger.Info("[EVENT] → WindowClose, quitting");
                running = false;
                window.Close();
                break;
            }
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                logger.Info("[EVENT] → KeyPress key={0}", static_cast<int>(k->GetKey()));
                if (k->GetKey() == NkKey::NK_ESCAPE) {
                    running = false;
                    window.Close();
                    break;
                }
            }
        }

        if (!running || !window.IsOpen()) break;

        ++frames;
        if (frames % 600 == 0)
            logger.Info("[LOOP] alive frames={0} events={1}", frames, events);
    }

    logger.Info("[5] Loop exited frames={0} events={1}", frames, events);
    logger.Info("[6] Done");
    return 0;
}
