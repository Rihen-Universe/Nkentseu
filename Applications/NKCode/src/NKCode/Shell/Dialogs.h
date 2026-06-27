#pragma once
// =============================================================================
// Dialogs.h — Menus applicatifs (Projet / Deploiement) + dialogues modaux.
//   - Menu « Projet »      : Nouveau projet…, Nouveau workspace…, Proprietes…
//   - Menu « Deploiement » : Empaqueter, Deployer, Creer un installateur (stubs).
//   - Overlay modal        : assistant de creation (genere les .jenga).
// Le shell appelle DrawAppMenu (dans la barre de menus) et DrawOverlay (apres
// les panneaux). ctx.appModal est leve tant qu'un dialogue est ouvert.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Project/NkCodeGen.h"
#include "NKWindow/Core/NkDialogs.h"

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    // Etat des dialogues modaux (un seul a la fois). 0 = aucun.
    struct NkCodeDialogs {
        NkCodeState*  st    = nullptr;
        NkEditorShell* shell = nullptr;   // pour appliquer l'etat d'UI du projet (ui.cfg)
        enum Mode { None = 0, NewProject, NewWorkspace, SaveAs, Properties };
        int32   mode      = None;
        bool    showStart = true;     // ecran de demarrage plein cadre (remplace l'editeur)
        char    nameBuf[256] = {};
        int32   kindIdx   = 0;
        int32   langIdx   = 0;
        int32   projDialect = 0;               // index NkDialects (NewProject)
        char    projDefines[256] = {};         // defines projet (csv)
        char    projVersion[32]  = {};         // appversion (apps)
        char    projPublisher[64]= {};         // apppublisher (apps)
        bool    justOpened = false;
        NkString status;          // message d'erreur/succes affiche dans le dialogue

        // ── Launcher : onglets (0 Recent, 1 Nouveau, 2 Charger) ──
        int32   launcherTab   = 0;
        int32   launcherFocus = 0;             // champ de saisie focus (onglet Nouveau)
        float32 newScrollY    = 0.f;           // defilement du formulaire Nouveau
        char    wsName[128]   = {};            // Nouveau : nom du workspace
        char    wsDir[512]    = {};            // Nouveau : repertoire cible
        bool    wsCfg[4]      = { true, true, false, false };  // Debug, Release, Profile, Shipping
        bool    wsPlat[8]     = {};            // plateformes/OS (index Systems())
        bool    wsArch[5]     = { true, false, false, false, false };  // x86_64, x86, arm64, arm, wasm32
        bool    wsMakeProj    = false;         // creer un projet de demarrage
        char    wsProjName[128] = {};          // projet de demarrage : nom
        int32   wsProjKind    = 0;             // genre (index NkKinds)
        int32   wsProjLang    = 0;             // langage (index NkLangs)
        bool    wsDutc        = false;         // disable unittest compilation
        bool    wsDute        = false;         // disable unittest execution
        bool    wsEnvFilled   = false;         // chemins SDK pre-remplis depuis l'env ?
        char    androidSdk[512] = {}, androidNdk[512] = {}, javaJdk[512] = {};
        char    harmonySdk[512] = {}, gdkPath[512] = {};
        char    loadDir[512]  = {};            // Charger : dossier choisi
        bool    loadScanned   = false;
        NkVector<NkString> foundPaths, foundNames;   // Charger : workspaces trouves

        // ── Gestion des toolchains (interface dediee) ──
        bool    tcOpen        = false;
        float32 tcScroll      = 0.f;
        int32   tcFocus       = -1;            // champ SDK en edition

        // ── Selecteur de dossier CUSTOM (NKGui) ──
        enum PickFor { PK_None = 0, PK_Open, PK_NewDir, PK_LoadDir, PK_Buf };
        bool    pickerOpen    = false;
        int32   pickerFor     = PK_None;
        char    pickerPath[512] = {};
        char*   pickerBuf     = nullptr;
        int32   pickerBufCap  = 0;
        float32 pickerScroll  = 0.f;           // defilement vertical
        float32 pickerScrollX = 0.f;           // defilement horizontal
        bool    pickerEditing = false;         // edition du champ chemin
        char    pickerNew[128] = {};           // nom du dossier a creer
        bool    pickerNewFocus = false;
        int32   pickerDrag    = 0;             // 0 aucun, 1 thumb V, 2 thumb H
        float32 pickerDragOff = 0.f;           // offset souris/thumb pendant le drag
        NkVector<NkString> pickerDirs;         // sous-dossiers du chemin courant
        void PickerCreateFolder() {
            if (!pickerNew[0]) return;
            if (NkDirectory::CreateRecursive(NkPath(pickerPath) / pickerNew)) { pickerNew[0] = '\0'; ScanPicker(); }
        }

        static void CopyTo(char* dst, const char* src, int32 cap) {
            int32 i = 0; if (src) for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i]; dst[i] = '\0';
        }
        void OpenPicker(int32 purpose, const char* startDir, char* buf = nullptr, int32 cap = 0) {
            pickerOpen = true; pickerFor = purpose; pickerBuf = buf; pickerBufCap = cap;
            pickerScroll = 0.f; pickerEditing = false;
            const char* start = (startDir && *startDir) ? startDir : (st ? st->root.ToString().CStr() : ".");
            CopyTo(pickerPath, start, (int32)sizeof(pickerPath));
            ScanPicker();
        }
        void ScanPicker() {
            pickerDirs.Clear();
            NkVector<NkDirectoryEntry> e = NkDirectory::GetEntries(NkPath(pickerPath), "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (usize i = 0; i < e.Size(); ++i) if (e[i].IsDirectory && e[i].Name.CStr()[0] != '.') pickerDirs.PushBack(e[i].Name);
        }
        void PickerGoto(const NkPath& p) { CopyTo(pickerPath, p.ToString().CStr(), (int32)sizeof(pickerPath)); pickerScroll = 0.f; ScanPicker(); }
        void PickerUp()    { PickerGoto(NkPath(pickerPath).GetParent()); }
        void PickerEnter(const char* sub) { PickerGoto(NkPath(pickerPath) / sub); }
        void PickerCancel(){ pickerOpen = false; pickerFor = PK_None; pickerBuf = nullptr; }
        void PickerConfirm() {
            NkString chosen(pickerPath); const int32 purpose = pickerFor; char* buf = pickerBuf; const int32 cap = pickerBufCap;
            PickerCancel();
            if (purpose == PK_Open)        DoLoad(NkPath(chosen.CStr()));
            else if (purpose == PK_NewDir) CopyTo(wsDir, chosen.CStr(), (int32)sizeof(wsDir));
            else if (purpose == PK_LoadDir){ CopyTo(loadDir, chosen.CStr(), (int32)sizeof(loadDir)); ScanLoad(); }
            else if (purpose == PK_Buf && buf) CopyTo(buf, chosen.CStr(), cap);
        }

        void Open(int32 m) { mode = m; justOpened = true; status.Clear(); if (m == NewProject || m == NewWorkspace) nameBuf[0] = '\0'; }
        void ShowStart()  { showStart = true; mode = None; }

        // Onglet Nouveau : parcourir le repertoire cible (picker NKGui).
        void BrowseNewDir()  { OpenPicker(PK_NewDir, wsDir); }
        // Onglet Charger : parcourir un dossier (picker NKGui) puis scanner.
        void BrowseLoadDir() { OpenPicker(PK_LoadDir, loadDir[0] ? loadDir : (st ? st->root.ToString().CStr() : ".")); }
        void ScanLoad() {
            loadScanned = true;
            if (loadDir[0]) NkCodeState::ScanWorkspacesIn(NkPath(loadDir), foundPaths, foundNames);
            else { foundPaths.Clear(); foundNames.Clear(); }
        }
        // Parcourir pour un chemin SDK (Nouveau) via le picker NKGui.
        void BrowseInto(char* dst, int32 cap, const char* /*title*/) { OpenPicker(PK_Buf, dst, dst, cap); }
        // Onglet Nouveau : genere le workspace (toutes proprietes) puis le charge.
        void CreateNew() {
            int32 nSys = 0; const NkCodeState::SysDef* sys = NkCodeState::Systems(&nSys);
            static const char* osNames[8]; for (int32 i = 0; i < nSys && i < 8; ++i) osNames[i] = sys[i].name;
            NkWorkspaceOpts o;
            o.name = wsName;
            for (int32 i = 0; i < 4; ++i) o.cfg[i] = wsCfg[i];
            o.os = wsPlat; o.osNames = osNames; o.nOs = nSys;
            for (int32 i = 0; i < 5; ++i) o.arch[i] = wsArch[i];
            o.startProject = (wsMakeProj && wsProjName[0]) ? wsProjName : "";   // startproject() si projet cree
            o.dutc = wsDutc; o.dute = wsDute;
            o.androidSdk = androidSdk; o.androidNdk = androidNdk; o.javaJdk = javaJdk;
            o.harmonySdk = harmonySdk; o.gdkPath = gdkPath;
            NkPath dir = wsDir[0] ? NkPath(wsDir) : st->root;
            NkString made = GenerateWorkspaceEx(dir, o);
            if (made.Empty()) { status = "Echec : nom invalide ou .jenga deja existant."; return; }
            // Projet de demarrage : genere le projet + son include() dans le workspace.
            if (wsMakeProj && wsProjName[0])
                GenerateProject(dir, NkPath(made.CStr()), wsProjName, wsProjKind, wsProjLang);
            if (st->LoadFolder(dir)) {
                if (shell) shell->LoadUiState(st->UiConfigPath().CStr());
                showStart = false;
            }
        }
        // Pre-remplit les chemins SDK depuis les variables d'environnement (une fois).
        void FillEnvOnce() {
            if (wsEnvFilled) return; wsEnvFilled = true;
            auto envc = [](const char* a, const char* b) -> const char* {
                const char* v = std::getenv(a); if ((!v || !*v) && b) v = std::getenv(b); return (v && *v) ? v : nullptr; };
            if (const char* v = envc("ANDROID_SDK_ROOT", "ANDROID_HOME"))     CopyTo(androidSdk, v, 512);
            if (const char* v = envc("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT")) CopyTo(androidNdk, v, 512);
            if (const char* v = envc("JAVA_HOME", nullptr))                   CopyTo(javaJdk, v, 512);
            if (const char* v = envc("OHOS_NDK_HOME", "OHOS_SDK"))            CopyTo(harmonySdk, v, 512);
            if (const char* v = envc("GameDK", "GameDKLatest"))               CopyTo(gdkPath, v, 512);
        }
        // Un OS (par nom) est-il coche dans le formulaire Nouveau ?
        bool OsChecked(const char* nm) const {
            int32 nSys = 0; const NkCodeState::SysDef* sys = NkCodeState::Systems(&nSys);
            for (int32 i = 0; i < nSys; ++i) if (wsPlat[i] && StrEqA(sys[i].name, nm)) return true;
            return false;
        }
        static bool StrEqA(const char* a, const char* b) {
            if (!a || !b) return false; while (*a && *b) { if (*a != *b) return false; ++a; ++b; } return *a == *b;
        }
        // Onglet Charger : charge le workspace `i` trouve dans loadDir.
        void LoadFoundAt(usize i) {
            if (!st || i >= foundPaths.Size()) return;
            if (st->LoadFolder(NkPath(loadDir))) {
                if (i < st->wsPaths.Size()) { st->wsIdx = (int32)i; st->OpenPath(NkPath(foundPaths[i].CStr())); st->RequestReload(); }
                if (shell) shell->LoadUiState(st->UiConfigPath().CStr());
                showStart = false;
            }
        }

        // Selecteur de dossier CUSTOM (NKGui) pour ouvrir un workspace.
        void OpenFolderDialog() { OpenPicker(PK_Open, st ? st->root.ToString().CStr() : "."); }
        // Ouvrir un .jenga precis reste via la boite fichier native (pas un dossier).
        void OpenWorkspaceDialog() {
            NkDialogResult res = NkDialogs::OpenFileDialog("*.jenga", "Ouvrir un workspace (.jenga)");
            if (res.confirmed && st) DoLoad(NkPath(res.path.CStr()).GetParent());
        }
        void DoLoad(const NkPath& folder) {
            if (st->LoadFolder(folder)) {
                if (shell) shell->LoadUiState(st->UiConfigPath().CStr());   // restaure l'etat d'UI du projet
                showStart = false; Close();                                // -> bascule vers l'editeur
            }
            else NkDialogs::OpenMessageBox(
                "Aucun workspace (.jenga avec 'with workspace') dans ce dossier. Chargement refuse.",
                "NKCode", 2);
        }
        void OpenSaveAs() {
            mode = SaveAs; justOpened = true; status.Clear(); nameBuf[0] = '\0';
            if (st && st->HasActive()) {   // prefill = nom du fichier actif (ou vide si sans titre)
                NkString nm = st->files[st->active].Name();
                int32 i = 0; for (; nm.CStr()[i] && i + 1 < (int32)sizeof(nameBuf); ++i) nameBuf[i] = nm.CStr()[i];
                nameBuf[i] = '\0';
            }
        }
        void Close() { mode = None; }
    };

    // ── Items injectes DANS le menu « Fichier » (via SetFileMenu) ──
    // Appele alors que BeginMenu("Fichier") est deja ouvert : on dessine les items
    // directement (creation, enregistrement, deploiement), pas un nouveau menu.
    inline void DrawFileMenu(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d) return;
        auto& ctx = ec.Ui();
        NkCodeState* s = d->st;
        const bool hasWs   = s && s->HasWorkspace();
        const bool hasFile = s && s->HasActive();

        if (MenuItem(ctx, "Ecran de demarrage...")) d->ShowStart();
        if (MenuItem(ctx, "Nouveau fichier", "Ctrl+N")) { if (s) s->NewFile(); }
        if (MenuItem(ctx, "Nouveau projet...", nullptr, hasWs)) d->Open(NkCodeDialogs::NewProject);
        if (MenuItem(ctx, "Nouveau workspace...")) d->Open(NkCodeDialogs::NewWorkspace);
        if (MenuItem(ctx, "Ouvrir un dossier...")) d->OpenFolderDialog();
        if (MenuItem(ctx, "Ouvrir un workspace (.jenga)...")) d->OpenWorkspaceDialog();

        if (MenuItem(ctx, "Enregistrer", "Ctrl+S", hasFile)) {
            if (s) { if (s->ActiveHasPath()) s->SaveActive(); else d->OpenSaveAs(); }
        }
        if (MenuItem(ctx, "Enregistrer sous...", "Ctrl+Shift+S", hasFile)) d->OpenSaveAs();
        if (MenuItem(ctx, "Enregistrer tout", nullptr, hasFile)) { if (s) s->SaveAll(); }

        if (BeginMenu(ctx, "Proprietes")) {
            MenuItem(ctx, "Proprietes du projet...", nullptr, false);     // TODO #5 (editeur de proprietes)
            MenuItem(ctx, "Proprietes du workspace...", nullptr, false);  // TODO #5
            EndMenu(ctx);
        }
        if (BeginMenu(ctx, "Deploiement")) {
            MenuItem(ctx, "Empaqueter (jenga package)", nullptr, false);        // TODO #3
            MenuItem(ctx, "Deployer (jenga deploy)", nullptr, false);           // TODO #3
            MenuItem(ctx, "Creer un installateur (.jng)", nullptr, false);      // TODO #3
            MenuItem(ctx, "Gerer les emulateurs...", nullptr, false);           // TODO #4
            EndMenu(ctx);
        }
    }

    // ── Reglage des flags d'etat CHAQUE FRAME (via SetAppMenu, appele
    // inconditionnellement dans la barre de menus, AVANT la decision de corps du
    // shell). NE PAS mettre dans DrawFileMenu : celui-ci n'est execute que quand le
    // menu Fichier est OUVERT -> le launcher n'apparaitrait qu'en ouvrant Fichier.
    inline void DrawAppFlags(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d) return;
        auto& ctx = ec.Ui();
        ctx.appModal      = (d->mode != NkCodeDialogs::None) || d->pickerOpen || d->tcOpen;
        ctx.appFullScreen = d->showStart;
    }

    // ── Champ de saisie mono-ligne minimal pour l'overlay (positionne en absolu) ──
    // Edite `buf` quand `focused` ; renvoie via les codepoints tapes + Backspace.
    inline void NkOverlayTextField(NkGuiContext& ctx, NkGuiDrawList& dl, const NkGuiFont* f,
                                   const NkRect& r, char* buf, int32 cap, bool focused) {
        const float32 asc = f ? f->Ascent() : 0.f, lh = f ? f->LineHeight() : 0.f;
        dl.AddRectFilled(r, NkColor{ 22, 27, 34, 255 }, 4.f);
        dl.AddRect(r, focused ? NkColor{ 88, 166, 255, 255 } : NkColor{ 48, 54, 61, 255 }, 1.f);
        if (focused) {
            // Backspace
            if (ctx.input.KeyPressedRepeat(NkGuiKey::Backspace)) {
                int32 n = 0; while (buf[n]) ++n;
                if (n > 0) { // retire 1 octet (ASCII suffit pour un nom sanitize)
                    buf[n - 1] = '\0';
                }
            }
            // Caracteres tapes cette frame (ASCII imprimable)
            for (int32 i = 0; i < ctx.input.charCount; ++i) {
                const uint32 cp = ctx.input.chars[i];
                if (cp >= 32 && cp < 127) {
                    int32 n = 0; while (buf[n]) ++n;
                    if (n + 1 < cap) { buf[n] = (char)cp; buf[n + 1] = '\0'; }
                }
            }
        }
        if (f) {
            // Texte clippe a la zone interne ; defilement horizontal pour garder la
            // FIN visible (caret en bout) quand le texte est plus large que le champ.
            const float32 pad = 8.f, availW = r.w - pad * 2.f;
            const float32 tw = buf[0] ? f->MeasureWidth(buf) : 0.f;
            const float32 offX = (tw > availW) ? (tw - availW) : 0.f;   // montre la fin
            const NkRect clip = { r.x + pad, r.y, r.w - pad * 2.f, r.h };
            dl.PushClipRect(clip, true);
            dl.AddText(f->Face(), f->TexId(), { r.x + pad - offX, r.y + (r.h - lh) * 0.5f + asc },
                       buf[0] ? buf : "", NkColor{ 230, 237, 243, 255 });
            if (focused) {
                const float32 caretX = r.x + pad + (tw - offX) + 1.f;
                dl.AddRectFilled({ caretX, r.y + 5.f, 1.5f, r.h - 10.f }, NkColor{ 200, 210, 220, 255 });
            }
            dl.PopClipRect();
        }
    }

    // ── Ecran de demarrage PLEIN CADRE (via SetStartScreen) ──
    // Carte centree facon « page de demarrage » : sidebar (marque + actions) a
    // gauche, liste des workspaces recents a droite. Remplace l'editeur tant
    // qu'aucun workspace n'est charge. Palette sombre + accent violet (#7c6cf0).
    inline void DrawStartScreen(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d || !d->showStart) return;
        auto& ctx = ec.Ui();
        const NkGuiFont* f = ctx.font;
        if (!f || !f->Valid()) return;
        // Pompe `jenga info` (toolchains detectees + projets) meme sur le launcher.
        if (d->st) { d->st->ScanWorkspaces(); d->st->LoadProjects(); d->st->PollProjects(); }
        auto& dl = ctx.DL();
        const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
        const float32 top = ctx.ItemHeight();
        const float32 asc = f->Ascent(), lh = f->LineHeight();
        const NkVec2 mp = ctx.input.mousePos;
        const bool   click = ctx.input.mouseClicked[0];
        const float32 S = ctx.S(1.f);
        auto hit  = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };
        auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(f->Face(), f->TexId(), { x, y + asc }, s, c); };

        // Palette — accent bleu #0F73D5 (option choisie ; secondaires orange/teal)
        const NkColor cBack   = { 11, 13, 16, 255 };
        const NkColor cCard   = { 20, 22, 26, 255 };
        const NkColor cSide   = { 26, 29, 34, 255 };
        const NkColor cBorder = { 42, 46, 53, 255 };
        const NkColor cAccent = { 15, 115, 213, 255 };   // #0F73D5
        const NkColor cAccentH= { 41, 133, 224, 255 };
        const NkColor cText   = { 236, 237, 239, 255 };
        const NkColor cSub    = { 140, 146, 154, 255 };
        const NkColor cFaint  = { 112, 118, 126, 255 };
        const NkColor cRowHov = { 30, 34, 40, 255 };
        const NkColor cSelBg  = { 22, 42, 64, 255 };

        dl.AddRectFilled({ 0.f, top, W, H - top }, cBack);

        // Carte centree
        const float32 cw = 820.f * S, chh = 540.f * S;
        const float32 cx = (W - cw) * 0.5f, cy = top + (H - top - chh) * 0.5f;
        dl.AddRectFilled({ cx, cy, cw, chh }, cCard, 18.f * S);
        dl.AddRect({ cx, cy, cw, chh }, cBorder, 1.f);

        auto btn = [&](const NkRect& r, const char* s, bool en) -> bool {
            const bool hov = en && hit(r);
            dl.AddRectFilled(r, !en ? NkColor{ 30, 32, 40, 255 } : hov ? cAccentH : cAccent, 8.f * S);
            const float32 tw = f->MeasureWidth(s);
            text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? NkColor{ 255, 255, 255, 255 } : cFaint);
            return en && hov && click;
        };
        auto sbtn = [&](const NkRect& r, const char* s) -> bool {   // bouton secondaire (bordure)
            const bool hov = hit(r);
            dl.AddRectFilled(r, hov ? NkColor{ 34, 34, 43, 255 } : cCard, 8.f * S);
            dl.AddRect(r, hov ? NkColor{ 56, 56, 70, 255 } : cBorder, 1.f);
            const float32 tw = f->MeasureWidth(s);
            text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, cText);
            return hov && click;
        };
        auto check = [&](const NkRect& bx, bool& v, const char* label) {
            const NkRect box = { bx.x, bx.y, 18.f * S, 18.f * S };
            const bool hov = hit({ bx.x, bx.y, bx.w, 18.f * S });
            dl.AddRectFilled(box, v ? cAccent : NkColor{ 31, 31, 39, 255 }, 4.f * S);
            dl.AddRect(box, v ? cAccent : (hov ? NkColor{ 80, 80, 96, 255 } : cBorder), 1.f);
            if (v) { dl.AddRectFilled({ box.x + 4.f * S, box.y + 8.f * S, 4.f * S, 4.f * S }, NkColor{ 255,255,255,255 });
                     dl.AddRectFilled({ box.x + 7.f * S, box.y + 5.f * S, 7.f * S, 4.f * S }, NkColor{ 255,255,255,255 }); }
            text(box.x + 26.f * S, bx.y + (18.f * S - lh) * 0.5f, label, cText);
            if (hov && click) v = !v;
        };

        // ── Sidebar gauche : marque + 3 onglets de navigation ──
        const float32 sw = 230.f * S;
        dl.AddRectFilled({ cx, cy, sw, chh }, cSide, 18.f * S);
        dl.AddRectFilled({ cx + sw - 18.f * S, cy, 18.f * S, chh }, cSide);
        dl.AddRectFilled({ cx + sw - 1.f, cy + 12.f, 1.f, chh - 24.f }, cBorder);
        const float32 bx = cx + 20.f * S, by = cy + 22.f * S;
        dl.AddRectFilled({ bx,            by + 7.f * S, 8.f * S, 8.f * S }, cAccent, 2.f * S);
        dl.AddRectFilled({ bx + 9.f * S,  by,           8.f * S, 8.f * S }, cAccent, 2.f * S);
        dl.AddRectFilled({ bx + 12.f * S, by + 12.f * S,8.f * S, 8.f * S }, cAccent, 2.f * S);
        text(bx + 28.f * S, by, "NKCode", cText);

        const char* tabs[] = { "Recent", "Nouveau", "Charger un workspace" };
        const char* tabsub[]= { "Vos projets recents", "Creer un workspace", "Ouvrir un dossier" };
        float32 ny = cy + 72.f * S;
        for (int32 i = 0; i < 3; ++i) {
            const NkRect r = { cx + 16.f * S, ny, sw - 32.f * S, 50.f * S };
            const bool active = (d->launcherTab == i);
            const bool hov = hit(r);
            dl.AddRectFilled(r, active ? cSelBg : (hov ? NkColor{ 34, 34, 43, 255 } : cSide), 12.f * S);
            if (active) dl.AddRect(r, cAccent, 1.f);
            dl.AddRectFilled({ r.x + 11.f * S, r.y + 10.f * S, 30.f * S, 30.f * S }, active ? cAccent : NkColor{ 124,108,240,40 }, 8.f * S);
            text(r.x + 52.f * S, r.y + 8.f * S, tabs[i], active ? NkColor{ 255,255,255,255 } : cText);
            text(r.x + 52.f * S, r.y + 8.f * S + lh, tabsub[i], cSub);
            if (hov && click) d->launcherTab = i;
            ny += 58.f * S;
        }
        text(cx + 20.f * S, cy + chh - 28.f * S, "NKCode - Nkentseu", cFaint);

        // ── Zone principale (depend de l'onglet) ──
        const float32 mx = cx + sw + 22.f * S;
        const float32 mw = cw - sw - 44.f * S;
        const NkColor rowCols[] = { {15,115,213,255}, {247,154,40,255}, {10,85,95,255}, {51,177,160,255} };

        if (d->launcherTab == 0) {
            // ===== RECENT =====
            text(mx, cy + 22.f * S, "Workspaces recents", cText);
            float32 ry = cy + 60.f * S;
            const bool canCont = d->st && d->st->HasWorkspace();
            if (canCont) {
                const NkRect r = { mx, ry, mw, 50.f * S };
                const bool hov = hit(r);
                dl.AddRectFilled(r, cSelBg, 10.f * S); dl.AddRect(r, cAccent, 1.f);
                dl.AddRectFilled({ r.x + 10.f * S, r.y + 10.f * S, 30.f * S, 30.f * S }, cAccent, 8.f * S);
                text(r.x + 52.f * S, r.y + 7.f * S, d->st->root.GetFileName().CStr(), NkColor{ 255,255,255,255 });
                text(r.x + 52.f * S, r.y + 7.f * S + lh, d->st->root.ToString().CStr(), cSub);
                const char* badge = "courant"; const float32 bw = f->MeasureWidth(badge) + 14.f * S;
                const NkRect pb = { r.x + r.w - bw - 14.f * S, r.y + (50.f * S - 18.f * S) * 0.5f, bw, 18.f * S };
                dl.AddRect(pb, cAccent, 1.f); text(pb.x + 7.f * S, pb.y + (18.f * S - lh) * 0.5f, badge, cAccent);
                if (hov && click) { if (d->shell) d->shell->LoadUiState(d->st->UiConfigPath().CStr()); d->showStart = false; return; }
                ry += 56.f * S;
            }
            // Dessine une ligne workspace (icone + nom + chemin) avec, au survol, les
            // boutons EPINGLER/DESEPINGLER et RETIRER a droite. action: 1=charger,
            // 2=(des)epingler, 3=retirer.
            auto wsRow = [&](const NkString& path, int32 colorIdx, bool pinnedRow) -> int32 {
                NkPath pp(path.CStr());
                const NkRect r = { mx, ry, mw, 50.f * S };
                const bool hov = hit(r);
                if (hov) dl.AddRectFilled(r, cRowHov, 10.f * S);
                dl.AddRectFilled({ r.x + 10.f * S, r.y + 10.f * S, 30.f * S, 30.f * S }, pinnedRow ? cAccent : rowCols[colorIdx % 4], 8.f * S);
                text(r.x + 52.f * S, r.y + 7.f * S, pp.GetFileName().CStr(), cText);
                text(r.x + 52.f * S, r.y + 7.f * S + lh, path.CStr(), NkColor{ 140,140,150,255 });
                int32 act = 0;
                const NkRect bRem = { r.x + r.w - 30.f * S, r.y + 15.f * S, 20.f * S, 20.f * S };
                const NkRect bPin = { r.x + r.w - 56.f * S, r.y + 15.f * S, 20.f * S, 20.f * S };
                if (hov || pinnedRow) {
                    const bool hPin = hit(bPin), hRem = hit(bRem);
                    dl.AddRectFilled(bPin, hPin ? NkColor{ 44,52,62,255 } : NkColor{ 0,0,0,0 }, 4.f * S);
                    // glyphe epingle (petit losange) — plein si epingle
                    dl.AddRectFilled({ bPin.x + 8.f * S, bPin.y + 4.f * S, 4.f * S, 8.f * S }, pinnedRow ? cAccent : cSub);
                    dl.AddRectFilled({ bPin.x + 5.f * S, bPin.y + 11.f * S, 10.f * S, 3.f * S }, pinnedRow ? cAccent : cSub);
                    dl.AddRectFilled(bRem, hRem ? NkColor{ 60,34,38,255 } : NkColor{ 0,0,0,0 }, 4.f * S);
                    text(bRem.x + 6.f * S, bRem.y + (20.f * S - lh) * 0.5f, "x", NkColor{ 226,114,91,255 });
                    if (click && hPin) act = 2;
                    else if (click && hRem) act = 3;
                }
                if (act == 0 && hov && click) act = 1;
                ry += 56.f * S;
                return act;
            };
            // Epingles d'abord
            for (usize i = 0; d->st && i < d->st->pinned.Size(); ++i) {
                const NkString path = d->st->pinned[i];
                const int32 a = wsRow(path, (int32)i, true);
                if (a == 1) { NkPath pp(path.CStr()); d->DoLoad(pp.GetParent()); return; }
                if (a == 2) { d->st->UnpinRecent(path); return; }
                if (a == 3) { d->st->RemoveRecent(path); return; }
                if (ry > cy + chh - 56.f * S) break;
            }
            if (d->st && d->st->recents.Empty() && d->st->pinned.Empty() && !canCont) text(mx, ry + 4.f, "(aucun workspace recent)", cFaint);
            for (usize i = 0; d->st && i < d->st->recents.Size(); ++i) {
                const NkString path = d->st->recents[i];
                NkPath pp(path.CStr());
                if (canCont && StrEq(pp.GetParent().ToString().CStr(), d->st->root.ToString().CStr())) continue;
                const int32 a = wsRow(path, (int32)i, false);
                if (a == 1) { d->DoLoad(pp.GetParent()); return; }
                if (a == 2) { d->st->PinRecent(path); return; }
                if (a == 3) { d->st->RemoveRecent(path); return; }
                if (ry > cy + chh - 56.f * S) break;
            }
        }
        else if (d->launcherTab == 1) {
            // ===== NOUVEAU : toutes les proprietes de creation (DSL Jenga) =====
            if (!d->wsDir[0] && d->st) NkCodeDialogs::CopyTo(d->wsDir, d->st->root.ToString().CStr(), (int32)sizeof(d->wsDir));
            d->FillEnvOnce();
            text(mx, cy + 22.f * S, "Nouveau workspace", cText);
            // Zone de formulaire defilante (clip + offset). Bouton Creer fixe en bas.
            const float32 footH = 50.f * S;
            const NkRect area = { mx, cy + 50.f * S, mw, chh - 50.f * S - footH - 12.f * S };
            const bool overArea = hit(area);
            if (overArea && ctx.input.wheel != 0.f) { d->newScrollY -= ctx.input.wheel * 36.f; ctx.input.wheel = 0.f; }
            dl.PushClipRect(area, true);
            float32 y = area.y - d->newScrollY;
            auto label = [&](const char* s) { text(mx, y, s, cSub); y += 22.f * S; };
            // Nom
            label("Nom");
            { const NkRect r = { mx, y, mw, 30.f * S };
              NkOverlayTextField(ctx, dl, f, r, d->wsName, (int32)sizeof(d->wsName), d->launcherFocus == 0);
              if (hit(r) && click) d->launcherFocus = 0; } y += 40.f * S;
            // Repertoire + Parcourir
            label("Repertoire");
            { const NkRect r = { mx, y, mw - 110.f * S, 30.f * S };
              NkOverlayTextField(ctx, dl, f, r, d->wsDir, (int32)sizeof(d->wsDir), d->launcherFocus == 1);
              if (hit(r) && click) d->launcherFocus = 1;
              if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "Parcourir")) { d->BrowseNewDir(); } } y += 40.f * S;
            // Configurations
            label("Configurations");
            { int32 nC = 0; const char* const* cN = NkConfigNames(&nC);
              for (int32 i = 0; i < nC; ++i) check({ mx + (i % 2) * (mw * 0.5f), y + (i / 2) * 26.f * S, mw * 0.5f, 18.f * S }, d->wsCfg[i], cN[i]);
              y += ((nC + 1) / 2) * 26.f * S + 12.f * S; }
            // Systemes cibles
            label("Systemes cibles (OS)");
            { int32 nSys = 0; const NkCodeState::SysDef* sys = NkCodeState::Systems(&nSys);
              for (int32 i = 0; i < nSys; ++i) check({ mx + (i % 2) * (mw * 0.5f), y + (i / 2) * 26.f * S, mw * 0.5f, 18.f * S }, d->wsPlat[i], sys[i].name);
              y += ((nSys + 1) / 2) * 26.f * S + 12.f * S; }
            // Architectures cibles
            label("Architectures cibles");
            { int32 nA = 0; const char* const* aN = NkArchNames(&nA);
              for (int32 i = 0; i < nA; ++i) check({ mx + (i % 2) * (mw * 0.5f), y + (i / 2) * 26.f * S, mw * 0.5f, 18.f * S }, d->wsArch[i], aN[i]);
              y += ((nA + 1) / 2) * 26.f * S + 12.f * S; }
            // Toolchains DETECTEES par Jenga (jenga info) — affichage reel.
            { char hdr[64]; std::snprintf(hdr, sizeof(hdr), "Toolchains detectees par Jenga (%d)", (int)(d->st ? d->st->toolchains.Size() : 0)); label(hdr); }
            if (d->st && d->st->toolchains.Empty()) { text(mx, y, "(detection en cours...)", cFaint); y += 24.f * S; }
            for (usize i = 0; d->st && i < d->st->toolchains.Size(); ++i) {
                const NkCodeState::ToolchainRow& t = d->st->toolchains[i];
                // surligne si la cible correspond a un OS coche
                const bool rel = d->OsChecked(t.os.CStr());
                const NkRect r = { mx, y, mw, 24.f * S };
                if (rel) { dl.AddRectFilled(r, NkColor{ 22,42,64,255 }, 4.f * S); dl.AddRect(r, cAccent, 1.f); }
                text(mx + 8.f * S, y + (24.f * S - lh) * 0.5f, t.name.CStr(), rel ? NkColor{ 255,255,255,255 } : cText);
                char meta[128]; std::snprintf(meta, sizeof(meta), "%s  %s/%s %s", t.family.CStr(), t.os.CStr(), t.arch.CStr(), t.env.CStr());
                text(mx + mw - f->MeasureWidth(meta) - 8.f * S, y + (24.f * S - lh) * 0.5f, meta, cSub);
                y += 28.f * S;
            }
            if (sbtn({ mx, y, 200.f * S, 30.f * S }, "Gerer les toolchains...")) { d->tcOpen = true; } y += 40.f * S;
            // Projet de demarrage (formulaire de projet complet)
            label("Projet de demarrage");
            check({ mx, y, mw, 18.f * S }, d->wsMakeProj, "Creer un projet de demarrage avec le workspace"); y += 26.f * S;
            if (d->wsMakeProj) {
                text(mx, y, "Nom du projet", cSub); y += 22.f * S;
                { const NkRect r = { mx, y, mw, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->wsProjName, (int32)sizeof(d->wsProjName), d->launcherFocus == 2); if (hit(r) && click) d->launcherFocus = 2; } y += 38.f * S;
                text(mx, y, "Genre", cSub); y += 22.f * S;
                { int32 nk = 0; const NkKindDef* K = NkKinds(&nk);
                  for (int32 i = 0; i < nk; ++i) {
                      const NkRect r = { mx, y, mw, 24.f * S }; const bool sel = (d->wsProjKind == i);
                      dl.AddRectFilled(r, sel ? cSelBg : (hit(r) ? cRowHov : NkColor{ 24,27,32,255 }), 4.f * S);
                      if (sel) dl.AddRect(r, cAccent, 1.f);
                      text(r.x + 10.f * S, r.y + (24.f * S - lh) * 0.5f, K[i].label, sel ? cText : cSub);
                      if (hit(r) && click) d->wsProjKind = i; y += 28.f * S;
                  } }
                text(mx, y, "Langage", cSub); y += 22.f * S;
                { int32 nl = 0; const NkLangDef* L = NkLangs(&nl); float32 lx = mx;
                  for (int32 i = 0; i < nl; ++i) { const float32 bw = f->MeasureWidth(L[i].label) + 22.f * S;
                      const NkRect r = { lx, y, bw, 26.f * S }; const bool sel = (d->wsProjLang == i);
                      dl.AddRectFilled(r, sel ? cAccent : NkColor{ 30,34,40,255 }, 6.f * S);
                      text(r.x + 11.f * S, r.y + (26.f * S - lh) * 0.5f, L[i].label, sel ? NkColor{ 255,255,255,255 } : cText);
                      if (hit(r) && click) d->wsProjLang = i; lx += bw + 8.f * S; } y += 36.f * S; }
            }
            // Tests unitaires
            label("Tests unitaires");
            check({ mx, y, mw * 0.5f, 18.f * S }, d->wsDutc, "Desactiver la compilation (dutc)");
            check({ mx, y + 24.f * S, mw, 18.f * S }, d->wsDute, "Desactiver l'execution (dute)"); y += 52.f * S;
            // SDK conditionnels
            if (d->OsChecked("Android")) {
                label("Android SDK");  { const NkRect r = { mx, y, mw - 110.f * S, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->androidSdk, 512, d->launcherFocus == 4); if (hit(r) && click) d->launcherFocus = 4; if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "...")) d->BrowseInto(d->androidSdk, 512, "Android SDK"); } y += 40.f * S;
                label("Android NDK");  { const NkRect r = { mx, y, mw - 110.f * S, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->androidNdk, 512, d->launcherFocus == 5); if (hit(r) && click) d->launcherFocus = 5; if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "...")) d->BrowseInto(d->androidNdk, 512, "Android NDK"); } y += 40.f * S;
                label("Java JDK");     { const NkRect r = { mx, y, mw - 110.f * S, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->javaJdk, 512, d->launcherFocus == 6); if (hit(r) && click) d->launcherFocus = 6; if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "...")) d->BrowseInto(d->javaJdk, 512, "Java JDK"); } y += 40.f * S;
            }
            if (d->OsChecked("HarmonyOS")) { label("HarmonyOS SDK"); { const NkRect r = { mx, y, mw - 110.f * S, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->harmonySdk, 512, d->launcherFocus == 7); if (hit(r) && click) d->launcherFocus = 7; if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "...")) d->BrowseInto(d->harmonySdk, 512, "HarmonyOS SDK"); } y += 40.f * S; }
            if (d->OsChecked("XboxSeries")) { label("Xbox GDK"); { const NkRect r = { mx, y, mw - 110.f * S, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->gdkPath, 512, d->launcherFocus == 8); if (hit(r) && click) d->launcherFocus = 8; if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "...")) d->BrowseInto(d->gdkPath, 512, "Xbox GDK"); } y += 40.f * S; }
            const float32 contentH = (y + d->newScrollY) - area.y;
            dl.PopClipRect();
            // clamp du defilement
            const float32 maxScroll = contentH - area.h > 0.f ? contentH - area.h : 0.f;
            if (d->newScrollY < 0.f) d->newScrollY = 0.f; if (d->newScrollY > maxScroll) d->newScrollY = maxScroll;
            // barre de defilement
            if (maxScroll > 0.f) {
                const float32 th = area.h * (area.h / contentH);
                const float32 tt = area.y + (area.h - th) * (d->newScrollY / maxScroll);
                dl.AddRectFilled({ area.x + area.w - 4.f * S, tt, 4.f * S, th }, NkColor{ 70, 76, 84, 255 }, 2.f * S);
            }
            // Pied fixe : status + bouton Creer
            const float32 fy = cy + chh - footH;
            if (!d->status.Empty()) text(mx, fy - 22.f * S, d->status.CStr(), NkColor{ 240,120,120,255 });
            if (btn({ mx, fy, 220.f * S, 36.f * S }, "Creer le workspace", d->wsName[0] != '\0')) { d->CreateNew(); return; }
        }
        else {
            // ===== CHARGER UN WORKSPACE =====
            text(mx, cy + 22.f * S, "Charger un workspace", cText);
            float32 y = cy + 56.f * S;
            text(mx, y, "Dossier", cSub); y += 22.f * S;
            { const NkRect box = { mx, y, mw - 110.f * S, 30.f * S };
              dl.AddRectFilled(box, NkColor{ 31,31,39,255 }, 6.f * S); dl.AddRect(box, cBorder, 1.f);
              text(box.x + 10.f * S, box.y + (30.f * S - lh) * 0.5f, d->loadDir[0] ? d->loadDir : "(non choisi)", d->loadDir[0] ? cText : cFaint);
              if (sbtn({ mx + mw - 100.f * S, y, 100.f * S, 30.f * S }, "Parcourir")) { d->BrowseLoadDir(); return; } }
            y += 48.f * S;
            text(mx, y, "Workspaces du dossier", cSub); y += 24.f * S;
            if (d->loadScanned && d->foundNames.Empty())
                text(mx, y, "Aucun workspace dans ce dossier - ouverture impossible.", NkColor{ 240,120,120,255 });
            else if (!d->loadScanned)
                text(mx, y, "Choisissez un dossier avec « Parcourir ».", cFaint);
            for (usize i = 0; i < d->foundNames.Size(); ++i) {
                const NkRect r = { mx, y, mw, 46.f * S };
                const bool hov = hit(r);
                if (hov) dl.AddRectFilled(r, cRowHov, 10.f * S);
                dl.AddRectFilled({ r.x + 10.f * S, r.y + 9.f * S, 28.f * S, 28.f * S }, rowCols[i % 4], 8.f * S);
                text(r.x + 48.f * S, r.y + 6.f * S, d->foundNames[i].CStr(), cText);
                text(r.x + 48.f * S, r.y + 6.f * S + lh, d->foundPaths[i].CStr(), NkColor{ 140,140,150,255 });
                if (hov && click) { d->LoadFoundAt(i); return; }
                y += 52.f * S;
                if (y > cy + chh - 52.f * S) break;
            }
        }
    }

    // ── Interface de gestion des TOOLCHAINS (detectees par Jenga) ──
    inline void DrawToolchains(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        auto& ctx = ec.Ui(); const NkGuiFont* f = ctx.font; if (!f || !f->Valid()) return;
        auto& dl = ctx.dlOverlay;
        const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH, S = ctx.S(1.f);
        const float32 asc = f->Ascent(), lh = f->LineHeight();
        const NkVec2 mp = ctx.input.mousePos; const bool click = ctx.input.mouseClicked[0];
        auto hit = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };
        auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(f->Face(), f->TexId(), { x, y + asc }, s, c); };
        const NkColor cCard = { 22,24,29,255 }, cBorder = { 50,55,63,255 }, cAccent = { 15,115,213,255 };
        const NkColor cText = { 236,237,239,255 }, cSub = { 150,156,164,255 };
        auto sbtn = [&](const NkRect& r, const char* s) -> bool { const bool hov = hit(r);
            dl.AddRectFilled(r, hov ? NkColor{ 40,46,54,255 } : NkColor{ 30,34,40,255 }, 6.f * S); dl.AddRect(r, cBorder, 1.f);
            const float32 tw = f->MeasureWidth(s); text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, cText); return hov && click; };

        const float32 pw = 640.f * S, ph = 520.f * S, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
        dl.AddRectFilled({ 0.f, 0.f, W, H }, NkColor{ 0,0,0,160 });
        dl.AddRectFilled({ px, py, pw, ph }, cCard, 10.f * S); dl.AddRect({ px, py, pw, ph }, cBorder, 1.5f);
        text(px + 20.f * S, py + 16.f * S, "Toolchains detectees par Jenga", cText);
        text(px + 20.f * S, py + 16.f * S + lh + 2.f, "Modifiez les chemins SDK ci-dessous (override via variables d'environnement).", cSub);

        const float32 cx = px + 20.f * S, cwid = pw - 40.f * S;
        const NkRect area = { cx, py + 64.f * S, cwid, ph - 64.f * S - 130.f * S };
        dl.AddRectFilled(area, NkColor{ 16,18,22,255 }, 6.f * S); dl.AddRect(area, cBorder, 1.f);
        if (hit(area) && ctx.input.wheel != 0.f) { d->tcScroll -= ctx.input.wheel * 34.f; ctx.input.wheel = 0.f; }
        dl.PushClipRect(area, true);
        float32 ly = area.y + 8.f * S - d->tcScroll;
        // entete colonnes
        text(area.x + 12.f * S, ly, "Nom", cSub); text(area.x + 200.f * S, ly, "Famille", cSub);
        text(area.x + 320.f * S, ly, "Cible", cSub); text(area.x + 500.f * S, ly, "Env", cSub); ly += 26.f * S;
        for (usize i = 0; d->st && i < d->st->toolchains.Size(); ++i) {
            const NkCodeState::ToolchainRow& t = d->st->toolchains[i];
            text(area.x + 12.f * S, ly, t.name.CStr(), cText);
            text(area.x + 200.f * S, ly, t.family.CStr(), cSub);
            char tgt[64]; std::snprintf(tgt, sizeof(tgt), "%s/%s", t.os.CStr(), t.arch.CStr());
            text(area.x + 320.f * S, ly, tgt, cSub);
            text(area.x + 500.f * S, ly, t.env.CStr(), cSub);
            ly += 24.f * S;
        }
        const float32 contentH = (ly + d->tcScroll) - (area.y + 8.f * S);
        dl.PopClipRect();
        const float32 maxS = contentH - area.h > 0.f ? contentH - area.h : 0.f;
        if (d->tcScroll < 0.f) d->tcScroll = 0.f; if (d->tcScroll > maxS) d->tcScroll = maxS;

        // Chemins SDK editables (Android NDK/SDK, JDK, Harmony, GDK)
        struct PF { const char* lab; char* buf; int32 id; };
        const PF pfs[] = {
            { "Android SDK", d->androidSdk, 0 }, { "Android NDK", d->androidNdk, 1 }, { "Java JDK", d->javaJdk, 2 },
            { "HarmonyOS SDK", d->harmonySdk, 3 }, { "Xbox GDK", d->gdkPath, 4 },
        };
        const float32 yy = area.y + area.h + 10.f * S, colA = cx, colB = cx + cwid * 0.5f + 8.f * S;
        for (int32 i = 0; i < 5; ++i) {
            const float32 bx = (i % 2 == 0) ? colA : colB; const float32 bw = cwid * 0.5f - 8.f * S;
            const float32 ry = yy + (i / 2) * 26.f * S;
            text(bx, ry, pfs[i].lab, cSub);
            const NkRect r = { bx + 96.f * S, ry - 4.f * S, bw - 130.f * S, 24.f * S };
            NkOverlayTextField(ctx, dl, f, r, pfs[i].buf, 512, d->tcFocus == pfs[i].id);
            if (hit(r) && click) d->tcFocus = pfs[i].id;
            if (sbtn({ bx + bw - 30.f * S, ry - 4.f * S, 30.f * S, 24.f * S }, "...")) d->BrowseInto(pfs[i].buf, 512, pfs[i].lab);
        }

        const float32 by = py + ph - 44.f * S;
        if (sbtn({ px + 20.f * S, by, 200.f * S, 32.f * S }, "Rafraichir la detection")) { if (d->st) d->st->RequestReload(); }
        if (sbtn({ px + pw - 110.f * S, by, 90.f * S, 32.f * S }, "Fermer")) { d->tcOpen = false; }
        if (ctx.input.KeyPressed(NkGuiKey::Escape)) d->tcOpen = false;
    }

    // ── Selecteur de dossier CUSTOM (NKGui) : modal centre, navigation arborescente ──
    inline void DrawFolderPicker(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        auto& ctx = ec.Ui();
        const NkGuiFont* f = ctx.font;
        if (!f || !f->Valid()) return;
        auto& dl = ctx.dlOverlay;
        const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH, S = ctx.S(1.f);
        const float32 asc = f->Ascent(), lh = f->LineHeight();
        const NkVec2 mp = ctx.input.mousePos; const bool click = ctx.input.mouseClicked[0];
        auto hit  = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };
        auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(f->Face(), f->TexId(), { x, y + asc }, s, c); };
        const NkColor cCard = { 22,24,29,255 }, cBorder = { 50,55,63,255 }, cAccent = { 15,115,213,255 };
        const NkColor cText = { 236,237,239,255 }, cSub = { 150,156,164,255 }, cRowHov = { 33,38,46,255 };
        auto sbtn = [&](const NkRect& r, const char* s) -> bool {
            const bool hov = hit(r);
            dl.AddRectFilled(r, hov ? NkColor{ 40,46,54,255 } : NkColor{ 30,34,40,255 }, 6.f * S); dl.AddRect(r, cBorder, 1.f);
            const float32 tw = f->MeasureWidth(s); text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, cText); return hov && click;
        };
        auto pbtn = [&](const NkRect& r, const char* s, bool en) -> bool {
            const bool hov = en && hit(r);
            dl.AddRectFilled(r, !en ? NkColor{ 30,34,40,255 } : hov ? NkColor{ 41,133,224,255 } : cAccent, 6.f * S);
            const float32 tw = f->MeasureWidth(s); text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? NkColor{ 255,255,255,255 } : cSub); return en && hov && click;
        };

        const float32 pw = 580.f * S, ph = 500.f * S, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
        const bool down = ctx.input.mouseDown[0];
        dl.AddRectFilled({ 0.f, 0.f, W, H }, NkColor{ 0,0,0,160 });
        dl.AddRectFilled({ px, py, pw, ph }, cCard, 10.f * S); dl.AddRect({ px, py, pw, ph }, cBorder, 1.5f);
        text(px + 20.f * S, py + 16.f * S, "Choisir un dossier", cText);

        // Champ chemin editable + Aller + Remonter
        const float32 cx = px + 20.f * S, cwid = pw - 40.f * S;
        float32 y = py + 50.f * S;
        { const NkRect r = { cx, y, cwid - 180.f * S, 30.f * S };
          NkOverlayTextField(ctx, dl, f, r, d->pickerPath, (int32)sizeof(d->pickerPath), d->pickerEditing);
          if (hit(r) && click) { d->pickerEditing = true; d->pickerNewFocus = false; }
          if (sbtn({ cx + cwid - 170.f * S, y, 80.f * S, 30.f * S }, "Aller")) { d->PickerGoto(NkPath(d->pickerPath)); d->pickerEditing = false; }
          if (sbtn({ cx + cwid - 84.f * S, y, 84.f * S, 30.f * S }, ".. Remonter")) d->PickerUp();
        }
        y += 42.f * S;

        // Liste des sous-dossiers (defilante V + H), avec barres VISIBLES
        const float32 barW = 10.f * S;
        const NkRect area = { cx, y, cwid, ph - (y - py) - 96.f * S };
        const NkRect inner = { area.x, area.y, area.w - barW, area.h - barW };   // zone hors barres
        dl.AddRectFilled(area, NkColor{ 16,18,22,255 }, 6.f * S); dl.AddRect(area, cBorder, 1.f);
        if (hit(inner) && ctx.input.wheel != 0.f) { d->pickerScroll -= ctx.input.wheel * 34.f; ctx.input.wheel = 0.f; }
        // largeur de contenu (nom le plus long)
        float32 contentW = 0.f;
        for (usize i = 0; i < d->pickerDirs.Size(); ++i) { const float32 w = f->MeasureWidth(d->pickerDirs[i].CStr()) + 40.f * S; if (w > contentW) contentW = w; }
        dl.PushClipRect(inner, true);
        float32 ly = inner.y + 6.f * S - d->pickerScroll;
        for (usize i = 0; i < d->pickerDirs.Size(); ++i) {
            const NkRect r = { inner.x + 4.f * S - d->pickerScrollX, ly, (contentW > inner.w ? contentW : inner.w) - 8.f * S, 28.f * S };
            const bool hov = NkGuiRectContains(r, mp) && hit(inner);
            if (hov) dl.AddRectFilled(r, cRowHov, 5.f * S);
            dl.AddRectFilled({ r.x + 8.f * S, r.y + 9.f * S, 16.f * S, 11.f * S }, NkColor{ 247,154,40,220 }, 2.f * S);
            dl.AddRectFilled({ r.x + 8.f * S, r.y + 7.f * S, 8.f * S, 4.f * S }, NkColor{ 247,154,40,220 }, 1.f * S);
            text(r.x + 32.f * S, r.y + (28.f * S - lh) * 0.5f, d->pickerDirs[i].CStr(), cText);
            if (hov && click) { d->PickerEnter(d->pickerDirs[i].CStr()); dl.PopClipRect(); return; }
            ly += 30.f * S;
        }
        const float32 contentH = (ly + d->pickerScroll) - (inner.y + 6.f * S);
        dl.PopClipRect();
        if (d->pickerDirs.Empty()) text(inner.x + 12.f * S, inner.y + 10.f * S, "(aucun sous-dossier)", cSub);
        // clamp
        const float32 maxS = contentH - inner.h > 0.f ? contentH - inner.h : 0.f;
        const float32 maxX = contentW - inner.w > 0.f ? contentW - inner.w : 0.f;
        // gestion du drag des thumbs
        if (!down) d->pickerDrag = 0;
        // barre V (toujours visible)
        { const NkRect track = { area.x + area.w - barW, inner.y, barW, inner.h };
          dl.AddRectFilled(track, NkColor{ 22,25,30,255 }, 3.f * S);
          const float32 th = maxS > 0.f ? inner.h * (inner.h / contentH) : inner.h;
          const float32 tt = inner.y + (maxS > 0.f ? (inner.h - th) * (d->pickerScroll / maxS) : 0.f);
          const NkRect thumb = { track.x + 2.f * S, tt, barW - 4.f * S, th };
          if (click && hit(thumb)) { d->pickerDrag = 1; d->pickerDragOff = mp.y - tt; }
          if (d->pickerDrag == 1 && maxS > 0.f) d->pickerScroll = ((mp.y - d->pickerDragOff - inner.y) / (inner.h - th)) * maxS;
          dl.AddRectFilled(thumb, d->pickerDrag == 1 ? cAccent : NkColor{ 70,76,84,255 }, 3.f * S); }
        // barre H (toujours visible)
        { const NkRect track = { inner.x, area.y + area.h - barW, inner.w, barW };
          dl.AddRectFilled(track, NkColor{ 22,25,30,255 }, 3.f * S);
          const float32 tw = maxX > 0.f ? inner.w * (inner.w / contentW) : inner.w;
          const float32 tt = inner.x + (maxX > 0.f ? (inner.w - tw) * (d->pickerScrollX / maxX) : 0.f);
          const NkRect thumb = { tt, track.y + 2.f * S, tw, barW - 4.f * S };
          if (click && hit(thumb)) { d->pickerDrag = 2; d->pickerDragOff = mp.x - tt; }
          if (d->pickerDrag == 2 && maxX > 0.f) d->pickerScrollX = ((mp.x - d->pickerDragOff - inner.x) / (inner.w - tw)) * maxX;
          dl.AddRectFilled(thumb, d->pickerDrag == 2 ? cAccent : NkColor{ 70,76,84,255 }, 3.f * S); }
        if (d->pickerScroll < 0.f) d->pickerScroll = 0.f; if (d->pickerScroll > maxS) d->pickerScroll = maxS;
        if (d->pickerScrollX < 0.f) d->pickerScrollX = 0.f; if (d->pickerScrollX > maxX) d->pickerScrollX = maxX;

        // Ligne creation de dossier : champ + bouton
        const float32 ny = area.y + area.h + 8.f * S;
        { const NkRect r = { cx, ny, cwid - 150.f * S, 30.f * S };
          NkOverlayTextField(ctx, dl, f, r, d->pickerNew, (int32)sizeof(d->pickerNew), d->pickerNewFocus);
          if (hit(r) && click) { d->pickerNewFocus = true; d->pickerEditing = false; }
          if (d->pickerNew[0] == '\0' && !d->pickerNewFocus) text(r.x + 10.f * S, r.y + (30.f * S - lh) * 0.5f, "nom du nouveau dossier", cSub);
          if (sbtn({ cx + cwid - 140.f * S, ny, 140.f * S, 30.f * S }, "+ Creer dossier")) d->PickerCreateFolder();
        }

        // Boutons bas
        const float32 by = py + ph - 44.f * S;
        if (pbtn({ px + pw - 200.f * S, by, 180.f * S, 32.f * S }, "Selectionner ce dossier", true)) { d->PickerConfirm(); return; }
        if (sbtn({ px + pw - 290.f * S, by, 80.f * S, 32.f * S }, "Annuler")) { d->PickerCancel(); return; }
        if (ctx.input.KeyPressed(NkGuiKey::Escape)) { d->PickerCancel(); return; }
    }

    // ── Overlay modal (appele apres les panneaux via SetOverlay) ──
    inline void DrawOverlay(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d) return;
        if (d->pickerOpen) { DrawFolderPicker(ec, d); return; }
        if (d->tcOpen)     { DrawToolchains(ec, d); return; }
        if (d->mode == NkCodeDialogs::None) return;
        auto& ctx = ec.Ui();
        const NkGuiFont* f = ctx.font;
        if (!f || !f->Valid()) return;
        auto& dl = ctx.dlOverlay;
        const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
        const float32 asc = f->Ascent(), lh = f->LineHeight();
        const NkVec2 mp = ctx.input.mousePos;
        const bool   click = ctx.input.mouseClicked[0];
        auto hit = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };
        auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(f->Face(), f->TexId(), { x, y + asc }, s, c); };
        auto btn = [&](const NkRect& r, const char* s, bool en) -> bool {
            const bool hov = en && hit(r);
            dl.AddRectFilled(r, !en ? NkColor{ 30, 34, 40, 255 } : hov ? NkColor{ 56, 104, 184, 255 } : NkColor{ 40, 46, 54, 255 }, 5.f);
            dl.AddRect(r, NkColor{ 60, 66, 74, 255 }, 1.f);
            const float32 tw = f->MeasureWidth(s);
            text(r.x + (r.w - tw) * 0.5f, r.y + (r.h - lh) * 0.5f, s, en ? NkColor{ 230, 237, 243, 255 } : NkColor{ 110, 118, 126, 255 });
            return en && hov && click;
        };

        const bool isProj   = (d->mode == NkCodeDialogs::NewProject);
        const bool isSaveAs = (d->mode == NkCodeDialogs::SaveAs);
        const float32 pw = 460.f, ph = isProj ? 320.f : 220.f, px = (W - pw) * 0.5f, py = (H - ph) * 0.5f;
        dl.AddRectFilled({ 0.f, 0.f, W, H }, NkColor{ 0, 0, 0, 150 });
        const NkRect panel = { px, py, pw, ph };
        // Fermeture par clic hors panneau (sauf frame d'ouverture).
        if (d->justOpened) d->justOpened = false;
        else if (click && !hit(panel)) { d->Close(); ctx.appModal = false; return; }
        dl.AddRectFilled(panel, NkColor{ 28, 33, 40, 255 }, 8.f);
        dl.AddRect(panel, NkColor{ 70, 78, 88, 255 }, 1.5f);

        const char* title = isProj ? "Nouveau projet" : isSaveAs ? "Enregistrer sous" : "Nouveau workspace";
        text(px + 18.f, py + 16.f, title, NkColor{ 230, 237, 243, 255 });

        const float32 cx = px + 20.f;
        float32 y = py + 52.f;
        text(cx, y, isSaveAs ? "Nom ou chemin du fichier" : "Nom", NkColor{ 160, 170, 180, 255 }); y += 22.f;
        NkOverlayTextField(ctx, dl, f, { cx, y, pw - 40.f, 28.f }, d->nameBuf, (int32)sizeof(d->nameBuf), true);
        y += 42.f;

        if (isProj) {
            int32 nk = 0, nl = 0; const NkKindDef* K = NkKinds(&nk); const NkLangDef* L = NkLangs(&nl);
            // Genre
            text(cx, y, "Genre", NkColor{ 160, 170, 180, 255 }); y += 22.f;
            // Liste verticale de genres (boutons radio simples)
            for (int32 i = 0; i < nk; ++i) {
                const NkRect r = { cx, y, pw - 40.f, 26.f };
                const bool sel = (d->kindIdx == i);
                dl.AddRectFilled(r, sel ? NkColor{ 38, 60, 92, 255 } : (hit(r) ? NkColor{ 33, 39, 48, 255 } : NkColor{ 24, 28, 34, 255 }), 4.f);
                if (sel) dl.AddRect(r, NkColor{ 88, 166, 255, 255 }, 1.f);
                text(r.x + 10.f, r.y + (r.h - lh) * 0.5f, K[i].label, sel ? NkColor{ 230, 237, 243, 255 } : NkColor{ 180, 188, 196, 255 });
                if (hit(r) && click) d->kindIdx = i;
                y += 30.f;
            }
            // Langage (combo horizontal de boutons)
            y += 4.f;
            text(cx, y, "Langage", NkColor{ 160, 170, 180, 255 }); y += 22.f;
            float32 lx = cx;
            for (int32 i = 0; i < nl; ++i) {
                const float32 bw = f->MeasureWidth(L[i].label) + 22.f;
                if (btn({ lx, y, bw, 26.f }, L[i].label, true)) d->langIdx = i;
                if (d->langIdx == i) dl.AddRect({ lx, y, bw, 26.f }, NkColor{ 88, 166, 255, 255 }, 1.5f);
                lx += bw + 8.f;
            }
            y += 34.f;
        }

        if (!d->status.Empty())
            text(cx, py + ph - 70.f, d->status.CStr(), NkColor{ 240, 120, 120, 255 });

        // ── Boutons Creer/Enregistrer / Annuler ──
        const float32 by = py + ph - 42.f;
        if (btn({ px + pw - 110.f, by, 96.f, 30.f }, isSaveAs ? "Enregistrer" : "Creer", d->nameBuf[0] != '\0')) {
            if (isSaveAs) {
                // Chemin absolu (contient ':' ou separateur) -> tel quel ; sinon sous la racine.
                bool abs = false; for (const char* p = d->nameBuf; *p; ++p) if (*p == ':' || *p == '/' || *p == '\\') { abs = true; break; }
                NkPath dest = abs ? NkPath(d->nameBuf) : (d->st->root / d->nameBuf);
                if (d->st->SaveActiveAs(dest)) { d->Close(); ctx.appModal = false; return; }
                d->status = "Echec : impossible d'ecrire le fichier.";
            } else {
                NkString made;
                if (isProj) made = GenerateProject(d->st->root, NkPath(d->st->wsPaths[d->st->wsIdx].CStr()),
                                                   d->nameBuf, d->kindIdx, d->langIdx);
                else        made = GenerateWorkspace(d->st->root, d->nameBuf);
                if (made.Empty()) {
                    d->status = isProj ? "Echec : nom invalide ou dossier deja existant."
                                       : "Echec : nom invalide ou .jenga deja existant.";
                } else {
                    d->st->RequestReload();
                    d->st->mWsScanned = false; d->st->ScanWorkspaces();
                    d->st->OpenPath(NkPath(made.CStr()));   // ouvre le .jenga genere
                    if (!isProj) {                          // workspace cree -> on entre dans l'editeur
                        d->st->AddRecent(d->st->wsPaths.Empty() ? made : d->st->wsPaths[d->st->wsIdx]);
                        d->showStart = false;
                    }
                    d->Close(); ctx.appModal = false; return;
                }
            }
        }
        if (btn({ px + pw - 218.f, by, 96.f, 30.f }, "Annuler", true)) { d->Close(); ctx.appModal = false; return; }

        // Echap = annuler, Entree = creer (si nom valide)
        if (ctx.input.KeyPressed(NkGuiKey::Escape)) { d->Close(); ctx.appModal = false; return; }

        ctx.appModal = true;   // maintien
    }

} // namespace nkcode
} // namespace nkentseu
