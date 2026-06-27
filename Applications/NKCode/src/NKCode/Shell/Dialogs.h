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
        char    wsStart[128]  = {};            // projet de demarrage (optionnel)
        char    wsTool[64]    = {};            // toolchain par defaut (optionnel)
        bool    wsDutc        = false;         // disable unittest compilation
        bool    wsDute        = false;         // disable unittest execution
        char    androidSdk[512] = {}, androidNdk[512] = {}, javaJdk[512] = {};
        char    harmonySdk[512] = {}, gdkPath[512] = {};
        char    loadDir[512]  = {};            // Charger : dossier choisi
        bool    loadScanned   = false;
        NkVector<NkString> foundPaths, foundNames;   // Charger : workspaces trouves

        static void CopyTo(char* dst, const char* src, int32 cap) {
            int32 i = 0; if (src) for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i]; dst[i] = '\0';
        }

        void Open(int32 m) { mode = m; justOpened = true; status.Clear(); if (m == NewProject || m == NewWorkspace) nameBuf[0] = '\0'; }
        void ShowStart()  { showStart = true; mode = None; }

        // Onglet Nouveau : parcourir le repertoire cible.
        void BrowseNewDir() {
            NkDialogResult r = NkDialogs::OpenFolderDialog("Repertoire du nouveau workspace");
            if (r.confirmed) CopyTo(wsDir, r.path.CStr(), (int32)sizeof(wsDir));
        }
        // Onglet Charger : parcourir un dossier puis scanner ses workspaces.
        void BrowseLoadDir() {
            NkDialogResult r = NkDialogs::OpenFolderDialog("Dossier contenant le(s) workspace(s)");
            if (r.confirmed) { CopyTo(loadDir, r.path.CStr(), (int32)sizeof(loadDir)); ScanLoad(); }
        }
        void ScanLoad() {
            loadScanned = true;
            if (loadDir[0]) NkCodeState::ScanWorkspacesIn(NkPath(loadDir), foundPaths, foundNames);
            else { foundPaths.Clear(); foundNames.Clear(); }
        }
        // Parcourir pour un chemin SDK (Nouveau).
        void BrowseInto(char* dst, int32 cap, const char* title) {
            NkDialogResult r = NkDialogs::OpenFolderDialog(title);
            if (r.confirmed) CopyTo(dst, r.path.CStr(), cap);
        }
        // Onglet Nouveau : genere le workspace (toutes proprietes) puis le charge.
        void CreateNew() {
            int32 nSys = 0; const NkCodeState::SysDef* sys = NkCodeState::Systems(&nSys);
            static const char* osNames[8]; for (int32 i = 0; i < nSys && i < 8; ++i) osNames[i] = sys[i].name;
            NkWorkspaceOpts o;
            o.name = wsName;
            for (int32 i = 0; i < 4; ++i) o.cfg[i] = wsCfg[i];
            o.os = wsPlat; o.osNames = osNames; o.nOs = nSys;
            for (int32 i = 0; i < 5; ++i) o.arch[i] = wsArch[i];
            o.startProject = wsStart; o.toolchain = wsTool;
            o.dutc = wsDutc; o.dute = wsDute;
            o.androidSdk = androidSdk; o.androidNdk = androidNdk; o.javaJdk = javaJdk;
            o.harmonySdk = harmonySdk; o.gdkPath = gdkPath;
            NkPath dir = wsDir[0] ? NkPath(wsDir) : st->root;
            NkString made = GenerateWorkspaceEx(dir, o);
            if (made.Empty()) { status = "Echec : nom invalide ou .jenga deja existant."; return; }
            if (st->LoadFolder(dir)) {
                if (shell) shell->LoadUiState(st->UiConfigPath().CStr());
                showStart = false;
            }
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

        // Ouvre un dossier/workspace via les dialogues natifs. Refuse si pas de workspace.
        void OpenFolderDialog() {
            NkDialogResult res = NkDialogs::OpenFolderDialog("Ouvrir un dossier (workspace)");
            if (res.confirmed && st) DoLoad(NkPath(res.path.CStr()));
        }
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
        ctx.appModal      = (d->mode != NkCodeDialogs::None);
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
            dl.AddText(f->Face(), f->TexId(), { r.x + 8.f, r.y + (r.h - lh) * 0.5f + asc },
                       buf[0] ? buf : "", NkColor{ 230, 237, 243, 255 });
            if (focused) {
                const float32 cw = buf[0] ? f->MeasureWidth(buf) : 0.f;
                dl.AddRectFilled({ r.x + 8.f + cw + 1.f, r.y + 5.f, 1.5f, r.h - 10.f }, NkColor{ 200, 210, 220, 255 });
            }
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
            if (d->st && d->st->recents.Empty() && !canCont) text(mx, ry + 4.f, "(aucun workspace recent)", cFaint);
            for (usize i = 0; d->st && i < d->st->recents.Size(); ++i) {
                NkPath pp(d->st->recents[i].CStr());
                if (canCont && StrEq(pp.GetParent().ToString().CStr(), d->st->root.ToString().CStr())) continue;
                const NkRect r = { mx, ry, mw, 50.f * S };
                const bool hov = hit(r);
                if (hov) dl.AddRectFilled(r, cRowHov, 10.f * S);
                dl.AddRectFilled({ r.x + 10.f * S, r.y + 10.f * S, 30.f * S, 30.f * S }, rowCols[i % 4], 8.f * S);
                text(r.x + 52.f * S, r.y + 7.f * S, pp.GetFileName().CStr(), cText);
                text(r.x + 52.f * S, r.y + 7.f * S + lh, d->st->recents[i].CStr(), NkColor{ 140,140,150,255 });
                if (hov && click) { d->DoLoad(pp.GetParent()); return; }
                ry += 56.f * S;
                if (ry > cy + chh - 56.f * S) break;
            }
        }
        else if (d->launcherTab == 1) {
            // ===== NOUVEAU : toutes les proprietes de creation (DSL Jenga) =====
            if (!d->wsDir[0] && d->st) NkCodeDialogs::CopyTo(d->wsDir, d->st->root.ToString().CStr(), (int32)sizeof(d->wsDir));
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
            // Projet de demarrage / Toolchain
            label("Projet de demarrage (optionnel)");
            { const NkRect r = { mx, y, mw, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->wsStart, (int32)sizeof(d->wsStart), d->launcherFocus == 2); if (hit(r) && click) d->launcherFocus = 2; } y += 40.f * S;
            label("Toolchain par defaut (optionnel)");
            { const NkRect r = { mx, y, mw, 30.f * S }; NkOverlayTextField(ctx, dl, f, r, d->wsTool, (int32)sizeof(d->wsTool), d->launcherFocus == 3); if (hit(r) && click) d->launcherFocus = 3; } y += 40.f * S;
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

    // ── Overlay modal (appele apres les panneaux via SetOverlay) ──
    inline void DrawOverlay(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d || d->mode == NkCodeDialogs::None) return;
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
