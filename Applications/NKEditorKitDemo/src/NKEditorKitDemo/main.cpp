// =============================================================================
// main.cpp — Point d'entree de la demo NKEditorKit.
// Montre le minimum cote application : creer le shell, enregistrer panneaux +
// commandes, lancer la boucle. Tout le pipeline (fenetre, docking, rendu) est
// dans NKEditorKit.
// =============================================================================
#include "NKWindow/NKWindow.h"
#include "NKWindow/NKMain.h"
#include "NKEditorKit/NkEditorKit.h"
#include "NKMemory/NkUniquePtr.h"
#include "Panels.h"

using namespace nkentseu;
using namespace nkentseu::editorkit;

NKENTSEU_DEFINE_APP_DATA(([]() {
    NkAppData d{};
    d.appName    = "NKEditorKitDemo";
    d.appVersion = "0.1.0";
    return d;
})());

// ── Commandes (callbacks C : signature NkEditorCommandFn) ────────────────────
static void CmdNewFile(void*)  {}
static void CmdOpenFile(void*) {}
static void CmdSaveFile(void*) {}
static void CmdResetLayout(void* user) {
    if (user) static_cast<NkEditorShell*>(user)->ResetLayout();
}
static void CmdSaveLayout(void* user) {
    if (user) static_cast<NkEditorShell*>(user)->SaveLayout("editor_layout.json");
}
static void CmdQuit(void* user) {
    if (user) static_cast<NkEditorShell*>(user)->RequestClose();
}

int nkmain(const NkEntryState& state) {
    (void)state;

    // Le shell possede de gros etats NKUI (gestionnaires fenetres/dock, contexte) :
    // on l'alloue sur le TAS (NKMemory) — jamais sur la pile (>1 Mo).
    auto shell = memory::NkMakeUnique<NkEditorShell>();

    NkEditorShellConfig cfg;
    cfg.title  = "NKEditorKit - Demo (coquille d'editeur dockable)";
    cfg.width  = 1280;
    cfg.height = 720;
    if (!shell || !shell->Init(cfg)) {
        return -1;
    }

    // Scene partagee + panneaux (statiques : duree de vie >= shell).
    static nkedemo::EditorScene    scene;
    static nkedemo::ExplorerPanel  explorer;
    static nkedemo::HierarchyPanel hierarchy(&scene);
    static nkedemo::ViewportPanel  viewport;
    static nkedemo::InspectorPanel inspector(&scene);
    static nkedemo::ConsolePanel   console;
    shell->AddPanel(&explorer);
    shell->AddPanel(&hierarchy);
    shell->AddPanel(&viewport);
    shell->AddPanel(&inspector);
    shell->AddPanel(&console);

    // Commandes (palette Ctrl+P).
    shell->RegisterCommand("Fichier: Nouveau",           &CmdNewFile,     nullptr,      "Ctrl+N");
    shell->RegisterCommand("Fichier: Ouvrir",            &CmdOpenFile,    nullptr,      "Ctrl+O");
    shell->RegisterCommand("Fichier: Enregistrer",       &CmdSaveFile,    nullptr,      "Ctrl+S");
    shell->RegisterCommand("Disposition: Reinitialiser", &CmdResetLayout, shell.Get());
    shell->RegisterCommand("Disposition: Sauvegarder",   &CmdSaveLayout,  shell.Get());
    shell->RegisterCommand("Application: Quitter",       &CmdQuit,        shell.Get(),  "Ctrl+Q");

    return shell->Run();
}
