#include "NKPlatform/NkPlatformDetect.h"

#include "NKPlatform/NkFoundationLog.h"

// ============================================================
// Exemple : Utilisation du Système de Fenêtres (sans STL)
// ============================================================

class WindowSystem {
public:
    WindowSystem() : mSystemName("Unknown/None") {
        DetectWindowingSystem();
    }

    void DisplayInfo() const {
        NK_FOUNDATION_LOG_INFO("=====================================\n");
        NK_FOUNDATION_LOG_INFO("Windowing System Information\n");
        NK_FOUNDATION_LOG_INFO("=====================================\n");
        NK_FOUNDATION_LOG_INFO("Detected System: %s\n", mSystemName);

#ifdef NKENTSEU_WINDOWING_PREFERRED
        NK_FOUNDATION_LOG_INFO("Preferred System: %s\n", NKENTSEU_WINDOWING_PREFERRED);
#endif

        NK_FOUNDATION_LOG_INFO("Capabilities:\n");

#ifdef NKENTSEU_WINDOWING_XCB
        NK_FOUNDATION_LOG_INFO("  [OK] XCB (X11 Core Protocol)\n");
#else
        NK_FOUNDATION_LOG_INFO("  [NO] XCB (X11 Core Protocol)\n");
#endif

#ifdef NKENTSEU_WINDOWING_XLIB
        NK_FOUNDATION_LOG_INFO("  [OK] Xlib (X11 Legacy)\n");
#else
        NK_FOUNDATION_LOG_INFO("  [NO] Xlib (X11 Legacy)\n");
#endif

#ifdef NKENTSEU_WINDOWING_WAYLAND
        NK_FOUNDATION_LOG_INFO("  [OK] Wayland\n");
#else
        NK_FOUNDATION_LOG_INFO("  [NO] Wayland\n");
#endif

#ifdef NKENTSEU_WINDOWING_X11
        NK_FOUNDATION_LOG_INFO("  [OK] X11 (XCB or Xlib)\n");
#else
        NK_FOUNDATION_LOG_INFO("  [NO] X11 (XCB or Xlib)\n");
#endif

        NK_FOUNDATION_LOG_INFO("=====================================\n");
    }

    void InitializeWindow() {
        NK_FOUNDATION_LOG_INFO("\nInitializing window with: %s\n", mSystemName);
        NK_FOUNDATION_LOG_INFO("----------------------------------------\n");

        NKENTSEU_XCB_ONLY({
            InitializeXCB();
        });

        NKENTSEU_XLIB_ONLY({
            InitializeXlib();
        });

        NKENTSEU_WAYLAND_ONLY({
            InitializeWayland();
        });

        NKENTSEU_NOT_X11({
            NK_FOUNDATION_LOG_INFO("Not using X11-based windowing\n");
        });
    }

    void Cleanup() {
        NK_FOUNDATION_LOG_INFO("\nCleaning up %s resources...\n", mSystemName);

        NKENTSEU_XCB_ONLY({
            CleanupXCB();
        });

        NKENTSEU_XLIB_ONLY({
            CleanupXlib();
        });

        NKENTSEU_WAYLAND_ONLY({
            CleanupWayland();
        });

        NK_FOUNDATION_LOG_INFO("Cleanup complete.\n");
    }

private:
    const char* mSystemName;

    void DetectWindowingSystem() {
#if defined(NKENTSEU_WINDOWING_XCB)
        mSystemName = "XCB";
#elif defined(NKENTSEU_WINDOWING_XLIB)
        mSystemName = "Xlib";
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
        mSystemName = "Wayland";
#else
        mSystemName = "Unknown/None";
#endif
    }

    void InitializeXCB() {
        NK_FOUNDATION_LOG_INFO("  - Connecting to X Server via XCB...\n");
        NK_FOUNDATION_LOG_INFO("  - Modern X11 Core Protocol (XCB)\n");
        NK_FOUNDATION_LOG_INFO("  - Better performance than Xlib\n");
    }

    void InitializeXlib() {
        NK_FOUNDATION_LOG_INFO("  - Connecting to X Server via Xlib...\n");
        NK_FOUNDATION_LOG_INFO("  - Legacy X11 Client Library (Xlib)\n");
        NK_FOUNDATION_LOG_INFO("  - Wider compatibility, older API\n");
    }

    void InitializeWayland() {
        NK_FOUNDATION_LOG_INFO("  - Connecting to Wayland Compositor...\n");
        NK_FOUNDATION_LOG_INFO("  - Modern Display Protocol (Wayland)\n");
        NK_FOUNDATION_LOG_INFO("  - Future-proof, simpler architecture\n");
    }

    void CleanupXCB() {
        NK_FOUNDATION_LOG_INFO("  - Disconnecting from X Server (XCB)\n");
    }

    void CleanupXlib() {
        NK_FOUNDATION_LOG_INFO("  - Closing X11 Display (Xlib)\n");
    }

    void CleanupWayland() {
        NK_FOUNDATION_LOG_INFO("  - Disconnecting from Wayland Compositor\n");
    }
};

static void DemonstrateConditionalCode() {
    NK_FOUNDATION_LOG_INFO("\n\nConditional Code Execution Examples:\n");
    NK_FOUNDATION_LOG_INFO("----------------------------------------\n");

    NKENTSEU_XCB_ONLY({
        NK_FOUNDATION_LOG_INFO("[XCB] This code runs only when XCB is enabled\n");
    });

    NKENTSEU_XLIB_ONLY({
        NK_FOUNDATION_LOG_INFO("[Xlib] This code runs only when Xlib is enabled\n");
    });

    NKENTSEU_WAYLAND_ONLY({
        NK_FOUNDATION_LOG_INFO("[Wayland] This code runs only when Wayland is enabled\n");
    });

    NKENTSEU_X11_ONLY({
        NK_FOUNDATION_LOG_INFO("[X11] This code runs for XCB or Xlib (but not Wayland)\n");
    });

    NKENTSEU_NOT_XCB({
        NK_FOUNDATION_LOG_INFO("[Not XCB] This code runs for Xlib or Wayland\n");
    });

    NKENTSEU_NOT_WAYLAND({
        NK_FOUNDATION_LOG_INFO("[Not Wayland] This code runs for XCB or Xlib\n");
    });
}

int main() {
    WindowSystem ws;

    ws.DisplayInfo();
    DemonstrateConditionalCode();
    ws.InitializeWindow();
    ws.Cleanup();

    NK_FOUNDATION_LOG_INFO("\nExample completed successfully!\n");
    return 0;
}
