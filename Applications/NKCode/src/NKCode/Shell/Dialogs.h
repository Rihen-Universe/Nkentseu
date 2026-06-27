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
        NkCodeState* st = nullptr;
        enum Mode { None = 0, NewProject, NewWorkspace, SaveAs, Properties };
        int32   mode      = None;
        bool    showStart = true;     // ecran de demarrage plein cadre (remplace l'editeur)
        char    nameBuf[256] = {};
        int32   kindIdx   = 0;
        int32   langIdx   = 0;
        bool    justOpened = false;
        NkString status;          // message d'erreur/succes affiche dans le dialogue

        void Open(int32 m) { mode = m; justOpened = true; status.Clear(); if (m == NewProject || m == NewWorkspace) nameBuf[0] = '\0'; }
        void ShowStart()  { showStart = true; mode = None; }

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
            if (st->LoadFolder(folder)) { showStart = false; Close(); }   // -> bascule vers l'editeur
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
        // Leve les flags AVANT le masquage d'input du corps (la barre de menus est
        // dessinee avant cette decision dans la boucle du shell).
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
    // Remplit le corps (sous la barre de titre) ; remplace l'editeur tant qu'aucun
    // workspace n'est charge. Gauche = actions, droite = liste des recents.
    inline void DrawStartScreen(NkEditorFrameContext& ec, NkCodeDialogs* d) {
        if (!d || !d->showStart) return;
        auto& ctx = ec.Ui();
        const NkGuiFont* f = ctx.font;
        if (!f || !f->Valid()) return;
        auto& dl = ctx.DL();                    // corps : draw-list principale
        const float32 W = (float32)ctx.viewW, H = (float32)ctx.viewH;
        const float32 top = ctx.ItemHeight();   // sous la barre de titre
        const float32 asc = f->Ascent(), lh = f->LineHeight();
        const NkVec2 mp = ctx.input.mousePos;
        const bool   click = ctx.input.mouseClicked[0];
        auto hit  = [&](const NkRect& r) { return NkGuiRectContains(r, mp); };
        auto text = [&](float32 x, float32 y, const char* s, const NkColor& c) { dl.AddText(f->Face(), f->TexId(), { x, y + asc }, s, c); };

        dl.AddRectFilled({ 0.f, top, W, H - top }, NkColor{ 18, 21, 26, 255 });
        // En-tete
        text(48.f, top + 40.f, "NKCode", NkColor{ 230, 237, 243, 255 });
        text(48.f, top + 40.f + lh + 6.f, "Demarrer - ouvrir ou creer un workspace", NkColor{ 140, 150, 160, 255 });

        // ── Colonne gauche : actions ──
        struct WItem { const char* t; const char* sub; int32 act; };
        const WItem items[] = {
            { "Ouvrir un dossier...",            "Charge un dossier et detecte ses workspaces",  1 },
            { "Ouvrir un workspace (.jenga)...", "Selectionne directement un fichier workspace", 2 },
            { "Nouveau workspace...",            "Cree un workspace vierge (pour de futurs projets)", 3 },
        };
        const float32 colW = (W - 96.f) * 0.5f;
        float32 wy = top + 120.f;
        for (int32 i = 0; i < 3; ++i) {
            const NkRect r = { 48.f, wy, colW, 58.f };
            const bool hov = hit(r);
            dl.AddRectFilled(r, hov ? NkColor{ 38, 60, 92, 255 } : NkColor{ 30, 35, 42, 255 }, 6.f);
            dl.AddRect(r, hov ? NkColor{ 88, 166, 255, 255 } : NkColor{ 50, 57, 65, 255 }, 1.f);
            text(r.x + 16.f, r.y + 10.f, items[i].t, NkColor{ 230, 237, 243, 255 });
            text(r.x + 16.f, r.y + 10.f + lh + 2.f, items[i].sub, NkColor{ 150, 160, 170, 255 });
            if (hov && click) {
                if (items[i].act == 1) d->OpenFolderDialog();
                else if (items[i].act == 2) d->OpenWorkspaceDialog();
                else d->Open(NkCodeDialogs::NewWorkspace);
                return;
            }
            wy += 68.f;
        }
        // Continuer avec le dossier courant (s'il a deja un workspace)
        const bool canCont = d->st && d->st->HasWorkspace();
        {
            const NkRect r = { 48.f, wy + 6.f, colW, 40.f };
            const bool hov = canCont && hit(r);
            dl.AddRectFilled(r, !canCont ? NkColor{ 26, 30, 36, 255 } : hov ? NkColor{ 56, 104, 184, 255 } : NkColor{ 35, 41, 49, 255 }, 6.f);
            text(r.x + 16.f, r.y + (40.f - lh) * 0.5f, "Continuer avec le dossier courant", canCont ? NkColor{ 230, 237, 243, 255 } : NkColor{ 100, 108, 116, 255 });
            if (hov && click) { d->showStart = false; return; }
        }

        // ── Colonne droite : recents ──
        const float32 rx = 48.f + colW + 24.f;
        text(rx, top + 120.f - lh - 6.f, "Recents", NkColor{ 140, 150, 160, 255 });
        float32 ry = top + 120.f;
        if (d->st && d->st->recents.Empty())
            text(rx, ry + 4.f, "(aucun workspace recent)", NkColor{ 100, 108, 116, 255 });
        for (usize i = 0; d->st && i < d->st->recents.Size(); ++i) {
            const NkRect r = { rx, ry, colW, 44.f };
            const bool hov = hit(r);
            dl.AddRectFilled(r, hov ? NkColor{ 33, 39, 48, 255 } : NkColor{ 24, 28, 34, 255 }, 5.f);
            const NkString& path = d->st->recents[i];
            NkPath pp(path.CStr());
            text(r.x + 12.f, r.y + 6.f, pp.GetFileName().CStr(), NkColor{ 210, 220, 230, 255 });
            text(r.x + 12.f, r.y + 6.f + lh, path.CStr(), NkColor{ 120, 130, 140, 255 });
            if (hov && click) { d->DoLoad(pp.GetParent()); return; }
            ry += 50.f;
            if (ry > H - 60.f) break;
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
