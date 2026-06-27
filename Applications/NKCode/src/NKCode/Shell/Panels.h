#pragma once
// =============================================================================
// Panels.h — Panneaux de l'IDE NKCode (sur NKEditorKit / NKGui).
//   Explorateur (arbre de fichiers reel) · Editeur (onglets + saisie multi-ligne)
//   · Sortie (resultat de jenga build).
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"

namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    // Texte COLORE sur une ligne : reserve un rect de la LARGEUR DU TEXTE (pas
    // pleine largeur, sinon un SameLine() suivant pousse l'item hors champ) et
    // dessine `s` en `col`. Avance le curseur (nouvelle ligne par defaut).
    inline void TermText(NkGuiContext& ctx, const char* s, const NkColor& col) {
        const float32 h = ctx.ItemHeight();
        const float32 w = (ctx.font && ctx.font->Valid() && s) ? ctx.font->MeasureWidth(s) + 4.f : 40.f;
        const NkRect  r = ctx.NextItemRect(w, h);
        if (ctx.font && ctx.font->Valid() && s && *s)
            ctx.DL().AddText(ctx.font->Face(), ctx.font->TexId(),
                             { r.x, r.y + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent() }, s, col);
    }

    // ── Explorateur : ARBRE repliable facon VSCode. Les dossiers s'ouvrent/ferment
    //    en place (chevron + indentation) ; clic fichier = ouvrir dans l'editeur. ──
    class ExplorerPanel : public NkEditorPanel {
    public:
        explicit ExplorerPanel(NkCodeState* s)
            : NkEditorPanel("Explorateur", NkEditorDockSide::NK_LEFT), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            // En-tete "EXPLORATEUR" + le PROJET comme NOEUD RACINE repliable (tout
            // l'arbre est imbrique dessous). SCOPE projet : pas de ".." -> jamais hors projet.
            ec.Text("EXPLORATEUR");
            ec.Separator();
            char name[260]; ToUpperInto(mS->root.GetFileName().CStr(), name, sizeof(name));
            char rootLbl[280]; std::snprintf(rootLbl, sizeof(rootLbl), "%s %s", mRootOpen ? "v" : ">", name);
            if (Selectable(ctx, rootLbl, false)) mRootOpen = !mRootOpen;
            if (mRootOpen) DrawTree(ctx, mS->root, 1);
        }
    private:
        // Dessine recursivement l'arbre du dossier `dir` a la profondeur `depth`.
        void DrawTree(NkGuiContext& ctx, const NkPath& dir, int32 depth) {
            if (depth > 24) return;                    // garde-fou
            NkVector<NkDirectoryEntry> entries =
                NkDirectory::GetEntries(dir, "*", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
            for (int pass = 0; pass < 2; ++pass) {     // dossiers d'abord, puis fichiers
                const bool wantDir = (pass == 0);
                for (usize i = 0; i < entries.Size(); ++i) {
                    const NkDirectoryEntry& e = entries[i];
                    if (e.IsDirectory != wantDir) continue;
                    char lbl[340]; int32 n = 0;
                    for (int32 d = 0; d < depth; ++d) { lbl[n++] = ' '; lbl[n++] = ' '; }  // indentation
                    if (e.IsDirectory) {
                        const bool exp = IsExpanded(e.FullPath);
                        lbl[n++] = exp ? 'v' : '>'; lbl[n++] = ' ';       // chevron
                        std::snprintf(lbl + n, sizeof(lbl) - n, "%s", e.Name.CStr());
                        if (Selectable(ctx, lbl, false)) ToggleExpanded(e.FullPath);
                        if (IsExpanded(e.FullPath)) DrawTree(ctx, e.FullPath, depth + 1);
                    } else {
                        lbl[n++] = ' '; lbl[n++] = ' ';                   // aligne sous le chevron
                        std::snprintf(lbl + n, sizeof(lbl) - n, "%s", e.Name.CStr());
                        if (Selectable(ctx, lbl, false)) mS->OpenPath(e.FullPath);
                    }
                }
            }
        }
        bool IsExpanded(const NkPath& p) const {
            const NkString s = p.ToString();
            for (usize i = 0; i < mExpanded.Size(); ++i)
                if (StrEq(mExpanded[i].CStr(), s.CStr())) return true;
            return false;
        }
        void ToggleExpanded(const NkPath& p) {
            const NkString s = p.ToString();
            for (usize i = 0; i < mExpanded.Size(); ++i)
                if (StrEq(mExpanded[i].CStr(), s.CStr())) { mExpanded.Erase(mExpanded.Begin() + i); return; }
            mExpanded.PushBack(s);
        }
        static void ToUpperInto(const char* s, char* dst, usize cap) {
            usize i = 0;
            if (s) for (; s[i] && i + 1 < cap; ++i) {
                char c = s[i];
                dst[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
            }
            dst[i] = '\0';
        }
        NkCodeState*       mS;
        NkVector<NkString> mExpanded;   // dossiers deplies (chemins) — etat persistant
        bool               mRootOpen = true;   // noeud racine (projet) deplie par defaut
    };

    // ── Editeur : onglets des fichiers ouverts + saisie multi-ligne du fichier actif. ──
    class EditorPanel : public NkEditorPanel {
    public:
        EditorPanel(NkCodeState* s, NkEditorShell* shell)
            : NkEditorPanel("Editeur", NkEditorDockSide::NK_CENTER), mS(s), mShell(shell) {}
        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            if (mS->files.Empty()) {
                if (mShell) mShell->SetFooter("NKCode", "Jenga");
                ec.Text("Ouvrez un fichier depuis l'Explorateur.");
                return;
            }

            // Bandeau d'onglets de fichiers CUSTOM (pilote par mS->active) : onglet
            // actif surligne, point "modifie", bouton X. Remplace le TabBar NKGui
            // (qui gardait son propre index et empechait la revelation au clic).
            DrawFileTabs(ctx);
            if (mS->active < 0 || mS->active >= static_cast<int32>(mS->files.Size())) mS->active = 0;
            if (mS->files.Empty()) return;

            OpenFile& f = mS->files[mS->active];
            // AvailHeight()/ContentWidth() = taille du CONTENU (scrollable, ~1e9), PAS
            // la taille visible -> on borne par le rect de CLIP (zone visible du dock)
            // sinon viewH gigantesque (pas de scrollbar, barre H hors ecran).
            const NkRect clip = ctx.DL().CurrentClip();
            NkRect r = { ctx.layout.cursor.x, ctx.layout.cursor.y, ctx.ContentWidth(), ctx.AvailHeight() };
            if (r.x + r.w > clip.x + clip.w) r.w = clip.x + clip.w - r.x;
            if (r.y + r.h > clip.y + clip.h) r.h = clip.y + clip.h - r.y;
            CodeEditor(ctx, "##code", f.doc, r, NkLangFromExt(f.path.GetExtension().CStr()));

            // Footer VSCode : nom du fichier (gauche) + Ln/Col + langage (droite).
            if (mShell) {
                char rbuf[128];
                std::snprintf(rbuf, sizeof(rbuf), "Ln %d, Col %d    %s",
                              f.doc.curLine + 1, f.doc.curCol + 1, LangOf(f.path));
                NkString left = f.Name();
                if (f.doc.dirty) left = NkString("* ") + left.CStr();
                mShell->SetFooter(left.CStr(), rbuf);

                // Infos centrees dans la barre de titre : "fichier - NKCode".
                char center[200];
                std::snprintf(center, sizeof(center), "%s%s - NKCode",
                              f.doc.dirty ? "* " : "", f.Name().CStr());
                mShell->SetTitleInfo(center);
            }
        }
    private:
        // Bandeau d'onglets de fichiers (facon VSCode) : dessine chaque fichier
        // ouvert, gere clic (activer) + X (fermer), puis avance le curseur de layout
        // sous le bandeau. Pilote par mS->active (source de verite).
        void DrawFileTabs(NkGuiContext& ctx) {
            const float32 h  = ctx.ItemHeight();
            const float32 x0 = ctx.layout.cursor.x, y0 = ctx.layout.cursor.y;
            const float32 fullW = ctx.ContentWidth();
            auto& dl = ctx.DL();
            dl.AddRectFilled({ x0, y0, fullW, h }, ctx.theme.tabBar);
            const NkVec2 m = ctx.input.mousePos;
            float32 x = x0; int32 toClose = -1;
            for (usize i = 0; i < mS->files.Size(); ++i) {
                OpenFile& f = mS->files[i];
                const NkString nm = f.Name();
                const float32 nameW = (ctx.font && ctx.font->Valid()) ? ctx.font->MeasureWidth(nm.CStr()) : 40.f;
                const float32 dotW = 16.f;
                const float32 tabW = nameW + 14.f + dotW + 6.f;
                const NkRect  tab  = { x, y0, tabW, h };
                const bool active = (static_cast<int32>(i) == mS->active);
                const bool hov = m.x >= tab.x && m.x < tab.x + tab.w && m.y >= tab.y && m.y < tab.y + tab.h;
                dl.AddRectFilled(tab, active ? ctx.theme.tabActive : (hov ? ctx.theme.tabHover : ctx.theme.tab));
                if (active) dl.AddRectFilled({ tab.x, y0 + h - 2.f, tab.w, 2.f }, ctx.theme.accent);
                if (ctx.font && ctx.font->Valid())
                    dl.AddText(ctx.font->Face(), ctx.font->TexId(),
                               { tab.x + 8.f, y0 + (h - ctx.font->LineHeight()) * 0.5f + ctx.font->Ascent() },
                               nm.CStr(), active ? ctx.theme.text : ctx.theme.textDisabled, nameW);
                // Zone droite : point "modifie" (si dirty et non survole) sinon X.
                const NkRect cl = { tab.x + tabW - dotW - 5.f, y0 + (h - dotW) * 0.5f, dotW, dotW };
                const bool clHov = m.x >= cl.x && m.x < cl.x + cl.w && m.y >= cl.y && m.y < cl.y + cl.h;
                if (f.doc.dirty && !clHov) {
                    dl.AddCircleFilled({ cl.x + cl.w * 0.5f, cl.y + cl.h * 0.5f }, 4.f, ctx.theme.text);
                } else {
                    if (clHov) dl.AddRectFilled(cl, ctx.theme.buttonHover);
                    const float32 cx = cl.x + cl.w * 0.5f, cy = cl.y + cl.h * 0.5f, a = 3.5f;
                    dl.AddLine({ cx - a, cy - a }, { cx + a, cy + a }, ctx.theme.text, 1.2f);
                    dl.AddLine({ cx - a, cy + a }, { cx + a, cy - a }, ctx.theme.text, 1.2f);
                }
                if (ctx.input.mouseClicked[0] && hov) { if (clHov) toClose = static_cast<int32>(i); else mS->active = static_cast<int32>(i); }
                dl.AddRectFilled({ tab.x + tabW - 1.f, y0, 1.f, h }, ctx.theme.border);
                x += tabW;
            }
            // Avance le curseur de layout SOUS le bandeau (l'editeur suit dessous).
            ctx.layout.cursor.x   = x0;
            ctx.layout.cursor.y   = y0 + h;
            ctx.layout.lineStartX = x0;
            ctx.layout.curLineH   = 0.f;
            if (toClose >= 0) mS->CloseFile(toClose);
        }
        // Langage devine a partir de l'extension (affiche dans le footer).
        static const char* LangOf(const NkPath& p) {
            const NkString e = p.GetExtension();
            const char* x = e.CStr();
            if (StrEq(x, ".cpp") || StrEq(x, ".cc") || StrEq(x, ".cxx") || StrEq(x, ".h") || StrEq(x, ".hpp")) return "C++";
            if (StrEq(x, ".c"))    return "C";
            if (StrEq(x, ".py"))   return "Python";
            if (StrEq(x, ".jenga"))return "Jenga";
            if (StrEq(x, ".md"))   return "Markdown";
            if (StrEq(x, ".json")) return "JSON";
            if (StrEq(x, ".txt"))  return "Texte";
            return "Texte";
        }
        NkCodeState*   mS;
        NkEditorShell* mShell;
    };

    // ── Sortie : resultat du build jenga (ASYNCHRONE : la sortie arrive en flux). ──
    class OutputPanel : public NkEditorPanel {
    public:
        explicit OutputPanel(NkCodeState* s)
            : NkEditorPanel("OUTPUT", NkEditorDockSide::NK_BOTTOM), mS(s) {}
        void OnUI(NkEditorFrameContext& ec) override {
            mS->PollBuild();   // recupere la sortie du build de fond, sans geler l'UI
            if (!mS->status.Empty()) { ec.Text(mS->status.CStr()); ec.Separator(); }
            if (mS->output.Empty()) { ec.Text("(Ctrl+B : construire le projet via jenga)"); return; }
            for (usize i = 0; i < mS->output.Size(); ++i) ec.Text(mS->output[i].CStr());
        }
    private:
        NkCodeState* mS;
    };

    // ── Terminal MULTI-SHELL facon VSCode : plusieurs terminaux internes
    //    (PowerShell / WSL / cmd / Jenga), onglets + bouton "+" + selecteur de
    //    shell. Chaque commande est routee vers le shell de l'onglet. Fond noir,
    //    invite coloree, execution ASYNC -> sortie en flux. ──
    class TerminalPanel : public NkEditorPanel {
    public:
        enum Shell { SH_PWSH = 0, SH_WSL, SH_BASH, SH_CMD, SH_JENGA, SH_COUNT };

        TerminalPanel() : NkEditorPanel("TERMINAL", NkEditorDockSide::NK_BOTTOM) {
            for (int32 i = 0; i < 8; ++i) mTerm[i].proc.SetKeepAnsi(true);   // couleurs ANSI
            mTerm[0].alive = true;   // un terminal PowerShell par defaut
        }

        void OnUI(NkEditorFrameContext& ec) override {
            auto& ctx = ec.Ui();
            auto& dl  = ctx.DL();
            if (!mTerm[mActive].alive) mActive = FirstAlive();
            Term& t = mTerm[mActive];
            t.proc.Drain(t.lines);

            const NkRect clip = dl.CurrentClip();
            dl.AddRectFilled(clip, NkColor{ 13, 17, 23, 255 });    // fond terminal #0D1117

            // Disposition VSCode : sortie a GAUCHE, LISTE des terminaux a DROITE.
            const float32 listW = (AliveCount() > 1 || true) ? ctx.S(190.f) : 0.f;
            const NkRect  mainR = { clip.x, clip.y, clip.w - listW, clip.h };
            const NkRect  listR = { clip.x + clip.w - listW, clip.y, listW, clip.h };
            DrawTermList(ctx, listR);

            // Mesure incrementale de la ligne la plus longue (barre H stable).
            if (ctx.font && ctx.font->Valid())
                while (t.measured < t.lines.Size()) {
                    const float32 w = ctx.font->MeasureWidth(t.lines[t.measured].CStr());
                    if (w > t.maxW) t.maxW = w; ++t.measured;
                }

            const float32 lineH   = (ctx.font && ctx.font->Valid()) ? ctx.font->LineHeight() : 16.f;
            const float32 promptH = ctx.ItemHeight(), pad = 6.f;
            const NkRect  out = { mainR.x, mainR.y, mainR.w, mainR.h - promptH };
            if (out.h > lineH) DrawOutput(ctx, t, out, lineH, pad);

            // Invite + saisie EN LIGNE, ancrees EN BAS de la zone gauche.
            ctx.layout.cursor     = { mainR.x + pad, mainR.y + mainR.h - promptH };
            ctx.layout.lineStartX = ctx.layout.cursor.x; ctx.layout.curLineH = 0.f;
            const float32 savedW = ctx.layout.region.w;
            ctx.layout.region.w   = mainR.x + mainR.w - ctx.layout.region.x;   // borne a la zone gauche
            const char* prompt = PromptOf(t.shell);
            TermText(ctx, prompt, NkColor{ 81, 154, 186, 255 });   // #519ABA
            ctx.SameLine();
            ctx.PushId(reinterpret_cast<const void*>(&t));         // id unique par terminal
            if (InputText(ctx, "##cmd", t.input, static_cast<int32>(sizeof(t.input)))) {
                if (t.input[0]) {
                    t.lines.PushBack(NkString(prompt) + " " + t.input);
                    t.proc.Start(WrapCmd(t.shell, t.input));
                    t.input[0] = '\0';
                    t.scrollY = 1.0e9f;                            // suit le bas apres une commande
                }
            }
            ctx.PopId();
            ctx.layout.region.w = savedW;
        }

    private:
        struct Term {
            NkProcess          proc;
            NkVector<NkString> lines;
            char               input[512] = {};
            int32              shell = SH_PWSH;
            bool               alive = false;
            float32            scrollX = 0.f, scrollY = 0.f;
            float32            maxW = 0.f;     // largeur ligne max (cache incremental)
            usize              measured = 0;   // nb de lignes deja mesurees
        };

        // Sortie defilante : rend les lignes visibles (clippees) + scrollbars V/H
        // avec fleches + auto-suivi du bas (facon vrai terminal).
        void DrawOutput(NkGuiContext& ctx, Term& t, const NkRect& out, float32 lineH, float32 pad) {
            auto& dl = ctx.DL();
            const NkColor kOut = { 204, 204, 204, 255 }, kCmd = { 223, 223, 223, 255 };
            const NkColor kTrk = { 25, 29, 35, 255 }, kThb = { 72, 79, 87, 200 }, kThbH = { 110, 118, 129, 235 };
            const float32 sbW = 14.f;
            const float32 viewW = out.w - sbW - pad * 2.f;
            const float32 viewH = out.h - sbW;
            const float32 contentH = t.lines.Size() * lineH;
            const float32 maxSY = contentH > viewH ? contentH - viewH : 0.f;
            const float32 maxSX = t.maxW > viewW ? t.maxW - viewW : 0.f;
            const NkVec2 m = ctx.input.mousePos;
            auto in = [&](const NkRect& r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
            const bool stick = (t.scrollY >= maxSY - lineH * 1.5f);            // colle au bas ?
            if (in(out)) {
                if (ctx.input.wheel != 0.f) { t.scrollY -= ctx.input.wheel * lineH * 3.f; ctx.input.wheel = 0.f; }
                if (ctx.input.wheelH != 0.f) { t.scrollX -= ctx.input.wheelH * 40.f; ctx.input.wheelH = 0.f; }
            }
            if (stick) t.scrollY = maxSY;
            if (t.scrollY < 0.f) t.scrollY = 0.f; if (t.scrollY > maxSY) t.scrollY = maxSY;
            if (t.scrollX < 0.f) t.scrollX = 0.f; if (t.scrollX > maxSX) t.scrollX = maxSX;

            // Lignes visibles.
            const NkRect txtClip = { out.x, out.y, out.w - sbW, viewH };
            dl.PushClipRect(txtClip, true);
            const int32 first = t.scrollY > 0.f ? static_cast<int32>(t.scrollY / lineH) : 0;
            const int32 last  = first + static_cast<int32>(viewH / lineH) + 1;
            if (ctx.font && ctx.font->Valid())
                for (int32 i = first; i <= last && i < static_cast<int32>(t.lines.Size()); ++i) {
                    if (i < 0) continue;
                    const float32 yb = out.y + i * lineH - t.scrollY + ctx.font->Ascent();
                    DrawAnsi(ctx, out.x + pad - t.scrollX, yb, t.lines[i].CStr(),
                             IsCmdLine(t.lines[i].CStr()) ? kCmd : kOut);   // couleurs ANSI
                }
            dl.PopClipRect();

            // Scrollbars V + H avec fleches.
            auto arrow = [&](const NkRect& r, int32 dir) -> bool {
                const bool h = in(r); if (h) dl.AddRectFilled(r, NkColor{ 33, 39, 48, 255 });
                const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, a = 3.2f; const NkColor c = h ? kThbH : kThb;
                if (dir == 0) dl.AddTriangleFilled({ cx, cy - a }, { cx - a, cy + a }, { cx + a, cy + a }, c);
                else if (dir == 1) dl.AddTriangleFilled({ cx - a, cy - a }, { cx + a, cy - a }, { cx, cy + a }, c);
                else if (dir == 2) dl.AddTriangleFilled({ cx - a, cy }, { cx + a, cy - a }, { cx + a, cy + a }, c);
                else dl.AddTriangleFilled({ cx - a, cy - a }, { cx + a, cy }, { cx - a, cy + a }, c);
                return h && ctx.input.mouseDown[0];
            };
            const NkRect vT = { out.x + out.w - sbW, out.y, sbW, viewH };
            const NkRect hT = { out.x, out.y + viewH, out.w - sbW, sbW };
            dl.AddRectFilled(vT, kTrk); dl.AddRectFilled(hT, kTrk);
            dl.AddRectFilled({ vT.x, hT.y, sbW, sbW }, kTrk);
            { const NkRect up = { vT.x, vT.y, sbW, sbW }, dn = { vT.x, vT.y + viewH - sbW, sbW, sbW };
              const NkRect iv = { vT.x, vT.y + sbW, sbW, viewH - 2.f * sbW };
              if (arrow(up, 0)) t.scrollY -= lineH * 0.8f; if (arrow(dn, 1)) t.scrollY += lineH * 0.8f;
              if (maxSY > 0.f && iv.h > 8.f) {
                  float32 th = iv.h * (viewH / contentH); if (th < 24.f) th = 24.f; if (th > iv.h) th = iv.h;
                  const float32 ty = iv.y + (t.scrollY / maxSY) * (iv.h - th);
                  if (ctx.input.mouseClicked[0] && in(iv)) ctx.activeId = ctx.GetId("##tvbar");
                  const bool act = (ctx.activeId == ctx.GetId("##tvbar"));
                  if (act && ctx.input.mouseDown[0]) { const float32 u = (m.y - iv.y - th * 0.5f) / (iv.h - th); t.scrollY = (u < 0 ? 0 : u > 1 ? 1 : u) * maxSY; }
                  dl.AddRectFilled({ iv.x + 3.f, ty, sbW - 6.f, th }, (act || in(iv)) ? kThbH : kThb, 3.f);
              } }
            { const NkRect lf = { hT.x, hT.y, sbW, sbW }, rt = { hT.x + hT.w - sbW, hT.y, sbW, sbW };
              const NkRect ih = { hT.x + sbW, hT.y, hT.w - 2.f * sbW, sbW };
              if (arrow(lf, 2)) t.scrollX -= 18.f; if (arrow(rt, 3)) t.scrollX += 18.f;
              if (maxSX > 0.f && ih.w > 8.f) {
                  float32 tw = ih.w * (viewW / t.maxW); if (tw < 24.f) tw = 24.f; if (tw > ih.w) tw = ih.w;
                  const float32 tx = ih.x + (t.scrollX / maxSX) * (ih.w - tw);
                  if (ctx.input.mouseClicked[0] && in(ih)) ctx.activeId = ctx.GetId("##thbar");
                  const bool act = (ctx.activeId == ctx.GetId("##thbar"));
                  if (act && ctx.input.mouseDown[0]) { const float32 u = (m.x - ih.x - tw * 0.5f) / (ih.w - tw); t.scrollX = (u < 0 ? 0 : u > 1 ? 1 : u) * maxSX; }
                  dl.AddRectFilled({ tx, hT.y + 3.f, tw, sbW - 6.f }, (act || in(ih)) ? kThbH : kThb, 3.f);
              } }
            if (t.scrollY < 0.f) t.scrollY = 0.f; if (t.scrollY > maxSY) t.scrollY = maxSY;
            if (t.scrollX < 0.f) t.scrollX = 0.f; if (t.scrollX > maxSX) t.scrollX = maxSX;
        }

        static const char* ShellName(int32 s) {
            switch (s) { case SH_PWSH: return "powershell"; case SH_WSL: return "wsl";
                         case SH_BASH: return "bash"; case SH_JENGA: return "jenga"; default: return "cmd"; }
        }
        static const char* PromptOf(int32 s) {
            switch (s) { case SH_PWSH: return "PS>"; case SH_WSL: return "wsl$";
                         case SH_BASH: return "bash$"; case SH_JENGA: return "jenga>"; default: return "cmd>"; }
        }
        // Enveloppe la commande tapee dans le shell choisi (execute via NkProcess).
        static NkString WrapCmd(int32 s, const char* cmd) {
            switch (s) {
                case SH_PWSH:  return NkString("powershell -NoProfile -Command \"") + cmd + "\"";
                case SH_WSL:   return NkString("wsl ") + cmd;
                case SH_BASH:  return NkString("bash -c \"") + cmd + "\"";
                case SH_JENGA: return NkString("jenga ") + cmd;
                default:       return NkString(cmd);   // cmd.exe (via _popen)
            }
        }
        static bool IsCmdLine(const char* s) {   // ligne = commande tapee (prefixe d'invite)
            return s[0] == 'P' || s[0] == 'w' || s[0] == 'b' || s[0] == 'c' || s[0] == 'j';
        }
        // Couleur de PREMIER PLAN ANSI (SGR 30-37 / 90-97). a==0 => code non gere.
        static NkColor AnsiFg(int32 code) {
            switch (code) {
                case 30: return {   1,   4,   9, 255 }; case 90: return { 110, 118, 129, 255 };
                case 31: return { 248,  81,  73, 255 }; case 91: return { 255, 123, 114, 255 };
                case 32: return {  63, 185,  80, 255 }; case 92: return {  86, 211, 100, 255 };
                case 33: return { 210, 153,  34, 255 }; case 93: return { 233, 196, 106, 255 };
                case 34: return {  88, 166, 255, 255 }; case 94: return { 121, 192, 255, 255 };
                case 35: return { 188, 140, 255, 255 }; case 95: return { 210, 168, 255, 255 };
                case 36: return {  57, 200, 214, 255 }; case 96: return {  86, 221, 232, 255 };
                case 37: return { 223, 223, 223, 255 }; case 97: return { 255, 255, 255, 255 };
                default: return {   0,   0,   0,   0 };
            }
        }
        // Dessine une ligne en interpretant les sequences ANSI couleur (ESC[...m).
        static void DrawAnsi(NkGuiContext& ctx, float32 x, float32 baseline, const char* s, const NkColor& def) {
            if (!ctx.font || !ctx.font->Valid()) return;
            const NkFont* face = ctx.font->Face(); const uint32 tex = ctx.font->TexId();
            auto& dl = ctx.DL();
            NkColor cur = def; const char* run = s; const char* p = s;
            auto flush = [&](const char* end) {
                if (end > run) { dl.AddTextRange(face, tex, { x, baseline }, run, end, cur); x += face->CalcTextSizeX(run, end); }
            };
            auto apply = [&](int32 code) {
                if (code == 0 || code == 39) cur = def;
                else { const NkColor c = AnsiFg(code); if (c.a != 0) cur = c; }
            };
            while (*p) {
                if (*p == 0x1b && p[1] == '[') {
                    flush(p);
                    const char* q = p + 2; int32 code = 0; bool any = false;
                    while (*q && *q != 'm' && !(*q >= '@' && *q <= '~')) {
                        if (*q >= '0' && *q <= '9') { code = code * 10 + (*q - '0'); any = true; }
                        else if (*q == ';') { apply(any ? code : 0); code = 0; any = false; }
                        ++q;
                    }
                    if (*q == 'm') { apply(any ? code : 0); ++q; }
                    else if (*q) ++q;
                    p = q; run = p;
                } else ++p;
            }
            flush(p);
        }
        int32 FirstAlive() const { for (int32 i = 0; i < 8; ++i) if (mTerm[i].alive) return i; return 0; }
        int32 AliveCount() const { int32 n = 0; for (int32 i = 0; i < 8; ++i) if (mTerm[i].alive) ++n; return n; }
        void AddTerm(int32 shell) {
            for (int32 i = 0; i < 8; ++i) if (!mTerm[i].alive) {
                mTerm[i].alive = true; mTerm[i].shell = shell;
                mTerm[i].lines.Clear(); mTerm[i].input[0] = '\0';
                mTerm[i].scrollX = mTerm[i].scrollY = 0.f; mTerm[i].maxW = 0.f; mTerm[i].measured = 0;
                mActive = i; return;
            }
        }
        void CloseTerm(int32 i) {
            if (AliveCount() <= 1) return;
            mTerm[i].alive = false;
            if (mActive == i) mActive = FirstAlive();
        }

        static NkColor ShellColor(int32 s) {
            switch (s) { case SH_PWSH: return { 31, 111, 235, 255 }; case SH_WSL: return { 233, 84, 32, 255 };
                         case SH_BASH: return { 77, 160, 79, 255 };  case SH_JENGA: return { 200, 150, 40, 255 };
                         default: return { 150, 158, 168, 255 }; }
        }

        // Liste VERTICALE des terminaux a DROITE (facon VSCode) : actions (+ / combo
        // de shell) en haut, puis un item par terminal (icone couleur + nom), actif
        // surligne, X au survol.
        void DrawTermList(NkGuiContext& ctx, const NkRect& R) {
            if (R.w < 8.f) return;
            auto& dl = ctx.DL();
            dl.AddRectFilled(R, NkColor{ 1, 4, 9, 255 });                       // fond liste #010409
            dl.AddRectFilled({ R.x, R.y, 1.f, R.h }, NkColor{ 33, 39, 48, 255 }); // bord gauche
            const NkVec2 m = ctx.input.mousePos;
            auto inR = [&](const NkRect& r) { return m.x >= r.x && m.x < r.x + r.w && m.y >= r.y && m.y < r.y + r.h; };
            const float32 h = ctx.ItemHeight();
            const float32 by = (h - (ctx.font ? ctx.font->LineHeight() : 14.f)) * 0.5f + (ctx.font ? ctx.font->Ascent() : 11.f);

            // Ligne d'actions : bouton "+" (a droite) + combobox de shell (a gauche).
            const NkRect addR = { R.x + R.w - h - 4.f, R.y + 3.f, h, h };
            { const bool hov = inR(addR); if (hov) dl.AddRectFilled(addR, ctx.theme.buttonHover);
              const float32 cx = addR.x + h * 0.5f, cy = addR.y + h * 0.5f, a = 5.f;
              dl.AddRectFilled({ cx - a, cy - 1.f, 2.f * a, 2.f }, ctx.theme.text);
              dl.AddRectFilled({ cx - 1.f, cy - a, 2.f, 2.f * a }, ctx.theme.text);
              if (hov && ctx.input.mouseClicked[0]) AddTerm(mNewShell); }
            const float32 savedW = ctx.layout.region.w;
            ctx.layout.cursor     = { R.x + 6.f, R.y + 3.f };
            ctx.layout.lineStartX = ctx.layout.cursor.x; ctx.layout.curLineH = 0.f;
            ctx.layout.region.w   = (ctx.layout.cursor.x - ctx.layout.region.x) + (R.w - h - 18.f);
            ctx.PushId("newshell");
            if (BeginCombo(ctx, "", ShellName(mNewShell), SH_COUNT)) {
                for (int32 i = 0; i < SH_COUNT; ++i)
                    if (Selectable(ctx, ShellName(i), i == mNewShell)) { mNewShell = i; ctx.ClosePopup(); }
                EndCombo(ctx);
            }
            ctx.PopId();
            ctx.layout.region.w = savedW;

            // Items : un par terminal vivant.
            float32 y = R.y + h + 6.f; int32 toClose = -1;
            for (int32 i = 0; i < 8; ++i) {
                if (!mTerm[i].alive) continue;
                const NkRect row = { R.x + 1.f, y, R.w - 1.f, h };
                const bool active = (i == mActive), hov = inR(row);
                if (active)    dl.AddRectFilled(row, NkColor{ 31, 111, 235, 55 });   // selection
                else if (hov)  dl.AddRectFilled(row, ctx.theme.buttonHover);
                if (active)    dl.AddRectFilled({ R.x + 1.f, y, 2.f, h }, ctx.theme.accent);
                dl.AddRectFilled({ R.x + 10.f, y + (h - 9.f) * 0.5f, 9.f, 9.f }, ShellColor(mTerm[i].shell)); // icone
                if (ctx.font && ctx.font->Valid())
                    dl.AddText(ctx.font->Face(), ctx.font->TexId(), { R.x + 26.f, y + by },
                               ShellName(mTerm[i].shell), active ? ctx.theme.text : ctx.theme.textDisabled);
                bool closeClicked = false;
                if (hov && AliveCount() > 1) {
                    const NkRect cl = { row.x + row.w - 20.f, y + (h - 14.f) * 0.5f, 14.f, 14.f };
                    const bool ch = inR(cl); if (ch) dl.AddRectFilled(cl, NkColor{ 33, 39, 48, 255 });
                    const float32 cx = cl.x + 7.f, cy = cl.y + 7.f, a = 3.f;
                    dl.AddLine({ cx - a, cy - a }, { cx + a, cy + a }, ctx.theme.text, 1.2f);
                    dl.AddLine({ cx - a, cy + a }, { cx + a, cy - a }, ctx.theme.text, 1.2f);
                    if (ch && ctx.input.mouseClicked[0]) { toClose = i; closeClicked = true; }
                }
                if (hov && !closeClicked && ctx.input.mouseClicked[0]) mActive = i;
                y += h;
            }
            if (toClose >= 0) CloseTerm(toClose);
        }

        Term  mTerm[8];
        int32 mActive   = 0;
        int32 mNewShell = SH_PWSH;
    };

} // namespace nkcode
