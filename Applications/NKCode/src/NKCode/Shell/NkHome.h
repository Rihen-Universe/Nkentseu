#pragma once
// =============================================================================
// NkHome.h — Ecran d'accueil (Launcher Home), reecriture propre d'apres le
// design Banani « Launcher — Accueil ». Sidebar (marque + navigation + versions)
// + panneau (filtres, workspaces recents groupes, actions rapides, exemples).
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"   // reutilise la logique d'actions (ouvrir/creer)
#include <cstdio>

namespace nkentseu {
namespace nkcode {

    struct NkHomeState {
        NkCodeState*   st  = nullptr;
        NkCodeDialogs* dlg = nullptr;
        uint32  logoIcon = 0, logoWord = 0;   // textures (0 = repli dessine)
        int32   wordW = 0, wordH = 0;          // dimensions naturelles du wordmark (aspect)
        NkIcons icons;                          // icones SVG (data/textures/icon)
        int32   nav = 0;                       // item de nav actif (0 = Accueil)
        float32 scroll = 0.f;                  // defilement de la liste des recents
    };

    // ── Item de navigation de la sidebar ──
    inline bool NkNavItem(const NkUi& u, const NkRect& r, uint32 icon, const char* label,
                          bool active, const NkColor& accent) {
        const bool hov = u.Hit(r);
        if (active)      u.Rect(r, NkColor{ 22, 32, 46, 255 }, NkR::md * u.S);
        else if (hov)    u.Rect(r, NkCol::hover, NkR::md * u.S);
        if (active)      u.Rect({ r.x, r.y + u.s(6), u.s(3), r.h - u.s(12) }, accent, u.s(2));
        const NkColor ic = active ? accent : NkColor{ 188, 196, 206, 255 };   // icone plus lumineuse
        const float32 isz = u.s(19);
        NkDrawIcon(u, icon, { r.x + u.s(13), r.y + (r.h - isz) * 0.5f, isz, isz }, ic);
        u.TextV(r.x + u.s(42), r.y, r.h, label, active ? NkCol::foreground : NkCol::sidebarFg);
        return hov && u.click;
    }

    // ── Sidebar ──
    inline void NkHomeSidebar(const NkUi& u, const NkRect& r, NkHomeState* H) {
        u.Rect(r, NkCol::sidebar);
        u.Rect({ r.x + r.w - 1.f, r.y, 1.f, r.h }, NkCol::border);

        // Logo dans un CADRE borde (cf. maquette) — wordmark aspect 512:128 preserve.
        const float32 logoH = u.s(78);
        const NkRect box = { r.x + u.s(12), r.y + u.s(12), r.w - u.s(24), logoH - u.s(22) };
        u.Panel(box, NkColor{ 9, 19, 14, 255 }, NkCol::border, NkR::md * u.S);   // fond sombre legerement vert
        if (H->logoWord) {
            const float32 aspect = (H->wordH > 0) ? (float32)H->wordW / (float32)H->wordH : 4.f;
            const float32 padX = u.s(12), padY = u.s(8);
            float32 lw = box.w - padX * 2.f, lh2 = lw / aspect;
            if (lh2 > box.h - padY * 2.f) { lh2 = box.h - padY * 2.f; lw = lh2 * aspect; }
            u.dl->AddImage(H->logoWord, { box.x + (box.w - lw) * 0.5f, box.y + (box.h - lh2) * 0.5f, lw, lh2 }, { 0,0 }, { 1,1 }, NkCol::foreground);
        } else {
            NkBrandMark(u, { box.x + u.s(8), box.y + (box.h - u.s(28)) * 0.5f, u.s(28), u.s(28) }, NkCol::foreground);
            u.TextV(box.x + u.s(44), box.y, box.h, "nkcode", NkCol::foreground);
        }

        auto section = [&](float32 y, const char* title) {
            u.Text(r.x + u.s(16), y, title, NkCol::mutedFg);
        };
        float32 y = r.y + logoH + u.s(14);
        section(y, "PRINCIPAL"); y += u.s(20);
        struct NavDef { uint32 icon; const char* label; };
        const NavDef main_[] = { {H->icons.accueil,"Accueil"}, {H->icons.ouvrir,"Ouvrir"}, {H->icons.nouveau,"Nouveau Workspace"}, {H->icons.cloner,"Cloner (Git)"} };
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
        const NavDef tools[] = { {H->icons.toolchains,"Toolchains"}, {H->icons.platforms,"Plateformes"}, {H->icons.gear,"Parametres"} };
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

    // Proprietes affichees sur la carte d'un workspace (cf. maquette).
    struct NkWsInfo {
        const char* name = ""; const char* path = "";
        const char* langVer = "";    // "C++20"
        const char* configs = "";    // "Debug, Release"
        const char* platforms = "";  // "Windows, Linux, Android, Web"
        const char* projects = "";   // "Renderer, Physics, Audio, UI +1"
        const char* buildConfig = "";// "Debug"
        const char* modified = "";   // "Modifie il y a 2h"
        int32 build = 0;             // 0 inconnu,1 ok,2 erreur,3 partiel
        bool pinned = false; uint32 icon = 0; NkColor iconBg{ 15,115,213,255 };
    };
    // ── Carte d'un workspace : nom/chemin, langage·configs·plateformes, projets,
    //    statut de build + modif, etoile + menu (format maquette) ──
    inline int32 NkWorkspaceCard(const NkUi& u, const NkRect& r, const NkWsInfo& w, uint32 starIcon) {
        const bool hov = u.Hit(r);
        u.Panel(r, hov ? NkCol::hover : NkCol::surface, hov ? NkColor{ 48,54,61,255 } : NkCol::border, NkR::lg * u.S);
        const float32 lh = u.Lh();
        // pastille icone (logo projet)
        const float32 ic = u.s(38);
        const NkRect icR = { r.x + u.s(14), r.y + u.s(14), ic, ic };
        u.Rect(icR, w.iconBg, NkR::sm * u.S);
        if (w.icon) NkDrawIcon(u, w.icon, { icR.x + u.s(8), icR.y + u.s(8), ic - u.s(16), ic - u.s(16) }, NkCol::foreground);
        const float32 tx = r.x + u.s(62);
        float32 y = r.y + u.s(12);
        // nom + chemin
        u.Text(tx, y, w.name, NkCol::foreground);
        u.Text(tx + u.TextW(w.name) + u.s(10), y + u.s(1), w.path, NkCol::mutedFg);
        y += lh + u.s(8);
        // ligne meta : langage · configs · plateformes (segments + separateurs)
        float32 mx = tx;
        const float32 dotW = u.s(14);
        auto seg = [&](const char* t, const NkColor& c) {
            if (!t || !*t) return;
            if (mx > tx) { u.dl->AddRectFilled({ mx + dotW * 0.5f - u.s(1.5f), y + lh * 0.5f - u.s(1.5f), u.s(3), u.s(3) }, NkCol::mutedFg, u.s(1.5f)); mx += dotW; }
            u.Text(mx, y, t, c); mx += u.TextW(t);
        };
        seg(w.langVer, NkCol::foreground);
        seg(w.configs, NkCol::mutedFg);
        seg(w.platforms, NkCol::mutedFg);
        y += lh + u.s(6);
        // ligne projets
        if (w.projects && *w.projects) {
            u.Text(tx, y, "Projets : ", NkCol::sidebarFg);
            u.Text(tx + u.TextW("Projets : "), y, w.projects, NkCol::mutedFg);
            y += lh + u.s(6);
        }
        // ligne statut de build + modif
        const NkColor stc = w.build == 1 ? NkCol::success : w.build == 2 ? NkCol::danger : w.build == 3 ? NkCol::accent : NkCol::mutedFg;
        const NkRect sb = { tx, y + u.s(1), u.s(14), u.s(14) };
        u.Rect(sb, w.build ? NkColor{ stc.r, stc.g, stc.b, 40 } : NkCol::muted, NkR::sm * u.S);
        if (w.build == 2) u.Icon("x", { sb.x + u.s(2), sb.y + u.s(2), u.s(10), u.s(10) }, stc);
        else { // coche
            u.dl->AddLine({ sb.x + u.s(3), sb.y + u.s(7) }, { sb.x + u.s(6), sb.y + u.s(10) }, stc, u.s(1.6f));
            u.dl->AddLine({ sb.x + u.s(6), sb.y + u.s(10) }, { sb.x + u.s(11), sb.y + u.s(4) }, stc, u.s(1.6f));
        }
        float32 sx = sb.x + u.s(20);
        if (w.buildConfig && *w.buildConfig) { u.Text(sx, y, w.buildConfig, NkCol::foreground); sx += u.TextW(w.buildConfig); }
        if (w.modified && *w.modified) { u.dl->AddRectFilled({ sx + u.s(6), y + lh * 0.5f - u.s(1.5f), u.s(3), u.s(3) }, NkCol::mutedFg, u.s(1.5f)); sx += u.s(15); u.Text(sx, y, w.modified, NkCol::mutedFg); }

        // ── etoile (favori) + menu (...) en haut a droite ──
        int32 act = 0;   // 1 charger, 2 (des)epingler, 3 menu/retirer
        const NkRect bStar = { r.x + r.w - u.s(58), r.y + u.s(12), u.s(20), u.s(20) };
        const NkRect bDots = { r.x + r.w - u.s(32), r.y + u.s(12), u.s(20), u.s(20) };
        if (u.Hit(bStar)) u.Rect(bStar, NkCol::muted, NkR::sm * u.S);
        NkDrawIcon(u, starIcon, bStar, w.pinned ? NkCol::accent : NkCol::mutedFg);
        if (u.Hit(bDots)) u.Rect(bDots, NkCol::muted, NkR::sm * u.S);
        for (int32 i = 0; i < 3; ++i) u.dl->AddRectFilled({ bDots.x + u.s(4) + i * u.s(5), bDots.y + u.s(9), u.s(2.5f), u.s(2.5f) }, NkCol::mutedFg, u.s(1.2f));
        if (u.Hit(bStar) && u.click) act = 2;
        else if (u.Hit(bDots) && u.click) act = 3;
        else if (hov && u.click) act = 1;
        return act;
    }

    // ── Action rapide (colonne droite) ──
    inline bool NkQuickAction(const NkUi& u, const NkRect& r, uint32 icon, const char* label,
                              const char* sub, const NkColor& borderCol, const NkColor& iconCol) {
        const bool hov = u.Hit(r);
        u.Panel(r, hov ? NkCol::hover : NkCol::surface, borderCol, NkR::lg * u.S);
        const float32 isz = u.s(18);
        NkDrawIcon(u, icon, { r.x + u.s(12), r.y + (r.h - isz) * 0.5f, isz, isz }, iconCol);
        u.Text(r.x + u.s(42), r.y + u.s(8), label, NkCol::foreground);
        u.Text(r.x + u.s(42), r.y + u.s(8) + u.Lh(), sub, NkCol::mutedFg);
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
        u.Panel(search, NkCol::input, NkCol::border, NkR::md * u.S);
        if (H->icons.search) NkDrawIcon(u, H->icons.search, { search.x + u.s(9), search.y + (fh - u.s(14)) * 0.5f, u.s(14), u.s(14) }, NkCol::mutedFg);
        else u.Icon("search", { search.x + u.s(9), search.y + (fh - u.s(13)) * 0.5f, u.s(13), u.s(13) }, NkCol::mutedFg);
        u.TextV(search.x + u.s(28), search.y, fh, "Filtrer les recents...", NkCol::mutedFg);
        auto chip = [&](float32 x, const char* t) -> NkRect {
            const float32 w = u.TextW(t) + u.s(34); const NkRect c = { x, left.y, w, fh };
            u.Panel(c, NkCol::input, NkCol::border, NkR::md * u.S);
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

        const float32 CH = u.s(104), CSTEP = u.s(112);
        // Workspace courant (carte "en cours") en tete s'il y en a un — proprietes reelles
        NkString curName, curPath, curProj;
        if (st && st->HasWorkspace()) {
            groupHeader("EN COURS", NkCol::accent, true);
            curName = st->root.GetFileName(); curPath = st->root.ToString();
            // liste des projets : 4 premiers + "+N"
            for (usize p = 0; p < st->projects.Size() && p < 4; ++p) { if (!curProj.Empty()) curProj += ", "; curProj += st->projects[p]; }
            if (st->projects.Size() > 4) { char e[16]; std::snprintf(e, sizeof(e), " +%d", (int)(st->projects.Size() - 4)); curProj += e; }
            NkWsInfo wi; wi.name = curName.CStr(); wi.path = curPath.CStr();
            wi.langVer = "C++20"; wi.configs = st->infoConfigs.CStr(); wi.platforms = st->infoOSes.CStr();
            wi.projects = curProj.CStr(); wi.buildConfig = st->ConfigName(); wi.build = 1;
            wi.iconBg = NkCol::primary; wi.icon = H->icons.workspace;
            const NkRect cr = { listArea.x, y, listArea.w, CH };
            if (NkWorkspaceCard(u, cr, wi, H->icons.star) == 1) loadCur = true;
            y += CSTEP;
        }
        // Epingles
        if (st && !st->pinned.Empty()) {
            groupHeader("EPINGLES", NkCol::accent, true);
            for (usize i = 0; i < st->pinned.Size(); ++i) {
                NkWsInfo wi; wi.name = (i < st->pinnedNames.Size() && !st->pinnedNames[i].Empty()) ? st->pinnedNames[i].CStr() : st->pinned[i].CStr();
                wi.path = st->pinned[i].CStr(); wi.pinned = true; wi.iconBg = cols[i % 4]; wi.icon = H->icons.workspace;
                const NkRect cr = { listArea.x, y, listArea.w, CH };
                const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
                if (a == 1) doLoad = (int32)(1000 + i); else if (a == 2) doPin = (int32)(1000 + i); else if (a == 3) doRem = (int32)(1000 + i);
                y += CSTEP;
            }
        }
        // Recents
        groupHeader("RECENTS", NkCol::mutedFg, true);
        if (!st || st->recents.Empty()) { u.Text(listArea.x + u.s(2), y, "(aucun workspace recent)", NkCol::mutedFg); y += u.s(24); }
        for (usize i = 0; st && i < st->recents.Size(); ++i) {
            NkWsInfo wi; wi.name = (i < st->recentNames.Size() && !st->recentNames[i].Empty()) ? st->recentNames[i].CStr() : st->recents[i].CStr();
            wi.path = st->recents[i].CStr(); wi.iconBg = cols[i % 4]; wi.icon = H->icons.workspace;
            const NkRect cr = { listArea.x, y, listArea.w, CH };
            const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
            if (a == 1) doLoad = (int32)i; else if (a == 2) doPin = (int32)i; else if (a == 3) doRem = (int32)i;
            y += CSTEP;
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
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, H->icons.nouveau, "Nouveau Workspace", "Creer un workspace Jenga", NkCol::primary, NkCol::primary)) dlg->Open(NkCodeDialogs::NewWorkspace);
        ry += qh + u.s(10);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, H->icons.ouvrir, "Ouvrir un Workspace", "Charger un .jenga existant", NkCol::border, NkCol::mutedFg)) dlg->OpenWorkspaceDialog();
        ry += qh + u.s(10);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, H->icons.ouvrirDossier, "Ouvrir un Dossier", "Explorer un dossier de projets", NkCol::border, NkCol::mutedFg)) dlg->OpenFolderDialog();
        ry += qh + u.s(10);
        if (NkQuickAction(u, { right.x, ry, right.w, qh }, H->icons.cloner, "Cloner depuis Git", "GitHub, GitLab, Bitbucket...", NkCol::secondary, NkCol::secondaryFg)) {/* TODO */}
        ry += qh + u.s(16);

        u.Rect({ right.x, ry - u.s(8), right.w, 1.f }, NkCol::border);
        u.Text(right.x, ry, "EXEMPLES JENGA", NkCol::mutedFg); ry += u.s(22);
        const char* ex[] = { "01_hello_console", "09_multi_projects", "25_opengl_triangle", "24_all_platforms" };
        const NkRect exBox = { right.x, ry, right.w, u.s(34) * 5.f };
        u.Panel(exBox, NkCol::surface, NkCol::border, NkR::lg * u.S);
        for (int32 i = 0; i < 4; ++i) {
            const float32 ey = ry + i * u.s(34);
            NkDrawIcon(u, H->icons.exemple, { right.x + u.s(10), ey + u.s(10), u.s(14), u.s(14) }, NkCol::accent);
            u.TextV(right.x + u.s(30), ey, u.s(34), ex[i], NkCol::foreground);
            if (i < 3) u.Rect({ right.x + u.s(8), ey + u.s(34), right.w - u.s(16), 1.f }, NkCol::border);
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
        const float32 top = u.ctx->titleBarH > 1.f ? u.ctx->titleBarH : u.ctx->ItemHeight();
        const float32 sbW = u.s(220);
        NkHomeSidebar(u, { 0.f, top, sbW, Ht - top }, H);
        NkHomePanel  (u, { sbW, top, W - sbW, Ht - top }, H);
    }

} // namespace nkcode
} // namespace nkentseu
