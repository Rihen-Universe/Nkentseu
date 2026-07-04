#pragma once
// =============================================================================
// Toolbar.h — Barre d'outils de la VUE PRINCIPALE (dessin custom NkUi + vraies
// icônes, fidèle à la maquette Banani) :
//   [Solution ▸] › [Projet] | [Plateforme][Config][Archi] | [Tests] |
//   Build Rebuild Nettoyer | ▶Exécuter Déboguer Tests(n) | 🔍 Recherche rapide
// 100 % piloté par la structure Jenga réelle (`jenga info`). Combos fonctionnels
// (dropdown différé). Les actions appellent DoBuildAction/DoClean/DoRun/DoTest.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Project/NkCodeState.h"
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"   // NkOwIco
#include "NKCode/Shell/NkI18n.h"     // NkT

namespace nkentseu {
namespace nkcode {

    using namespace nkentseu;
    using namespace nkentseu::editorkit;
    using namespace nkentseu::nkgui;

    // État local (un seul combo ouvert à la fois).
    struct NkTbState { int32 open = -1; NkRect openR{}; bool justOpened = false; };
    inline NkTbState& NkTb() { static NkTbState s; return s; }

    inline void DrawCodeToolbar(NkEditorFrameContext& ec, NkCodeState* s) {
        if (!s) return;
        s->ScanWorkspaces(); s->TickWatch(ec.dt); s->LoadProjects(); s->PollProjects();

        const NkUi u = NkUi::From(ec);
        const NkRect r = ec.Ui().layout.region;   // bornes de la toolbar (46px)
        NkTbState& tb = NkTb();
        const NkIcons* ic = s->icons;
        auto TEX = [&](uint32 v) -> uint32 { return ic ? v : 0u; };

        u.Rect(r, NkCol::sidebar);                                   // fond sidebar (maquette)
        u.Rect({ r.x, r.y + r.h - 1.f, r.w, 1.f }, NkCol::border);

        if (!s->HasWorkspace()) {
            u.Text(r.x + u.s(14), r.y + (r.h - u.Lh()) * 0.5f, NkT("tb.noworkspace"), NkCol::mutedFg);
            return;
        }

        // Hauteur UNIQUE pour combos / champ recherche / boutons (= hauteur des boutons Build),
        // centrée verticalement dans la toolbar (dont la hauteur globale ne change pas). Plus de libellés au-dessus.
        const float32 ctrlH = u.s(26);
        const float32 ctrlY = r.y + (r.h - ctrlH) * 0.5f;
        const float32 cyBtn = ctrlY + ctrlH * 0.5f;

        // ── Combo (sans libellé) : toggle l'ouverture ──
        auto combo = [&](float32 x, const char* label, const char* value, uint32 tex, const char* drawn, bool accent, int32 id, float32 w) -> void {
            (void)label;                                       // maquette simplifiée : plus de libellé au-dessus
            const NkRect box = { x, ctrlY, w, ctrlH };
            const bool open = (tb.open == id);
            u.Panel(box, accent ? NkCol::secondary : NkCol::muted, (open || accent) ? NkCol::primary : NkCol::border, NkR::sm * u.S);
            float32 tx = x + u.s(9);
            if (tex || drawn) { NkOwIco(u, tex, drawn, { tx, ctrlY + (ctrlH - u.s(12)) * 0.5f, u.s(12), u.s(12) }, accent ? NkCol::primary : NkCol::mutedFg); tx += u.s(18); }
            u.TextEllipsis(tx, ctrlY + (ctrlH - u.Lh()) * 0.5f, (x + w) - tx - u.s(16), value, accent ? NkCol::primary : NkCol::foreground);
            NkOwIco(u, 0u, "chevron-down", { x + w - u.s(15), ctrlY + (ctrlH - u.s(9)) * 0.5f, u.s(9), u.s(9) }, NkCol::mutedFg);
            if (u.Hit(box) && u.click) { if (open) tb.open = -1; else { tb.open = id; tb.openR = box; tb.justOpened = true; } }
        };
        // ── Séparateur vertical ──
        auto vsep = [&](float32 x) { u.Rect({ x, ctrlY, 1.f, ctrlH }, NkCol::border); };
        // ── Bouton d'action (icône + label + variante + badge) ──
        auto btn = [&](float32 x, const char* label, uint32 tex, const char* drawn, int32 variant, const char* badge) -> float32 {
            const float32 tw = u.TextW(label);
            const float32 w = u.s(12) + u.s(6) + tw + u.s(20);
            const NkRect b = { x, cyBtn - u.s(13), w, u.s(26) };
            const bool hv = u.Hit(b);
            NkColor bg = NkCol::muted, brd = NkCol::border, fg = NkCol::foreground;
            if (variant == 1) { bg = NkCol::primary; brd = NkCol::primary; fg = NkCol::primaryFg; }
            else if (variant == 2) { bg = NkCol::success; brd = NkCol::success; fg = NkCol::primaryFg; }
            if (hv) bg = (variant == 0) ? NkCol::hover : NkColor{ (uint8)(bg.r > 20 ? bg.r - 18 : bg.r), (uint8)(bg.g > 20 ? bg.g - 18 : bg.g), (uint8)(bg.b > 20 ? bg.b - 18 : bg.b), 255 };
            u.Panel(b, bg, brd, NkR::sm * u.S);
            NkOwIco(u, tex, drawn, { b.x + u.s(10), b.y + u.s(7), u.s(12), u.s(12) }, fg);
            u.Text(b.x + u.s(10) + u.s(12) + u.s(6), b.y + (b.h - u.Lh()) * 0.5f, label, fg);
            if (badge && badge[0]) {   // pastille en haut à droite
                const float32 bw = u.TextW(badge) + u.s(6);
                const NkRect pr = { b.x + w - bw * 0.5f - u.s(2), b.y - u.s(4), bw, u.s(13) };
                u.dl->AddRectFilled(pr, NkCol::primary, pr.h * 0.5f);
                u.Text(pr.x + u.s(3), pr.y - u.s(1), badge, NkCol::primaryFg);
            }
            return w;
        };

        // ── Données réelles ──
        int32 nSys = 0; const NkCodeState::SysDef* sysd = NkCodeState::Systems(&nSys);
        if (s->sysIdx < 0 || s->sysIdx >= nSys) s->sysIdx = 0;
        const NkCodeState::SysDef& SY = sysd[s->sysIdx];
        if (s->archIdx < 0 || s->archIdx > SY.nArch) s->archIdx = 0;
        static const char* kCfg[] = { "Debug", "Release", "Toutes" };
        const int32 cfgI = (s->cfgIdx >= 0 && s->cfgIdx < 3) ? s->cfgIdx : 0;
        const char* wsPrev   = (s->wsIdx >= 0 && s->wsIdx < (int32)s->wsNames.Size()) ? s->wsNames[s->wsIdx].CStr() : "(workspace)";
        const char* projPrev = s->projects.Empty() ? "…" : (s->AllProjects() ? NkT("tb.allprojects") : s->SelectedProject());
        const char* archPrev = (s->archIdx >= SY.nArch) ? NkT("tb.all") : SY.archs[s->archIdx];
        int32 nTestVis = 0; for (int32 i = 0; i < (int32)s->tests.Size(); ++i) if (s->TestVisible(i)) ++nTestVis;
        const bool hasTests = !s->tests.Empty();
        if (s->testIdx >= 0 && !s->TestVisible(s->testIdx)) s->testIdx = -1;
        const char* testPrev = (s->testIdx >= 0 && s->TestVisible(s->testIdx)) ? s->tests[s->testIdx].CStr() : NkT("tb.alltests");

        // ── Largeurs & centrage ──
        const float32 wSol = u.s(140), wProj = u.s(120), wPlat = u.s(104), wCfg = u.s(92), wArch = u.s(92), wTest = u.s(140);
        const float32 gap = u.s(6), sep = u.s(10);
        auto btnW = [&](const char* l) { return u.s(12) + u.s(6) + u.TextW(l) + u.s(20); };
        float32 total = wSol + u.s(14) + wProj + sep + wPlat + gap + wCfg + gap + wArch + sep + (hasTests ? wTest + sep : 0.f)
                      + btnW(NkT("tb.build")) + gap + btnW(NkT("tb.rebuild")) + gap + btnW(NkT("tb.clean")) + sep
                      + btnW(NkT("tb.run")) + gap + btnW(NkT("tb.debug")) + gap + btnW(NkT("tb.test")) + sep + u.s(150);
        float32 x = r.x + (r.w - total) * 0.5f; if (x < r.x + u.s(8)) x = r.x + u.s(8);

        combo(x, NkT("tb.solution"), wsPrev, TEX(ic ? ic->jenga : 0), "git-branch", true, 0, wSol); x += wSol + u.s(6);
        u.Text(x, ctrlY + (ctrlH - u.Lh()) * 0.5f, "\xE2\x80\xBA", NkCol::border); x += u.s(8);
        combo(x, NkT("tb.projet"), projPrev, TEX(ic ? ic->pkg : 0), "package", false, 1, wProj); x += wProj + sep; vsep(x - sep * 0.5f);
        combo(x, NkT("tb.plateforme"), SY.name, TEX(ic ? ic->monitor : 0), "monitor", false, 2, wPlat); x += wPlat + gap;
        combo(x, NkT("tb.config"), kCfg[cfgI], TEX(ic ? ic->kConfig : 0), "settings", false, 3, wCfg); x += wCfg + gap;
        combo(x, NkT("tb.archi"), archPrev, TEX(ic ? ic->platforms : 0), "cpu", false, 4, wArch); x += wArch + sep; vsep(x - sep * 0.5f);
        if (hasTests) { combo(x, NkT("tb.tests"), testPrev, TEX(ic ? ic->kTest : 0), "flask", false, 5, wTest); x += wTest + sep; vsep(x - sep * 0.5f); }

        // Groupe Build
        float32 xB = x; float32 wB = btn(x, NkT("tb.build"), TEX(ic ? ic->hammer : 0), "hammer", 0, nullptr);
        const NkRect bBuild = { xB, cyBtn - u.s(13), wB, u.s(26) }; x += wB + gap;
        float32 xR = x; float32 wR = btn(x, NkT("tb.rebuild"), TEX(ic ? ic->rebuild : 0), "refresh", 0, nullptr);
        const NkRect bRebuild = { xR, cyBtn - u.s(13), wR, u.s(26) }; x += wR + gap;
        float32 wClean = btn(x, NkT("tb.clean"), TEX(ic ? ic->eraser : 0), "eraser", 0, nullptr);
        const NkRect bClean = { x, cyBtn - u.s(13), wClean, u.s(26) }; x += wClean + sep; vsep(x - sep * 0.5f);
        // Groupe Run/Debug/Test
        float32 wRun = btn(x, NkT("tb.run"), TEX(ic ? ic->play : 0), "play", 1, nullptr);
        const NkRect bRun = { x, cyBtn - u.s(13), wRun, u.s(26) }; x += wRun + gap;
        float32 wDbg = btn(x, NkT("tb.debug"), TEX(ic ? ic->bug : 0), "bug", 0, nullptr);
        const NkRect bDbg = { x, cyBtn - u.s(13), wDbg, u.s(26) }; x += wDbg + gap;
        char testBadge[8] = ""; if (nTestVis > 0) std::snprintf(testBadge, sizeof(testBadge), "%d", nTestVis);
        float32 wTst = btn(x, NkT("tb.test"), TEX(ic ? ic->kTest : 0), "flask", 2, hasTests ? testBadge : nullptr);
        const NkRect bTst = { x, cyBtn - u.s(13), wTst, u.s(26) }; x += wTst + sep; vsep(x - sep * 0.5f);
        // Recherche rapide (même hauteur que le reste)
        { const NkRect sb = { x, ctrlY, u.s(150), ctrlH }; u.Panel(sb, NkCol::input, NkCol::border, NkR::sm * u.S);
          NkOwIco(u, TEX(ic ? ic->search : 0), "search", { sb.x + u.s(9), ctrlY + (ctrlH - u.s(12)) * 0.5f, u.s(12), u.s(12) }, NkCol::mutedFg);
          u.Text(sb.x + u.s(27), ctrlY + (ctrlH - u.Lh()) * 0.5f, NkT("tb.quicksearch"), NkCol::mutedFg); }

        // ── Actions (clic sur les boutons) ──
        if (u.Hit(bBuild)   && u.click) s->DoBuildAction("build");
        if (u.Hit(bRebuild) && u.click) s->DoBuildAction("rebuild");
        if (u.Hit(bClean)   && u.click) s->DoClean();
        if (u.Hit(bRun)     && u.click) s->DoRun();
        if (u.Hit(bDbg)     && u.click) s->DoRun();            // debug -> run (câblage debug ultérieur)
        if (u.Hit(bTst)     && u.click && hasTests) s->DoTest();

        // ── Dropdown différé (par-dessus TOUT : couche overlay, sinon l'éditeur le recouvre) ──
        const bool ddWasOpen = (tb.open >= 0);   // (le clic de fermeture est consommé aussi -> pas de fuite vers le corps)
        if (tb.open >= 0) {
            const NkUi uo = NkUi::From(ec, /*overlay*/true);   // dessine dans dlOverlay (composité en dernier)
            const NkRect a = tb.openR;
            struct Item { NkString label; int32 idx; };
            NkVector<Item> items; int32 curSel = -1;
            switch (tb.open) {
                case 0: for (usize i = 0; i < s->wsNames.Size(); ++i) items.PushBack({ s->wsNames[i], (int32)i }); curSel = s->wsIdx; break;
                case 1: { const int32 np = (int32)s->projects.Size(); for (int32 i = 0; i < np; ++i) items.PushBack({ s->projects[i], i }); items.PushBack({ NkString(NkT("tb.allprojects")), np }); curSel = s->projIdx; } break;
                case 2: for (int32 i = 0; i < nSys; ++i) items.PushBack({ NkString(sysd[i].name), i }); curSel = s->sysIdx; break;
                case 3: for (int32 i = 0; i < 3; ++i) items.PushBack({ NkString(kCfg[i]), i }); curSel = cfgI; break;
                case 4: { for (int32 i = 0; i < SY.nArch; ++i) items.PushBack({ NkString(SY.archs[i]), i }); items.PushBack({ NkString(NkT("tb.all")), SY.nArch }); curSel = s->archIdx; } break;
                case 5: { items.PushBack({ NkString(NkT("tb.alltests")), -1 }); for (int32 i = 0; i < (int32)s->tests.Size(); ++i) if (s->TestVisible(i)) items.PushBack({ s->tests[i], i }); curSel = s->testIdx; } break;
            }
            const float32 ih = u.s(24);
            float32 ddw = a.w; for (usize i = 0; i < items.Size(); ++i) { const float32 tw = u.TextW(items[i].label.CStr()) + u.s(28); if (tw > ddw) ddw = tw; }
            float32 ddx = a.x; if (ddx + ddw > r.x + r.w - u.s(8)) ddx = r.x + r.w - u.s(8) - ddw;
            const NkRect dd = { ddx, a.y + a.h + u.s(2), ddw, items.Size() * ih + u.s(6) };
            uo.dl->AddRectFilled({ dd.x + u.s(2), dd.y + u.s(3), dd.w, dd.h }, NkColor{ 0,0,0,110 }, NkR::md * u.S);
            uo.Panel(dd, NkCol::surface, NkCol::primary, NkR::md * u.S);
            bool chose = false;
            for (usize i = 0; i < items.Size(); ++i) {
                const NkRect ir = { dd.x + u.s(4), dd.y + u.s(3) + (float32)i * ih, dd.w - u.s(8), ih };
                const bool hv = u.Hit(ir); const bool selrow = (items[i].idx == curSel);
                if (hv || selrow) uo.Rect(ir, NkCol::hover, NkR::sm * u.S);
                uo.TextEllipsis(ir.x + u.s(8), ir.y + (ih - u.Lh()) * 0.5f, ir.w - u.s(12), items[i].label.CStr(), selrow ? NkCol::primary : NkCol::foreground);
                if (hv && u.click) {
                    switch (tb.open) {
                        case 0: s->wsIdx = items[i].idx; s->RequestReload(); break;
                        case 1: s->projIdx = items[i].idx; break;
                        case 2: s->sysIdx = items[i].idx; s->archIdx = 0; break;
                        case 3: s->cfgIdx = items[i].idx; break;
                        case 4: s->archIdx = items[i].idx; break;
                        case 5: s->testIdx = items[i].idx; break;
                    }
                    chose = true;
                }
            }
            if (chose || (u.click && !u.Hit(dd) && !tb.justOpened)) tb.open = -1;
            tb.justOpened = false;
        }
        // MODAL : tant qu'un dropdown est (ou vient d'être) ouvert, le corps (éditeur/panneaux,
        // dessinés APRÈS la toolbar) ne doit recevoir NI survol NI clic -> on neutralise l'input.
        if (ddWasOpen) {
            auto& in = ec.Ui().input;
            in.mousePos = { -1.0e6f, -1.0e6f };
            for (int32 k = 0; k < 3; ++k) { in.mouseClicked[k] = false; in.mouseDown[k] = false; in.mouseDoubleClicked[k] = false; }
            in.wheel = in.wheelH = 0.f; in.charCount = 0;
        }
    }

} // namespace nkcode
} // namespace nkentseu
