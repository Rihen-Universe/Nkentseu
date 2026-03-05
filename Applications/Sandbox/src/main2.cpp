// =============================================================================
// ex01_diagnostic.cpp
// TEST MINIMAL — Fenêtre + events seulement
// Objectif : confirmer que PollEvent() remonte bien les events.
//            Loggue chaque événement reçu dans la console ET dans un fichier.
// Couvre  : NkSystem lifecycle, NkEventSystem::PollEvent, NkWindowCloseEvent,
//           NkKeyPressEvent, NkKeyReleaseEvent, NkWindowResizeEvent,
//           NkWindowFocusGainedEvent, NkWindowFocusLostEvent, NkWindowMoveEvent
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvents.h"        // définit NkEvents() et inclut les événements
#include "NKRenderer/NkRenderer.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"

#include <cstdio>
#include <fstream>
#include <string>

// ---------------------------------------------------------------------------
// Logger dual (console + fichier)
// ---------------------------------------------------------------------------
static std::ofstream gLog("ex01_diagnostic.log", std::ios::trunc);

#define LOG(x)                                    \
    do {                                          \
        std::printf("%s\n", (std::string("") + x).c_str()); \
        gLog << x << "\n";                        \
        gLog.flush();                             \
    } while (0)

// ---------------------------------------------------------------------------
// Utilitaires de debug
// ---------------------------------------------------------------------------
static const char* EventTypeName(nkentseu::NkEventType::Value t) {
    using namespace nkentseu;
    switch (t) {
        case NkEventType::NK_WINDOW_CLOSE:               return "WindowClose";
        case NkEventType::NK_WINDOW_RESIZE:               return "WindowResize";
        case NkEventType::NK_WINDOW_FOCUS_GAINED:         return "WindowFocusGained";
        case NkEventType::NK_WINDOW_FOCUS_LOST:           return "WindowFocusLost";
        case NkEventType::NK_WINDOW_MOVE:                 return "WindowMove";
        case NkEventType::NK_KEY_PRESSED:                 return "KeyPress";
        case NkEventType::NK_KEY_RELEASED:                return "KeyRelease";
        case NkEventType::NK_TEXT_INPUT:                  return "TextInput";
        case NkEventType::NK_MOUSE_BUTTON_PRESSED:        return "MousePress";
        case NkEventType::NK_MOUSE_BUTTON_RELEASED:       return "MouseRelease";
        case NkEventType::NK_MOUSE_MOVE:                  return "MouseMove";
        case NkEventType::NK_MOUSE_WHEEL_VERTICAL:        return "MouseWheelVertical";
        case NkEventType::NK_MOUSE_WHEEL_HORIZONTAL:      return "MouseWheelHorizontal";
        case NkEventType::NK_MOUSE_ENTER:                 return "MouseEnter";
        case NkEventType::NK_MOUSE_LEAVE:                 return "MouseLeave";
        case NkEventType::NK_DROP_FILE:                   return "DropFile";
        case NkEventType::NK_DROP_TEXT:                   return "DropText";
        case NkEventType::NK_DROP_ENTER:                  return "DropEnter";
        case NkEventType::NK_DROP_LEAVE:                  return "DropLeave";
        case NkEventType::NK_GAMEPAD_CONNECT:             return "GamepadConnect";
        case NkEventType::NK_GAMEPAD_AXIS_MOTION:         return "GamepadAxis";
        case NkEventType::NK_GAMEPAD_BUTTON_PRESSED:      return "GamepadButtonPress";
        default:                                           return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int nkmain(const nkentseu::NkEntryState& /*state*/)
{
    using namespace nkentseu;

    LOG("=== ex01_diagnostic — NkWindow event diagnostic ===");

    // -----------------------------------------------------------------------
    // 1. Init système
    // -----------------------------------------------------------------------
    NkAppData app;
    app.appName = "ex01_diagnostic";

    if (!NkInitialise(app)) {
        LOG("[FATAL] NkInitialise failed");
        return -1;
    }
    LOG("[1] NkInitialise OK");

    // -----------------------------------------------------------------------
    // 2. Fenêtre
    // -----------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "ex01 — Diagnostic Events";
    cfg.width       = 800;
    cfg.height      = 600;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;  // active DnD pour observer NK_DROP_*

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        LOG("[FATAL] Window creation failed");
        NkClose();
        return -2;
    }
    LOG("[2] Window open — valid: " + std::string(window.IsValid() ? "yes" : "no"));

    // -----------------------------------------------------------------------
    // 3. Boucle diagnostique
    //    — affiche chaque event avec son type, son windowId, et ses données
    // -----------------------------------------------------------------------
    auto& es      = NkEvents();
    int   frames  = 0;
    int   evCount = 0;
    bool  running = true;

    LOG("[3] Entering event loop — close window or press ESC to exit");
    LOG("    Interact : move/resize/focus/unfocus/drop files/press keys");

    while (running && window.IsOpen())
    {
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            ++evCount;
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[EVENT #%3d] type=%-22s winId=%llu",
                evCount,
                EventTypeName(ev->GetType()),
                static_cast<unsigned long long>(ev->GetWindowId()));
            std::string line = buf;

            // Détail selon le type
            if (auto* e = ev->As<NkWindowCloseEvent>()) {
                (void)e;
                line += " → CLOSE";
                running = false;
                window.Close();
            }
            else if (auto* e = ev->As<NkWindowResizeEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → %ux%u", e->GetWidth(), e->GetHeight());
                line += d;
            }
            else if (auto* e = ev->As<NkWindowFocusGainedEvent>()) {
                line += " → GAINED";
            }
            else if (auto* e = ev->As<NkWindowFocusLostEvent>()) {
                line += " → LOST";
            }
            else if (auto* e = ev->As<NkWindowMoveEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → (%d,%d)", e->GetX(), e->GetY());
                line += d;
            }
            else if (auto* e = ev->As<NkKeyPressEvent>()) {
                // Affichage des modificateurs sous forme de chaîne (ToString)
                std::string modStr = e->GetModifiers().ToString();
                char d[128];
                std::snprintf(d, sizeof(d), " → key=%d mods=%s",
                    static_cast<int>(e->GetKey()), modStr.c_str());
                line += d;
                if (e->GetKey() == NkKey::NK_ESCAPE) {
                    line += " [ESC → exit]";
                    running = false;
                    window.Close();
                }
            }
            else if (auto* e = ev->As<NkKeyReleaseEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → key=%d", static_cast<int>(e->GetKey()));
                line += d;
            }
            else if (auto* e = ev->As<NkMouseMoveEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → pos=(%d,%d) delta=(%d,%d)",
                    e->GetX(), e->GetY(), e->GetDeltaX(), e->GetDeltaY());
                line += d;
            }
            else if (auto* e = ev->As<NkMouseButtonPressEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → btn=%d pos=(%d,%d)",
                    static_cast<int>(e->GetButton()), e->GetX(), e->GetY());
                line += d;
            }
            else if (auto* e = ev->As<NkMouseWheelVerticalEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → delta=%.2f", e->GetDeltaY());
                line += d;
            }
            else if (auto* e = ev->As<NkDropFileEvent>()) {
                line += " → files=[";
                for (const auto& f : e->data.paths)  // CORRECTION : utilisation de data.paths
                    line += f + ", ";
                line += "]";
            }
            else if (auto* e = ev->As<NkDropTextEvent>()) {
                line += " → text=" + e->data.text.substr(0, 40);  // CORRECTION : data.text
            }
            else if (auto* e = ev->As<NkDropEnterEvent>()) {
                char d[64];
                std::snprintf(d, sizeof(d), " → pos=(%d,%d)", e->data.x, e->data.y);  // CORRECTION : data.x et data.y
                line += d;
            }

            LOG(line);

            if (!running) break;
        }

        ++frames;
        if (frames % 300 == 0)
            LOG("[HEARTBEAT] frames=" + std::to_string(frames) +
                " events=" + std::to_string(evCount));
    }

    LOG("[4] Loop done — frames=" + std::to_string(frames) +
        " totalEvents=" + std::to_string(evCount));

    NkClose();
    LOG("[5] NkClose OK — see ex01_diagnostic.log for full log");
    return 0;
}