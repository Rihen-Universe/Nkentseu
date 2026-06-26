// =============================================================================
// main.cpp — Point d'entree de NKCode (IDE type VSCode, base sur Jenga).
// Coquille = NKEditorKit (sur NKGui). Panneaux : Explorateur (fichiers reels),
// Editeur (onglets + saisie), Sortie (jenga build). Commandes : Construire /
// Executer (jenga) + Enregistrer. Le visuel/Blueprint/UIBuilder viendront ensuite.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "NKCode/Panels.h"

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NKCode";
    d.appVersion = "0.1.0";
    return d;
})());

// Etat global de l'IDE (duree de vie = appli).
static nkcode::NkCodeState g_state;

// ── Commandes (NkEditorCommandFn = void(*)(void*)) ──
static void CmdBuild(void*)        { g_state.RunJenga("build --target NKCode --config Debug"); }
static void CmdRun(void*)          { g_state.RunJenga("--version"); }
static void CmdSave(void* user)    { if (user) static_cast<nkcode::NkCodeState*>(user)->SaveActive(); }
static void CmdQuit(void* user)    { if (user) static_cast<NkEditorShell*>(user)->RequestClose(); }
static void CmdResetLayout(void* u){ if (u) static_cast<NkEditorShell*>(u)->ResetLayout(); }

int nkmain(const NkEntryState& state) {
    (void)state;

    auto shell = memory::NkMakeUnique<NkEditorShell>();
    NkEditorShellConfig cfg;
    cfg.title  = "NKCode - IDE (Jenga)";
    cfg.width  = 1280;
    cfg.height = 720;
    if (!shell || !shell->Init(cfg)) return -1;

    // Ouvre un fichier au demarrage (demo) : le README de NKCode.
    g_state.OpenPath(g_state.root / "README.md");

    static nkcode::ExplorerPanel explorer(&g_state);
    static nkcode::EditorPanel   editor(&g_state);
    static nkcode::OutputPanel   output(&g_state);
    shell->AddPanel(&explorer);
    shell->AddPanel(&editor);
    shell->AddPanel(&output);

    shell->RegisterCommand("Projet: Construire (jenga build)", &CmdBuild, nullptr,      "Ctrl+B");
    shell->RegisterCommand("Projet: Version de Jenga",         &CmdRun,   nullptr);
    shell->RegisterCommand("Fichier: Enregistrer",             &CmdSave,  &g_state,     "Ctrl+S");
    shell->RegisterCommand("Disposition: Reinitialiser",       &CmdResetLayout, shell.Get());
    shell->RegisterCommand("Application: Quitter",             &CmdQuit,  shell.Get(),  "Ctrl+Q");

    return shell->Run();
}
