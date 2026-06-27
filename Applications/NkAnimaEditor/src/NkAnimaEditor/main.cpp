// =============================================================================
// main.cpp — NkAnimaEditor : éditeur d'animation (timeline) sur NKEditorKit.
// L'app ne touche QUE l'Editor Kit + AnimBridge (pas NKRenderer directement, pour
// éviter le conflit de types NKRenderer/NKCanvas). L'anim vit dans AnimBridge.cpp.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "AnimBridge.h"
#include "Panels.h"

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NkAnimaEditor";
    d.appVersion = "0.1.0";
    return d;
})());

static void CmdUndo(void*)   { nkanima::AnimUndo(); }
static void CmdRedo(void*)   { nkanima::AnimRedo(); }
static void CmdInsert(void*) { nkanima::AnimInsertKeyAtCursor(); }
static void CmdQuit(void* u) { if (u) static_cast<NkEditorShell*>(u)->RequestClose(); }

int nkmain(const NkEntryState& state) {
    (void)state;

    auto shell = memory::NkMakeUnique<NkEditorShell>();
    NkEditorShellConfig cfg;
    cfg.title  = "NkAnimaEditor — timeline (NkAnima M1.c)";
    cfg.width  = 1280;
    cfg.height = 720;
    if (!shell || !shell->Init(cfg)) return -1;

    nkanima::AnimInit("Resources/Models/CesiumMan/CesiumMan.glb");

    static nkanima::PreviewPanel  preview;
    static nkanima::TimelinePanel timeline;
    shell->AddPanel(&preview);
    shell->AddPanel(&timeline);

    shell->RegisterCommand("Edition: Inserer cle", &CmdInsert, nullptr, "I");
    shell->RegisterCommand("Edition: Annuler",     &CmdUndo,   nullptr, "Ctrl+Z");
    shell->RegisterCommand("Edition: Refaire",     &CmdRedo,   nullptr, "Ctrl+Y");
    shell->RegisterCommand("Application: Quitter", &CmdQuit,   shell.Get(), "Ctrl+Q");

    return shell->Run();
}
