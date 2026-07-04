// =============================================================================
// NkCloneGit.h — Panneau « Cloner un depot Git » (plein cadre, 2 colonnes).
// Construit l'interface (mockup) ; le clonage reel (git process + progression)
// sera cable ensuite. Renvoie 1 si « Annuler » (retour Accueil), 0 sinon.
// =============================================================================
#pragma once
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"          // NkOwEdit (editeur caret) + NkOwIco
#include "NKCode/Shell/NkNewWorkspace.h"    // NkWizLabel / NkWizCheck (sans etat)
#include "NKCode/Shell/NkI18n.h"            // NkT() : traductions multi-langue
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"
#include <cstdio>

namespace nkentseu {
namespace nkcode {

    struct NkCgRecent { NkString url; NkString age; };

    struct NkCloneGitState {
        char    url[400]  = "";
        char    dest[400] = "";
        bool    subFolder = true;
        int32   branch = 0;                 // index dans NkCgBranches
        int32   depth = 0;                  // 0 Complete, 1 Shallow
        bool    recursive = false;
        bool    useSysCreds = true;
        bool    useToken = false; char token[200] = ""; bool showToken = false;
        bool    useSsh = false;
        int32   afterAction = 0;            // 0 IDE, 1 launcher, 2 explorateur
        // saisie
        int32   focus = -1, caret = 0; float32 blink = 0.f; bool focusClaimed = false;
        // combo deroulant differe
        int32   comboOpen = -1; NkRect comboR{}; const char* const* comboOpts = nullptr; int32 comboN = 0; int32* comboSel = nullptr; bool comboJustOpened = false;
        // recents (mockup)
        NkVector<NkCgRecent> recents;
        bool    inited = false;

        void SetFocus(int32 id, const char* buf) { focus = id; blink = 0.f; int32 n = 0; while (buf[n]) ++n; caret = n; }
        void EnsureInit(NkCodeState* st) {
            if (inited) return; inited = true; (void)st;
            const NkString base = NkOpenWsState::DefaultProjectDir();   // reglage Parametres > Chemins (projDir), sinon ~/Projects
            std::snprintf(dest, sizeof(dest), "%s", base.CStr());
            LoadRecents();   // depots reellement clones (vide tant qu'aucun clonage n'a eu lieu)
        }
        // Historique REEL des clones : ~/.nkcode/clone_history.txt  (1 ligne par depot : "url|age")
        static NkString HistFile() { const NkString home = NkOpenWsState::Home(); return (NkPath(home.CStr()) / ".nkcode" / "clone_history.txt").ToString(); }
        void LoadRecents() {
            recents = NkVector<NkCgRecent>();
            const NkString path = HistFile();
            if (!NkFile::Exists(path.CStr())) return;
            const NkString content = NkFile::ReadAllText(NkPath(path.CStr()));
            NkString line;
            for (const char* s = content.CStr(); ; ++s) {
                if (*s == '\n' || *s == '\0') {
                    if (!line.Empty()) { NkString url, age; bool sep = false;
                        for (const char* c = line.CStr(); *c; ++c) { if (*c == '|') { sep = true; continue; } if (*c == '\r') continue; if (sep) age += *c; else url += *c; }
                        if (!url.Empty()) recents.PushBack({ url, age.Empty() ? NkString(NkT("cg.recently")) : age }); }
                    line.Clear(); if (*s == '\0') break;
                } else line += *s;
            }
        }
        // Nom du depot extrait de l'URL (dernier segment sans .git).
        NkString RepoName() const {
            const char* s = url; const char* last = url;
            for (const char* c = url; *c; ++c) if (*c == '/' || *c == ':') last = c + 1;
            NkString n; for (const char* c = last; *c; ++c) n += *c;
            // retire .git final
            if (n.Size() >= 4) { const char* e = n.CStr() + n.Size() - 4; if (e[0] == '.' && e[1] == 'g' && e[2] == 'i' && e[3] == 't') { NkString m; for (usize i = 0; i + 4 <= n.Size(); ++i) m += n.CStr()[i]; n = m; } }
            (void)s; return n.Empty() ? NkString("repo") : n;
        }
        NkString FinalDest() const { if (!subFolder) return NkString(dest); NkString d = dest; if (!d.Empty() && d.CStr()[d.Size() - 1] != '/' && d.CStr()[d.Size() - 1] != '\\') d += "/"; d += RepoName(); return d; }
    };

    inline const char* const* NkCgBranches(int32* n) { static const char* k[] = { "main", "master", "develop", "(par defaut)" }; if (n) *n = 4; return k; }
    inline const char* const* NkCgDepth(int32* n)    { static const char* k[] = { "Complete", "Shallow (--depth 1)" }; if (n) *n = 2; return k; }

    // Champ texte (focus + caret). password=true : si showToken est FAUX (oeil ferme) -> masque (asterisques),
    // si showToken est VRAI (oeil ouvert) -> texte normal.
    inline void NkCgField(const NkUi& u, const NkRect& r, char* buf, int32 cap, int32 id, NkCloneGitState* g, float32 dt, bool blockBg, float32 leftPad, bool password = false) {
        const bool foc = (g->focus == id);
        const bool masked = password && !g->showToken;
        u.Panel(r, NkCol::input, foc ? NkCol::primary : NkCol::border, NkR::md * u.S);
        const float32 ty = r.y + (r.h - u.Lh()) * 0.5f;
        // Edition unifiee (selection, copier/couper/coller, double-clic) — masquee si mot de passe.
        if (foc) {
            NkOwEdit(u, r, buf, cap, g->caret, g->blink, dt, leftPad, masked);
        } else if (masked) {
            int32 len = 0; while (buf[len]) ++len; NkString dots; for (int32 i = 0; i < len; ++i) dots += "*";
            if (len) u.TextEllipsis(r.x + leftPad, ty, r.w - leftPad - u.s(8), dots.CStr(), NkCol::foreground);
        } else {
            u.TextEllipsis(r.x + leftPad, ty, r.w - leftPad - u.s(8), buf, buf[0] ? NkCol::foreground : NkCol::mutedFg);
        }
        if (!blockBg && u.Hit(r) && u.click) {
            int32 len = 0; while (buf[len]) ++len;
            g->focusClaimed = true;
            if (masked) { g->SetFocus(id, buf); g->caret = len; return; }   // mot de passe : caret en fin
            const float32 viewW = r.w - leftPad - u.s(6); float32 off = 0.f;
            if (foc) { NkString pre; const int32 cc = (g->caret < len ? g->caret : len); for (int32 k = 0; k < cc; ++k) pre += buf[k]; const float32 pw = u.TextW(pre.CStr()); off = (pw > viewW) ? (pw - viewW) : 0.f; }
            const float32 target = u.mp.x - (r.x + leftPad) + off;
            int32 best = 0; float32 bestd = 1e9f; NkString acc;
            for (int32 i = 0; ; ++i) { const float32 wv = u.TextW(acc.CStr()); const float32 d = (wv > target) ? (wv - target) : (target - wv); if (d < bestd) { bestd = d; best = i; } if (!buf[i]) break; acc += buf[i]; }
            g->SetFocus(id, buf); g->caret = best;
        }
    }

    // Combo (menu differe rendu en fin de panneau).
    inline void NkCgCombo(const NkUi& u, const NkRect& r, int32 id, NkCloneGitState* g, const char* const* opts, int32 n, int32* sel, bool blockBg) {
        const bool open = g->comboOpen == id;
        u.Panel(r, NkCol::input, open ? NkCol::primary : NkCol::border, NkR::md * u.S);
        const int32 s = (*sel >= 0 && *sel < n) ? *sel : 0;
        u.TextEllipsis(r.x + u.s(10), r.y + (r.h - u.Lh()) * 0.5f, r.w - u.s(34), opts[s], NkCol::foreground);
        u.Icon("chevron-down", { r.x + r.w - u.s(18), r.y + (r.h - u.s(10)) * 0.5f, u.s(10), u.s(10) }, NkCol::mutedFg);
        if (!blockBg && u.Hit(r) && u.click) {
            if (open) g->comboOpen = -1;
            else { g->comboOpen = id; g->comboR = r; g->comboOpts = opts; g->comboN = n; g->comboSel = sel; g->comboJustOpened = true; }
        }
    }

    // Bouton radio (cercle + label) ; renvoie true si clique.
    inline bool NkCgRadio(const NkUi& u, float32 x, float32 cy, const char* label, bool on, bool blockBg) {
        const float32 rd = u.s(16);
        const NkRect c = { x, cy - rd * 0.5f, rd, rd };
        u.Panel(c, on ? NkCol::secondary : NkCol::input, on ? NkCol::primary : NkCol::border, rd * 0.5f);   // cercle (carre tres arrondi)
        if (on) u.dl->AddRectFilled({ c.x + rd * 0.28f, c.y + rd * 0.28f, rd * 0.44f, rd * 0.44f }, NkCol::primary, rd * 0.22f);
        const float32 lw = u.TextW(label);
        const NkRect hit = { x - u.s(3), cy - rd * 0.5f - u.s(4), rd + u.s(10) + lw + u.s(10), rd + u.s(8) };
        const bool hv = u.Hit(hit);
        u.Text(x + rd + u.s(10), cy - u.Lh() * 0.5f, label, hv ? NkCol::foreground : NkCol::sidebarFg);
        return hv && u.click && !blockBg;
    }

    // ── Panneau plein cadre « Cloner un depot Git ». ──
    inline int32 NkCloneGitPanel(const NkUi& u, const NkRect& r, NkCloneGitState* g, NkCodeState* st, NkCodeDialogs* dlg, float32 dt, const NkIcons& ic) {
        g->EnsureInit(st);
        int32 result = 0;
        const bool blockBg = (g->comboOpen >= 0) || NkTxtMenu().open || (dlg && dlg->pickerOpen);
        g->focusClaimed = false;   // un champ revendiquera le clic ; sinon clic dans le vide = deselection

        // En-tete
        const float32 hH = u.s(54);
        u.Rect({ r.x, r.y, r.w, hH }, NkCol::sidebar);
        u.Rect({ r.x, r.y + hH - 1.f, r.w, 1.f }, NkCol::border);
        NkOwIco(u, ic.cloner, "git-branch", { r.x + u.s(28), r.y + (hH - u.s(18)) * 0.5f, u.s(18), u.s(18) }, NkCol::primary);
        u.Text(r.x + u.s(54), r.y + (hH - u.Lh()) * 0.5f, NkT("cg.title"), NkCol::foreground);

        const float32 padL = u.s(28);
        const float32 rightW = (r.w > u.s(820)) ? u.s(300) : u.s(260);
        const float32 cx = r.x + padL;
        const float32 rx = r.x + r.w - u.s(28) - rightW;
        const float32 cw = (rx - u.s(34)) - cx;
        const float32 fH = u.s(34);

        // ===== COLONNE GAUCHE =====
        float32 y = r.y + hH + u.s(22);
        // URL
        NkWizLabel(u, cx, y, NkT("cg.url")); y += u.s(22);
        NkCgField(u, { cx, y, cw, fH }, g->url, (int32)sizeof(g->url), 1, g, dt, blockBg, u.s(34));
        NkOwIco(u, ic.github, "git-branch", { cx + u.s(11), y + (fH - u.s(14)) * 0.5f, u.s(14), u.s(14) }, NkCol::mutedFg);
        y += fH + u.s(10);
        { const char* badges[] = { "GitHub", "GitLab", "Bitbucket", "Azure DevOps", "SSH", "HTTPS" };
          float32 bx = cx; for (int32 i = 0; i < 6; ++i) { const float32 bw = u.TextW(badges[i]) + u.s(16);
              u.Panel({ bx, y, bw, u.s(20) }, NkCol::muted, NkCol::border, NkR::sm * u.S);
              u.TextV(bx + u.s(8), y - u.s(3), u.s(20), badges[i], NkCol::mutedFg); bx += bw + u.s(6); } }
        y += u.s(34);
        // DESTINATION
        NkWizLabel(u, cx, y, NkT("cg.dest")); y += u.s(22);
        { const float32 browseW = u.s(120);
          NkCgField(u, { cx, y, cw - browseW - u.s(8), fH }, g->dest, (int32)sizeof(g->dest), 2, g, dt, blockBg, u.s(10));
          const NkRect br = { cx + cw - browseW, y, browseW, fH }; const bool hv = !blockBg && u.Hit(br);
          u.Rect(br, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, ic.ouvrirDossier, "folder-open", { br.x + u.s(14), br.y + (fH - u.s(14)) * 0.5f, u.s(14), u.s(14) }, NkCol::mutedFg);
          u.TextV(br.x + u.s(34), br.y, fH, NkT("btn.browse"), NkCol::foreground);
          if (hv && u.click && dlg) { g->focus = -1; dlg->BrowseInto(g->dest, (int32)sizeof(g->dest), "Dossier de destination"); } }
        y += fH + u.s(10);
        if (NkWizCheck(u, cx, y + u.s(8), NkT("cg.subfolder"), g->subFolder, blockBg)) g->subFolder = !g->subFolder;
        y += u.s(34);
        // OPTIONS
        NkWizLabel(u, cx, y, NkT("tc.options")); y += u.s(22);
        { int32 nB = 0, nD = 0; const char* const* br = NkCgBranches(&nB); const char* const* dp = NkCgDepth(&nD);
          const float32 colW = (cw - u.s(16)) * 0.5f;
          u.Text(cx, y, NkT("cg.branch"), NkCol::mutedFg); u.Text(cx + colW + u.s(16), y, NkT("cg.depth"), NkCol::mutedFg);
          NkCgCombo(u, { cx, y + u.s(16), colW, u.s(30) }, 10, g, br, nB, &g->branch, blockBg);
          NkCgCombo(u, { cx + colW + u.s(16), y + u.s(16), colW, u.s(30) }, 11, g, dp, nD, &g->depth, blockBg);
          y += u.s(16) + u.s(30) + u.s(12); }
        if (NkWizCheck(u, cx, y + u.s(8), NkT("cg.recursive"), g->recursive, blockBg)) g->recursive = !g->recursive;
        y += u.s(36);
        // AUTHENTIFICATION
        NkWizLabel(u, cx, y, NkT("cg.auth")); y += u.s(22);
        if (NkWizCheck(u, cx, y + u.s(8), NkT("cg.syscreds"), g->useSysCreds, blockBg)) g->useSysCreds = !g->useSysCreds;
        y += u.s(30);
        if (NkWizCheck(u, cx, y + u.s(8), NkT("cg.token"), g->useToken, blockBg)) g->useToken = !g->useToken;
        y += u.s(30);
        { const bool en = g->useToken; const bool dis = blockBg || !en;
          NkCgField(u, { cx + u.s(14), y, cw - u.s(60), u.s(30) }, g->token, (int32)sizeof(g->token), 3, g, dt, dis, u.s(10), true);
          const NkRect er = { cx + cw - u.s(40), y, u.s(40), u.s(30) }; const bool hv = !dis && u.Hit(er);
          u.Rect(er, hv ? NkCol::hover : NkCol::muted, NkR::md * u.S);
          NkOwIco(u, g->showToken ? ic.oeilOuvert : ic.oeilFermer, g->showToken ? "eye-off" : "eye", { er.x + u.s(13), er.y + u.s(8), u.s(14), u.s(14) }, en ? NkCol::foreground : NkCol::mutedFg);
          if (hv && u.click) g->showToken = !g->showToken; }
        y += u.s(40);
        if (NkWizCheck(u, cx, y + u.s(8), NkT("cg.ssh"), g->useSsh, blockBg)) g->useSsh = !g->useSsh;
        y += u.s(36);
        // APRES LE CLONAGE
        NkWizLabel(u, cx, y, NkT("cg.afterclone")); y += u.s(24);
        if (NkCgRadio(u, cx, y, NkT("cg.after0"), g->afterAction == 0, blockBg)) g->afterAction = 0; y += u.s(28);
        if (NkCgRadio(u, cx, y, NkT("cg.after1"), g->afterAction == 1, blockBg)) g->afterAction = 1; y += u.s(28);
        if (NkCgRadio(u, cx, y, NkT("cg.after2"), g->afterAction == 2, blockBg)) g->afterAction = 2; y += u.s(28);

        // ===== COLONNE DROITE =====
        float32 ry = r.y + hH + u.s(22);
        NkWizLabel(u, rx, ry, NkT("cg.recent")); ry += u.s(22);
        if (g->recents.Empty()) { const NkRect box = { rx, ry, rightW, u.s(40) }; u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            u.TextV(box.x + u.s(12), box.y, u.s(40), NkT("cg.norepo"), NkCol::mutedFg); ry += u.s(48); }
        for (usize i = 0; i < g->recents.Size(); ++i) {
            const NkRect box = { rx, ry, rightW, u.s(48) };
            u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
            // GitHub -> icone GitHub, sinon icone clone ; teinte ORANGE (accent).
            const bool isGh = g->recents[i].url.Contains("github");
            NkOwIco(u, isGh ? ic.github : ic.cloner, "git-branch", { box.x + u.s(12), box.y + u.s(10), u.s(14), u.s(14) }, NkCol::accent);
            u.TextEllipsis(box.x + u.s(32), box.y + u.s(8), rightW - u.s(120), g->recents[i].url.CStr(), NkCol::foreground);
            u.Text(box.x + u.s(32), box.y + u.s(26), g->recents[i].age.CStr(), NkCol::mutedFg);
            const NkRect rb = { box.x + rightW - u.s(80), box.y + u.s(12), u.s(70), u.s(24) }; const bool hv = !blockBg && u.Hit(rb);
            u.Rect(rb, hv ? NkCol::hover : NkCol::muted, NkR::sm * u.S);
            NkOwIco(u, ic.clonerTel, "refresh", { rb.x + u.s(6), rb.y + u.s(6), u.s(11), u.s(11) }, hv ? NkCol::foreground : NkCol::mutedFg);
            u.TextV(rb.x + u.s(20), rb.y, u.s(24), NkT("cg.reclone"), hv ? NkCol::foreground : NkCol::mutedFg);
            if (hv && u.click) { std::snprintf(g->url, sizeof(g->url), "https://%s.git", g->recents[i].url.CStr()); }
            ry += u.s(56);
        }
        ry += u.s(6);
        // APERCU
        NkWizLabel(u, rx, ry, NkT("cg.preview")); ry += u.s(22);
        { const NkRect box = { rx, ry, rightW, u.s(78) };
          u.Panel(box, NkCol::surface, NkCol::border, NkR::md * u.S);
          int32 nB = 0; const char* const* brs = NkCgBranches(&nB);
          NkOwIco(u, ic.github, "git-branch", { box.x + u.s(12), box.y + u.s(10), u.s(13), u.s(13) }, NkCol::primary);
          u.TextEllipsis(box.x + u.s(32), box.y + u.s(8), rightW - u.s(44), g->url[0] ? g->url : NkT("cg.urlph"), g->url[0] ? NkCol::foreground : NkCol::mutedFg);
          u.Icon("git-branch", { box.x + u.s(12), box.y + u.s(34), u.s(12), u.s(12) }, NkCol::mutedFg);
          char bl[80]; std::snprintf(bl, sizeof(bl), "branche: %s", (g->branch >= 0 && g->branch < nB) ? brs[g->branch] : "main");
          u.TextEllipsis(box.x + u.s(32), box.y + u.s(32), rightW - u.s(44), bl, NkCol::mutedFg);
          NkOwIco(u, ic.ouvrirDossier, "folder", { box.x + u.s(12), box.y + u.s(56), u.s(12), u.s(12) }, NkCol::mutedFg);
          u.TextEllipsis(box.x + u.s(32), box.y + u.s(54), rightW - u.s(44), g->FinalDest().CStr(), NkCol::mutedFg);
          ry += u.s(86); }
        // info
        { const NkRect box = { rx, ry, rightW, u.s(64) };
          u.Panel(box, NkCol::secondary, NkCol::border, NkR::md * u.S);
          NkOwIco(u, ic.rondI, "info", { box.x + u.s(12), box.y + u.s(12), u.s(14), u.s(14) }, NkCol::primary);
          const char* msg = "Apres le clonage, l'IDE detectera tout fichier .jenga dans le dossier clone.";
          // wrap simple en 3 lignes
          u.TextEllipsis(box.x + u.s(34), box.y + u.s(10), rightW - u.s(44), NkT("cg.prev1"), NkCol::foreground);
          u.TextEllipsis(box.x + u.s(34), box.y + u.s(26), rightW - u.s(44), NkT("cg.prev2"), NkCol::foreground);
          u.TextEllipsis(box.x + u.s(34), box.y + u.s(42), rightW - u.s(44), NkT("cg.prev3"), NkCol::foreground);
          (void)msg; ry += u.s(72); }
        // Boutons Annuler / Cloner
        { const NkRect ca = { rx, ry, rightW, fH }; if (u.Button(ca, NkT("btn.cancel"), NkCol::muted, NkCol::hover, NkCol::foreground, NkR::md * u.S) && !blockBg) result = 1; ry += fH + u.s(10); }
        { const bool valid = g->url[0] != 0; const NkRect cl = { rx, ry, rightW, u.s(40) };
          const bool hv = valid && !blockBg && u.Hit(cl);
          u.Rect(cl, !valid ? NkCol::muted : (hv ? NkColHover(NkCol::primary) : NkCol::primary), NkR::md * u.S);
          const NkColor fg = valid ? NkCol::primaryFg : NkCol::mutedFg;
          NkOwIco(u, ic.clonerTel, "download", { cl.x + (rightW - u.TextW(NkT("cg.clone")) - u.s(20)) * 0.5f, cl.y + u.s(13), u.s(14), u.s(14) }, fg);
          u.TextV(cl.x + (rightW - u.TextW(NkT("cg.clone")) - u.s(20)) * 0.5f + u.s(20), cl.y, u.s(40), NkT("cg.clone"), fg);
          // (clonage reel a cabler) ; pour l'instant pas d'action
          (void)hv; }

        // Deselection des champs si on clique dans le vide (aucun champ n'a pris le clic).
        if (u.click && !blockBg && !g->focusClaimed && u.Hit(r)) g->focus = -1;

        // ── Dropdown de combo (par-dessus tout) ──
        if (g->comboOpen >= 0 && g->comboOpts && g->comboSel) {
            const float32 ih = u.s(26);
            float32 ddw = g->comboR.w;
            for (int32 k = 0; k < g->comboN; ++k) { const float32 tw = u.TextW(g->comboOpts[k]) + u.s(30); if (tw > ddw) ddw = tw; }
            float32 ddx = g->comboR.x; if (ddx + ddw > r.x + r.w - u.s(8)) ddx = r.x + r.w - u.s(8) - ddw;
            const NkRect dd = { ddx, g->comboR.y + g->comboR.h + u.s(2), ddw, g->comboN * ih + u.s(6) };
            u.dl->AddRectFilled({ dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h }, NkColor{ 0,0,0,90 }, NkR::md * u.S);
            u.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
            bool chose = false;
            for (int32 k = 0; k < g->comboN; ++k) {
                const NkRect ir = { dd.x + u.s(4), dd.y + u.s(3) + k * ih, dd.w - u.s(8), ih };
                const bool hv = u.Hit(ir);
                if (hv || k == *g->comboSel) u.Rect(ir, NkCol::hover, NkR::sm * u.S);
                u.TextEllipsis(ir.x + u.s(8), ir.y + (ih - u.Lh()) * 0.5f, ir.w - u.s(12), g->comboOpts[k], NkCol::foreground);
                if (hv && u.click) { *g->comboSel = k; chose = true; }
            }
            if (chose || (u.click && !u.Hit(dd) && !g->comboJustOpened)) g->comboOpen = -1;
            g->comboJustOpened = false;
        }
        if (u.ctx->input.KeyPressed(NkGuiKey::Escape)) { if (g->comboOpen >= 0) g->comboOpen = -1; else if (g->focus >= 0) g->focus = -1; else result = 1; }
        return result;
    }

} // namespace nkcode
} // namespace nkentseu
