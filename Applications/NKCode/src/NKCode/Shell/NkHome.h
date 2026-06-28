#pragma once
// =============================================================================
// NkHome.h — Ecran d'accueil (Launcher Home), reecriture propre d'apres le
// design Banani « Launcher — Accueil ». Sidebar (marque + navigation + versions)
// + panneau (filtres, workspaces recents groupes, actions rapides, exemples).
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"   // reutilise la logique d'actions (ouvrir/creer)

namespace nkentseu {
namespace nkcode {

    struct NkHomeState {
        NkCodeState*   st  = nullptr;
        NkCodeDialogs* dlg = nullptr;
        uint32  logoIcon = 0, logoWord = 0;   // textures (0 = repli dessine)
        int32   wordW = 0, wordH = 0;          // dimensions naturelles du wordmark (aspect)
        int32   nav = 0;                       // item de nav actif (0 = Accueil)
        float32 scroll = 0.f;                  // defilement de la liste des recents
    };

    // ── Item de navigation de la sidebar ──
    inline bool NkNavItem(const NkUi& u, const NkRect& r, const char* icon, const char* label,
                          bool active, const NkColor& accent) {
        const bool hov = u.Hit(r);
        if (active)      u.Rect(r, NkColor{ 22, 32, 46, 255 }, NkR::md * u.S);
        else if (hov)    u.Rect(r, NkCol::hover, NkR::md * u.S);
        if (active)      u.Rect({ r.x, r.y + u.s(6), u.s(3), r.h - u.s(12) }, accent, u.s(2));
        const NkColor ic = active ? accent : NkCol::sidebarFg;
        const float32 isz = u.s(16);
        u.Icon(icon, { r.x + u.s(14), r.y + (r.h - isz) * 0.5f, isz, isz }, ic);
        u.TextV(r.x + u.s(40), r.y, r.h, label, active ? NkCol::foreground : NkCol::sidebarFg);
        return hov && u.click;
    }

    // ── Sidebar ──
    inline void NkHomeSidebar(const NkUi& u, const NkRect& r, NkHomeState* H) {
        u.Rect(r, NkCol::sidebar);
        u.Rect({ r.x + r.w - 1.f, r.y, 1.f, r.h }, NkCol::border);

        // Logo (wordmark) ou repli dessine
        const float32 logoH = u.s(52);
        if (H->logoWord) {
            const float32 aspect = (H->wordH > 0) ? (float32)H->wordW / (float32)H->wordH : 4.f;
            float32 lh2 = u.s(30), lw = lh2 * aspect;
            const float32 maxW = r.w - u.s(28);
            if (lw > maxW) { lw = maxW; lh2 = lw / aspect; }
            u.dl->AddImage(H->logoWord, { r.x + u.s(14), r.y + (logoH - lh2) * 0.5f, lw, lh2 }, { 0,0 }, { 1,1 }, NkCol::foreground);
        } else {
            NkBrandMark(u, { r.x + u.s(12), r.y + u.s(12), u.s(28), u.s(28) }, NkCol::foreground);
            u.TextV(r.x + u.s(48), r.y, logoH, "nkcode", NkCol::foreground);
        }
        u.Rect({ r.x, r.y + logoH, r.w, 1.f }, NkCol::border);

        auto section = [&](float32 y, const char* title) {
            u.Text(r.x + u.s(16), y, title, NkCol::mutedFg);
        };
        float32 y = r.y + logoH + u.s(14);
        section(y, "PRINCIPAL"); y += u.s(20);
        struct NavDef { const char* icon; const char* label; };
        const NavDef main_[] = { {"house","Accueil"}, {"folder-open","Ouvrir"}, {"plus-circle","Nouveau Workspace"}, {"git-branch","Cloner (Git)"} };
        for (int32 i = 0; i < 4; ++i) {
            const NkRect ir = { r.x + u.s(8), y, r.w - u.s(16), u.s(34) };
            if (NkNavItem(u, ir, main_[i].icon, main_[i].label, H->nav == i, NkCol::primary)) {
                H->nav = i;
                if (i == 1) H->dlg->OpenFolderDialog();
                else if (i == 2) H->dlg->Open(NkCodeDialogs::NewWorkspace);
                else if (i == 3) {/* TODO clone git */}
            }
            y += u.s(36);
        }
        y += u.s(8);
        u.Rect({ r.x + u.s(16), y, r.w - u.s(32), 1.f }, NkCol::border); y += u.s(12);
        section(y, "OUTILS"); y += u.s(20);
        const NavDef tools[] = { {"wrench","Toolchains"}, {"cpu","Plateformes"}, {"settings","Parametres"} };
        for (int32 i = 0; i < 3; ++i) {
            const NkRect ir = { r.x + u.s(8), y, r.w - u.s(16), u.s(34) };
            if (NkNavItem(u, ir, tools[i].icon, tools[i].label, H->nav == 10 + i, NkCol::secondary)) {
                H->nav = 10 + i;
                if (i == 0 && H->dlg) H->dlg->tcOpen = true;   // ouvre la gestion des toolchains
            }
            y += u.s(36);
        }

        // Footer versions (bas de sidebar)
        const float32 fy = r.y + r.h - u.s(44);
        u.Rect({ r.x, fy - u.s(8), r.w, 1.f }, NkCol::border);
        u.Text(r.x + u.s(16), fy, "IDE", NkCol::mutedFg);
        u.Text(r.x + r.w - u.s(16) - u.TextW("1.0.0"), fy, "1.0.0", NkCol::mutedFg);
        u.Text(r.x + u.s(16), fy + u.s(16), "Jenga", NkCol::mutedFg);
        u.Text(r.x + r.w - u.s(16) - u.TextW("2.0.7"), fy + u.s(16), "2.0.7", NkCol::accent);
    }

    // ── Carte d'un workspace recent ──
    inline int32 NkWorkspaceCard(const NkUi& u, const NkRect& r, const char* name, const char* path,
                                 const NkColor& iconCol, bool pinned) {
        const bool hov = u.Hit(r);
        u.Rect(r, hov ? NkCol::hover : NkCol::surface, NkR::md * u.S);
        u.Stroke(r, hov ? NkColor{ 48, 54, 61, 255 } : NkCol::border, NkR::md * u.S);
        // pastille icone
        const float32 ic = u.s(30);
        u.Rect({ r.x + u.s(10), r.y + (r.h - ic) * 0.5f, ic, ic }, iconCol, NkR::sm * u.S);
        // nom + chemin
        u.Text(r.x + u.s(50), r.y + u.s(8), name, NkCol::foreground);
        u.Text(r.x + u.s(50), r.y + u.s(8) + u.Lh(), path, NkCol::mutedFg);
        // badge epingle + bouton retirer (au survol)
        int32 act = 0;   // 1 = charger, 2 = (des)epingler, 3 = retirer
        const NkRect bPin = { r.x + r.w - u.s(54), r.y + (r.h - u.s(20)) * 0.5f, u.s(20), u.s(20) };
        const NkRect bRem = { r.x + r.w - u.s(28), r.y + (r.h - u.s(20)) * 0.5f, u.s(20), u.s(20) };
        if (pinned || hov) {
            if (u.Hit(bPin)) u.Rect(bPin, NkCol::muted, NkR::sm * u.S);
            u.Icon("git-branch", bPin, pinned ? NkCol::accent : NkCol::mutedFg);   // pin ~ marqueur
            if (u.Hit(bRem)) u.Rect(bRem, NkCol::muted, NkR::sm * u.S);
            u.Icon("x", bRem, NkCol::mutedFg);
            if (u.Hit(bPin) && u.click) act = 2;
            else if (u.Hit(bRem) && u.click) act = 3;
        }
        if (act == 0 && hov && u.click) act = 1;
        return act;
    }

    // ── Action rapide (colonne droite) ──
    inline bool NkQuickAction(const NkUi& u, const NkRect& r, const char* icon, const char* label,
                              const char* sub, const NkColor& borderCol, const NkColor& iconCol) {
        const bool hov = u.Hit(r);
        u.Rect(r, hov ? NkCol::hover : NkCol::surface, NkR::md * u.S);
        u.Stroke(r, borderCol, NkR::md * u.S);
        const float32 isz = u.s(16);
        u.Icon(icon, { r.x + u.s(12), r.y + (r.h - isz) * 0.5f, isz, isz }, iconCol);
        u.Text(r.x + u.s(40), r.y + u.s(8), label, NkCol::foreground);
        u.Text(r.x + u.s(40), r.y + u.s(8) + u.Lh(), sub, NkCol::mutedFg);
        return hov && u.click;
    }

    // ── Panneau principal du Home ──
    inline void NkHomePanel(const NkUi& u, const NkRect& r, NkHomeState* H) {
        NkCodeState* st = H->st; NkCodeDialogs* dlg = H->dlg;
        u.Rect(r, NkCol::background);
        const float32 pad = u.s(24);
        const float32 rightW = u.s(240), gap = u.s(24);
        const NkRect left  = { r.x + pad, r.y + u.s(18), r.w - pad * 2.f - rightW - gap, r.h - u.s(36) };
        const NkRect right = { left.x + left.w + gap, r.y + u.s(18), rightW, r.h - u.s(36) };

        // ── Barre de filtres ──
        const float32 fh = u.s(30);
        NkRect search = { left.x, left.y, left.w - u.s(220), fh };
        u.Rect(search, NkCol::input, NkR::md * u.S); u.Stroke(search, NkCol::border, NkR::md * u.S);
        u.Icon("search", { search.x + u.s(9), search.y + (fh - u.s(13)) * 0.5f, u.s(13), u.s(13) }, NkCol::mutedFg);
        u.TextV(search.x + u.s(28), search.y, fh, "Filtrer les recents...", NkCol::mutedFg);
        auto chip = [&](float32 x, const char* t) -> NkRect {
            const float32 w = u.TextW(t) + u.s(34); const NkRect c = { x, left.y, w, fh };
            u.Rect(c, NkCol::input, NkR::md * u.S); u.Stroke(c, NkCol::border, NkR::md * u.S);
            u.TextV(c.x + u.s(10), c.y, fh, t, NkCol::mutedFg);
            u.Icon("chevron-down", { c.x + w - u.s(20), c.y + (fh - u.s(12)) * 0.5f, u.s(12), u.s(12) }, NkCol::mutedFg);
            return c;
        };
        chip(left.x + left.w - u.s(210), "C++");
        chip(left.x + left.w - u.s(100), "Windows");

        // ── Liste des recents (defilable) ──
        const NkRect listArea = { left.x, left.y + fh + u.s(14), left.w, left.h - fh - u.s(14) };
        if (u.Hit(listArea) && u.ctx->input.wheel != 0.f) { H->scroll -= u.ctx->input.wheel * u.s(34); u.ctx->input.wheel = 0.f; }
        u.dl->PushClipRect(listArea, true);
        float32 y = listArea.y - H->scroll;
        int32 doLoad = -1, doPin = -1, doRem = -1; bool loadCur = false;

        auto groupHeader = [&](const char* label, const NkColor& col, bool open) {
            u.Icon(open ? "chevron-down" : "chevron-right", { listArea.x, y + u.s(1), u.s(12), u.s(12) }, NkCol::mutedFg);
            u.Text(listArea.x + u.s(18), y, label, col);
            y += u.s(20);
        };
        const NkColor cols[] = { NkCol::primary, NkCol::accent, NkCol::secondary, NkColor{ 51,177,160,255 } };

        // Workspace courant (carte "en cours") en tete s'il y en a un
        if (st && st->HasWorkspace()) {
            groupHeader("EN COURS", NkCol::accent, true);
            const NkRect cr = { listArea.x, y, listArea.w, u.s(50) };
            if (NkWorkspaceCard(u, cr, st->root.GetFileName().CStr(), st->root.ToString().CStr(), NkCol::primary, false) == 1)
                loadCur = true;
            y += u.s(56);
        }
        // Epingles
        if (st && !st->pinned.Empty()) {
            groupHeader("EPINGLES", NkCol::accent, true);
            for (usize i = 0; i < st->pinned.Size(); ++i) {
                const char* nm = (i < st->pinnedNames.Size() && !st->pinnedNames[i].Empty()) ? st->pinnedNames[i].CStr() : st->pinned[i].CStr();
                const NkRect cr = { listArea.x, y, listArea.w, u.s(50) };
                const int32 a = NkWorkspaceCard(u, cr, nm, st->pinned[i].CStr(), cols[i % 4], true);
                if (a == 1) doLoad = (int32)(1000 + i); else if (a == 2) doPin = (int32)(1000 + i); else if (a == 3) doRem = (int32)(1000 + i);
                y += u.s(56);
            }
        }
        // Recents
        groupHeader("RECENTS", NkCol::mutedFg, true);
        if (!st || st->recents.Empty()) { u.Text(listArea.x + u.s(2), y, "(aucun workspace recent)", NkCol::mutedFg); y += u.s(24); }
        for (usize i = 0; st && i < st->recents.Size(); ++i) {
            const char* nm = (i < st->recentNames.Size() && !st->recentNames[i].Empty()) ? st->recentNames[i].CStr() : st->recents[i].CStr();
            const NkRect cr = { listArea.x, y, listArea.w, u.s(50) };
            const int32 a = NkWorkspaceCard(u, cr, nm, st->recents[i].CStr(), cols[i % 4], false);
            if (a == 1) doLoad = (int32)i; else if (a == 2) doPin = (int32)i; else if (a == 3) doRem = (int32)i;
            y += u.s(56);
        }
        const float32 contentH = (y + H->scroll) - listArea.y;
        u.dl->PopClipRect();
        const float32 maxS = contentH - listArea.h > 0.f ? contentH - listArea.h : 0.f;
        if (H->scroll < 0.f) H->scroll = 0.f; if (H->scroll > maxS) H->scroll = maxS;

        // applique les actions differees (apres le dessin)
        if (loadCur) { if (dlg) { if (H->st) H->st->RequestReload(); } dlg->showStart = false; return; }
        auto pathOf = [&](int32 idx) -> NkString { return idx >= 1000 ? st->pinned[idx - 1000] : st->recents[idx]; };
        if (doLoad >= 0) { dlg->DoLoad(NkPath(pathOf(doLoad).CStr()).GetParent()); return; }
        if (doPin  >= 0) { if (doPin >= 1000) st->UnpinRecent(pathOf(doPin)); else st->PinRecent(pathOf(doPin)); return; }
        if (doRem  >= 0) { st->RemoveRecent(pathOf(doRem)); return; }

        // ── Colonne droite : actions rapides + exemples ──
        u.Text(right.x, right.y, "ACTIONS RAPIDES", NkCol::mutedFg);
        float32 ry = right.y + u.s(22);
        const float32 qh = u.s(52);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, "plus-circle", "Nouveau Workspace", "Creer un workspace Jenga", NkCol::primary, NkCol::primary)) dlg->Open(NkCodeDialogs::NewWorkspace);
        ry += qh + u.s(10);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, "folder-open", "Ouvrir un Workspace", "Charger un .jenga existant", NkCol::border, NkCol::mutedFg)) dlg->OpenWorkspaceDialog();
        ry += qh + u.s(10);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, "folder", "Ouvrir un Dossier", "Explorer un dossier de projets", NkCol::border, NkCol::mutedFg)) dlg->OpenFolderDialog();
        ry += qh + u.s(10);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, "git-branch", "Cloner depuis Git", "GitHub, GitLab, Bitbucket...", NkCol::secondary, NkCol::secondaryFg)) {/* TODO */}
        ry += qh + u.s(16);

        u.Rect({ right.x, ry - u.s(8), right.w, 1.f }, NkCol::border);
        u.Text(right.x, ry, "EXEMPLES JENGA", NkCol::mutedFg); ry += u.s(22);
        const char* ex[] = { "01_hello_console", "09_multi_projects", "25_opengl_triangle", "24_all_platforms" };
        const NkRect exBox = { right.x, ry, right.w, u.s(34) * 5.f };
        u.Rect(exBox, NkCol::surface, NkR::md * u.S); u.Stroke(exBox, NkCol::border, NkR::md * u.S);
        for (int32 i = 0; i < 4; ++i) {
            const float32 ey = ry + i * u.s(34);
            u.Icon("book-open", { right.x + u.s(10), ey + u.s(11), u.s(12), u.s(12) }, NkCol::accent);
            u.TextV(right.x + u.s(28), ey, u.s(34), ex[i], NkCol::foreground);
            u.Rect({ right.x + u.s(8), ey + u.s(34), right.w - u.s(16), 1.f }, NkCol::border);
        }
        u.TextV(right.x, ry + u.s(34) * 4.f, u.s(34), "  Voir les 27 exemples", NkCol::primary);
    }

    // ── Point d'entree : ecran d'accueil PLEIN CADRE (via SetStartScreen) ──
    inline void DrawHome(NkEditorFrameContext& ec, NkHomeState* H) {
        if (!H || !H->dlg || !H->dlg->showStart) return;
        const NkUi u = NkUi::From(ec);
        if (!u.Valid()) return;
        // pompe jenga info (toolchains/projets) comme avant
        if (H->st) { H->st->ScanWorkspaces(); H->st->LoadProjects(); H->st->PollProjects(); H->st->PollConfig(); }
        const float32 W = (float32)u.ctx->viewW, Ht = (float32)u.ctx->viewH;
        const float32 top = u.ctx->ItemHeight();
        const float32 sbW = u.s(220);
        NkHomeSidebar(u, { 0.f, top, sbW, Ht - top }, H);
        NkHomePanel  (u, { sbW, top, W - sbW, Ht - top }, H);
    }

} // namespace nkcode
} // namespace nkentseu
