// =============================================================================
// Apps.cpp -- Point d'entree de NkCameraDemos.
//
// Parse --demo=<name> dans state.args et delegue a la demo correspondante.
//   --demo=viewer  : flux camera 0 plein ecran (defaut)
//   --demo=multi   : grille 2x2 de cameras
//   --demo=format  : test cross-format ConvertToRGBA8 (frames synthetiques)
//
// Le pattern reproduit Renderer2dExample : nkmain unique, NKENTSEU_DEFINE_APP_DATA.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/Core/NkMain.h"
#include "NKLogger/NkLog.h"

#include "Viewer/CameraViewerDemo.h"
#include "Multi/CameraMultiDemo.h"
#include "Format/CameraFormatDemo.h"

using namespace nkentseu;

// -----------------------------------------------------------------------------
// AppData : informations communes a toutes les demos.
// -----------------------------------------------------------------------------
NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName              = "NkCameraDemos";
    d.appVersion           = "1.0.0";
    d.enableEventLogging   = false;
    d.enableRendererDebug  = false;
    d.enableMultiWindow    = false;
    return d;
})());

// -----------------------------------------------------------------------------
// Enumeration des demos disponibles.
// -----------------------------------------------------------------------------
enum class DemoKind : uint8 {
    NK_DEMO_VIEWER = 0,
    NK_DEMO_MULTI,
    NK_DEMO_FORMAT,
};

static DemoKind ParseDemoKind(const NkVector<NkString>& args) {
    for (usize i = 1; i < args.Size(); ++i) {
        const NkString& a = args[i];
        if (a == "--demo=viewer" || a == "-dv")  return DemoKind::NK_DEMO_VIEWER;
        if (a == "--demo=multi"  || a == "-dm")  return DemoKind::NK_DEMO_MULTI;
        if (a == "--demo=format" || a == "-df")  return DemoKind::NK_DEMO_FORMAT;
    }
    return DemoKind::NK_DEMO_VIEWER;
}

// -----------------------------------------------------------------------------
// nkmain
// -----------------------------------------------------------------------------
int nkmain(const NkEntryState& state) {
    const DemoKind kind = ParseDemoKind(state.args);

    switch (kind) {
        case DemoKind::NK_DEMO_VIEWER:
            logger.Info("[NkCameraDemos] Lancement de la demo : viewer");
            return cameradem::RunCameraViewerDemo(state);
        case DemoKind::NK_DEMO_MULTI:
            logger.Info("[NkCameraDemos] Lancement de la demo : multi");
            return cameradem::RunCameraMultiDemo(state);
        case DemoKind::NK_DEMO_FORMAT:
            logger.Info("[NkCameraDemos] Lancement de la demo : format");
            return cameradem::RunCameraFormatDemo(state);
    }
    return -1;
}