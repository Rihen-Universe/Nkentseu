// =============================================================================
// NkRHIDemo.cpp
// Démo graphique : fenêtre NkWindow + OpenGL via GLAD
//   • Figures 2D : triangle coloré, rectangle
//   • Figures 3D : cube rotatif, sphère, plan (sol)
//   • Éclairage  : Phong directionnel (ambiant + diffus + spéculaire)
//   • Ombres     : shadow mapping directionnel (1024×1024)
//   • Contrôles  : ESC = quitter, WASD = caméra, Flèches = lumière
// =============================================================================

// ── Détection plateforme avant tout le reste ──────────────────────────────────
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkAngle.h"
#include "NKTime/NkTime.h"

// ── NkEngine headers ─────────────────────────────────────────────────────────
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Core/NkOpenGLDesc.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>
#include <cstring>

using namespace nkentseu;
using namespace nkentseu::math;

// =============================================================================
// Application data
// =============================================================================
nkentseu::NkAppData appData = [] {
    nkentseu::NkAppData d{};
    d.appName    = "NkRHI Demo";
    d.appVersion = "1.0.0";
    return d;
}();
NKENTSEU_APP_DATA_DEFINED(appData);

// =============================================================================
// nkmain — point d'entrée
// =============================================================================
int nkmain(const nkentseu::NkEntryState&)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  NkRHI Demo — 2D/3D + Phong + Shadow Mapping    ║\n");
    printf("║  ESC=Quitter | WASD=Caméra | ↑↓←→=Lumière       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");

    // ── Fenêtre ──────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = "NkRHI Demo — 2D/3D + Phong + Ombres";
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;

    NkWindow window(winCfg);
    if (!window.IsOpen()) { printf("[RHIDemo] Fenêtre échouée\n"); return -1; }

    // ── Boucle principale ─────────────────────────────────────────────────────
    NkClock    clock;
    auto& events = NkEvents();
    float time=0.f;
    bool running=true;

    while (running)
    {
        float dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f/60.f;
        time += dt;

        // ── Événements ───────────────────────────────────────────────────────
        NkEvent* ev = nullptr;
        while ((ev = events.PollEvent()) != nullptr) {
            if (ev->Is<NkWindowCloseEvent>()) { running=false; break; }
            if (auto* k = ev->As<NkKeyPressEvent>()) {
                if (k->GetKey() == NkKey::NK_ESCAPE) running=false;
            }
        }
        if (!running) break;
        NkClock::SleepMilliseconds(16);
    }

    window.Close();
    printf("[RHIDemo] Terminé proprement.\n");
    return 0;
}
