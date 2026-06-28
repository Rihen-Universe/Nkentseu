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
#include "NKCode/Shell/ScaffoldPanels.h"
#include "NKCode/Shell/NkHome.h"
#include "NKCode/Project/NkLogSink.h"
#include "NKImage/NKImage.h"

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
// Ecran d'accueil (Home) — nouvelle UI propre (design Banani).
static nkcode::NkHomeState g_home;
static void StartScreenThunk(NkEditorFrameContext& ec, void* u) {
    nkcode::DrawHome(ec, static_cast<nkcode::NkHomeState*>(u));
}
// Pose appFullScreen/appModal CHAQUE FRAME (barre de menus, inconditionnel).
static void AppFlagsThunk(NkEditorFrameContext& ec, void* u) {
    nkcode::DrawAppFlags(ec, static_cast<nkcode::NkCodeDialogs*>(u));
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
    cfg.width  = 1440;     // grande fenetre centree, REDIMENSIONNABLE (pas maximisee de force)
    cfg.height = 900;
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

    // Maquettes des interfaces (interface.md) : structure visuelle d'abord, rendu
    // fonctionnel ensuite (roadmap #2-#20). Fermees par defaut -> menu Affichage.
    using nkcode::ScaffoldPanel;
    namespace sc = nkcode::scaffold;
    static ScaffoldPanel pSearch ("Recherche",   NkEditorDockSide::NK_LEFT,   "Maquette - roadmap #7", sc::kSearch,     1);
    static ScaffoldPanel pProblem("Problemes",    NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #8", sc::kProblems,   1);
    static ScaffoldPanel pGit    ("Controle de version", NkEditorDockSide::NK_LEFT, "Maquette - roadmap #9", sc::kGit, 3);
    static ScaffoldPanel pDebug  ("Debogueur",    NkEditorDockSide::NK_LEFT,   "Maquette - roadmap #10", sc::kDebug,     2);
    static ScaffoldPanel pBuild  ("Build & Taches", NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #14", sc::kBuild,   1);
    static ScaffoldPanel pProf   ("Profiler",     NkEditorDockSide::NK_BOTTOM, "Maquette - roadmap #19", sc::kProfiler,  1);
    static ScaffoldPanel pAi     ("Assistant IA", NkEditorDockSide::NK_RIGHT,  "Maquette - roadmap #16", sc::kAi,        1);
    static ScaffoldPanel pEngine ("Moteur",       NkEditorDockSide::NK_RIGHT,  "Maquette - roadmap #17", sc::kEngine,    1);
    static ScaffoldPanel pExt    ("Extensions",   NkEditorDockSide::NK_LEFT,   "Maquette - roadmap #12", sc::kExtensions,1);
    shell->AddPanel(&pSearch);  shell->AddPanel(&pProblem); shell->AddPanel(&pGit);
    shell->AddPanel(&pDebug);   shell->AddPanel(&pBuild);   shell->AddPanel(&pProf);
    shell->AddPanel(&pAi);      shell->AddPanel(&pEngine);  shell->AddPanel(&pExt);

    shell->SetToolbar(&ToolbarThunk, &g_state);   // barre d'outils Visual Studio
    g_dialogs.st    = &g_state;
    g_dialogs.shell = shell.Get();
    g_state.LoadRecents();                           // workspaces recents (ecran de demarrage)
    shell->SetAppMenu(&AppFlagsThunk, &g_dialogs);   // pose appFullScreen/appModal chaque frame
    shell->SetFileMenu(&FileMenuThunk, &g_dialogs);  // items du menu Fichier (Nouveau/Enregistrer/Deploiement)
    shell->SetOverlay(&OverlayThunk, &g_dialogs);    // dialogues modaux (creation/enregistrement)

    // ── Ecran d'accueil (Home) : nouvelle UI + logos/icones rasterises en texture ──
    g_home.st = &g_state; g_home.dlg = &g_dialogs;
    {
        // Charge une texture NETTE : PNG en priorite (repli SVG), puis REDIMENSIONNE
        // a ~ la taille d'affichage (tw x th) au filtre bilineaire. Sans mipmaps, une
        // texture bien plus grande que l'affichage est sous-echantillonnee (flou) ;
        // on l'amene donc proche de sa taille a l'ecran. `base` = nom sans extension.
        auto upload = [&](NkImage& img, int32 tw, int32 th, int32* outW, int32* outH) -> uint32 {
            NkImage* use = &img; NkImage* resized = nullptr;
            if (img.Width() > tw || img.Height() > th) { resized = img.Resize(tw, th); if (resized && resized->IsValid()) use = resized; }
            uint32 id = shell->UploadRGBA(use->Pixels(), use->Width(), use->Height());
            if (outW) *outW = use->Width(); if (outH) *outH = use->Height();
            if (resized) resized->Free();
            return id;
        };
        auto loadTex = [&](const char* base, int32 tw, int32 th, int32* outW = nullptr, int32* outH = nullptr) -> uint32 {
            const char* dirs[] = { "Applications/NKCode/data/textures/", "data/textures/", "NKCode/data/textures/", "" };
            char path[512];
            for (const char* const* d = dirs; ; ++d) {
                std::snprintf(path, sizeof(path), "%s%s.png", *d, base);
                { std::FILE* fp = std::fopen(path, "rb");
                  if (fp) { std::fclose(fp); NkImage img;
                      if (img.LoadFromFile(path) && img.IsValid()) return upload(img, tw, th, outW, outH); } }
                std::snprintf(path, sizeof(path), "%s%s.svg", *d, base);
                { std::FILE* fp = std::fopen(path, "rb");
                  if (fp) { std::fclose(fp);
                      NkImage* im = NkSVGCodec::DecodeFromFile(path, tw * 2, th * 2);   // rasterise large puis reduit = net
                      if (im && im->IsValid()) { uint32 id = upload(*im, tw, th, outW, outH); im->Free(); return id; }
                      if (im) im->Free(); } }
                if (!**d) break;
            }
            return 0;
        };
        // Logos : icone titre ~ petite (48), wordmark sidebar ~320x80 (aspect 4:1)
        g_home.logoIcon = loadTex("logo/icon_blanc_fond_transparent", 48, 48);
        g_home.logoWord = loadTex("logo/logo_complet_blanc_fond_transparent", 320, 80, &g_home.wordW, &g_home.wordH);
        shell->SetTitleLogo(g_home.logoIcon);
        // Icones : redimensionnees a ~64px (proche de l'affichage 17-38px) -> NET
        nkcode::NkIcons& ic = g_home.icons;
        ic.accueil       = loadTex("icon/Accueil", 64, 64);
        ic.ouvrir        = loadTex("icon/Ouvrir", 64, 64);
        ic.ouvrirDossier = loadTex("icon/OuvrirUnDossier", 64, 64);
        ic.nouveau       = loadTex("icon/Nouveau", 64, 64);
        ic.cloner        = loadTex("icon/Cloner", 64, 64);
        ic.toolchains    = loadTex("icon/Toolchains", 64, 64);
        ic.platforms     = loadTex("icon/Platforms", 64, 64);
        ic.gear          = loadTex("icon/Gear", 64, 64);
        ic.exemple       = loadTex("icon/Exemple", 64, 64);
        ic.star          = loadTex("icon/Star", 64, 64);
        ic.search        = loadTex("icon/Search", 48, 48);
        ic.workspace     = loadTex("icon/workspace", 64, 64);
    }
    shell->SetStartScreen(&StartScreenThunk, &g_home);  // ecran de demarrage plein cadre (Home)
    // Fenetre large/centree mais NON maximisee -> redimensionnable par les bords des
    // le lancement (une fenetre maximisee n'est pas redimensionnable). L'utilisateur
    // peut maximiser via le bouton de la barre de titre ; l'etat projet le surchargera.
    // g_dialogs.showStart est vrai par defaut -> l'ecran de demarrage s'affiche au lancement.

    shell->RegisterCommand("Projet: Construire (jenga build)", &CmdBuild, nullptr,      "Ctrl+B");
    shell->RegisterCommand("Projet: Demarrer (jenga run)",     &CmdRun,   nullptr,      "Ctrl+R");
    shell->RegisterCommand("Fichier: Enregistrer",             &CmdSave,  &g_state,     "Ctrl+S");
    shell->RegisterCommand("Edition: Formater le document",    &CmdFormat, nullptr,     "Ctrl+L");
    shell->RegisterCommand("Disposition: Reinitialiser",       &CmdResetLayout, shell.Get());
    shell->RegisterCommand("Application: Quitter",             &CmdQuit,  shell.Get(),  "Ctrl+Q");

    const int rc = shell->Run();
    // Sauvegarde l'etat d'interface du projet courant (maximise + panneaux ouverts).
    if (!g_dialogs.showStart && g_state.HasWorkspace())
        shell->SaveUiState(g_state.UiConfigPath().CStr());
    return rc;
}
