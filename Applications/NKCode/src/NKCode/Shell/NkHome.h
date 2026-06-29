#pragma once
// =============================================================================
// NkHome.h — Ecran d'accueil (Launcher Home), reecriture propre d'apres le
// design Banani « Launcher — Accueil ». Sidebar (marque + navigation + versions)
// + panneau (filtres, workspaces recents groupes, actions rapides, exemples).
// =============================================================================
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/Dialogs.h"   // reutilise la logique d'actions (ouvrir/creer)
#include "NKCode/Shell/NkOpenWs.h"  // vue « Ouvrir un Workspace » (navigateur de fichiers)
#include "NKCode/Shell/NkNewWorkspace.h"  // wizard « Nouveau Workspace »
#include <cstdio>

namespace nkentseu {
namespace nkcode {

    struct NkHomeState {
        NkCodeState*   st  = nullptr;
        NkCodeDialogs* dlg = nullptr;
        uint32  logoIcon = 0, logoWord = 0;   // textures (0 = repli dessine)
        int32   wordW = 0, wordH = 0;          // dimensions naturelles du wordmark (aspect)
        NkIcons icons;                          // icones SVG (data/textures/icon)
        int32   nav = 0;                       // item de nav actif (0 = Accueil, 1 = Ouvrir, 2 = Nouveau Workspace)
        NkOpenWsState ow;                      // etat de la vue « Ouvrir un Workspace »
        NkNewWsState  nw;                      // etat du wizard « Nouveau Workspace »
        float32 scroll  = 0.f;                 // centre : defilement vertical (recents)
        float32 scrollR = 0.f;                 // droite : defilement vertical
        float32 scrollMax  = 0.f;              // borne max du centre (frame precedente) -> anti-clignotement
        float32 scrollRMax = 0.f;              // borne max de droite (frame precedente)
        int32   barDrag = 0;                   // scrollbar en cours de drag (0 aucun)
        float32 barOff  = 0.f;                 // offset souris->thumb pendant le drag
        // Menu contextuel "..." d'une carte workspace.
        int32   ctxIdx  = -2;                  // -2 = ferme ; sinon index carte (2000 courant / 1000+i epingle / i recent)
        NkVec2  ctxPos{};                      // coin haut-gauche du menu
        NkString ctxPath;                      // chemin .jenga de la cible
        bool    ctxPinned = false;             // la cible est-elle deja epinglee ?
        bool    ctxIsCurrent = false;          // la cible est-elle le workspace ouvert ?
        // Filtres (barre du panneau) + recherche d'exemples.
        char    searchText[64] = {};           // filtre des recents (nom/chemin)
        int32   langFilter = 0;                // index dans NkLangFilters (0 = Tous)
        int32   sysFilter  = 0;                // index dans NkSysFilters (0 = Tous)
        int32   focusField = 0;                // 0 aucun, 1 recherche recents, 2 recherche exemples
        char    exSearch[64] = {};             // filtre des exemples
        // Combo box des chips (langage/systeme) : menu deroulant.
        int32   comboOpen = 0;                 // 0 ferme, 1 langage, 2 systeme
        NkVec2  comboPos{};                    // coin haut-gauche du deroulant
        float32 comboW = 0.f;                  // largeur du deroulant
        float32 caretBlink = 0.f;              // accumulateur (s) pour le clignotement du caret
        bool    groupCollapsed[5] = {};        // pliage : 0 epingles,1 aujourd'hui,2 semaine,3 mois,4 plus anciens
        NkString exePath;                      // chemin de l'executable NKCode (pour "nouvelle fenetre")
        // Popup de renommage dans les recents.
        bool     renameOpen = false;
        NkString renamePath;                   // chemin .jenga cible
        NkVec2   renamePos{};
        char     renameBuf[96] = {};
    };

    // Options des chips de filtre (0 = "Tous" -> pas de filtre).
    static const char* const NkLangFilters[] = { "Tous", "C++", "C", "Python", "Rust", "Zig" };
    static const char* const NkSysFilters[]  = { "Tous", "Windows", "Linux", "macOS", "Android", "Web", "iOS", "HarmonyOS", "Xbox" };
    // (Les exemples sont enumeres dynamiquement via `jenga examples list` -> st->examples.)

    // ── Scrollbar VERTICALE draggable au bord droit de `area`. `id` unique. ──
    inline void NkVScroll(const NkUi& u, const NkRect& area, float32 contentH, float32& scroll, int32 id, NkHomeState* H) {
        const float32 maxS = contentH > area.h ? contentH - area.h : 0.f;
        if (scroll < 0.f) scroll = 0.f; if (scroll > maxS) scroll = maxS;
        if (maxS <= 0.5f) { if (H->barDrag == id) H->barDrag = 0; return; }
        const float32 sw = u.s(10);
        const NkRect track = { area.x + area.w - sw, area.y, sw, area.h };
        u.dl->AddRectFilled(track, NkColor{ 18, 21, 26, 160 }, sw * 0.5f);
        float32 thh = area.h * (area.h / contentH); const float32 thmin = u.s(28);
        if (thh < thmin) thh = thmin;
        const float32 ty = area.y + (area.h - thh) * (scroll / maxS);
        const NkRect thumb = { track.x + u.s(2), ty, sw - u.s(4), thh };
        const bool hov = u.Hit(thumb);
        if (H->barDrag == 0 && hov && u.click) { H->barDrag = id; H->barOff = u.mp.y - ty; }
        if (H->barDrag == id) {
            if (!u.down) H->barDrag = 0;
            else { const float32 t = (u.mp.y - H->barOff - area.y) / (area.h - thh); scroll = t * maxS;
                   if (scroll < 0.f) scroll = 0.f; if (scroll > maxS) scroll = maxS; }
        }
        u.dl->AddRectFilled(thumb, (H->barDrag == id || hov) ? NkColor{ 96, 104, 114, 255 } : NkColor{ 56, 63, 72, 255 }, (sw - u.s(4)) * 0.5f);
    }

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
                // i == 0 Accueil, i == 1 Ouvrir, i == 2 Nouveau Workspace (wizard plein cadre)
                if (i == 3) {/* TODO clone git */}
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
        const char* projects = "";   // "Renderer, Physics, Audio, UI, Main"
        int32 projCount = 0;         // nombre total de projets -> "(N)"
        const char* buildConfig = "";// "Debug"
        const char* modified = "";   // "il y a 2h"
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
        const float32 nameRight = r.x + r.w - u.s(66);   // 1re ligne (nom) s'arrete avant etoile/...
        const float32 lineRight = r.x + r.w - u.s(16);
        // Separateur " · " centre verticalement sur la ligne courante (avance cx).
        auto dot = [&](float32& cx) {
            if (cx >= lineRight) return;
            u.dl->AddRectFilled({ cx + u.s(6), y + lh * 0.5f - u.s(1.5f), u.s(3), u.s(3) }, NkCol::mutedFg, u.s(1.5f)); cx += u.s(16);
        };
        // "Label: " (muted) + valeur (couleur), clippe a droite, avance cx.
        auto field = [&](float32& cx, const char* label, const char* val, const NkColor& vc) {
            if (cx >= lineRight || !val || !*val) return;
            u.Text(cx, y, label, NkCol::mutedFg); cx += u.TextW(label);
            cx += u.TextEllipsis(cx, y, lineRight - cx, val, vc);
        };

        // Ligne 1 : NOM
        u.TextEllipsis(tx, y, nameRight - tx, w.name, NkCol::foreground);
        y += lh + u.s(6);
        // Ligne 2 : CHEMIN complet (cliquable -> revele dans l'explorateur)
        bool pathHov = false;
        if (w.path && w.path[0]) {
            const float32 pavail = lineRight - tx;
            float32 pw = u.TextW(w.path); if (pw > pavail) pw = pavail;
            const NkRect pathRect = { tx, y, pw, lh + u.s(2) };
            pathHov = u.Hit(pathRect);
            u.TextEllipsis(tx, y, pavail, w.path, pathHov ? NkCol::primary : NkCol::mutedFg);
            if (pathHov) u.dl->AddRectFilled({ tx, y + lh, pw, u.s(1) }, NkCol::primary, 0.f);   // souligne
        }
        y += lh + u.s(6);
        // Ligne 3 : Langages: X  ·  Configs: Y
        { float32 cx = tx;
          field(cx, "Langages: ", (w.langVer && *w.langVer) ? w.langVer : "C++", NkCol::foreground);
          if (w.configs && *w.configs) { dot(cx); field(cx, "Configs: ", w.configs, NkCol::foreground); } }
        y += lh + u.s(5);
        // Ligne 4 : Plateformes: Z
        if (w.platforms && *w.platforms) { float32 cx = tx; field(cx, "Plateformes: ", w.platforms, NkCol::foreground); y += lh + u.s(5); }
        // Ligne 5 : Projets: A, B, C (N)
        if (w.projects && *w.projects) {
            float32 cx = tx; u.Text(cx, y, "Projets: ", NkCol::mutedFg); cx += u.TextW("Projets: ");
            char cnt[16]; cnt[0] = '\0'; if (w.projCount > 0) std::snprintf(cnt, sizeof(cnt), "  (%d)", w.projCount);
            const float32 cntw = cnt[0] ? u.TextW(cnt) : 0.f;
            cx += u.TextEllipsis(cx, y, lineRight - cx - cntw, w.projects, NkCol::foreground);
            if (cnt[0]) u.Text(cx, y, cnt, NkCol::mutedFg);
            y += lh + u.s(5);
        }
        // Ligne 6 : Dernier build: [statut] Cfg  ·  Modifie: ...
        { float32 cx = tx;
          u.Text(cx, y, "Dernier build: ", NkCol::mutedFg); cx += u.TextW("Dernier build: ");
          const NkColor stc = w.build == 1 ? NkCol::success : w.build == 2 ? NkCol::danger : w.build == 3 ? NkCol::accent : NkCol::mutedFg;
          const NkRect sb = { cx, y + u.s(1), u.s(14), u.s(14) };
          u.Rect(sb, w.build ? NkColor{ stc.r, stc.g, stc.b, 40 } : NkCol::muted, NkR::sm * u.S);
          if (w.build == 2) u.Icon("x", { sb.x + u.s(2), sb.y + u.s(2), u.s(10), u.s(10) }, stc);
          else { u.dl->AddLine({ sb.x + u.s(3), sb.y + u.s(7) }, { sb.x + u.s(6), sb.y + u.s(10) }, stc, u.s(1.6f));
                 u.dl->AddLine({ sb.x + u.s(6), sb.y + u.s(10) }, { sb.x + u.s(11), sb.y + u.s(4) }, stc, u.s(1.6f)); }
          cx = sb.x + u.s(20);
          if (w.buildConfig && *w.buildConfig) { u.Text(cx, y, w.buildConfig, NkCol::foreground); cx += u.TextW(w.buildConfig); }
          else { u.Text(cx, y, "inconnu", NkCol::mutedFg); cx += u.TextW("inconnu"); }
          if (w.modified && *w.modified) { dot(cx); field(cx, "Modifie: ", w.modified, NkCol::mutedFg); }
        }

        // ── etoile (favori) + menu (...) : BOUTONS a focus PRIORITAIRE (le clic agit
        //    sur le bouton, jamais sur la carte ; survol = fond + icone plus lumineuse) ──
        int32 act = 0;   // 1 charger, 2 (des)epingler, 3 menu/..., 4 reveler le chemin
        const NkRect bStar = { r.x + r.w - u.s(60), r.y + u.s(12), u.s(22), u.s(22) };
        const NkRect bDots = { r.x + r.w - u.s(34), r.y + u.s(12), u.s(22), u.s(22) };
        const bool hStar = u.Hit(bStar), hDots = u.Hit(bDots), overBtn = hStar || hDots;
        if (hStar) u.Rect(bStar, NkColor{ 48,54,61,255 }, NkR::sm * u.S);
        NkDrawIcon(u, starIcon, { bStar.x + u.s(2), bStar.y + u.s(2), u.s(18), u.s(18) },
                   w.pinned ? NkCol::accent : (hStar ? NkCol::foreground : NkCol::mutedFg));
        if (hDots) u.Rect(bDots, NkColor{ 48,54,61,255 }, NkR::sm * u.S);
        const NkColor dotsC = hDots ? NkCol::foreground : NkCol::mutedFg;
        for (int32 i = 0; i < 3; ++i) u.dl->AddRectFilled({ bDots.x + u.s(5) + i * u.s(5), bDots.y + u.s(10), u.s(2.5f), u.s(2.5f) }, dotsC, u.s(1.2f));
        if (hStar && u.click) act = 2;                          // priorite : etoile
        else if (hDots && u.click) act = 3;                     // priorite : menu (...)
        else if (pathHov && u.click) act = 4;                   // priorite : chemin -> reveler
        else if (hov && !overBtn && !pathHov && u.click) act = 1;  // carte : sinon charger
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

    // Ouvre un dossier dans l'explorateur de fichiers de l'OS.
    inline void NkHomeOpenFolder(const NkString& folder) {
    #ifdef _WIN32
        NkString bs; for (const char* p = folder.CStr(); *p; ++p) bs += (*p == '/') ? '\\' : *p;
        std::system((NkString("explorer \"") + bs + "\"").CStr());
    #elif defined(__APPLE__)
        std::system((NkString("open \"") + folder + "\"").CStr());
    #else
        std::system((NkString("xdg-open \"") + folder + "\"").CStr());
    #endif
    }

    // Ouvre un terminal dans le dossier.
    inline void NkHomeOpenTerminal(const NkString& folder) {
    #ifdef _WIN32
        NkString bs; for (const char* p = folder.CStr(); *p; ++p) bs += (*p == '/') ? '\\' : *p;
        std::system((NkString("start \"\" cmd /K cd /d \"") + bs + "\"").CStr());
    #elif defined(__APPLE__)
        std::system((NkString("open -a Terminal \"") + folder + "\"").CStr());
    #else
        std::system((NkString("(x-terminal-emulator --working-directory=\"") + folder + "\" || gnome-terminal --working-directory=\"" + folder + "\") &").CStr());
    #endif
    }
    // Lance une NOUVELLE fenetre NKCode sur ce dossier (re-exec de l'executable + arg dossier).
    inline void NkHomeOpenNewWindow(const NkString& exe, const NkString& folder) {
        if (exe.Empty()) return;
    #ifdef _WIN32
        std::system((NkString("start \"\" \"") + exe + "\" \"" + folder + "\"").CStr());
    #else
        std::system((NkString("\"") + exe + "\" \"" + folder + "\" &").CStr());
    #endif
    }

    // ── Menu contextuel "..." d'une carte : 8 actions + separateurs (cf. spec). Modal. ──
    inline void NkHomeCardMenu(const NkUi& u, NkHomeState* H, NkCodeState* st, NkCodeDialogs* dlg, bool justOpened) {
        if (H->ctxIdx == -2) return;
        struct Item { const char* label; int32 id; bool sep; };
        const NkString pinLbl = H->ctxPinned ? NkString("Detacher des favoris") : NkString("Epingler en favori");
        const Item items[] = {
            { "Ouvrir",                          1, false },
            { "Ouvrir dans une nouvelle fenetre",2, false },
            { "",                                0, true  },
            { "Ouvrir dans le terminal",         3, false },
            { "Reveler dans l'explorateur",      4, false },
            { "",                                0, true  },
            { pinLbl.CStr(),                     5, false },
            { "Renommer dans les recents",       6, false },
            { "",                                0, true  },
            { "Copier le chemin",                7, false },
            { "Supprimer des recents",           8, false },
        };
        const int32 N = (int32)(sizeof(items) / sizeof(items[0]));
        const float32 ih = u.s(26), sh = u.s(9), w = u.s(248);
        float32 h = u.s(8);
        for (int32 i = 0; i < N; ++i) h += items[i].sep ? sh : ih;
        NkRect box = { H->ctxPos.x, H->ctxPos.y, w, h };
        if (box.x + box.w > (float32)u.ctx->viewW) box.x = (float32)u.ctx->viewW - box.w - u.s(8);
        if (box.y + box.h > (float32)u.ctx->viewH) box.y = (float32)u.ctx->viewH - box.h - u.s(8);
        if (box.x < u.s(4)) box.x = u.s(4);
        if (box.y < u.s(4)) box.y = u.s(4);
        u.dl->AddRectFilled({ box.x + u.s(2), box.y + u.s(3), box.w, box.h }, NkColor{ 0,0,0,90 }, NkR::md * u.S);   // ombre
        u.Panel(box, NkCol::surface, NkColor{ 48,54,61,255 }, NkR::md * u.S);

        int32 clicked = 0; float32 iy = box.y + u.s(4);
        for (int32 i = 0; i < N; ++i) {
            if (items[i].sep) { u.dl->AddRectFilled({ box.x + u.s(8), iy + sh * 0.5f, w - u.s(16), 1.f }, NkCol::border, 0.f); iy += sh; continue; }
            const NkRect ir = { box.x + u.s(4), iy, w - u.s(8), ih };
            const bool hv = u.Hit(ir);
            if (hv) u.Rect(ir, NkCol::hover, NkR::sm * u.S);
            u.TextV(ir.x + u.s(12), ir.y, ih, items[i].label, items[i].id == 8 ? NkCol::danger : NkCol::foreground);
            if (hv && u.click) clicked = items[i].id;
            iy += ih;
        }
        if (justOpened) return;   // ignore le clic d'ouverture
        const bool outside = u.click && !u.Hit(box);
        if (!clicked && !outside) return;
        const NkString path = H->ctxPath; const bool isCur = H->ctxIsCurrent;
        const NkString folder = NkPath(path.CStr()).GetParent().ToString();
        u.ctx->input.mouseClicked[0] = false;   // consomme le clic (menu prioritaire)
        H->ctxIdx = -2;                          // ferme le menu
        switch (clicked) {
            case 1: if (isCur) { st->RequestReload(); dlg->showStart = false; } else dlg->DoLoad(NkPath(path.CStr()).GetParent()); break;
            case 2: NkHomeOpenNewWindow(H->exePath, folder); break;
            case 3: NkHomeOpenTerminal(folder); break;
            case 4: NkHomeOpenFolder(folder); break;
            case 5: if (st->IsPinned(path.CStr())) st->UnpinRecent(path); else st->PinRecent(path); break;
            case 6: {   // ouvre le popup de renommage (prefill = nom actuel)
                H->renameOpen = true; H->renamePath = path; H->renamePos = H->ctxPos;
                const char* cur = st->NameOverride(path.CStr());
                const NkString nm = cur ? NkString(cur) : NkCodeState::WorkspaceNameOf(path.CStr());
                int32 k = 0; for (; nm.CStr()[k] && k + 1 < (int32)sizeof(H->renameBuf); ++k) H->renameBuf[k] = nm.CStr()[k];
                H->renameBuf[k] = '\0';
                break;
            }
            case 7: u.ctx->SetClipboard(folder.CStr()); break;
            case 8: st->RemoveRecent(path); break;
        }
    }

    // ── Popup de renommage dans les recents (champ texte + OK/Annuler). Modal. ──
    inline void NkHomeRenamePopup(const NkUi& u, NkHomeState* H, NkCodeState* st, bool justOpened) {
        if (!H->renameOpen) return;
        const float32 w = u.s(290), h = u.s(104);
        NkRect box = { H->renamePos.x, H->renamePos.y, w, h };
        if (box.x + box.w > (float32)u.ctx->viewW) box.x = (float32)u.ctx->viewW - box.w - u.s(8);
        if (box.y + box.h > (float32)u.ctx->viewH) box.y = (float32)u.ctx->viewH - box.h - u.s(8);
        if (box.x < u.s(4)) box.x = u.s(4);
        if (box.y < u.s(4)) box.y = u.s(4);
        u.dl->AddRectFilled({ box.x + u.s(2), box.y + u.s(3), box.w, box.h }, NkColor{ 0,0,0,110 }, NkR::md * u.S);
        u.Panel(box, NkCol::surface, NkColor{ 48,54,61,255 }, NkR::md * u.S);
        u.Text(box.x + u.s(14), box.y + u.s(10), "Renommer dans les recents", NkCol::foreground);
        const NkRect field = { box.x + u.s(14), box.y + u.s(34), w - u.s(28), u.s(28) };
        if (!justOpened) {
            if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Backspace)) { int32 n = 0; while (H->renameBuf[n]) ++n; if (n > 0) H->renameBuf[n - 1] = '\0'; }
            for (int32 i = 0; i < u.ctx->input.charCount; ++i) {
                const uint32 cp = u.ctx->input.chars[i];
                if (cp >= 32 && cp < 127) { int32 n = 0; while (H->renameBuf[n]) ++n; if (n + 1 < (int32)sizeof(H->renameBuf)) { H->renameBuf[n] = (char)cp; H->renameBuf[n + 1] = '\0'; } }
            }
        }
        u.Panel(field, NkCol::input, NkCol::primary, NkR::md * u.S);
        u.Text(field.x + u.s(8), field.y + (field.h - u.Lh()) * 0.5f, H->renameBuf[0] ? H->renameBuf : "(nom personnalise)", H->renameBuf[0] ? NkCol::foreground : NkCol::mutedFg);
        u.dl->AddRectFilled({ field.x + u.s(8) + (H->renameBuf[0] ? u.TextW(H->renameBuf) : 0.f) + u.s(1), field.y + u.s(6), u.s(1.5f), field.h - u.s(12) }, NkCol::primary, 0.f);
        const NkRect okR = { box.x + w - u.s(160), box.y + h - u.s(32), u.s(72), u.s(24) };
        const NkRect caR = { box.x + w - u.s(82),  box.y + h - u.s(32), u.s(72), u.s(24) };
        const bool okClick = u.Button(okR, "OK",      NkCol::primary, NkCol::primary, NkCol::primaryFg, NkR::md * u.S);
        const bool caClick = u.Button(caR, "Annuler", NkCol::input,   NkCol::hover,   NkCol::foreground, NkR::md * u.S);
        if (justOpened) return;
        if (okClick)                       { st->SetRecentName(H->renamePath, NkString(H->renameBuf)); H->renameOpen = false; u.ctx->input.mouseClicked[0] = false; }
        else if (caClick)                  { H->renameOpen = false; u.ctx->input.mouseClicked[0] = false; }
        else if (u.click && !u.Hit(box))   { H->renameOpen = false; u.ctx->input.mouseClicked[0] = false; }
    }

    // ── Combo box d'un chip de filtre : liste deroulante des options. Modal (consomme le clic). ──
    inline void NkHomeCombo(const NkUi& u, NkHomeState* H, bool justOpened) {
        if (H->comboOpen == 0) return;
        const char* const* opts; int32 nopts; int32* target;
        if (H->comboOpen == 1) { opts = NkLangFilters; nopts = (int32)(sizeof(NkLangFilters) / sizeof(NkLangFilters[0])); target = &H->langFilter; }
        else                   { opts = NkSysFilters;  nopts = (int32)(sizeof(NkSysFilters)  / sizeof(NkSysFilters[0]));  target = &H->sysFilter; }
        const float32 ih = u.s(28), w = (H->comboW > u.s(80)) ? H->comboW : u.s(120), h = nopts * ih + u.s(8);
        NkRect box = { H->comboPos.x, H->comboPos.y, w, h };
        if (box.y + box.h > (float32)u.ctx->viewH) box.y = (float32)u.ctx->viewH - box.h - u.s(8);
        if (box.x + box.w > (float32)u.ctx->viewW) box.x = (float32)u.ctx->viewW - box.w - u.s(8);
        u.dl->AddRectFilled({ box.x + u.s(2), box.y + u.s(3), box.w, box.h }, NkColor{ 0,0,0,90 }, NkR::md * u.S);   // ombre
        u.Panel(box, NkCol::surface, NkColor{ 48,54,61,255 }, NkR::md * u.S);
        int32 picked = -1;
        for (int32 i = 0; i < nopts; ++i) {
            const NkRect ir = { box.x + u.s(4), box.y + u.s(4) + i * ih, w - u.s(8), ih };
            const bool hv = u.Hit(ir);
            if (hv) u.Rect(ir, NkCol::hover, NkR::sm * u.S);
            if (i == *target) u.dl->AddRectFilled({ ir.x + u.s(2), ir.y + u.s(6), u.s(3), ih - u.s(12) }, NkCol::primary, u.s(1.5f));
            u.TextV(ir.x + u.s(12), ir.y, ih, opts[i], (i == *target) ? NkCol::foreground : NkCol::sidebarFg);
            if (hv && u.click) picked = i;
        }
        if (justOpened) return;   // ignore le clic d'ouverture
        const bool outside = u.click && !u.Hit(box);
        if (picked >= 0 || outside) {
            u.ctx->input.mouseClicked[0] = false;   // consomme (priorite combo)
            if (picked >= 0) *target = picked;
            H->comboOpen = 0;
        }
    }

    // ── Champ de recherche : panneau + loupe + saisie (backspace/frappe) + placeholder.
    //    Renvoie true si clique (pour poser le focus). `focused` -> capte le clavier. ──
    inline bool NkHomeSearch(const NkUi& u, const NkRect& r, char* buf, int32 cap, bool focused,
                             uint32 icon, const char* placeholder, bool caretOn = true) {
        u.Panel(r, NkCol::input, focused ? NkCol::primary : NkCol::border, NkR::md * u.S);
        const float32 isz = u.s(14);
        if (icon) NkDrawIcon(u, icon, { r.x + u.s(9), r.y + (r.h - isz) * 0.5f, isz, isz }, NkCol::mutedFg);
        else      u.Icon("search", { r.x + u.s(9), r.y + (r.h - u.s(13)) * 0.5f, u.s(13), u.s(13) }, NkCol::mutedFg);
        if (focused) {
            if (u.ctx->input.KeyPressedRepeat(NkGuiKey::Backspace)) { int32 n = 0; while (buf[n]) ++n; if (n > 0) buf[n - 1] = '\0'; }
            for (int32 i = 0; i < u.ctx->input.charCount; ++i) {
                const uint32 cp = u.ctx->input.chars[i];
                if (cp >= 32 && cp < 127) { int32 n = 0; while (buf[n]) ++n; if (n + 1 < cap) { buf[n] = (char)cp; buf[n + 1] = '\0'; } }
            }
        }
        const float32 tx = r.x + u.s(28), ty = r.y + (r.h - u.Lh()) * 0.5f;
        const float32 availW = r.x + r.w - u.s(10) - tx;
        if (buf[0]) u.TextEllipsis(tx, ty, availW, buf, NkCol::foreground);
        else        u.Text(tx, ty, placeholder, NkCol::mutedFg);
        if (focused && caretOn) {   // caret clignotant (curseur de saisie)
            float32 cxp = tx + (buf[0] ? u.TextW(buf) : 0.f);
            if (cxp > tx + availW) cxp = tx + availW;
            u.dl->AddRectFilled({ cxp + u.s(1.f), r.y + u.s(7), u.s(1.5f), r.h - u.s(14) }, NkCol::primary, 0.f);
        }
        return u.Hit(r) && u.click;
    }

    // ── Panneau principal du Home ──
    inline void NkHomePanel(const NkUi& u, const NkRect& r, NkHomeState* H) {
        NkCodeState* st = H->st; NkCodeDialogs* dlg = H->dlg;
        u.Rect(r, NkCol::background);
        const float32 pad = u.s(24);
        const float32 rightW = u.s(272), gap = u.s(24);   // colonne droite un peu plus large (actions rapides)
        const NkRect left  = { r.x + pad, r.y + u.s(18), r.w - pad * 2.f - rightW - gap, r.h - u.s(36) };
        const NkRect right = { left.x + left.w + gap, r.y + u.s(18), rightW, r.h - u.s(36) };

        // Etat des popups (menu "..." ou combo de filtre) -> gele les clics des autres widgets.
        const bool menuOpen      = (H->ctxIdx != -2);
        const bool comboWasOpen  = (H->comboOpen != 0);
        const bool renameWasOpen = H->renameOpen;
        const bool anyPopup      = menuOpen || comboWasOpen || renameWasOpen;
        const bool caretOn      = (H->caretBlink - (float32)(int32)H->caretBlink) < 0.55f;   // clignotement ~1s

        // ── Barre de filtres : recherche + combo langage + combo systeme (fonctionnels) ──
        const float32 fh = u.s(30);
        const float32 cw = u.s(110);
        const NkRect searchR = { left.x, left.y, left.w - cw * 2.f - u.s(16), fh };
        { const bool clk = NkHomeSearch(u, searchR, H->searchText, (int32)sizeof(H->searchText), H->focusField == 1, H->icons.search, "Filtrer les recents...", caretOn);
          if (!anyPopup && clk) H->focusField = 1; }
        // Chip = bouton ouvrant un COMBO BOX (liste deroulante des options).
        auto chip = [&](float32 x, int32 which, int32 idx, const char* const* opts) {
            const NkRect c = { x, left.y, cw, fh };
            const bool active = (idx != 0);
            u.Panel(c, NkCol::input, (active || H->comboOpen == which) ? NkCol::primary : NkCol::border, NkR::md * u.S);
            u.TextV(c.x + u.s(10), c.y, fh, opts[idx], active ? NkCol::foreground : NkCol::mutedFg);
            u.Icon("chevron-down", { c.x + cw - u.s(18), c.y + (fh - u.s(12)) * 0.5f, u.s(12), u.s(12) }, NkCol::mutedFg);
            if (!anyPopup && u.Hit(c) && u.click) { H->comboOpen = which; H->comboPos = { c.x, c.y + fh + u.s(3) }; H->comboW = cw; H->focusField = 0; }
        };
        chip(left.x + left.w - cw * 2.f - u.s(8), 1, H->langFilter, NkLangFilters);
        chip(left.x + left.w - cw,               2, H->sysFilter,  NkSysFilters);

        // Filtre commun (recherche nom/chemin + langage + systeme).
        auto passFilter = [&](const char* name, const char* path, const char* lang, const char* plats) -> bool {
            if (H->searchText[0] && !NkCodeState::ContainsI(name, H->searchText) && !NkCodeState::ContainsI(path, H->searchText)) return false;
            if (H->langFilter != 0 && !NkCodeState::ContainsI(lang,  NkLangFilters[H->langFilter])) return false;
            if (H->sysFilter  != 0 && !NkCodeState::ContainsI(plats, NkSysFilters[H->sysFilter]))  return false;
            return true;
        };

        // ── Liste des recents (defilable) ──
        const NkRect listArea = { left.x, left.y + fh + u.s(14), left.w, left.h - fh - u.s(14) };
        if (u.Hit(listArea) && u.ctx->input.wheel != 0.f) {
            H->scroll -= u.ctx->input.wheel * u.s(34); u.ctx->input.wheel = 0.f;
            if (H->scroll < 0.f) H->scroll = 0.f; if (H->scroll > H->scrollMax) H->scroll = H->scrollMax;  // borne AVANT le dessin (anti-clignotement)
        }
        u.dl->PushClipRect(listArea, true);
        float32 y = listArea.y - H->scroll;
        int32 doLoad = -1, doPin = -1, doRem = -1; bool loadCur = false;
        NkString revealPath;  // chemin a reveler dans l'explorateur (clic sur le chemin d'une carte)
        NkString curWsPath;   // chemin .jenga du workspace courant (pour favori/... sur sa carte)
        if (st && st->HasWorkspace() && st->wsIdx >= 0 && st->wsIdx < (int32)st->wsPaths.Size())
            curWsPath = st->wsPaths[st->wsIdx];

        // En-tete de groupe PLIABLE (clic sur la ligne -> replie/deplie). Renvoie l'etat ouvert.
        auto groupHeader = [&](int32 gid, const char* label, const NkColor& col) -> bool {
            const NkRect hr = { listArea.x, y - u.s(2), listArea.w, u.s(20) };
            if (!anyPopup && u.Hit(hr) && u.click) H->groupCollapsed[gid] = !H->groupCollapsed[gid];
            const bool open = !H->groupCollapsed[gid];
            u.Icon(open ? "chevron-down" : "chevron-right", { listArea.x, y + u.s(1), u.s(12), u.s(12) }, NkCol::mutedFg);
            u.Text(listArea.x + u.s(18), y, label, col);
            y += u.s(20);
            return open;
        };
        // Ouvre le menu contextuel d'une carte, positionne sous son bouton "...".
        auto openMenu = [&](int32 index, const NkRect& cr, const NkString& path, bool pinned, bool isCur) {
            H->ctxIdx = index; H->ctxPath = path; H->ctxPinned = pinned; H->ctxIsCurrent = isCur;
            H->ctxPos = { cr.x + cr.w - u.s(190), cr.y + u.s(36) };
        };
        const NkColor cols[] = { NkCol::primary, NkCol::accent, NkCol::secondary, NkColor{ 51,177,160,255 } };

        const float32 cardLh = u.Lh();                                  // 6 lignes libellees -> carte plus haute
        const float32 CH = u.s(24) + 5.f * (cardLh + u.s(5)) + cardLh;  // nom+chemin+langs+plats+projets+build
        const float32 CSTEP = CH + u.s(12);
        const int64 now = NkCodeState::NowEpoch();
        NkString curName, curPath, curProj;   // gardes en vie pour la carte du workspace courant

        // Carte d'un workspace epingle (factorisee : utilisee pour le pre-check et le dessin).
        auto pinnedPasses = [&](usize i) -> bool {
            if (StrEq(st->pinned[i].CStr(), curWsPath.CStr())) return false;   // doublon avec le courant
            const auto meta = st->WorkspaceMeta(st->pinned[i].CStr());
            const char* nm = (i < st->pinnedNames.Size() && !st->pinnedNames[i].Empty()) ? st->pinnedNames[i].CStr() : st->pinned[i].CStr();
            return passFilter(nm, st->pinned[i].CStr(), meta.langVer.CStr(), meta.platforms.CStr());
        };

        // ── ÉPINGLÉS (favoris, pliable) — SAUF le workspace courant (montre dans son bucket) ──
        if (st && !st->pinned.Empty()) {
            bool hasAny = false;
            for (usize i = 0; i < st->pinned.Size() && !hasAny; ++i) hasAny = pinnedPasses(i);
            if (hasAny) {
                const bool open = groupHeader(0, "EPINGLES", NkCol::accent);
                for (usize i = 0; open && i < st->pinned.Size(); ++i) {
                    if (!pinnedPasses(i)) continue;
                    const auto meta = st->WorkspaceMeta(st->pinned[i].CStr());
                    const char* nm = (i < st->pinnedNames.Size() && !st->pinnedNames[i].Empty()) ? st->pinnedNames[i].CStr() : st->pinned[i].CStr();
                    const NkString mod = NkCodeState::HumanAge(meta.activity, now);
                    NkWsInfo wi; wi.name = nm;
                    wi.path = st->pinned[i].CStr(); wi.pinned = true; wi.iconBg = cols[i % 4]; wi.icon = H->icons.workspace;
                    wi.modified = mod.CStr();
                    wi.langVer = meta.langVer.CStr(); wi.configs = meta.configs.CStr(); wi.platforms = meta.platforms.CStr(); wi.projects = meta.projects.CStr(); wi.projCount = meta.projCount;
                    const NkRect cr = { listArea.x, y, listArea.w, CH };
                    const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
                    if (!anyPopup) {
                        if (a == 1) doLoad = (int32)(1000 + i);
                        else if (a == 2) doPin = (int32)(1000 + i);
                        else if (a == 3) openMenu((int32)(1000 + i), cr, st->pinned[i], true, false);
                        else if (a == 4) revealPath = NkPath(st->pinned[i].CStr()).GetParent().ToString();   // reveler
                    }
                    y += CSTEP;
                }
            }
        }

        // ── RÉCENTS groupes PAR DATE (AUJOURD'HUI / CETTE SEMAINE / PLUS ANCIEN).
        //    Le workspace COURANT est fondu dans ces groupes (idx -1), trie par mtime. ──
        {
            struct Row { int32 idx; int64 mtime; int32 bucket; };   // idx -1 = courant
            NkVector<Row> rows;
            if (!curWsPath.Empty()) {                                // le courant : montre (riche) dans son bucket, si filtre OK
                const NkString cn = st->root.GetFileName(), cp = st->root.ToString();
                if (passFilter(cn.CStr(), cp.CStr(), "C++20", st->infoOSes.CStr())) {
                    const int64 mt = st->WorkspaceMeta(curWsPath.CStr()).activity;
                    rows.PushBack({ -1, mt, NkCodeState::AgeBucket(mt, now) });
                }
            }
            for (usize i = 0; st && i < st->recents.Size(); ++i) {
                if (!curWsPath.Empty() && StrEq(curWsPath.CStr(), st->recents[i].CStr())) continue;   // pas de doublon avec le courant
                const auto meta = st->WorkspaceMeta(st->recents[i].CStr());
                const char* nm = (i < st->recentNames.Size() && !st->recentNames[i].Empty()) ? st->recentNames[i].CStr() : st->recents[i].CStr();
                if (!passFilter(nm, st->recents[i].CStr(), meta.langVer.CStr(), meta.platforms.CStr())) continue;   // filtres actifs
                rows.PushBack({ (int32)i, meta.activity, NkCodeState::AgeBucket(meta.activity, now) });
            }
            for (usize a = 0; a < rows.Size(); ++a)                  // tri mtime decroissant (peu d'elements)
                for (usize b = a + 1; b < rows.Size(); ++b)
                    if (rows[b].mtime > rows[a].mtime) { Row t = rows[a]; rows[a] = rows[b]; rows[b] = t; }

            if (rows.Empty()) {
                u.Icon("chevron-down", { listArea.x, y + u.s(1), u.s(12), u.s(12) }, NkCol::mutedFg);
                u.Text(listArea.x + u.s(18), y, "RECENTS", NkCol::mutedFg); y += u.s(20);
                u.Text(listArea.x + u.s(2), y, "(aucun workspace recent)", NkCol::mutedFg); y += u.s(24);
            }
            int32 lastBucket = -1; bool bucketOpen = true;
            for (usize r = 0; r < rows.Size(); ++r) {
                if (rows[r].bucket != lastBucket) {                  // nouvel en-tete de bucket (pliable)
                    lastBucket  = rows[r].bucket;
                    bucketOpen  = groupHeader(1 + lastBucket, NkCodeState::BucketLabel(lastBucket), NkCol::mutedFg);
                }
                if (!bucketOpen) continue;                          // groupe plie -> cartes masquees
                const int32 idx = rows[r].idx;
                const NkString mod = NkCodeState::HumanAge(rows[r].mtime, now);
                NkWsInfo wi; wi.modified = mod.CStr();
                const NkRect cr = { listArea.x, y, listArea.w, CH };
                if (idx < 0) {                                      // workspace COURANT (meta complete)
                    curName = st->root.GetFileName(); curPath = st->root.ToString(); curProj.Clear();
                    for (usize p = 0; p < st->projects.Size() && p < 6; ++p) { if (!curProj.Empty()) curProj += ", "; curProj += st->projects[p]; }
                    wi.name = curName.CStr(); wi.path = curPath.CStr();
                    wi.langVer = "C++20"; wi.configs = st->infoConfigs.CStr(); wi.platforms = st->infoOSes.CStr();
                    wi.projects = curProj.CStr(); wi.projCount = (int32)st->projects.Size(); wi.buildConfig = st->ConfigName(); wi.build = 1;
                    wi.iconBg = NkCol::primary; wi.icon = H->icons.workspace;
                    wi.pinned = st->IsPinned(curWsPath.CStr());            // etoile reflete l'etat epingle
                    const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
                    if (!anyPopup) {
                        if (a == 1) loadCur = true;
                        else if (a == 2) doPin = 2000;                     // 2000 = courant
                        else if (a == 3) openMenu(2000, cr, curWsPath, wi.pinned, true);
                        else if (a == 4) revealPath = st->root.ToString();
                    }
                } else {                                            // recent : meta parsee du .jenga
                    const auto meta = st->WorkspaceMeta(st->recents[idx].CStr());
                    wi.name = ((usize)idx < st->recentNames.Size() && !st->recentNames[idx].Empty()) ? st->recentNames[idx].CStr() : st->recents[idx].CStr();
                    wi.path = st->recents[idx].CStr(); wi.iconBg = cols[idx % 4]; wi.icon = H->icons.workspace;
                    wi.langVer = meta.langVer.CStr(); wi.configs = meta.configs.CStr(); wi.platforms = meta.platforms.CStr(); wi.projects = meta.projects.CStr(); wi.projCount = meta.projCount;
                    const int32 a = NkWorkspaceCard(u, cr, wi, H->icons.star);
                    if (!anyPopup) {
                        if (a == 1) doLoad = idx;
                        else if (a == 2) doPin = idx;
                        else if (a == 3) openMenu(idx, cr, st->recents[idx], false, false);
                        else if (a == 4) revealPath = NkPath(st->recents[idx].CStr()).GetParent().ToString();
                    }
                }
                y += CSTEP;
            }
        }
        const float32 contentH = (y + H->scroll) - listArea.y;
        H->scrollMax = contentH > listArea.h ? contentH - listArea.h : 0.f;   // memorise pour borner la frame suivante
        u.dl->PopClipRect();
        NkVScroll(u, listArea, contentH, H->scroll, 1, H);   // scrollbar verticale du centre

        // applique les actions differees (apres le dessin)
        if (!revealPath.Empty()) NkHomeOpenFolder(revealPath);   // chemin clique -> revele dans l'explorateur
        if (loadCur) { if (dlg) { if (H->st) H->st->RequestReload(); } dlg->showStart = false; return; }
        auto pathOf = [&](int32 idx) -> NkString { return idx >= 2000 ? curWsPath : idx >= 1000 ? st->pinned[idx - 1000] : st->recents[idx]; };
        if (doLoad >= 0) { dlg->DoLoad(NkPath(pathOf(doLoad).CStr()).GetParent()); return; }
        if (doPin  >= 0) {   // (des)epingle : courant (2000) ou epingle (>=1000) -> retire ; recent -> epingle
            const NkString p = pathOf(doPin);
            if (doPin >= 2000) { if (st->IsPinned(p.CStr())) st->UnpinRecent(p); else st->PinRecent(p); }
            else if (doPin >= 1000) st->UnpinRecent(p);
            else st->PinRecent(p);
            return;
        }
        if (doRem  >= 0) { st->RemoveRecent(pathOf(doRem)); return; }

        // ── Colonne droite : HAUT FIXE (actions rapides + en-tete/recherche exemples)
        //    puis BAS DEFILABLE = UNIQUEMENT la liste des exemples. ──
        const float32 rcw = right.w;             // zone fixe : pas de scrollbar
        float32 ry = right.y + u.s(2);
        u.Text(right.x, ry, "ACTIONS RAPIDES", NkCol::mutedFg); ry += u.s(22);
        const float32 qh = u.s(52);
        { const bool c = NkQuickAction(u, { right.x, ry, rcw, qh }, H->icons.nouveau, "Nouveau Workspace", "Creer un workspace Jenga", NkCol::primary, NkCol::primary); if (!anyPopup && c) H->nav = 2; }
        ry += qh + u.s(10);
        { const bool c = NkQuickAction(u, { right.x, ry, rcw, qh }, H->icons.ouvrir, "Ouvrir un Workspace", "Parcourir un dossier .jenga", NkCol::border, NkCol::mutedFg); if (!anyPopup && c) H->nav = 1; }
        ry += qh + u.s(10);
        { const bool c = NkQuickAction(u, { right.x, ry, rcw, qh }, H->icons.ouvrirDossier, "Ouvrir un Dossier", "Explorer un dossier de projets", NkCol::border, NkCol::mutedFg); if (!anyPopup && c) H->nav = 1; }
        ry += qh + u.s(10);
        (void)NkQuickAction(u, { right.x, ry, rcw, qh }, H->icons.cloner, "Cloner depuis Git", "GitHub, GitLab, Bitbucket...", NkCol::secondary, NkCol::secondaryFg);  // TODO clone
        ry += qh + u.s(18);

        u.Rect({ right.x, ry - u.s(8), rcw, 1.f }, NkCol::border);
        // En-tete + compteur (exemples enumeres via `jenga examples list`).
        { char hdr[48]; std::snprintf(hdr, sizeof(hdr), "EXEMPLES JENGA (%d)", st ? (int)st->examples.Size() : 0);
          u.Text(right.x, ry, hdr, NkCol::mutedFg); } ry += u.s(20);
        // Recherche d'exemple (champ fixe, hors scroll).
        const NkRect exSearchR = { right.x, ry, rcw, u.s(28) };
        { const bool clk = NkHomeSearch(u, exSearchR, H->exSearch, (int32)sizeof(H->exSearch), H->focusField == 2, H->icons.search, "Rechercher un exemple...", caretOn);
          if (!anyPopup && clk) H->focusField = 2; }
        ry += u.s(28) + u.s(10);

        // ── Zone DEFILABLE : la LISTE des exemples uniquement ──
        const NkRect exList = { right.x, ry, right.w, right.y + right.h - ry };
        if (!anyPopup && u.Hit(exList) && u.ctx->input.wheel != 0.f) {
            H->scrollR -= u.ctx->input.wheel * u.s(34); u.ctx->input.wheel = 0.f;
            if (H->scrollR < 0.f) H->scrollR = 0.f; if (H->scrollR > H->scrollRMax) H->scrollR = H->scrollRMax;
        }
        u.dl->PushClipRect(exList, true);
        const float32 elw = exList.w - u.s(14);   // place pour la scrollbar
        float32 ey = exList.y - H->scrollR;
        int32 shown = 0;
        for (usize i = 0; st && i < st->examples.Size(); ++i) {
            const auto& e = st->examples[i];
            if (H->exSearch[0] && !NkCodeState::ContainsI(e.id.CStr(), H->exSearch)
                               && !NkCodeState::ContainsI(e.desc.CStr(), H->exSearch)
                               && !NkCodeState::ContainsI(e.platforms.CStr(), H->exSearch)) continue;
            const NkRect er = { exList.x, ey, elw, u.s(40) };
            if (!anyPopup && u.Hit(er)) u.Rect(er, NkCol::hover, NkR::sm * u.S);
            NkDrawIcon(u, H->icons.exemple, { exList.x + u.s(8), ey + u.s(7), u.s(14), u.s(14) }, NkCol::accent);
            u.TextEllipsis(exList.x + u.s(28), ey + u.s(4), elw - u.s(34), e.id.CStr(), NkCol::foreground);
            if (e.platforms.CStr()[0]) u.TextEllipsis(exList.x + u.s(28), ey + u.s(4) + u.Lh(), elw - u.s(34), e.platforms.CStr(), NkCol::mutedFg);
            ey += u.s(42); ++shown;
        }
        if (st && st->examples.Empty()) { u.Text(exList.x, ey, "(chargement des exemples...)", NkCol::mutedFg); ey += u.s(20); }
        else if (shown == 0)            { u.Text(exList.x, ey, "(aucun exemple trouve)", NkCol::mutedFg); ey += u.s(20); }

        const float32 elContentH = (ey + H->scrollR) - exList.y;
        H->scrollRMax = elContentH > exList.h ? elContentH - exList.h : 0.f;   // memorise pour borner la frame suivante
        u.dl->PopClipRect();
        NkVScroll(u, exList, elContentH, H->scrollR, 2, H);   // scrollbar de la LISTE d'exemples

        NkHomeCardMenu(u, H, st, dlg, /*justOpened=*/!menuOpen);    // menu "..." dessine EN DERNIER (au-dessus de tout)
        NkHomeCombo(u, H, /*justOpened=*/!comboWasOpen);           // combo box des filtres (au-dessus de tout)
        NkHomeRenamePopup(u, H, st, /*justOpened=*/!renameWasOpen); // popup de renommage (au-dessus de tout)
    }

    // ── Point d'entree : ecran d'accueil PLEIN CADRE (via SetStartScreen) ──
    inline void DrawHome(NkEditorFrameContext& ec, NkHomeState* H) {
        if (!H || !H->dlg || !H->dlg->showStart) return;
        H->caretBlink += ec.dt;                          // avance le clignotement du caret
        if (H->caretBlink > 1.0e6f) H->caretBlink = 0.f; // anti-overflow lointain
        const NkUi u = NkUi::From(ec);
        if (!u.Valid()) return;
        // pompe jenga info (toolchains/projets) comme avant
        if (H->st) { H->st->ScanWorkspaces(); H->st->LoadProjects(); H->st->PollProjects(); H->st->PollConfig(); H->st->LoadExamples(); H->st->PollExamples(); }
        const float32 W = (float32)u.ctx->viewW, Ht = (float32)u.ctx->viewH;
        const float32 top = u.ctx->titleBarH > 1.f ? u.ctx->titleBarH : u.ctx->ItemHeight();
        const float32 sbW = u.s(220);
        NkHomeSidebar(u, { 0.f, top, sbW, Ht - top }, H);
        const NkRect panel = { sbW, top, W - sbW, Ht - top };
        if (H->nav == 1) {                                   // vue « Ouvrir un Workspace »
            if (NkOpenWsPanel(u, panel, &H->ow, H->st, H->dlg, ec.dt, H->icons) == 1) H->nav = 0;   // Annuler -> retour Accueil
        } else if (H->nav == 2) {                            // wizard « Nouveau Workspace »
            if (NkNewWsPanel(u, panel, &H->nw, H->st, H->dlg, ec.dt, H->icons) == 1) H->nav = 0;     // Annuler -> retour Accueil
        } else {
            NkHomePanel(u, panel, H);                        // Accueil (recents + actions + exemples)
        }
    }

} // namespace nkcode
} // namespace nkentseu
