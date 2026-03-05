// ============================================================================
// TEST MINIMAL — fenêtre + events seulement, rien d'autre
// Objectif : confirmer si OUI ou NON les events remontent dans PollEvent()
// ============================================================================
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"        // définit NkEvents() et inclut les événements
#include "NKRenderer/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"

#include <iostream>
#include <fstream>

static std::ofstream gLog("sandbox_diag.log", std::ios::trunc);
#define LOG(x) do { std::cout << x << "\n"; std::cout.flush(); \
                    gLog      << x << "\n"; gLog.flush(); } while(0)

int nkmain(const nkentseu::NkEntryState&)
{
    using namespace nkentseu;

    LOG("[1] nkmain start");

    // --- Init ---
    NkAppData app;
    app.appName = "Test";
    if (!NkInitialise(app)) { LOG("[FATAL] NkInitialise failed"); return -1; }
    LOG("[2] NkInitialise OK  platform=" << NkEvents().GetPlatformName());

    // --- Fenetre ---
    NkWindowConfig cfg;
    cfg.title  = "Test Events";
    cfg.width  = 800;
    cfg.height = 600;

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        LOG("[FATAL] Window failed: " << window.GetLastError().ToString());
        NkClose();
        return -2;
    }
    LOG("[3] Window open OK");

    // --- Boucle minimale ---
    auto& es = NkEvents();
    int frames = 0;
    int events = 0;
    bool running = true;

    LOG("[4] Entering loop — close the window to exit");

    while (running)
    {
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            ++events;
            LOG("[EVENT #" << events << "] type=" << (int)ev->GetType());

            if (ev->Is<NkWindowCloseEvent>()) {
                LOG("[EVENT] → WindowClose, quitting");
                running = false;
                window.Close();
                break;
            }
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                LOG("[EVENT] → KeyPress key=" << (int)k->GetKey());
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
            LOG("[LOOP] alive frames=" << frames << " events=" << events);
    }

    LOG("[5] Loop exited frames=" << frames << " events=" << events);
    NkClose();
    LOG("[6] Done");
    return 0;
}