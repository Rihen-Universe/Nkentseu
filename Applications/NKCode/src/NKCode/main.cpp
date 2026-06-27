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
#include "NKCode/Shell/Panels.h"
#include "NKCode/Shell/Toolbar.h"
#include "NKCode/Shell/Dialogs.h"
#include "NKCode/Project/NkLogSink.h"

#include <cstdio>
#include <cstddef>

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
static void CmdBuild(void*)        { g_state.DoBuildAction("build"); }
static void CmdRun(void*)          { g_state.DoRun(); }
static void CmdSave(void* user)    { if (user) static_cast<nkcode::NkCodeState*>(user)->SaveActive(); }
static void CmdFormat(void*)       {            // Formater le document actif (C/C++)
    if (g_state.active >= 0 && g_state.active < static_cast<int32>(g_state.files.Size()))
        g_state.files[g_state.active].doc.FormatCpp();
}
static void CmdQuit(void* user)    { if (user) static_cast<NkEditorShell*>(user)->RequestClose(); }
static void CmdResetLayout(void* u){ if (u) static_cast<NkEditorShell*>(u)->ResetLayout(); }

// Barre d'outils Visual Studio (config/plateforme + Demarrer) -> delegue a NKCode.
static void ToolbarThunk(NkEditorFrameContext& ec, void* u) {
    nkcode::DrawCodeToolbar(ec, static_cast<nkcode::NkCodeState*>(u));
}

// Dialogues modaux (creation/enregistrement) + items du menu Fichier.
static nkcode::NkCodeDialogs g_dialogs;
static void FileMenuThunk(NkEditorFrameContext& ec, void* u) {
    nkcode::DrawFileMenu(ec, static_cast<nkcode::NkCodeDialogs*>(u));
}
static void OverlayThunk(NkEditorFrameContext& ec, void* u) {
    nkcode::DrawOverlay(ec, static_cast<nkcode::NkCodeDialogs*>(u));
}

int nkmain(const NkEntryState& state) {
    (void)state;

    nkcode::InstallLogSink();   // capture les logs NKLogger -> panneau OUTPUT

    // Polices de REPLI externes depuis le dossier data/fonts de NKCode (charge au
    // runtime, pas embarque) : tout glyphe absent d'Inter/DejaVu y est cherche.
    // Roles : broad (large couverture), cjk (ideogrammes, opt-in), emoji.
    {
        auto fileOk = [](const char* p) { std::FILE* f = std::fopen(p, "rb"); if (f) { std::fclose(f); return true; } return false; };
        static const char* dirs[] = { "Applications/NKCode/data/fonts/", "data/fonts/", "NKCode/data/fonts/", "" };
        auto find = [&](const char* const* names, char* out, std::size_t cap) {
            out[0] = '\0';
            for (const char* const* np = names; *np; ++np)
                for (const char* const* dp = dirs; ; ++dp) { std::snprintf(out, cap, "%s%s", *dp, *np); if (fileOk(out)) return; if (!**dp) break; }
            out[0] = '\0';
        };
        static char broad[600], cjk[600], emoji[600];
        const char* broadN[] = { "NotoSans-Regular.ttf", nullptr };
        const char* cjkN[]   = { "NotoSansSC-Regular.ttf", "NotoSansSC.ttf", "NotoSansCJKsc-Regular.otf", nullptr };
        const char* emojiN[] = { "NotoEmoji-Regular.ttf", nullptr };
        find(broadN, broad, sizeof(broad)); find(cjkN, cjk, sizeof(cjk)); find(emojiN, emoji, sizeof(emoji));
        nkgui::NkSetFallbackFontPaths(broad[0] ? broad : nullptr, cjk[0] ? cjk : nullptr, emoji[0] ? emoji : nullptr);
    }

    auto shell = memory::NkMakeUnique<NkEditorShell>();
    NkEditorShellConfig cfg;
    cfg.title  = "NKCode - IDE (Jenga)";
    cfg.width  = 1280;
    cfg.height = 720;
    if (!shell || !shell->Init(cfg)) return -1;

    // Ouvre un fichier au demarrage (demo) : le README de NKCode.
    g_state.OpenPath(g_state.root / "README.md");

    static nkcode::ExplorerPanel explorer(&g_state);
    static nkcode::EditorPanel   editor(&g_state, shell.Get());
    static nkcode::OutputPanel   output(&g_state);
    static nkcode::TerminalPanel terminal;
    shell->AddPanel(&explorer);
    shell->AddPanel(&editor);
    shell->AddPanel(&output);
    shell->AddPanel(&terminal);

    shell->SetToolbar(&ToolbarThunk, &g_state);   // barre d'outils Visual Studio
    g_dialogs.st = &g_state;
    shell->SetFileMenu(&FileMenuThunk, &g_dialogs);  // items du menu Fichier (Nouveau/Enregistrer/Deploiement)
    shell->SetOverlay(&OverlayThunk, &g_dialogs);    // dialogues modaux (creation/enregistrement)
    g_dialogs.Open(nkcode::NkCodeDialogs::Welcome);  // ecran de demarrage facon VS Community

    shell->RegisterCommand("Projet: Construire (jenga build)", &CmdBuild, nullptr,      "Ctrl+B");
    shell->RegisterCommand("Projet: Demarrer (jenga run)",     &CmdRun,   nullptr,      "Ctrl+R");
    shell->RegisterCommand("Fichier: Enregistrer",             &CmdSave,  &g_state,     "Ctrl+S");
    shell->RegisterCommand("Edition: Formater le document",    &CmdFormat, nullptr,     "Ctrl+L");
    shell->RegisterCommand("Disposition: Reinitialiser",       &CmdResetLayout, shell.Get());
    shell->RegisterCommand("Application: Quitter",             &CmdQuit,  shell.Get(),  "Ctrl+Q");

    return shell->Run();
}
