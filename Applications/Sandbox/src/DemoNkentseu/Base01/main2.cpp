// =============================================================================
// ex01_diagnostic.cpp
// TEST MINIMAL - FenAtre + events seulement
// Objectif : confirmer que PollEvent() remonte bien les events.
//            Loggue chaque AvAnement reAu dans la console ET dans un fichier.
// Couvre  : NkSystem lifecycle, NkEventSystem::PollEvent, NkWindowCloseEvent,
//           NkKeyPressEvent, NkKeyReleaseEvent, NkWindowResizeEvent,
//           NkWindowFocusGainedEvent, NkWindowFocusLostEvent, NkWindowMoveEvent
// =============================================================================

#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkEvent.h"        // dAfinit NkEvents() et inclut les AvAnements
#include "NKWindow/Core/NkContext.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkMain.h"

#include "NKLogger/NkLog.h"

#include "NKMemory/NkMemory.h"

#include <cstdio>
#include <fstream>
#include <string>

// AppData pattern #3: explicit updater callback registration.
static void ConfigureDiagnosticAppData(nkentseu::NkAppData& d) {
    d.appName = "SandboxDiagnostic";
    d.appVersion = "1.0.0";
    d.enableEventLogging = true;
    d.enableRendererDebug = true;
    d.enableMultiWindow = true;
}
NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureDiagnosticAppData);

// ---------------------------------------------------------------------------
// Logger dual (console + fichier)
// ---------------------------------------------------------------------------
#define gLog logger;
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

    logger.Info("=== ex01_diagnostic - NkWindow event diagnostic ===");

    // -----------------------------------------------------------------------
    // 1. Runtime dAjA  initialisA par l'entrypoint NkMain
    // -----------------------------------------------------------------------
    logger.Info("[1] Runtime already initialized");

    NkContextInit();
    NkContextConfig contextConfig{};
    contextConfig.api = NkRendererApi::NK_SOFTWARE;
    contextConfig.vsync = true;
    contextConfig.debug = false;
    NkContextSetHints(contextConfig);
    contextConfig = NkContextGetHints();

    // -----------------------------------------------------------------------
    // 2. FenAtre
    // -----------------------------------------------------------------------
    NkWindowConfig cfg;
    cfg.title       = "ex01 - Diagnostic Events";
    cfg.width       = 800;
    cfg.height      = 600;
    cfg.centered    = true;
    cfg.resizable   = true;
    cfg.dropEnabled = true;  // active DnD pour observer NK_DROP_*
    NkContextApplyWindowHints(cfg);

    NkWindow window(cfg);
    if (!window.IsOpen()) {
        logger.Error("[FATAL] Window creation failed");
        NkContextShutdown();
        return -2;
    }
    NkContext context{};
    if (!NkContextCreate(window, context, &contextConfig)) {
        logger.Error("[FATAL] Context creation failed: {0}", context.lastError.ToString());
        window.Close();
        NkContextShutdown();
        return -3;
    }
    logger.Info("[2] Window open - valid: {0}", window.IsValid() ? "yes" : "no");

    // -----------------------------------------------------------------------
    // 3. Boucle diagnostique
    //    - affiche chaque event avec son type, son windowId, et ses donnAes
    // -----------------------------------------------------------------------
    auto& es      = NkEvents();
    int   frames  = 0;
    int   evCount = 0;
    bool  running = true;

    logger.Info("[3] Entering event loop - close window or press ESC to exit");
    logger.Info("    Interact : move/resize/focus/unfocus/drop files/press keys");

    while (running && window.IsOpen())
    {
        NkEvent* ev = nullptr;
        while ((ev = es.PollEvent()) != nullptr)
        {
            ++evCount;
            char buf[256];
            logger.Info("[EVENT] type={0} winId={1}", EventTypeName(ev->GetType()), ev->GetWindowId());
            
            NkString line = buf;

            // DAtail selon le type
            if (auto* e = ev->As<NkWindowCloseEvent>()) {
                (void)e;
                line += " -> CLOSE";
                running = false;
                window.Close();
            }
            else if (auto* e = ev->As<NkWindowResizeEvent>()) {
                char d[64];
                logger.Info("Resize event: new size = {0}x{1}", e->GetWidth(), e->GetHeight());
                line += d;
            }
            else if (auto* e = ev->As<NkWindowFocusGainedEvent>()) {
                line += " -> GAINED";
            }
            else if (auto* e = ev->As<NkWindowFocusLostEvent>()) {
                line += " -> LOST";
            }
            else if (auto* e = ev->As<NkWindowMoveEvent>()) {
                char d[64];
                logger.Info("Move event: new pos = ({0},{1})", e->GetX(), e->GetY());
                line += d;
            }
            else if (auto* e = ev->As<NkKeyPressEvent>()) {
                // Affichage des modificateurs sous forme de chaAne (ToString)
                NkString modStr = e->GetModifiers().ToString();
                char d[128];
                logger.Info("KeyPress event: key={0} mods={1}", static_cast<int>(e->GetKey()), modStr);
                
                line += d;
                if (e->GetKey() == NkKey::NK_ESCAPE) {
                    line += " [ESC -> exit]";
                    running = false;
                    window.Close();
                }
            }
            else if (auto* e = ev->As<NkKeyReleaseEvent>()) {
                char d[64];
                logger.Info("KeyRelease event: key={0}", static_cast<int>(e->GetKey()));
                line += d;
            }
            else if (auto* e = ev->As<NkMouseMoveEvent>()) {
                char d[64];
                logger.Info("MouseMove event: pos=({0},{1}) delta=({2},{3})", e->GetX(), e->GetY(), e->GetDeltaX(), e->GetDeltaY());
                line += d;
            }
            else if (auto* e = ev->As<NkMouseButtonPressEvent>()) {
                char d[64];
                logger.Info("MouseButtonPress event: btn={0} pos=({1},{2})", static_cast<int>(e->GetButton()), e->GetX(), e->GetY());
                line += d;
            }
            else if (auto* e = ev->As<NkMouseWheelVerticalEvent>()) {
                char d[64];
                logger.Info("MouseWheelVertical event: delta={0:.2f}", e->GetDeltaY());
                line += d;
            }
            else if (auto* e = ev->As<NkDropFileEvent>()) {
                line += " -> files=[";
                for (const auto& f : e->data.paths)  // CORRECTION : utilisation de data.paths
                    line += f + ", ";
                line += "]";
            }
            else if (auto* e = ev->As<NkDropTextEvent>()) {
                line += " -> text=" + e->data.text.SubStr(0, 40);  // CORRECTION : data.text
            }
            else if (auto* e = ev->As<NkDropEnterEvent>()) {
                char d[64];
                logger.Info("DropEnter event: pos=({0},{1})", e->data.x, e->data.y);  // CORRECTION : data.x et data.y
                line += d;
            }

            logger.Info(line);

            if (!running) break;
        }

        ++frames;
        if (frames % 300 == 0)
            logger.Info("[HEARTBEAT] frames={0} events={1}", frames, evCount);
    }

    logger.Info("[4] Loop done - frames={0} totalEvents={1}", frames, evCount);

    logger.Info("[5] Runtime shutdown handled by entrypoint - see ex01_diagnostic.log for full log");
    NkContextDestroy(context);
    window.Close();
    NkContextShutdown();
    return 0;
}
